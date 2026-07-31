/*
 * Design provenance: the bounded newest-first JSONL scan and its initial
 * 40-file, 1.5 MB, four-minute, and five-minute defaults are adapted from
 * Gary Zhang's VibeStick project at revision
 * 2223d5e414234cc95424106c69031e834b3c2163 (MIT licensed).
 *
 * This Windows implementation is independently written for the confirmed
 * Codex Desktop session structure and never retains raw record content.
 */

import { spawn } from 'node:child_process'
import { open, readdir, stat } from 'node:fs/promises'
import { homedir } from 'node:os'
import { basename, extname, join } from 'node:path'

const PROVIDER = 'codex'
const MIN_TIMESTAMP_MS = Date.UTC(2020, 0, 1)
const MAX_FUTURE_SKEW_MS = 24 * 60 * 60_000
const TIMESTAMP_MATCH_TOLERANCE_MS = 2_000
const DURATION_TOLERANCE_MS = 2_000
const HEARTBEAT_TYPES = new Set([
	'custom_tool_call',
	'custom_tool_call_output',
	'function_call',
	'function_call_output',
])

export const CODEX_SESSION_OBSERVER_DEFAULTS = Object.freeze({
	maxFiles: 40,
	maxBytesPerFile: 1_500_000,
	activeStaleMs: 4 * 60_000,
	doneRetentionMs: 5 * 60_000,
	processTimeoutMs: 2_000,
})

function positiveInteger(value, fallback) {
	const parsed = typeof value === 'number' ? value : Number.parseInt(value, 10)
	return Number.isSafeInteger(parsed) && parsed > 0 ? parsed : fallback
}

function optionValue(options, name, envName, fallback) {
	if (options[name] != null) return positiveInteger(options[name], fallback)
	return positiveInteger((options.env ?? process.env)[envName], fallback)
}

function resolveOptions(options) {
	const env = options.env ?? process.env
	return {
		sessionRoot:
			options.sessionRoot ??
			env.M5_CODEX_SESSION_ROOT ??
			join(options.homeDirectory ?? homedir(), '.codex', 'sessions'),
		maxFiles: optionValue(
			options,
			'maxFiles',
			'M5_CODEX_OBSERVER_MAX_FILES',
			CODEX_SESSION_OBSERVER_DEFAULTS.maxFiles
		),
		maxBytesPerFile: optionValue(
			options,
			'maxBytesPerFile',
			'M5_CODEX_OBSERVER_MAX_BYTES_PER_FILE',
			CODEX_SESSION_OBSERVER_DEFAULTS.maxBytesPerFile
		),
		activeStaleMs: optionValue(
			options,
			'activeStaleMs',
			'M5_CODEX_OBSERVER_ACTIVE_STALE_MS',
			CODEX_SESSION_OBSERVER_DEFAULTS.activeStaleMs
		),
		doneRetentionMs: optionValue(
			options,
			'doneRetentionMs',
			'M5_CODEX_OBSERVER_DONE_RETENTION_MS',
			CODEX_SESSION_OBSERVER_DEFAULTS.doneRetentionMs
		),
		processTimeoutMs: optionValue(
			options,
			'processTimeoutMs',
			'M5_CODEX_OBSERVER_PROCESS_TIMEOUT_MS',
			CODEX_SESSION_OBSERVER_DEFAULTS.processTimeoutMs
		),
	}
}

function validOpaqueId(value) {
	return typeof value === 'string' && value.length > 0 && value.length <= 256
}

function validTimestampMs(value, nowMs) {
	return (
		Number.isFinite(value) &&
		value >= MIN_TIMESTAMP_MS &&
		value <= nowMs + MAX_FUTURE_SKEW_MS
	)
}

function parseIsoTimestamp(value, nowMs) {
	if (typeof value !== 'string' || value.length === 0) return null
	const parsed = Date.parse(value)
	return validTimestampMs(parsed, nowMs) ? parsed : null
}

function parseUnixSeconds(value, nowMs) {
	if (typeof value === 'string' && !/^\d+$/.test(value)) return null
	const seconds = typeof value === 'number' ? value : Number(value)
	if (!Number.isSafeInteger(seconds) || seconds < 0) return null
	const parsed = seconds * 1000
	return validTimestampMs(parsed, nowMs) ? parsed : null
}

function parseDurationMs(value) {
	return typeof value === 'number' && Number.isSafeInteger(value) && value >= 0
		? value
		: null
}

function iso(value) {
	return value == null ? null : new Date(value).toISOString()
}

