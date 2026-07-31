/*
 * Synthetic tests informed by VibeStick's MIT-licensed JSONL helper tests at
 * revision 2223d5e414234cc95424106c69031e834b3c2163. No upstream fixture or
 * real Codex session content is copied here.
 */

import assert from 'node:assert/strict'
import { EventEmitter } from 'node:events'
import { mkdir, mkdtemp, rm, utimes, writeFile } from 'node:fs/promises'
import { tmpdir } from 'node:os'
import { join } from 'node:path'
import test from 'node:test'
import {
	detectWindowsCodexProcess,
	discoverCodexSessionFiles,
	observeCodexSessions,
	readBoundedJsonRecords,
} from './codex-session-observer.mjs'

const NOW = new Date('2026-07-31T20:15:00.000Z')
const SESSION_A = '11111111-1111-4111-8111-111111111111'
const SESSION_B = '22222222-2222-4222-8222-222222222222'
const TURN_A = 'aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa'
const TURN_B = 'bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb'

function unixSeconds(value) {
	return String(Math.trunc(Date.parse(value) / 1000))
}

function sessionMeta(sessionId, timestamp = '2026-07-31T20:00:00.000Z') {
	return {
		timestamp,
		type: 'session_meta',
		payload: { id: sessionId, session_id: sessionId },
	}
}

function turnContext(turnId, timestamp) {
	return { timestamp, type: 'turn_context', payload: { turn_id: turnId } }
}

function taskStarted(turnId, startedAt, timestamp = startedAt) {
	return {
		timestamp,
		type: 'event_msg',
		payload: { type: 'task_started', turn_id: turnId, started_at: unixSeconds(startedAt) },
	}
}

function taskComplete(turnId, startedAt, completedAt, options = {}) {
	const actualDuration = Date.parse(completedAt) - Date.parse(startedAt)
	return {
		timestamp: options.timestamp ?? completedAt,
		type: 'event_msg',
		payload: {
			type: 'task_complete',
			turn_id: turnId,
			started_at: unixSeconds(startedAt),
			completed_at: unixSeconds(completedAt),
			duration_ms: options.durationMs ?? actualDuration,
		},
	}
}

function turnAborted(turnId, startedAt, completedAt, options = {}) {
	const actualDuration = Date.parse(completedAt) - Date.parse(startedAt)
	return {
		timestamp: options.timestamp ?? completedAt,
		type: 'event_msg',
		payload: {
			type: 'turn_aborted',
			turn_id: turnId,
			reason: options.reason ?? 'synthetic-user-stop',
			started_at: Number(unixSeconds(startedAt)),
			completed_at: Number(unixSeconds(completedAt)),
			duration_ms: options.durationMs ?? actualDuration,
		},
	}
}

function toolHeartbeat(turnId, timestamp, secret = 'synthetic-secret-tool-content') {
	return [
		turnContext(turnId, timestamp),
		{
			timestamp,
			type: 'response_item',
			payload: {
				type: 'custom_tool_call',
				name: secret,
				input: { command: secret },
				status: 'completed',
			},
		},
	]
}

function processState(running = true) {
	return async () => ({ available: true, running })
}

async function fixtureRoot(t) {
	const root = await mkdtemp(join(tmpdir(), 'm5-codex-observer-test-'))
	t.after(() => rm(root, { recursive: true, force: true }))
	return root
}

async function writeSession(root, sessionId, records, options = {}) {
	const directory = options.directory ?? join(root, '2026', '07', '31')
	await mkdir(directory, { recursive: true })
	const path = join(
		directory,
		options.name ?? `rollout-2026-07-31T20-00-00-${sessionId}.jsonl`
	)
	const content = records.map((record) => JSON.stringify(record)).join('\n') + '\n'
	await writeFile(path, content)
	if (options.mtime) await utimes(path, options.mtime, options.mtime)
	return path
}

function observe(root, options = {}) {
	return observeCodexSessions({
		sessionRoot: root,
		now: () => NOW,
		processDetector: processState(true),
		...options,
	})
}

test('missing session directory reports unavailable', async (t) => {
	const root = await fixtureRoot(t)
	const result = await observe(join(root, 'missing'))
	assert.deepEqual(result, {
		available: false,
		provider: 'codex',
		state: 'unavailable',
		sessionId: null,
		turnId: null,
		startedAt: null,
		completedAt: null,
		updatedAt: NOW.toISOString(),
		stale: false,
	})
})

