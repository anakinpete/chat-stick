import assert from 'node:assert/strict'
import { EventEmitter } from 'node:events'
import { join, resolve } from 'node:path'
import { PassThrough } from 'node:stream'
import test from 'node:test'
import {
	readCodexAccountAndRateLimits,
	resolveCodexExecutable,
} from './codex-app-server-client.mjs'
import {
	createCodexAllowanceService,
	normalizeCodexAllowance,
} from './codex-allowance-service.mjs'

const authenticatedAccount = {
	requiresOpenaiAuth: true,
	account: { type: 'chatgpt', planType: 'plus' },
}

const fakeFileInfo = {
	size: 123,
	mtimeMs: 456,
	isFile: () => true,
}

test('honors CODEX_EXECUTABLE_PATH for an existing native exe', async () => {
	const configuredPath = 'C:\\Codex Test\\codex.exe'
	const result = await resolveCodexExecutable({
		env: { CODEX_EXECUTABLE_PATH: configuredPath, PATH: '' },
		platform: 'win32',
		arch: 'x64',
		statFile: async (path) => {
			assert.equal(path, resolve(configuredPath))
			return fakeFileInfo
		},
	})

	assert.equal(result.path, resolve(configuredPath))
	assert.equal(result.source, 'override')
	assert.equal(result.targetType, 'exe')
})

test('rejects a missing explicit override instead of silently falling back', async () => {
	await assert.rejects(
		resolveCodexExecutable({
			env: { CODEX_EXECUTABLE_PATH: 'C:\\missing\\codex.exe', PATH: 'C:\\bin' },
			platform: 'win32',
			statFile: async () => {
				throw Object.assign(new Error('missing'), { code: 'ENOENT' })
			},
		}),
		(error) => error.code === 'codex_override_missing'
	)
})

test('discovers the native executable inside the global npm package', async () => {
	const appData = 'C:\\Users\\Test\\AppData\\Roaming'
	const expected = join(
		appData,
		'npm',
		'node_modules',
		'@openai',
		'codex',
		'node_modules',
		'@openai/codex-win32-x64',
		'vendor',
		'x86_64-pc-windows-msvc',
		'bin',
		'codex.exe'
	)
	const result = await resolveCodexExecutable({
		env: { APPDATA: appData, PATH: `${appData}\\npm` },
		platform: 'win32',
		arch: 'x64',
		statFile: async (path) => {
			if (path === expected) return fakeFileInfo
			throw Object.assign(new Error('missing'), { code: 'ENOENT' })
		},
	})

	assert.equal(result.path, expected)
	assert.equal(result.source, 'npm-package')
	assert.equal(result.targetType, 'exe')
})

test('does not select a cmd wrapper from PATH', async () => {
	const checked = []
	await assert.rejects(
		resolveCodexExecutable({
			env: { PATH: 'C:\\bin' },
			platform: 'win32',
			arch: 'x64',
			statFile: async (path) => {
				checked.push(path)
				throw Object.assign(new Error('missing'), { code: 'ENOENT' })
			},
		}),
		(error) => error.code === 'codex_not_found'
	)
	assert.equal(checked.some((path) => path.endsWith('.cmd')), false)
})

function rateLimitSnapshot(primary, secondary = null) {
	return {
		limitId: 'codex',
		limitName: null,
		primary,
		secondary,
		credits: null,
	}
}

test('normalizes one generic window and derives remaining percentage', () => {
	const result = normalizeCodexAllowance(
		{
			account: authenticatedAccount,
			rateLimits: {
				rateLimits: rateLimitSnapshot({
					usedPercent: 23,
					windowDurationMins: 300,
					resetsAt: 1_800_000_000,
				}),
			},
		},
		new Date('2026-01-01T00:00:00Z')
	)

	assert.equal(result.windows.length, 1)
	assert.deepEqual(result.windows[0], {
		limitId: 'codex',
		limitLabel: null,
		kind: 'primary',
		label: '5h',
		windowMinutes: 300,
		usedPercent: 23,
		remainingPercent: 77,
		resetsAt: '2027-01-15T08:00:00.000Z',
	})
})

test('normalizes multiple buckets and windows without assuming fixed durations', () => {
	const result = normalizeCodexAllowance({
		account: authenticatedAccount,
		rateLimits: {
			rateLimits: rateLimitSnapshot({ usedPercent: 1 }),
			rateLimitsByLimitId: {
				codex: rateLimitSnapshot(
					{ usedPercent: 20, windowDurationMins: 60, resetsAt: null },
					{ usedPercent: 40, windowDurationMins: 10_080, resetsAt: null }
				),
				other: {
					limitId: 'other',
					limitName: 'Other allowance',
					primary: { usedPercent: 5, windowDurationMins: 45, resetsAt: null },
				},
			},
		},
	})

	assert.deepEqual(
		result.windows.map(({ limitId, kind, label }) => ({ limitId, kind, label })),
		[
			{ limitId: 'codex', kind: 'primary', label: '1h' },
			{ limitId: 'codex', kind: 'secondary', label: 'weekly' },
			{ limitId: 'other', kind: 'primary', label: '45m' },
		]
	)
	assert.equal(result.windows[0].resetsAt, null)
})

