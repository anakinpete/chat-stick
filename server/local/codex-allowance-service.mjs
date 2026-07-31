import { readCodexAccountAndRateLimits } from './codex-app-server-client.mjs'

const SOURCE = 'codex-app-server'
const DEFAULT_CACHE_MS = 60_000
const DEFAULT_RETRY_MS = 15_000

function clampPercent(value) {
	if (!Number.isFinite(value)) return null
	return Math.min(100, Math.max(0, Math.round(value)))
}

function isoFromUnixSeconds(value) {
	if (!Number.isInteger(value) || value < 0) return null
	const date = new Date(value * 1000)
	return Number.isNaN(date.getTime()) ? null : date.toISOString()
}

function windowLabel(durationMinutes, kind) {
	if (durationMinutes === 300) return '5h'
	if (durationMinutes === 10_080) return 'weekly'
	if (Number.isInteger(durationMinutes) && durationMinutes > 0) {
		if (durationMinutes % 1440 === 0) return `${durationMinutes / 1440}d`
		if (durationMinutes % 60 === 0) return `${durationMinutes / 60}h`
		return `${durationMinutes}m`
	}
	return kind
}

function normalizeWindow(snapshot, window, kind) {
	if (!window || typeof window !== 'object') return null
	const usedPercent = clampPercent(window.usedPercent)
	if (usedPercent == null) return null
	const windowMinutes = Number.isInteger(window.windowDurationMins)
		? window.windowDurationMins
		: null

	return {
		limitId: typeof snapshot.limitId === 'string' ? snapshot.limitId : null,
		limitLabel: typeof snapshot.limitName === 'string' ? snapshot.limitName : null,
		kind,
		label: windowLabel(windowMinutes, kind),
		windowMinutes,
		usedPercent,
		remainingPercent: 100 - usedPercent,
		resetsAt: isoFromUnixSeconds(window.resetsAt),
	}
}

function normalizeCredits(snapshot) {
	const credits = snapshot?.credits
	if (!credits || typeof credits !== 'object') {
		return { available: false, hasCredits: null, unlimited: null, balance: null }
	}

	const numericBalance =
		typeof credits.balance === 'number'
			? credits.balance
			: typeof credits.balance === 'string' && /^\d+(?:\.\d+)?$/.test(credits.balance)
				? Number(credits.balance)
				: null

	return {
		available: true,
		hasCredits: typeof credits.hasCredits === 'boolean' ? credits.hasCredits : null,
		unlimited: typeof credits.unlimited === 'boolean' ? credits.unlimited : null,
		balance: Number.isFinite(numericBalance) ? numericBalance : null,
	}
}

function snapshotsFromResponse(rateLimits) {
	if (rateLimits?.rateLimitsByLimitId && typeof rateLimits.rateLimitsByLimitId === 'object') {
		return Object.values(rateLimits.rateLimitsByLimitId).filter(Boolean)
	}
	return rateLimits?.rateLimits ? [rateLimits.rateLimits] : []
}

export function normalizeCodexAllowance(raw, now = new Date()) {
	if (!raw || typeof raw !== 'object' || !raw.account || !raw.rateLimits) {
		throw Object.assign(new Error('Malformed Codex app-server response'), {
			code: 'malformed_response',
		})
	}

	if (raw.account.requiresOpenaiAuth && !raw.account.account) {
		throw Object.assign(new Error('Codex is not authenticated'), { code: 'unauthenticated' })
	}

	const snapshots = snapshotsFromResponse(raw.rateLimits)
	if (snapshots.length === 0) {
		throw Object.assign(new Error('Codex returned no rate-limit buckets'), {
			code: 'malformed_response',
		})
	}

	const windows = []
	for (const snapshot of snapshots) {
		const primary = normalizeWindow(snapshot, snapshot.primary, 'primary')
		const secondary = normalizeWindow(snapshot, snapshot.secondary, 'secondary')
		if (primary) windows.push(primary)
		if (secondary) windows.push(secondary)
	}

	const creditSnapshot = snapshots.find((snapshot) => snapshot.credits) ?? snapshots[0]
	const resetCredits = raw.rateLimits.rateLimitResetCredits

	return {
		available: true,
		state: 'available',
		source: SOURCE,
		windows,
		credits: normalizeCredits(creditSnapshot),
		rateLimitResetCredits: {
			available:
				resetCredits != null && Number.isInteger(resetCredits.availableCount),
			availableCount: Number.isInteger(resetCredits?.availableCount)
				? Math.max(0, resetCredits.availableCount)
				: null,
		},
		updatedAt: now.toISOString(),
		stale: false,
	}
}

function unavailablePayload(code, now) {
	return {
		available: false,
		state: code === 'unauthenticated' ? 'unauthenticated' : 'unavailable',
		source: SOURCE,
		windows: [],
		credits: { available: false, hasCredits: null, unlimited: null, balance: null },
		rateLimitResetCredits: { available: false, availableCount: null },
		updatedAt: now.toISOString(),
		stale: false,
		error: { code },
	}
}

export function createCodexAllowanceService(options = {}) {
	const read = options.read ?? readCodexAccountAndRateLimits
	const cacheMs = options.cacheMs ?? DEFAULT_CACHE_MS
	const retryMs = options.retryMs ?? DEFAULT_RETRY_MS
	const now = options.now ?? (() => new Date())
	let cached = null
	let latestResponse = null
	let nextRefreshAtMs = 0
	let inFlight = null

	async function refresh() {
		const refreshTime = now()
		try {
			const raw = await read()
			const normalized = normalizeCodexAllowance(raw, refreshTime)
			cached = normalized
			latestResponse = normalized
			nextRefreshAtMs = refreshTime.getTime() + cacheMs
			return normalized
		} catch (error) {
			const code = typeof error?.code === 'string' ? error.code : 'provider_failure'
			if (cached) {
				latestResponse = {
					...cached,
					state: 'stale',
					stale: true,
					error: { code },
				}
				nextRefreshAtMs = refreshTime.getTime() + retryMs
				return latestResponse
			}
			latestResponse = unavailablePayload(code, refreshTime)
			nextRefreshAtMs = refreshTime.getTime() + retryMs
			return latestResponse
		}
	}

	return {
		async getAllowance() {
			const currentTime = now().getTime()
			if (latestResponse && currentTime < nextRefreshAtMs) return latestResponse
			if (!inFlight) {
				inFlight = refresh().finally(() => {
					inFlight = null
				})
			}
			return inFlight
		},
	}
}