test('empty session directory reports idle when Codex is running', async (t) => {
	const root = await fixtureRoot(t)
	const result = await observe(root)
	assert.equal(result.available, true)
	assert.equal(result.state, 'idle')
})

test('empty session directory reports offline when Codex is not running', async (t) => {
	const root = await fixtureRoot(t)
	const result = await observe(root, { processDetector: processState(false) })
	assert.equal(result.available, true)
	assert.equal(result.state, 'offline')
})

test('discovers JSONL files recursively through nested date directories', async (t) => {
	const root = await fixtureRoot(t)
	const expected = await writeSession(root, SESSION_A, [sessionMeta(SESSION_A)])
	await writeFile(join(root, 'ignored.txt'), '{}\n')
	const result = await discoverCodexSessionFiles(root)
	assert.equal(result.available, true)
	assert.deepEqual(result.files.map((file) => file.path), [expected])
})

test('orders discovered files newest-first by modification time', async (t) => {
	const root = await fixtureRoot(t)
	const older = await writeSession(root, SESSION_A, [sessionMeta(SESSION_A)], {
		name: `rollout-2026-07-31T19-00-00-${SESSION_A}.jsonl`,
		mtime: new Date('2026-07-31T19:00:00Z'),
	})
	const newer = await writeSession(root, SESSION_B, [sessionMeta(SESSION_B)], {
		name: `rollout-2026-07-31T20-00-00-${SESSION_B}.jsonl`,
		mtime: new Date('2026-07-31T20:00:00Z'),
	})
	const result = await discoverCodexSessionFiles(root)
	assert.deepEqual(result.files.map((file) => file.path), [newer, older])
})

test('bounds the number of discovered files', async (t) => {
	const root = await fixtureRoot(t)
	await writeSession(root, SESSION_A, [sessionMeta(SESSION_A)])
	await writeSession(root, SESSION_B, [sessionMeta(SESSION_B)])
	const result = await discoverCodexSessionFiles(root, { maxFiles: 1 })
	assert.equal(result.files.length, 1)
})

test('bounds bytes read per file', async (t) => {
	const root = await fixtureRoot(t)
	const path = join(root, 'events.jsonl')
	await writeFile(path, `${JSON.stringify({ padding: 'x'.repeat(1_000) })}\n`)
	const result = await readBoundedJsonRecords(path, { maxBytes: 128 })
	assert.equal(result.readable, true)
	assert.equal(result.bytesRead, 128)
})

test('ignores malformed JSON records', async (t) => {
	const root = await fixtureRoot(t)
	const path = join(root, 'events.jsonl')
	await writeFile(path, `{bad json\n${JSON.stringify(sessionMeta(SESSION_A))}\n`)
	const result = await readBoundedJsonRecords(path)
	assert.equal(result.records.length, 1)
	assert.equal(result.records[0].type, 'session_meta')
})

test('ignores an incomplete trailing line', async (t) => {
	const root = await fixtureRoot(t)
	const path = join(root, 'events.jsonl')
	await writeFile(path, `${JSON.stringify(sessionMeta(SESSION_A))}\n{"type":"event_msg"`)
	const result = await readBoundedJsonRecords(path)
	assert.equal(result.records.length, 1)
})

test('discards the first partial fragment when a bounded read begins mid-file', async (t) => {
	const root = await fixtureRoot(t)
	const path = join(root, 'events.jsonl')
	const expected = sessionMeta(SESSION_A)
	await writeFile(path, `${JSON.stringify({ padding: 'x'.repeat(500) })}\n${JSON.stringify(expected)}\n`)
	const result = await readBoundedJsonRecords(path, { maxBytes: 256 })
	assert.deepEqual(result.records, [expected])
})

test('task_started without task_complete is running', async (t) => {
	const root = await fixtureRoot(t)
	await writeSession(root, SESSION_A, [
		sessionMeta(SESSION_A),
		taskStarted(TURN_A, '2026-07-31T20:14:00Z'),
	])
	const result = await observe(root)
	assert.equal(result.state, 'running')
	assert.equal(result.sessionId, SESSION_A)
	assert.equal(result.turnId, TURN_A)
	assert.equal(result.startedAt, '2026-07-31T20:14:00.000Z')
	assert.equal(result.completedAt, null)
})

