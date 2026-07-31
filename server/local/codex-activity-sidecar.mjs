import { createServer } from 'node:http'
import { fileURLToPath } from 'node:url'
import { resolve } from 'node:path'
import { observeCodexSessions } from './codex-session-observer.mjs'

const DEFAULT_HOST = '127.0.0.1'
const DEFAULT_PORT = 8792
const DEFAULT_POLL_MS = 2_000
const DEFAULT_OBSERVER_TIMEOUT_MS = 5_000
const MIN_ENV_POLL_MS = 250
const ALLOWED_STATES = new Set([
	'idle',
	'running',
	'done',
	'cancelled',
	'stale',
	'offline',
	'unavailable',
])

function configurationError() {
	return Object.assign(new Error('Invalid Codex activity sidecar configuration'), {
		code: 'invalid_configuration',
	})
}

function parsePort(value, fallback = DEFAULT_PORT) {
	if (value == null || value === '') return fallback
	const port = typeof value === 'number' ? value : Number.parseInt(value, 10)
	if (!Number.isInteger(port) || port < 0 || port > 65_535) throw configurationError()
	return port
}

function parsePollMs(value, fallback = DEFAULT_POLL_MS, minimum = 1) {
	if (value == null || value === '') return fallback
	const pollMs = typeof value === 'number' ? value : Number.parseInt(value, 10)
	if (!Number.isInteger(pollMs) || pollMs < minimum) throw configurationError()
	return pollMs
}

function validatedHost(value) {
	const host = value || DEFAULT_HOST
	if (host !== DEFAULT_HOST) throw configurationError()
	return host
}

function validOpaqueId(value) {
	return typeof value === 'string' && value.length > 0 && value.length <= 256
}

function parseIso(value) {
	if (typeof value !== 'string' || value.length === 0) return null
	const parsed = Date.parse(value)
	return Number.isFinite(parsed) ? parsed : null
}

function isoNow(now) {
	const value = now()
	const date = value instanceof Date ? value : new Date(value)
	if (Number.isNaN(date.getTime())) throw new TypeError('Sidecar now() must return a valid time')
	return date.toISOString()
}

export function normalizeCodexActivity(value, now = () => new Date()) {
	if (!value || typeof value !== 'object' || Array.isArray(value)) throw new TypeError('invalid')
	if (value.provider !== 'codex' || !ALLOWED_STATES.has(value.state)) throw new TypeError('invalid')
	if (typeof value.available !== 'boolean' || typeof value.stale !== 'boolean') {
		throw new TypeError('invalid')
	}

	const updatedMs = parseIso(value.updatedAt)
	if (updatedMs == null) throw new TypeError('invalid')
	const updatedAt = new Date(updatedMs).toISOString()

	if (value.state === 'unavailable') {
		if (value.available !== false) throw new TypeError('invalid')
		return {
			available: false,
			provider: 'codex',
			state: 'unavailable',
			sessionId: null,
			turnId: null,
			startedAt: null,
			completedAt: null,
			updatedAt,
			stale: true,
		}
	}

	if (value.available !== true) throw new TypeError('invalid')
	if (value.state === 'idle' || value.state === 'offline') {
		return {
			available: true,
			provider: 'codex',
			state: value.state,
			sessionId: null,
			turnId: null,
			startedAt: null,
			completedAt: null,
			updatedAt,
			stale: Boolean(value.stale),
		}
	}

	if (!validOpaqueId(value.sessionId) || !validOpaqueId(value.turnId)) {
		throw new TypeError('invalid')
	}
	const startedMs = parseIso(value.startedAt)
	if (startedMs == null) throw new TypeError('invalid')
	const startedAt = new Date(startedMs).toISOString()

	if (value.state === 'done' || value.state === 'cancelled') {
		const completedMs = parseIso(value.completedAt)
		if (completedMs == null || completedMs < startedMs) throw new TypeError('invalid')
		return {
			available: true,
			provider: 'codex',
			state: value.state,
			sessionId: value.sessionId,
			turnId: value.turnId,
			startedAt,
			completedAt: new Date(completedMs).toISOString(),
			updatedAt,
			stale: Boolean(value.stale),
		}
	}

	if (value.completedAt != null) throw new TypeError('invalid')
	if (value.state === 'running' && value.stale) throw new TypeError('invalid')
	return {
		available: true,
		provider: 'codex',
		state: value.state,
		sessionId: value.sessionId,
		turnId: value.turnId,
		startedAt,
		completedAt: null,
		updatedAt,
		stale: value.state === 'stale',
	}
}

