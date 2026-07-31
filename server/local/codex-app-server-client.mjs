import { spawn } from 'node:child_process'
import { copyFile, mkdir, stat } from 'node:fs/promises'
import { tmpdir } from 'node:os'
import { basename, delimiter, extname, join, resolve } from 'node:path'

const DEFAULT_TIMEOUT_MS = 10_000
const APP_SERVER_ARGUMENTS = ['app-server']

export class CodexAppServerError extends Error {
	constructor(code, message) {
		super(message)
		this.name = 'CodexAppServerError'
		this.code = code
	}
}

function targetType(path) {
	const extension = extname(path).toLowerCase()
	if (extension === '.exe') return 'exe'
	if (extension === '.cmd') return 'cmd'
	if (extension === '.bat') return 'bat'
	return extension ? extension.slice(1) : 'native'
}

async function existingFile(path, statFile) {
	try {
		const info = await statFile(path)
		return info.isFile() ? info : null
	} catch {
		return null
	}
}

function windowsNpmNativeCandidates(env, arch) {
	const packageArch = arch === 'arm64' ? 'arm64' : 'x64'
	const rustTarget = arch === 'arm64' ? 'aarch64-pc-windows-msvc' : 'x86_64-pc-windows-msvc'
	const npmRoots = []

	if (env.APPDATA) npmRoots.push(join(env.APPDATA, 'npm', 'node_modules'))
	for (const directory of (env.PATH || '').split(delimiter)) {
		if (directory) npmRoots.push(join(directory, 'node_modules'))
	}

	return [...new Set(npmRoots)].map((root) =>
		join(
			root,
			'@openai',
			'codex',
			'node_modules',
			`@openai/codex-win32-${packageArch}`,
			'vendor',
			rustTarget,
			'bin',
			'codex.exe'
		)
	)
}

function pathNativeCandidates(env, platform) {
	const executableName = platform === 'win32' ? 'codex.exe' : 'codex'
	return [...new Set((env.PATH || '').split(delimiter).filter(Boolean))].map((directory) =>
		join(directory, executableName)
	)
}

export async function resolveCodexExecutable(options = {}) {
	const env = options.env ?? process.env
	const platform = options.platform ?? process.platform
	const arch = options.arch ?? process.arch
	const statFile = options.statFile ?? stat
	const configured = (env.CODEX_EXECUTABLE_PATH || env.CODEX_EXECUTABLE || '').trim()

	if (configured) {
		const path = resolve(configured)
		const info = await existingFile(path, statFile)
		if (!info) {
			throw new CodexAppServerError(
				'codex_override_missing',
				'Configured Codex executable does not exist'
			)
		}
		if (platform === 'win32' && targetType(path) !== 'exe') {
			throw new CodexAppServerError(
				'codex_override_not_native',
				'Configured Codex executable must be a native .exe on Windows'
			)
		}
		return { path, source: 'override', targetType: targetType(path), info }
	}

	if (platform === 'win32') {
		for (const path of windowsNpmNativeCandidates(env, arch)) {
			const info = await existingFile(path, statFile)
			if (info) return { path, source: 'npm-package', targetType: 'exe', info }
		}
	}

	for (const path of pathNativeCandidates(env, platform)) {
		const info = await existingFile(path, statFile)
		if (info) return { path, source: 'PATH', targetType: targetType(path), info }
	}

	throw new CodexAppServerError(
		'codex_not_found',
		'Native Codex CLI executable is not available'
	)
}

async function stageWindowsExecutable(resolution) {
	if (process.platform !== 'win32' || resolution.targetType !== 'exe' || !resolution.info) {
		return resolution
	}

	const cacheDirectory = join(tmpdir(), 'm5-companion-codex')
	const cacheName = `${basename(resolution.path, '.exe')}-${resolution.info.size}-${Math.trunc(
		resolution.info.mtimeMs
	)}.exe`
	const stagedPath = join(cacheDirectory, cacheName)
	let stagedInfo = await existingFile(stagedPath, stat)
	if (!stagedInfo) {
		await mkdir(cacheDirectory, { recursive: true })
		await copyFile(resolution.path, stagedPath)
		stagedInfo = await stat(stagedPath)
	}
	return { path: stagedPath, source: 'staged-copy', targetType: 'exe', info: stagedInfo }
}

function spawnError(error) {
	if (error?.code === 'ENOENT') {
		return new CodexAppServerError('codex_not_found', 'Codex app-server executable was not found')
	}
	if (error?.code === 'EINVAL') {
		return new CodexAppServerError('spawn_einval', 'Codex app-server spawn options were rejected')
	}
	if (error?.code === 'EACCES' || error?.code === 'EPERM') {
		return new CodexAppServerError(
			'spawn_permission_denied',
			'Codex app-server executable could not be launched directly'
		)
	}
	return new CodexAppServerError('subprocess_failed', 'Codex app-server could not be started')
}

