import assert from 'node:assert/strict'
import test from 'node:test'
import {
	fetchCodexActivityBridge,
	normalizeCodexActivity,
	resolveCodexActivityBridgeUrl,
} from '../src/codex-activity-proxy.ts'
import { isAuthorizedDeviceRequest } from '../src/request-auth.ts'

const NOW = new Date('2026-07-31T20:20:00.000Z')
const base = {
	available: true,
	provider: 'codex',
	state: 'running',
	sessionId: 'session-synthetic',
	turnId: 'turn-synthetic',
	startedAt: '2026-07-31T20:10:40.000Z',
	completedAt: null,
	updatedAt: '2026-07-31T20:14:14.042Z',
	stale: false,
}

function response(payload, status = 200) {
	return new Response(JSON.stringify(payload), {
		status,
		headers: { 'Content-Type': 'application/json' },
	})
}

test('uses the localhost activity endpoint by default', () => {
	assert.equal(resolveCodexActivityBridgeUrl(), 'http://localhost:8792/activity')
	assert.equal(resolveCodexActivityBridgeUrl('http://localhost:8792'), 'http://localhost:8792/activity')
})

test('normalizes running and strips extra fields', () => {
	assert.deepEqual(normalizeCodexActivity({ ...base, privateText: 'never forward' }), base)
})

test('accepts a valid done response', () => {
	const done = { ...base, state: 'done', completedAt: '2026-07-31T20:12:40.000Z' }
	assert.deepEqual(normalizeCodexActivity(done), done)
})

test('accepts a valid cancelled response and strips extra fields', () => {
	const cancelled = {
		...base,
		state: 'cancelled',
		completedAt: '2026-07-31T20:12:40.000Z',
	}
	assert.deepEqual(
		normalizeCodexActivity({ ...cancelled, abortReason: 'never forward' }),
		cancelled
	)
})

for (const state of ['idle', 'offline']) {
	test(`accepts valid ${state}`, () => {
		const payload = {
			...base,
			state,
			sessionId: null,
			turnId: null,
			startedAt: null,
		}
		assert.deepEqual(normalizeCodexActivity(payload), payload)
	})
}

test('accepts valid stale and unavailable states', () => {
	const stale = { ...base, state: 'stale', stale: true }
	assert.deepEqual(normalizeCodexActivity(stale), stale)
	const unavailable = {
		...base,
		available: false,
		state: 'unavailable',
		sessionId: null,
		turnId: null,
		startedAt: null,
		stale: true,
	}
	assert.deepEqual(normalizeCodexActivity(unavailable), unavailable)
})

test('fetch returns a valid normalized response', async () => {
	const result = await fetchCodexActivityBridge(undefined, {
		fetchImpl: async () => response({ ...base, toolInput: 'redacted' }),
	})
	assert.equal(result.status, 200)
	assert.deepEqual(result.payload, base)
})

test('retries localhost as IPv4 loopback only after a connection failure', async () => {
	const requested = []
	const result = await fetchCodexActivityBridge(undefined, {
		fetchImpl: async (url) => {
			requested.push(url)
			if (requested.length === 1) throw new Error('IPv6 loopback unavailable')
			return response(base)
		},
	})
	assert.equal(result.status, 200)
	assert.deepEqual(requested, [
		'http://localhost:8792/activity',
		'http://127.0.0.1:8792/activity',
	])
})

test('keeps a localhost HTTP failure bounded to the IPv4 loopback fallback', async () => {
	let calls = 0
	const result = await fetchCodexActivityBridge(undefined, {
		fetchImpl: async () => {
			calls++
			return response({ error: 'synthetic' }, 500)
		},
	})
	assert.equal(result.status, 503)
	assert.equal(calls, 2)
})

test('bridge timeout returns normalized unavailable without raw errors', async () => {
	const result = await fetchCodexActivityBridge(undefined, {
		timeoutMs: 5,
		now: () => NOW,
		fetchImpl: async () => new Promise(() => {}),
	})
	assert.equal(result.status, 503)
	assert.deepEqual(result.payload, {
		available: false,
		provider: 'codex',
		state: 'unavailable',
		sessionId: null,
		turnId: null,
		startedAt: null,
		completedAt: null,
		updatedAt: NOW.toISOString(),
		stale: true,
	})
})

for (const [name, fetchImpl] of [
	['connection failure', async () => { throw new Error('private connection detail') }],
	['non-200 response', async () => response({ error: 'private upstream detail' }, 500)],
	['malformed JSON', async () => new Response('{bad', { status: 200 })],
]) {
	test(`${name} returns sanitized unavailable`, async () => {
		const result = await fetchCodexActivityBridge(undefined, { fetchImpl, now: () => NOW })
		assert.equal(result.status, 503)
		assert.equal(result.payload.state, 'unavailable')
		assert.equal(JSON.stringify(result.payload).includes('private'), false)
		assert.deepEqual(Object.keys(result.payload), Object.keys(base))
	})
}

for (const [name, change] of [
	['provider', { provider: 'other' }],
	['state', { state: 'approval' }],
	['unknown terminal state', { state: 'interrupted', completedAt: '2026-07-31T20:12:40.000Z' }],
	['started timestamp', { startedAt: 'not-a-time' }],
	['updated timestamp', { updatedAt: '2026-07-31T20:14:14+01:00' }],
	['completion ordering', { state: 'done', completedAt: '2026-07-31T20:00:00.000Z' }],
]) {
	test(`rejects invalid ${name}`, () => {
		assert.equal(normalizeCodexActivity({ ...base, ...change }), null)
	})
}

test('Worker activity route uses the established device authorization behavior', () => {
	const env = { DEVICE_AUTH_TOKEN: 'synthetic-secret' }
	assert.equal(isAuthorizedDeviceRequest(new Request('http://local/api/codex/activity'), env), false)
	assert.equal(isAuthorizedDeviceRequest(new Request('http://local/api/codex/activity', {
		headers: { 'X-Device-Token': 'synthetic-secret' },
	}), env), true)
})
