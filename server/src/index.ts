import { LiveSession } from './live-session'
import { indexDocs, vectorSearch } from './docs-search'
import { fetchCodexAllowanceBridge } from './codex-allowance-proxy'
import { fetchCodexActivityBridge } from './codex-activity-proxy'
import {
	getRequestToken,
	isAuthenticatedDeviceRequest,
	isAuthorizedDeviceRequest,
	secureTokenEquals,
} from './request-auth'

export { LiveSession }

export interface Env {
	LIVE_SESSION: DurableObjectNamespace
	GEMINI_API_KEY: string
	AI: Ai
	VECTORIZE: VectorizeIndex
	DB: D1Database
	HISTORY_API_TOKEN: string
	ADMIN_API_TOKEN?: string
	DEVICE_AUTH_TOKEN?: string
	CODEX_ALLOWANCE_BRIDGE_URL?: string
	CODEX_ACTIVITY_BRIDGE_URL?: string
	STORAGE?: R2Bucket
}

const LEGACY_FIRMWARE_PREFIX = 'chat-stick/firmware/'
const FIRMWARE_DEVICE_IDS = new Set(['m5-stick', 'waveshare'])

async function findLatestFirmware(
	env: Env,
	device: string
): Promise<{ version: number; key: string } | null> {
	if (!env.STORAGE) return null
	const prefixes =
		device === 'm5-stick'
			? [`chat-stick/firmware/${device}/`, LEGACY_FIRMWARE_PREFIX]
			: [`chat-stick/firmware/${device}/`]
	let latest: { version: number; key: string } | null = null
	for (const prefix of prefixes) {
		const list = await env.STORAGE.list({ prefix })
		for (const obj of list.objects) {
			const objectName = obj.key.slice(prefix.length)
			if (objectName.includes('/')) continue
			const match = objectName.match(/^firmware-v(\d+)\.bin$/)
			if (!match) continue
			const version = Number(match[1])
			if (!latest || version > latest.version) {
				latest = { version, key: obj.key }
			}
		}
	}
	return latest
}

export default {
	async fetch(request: Request, env: Env): Promise<Response> {
		const url = new URL(request.url)

		if (request.method === 'OPTIONS') {
			return new Response(null, { headers: corsHeaders() })
		}

		switch (url.pathname) {
			case '/ws': {
				const upgrade = request.headers.get('Upgrade')
				if (upgrade !== 'websocket') {
					return new Response('Expected WebSocket', { status: 426 })
				}
				if (!isAuthorizedDeviceRequest(request, env)) {
					return new Response('Unauthorized', { status: 401 })
				}

				// Route to DO by device_id (one session per device)
				const deviceId = url.searchParams.get('device_id') || 'unknown'
				const id = env.LIVE_SESSION.idFromName(deviceId)
				const stub = env.LIVE_SESSION.get(id)
				// Forward the full URL so DO can read device_id and chat_id
				return stub.fetch(request)
			}

			// Admin: index docs into Vectorize
			case '/admin/index':
				if (!isAuthorizedAdminRequest(request, env)) {
					return new Response('Unauthorized', { status: 401, headers: corsHeaders() })
				}
				return indexDocs(env)

			// Admin: test vector search
			case '/admin/search': {
				if (!isAuthorizedAdminRequest(request, env)) {
					return new Response('Unauthorized', { status: 401, headers: corsHeaders() })
				}
				const q = url.searchParams.get('q') || 'hello'
				return vectorSearch(q, env)
			}

			case '/health':
				return new Response('ok')

			case '/ping':
				if (!isAuthorizedDeviceRequest(request, env)) {
					return new Response('Unauthorized', { status: 401, headers: corsHeaders() })
				}
				return new Response('pong', {
					headers: {
						...corsHeaders(),
						'Cache-Control': 'no-store',
						'Content-Type': 'text/plain',
					},
				})

			case '/api/codex/allowance':
				if (!isAuthorizedDeviceRequest(request, env)) {
					return json({ error: { code: 'unauthorized' } }, { status: 401 })
				}
				if (request.method !== 'GET') {
					return json({ error: { code: 'method_not_allowed' } }, { status: 405 })
				}
				return proxyCodexAllowance(env)

			case '/api/codex/activity':
				if (!isAuthorizedDeviceRequest(request, env)) {
					return json({ error: { code: 'unauthorized' } }, { status: 401 })
				}
				if (request.method !== 'GET') {
					return json({ error: { code: 'method_not_allowed' } }, { status: 405 })
				}
				return proxyCodexActivity(env)

			case '/firmware/check': {
				if (!isAuthorizedDeviceRequest(request, env)) {
					return new Response('Unauthorized', { status: 401, headers: corsHeaders() })
				}
				const currentVersion = Number(url.searchParams.get('version') || '0')
				const device = resolveFirmwareDevice(url)
				const latest = await findLatestFirmware(env, device)
				const updateAvailable = !!latest && latest.version > currentVersion
				return json({
					available: updateAvailable,
					latest_version: latest?.version ?? currentVersion,
					notes: '',
					download_url: updateAvailable
						? `${url.origin}/firmware/download?device=${encodeURIComponent(device)}`
						: '',
				})
			}

			case '/firmware/download': {
				const device = resolveFirmwareDevice(url)
				const latest = await findLatestFirmware(env, device)
				if (!env.STORAGE || !latest) {
					return new Response('Firmware download unavailable', { status: 404 })
				}

				const object = await env.STORAGE.get(latest.key)
				if (!object) {
					return new Response('Firmware not found', { status: 404 })
				}

				const headers = new Headers()
				object.writeHttpMetadata(headers)
				headers.set('etag', object.httpEtag)
				headers.set('content-length', object.size.toString())
				headers.set('content-type', headers.get('content-type') || 'application/octet-stream')
				headers.set(
					'content-disposition',
					`attachment; filename="${latest.key.split('/').pop()}"`
				)
				return new Response(object.body, { headers })
			}

			default: {
				// /history/:deviceId — list recent conversations
				const historyMatch = url.pathname.match(/^\/history\/(.+)$/)
				if (historyMatch) {
					const deviceId = decodeURIComponent(historyMatch[1])
					const authorized =
						isAuthorizedHistoryRequest(request, env) ||
						isAuthenticatedDeviceRequest(request, env)
					if (!authorized) {
						return new Response('Unauthorized', { status: 401, headers: corsHeaders() })
					}

					const rows = await env.DB.prepare(
						`SELECT chat_id, last_message, updated_at
						 FROM conversations
						 WHERE device_id = ? AND last_message IS NOT NULL
						 ORDER BY updated_at DESC
						 LIMIT 10`
					)
						.bind(deviceId)
						.all()
					return new Response(JSON.stringify(rows.results), {
						headers: { 'Content-Type': 'application/json' },
					})
				}

				const sessionMatch = url.pathname.match(/^\/session\/(.+)$/)
				if (sessionMatch) {
					const chatId = decodeURIComponent(sessionMatch[1])
					const row = await env.DB.prepare(
						`SELECT chat_id, device_id, last_message, updated_at
						 FROM conversations
						 WHERE chat_id = ?
						 LIMIT 1`
					)
						.bind(chatId)
						.first<{
							chat_id: string
							device_id: string
							last_message: string | null
							updated_at: string
						}>()

					if (!row) {
						return new Response('Not found', { status: 404, headers: corsHeaders() })
					}

					const authorized =
						isAuthorizedHistoryRequest(request, env) ||
						isAuthenticatedDeviceRequest(request, env)
					if (!authorized) {
						return new Response('Unauthorized', { status: 401, headers: corsHeaders() })
					}

					return new Response(
						JSON.stringify({
							chat_id: row.chat_id,
							device_id: row.device_id,
							last_message: row.last_message,
							updated_at: row.updated_at,
						}),
						{
							headers: { ...corsHeaders(), 'Content-Type': 'application/json' },
						}
					)
				}
				return new Response('Not found', { status: 404 })
			}
		}
	},
}

