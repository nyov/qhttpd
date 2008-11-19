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

		struct HttpRequest *req = NULL;
		struct HttpResponse *res = NULL;

		// reset keep-alive
		bKeepAlive = false;

		/////////////////////////////////////////////////////////
		// Request processing Block
		/////////////////////////////////////////////////////////

		// parse request
		req = httpRequestParse(nSockFd, g_conf.nConnectionTimeout);
		if(req == NULL) {
			LOG_ERR("System Error #1.");
			break;
		}

		if(req->nReqStatus >= 0) {
			poolSetConnRequest(req); // set request information

			// handle request
			res = httpHandler(req);
			if(res == NULL) {
				LOG_ERR("System Error #2.");
				break;
			}
			poolSetConnResponse(res); // set response information

			// serialize & stream out
			httpResponseOut(res, nSockFd);

			// logging
			httpAccessLog(req, res);

			// check keep-alive
			if(httpHeaderHasString(res->pHeaders, "CONNECTION", "KEEP-ALIVE") == true) bKeepAlive = true;
		}

		/////////////////////////////////////////////////////////
		// Post-processing Block
		/////////////////////////////////////////////////////////

		// free resources
		if(req != NULL) httpRequestFree(req);
		if(res != NULL) httpResponseFree(res);

		// check exit request
		//if(poolGetExitRequest() == true) bKeepAlive = false;
	} while(bKeepAlive == true);

	return 0;
}

/*
 * @return	HttpResponse pointer
 *		NULL : system error
 */
struct HttpResponse *httpHandler(struct HttpRequest *req) {
	if(req == NULL) return NULL;

	// create response
	struct HttpResponse *res = httpResponseCreate();
	if(res == NULL) return NULL;

	// filter bad request
	if(req->nReqStatus == 0) { // bad request
		DEBUG("Bad request.");

		// @todo: bad request answer
		httpResponseSetCode(res, HTTP_RESCODE_BAD_REQUEST, req, false);
		httpResponseSetContentHtml(res, "Your browser sent a request that this server could not understand.");
		return res;
	} else if(req->nReqStatus < 0) { // timeout or connection closed
		DEBUG("Connection timeout.");
		return res;
	}

	// handle method
	int nResCode = 0;

	// 특수 URI 체크
	if(g_conf.bStatusEnable == true
	&& !strcmp(req->pszRequestMethod, "GET")
	&& !strcmp(req->pszRequestPath, g_conf.szStatusUrl)) {
		Q_OBSTACK *obHtml = httpGetStatusHtml();
		if(obHtml == NULL) {
			nResCode = response500(req, res);
			return res;
		}

		httpResponseSetCode(res, HTTP_RESCODE_OK, req, true);
		httpResponseSetContent(res, "text/html; charset=\"utf-8\"", qObstackGetSize(obHtml), (char *)qObstackFinish(obHtml));
		qObstackFree(obHtml);

		return res;
	}

	// method hooking
	nResCode = hookMethodHandler(req, res);
	if(nResCode != 0) return res;

	// native methods
	if(!strcmp(req->pszRequestMethod, "OPTIONS")) {
		nResCode = httpMethodOptions(req, res);
	} else if(!strcmp(req->pszRequestMethod, "GET")) {
		nResCode = httpMethodGet(req, res);
	} else if(!strcmp(req->pszRequestMethod, "HEAD")) {
		nResCode = httpMethodHead(req, res);
	} else {
		nResCode = httpMethodNotImplemented(req, res);
	}

	return res;
}