test('matching task_complete produces done', async (t) => {
	const root = await fixtureRoot(t)
	await writeSession(root, SESSION_A, [
		sessionMeta(SESSION_A),
		taskStarted(TURN_A, '2026-07-31T20:12:00Z'),
		taskComplete(TURN_A, '2026-07-31T20:12:00Z', '2026-07-31T20:14:00Z'),
	])
	const result = await observe(root)
	assert.equal(result.state, 'done')
	assert.equal(result.turnId, TURN_A)
	assert.equal(result.completedAt, '2026-07-31T20:14:00.000Z')
})

test('confirmed turn_aborted structure produces cancelled for the matching turn', async (t) => {
	const root = await fixtureRoot(t)
	await writeSession(root, SESSION_A, [
		sessionMeta(SESSION_A),
		taskStarted(TURN_A, '2026-07-31T20:12:00Z'),
		turnAborted(TURN_A, '2026-07-31T20:12:00Z', '2026-07-31T20:14:00Z'),
	])
	const result = await observe(root)
	assert.equal(result.state, 'cancelled')
	assert.equal(result.turnId, TURN_A)
	assert.equal(result.completedAt, '2026-07-31T20:14:00.000Z')
})

test('an unrelated turn_aborted does not cancel the current running turn', async (t) => {
	const root = await fixtureRoot(t)
	await writeSession(root, SESSION_A, [
		sessionMeta(SESSION_A),
		taskStarted(TURN_A, '2026-07-31T20:14:00Z'),
		turnAborted(TURN_B, '2026-07-31T20:13:00Z', '2026-07-31T20:14:30Z'),
	])
	const result = await observe(root)
	assert.equal(result.state, 'running')
	assert.equal(result.turnId, TURN_A)
})

test('cancelled is terminal and survives reconstruction', async (t) => {
	const root = await fixtureRoot(t)
	await writeSession(root, SESSION_A, [
		sessionMeta(SESSION_A),
		taskStarted(TURN_A, '2026-07-31T20:12:00Z'),
		turnAborted(TURN_A, '2026-07-31T20:12:00Z', '2026-07-31T20:13:00Z'),
		taskStarted(TURN_A, '2026-07-31T20:14:00Z'),
		...toolHeartbeat(TURN_A, '2026-07-31T20:14:30Z'),
	])
	const first = await observe(root)
	const reconstructed = await observe(root)
	assert.equal(first.state, 'cancelled')
	assert.equal(first.startedAt, '2026-07-31T20:12:00.000Z')
	assert.deepEqual(reconstructed, first)
})

test('a later new turn runs after a cancelled turn', async (t) => {
	const root = await fixtureRoot(t)
	await writeSession(root, SESSION_A, [
		sessionMeta(SESSION_A),
		taskStarted(TURN_A, '2026-07-31T20:10:00Z'),
		turnAborted(TURN_A, '2026-07-31T20:10:00Z', '2026-07-31T20:11:00Z'),
		taskStarted(TURN_B, '2026-07-31T20:14:00Z'),
	])
	const result = await observe(root)
	assert.equal(result.state, 'running')
	assert.equal(result.turnId, TURN_B)
})

test('malformed turn_aborted timestamps leave the started turn unmatched', async (t) => {
	const root = await fixtureRoot(t)
	await writeSession(root, SESSION_A, [
		sessionMeta(SESSION_A),
		taskStarted(TURN_A, '2026-07-31T20:14:00Z'),
		turnAborted(TURN_A, '2026-07-31T20:14:00Z', '2026-07-31T20:13:00Z', {
			durationMs: 0,
		}),
	])
	assert.equal((await observe(root)).state, 'running')
})

test('mismatched completion turn does not complete the active turn', async (t) => {
	const root = await fixtureRoot(t)
	await writeSession(root, SESSION_A, [
		sessionMeta(SESSION_A),
		taskStarted(TURN_A, '2026-07-31T20:14:00Z'),
		taskComplete(TURN_B, '2026-07-31T20:13:00Z', '2026-07-31T20:14:30Z'),
	])
	const result = await observe(root)
	assert.equal(result.state, 'running')
	assert.equal(result.turnId, TURN_A)
})

test('rejects invalid and unreasonable timestamps', async (t) => {
	const root = await fixtureRoot(t)
	const invalid = taskStarted(TURN_A, '2010-01-01T00:00:00Z')
	invalid.timestamp = 'not-a-timestamp'
	await writeSession(root, SESSION_A, [sessionMeta(SESSION_A), invalid])
	const result = await observe(root)
	assert.equal(result.state, 'idle')
})