function queryAppServer(executable, options) {
	const timeoutMs = options.timeoutMs ?? DEFAULT_TIMEOUT_MS
	const spawnProcess = options.spawnProcess ?? spawn

	return new Promise((resolveQuery, rejectQuery) => {
		let settled = false
		let initialized = false
		let accountResult = null
		let rateLimitsResult = null
		let stdoutBuffer = ''
		let timer = null
		let child

		const cleanup = () => {
			clearTimeout(timer)
			child?.stdout?.removeAllListeners()
			child?.stderr?.removeAllListeners()
			child?.removeAllListeners()
			if (child && child.exitCode == null && !child.killed) child.kill()
		}

		const fail = (error) => {
			if (settled) return
			settled = true
			cleanup()
			rejectQuery(error)
		}

		const failCode = (code, message) => fail(new CodexAppServerError(code, message))

		try {
			child = spawnProcess(executable, APP_SERVER_ARGUMENTS, {
				shell: false,
				stdio: ['pipe', 'pipe', 'pipe'],
			})
		} catch (error) {
			fail(spawnError(error))
			return
		}

		const succeedIfComplete = () => {
			if (settled || accountResult == null || rateLimitsResult == null) return
			settled = true
			cleanup()
			resolveQuery({ account: accountResult, rateLimits: rateLimitsResult })
		}

		const send = (message) => {
			try {
				child.stdin.write(`${JSON.stringify(message)}\n`)
			} catch {
				failCode('protocol_write_failed', 'Could not write to Codex app-server')
			}
		}

		const handleMessage = (message) => {
			if (message.id === 0) {
				if (message.error) {
					failCode('initialization_failed', 'Codex app-server initialization failed')
					return
				}
				initialized = true
				send({ method: 'initialized' })
				send({ method: 'account/read', id: 1, params: { refreshToken: false } })
				return
			}

			if (message.id === 1) {
				if (message.error) {
					failCode('account_read_failed', 'Codex account state could not be read')
					return
				}
				accountResult = message.result
				if (accountResult?.requiresOpenaiAuth && !accountResult?.account) {
					failCode('unauthenticated', 'Codex is not authenticated')
					return
				}
				send({ method: 'account/rateLimits/read', id: 2 })
				succeedIfComplete()
				return
			}

			if (message.id === 2) {
				if (message.error) {
					failCode('rate_limits_read_failed', 'Codex rate limits could not be read')
					return
				}
				rateLimitsResult = message.result
				succeedIfComplete()
			}
		}

		child.stdout.on('data', (chunk) => {
			stdoutBuffer += chunk.toString('utf8')
			let newlineIndex
			while ((newlineIndex = stdoutBuffer.indexOf('\n')) >= 0) {
				const line = stdoutBuffer.slice(0, newlineIndex).trim()
				stdoutBuffer = stdoutBuffer.slice(newlineIndex + 1)
				if (!line) continue
				try {
					handleMessage(JSON.parse(line))
				} catch {
					failCode('malformed_response', 'Codex app-server returned malformed JSON')
				}
			}
		})

		child.stderr.on('data', () => {
			// Raw app-server stderr is deliberately discarded to avoid account leakage.
		})

		child.on('error', (error) => fail(spawnError(error)))
		child.on('exit', () => {
			if (settled) return
			const code = initialized ? 'subprocess_exited' : 'initialization_failed'
			failCode(code, 'Codex app-server exited before returning rate limits')
		})

		timer = setTimeout(() => {
			failCode('timeout', 'Codex app-server request timed out')
		}, timeoutMs)

		send({
			method: 'initialize',
			id: 0,
			params: {
				clientInfo: {
					name: 'm5_companion',
					title: 'M5 Sci-Fi Companion',
					version: '0.1.0',
				},
			},
		})
	})
}

export async function readCodexAccountAndRateLimits(options = {}) {
	const resolution = options.resolution ??
		(options.executable
			? { path: options.executable, source: 'provided', targetType: targetType(options.executable) }
			: await resolveCodexExecutable(options.resolveOptions))

	try {
		return await queryAppServer(resolution.path, options)
	} catch (error) {
		if (error?.code !== 'spawn_permission_denied') throw error
		const staged = await stageWindowsExecutable(resolution)
		if (staged.path === resolution.path) throw error
		options.onResolution?.(staged)
		return queryAppServer(staged.path, options)
	}
}
