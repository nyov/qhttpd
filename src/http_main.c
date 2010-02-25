/*
 * Copyright 2008 The qDecoder Project. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE QDECODER PROJECT ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE QDECODER PROJECT BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "qhttpd.h"

/////////////////////////////////////////////////////////////////////////
// PUBLIC FUNCTIONS
/////////////////////////////////////////////////////////////////////////

int httpMain(int nSockFd) {
	bool bKeepAlive = false;

	do {
		/////////////////////////////////////////////////////////
		// Pre-processing Block
		/////////////////////////////////////////////////////////

		// reset keep-alive
		bKeepAlive = false;

		/////////////////////////////////////////////////////////
		// Request processing Block
		/////////////////////////////////////////////////////////

		// parse request
		struct HttpRequest *pReq = httpRequestParse(nSockFd, g_conf.nConnectionTimeout);
		if(pReq == NULL) {
			LOG_ERR("Can't parse request.");
			break;
		}

		// create response
		struct HttpResponse *pRes = httpResponseCreate();
		if(pRes == NULL) {
			LOG_ERR("Can't create response.");
			httpRequestFree(pReq);
			break;
		}

		if(pReq->nReqStatus >= 0) { // normal request
			// set request information
			poolSetConnRequest(pReq);

			// call method handler
			// 1. parse request.
			// 2.   call luaRequestHandler(), if LUA is enabled
			// 3.   call hookRequestHandler(), if HOOK is enabled.
			// 4.   call default request handler
			// 5.   call hookResponseHandler(), if HOOK is enabled.
			// 6.   call luaResponseHandler(), if LUA is enabled
			// 7. response out
			if(pReq->nReqStatus > 0) {
				int nResCode = 0;

				// check if the request is for server status page
				nResCode = httpSpecialRequestHandler(pReq, pRes);

#ifdef ENABLE_LUA
				if(nResCode == 0 && g_conf.bEnableLua == true) { // if response does not set
					nResCode = luaRequestHandler(pReq, pRes);
				}
#endif
#ifdef ENABLE_HOOK
				if(nResCode == 0) { // if response does not set
					nResCode = hookRequestHandler(pReq, pRes);
				}
#endif
				if(nResCode == 0) { // if nothing done, call native handler
					nResCode =  httpRequestHandler(pReq, pRes);
				}

				if(nResCode == 0) { // never reach here
					nResCode = response500(pReq, pRes);
					LOG_ERR("An error occured while processing method.");
				}
			} else { // bad request
				httpResponseSetSimple(pReq, pRes, HTTP_CODE_BAD_REQUEST, false, "Your browser sent a request that this server could not understand.");
			}

			// hook response
#ifdef ENABLE_HOOK
			if(hookResponseHandler(pReq, pRes) == false) {
				LOG_WARN("An error occured while processing hookResponseHandler().");
			}
#endif

#ifdef ENABLE_LUA
			if(g_conf.bEnableLua == true
			&& luaResponseHandler(pReq, pRes) == false) {
				LOG_WARN("An error occured while processing luaResponseHandler().");
			}
#endif
			 // set response information
			poolSetConnResponse(pRes);

			// check exit request
			if(poolGetExitRequest() == true
			|| (g_conf.nMaxKeepAliveRequests > 0 && poolGetChildKeepaliveRequests() > g_conf.nMaxKeepAliveRequests))
			{
				httpHeaderSetStr(pRes->pHeaders, "Connection", "close");
			}

			// serialize & stream out
			httpResponseOut(pRes, nSockFd);

			// logging
			httpAccessLog(pReq, pRes);

			// check keep-alive
			if(httpHeaderHasStr(pRes->pHeaders, "CONNECTION", "KEEP-ALIVE") == true) bKeepAlive = true;
		} else { // timeout or connection closed
			DEBUG("Connection timeout.");
		}

		/////////////////////////////////////////////////////////
		// Post-processing Block
		/////////////////////////////////////////////////////////

		// free resources
		if(pReq != NULL) httpRequestFree(pReq);
		if(pRes != NULL) httpResponseFree(pRes);
	} while(bKeepAlive == true);

	return 0;
}

/*
 * @return	response code
 */
int httpRequestHandler(struct HttpRequest *pReq, struct HttpResponse *pRes) {
	if(pReq == NULL || pRes == NULL) return 0;

	int nResCode = 0;

	// HTTP methods : OPTIONS,HEAD,GET,PUT
	if(!strcmp(pReq->pszRequestMethod, "OPTIONS")) {
		nResCode = httpMethodOptions(pReq, pRes);
	} else if(!strcmp(pReq->pszRequestMethod, "HEAD")) {
		nResCode = httpMethodHead(pReq, pRes);
	} else if(!strcmp(pReq->pszRequestMethod, "GET")) {
		nResCode = httpMethodGet(pReq, pRes);
	} else if(!strcmp(pReq->pszRequestMethod, "PUT")) {
		nResCode = httpMethodPut(pReq, pRes);
	}
	// WebDAV methods : PROPFIND,PROPPATCH,MKCOL,MOVE,DELETE,LOCK,UNLOCK
	else if(!strcmp(pReq->pszRequestMethod, "PROPFIND")) {
		nResCode = httpMethodPropfind(pReq, pRes);
	} else if(!strcmp(pReq->pszRequestMethod, "PROPPATCH")) {
		nResCode = httpMethodProppatch(pReq, pRes);
	} else if(!strcmp(pReq->pszRequestMethod, "MKCOL")) {
		nResCode = httpMethodMkcol(pReq, pRes);
	} else if(!strcmp(pReq->pszRequestMethod, "MOVE")) {
		nResCode = httpMethodMove(pReq, pRes);
	} else if(!strcmp(pReq->pszRequestMethod, "DELETE")) {
		nResCode = httpMethodDelete(pReq, pRes);
	} else if(!strcmp(pReq->pszRequestMethod, "LOCK")) {
		nResCode = httpMethodLock(pReq, pRes);
	} else if(!strcmp(pReq->pszRequestMethod, "UNLOCK")) {
		nResCode = httpMethodUnlock(pReq, pRes);
	}
	// unknown methods
	else {
		nResCode = httpMethodNotImplemented(pReq, pRes);
	}

	return nResCode;
}

int httpSpecialRequestHandler(struct HttpRequest *pReq, struct HttpResponse *pRes) {
	if(pReq == NULL || pRes == NULL) return 0;

	// check if the request is for server status page
	if(g_conf.bStatusEnable == true
	&& !strcmp(pReq->pszRequestMethod, "GET")
	&& !strcmp(pReq->pszRequestPath, g_conf.szStatusUrl)) {
		Q_OBSTACK *obHtml = httpGetStatusHtml();
		if(obHtml == NULL) return response500(pReq, pRes);

		// get size
		size_t nHtmlSize = obHtml->getSize(obHtml);

		// set response
		httpResponseSetCode(pRes, HTTP_CODE_OK, pReq, true);
		httpResponseSetContent(pRes, "text/html; charset=\"utf-8\"", NULL, nHtmlSize);

		// print out header
		httpResponseOut(pRes, pReq->nSockFd);

		// print out contents
		streamStackOut(pReq->nSockFd, obHtml);

		// free
		obHtml->free(obHtml);

		return HTTP_CODE_OK;
	}

	return 0;
}