test('rejects completedAt earlier than startedAt', async (t) => {
	const root = await fixtureRoot(t)
	await writeSession(root, SESSION_A, [
		sessionMeta(SESSION_A),
		taskStarted(TURN_A, '2026-07-31T20:14:00Z'),
		taskComplete(TURN_A, '2026-07-31T20:14:00Z', '2026-07-31T20:13:00Z', {
			durationMs: 0,
		}),
	])
	const result = await observe(root)
	assert.equal(result.state, 'running')
})

test('uses duration_ms as a consistency check with second-level tolerance', async (t) => {
	const root = await fixtureRoot(t)
	await writeSession(root, SESSION_A, [
		sessionMeta(SESSION_A),
		taskStarted(TURN_A, '2026-07-31T20:13:00Z'),
		taskComplete(TURN_A, '2026-07-31T20:13:00Z', '2026-07-31T20:14:00Z', {
			durationMs: 59_400,
		}),
	])
	assert.equal((await observe(root)).state, 'done')

	const inconsistentRoot = await fixtureRoot(t)
	await writeSession(inconsistentRoot, SESSION_B, [
		sessionMeta(SESSION_B),
		taskStarted(TURN_B, '2026-07-31T20:13:00Z'),
		taskComplete(TURN_B, '2026-07-31T20:13:00Z', '2026-07-31T20:14:00Z', {
			durationMs: 10_000,
		}),
	])
	assert.equal((await observe(inconsistentRoot)).state, 'running')
})

test('reconstructs multiple turns independently within one session', async (t) => {
	const root = await fixtureRoot(t)
	await writeSession(root, SESSION_A, [
		sessionMeta(SESSION_A),
		taskStarted(TURN_A, '2026-07-31T20:10:00Z'),
		taskComplete(TURN_A, '2026-07-31T20:10:00Z', '2026-07-31T20:11:00Z'),
		taskStarted(TURN_B, '2026-07-31T20:14:00Z'),
	])
	const result = await observe(root)
	assert.equal(result.state, 'running')
	assert.equal(result.turnId, TURN_B)
})

test('reconstructs multiple sessions independently', async (t) => {
	const root = await fixtureRoot(t)
	await writeSession(root, SESSION_A, [
		sessionMeta(SESSION_A),
		taskStarted(TURN_A, '2026-07-31T20:12:00Z'),
		taskComplete(TURN_A, '2026-07-31T20:12:00Z', '2026-07-31T20:13:00Z'),
	])
	await writeSession(root, SESSION_B, [
		sessionMeta(SESSION_B),
		taskStarted(TURN_B, '2026-07-31T20:14:00Z'),
	], { name: `rollout-2026-07-31T20-01-00-${SESSION_B}.jsonl` })
	const result = await observe(root)
	assert.equal(result.state, 'running')
	assert.equal(result.sessionId, SESSION_B)
})

test('selects the newest active turn using event time rather than file mtime', async (t) => {
	const root = await fixtureRoot(t)
	await writeSession(root, SESSION_A, [
		sessionMeta(SESSION_A),
		taskStarted(TURN_A, '2026-07-31T20:14:30Z'),
	], {
		name: `rollout-2026-07-31T19-00-00-${SESSION_A}.jsonl`,
		mtime: new Date('2026-07-31T19:00:00Z'),
	})
	await writeSession(root, SESSION_B, [
		sessionMeta(SESSION_B),
		taskStarted(TURN_B, '2026-07-31T20:14:00Z'),
	], {
		name: `rollout-2026-07-31T20-00-00-${SESSION_B}.jsonl`,
		mtime: new Date('2026-07-31T20:14:59Z'),
	})
	const result = await observe(root)
	assert.equal(result.sessionId, SESSION_A)
	assert.equal(result.turnId, TURN_A)
})

test('expires completed turns after the done-retention window', async (t) => {
	const root = await fixtureRoot(t)
	await writeSession(root, SESSION_A, [
		sessionMeta(SESSION_A),
		taskStarted(TURN_A, '2026-07-31T20:00:00Z'),
		taskComplete(TURN_A, '2026-07-31T20:00:00Z', '2026-07-31T20:01:00Z'),
	])
	const result = await observe(root, { doneRetentionMs: 5 * 60_000 })
	assert.equal(result.state, 'idle')
	assert.equal(result.turnId, null)
})