test('represents missing credit data honestly', () => {
	const result = normalizeCodexAllowance({
		account: authenticatedAccount,
		rateLimits: {
			rateLimits: rateLimitSnapshot({ usedPercent: 0, resetsAt: null }),
		},
	})

	assert.deepEqual(result.credits, {
		available: false,
		hasCredits: null,
		unlimited: null,
		balance: null,
	})
	assert.deepEqual(result.rateLimitResetCredits, {
		available: false,
		availableCount: null,
	})
})

test('returns unauthenticated state without exposing account data', async () => {
	const service = createCodexAllowanceService({
		read: async () => ({
			account: { requiresOpenaiAuth: true, account: null },
			rateLimits: {},
		}),
	})
	const result = await service.getAllowance()

	assert.equal(result.available, false)
	assert.equal(result.state, 'unauthenticated')
	assert.deepEqual(result.error, { code: 'unauthenticated' })
	assert.equal(JSON.stringify(result).includes('account'), false)
})

test('uses stale last-success data when refresh fails', async () => {
	let currentMs = Date.parse('2026-01-01T00:00:00Z')
	let calls = 0
	const service = createCodexAllowanceService({
		cacheMs: 1_000,
		now: () => new Date(currentMs),
		read: async () => {
			calls += 1
			if (calls > 1) throw Object.assign(new Error('failed'), { code: 'timeout' })
			return {
				account: authenticatedAccount,
				rateLimits: {
					rateLimits: rateLimitSnapshot({ usedPercent: 10, resetsAt: null }),
				},
			}
		},
	})

	const fresh = await service.getAllowance()
	currentMs += 2_000
	const stale = await service.getAllowance()
	const staleRetrySuppressed = await service.getAllowance()

	assert.equal(fresh.stale, false)
	assert.equal(stale.available, true)
	assert.equal(stale.state, 'stale')
	assert.equal(stale.stale, true)
	assert.deepEqual(stale.error, { code: 'timeout' })
	assert.equal(stale.updatedAt, fresh.updatedAt)
	assert.equal(staleRetrySuppressed.stale, true)
	assert.equal(calls, 2)
})

function fakeChild(onRequest) {
	const child = new EventEmitter()
	child.stdout = new PassThrough()
	child.stderr = new PassThrough()
	child.stdin = {
		write(line) {
			onRequest(JSON.parse(line), child)
			return true
		},
	}
	child.exitCode = null
	child.killed = false
	child.kill = () => {
		child.killed = true
		return true
	}
	return child
}

test('performs a successful JSON-RPC query using native spawn options', async () => {
	let child
	let spawnCall
	const methods = []
	const result = await readCodexAccountAndRateLimits({
		executable: 'C:\\native\\codex.exe',
		spawnProcess: (executable, args, options) => {
			spawnCall = { executable, args, options }
			child = fakeChild((request, process) => {
				methods.push(request.method)
				if (request.method === 'initialize') {
					queueMicrotask(() => process.stdout.write(`${JSON.stringify({ id: 0, result: {} })}\n`))
				} else if (request.method === 'account/read') {
					queueMicrotask(() =>
						process.stdout.write(
							`${JSON.stringify({ id: 1, result: authenticatedAccount })}\n`
						)
					)
				} else if (request.method === 'account/rateLimits/read') {
					queueMicrotask(() =>
						process.stdout.write(
							`${JSON.stringify({
								id: 2,
								result: {
									rateLimits: rateLimitSnapshot({ usedPercent: 12 }),
								},
							})}\n`
						)
					)
				}
			})
			return child
		},
	})

	assert.deepEqual(spawnCall, {
		executable: 'C:\\native\\codex.exe',
		args: ['app-server'],
		options: { shell: false, stdio: ['pipe', 'pipe', 'pipe'] },
	})
	assert.deepEqual(methods, [
		'initialize',
		'initialized',
		'account/read',
		'account/rateLimits/read',
	])
	assert.equal(result.rateLimits.rateLimits.primary.usedPercent, 12)
	assert.equal(child.killed, true)
})

test('maps a synchronous spawn EINVAL to a sanitized diagnostic', async () => {
	await assert.rejects(
		readCodexAccountAndRateLimits({
			executable: 'C:\\native\\codex.exe',
			spawnProcess: () => {
				throw Object.assign(new Error('invalid argument'), { code: 'EINVAL' })
			},
		}),
		(error) => error.code === 'spawn_einval'
	)
})

test('rejects malformed app-server JSON and cleans up the subprocess', async () => {
	let child
	await assert.rejects(
		readCodexAccountAndRateLimits({
			executable: 'codex-test',
			spawnProcess: () => {
				child = fakeChild((request, process) => {
					if (request.method === 'initialize') {
						queueMicrotask(() => process.stdout.write('not-json\n'))
					}
				})
				return child
			},
		}),
		(error) => error.code === 'malformed_response'
	)
	assert.equal(child.killed, true)
})

test('times out and cleans up an unresponsive subprocess', async () => {
	let child
	await assert.rejects(
		readCodexAccountAndRateLimits({
			executable: 'codex-test',
			timeoutMs: 10,
			spawnProcess: () => {
				child = fakeChild(() => {})
				return child
			},
		}),
		(error) => error.code === 'timeout'
	)
	assert.equal(child.killed, true)
})