function newestBy(items, primary, secondary) {
	return items.reduce((selected, item) => {
		if (!selected) return item
		if (primary(item) !== primary(selected)) {
			return primary(item) > primary(selected) ? item : selected
		}
		return secondary(item) > secondary(selected) ? item : selected
	}, null)
}

async function walkJsonl(root, entries, isSessionRoot = false) {
	let directoryEntries
	try {
		directoryEntries = await readdir(root, { withFileTypes: true })
	} catch (error) {
		if (isSessionRoot) throw error
		return false
	}

	let readable = true
	for (const entry of directoryEntries) {
		const path = join(root, entry.name)
		if (entry.isDirectory()) {
			if (!(await walkJsonl(path, entries, false))) readable = false
			continue
		}
		if (!entry.isFile() || extname(entry.name).toLowerCase() !== '.jsonl') continue
		try {
			const info = await stat(path)
			if (info.isFile()) entries.push({ path, mtimeMs: info.mtimeMs, size: info.size })
		} catch {
			readable = false
		}
	}
	return readable
}

export async function discoverCodexSessionFiles(sessionRoot, options = {}) {
	const maxFiles = positiveInteger(options.maxFiles, CODEX_SESSION_OBSERVER_DEFAULTS.maxFiles)
	const entries = []
	try {
		const fullyReadable = await walkJsonl(sessionRoot, entries, true)
		entries.sort((left, right) => right.mtimeMs - left.mtimeMs)
		return { available: true, fullyReadable, files: entries.slice(0, maxFiles) }
	} catch (error) {
		if (error?.code === 'ENOENT') {
			return { available: false, fullyReadable: false, files: [] }
		}
		return { available: false, fullyReadable: false, files: [] }
	}
}

export async function readBoundedJsonRecords(path, options = {}) {
	const maxBytes = positiveInteger(
		options.maxBytes,
		CODEX_SESSION_OBSERVER_DEFAULTS.maxBytesPerFile
	)
	let handle
	try {
		handle = await open(path, 'r')
		const info = await handle.stat()
		const bytesToRead = Math.min(info.size, maxBytes)
		const start = Math.max(0, info.size - bytesToRead)
		const buffer = Buffer.allocUnsafe(bytesToRead)
		const { bytesRead } = await handle.read(buffer, 0, bytesToRead, start)
		let text = buffer.subarray(0, bytesRead).toString('utf8')

		if (start > 0) {
			const firstNewline = text.indexOf('\n')
			if (firstNewline < 0) return { readable: true, bytesRead, records: [] }
			text = text.slice(firstNewline + 1)
		}

		if (!text.endsWith('\n')) {
			const lastNewline = text.lastIndexOf('\n')
			if (lastNewline < 0) return { readable: true, bytesRead, records: [] }
			text = text.slice(0, lastNewline + 1)
		}

		const records = []
		for (const rawLine of text.split('\n')) {
			const line = rawLine.endsWith('\r') ? rawLine.slice(0, -1) : rawLine
			if (!line.startsWith('{')) continue
			try {
				const record = JSON.parse(line)
				if (record && typeof record === 'object' && !Array.isArray(record)) {
					records.push(record)
				}
			} catch {
				// Raw lines and parse failures are deliberately not logged.
			}
		}
		return { readable: true, bytesRead, records }
	} catch {
		return { readable: false, bytesRead: 0, records: [] }
	} finally {
		await handle?.close().catch(() => {})
	}
}

function sessionIdFromFilename(path) {
	const match = basename(path).match(
		/([0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})\.jsonl$/i
	)
	return match?.[1] ?? null
}