async function proxyCodexAllowance(env: Env): Promise<Response> {
	const result = await fetchCodexAllowanceBridge(env.CODEX_ALLOWANCE_BRIDGE_URL)
	return json(result.payload, {
		status: result.status,
		headers: { 'Cache-Control': 'no-store' },
	})
}

async function proxyCodexActivity(env: Env): Promise<Response> {
	const result = await fetchCodexActivityBridge(env.CODEX_ACTIVITY_BRIDGE_URL)
	return json(result.payload, {
		status: result.status,
		headers: { 'Cache-Control': 'no-store' },
	})
}

function corsHeaders(): HeadersInit {
	return {
		'Access-Control-Allow-Origin': '*',
		'Access-Control-Allow-Methods': 'GET, POST, OPTIONS',
		'Access-Control-Allow-Headers': 'Authorization, Content-Type, X-Admin-Token, X-Device-Token, X-History-Token',
	}
}

function json(payload: unknown, init?: ResponseInit): Response {
	return new Response(JSON.stringify(payload), {
		...init,
		headers: {
			...corsHeaders(),
			'Content-Type': 'application/json',
			...(init?.headers || {}),
		},
	})
}

function resolveFirmwareDevice(url: URL): string {
	const requested = url.searchParams.get('device') || 'm5-stick'
	return FIRMWARE_DEVICE_IDS.has(requested) ? requested : 'm5-stick'
}

function isAuthorizedHistoryRequest(request: Request, env: Env): boolean {
	const configuredToken = env.HISTORY_API_TOKEN?.trim()
	if (!configuredToken) return false

	const providedToken = getRequestToken(request, {
		headerNames: ['X-History-Token'],
		queryNames: ['token'],
	})

	return secureTokenEquals(providedToken, configuredToken)
}

function isAuthorizedAdminRequest(request: Request, env: Env): boolean {
	const configuredToken = (env.ADMIN_API_TOKEN || env.HISTORY_API_TOKEN || '').trim()
	if (!configuredToken) return false

	const providedToken = getRequestToken(request, {
		headerNames: ['X-Admin-Token', 'X-History-Token'],
		queryNames: ['admin_token', 'token'],
	})

	return secureTokenEquals(providedToken, configuredToken)
}
