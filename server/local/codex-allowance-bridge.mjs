import { createServer } from 'node:http'
import {
	readCodexAccountAndRateLimits,
	resolveCodexExecutable,
} from './codex-app-server-client.mjs'
import { createCodexAllowanceService } from './codex-allowance-service.mjs'

const HOST = '127.0.0.1'
const port = Number.parseInt(process.env.CODEX_ALLOWANCE_BRIDGE_PORT || '8790', 10)

function logResolution(resolution) {
	console.log(
		`[codex-allowance] executable source=${resolution.source} type=${resolution.targetType} path=${resolution.path}`
	)
}

let resolution
let resolutionError
try {
	resolution = await resolveCodexExecutable()
	logResolution(resolution)
} catch (error) {
	resolutionError = error
	console.log(`[codex-allowance] executable source=unavailable type=none path=none code=${error.code}`)
}

const service = createCodexAllowanceService({
	read: resolution
		? () =>
				readCodexAccountAndRateLimits({
					resolution,
					onResolution: logResolution,
				})
		: async () => {
				throw resolutionError
			},
})

function sendJson(response, status, payload) {
	response.writeHead(status, {
		'Cache-Control': 'no-store',
		'Content-Type': 'application/json; charset=utf-8',
	})
	response.end(JSON.stringify(payload))
}

const server = createServer(async (request, response) => {
	const url = new URL(request.url || '/', `http://${HOST}:${port}`)
	if (request.method !== 'GET') {
		sendJson(response, 405, { error: { code: 'method_not_allowed' } })
		return
	}

	if (url.pathname === '/health') {
		sendJson(response, 200, { ok: true, source: 'codex-app-server' })
		return
	}

	if (url.pathname !== '/allowance') {
		sendJson(response, 404, { error: { code: 'not_found' } })
		return
	}

	const payload = await service.getAllowance()
	const status = payload.available ? 200 : payload.state === 'unauthenticated' ? 401 : 503
	console.log(
		`[codex-allowance] state=${payload.state} stale=${payload.stale} windows=${payload.windows.length}`
	)
	sendJson(response, status, payload)
})

server.listen(port, HOST, () => {
	console.log(`[codex-allowance] listening on http://${HOST}:${port}`)
})

function shutdown() {
	server.close(() => process.exit(0))
}

process.on('SIGINT', shutdown)
process.on('SIGTERM', shutdown)
