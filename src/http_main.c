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
		struct HttpRequest *req = httpRequestParse(nSockFd, g_conf.nConnectionTimeout);
		if(req == NULL) {
			LOG_ERR("Can't parse request.");
			break;
		}

		// create response
		struct HttpResponse *res = httpResponseCreate();
		if(res == NULL) {
			LOG_ERR("Can't create response.");
			httpRequestFree(req);
			break;
		}

		if(req->nReqStatus >= 0) { // normal request
			// set request information
			poolSetConnRequest(req);

			// call method handler
			// 1. parse request.
			// 2.    lua> luaRequestHandler();
			// 3.   hook> hookRequestHandler();
			// 4. native request handler
			// 5.   hook> hookResponseHandler()
			// 6.    lua> luaResponseHandler()
			// 7. response out
			if(req->nReqStatus > 0) {
				int nResCode = 0;

				// check if the request is for server status page
				nResCode = httpSpecialRequestHandler(req, res);

#ifdef ENABLE_LUA
				if(nResCode == 0 && g_conf.bEnableLua == true) { // if response does not set
					nResCode = luaRequestHandler(req, res);
				}
#endif
#ifdef ENABLE_HOOK
				if(nResCode == 0) { // if response does not set
					nResCode = hookRequestHandler(req, res);
				}
#endif
				if(nResCode == 0) { // if nothing done, call native handler
					nResCode =  httpRequestHandler(req, res);
				}

				if(nResCode == 0) {
					LOG_ERR("An error occured while processing method.");
				}
			} else { // bad request
				httpResponseSetCode(res, HTTP_CODE_BAD_REQUEST, req, false);
				httpResponseSetContentHtml(res, "Your browser sent a request that this server could not understand.");
			}

			// hook response
#ifdef ENABLE_HOOK
			if(hookResponseHandler(req, res) == false) {
				LOG_WARN("An error occured while processing hookResponseHandler().");
			}
#endif

#ifdef ENABLE_LUA
			if(g_conf.bEnableLua == true && luaResponseHandler(req,res) == false) {
				LOG_WARN("An error occured while processing luaResponseHandler().");
			}
#endif

			 // set response information
			poolSetConnResponse(res);

			// check exit request
			if(poolGetExitRequest() == true) {
				httpHeaderSetStr(res->pHeaders, "Connection", "close");
			}

			// serialize & stream out
			httpResponseOut(res, nSockFd);

			// logging
			httpAccessLog(req, res);

			// check keep-alive
			if(httpHeaderHasStr(res->pHeaders, "CONNECTION", "KEEP-ALIVE") == true) bKeepAlive = true;
		} else { // timeout or connection closed
			DEBUG("Connection timeout.");
		}

		/////////////////////////////////////////////////////////
		// Post-processing Block
		/////////////////////////////////////////////////////////

		// free resources
		if(req != NULL) httpRequestFree(req);
		if(res != NULL) httpResponseFree(res);
	} while(bKeepAlive == true);

	return 0;
}

/*
 * @return	response code
 *		0 : system error
 */
int httpRequestHandler(struct HttpRequest *req, struct HttpResponse *res) {
	if(req == NULL || res == NULL) return 0;

	// native method handlers
	int nResCode = 0;
	if(!strcmp(req->pszRequestMethod, "OPTIONS")) {
		nResCode = httpMethodOptions(req, res);
	} else if(!strcmp(req->pszRequestMethod, "HEAD")) {
		nResCode = httpMethodHead(req, res);
	} else if(!strcmp(req->pszRequestMethod, "GET")) {
		nResCode = httpMethodGet(req, res);
	} else if(!strcmp(req->pszRequestMethod, "PUT")) {
		nResCode = httpMethodPut(req, res);
	} else if(!strcmp(req->pszRequestMethod, "DELETE")) {
		nResCode = httpMethodDelete(req, res);
	} else {
		nResCode = httpMethodNotImplemented(req, res);
	}

	return nResCode;
}

int httpSpecialRequestHandler(struct HttpRequest *req, struct HttpResponse *res) {
	if(req == NULL || res == NULL) return 0;

	// check if the request is for server status page
	if(g_conf.bStatusEnable == true && !strcmp(req->pszRequestMethod, "GET") && !strcmp(req->pszRequestPath, g_conf.szStatusUrl)) {
		Q_OBSTACK *obHtml = httpGetStatusHtml();
		if(obHtml == NULL) return response500(req, res);

		httpResponseSetCode(res, HTTP_CODE_OK, req, true);
		size_t nHtmlSize = 0;
		char *pszHtml = obHtml->getFinal(obHtml, &nHtmlSize);

		httpResponseSetContent(res, "text/html; charset=\"utf-8\"", pszHtml, nHtmlSize);

		obHtml->free(obHtml);

		return HTTP_CODE_OK;
	}

	return 0;
}
