import assert from 'node:assert/strict'
import test from 'node:test'
import {
	fetchCodexAllowanceBridge,
	resolveCodexAllowanceBridgeUrl,
} from '../src/codex-allowance-proxy.ts'

const normalizedPayload = {
	available: true,
	state: 'available',
	source: 'codex-app-server',
	windows: [
		{
			limitId: 'codex',
			kind: 'primary',
			remainingPercent: 50,
			resetsAt: '2026-08-05T08:17:52.000Z',
		},
	],
	updatedAt: '2026-07-31T12:00:00.000Z',
	stale: false,
}

test('uses localhost and preserves an explicit allowance endpoint path', () => {
	assert.equal(resolveCodexAllowanceBridgeUrl(), 'http://localhost:8790/allowance')
	assert.equal(
		resolveCodexAllowanceBridgeUrl('http://localhost:8790/allowance'),
		'http://localhost:8790/allowance'
	)
	assert.equal(
		resolveCodexAllowanceBridgeUrl('http://localhost:8790'),
		'http://localhost:8790/allowance'
	)
})

test('returns a successful normalized bridge response', async () => {
	let requestedUrl
	const result = await fetchCodexAllowanceBridge('http://localhost:8790/allowance', {
		fetchImpl: async (url) => {
			requestedUrl = url
			return new Response(JSON.stringify(normalizedPayload), {
				status: 200,
				headers: { 'Content-Type': 'application/json' },
			})
		},
	})

	assert.equal(requestedUrl, 'http://localhost:8790/allowance')
	assert.equal(result.status, 200)
	assert.deepEqual(result.payload, normalizedPayload)
})

test('returns bridge_timeout promptly when fetch never settles', async () => {
	const startedAt = Date.now()
	const result = await fetchCodexAllowanceBridge('http://localhost:8790/allowance', {
		timeoutMs: 10,
		fetchImpl: async () => new Promise(() => {}),
	})

	assert.equal(result.status, 503)
	assert.deepEqual(result.payload.error, { code: 'bridge_timeout' })
	assert.ok(Date.now() - startedAt < 1_000)
})

test('returns bridge_unavailable when the bridge cannot be reached', async () => {
	const result = await fetchCodexAllowanceBridge('http://localhost:8790/allowance', {
		fetchImpl: async () => {
			throw Object.assign(new Error('connection refused'), { code: 'ECONNREFUSED' })
		},
	})

	assert.equal(result.status, 503)
	assert.deepEqual(result.payload.error, { code: 'bridge_unavailable' })
})

test('returns a sanitized 502 for malformed bridge JSON', async () => {
	const result = await fetchCodexAllowanceBridge('http://localhost:8790/allowance', {
		fetchImpl: async () =>
			new Response('{not-json', {
				status: 200,
				headers: { 'Content-Type': 'application/json' },
			}),
	})

	assert.equal(result.status, 502)
	assert.deepEqual(result.payload.error, { code: 'bridge_malformed_response' })
	assert.equal(JSON.stringify(result.payload).includes('not-json'), false)
})