function unavailableActivity(now) {
	return {
		available: false,
		provider: 'codex',
		state: 'unavailable',
		sessionId: null,
		turnId: null,
		startedAt: null,
		completedAt: null,
		updatedAt: isoNow(now),
		stale: true,
	}
}

export function createCodexActivityService(options = {}) {
	const observe = options.observe ?? observeCodexSessions
	const now = options.now ?? (() => new Date())
	const pollMs = parsePollMs(options.pollMs, DEFAULT_POLL_MS)
	const observerTimeoutMs = parsePollMs(
		options.observerTimeoutMs,
		DEFAULT_OBSERVER_TIMEOUT_MS
	)
	const setIntervalFn = options.setIntervalFn ?? setInterval
	const clearIntervalFn = options.clearIntervalFn ?? clearInterval
	const logError = options.logError ?? ((message) => console.error(message))
	let latest = null
	let inFlight = null
	let timer = null
	let active = false
	let startPromise = null

	function failedObservation(kind) {
		try {
			logError(`[codex-activity] observer_failure kind=${kind} cached=${latest ? 'yes' : 'no'}`)
		} catch {
			// Diagnostics must never break reconciliation.
		}
		if (!latest) return unavailableActivity(now)
		return { ...latest, updatedAt: isoNow(now), stale: true }
	}

	function observeWithTimeout() {
		let timeout
		const deadline = new Promise((_, reject) => {
			timeout = setTimeout(() => {
				reject(Object.assign(new Error('Observer timed out'), { code: 'observer_timeout' }))
			}, observerTimeoutMs)
		})
		return Promise.race([Promise.resolve().then(() => observe()), deadline]).finally(() => {
			clearTimeout(timeout)
		})
	}

	function reconcile() {
		if (inFlight) return inFlight
		inFlight = observeWithTimeout()
			.then(
				(value) => {
					try {
						latest = normalizeCodexActivity(value, now)
					} catch {
						latest = failedObservation('invalid_result')
					}
					return latest
				},
				(error) => {
					latest = failedObservation(
						error?.code === 'observer_timeout' ? 'timeout' : 'exception'
					)
					return latest
				}
			)
			.finally(() => {
				inFlight = null
			})
		return inFlight
	}

	async function start() {
		if (startPromise) return startPromise
		active = true
		startPromise = (async () => {
			const initial = await reconcile()
			if (active && !timer) {
				timer = setIntervalFn(() => {
					void reconcile()
				}, pollMs)
				timer?.unref?.()
			}
			return initial
		})()
		return startPromise
	}

	function stop() {
		active = false
		if (timer) clearIntervalFn(timer)
		timer = null
	}

	return {
		start,
		stop,
		reconcile,
		async getActivity() {
			if (inFlight) return inFlight
			if (latest) return latest
			return reconcile()
		},
		getCachedActivity() {
			return latest
		},
	}
}

function sendJson(response, status, payload, extraHeaders = {}) {
	const body = JSON.stringify(payload)
	response.writeHead(status, {
		'Cache-Control': 'no-store',
		'Content-Type': 'application/json; charset=utf-8',
		'Content-Length': Buffer.byteLength(body),
		...extraHeaders,
	})
	response.end(body)
}

function hasLoopbackHostHeader(request) {
	const host = request.headers.host
	return typeof host === 'string' && (host === DEFAULT_HOST || host.startsWith(`${DEFAULT_HOST}:`))
}