function sanitizeRecords(records, path, nowMs) {
	let sessionId = null
	const events = []
	for (const record of records) {
		const topType = typeof record.type === 'string' ? record.type : ''
		const payload = record.payload && typeof record.payload === 'object' ? record.payload : null
		if (!payload) continue

		if (topType === 'session_meta') {
			const candidate = validOpaqueId(payload.session_id)
				? payload.session_id
				: validOpaqueId(payload.id)
					? payload.id
					: null
			if (candidate && !sessionId) sessionId = candidate
			continue
		}

		const timestampMs = parseIsoTimestamp(record.timestamp, nowMs)
		if (timestampMs == null) continue
		const payloadType = typeof payload.type === 'string' ? payload.type : ''

		if (topType === 'turn_context' && validOpaqueId(payload.turn_id)) {
			events.push({ type: 'turn_context', turnId: payload.turn_id, timestampMs })
			continue
		}

		if (topType === 'event_msg' && payloadType === 'task_started') {
			const startedMs = parseUnixSeconds(payload.started_at, nowMs)
			if (validOpaqueId(payload.turn_id) && startedMs != null) {
				events.push({
					type: 'task_started',
					turnId: payload.turn_id,
					startedMs,
					timestampMs,
				})
			}
			continue
		}

		if (topType === 'event_msg' && payloadType === 'task_complete') {
			const startedMs = parseUnixSeconds(payload.started_at, nowMs)
			const completedMs = parseUnixSeconds(payload.completed_at, nowMs)
			const durationMs = parseDurationMs(payload.duration_ms)
			if (
				validOpaqueId(payload.turn_id) &&
				startedMs != null &&
				completedMs != null &&
				completedMs >= startedMs &&
				durationMs != null &&
				Math.abs(completedMs - startedMs - durationMs) <= DURATION_TOLERANCE_MS
			) {
				events.push({
					type: 'task_complete',
					turnId: payload.turn_id,
					startedMs,
					completedMs,
					timestampMs,
				})
			}
			continue
		}

		if (topType === 'event_msg' && payloadType === 'turn_aborted') {
			const startedMs = parseUnixSeconds(payload.started_at, nowMs)
			const completedMs = parseUnixSeconds(payload.completed_at, nowMs)
			const durationMs = parseDurationMs(payload.duration_ms)
			if (
				validOpaqueId(payload.turn_id) &&
				startedMs != null &&
				completedMs != null &&
				completedMs >= startedMs &&
				durationMs != null &&
				Math.abs(completedMs - startedMs - durationMs) <= DURATION_TOLERANCE_MS
			) {
				events.push({
					type: 'turn_aborted',
					turnId: payload.turn_id,
					startedMs,
					completedMs,
					timestampMs,
				})
			}
			continue
		}

		if (topType === 'response_item' && HEARTBEAT_TYPES.has(payloadType)) {
			events.push({ type: 'heartbeat', timestampMs })
		}
	}
	return { sessionId: sessionId ?? sessionIdFromFilename(path), events }
}

function mergeFileEvidence(sessions, evidence) {
	if (!validOpaqueId(evidence.sessionId)) return
	let session = sessions.get(evidence.sessionId)
	if (!session) {
		session = { sessionId: evidence.sessionId, turns: new Map() }
		sessions.set(evidence.sessionId, session)
	}

	let currentTurnId = null
	for (const event of evidence.events) {
		if (event.type === 'turn_context') {
			currentTurnId = event.turnId
			continue
		}

		if (event.type === 'task_started') {
			currentTurnId = event.turnId
			const existing = session.turns.get(event.turnId)
			if (existing?.terminalState) continue
			if (!existing || event.startedMs >= existing.startedMs) {
				session.turns.set(event.turnId, {
					turnId: event.turnId,
					startedMs: event.startedMs,
					completedMs: existing?.completedMs ?? null,
					terminalState: existing?.terminalState ?? null,
					updatedMs: Math.max(event.timestampMs, existing?.updatedMs ?? 0),
				})
			}
			continue
		}

		if (event.type === 'heartbeat') {
			const turn = currentTurnId == null ? null : session.turns.get(currentTurnId)
			if (turn && turn.completedMs == null) turn.updatedMs = Math.max(turn.updatedMs, event.timestampMs)
			continue
		}

		if (event.type === 'task_complete' || event.type === 'turn_aborted') {
			const existing = session.turns.get(event.turnId)
			if (existing?.terminalState) continue
			if (
				existing &&
				Math.abs(existing.startedMs - event.startedMs) > TIMESTAMP_MATCH_TOLERANCE_MS
			) {
				continue
			}
				session.turns.set(event.turnId, {
					turnId: event.turnId,
					startedMs: existing?.startedMs ?? event.startedMs,
					completedMs: event.completedMs,
					terminalState: event.type === 'turn_aborted' ? 'cancelled' : 'done',
					updatedMs: Math.max(event.timestampMs, event.completedMs, existing?.updatedMs ?? 0),
			})
			if (currentTurnId === event.turnId) currentTurnId = null
		}
	}
}

function observation(state, nowMs, turn = null) {
	return {
		available: state !== 'unavailable',
		provider: PROVIDER,
		state,
		sessionId: turn?.sessionId ?? null,
		turnId: turn?.turnId ?? null,
		startedAt: iso(turn?.startedMs),
		completedAt: iso(turn?.completedMs),
		updatedAt: iso(turn?.updatedMs ?? nowMs),
		stale: state === 'stale',
	}
}

