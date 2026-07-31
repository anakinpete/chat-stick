const DEFAULT_BRIDGE_URL = 'http://localhost:8792/activity'
const DEFAULT_TIMEOUT_MS = 3_000

const ACTIVITY_STATES = new Set([
	'idle',
	'running',
	'done',
	'cancelled',
	'stale',
	'offline',
	'unavailable',
])

type FetchLike = (input: string, init?: RequestInit) => Promise<Response>

interface ActivityBridgeDependencies {
	fetchImpl?: FetchLike
	timeoutMs?: number
	now?: () => Date
}

export interface CodexActivityPayload {
	available: boolean
	provider: 'codex'
	state: 'idle' | 'running' | 'done' | 'cancelled' | 'stale' | 'offline' | 'unavailable'
	sessionId: string | null
	turnId: string | null
	startedAt: string | null
	completedAt: string | null
	updatedAt: string
	stale: boolean
}

export interface CodexActivityBridgeResult {
	status: number
	payload: CodexActivityPayload
}

export function resolveCodexActivityBridgeUrl(configuredUrl?: string): string {
	const url = new URL(configuredUrl?.trim() || DEFAULT_BRIDGE_URL)
	if (url.pathname === '/' || url.pathname === '') url.pathname = '/activity'
	return url.toString()
}

function isUtcIso8601(value: unknown): value is string {
	if (typeof value !== 'string' || !/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.\d{1,9})?Z$/.test(value)) {
		return false
	}
	return Number.isFinite(Date.parse(value))
}

function isNullableId(value: unknown): value is string | null {
	return value === null || (typeof value === 'string' && value.length > 0 && value.length <= 256)
}

export function normalizeCodexActivity(payload: unknown): CodexActivityPayload | null {
	if (!payload || typeof payload !== 'object' || Array.isArray(payload)) return null
	const value = payload as Record<string, unknown>
	if (
		typeof value.available !== 'boolean' ||
		value.provider !== 'codex' ||
		typeof value.state !== 'string' ||
		!ACTIVITY_STATES.has(value.state) ||
		!isNullableId(value.sessionId) ||
		!isNullableId(value.turnId) ||
		!(value.startedAt === null || isUtcIso8601(value.startedAt)) ||
		!(value.completedAt === null || isUtcIso8601(value.completedAt)) ||
		!isUtcIso8601(value.updatedAt) ||
		typeof value.stale !== 'boolean'
	) {
		return null
	}

	if (
		value.startedAt !== null &&
		value.completedAt !== null &&
		Date.parse(value.completedAt) < Date.parse(value.startedAt)
	) {
		return null
	}

	const state = value.state as CodexActivityPayload['state']
	const hasTurn = value.sessionId !== null && value.turnId !== null && value.startedAt !== null
	if ((state === 'running' || state === 'stale') && (!value.available || !hasTurn || value.completedAt !== null)) {
		return null
	}
	if (state === 'running' && value.stale) return null
	if (state === 'stale' && !value.stale) return null
	if (
		(state === 'done' || state === 'cancelled') &&
		(!value.available || !hasTurn || value.completedAt === null)
	) return null
	if (
		(state === 'idle' || state === 'offline' || state === 'unavailable') &&
		(value.sessionId !== null || value.turnId !== null || value.startedAt !== null || value.completedAt !== null)
	) return null
	if ((state === 'idle' || state === 'offline') && !value.available) return null
	if (state === 'unavailable' && (value.available || !value.stale)) return null

	return {
		available: value.available,
		provider: 'codex',
		state,
		sessionId: value.sessionId,
		turnId: value.turnId,
		startedAt: value.startedAt,
		completedAt: value.completedAt,
		updatedAt: value.updatedAt,
		stale: value.stale,
	}
}

export function unavailableCodexActivity(now = new Date()): CodexActivityPayload {
	return {
		available: false,
		provider: 'codex',
		state: 'unavailable',
		sessionId: null,
		turnId: null,
		startedAt: null,
		completedAt: null,
		updatedAt: now.toISOString(),
		stale: true,
	}
}

export async function fetchCodexActivityBridge(
	configuredUrl?: string,
	dependencies: ActivityBridgeDependencies = {}
): Promise<CodexActivityBridgeResult> {
	const fetchImpl = dependencies.fetchImpl ?? fetch
	const timeoutMs = dependencies.timeoutMs ?? DEFAULT_TIMEOUT_MS
	const now = dependencies.now ?? (() => new Date())
	let bridgeUrl: string

	try {
		bridgeUrl = resolveCodexActivityBridgeUrl(configuredUrl)
	} catch {
		return { status: 503, payload: unavailableCodexActivity(now()) }
	}

	const controller = new AbortController()
	let timeout: ReturnType<typeof setTimeout> | undefined
	const timeoutPromise = new Promise<never>((_, reject) => {
		timeout = setTimeout(() => {
			controller.abort()
			reject(new Error('timeout'))
		}, Math.max(1, timeoutMs))
	})

	try {
		const result = await Promise.race([
			(async () => {
				const candidates = [bridgeUrl]
				const primary = new URL(bridgeUrl)
				if (primary.hostname === 'localhost') {
					primary.hostname = '127.0.0.1'
					candidates.push(primary.toString())
				}

				let response: Response | undefined
				for (let index = 0; index < candidates.length; index++) {
					const candidate = candidates[index]
					try {
						response = await fetchImpl(candidate, {
							headers: { Accept: 'application/json' },
							signal: controller.signal,
						})
						if (response.status === 200 || index === candidates.length - 1) break
						response = undefined
					} catch {
						// Some Windows runtimes resolve localhost only to ::1 while the
					// privacy boundary intentionally listens on IPv4 loopback. Retry
					// only that loopback transport against the same IPv4 endpoint.
					}
				}
				if (!response) throw new Error('bridge_unavailable')
				if (response.status !== 200) throw new Error('upstream_status')
				let raw: unknown
				try {
					raw = await response.json()
				} catch {
					throw new Error('malformed_json')
				}
				const normalized = normalizeCodexActivity(raw)
				if (!normalized) throw new Error('invalid_contract')
				return normalized
			})(),
			timeoutPromise,
		])
		return { status: 200, payload: result }
	} catch {
		return { status: 503, payload: unavailableCodexActivity(now()) }
	} finally {
		if (timeout !== undefined) clearTimeout(timeout)
	}
}
