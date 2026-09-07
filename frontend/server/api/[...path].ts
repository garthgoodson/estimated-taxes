import { createError, getRequestURL, proxyRequest } from 'h3'
import { validatedBackendOrigin } from '../utils/backendOrigin'

export default defineEventHandler(event => {
  const config = useRuntimeConfig(event)
  let backendOrigin: string
  try {
    backendOrigin = validatedBackendOrigin(config.backendOrigin)
  } catch (error) {
    throw createError({ statusCode: 500, statusMessage: error instanceof Error ? error.message : 'Invalid backend origin.' })
  }

  const requestUrl = getRequestURL(event)
  return proxyRequest(event, `${backendOrigin}${requestUrl.pathname}${requestUrl.search}`, { streamRequest: true })
})