test('marks an unmatched old turn stale', async (t) => {
	const root = await fixtureRoot(t)
	await writeSession(root, SESSION_A, [
		sessionMeta(SESSION_A),
		taskStarted(TURN_A, '2026-07-31T20:00:00Z'),
	])
	const result = await observe(root, { activeStaleMs: 4 * 60_000 })
	assert.equal(result.state, 'stale')
	assert.equal(result.stale, true)
	assert.equal(result.turnId, TURN_A)
})

test('tool records update only the selected turn heartbeat timestamp', async (t) => {
	const root = await fixtureRoot(t)
	const records = [
		sessionMeta(SESSION_A),
		taskStarted(TURN_A, '2026-07-31T20:00:00Z'),
		...toolHeartbeat(TURN_A, '2026-07-31T20:14:00.250Z'),
	]
	await writeSession(root, SESSION_A, records)
	const result = await observe(root)
	assert.equal(result.state, 'running')
	assert.equal(result.updatedAt, '2026-07-31T20:14:00.250Z')
})

test('observer restart reconstructs the same result from persisted JSONL', async (t) => {
	const root = await fixtureRoot(t)
	await writeSession(root, SESSION_A, [
		sessionMeta(SESSION_A),
		taskStarted(TURN_A, '2026-07-31T20:14:00Z'),
	])
	const first = await observe(root)
	const afterRestart = await observe(root)
	assert.deepEqual(afterRestart, first)
})

function fakeProcess(exitCode, options = {}) {
	const child = new EventEmitter()
	child.killed = false
	child.kill = () => {
		child.killed = true
		return true
	}
	if (!options.neverExit) queueMicrotask(() => child.emit('exit', exitCode))
	return child
}

test('Windows process detector reports a successful Codex match', async () => {
	let spawnCall
	const result = await detectWindowsCodexProcess({
		platform: 'win32',
		spawnProcess: (executable, args, options) => {
			spawnCall = { executable, args, options }
			return fakeProcess(0)
		},
	})
	assert.equal(result.available, true)
	assert.equal(result.running, true)
	assert.equal(spawnCall.executable, 'powershell.exe')
	assert.equal(spawnCall.options.shell, false)
})

test('Windows process detector treats denied inspection as unavailable', async () => {
	const result = await detectWindowsCodexProcess({
		platform: 'win32',
		spawnProcess: () => fakeProcess(5),
	})
	assert.deepEqual(result, { available: false, running: null })
})

test('Windows process detector times out without breaking observation', async () => {
	let child
	const result = await detectWindowsCodexProcess({
		platform: 'win32',
		timeoutMs: 5,
		spawnProcess: () => {
			child = fakeProcess(null, { neverExit: true })
			return child
		},
	})
	assert.deepEqual(result, { available: false, running: null })
	assert.equal(child.killed, true)
})

test('observer output excludes conversation, path, allowance, and tool content', async (t) => {
	const root = await fixtureRoot(t)
	const secret = 'invented-private-content-never-export'
	await writeSession(root, SESSION_A, [
		sessionMeta(SESSION_A),
		taskStarted(TURN_A, '2026-07-31T20:14:00Z'),
		{
			timestamp: '2026-07-31T20:14:01Z',
			type: 'event_msg',
			payload: { type: 'user_message', message: secret, cwd: secret, rate_limits: secret },
		},
		...toolHeartbeat(TURN_A, '2026-07-31T20:14:02Z', secret),
		{
			timestamp: '2026-07-31T20:14:03Z',
			type: 'response_item',
			payload: { type: 'custom_tool_call_output', output: secret },
		},
		turnAborted(TURN_A, '2026-07-31T20:14:00Z', '2026-07-31T20:14:04Z', {
			reason: secret,
		}),
	])
	const result = await observe(root)
	assert.deepEqual(Object.keys(result), [
		'available',
		'provider',
		'state',
		'sessionId',
		'turnId',
		'startedAt',
		'completedAt',
		'updatedAt',
		'stale',
	])
	const serialized = JSON.stringify(result)
	assert.equal(serialized.includes(secret), false)
	for (const forbidden of ['message', 'tool', 'path', 'cwd', 'quota', 'allowance']) {
		assert.equal(Object.hasOwn(result, forbidden), false)
	}
})
