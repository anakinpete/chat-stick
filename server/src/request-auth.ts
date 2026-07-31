interface DeviceAuthEnv {
	DEVICE_AUTH_TOKEN?: string
}

export function isAuthorizedDeviceRequest(request: Request, env: DeviceAuthEnv): boolean {
	const configuredToken = env.DEVICE_AUTH_TOKEN?.trim()
	if (!configuredToken) return true

	const providedToken = getRequestToken(request, {
		headerNames: ['X-Device-Token'],
		queryNames: ['device_token'],
	})

	return secureTokenEquals(providedToken, configuredToken)
}

export function isAuthenticatedDeviceRequest(request: Request, env: DeviceAuthEnv): boolean {
	return Boolean(env.DEVICE_AUTH_TOKEN?.trim()) && isAuthorizedDeviceRequest(request, env)
}

export function getRequestToken(
	request: Request,
	options: { headerNames: string[]; queryNames: string[] }
): string {
	for (const headerName of options.headerNames) {
		const value = request.headers.get(headerName)
		if (value) return value.trim()
	}

	const auth = request.headers.get('Authorization') || ''
	const bearer = auth.match(/^Bearer\s+(.+)$/i)?.[1]
	if (bearer) return bearer.trim()

	const url = new URL(request.url)
	for (const queryName of options.queryNames) {
		const value = url.searchParams.get(queryName)
		if (value) return value.trim()
	}

	return ''
}

export function secureTokenEquals(providedToken: string, configuredToken: string): boolean {
	if (!providedToken || providedToken.length !== configuredToken.length) return false

	let diff = 0
	for (let i = 0; i < configuredToken.length; i++) {
		diff |= configuredToken.charCodeAt(i) ^ providedToken.charCodeAt(i)
	}
	return diff === 0
}
