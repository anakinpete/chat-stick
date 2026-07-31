const DEFAULT_BRIDGE_URL = 'http://localhost:8790/allowance'
const DEFAULT_TIMEOUT_MS = 3_000

type FetchLike = (input: string, init?: RequestInit) => Promise<Response>

interface BridgeDependencies {
	fetchImpl?: FetchLike
	timeoutMs?: number
	now?: () => Date
}

export interface CodexAllowanceBridgeResult {
	status: number
	payload: unknown
}

class BridgeRequestError extends Error {
	readonly code: string

	constructor(code: string) {
		super(code)
		this.code = code
	}
}

export function resolveCodexAllowanceBridgeUrl(configuredUrl?: string): string {
	const url = new URL(configuredUrl?.trim() || DEFAULT_BRIDGE_URL)
	if (url.pathname === '/' || url.pathname === '') url.pathname = '/allowance'
	return url.toString()
}

function isNormalizedAllowance(payload: unknown): boolean {
	if (!payload || typeof payload !== 'object') return false
	const value = payload as Record<string, unknown>
	return (
		typeof value.available === 'boolean' &&
		typeof value.state === 'string' &&
		value.source === 'codex-app-server' &&
		Array.isArray(value.windows) &&
		typeof value.updatedAt === 'string' &&
		typeof value.stale === 'boolean'
	)
}

function unavailablePayload(code: string, now: Date): unknown {
	return {
		available: false,
		state: 'unavailable',
		source: 'codex-app-server',
		windows: [],
		credits: { available: false, hasCredits: null, unlimited: null, balance: null },
		rateLimitResetCredits: { available: false, availableCount: null },
		updatedAt: now.toISOString(),
		stale: false,
		error: { code },
	}
}

export async function fetchCodexAllowanceBridge(
	configuredUrl?: string,
	dependencies: BridgeDependencies = {}
): Promise<CodexAllowanceBridgeResult> {
	const fetchImpl = dependencies.fetchImpl ?? fetch
	const timeoutMs = dependencies.timeoutMs ?? DEFAULT_TIMEOUT_MS
	const now = dependencies.now ?? (() => new Date())
	let bridgeUrl: string

	try {
		bridgeUrl = resolveCodexAllowanceBridgeUrl(configuredUrl)
	} catch {
		return { status: 503, payload: unavailablePayload('bridge_invalid_url', now()) }
	}

	const controller = new AbortController()
	let timeout: ReturnType<typeof setTimeout> | undefined
	const timeoutPromise = new Promise<never>((_, reject) => {
		timeout = setTimeout(() => {
			controller.abort()
			reject(new BridgeRequestError('bridge_timeout'))
		}, timeoutMs)
	})

	const requestPromise = (async () => {
		const response = await fetchImpl(bridgeUrl, {
			headers: { Accept: 'application/json' },
			signal: controller.signal,
		})
		let payload: unknown
		try {
			payload = await response.json()
		} catch {
			throw new BridgeRequestError('bridge_malformed_response')
		}
		if (!isNormalizedAllowance(payload)) {
			throw new BridgeRequestError('bridge_malformed_response')
		}
		return { status: response.status, payload }
	})()

	try {
		return await Promise.race([requestPromise, timeoutPromise])
	} catch (error) {
		const code = error instanceof BridgeRequestError ? error.code : 'bridge_unavailable'
		return {
			status: code === 'bridge_malformed_response' ? 502 : 503,
			payload: unavailablePayload(code, now()),
		}
	} finally {
		if (timeout !== undefined) clearTimeout(timeout)
	}
}