function allTurns(sessions) {
	const turns = []
	for (const session of sessions.values()) {
		for (const turn of session.turns.values()) {
			turns.push({ ...turn, sessionId: session.sessionId })
		}
	}
	return turns
}

function selectTurnObservation(sessions, processState, config, nowMs) {
	const turns = allTurns(sessions)
	const unmatched = turns.filter((turn) => turn.completedMs == null)
	const active = unmatched.filter((turn) => nowMs - turn.updatedMs <= config.activeStaleMs)
	const newestActive = newestBy(active, (turn) => turn.startedMs, (turn) => turn.updatedMs)
	if (newestActive) return observation('running', nowMs, newestActive)

	const retainedTerminal = turns.filter(
		(turn) =>
			turn.completedMs != null &&
			nowMs - turn.completedMs <= config.doneRetentionMs
	)
	const newestTerminal = newestBy(
		retainedTerminal,
		(turn) => turn.completedMs,
		(turn) => turn.updatedMs
	)
	if (newestTerminal) {
		return observation(newestTerminal.terminalState ?? 'done', nowMs, newestTerminal)
	}

	const newestStale = newestBy(unmatched, (turn) => turn.startedMs, (turn) => turn.updatedMs)
	if (newestStale) return observation('stale', nowMs, newestStale)

	if (processState.available && processState.running === false) {
		return observation('offline', nowMs)
	}
	return observation('idle', nowMs)
}

const WINDOWS_PROCESS_SCRIPT =
	"if (@(Get-Process -Name 'codex','codex-code-mode-host' -ErrorAction SilentlyContinue).Count -gt 0) { exit 0 }; exit 1"

export function detectWindowsCodexProcess(options = {}) {
	const platform = options.platform ?? process.platform
	if (platform !== 'win32') return Promise.resolve({ available: false, running: null })
	const timeoutMs = positiveInteger(
		options.timeoutMs,
		CODEX_SESSION_OBSERVER_DEFAULTS.processTimeoutMs
	)
	const spawnProcess = options.spawnProcess ?? spawn

	return new Promise((resolveProcess) => {
		let child
		let settled = false
		let timer
		const settle = (result) => {
			if (settled) return
			settled = true
			clearTimeout(timer)
			resolveProcess(result)
		}

		try {
			child = spawnProcess(
				'powershell.exe',
				['-NoLogo', '-NoProfile', '-NonInteractive', '-Command', WINDOWS_PROCESS_SCRIPT],
				{ shell: false, windowsHide: true, stdio: 'ignore' }
			)
		} catch {
			settle({ available: false, running: null })
			return
		}

		child.once('error', () => settle({ available: false, running: null }))
		child.once('exit', (code) => {
			if (code === 0) settle({ available: true, running: true })
			else if (code === 1) settle({ available: true, running: false })
			else settle({ available: false, running: null })
		})
		timer = setTimeout(() => {
			try {
				child.kill()
			} catch {
				// Process inspection failure remains non-fatal.
			}
			settle({ available: false, running: null })
		}, timeoutMs)
	})
}

export async function observeCodexSessions(options = {}) {
	const config = resolveOptions(options)
	const nowValue = options.now?.() ?? new Date()
	const nowMs = nowValue instanceof Date ? nowValue.getTime() : Number(nowValue)
	if (!Number.isFinite(nowMs)) throw new TypeError('Observer now() must return a valid time')

	const discovery = await discoverCodexSessionFiles(config.sessionRoot, {
		maxFiles: config.maxFiles,
	})
	if (!discovery.available) return observation('unavailable', nowMs)

	const sessions = new Map()
	let readableFiles = 0
	for (const file of discovery.files) {
		const tail = await readBoundedJsonRecords(file.path, {
			maxBytes: config.maxBytesPerFile,
		})
		if (!tail.readable) continue
		readableFiles += 1
		mergeFileEvidence(sessions, sanitizeRecords(tail.records, file.path, nowMs))
	}
	if (discovery.files.length > 0 && readableFiles === 0) {
		return observation('unavailable', nowMs)
	}

	const processDetector = options.processDetector ?? detectWindowsCodexProcess
	let processState
	try {
		processState = await processDetector({
			platform: options.platform ?? process.platform,
			timeoutMs: config.processTimeoutMs,
		})
	} catch {
		processState = { available: false, running: null }
	}
	return selectTurnObservation(sessions, processState, config, nowMs)
}
