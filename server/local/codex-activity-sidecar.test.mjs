import assert from 'node:assert/strict'
import test from 'node:test'
import {
	createCodexActivityService,
	createCodexActivitySidecar,
	normalizeCodexActivity,
} from './codex-activity-sidecar.mjs'

const SESSION_ID = '11111111-1111-4111-8111-111111111111'
const TURN_ID = 'aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa'
const INITIAL_TIME = '2026-07-31T20:14:00.000Z'

function runningActivity(overrides = {}) {
	return {
		available: true,
		provider: 'codex',
		state: 'running',
		sessionId: SESSION_ID,
		turnId: TURN_ID,
		startedAt: '2026-07-31T20:10:40.000Z',
		completedAt: null,
		updatedAt: INITIAL_TIME,
		stale: false,
		...overrides,
	}
}

function idleActivity(overrides = {}) {
	return {
		available: true,
		provider: 'codex',
		state: 'idle',
		sessionId: null,
		turnId: null,
		startedAt: null,
		completedAt: null,
		updatedAt: INITIAL_TIME,
		stale: false,
		...overrides,
	}
}

async function startTestSidecar(t, options = {}) {
	const logs = []
	const sidecar = createCodexActivitySidecar({
		port: 0,
		serviceOptions: {
			observe: options.observe ?? (async () => runningActivity()),
			pollMs: options.pollMs ?? 60_000,
			now: options.now,
			logError: (message) => logs.push(message),
		},
	})
	await sidecar.start()
	t.after(() => sidecar.close())
	const address = sidecar.server.address()
	return {
		sidecar,
		logs,
		url: `http://127.0.0.1:${address.port}`,
		address,
	}
}

test('binds to the IPv4 loopback address only', async (t) => {
	const { address } = await startTestSidecar(t)
	assert.equal(address.address, '127.0.0.1')
	assert.equal(address.family, 'IPv4')
})

test('GET /activity returns normalized JSON', async (t) => {
	const { url } = await startTestSidecar(t)
	const response = await fetch(`${url}/activity`)
	assert.equal(response.status, 200)
	assert.match(response.headers.get('content-type'), /^application\/json/)
	assert.deepEqual(await response.json(), runningActivity())
})

test('normalizes and serves a cancelled terminal turn', async (t) => {
	const cancelled = runningActivity({
		state: 'cancelled',
		completedAt: '2026-07-31T20:12:40.000Z',
		updatedAt: '2026-07-31T20:12:40.000Z',
	})
	const { url } = await startTestSidecar(t, { observe: async () => cancelled })
	const response = await fetch(`${url}/activity`)
	assert.equal(response.status, 200)
	assert.deepEqual(await response.json(), cancelled)
})

test('GET /health returns a small structural response', async (t) => {
	const { url } = await startTestSidecar(t)
	const response = await fetch(`${url}/health`)
	assert.equal(response.status, 200)
	assert.deepEqual(await response.json(), { ok: true, service: 'codex-activity' })
})

test('unknown route returns a bounded 404 response', async (t) => {
	const { url } = await startTestSidecar(t)
	const response = await fetch(`${url}/unknown`)
	assert.equal(response.status, 404)
	assert.deepEqual(await response.json(), { error: { code: 'not_found' } })
})

test('unsupported method returns 405 and an Allow header', async (t) => {
	const { url } = await startTestSidecar(t)
	const response = await fetch(`${url}/activity`, { method: 'POST' })
	assert.equal(response.status, 405)
	assert.equal(response.headers.get('allow'), 'GET')
	assert.deepEqual(await response.json(), { error: { code: 'method_not_allowed' } })
})

test('non-loopback Host header cannot retrieve activity', async (t) => {
	const { address } = await startTestSidecar(t)
	const response = await fetch(`http://0.0.0.0:${address.port}/activity`)
	assert.equal(response.status, 403)
	assert.deepEqual(await response.json(), { error: { code: 'loopback_only' } })
})

test('observer is called immediately on startup', async (t) => {
	let calls = 0
	await startTestSidecar(t, {
		observe: async () => {
			calls += 1
			return idleActivity()
		},
	})
	assert.equal(calls, 1)
})

test('periodic reconciliation updates the shared cache', async () => {
	let intervalCallback
	let calls = 0
	const service = createCodexActivityService({
		pollMs: 2_000,
		observe: async () => {
			calls += 1
			return calls === 1
				? idleActivity()
				: runningActivity({ updatedAt: '2026-07-31T20:14:02.000Z' })
		},
		setIntervalFn: (callback) => {
			intervalCallback = callback
			return { unref() {} }
		},
		clearIntervalFn: () => {},
		logError: () => {},
	})
	await service.start()
	assert.equal((await service.getActivity()).state, 'idle')
	intervalCallback()
	const updated = await service.getActivity()
	assert.equal(calls, 2)
	assert.equal(updated.state, 'running')
	service.stop()
})