function listen(server, port, host) {
	return new Promise((resolveListen, rejectListen) => {
		const onError = (error) => {
			server.off('listening', onListening)
			rejectListen(error)
		}
		const onListening = () => {
			server.off('error', onError)
			resolveListen()
		}
		server.once('error', onError)
		server.once('listening', onListening)
		server.listen(port, host)
	})
}

function closeServer(server) {
	if (!server.listening) return Promise.resolve()
	return new Promise((resolveClose, rejectClose) => {
		server.close((error) => (error ? rejectClose(error) : resolveClose()))
	})
}

export function createCodexActivitySidecar(options = {}) {
	const host = validatedHost(options.host)
	const port = parsePort(options.port)
	const service = options.service ?? createCodexActivityService(options.serviceOptions)
	const server = createServer(async (request, response) => {
		if (!hasLoopbackHostHeader(request)) {
			sendJson(response, 403, { error: { code: 'loopback_only' } })
			return
		}
		if (request.method !== 'GET') {
			sendJson(response, 405, { error: { code: 'method_not_allowed' } }, { Allow: 'GET' })
			return
		}

		let pathname
		try {
			pathname = new URL(request.url || '/', `http://${host}:${port}`).pathname
		} catch {
			sendJson(response, 400, { error: { code: 'bad_request' } })
			return
		}

		if (pathname === '/health') {
			sendJson(response, 200, { ok: true, service: 'codex-activity' })
			return
		}
		if (pathname !== '/activity') {
			sendJson(response, 404, { error: { code: 'not_found' } })
			return
		}

		const activity = await service.getActivity()
		sendJson(response, 200, activity)
	})
	let startPromise = null

	return {
		host,
		port,
		server,
		service,
		async start() {
			if (startPromise) return startPromise
			startPromise = (async () => {
				await service.start()
				await listen(server, port, host)
				return server.address()
			})()
			return startPromise
		},
		async close() {
			service.stop()
			await closeServer(server)
		},
	}
}

export async function startCodexActivitySidecar(options = {}) {
	const env = options.env ?? process.env
	const host = validatedHost(options.host ?? env.CODEX_ACTIVITY_HOST)
	const port = parsePort(options.port ?? env.CODEX_ACTIVITY_PORT)
	const pollMs = parsePollMs(
		options.pollMs ?? env.CODEX_ACTIVITY_POLL_MS,
		DEFAULT_POLL_MS,
		options.pollMs == null ? MIN_ENV_POLL_MS : 1
	)
	const observerTimeoutMs = parsePollMs(
		options.observerTimeoutMs ?? env.CODEX_ACTIVITY_OBSERVER_TIMEOUT_MS,
		DEFAULT_OBSERVER_TIMEOUT_MS,
		options.observerTimeoutMs == null ? MIN_ENV_POLL_MS : 1
	)
	const sessionRoot = options.sessionRoot ?? env.CODEX_SESSION_ROOT
	const observe = options.observe ?? (() => observeCodexSessions({ sessionRoot }))
	const sidecar = createCodexActivitySidecar({
		host,
		port,
		serviceOptions: {
			observe,
			pollMs,
			observerTimeoutMs,
			logError: options.logError,
		},
	})
	await sidecar.start()
	return sidecar
}

async function main() {
	let sidecar
	try {
		sidecar = await startCodexActivitySidecar()
		const address = sidecar.server.address()
		console.log(`[codex-activity] listening host=${address.address} port=${address.port}`)
	} catch {
		console.error('[codex-activity] startup_failure code=invalid_or_unavailable')
		process.exitCode = 1
		return
	}

	let shuttingDown = false
	const shutdown = async () => {
		if (shuttingDown) return
		shuttingDown = true
		try {
			await sidecar.close()
			process.exitCode = 0
		} catch {
			process.exitCode = 1
		}
	}
	process.once('SIGINT', shutdown)
	process.once('SIGTERM', shutdown)
}

const isMain = process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)
if (isMain) void main()