test('concurrent requests await one shared in-flight observation', async (t) => {
	let calls = 0
	let resolveSecond
	const secondObservation = new Promise((resolve) => {
		resolveSecond = resolve
	})
	const service = createCodexActivityService({
		pollMs: 60_000,
		observe: async () => {
			calls += 1
			return calls === 1 ? idleActivity() : secondObservation
		},
		logError: () => {},
	})
	const originalGetActivity = service.getActivity
	let requestCount = 0
	let resolveBothRequestsStarted
	const bothRequestsStarted = new Promise((resolve) => {
		resolveBothRequestsStarted = resolve
	})
	service.getActivity = async () => {
		requestCount += 1
		if (requestCount === 2) resolveBothRequestsStarted()
		return originalGetActivity()
	}
	const sidecar = createCodexActivitySidecar({ port: 0, service })
	await sidecar.start()
	t.after(() => sidecar.close())
	const address = sidecar.server.address()
	const url = `http://127.0.0.1:${address.port}/activity`

	const reconciliation = service.reconcile()
	const firstRequest = fetch(url).then((response) => response.json())
	const secondRequest = fetch(url).then((response) => response.json())
	await bothRequestsStarted
	assert.equal(calls, 2)
	resolveSecond(runningActivity())
	const [reconciled, first, second] = await Promise.all([
		reconciliation,
		firstRequest,
		secondRequest,
	])

	assert.equal(calls, 2)
	assert.deepEqual(first, reconciled)
	assert.deepEqual(second, reconciled)
})

test('observer exception with no prior value returns unavailable', async () => {
	const logs = []
	const service = createCodexActivityService({
		observe: async () => {
			throw new Error('invented private failure detail')
		},
		now: () => new Date('2026-07-31T20:15:00Z'),
		logError: (message) => logs.push(message),
	})
	const result = await service.getActivity()
	assert.deepEqual(result, {
		available: false,
		provider: 'codex',
		state: 'unavailable',
		sessionId: null,
		turnId: null,
		startedAt: null,
		completedAt: null,
		updatedAt: '2026-07-31T20:15:00.000Z',
		stale: true,
	})
	assert.deepEqual(logs, ['[codex-activity] observer_failure kind=exception cached=no'])
})

test('observer timeout returns unavailable without exposing timeout details', async () => {
	const logs = []
	const service = createCodexActivityService({
		observe: async () => new Promise(() => {}),
		observerTimeoutMs: 5,
		now: () => new Date('2026-07-31T20:15:00Z'),
		logError: (message) => logs.push(message),
	})
	const result = await service.getActivity()
	assert.equal(result.state, 'unavailable')
	assert.equal(result.stale, true)
	assert.deepEqual(logs, ['[codex-activity] observer_failure kind=timeout cached=no'])
})

test('observer exception preserves a prior value as stale', async () => {
	let calls = 0
	let currentTime = '2026-07-31T20:14:00Z'
	const service = createCodexActivityService({
		observe: async () => {
			calls += 1
			if (calls > 1) throw new Error('invented failure')
			return runningActivity()
		},
		now: () => new Date(currentTime),
		setIntervalFn: () => ({ unref() {} }),
		clearIntervalFn: () => {},
		logError: () => {},
	})
	await service.start()
	currentTime = '2026-07-31T20:15:00Z'
	const stale = await service.reconcile()
	assert.deepEqual(stale, {
		...runningActivity(),
		updatedAt: '2026-07-31T20:15:00.000Z',
		stale: true,
	})
	service.stop()
})

test('invalid observer output is rejected safely', async () => {
	const service = createCodexActivityService({
		observe: async () => ({
			available: true,
			provider: 'codex',
			state: 'approval',
			rawError: 'must-not-escape',
		}),
		now: () => new Date('2026-07-31T20:15:00Z'),
		logError: () => {},
	})
	const result = await service.getActivity()
	assert.equal(result.state, 'unavailable')
	assert.equal(JSON.stringify(result).includes('must-not-escape'), false)
})

test('HTTP failure response contains no raw error or stack trace', async (t) => {
	const secret = 'invented-sensitive-error-text'
	const { url, logs } = await startTestSidecar(t, {
		observe: async () => {
			throw new Error(secret)
		},
	})
	const response = await fetch(`${url}/activity`)
	const body = await response.text()
	assert.equal(response.status, 200)
	assert.equal(body.includes(secret), false)
	assert.equal(body.includes('stack'), false)
	assert.equal(logs.join(' ').includes(secret), false)
})

test('clean shutdown stops polling and closes the server', async () => {
	let clearedTimer = null
	const timer = { unref() {} }
	const service = createCodexActivityService({
		observe: async () => idleActivity(),
		setIntervalFn: () => timer,
		clearIntervalFn: (value) => {
			clearedTimer = value
		},
		logError: () => {},
	})
	const sidecar = createCodexActivitySidecar({ port: 0, service })
	await sidecar.start()
	assert.equal(sidecar.server.listening, true)
	await sidecar.close()
	assert.equal(clearedTimer, timer)
	assert.equal(sidecar.server.listening, false)
})

test('configuration rejects every non-loopback bind address', () => {
	for (const host of ['0.0.0.0', 'localhost', '::1', '192.168.1.10']) {
		assert.throws(
			() => createCodexActivitySidecar({ host }),
			(error) => error.code === 'invalid_configuration'
		)
	}
})

test('privacy response contains only allowlisted activity fields', async (t) => {
	const secret = 'invented-content-never-returned'
	const { url } = await startTestSidecar(t, {
		observe: async () => ({
			...runningActivity(),
			prompt: secret,
			response: secret,
			tool: secret,
			path: secret,
			quota: secret,
			error: { stack: secret },
		}),
	})
	const result = await fetch(`${url}/activity`).then((response) => response.json())
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
	assert.equal(JSON.stringify(result).includes(secret), false)
})

test('normalizer converts unavailable observer output to stale unavailable', () => {
	const result = normalizeCodexActivity({
		available: false,
		provider: 'codex',
		state: 'unavailable',
		sessionId: null,
		turnId: null,
		startedAt: null,
		completedAt: null,
		updatedAt: INITIAL_TIME,
		stale: false,
	})
	assert.equal(result.state, 'unavailable')
	assert.equal(result.stale, true)
})
