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

static char *_getCorrectedHostname(const char *pszRequestHost);

/*
 * @return	HttpRequest pointer
 *		NULL : system error
 */
struct HttpRequest *httpRequestParse(int nSockFd, int nTimeout) {
	struct HttpRequest *req;
	char szLineBuf[URI_MAX + 32];

	// initialize request structure
	req = (struct HttpRequest*)malloc(sizeof(struct HttpRequest));
	if(req == NULL) return NULL;

	// initialize response structure
	memset((void *)req, 0, sizeof(struct HttpRequest));

	// set initial values
	req->nSockFd = nSockFd;
	req->nTimeout = nTimeout;

	req->nReqStatus = 0;
	req->nContentsLength = -1;

	req->pHeaders = qEntry();
	if(req->pHeaders == NULL) return req;

	//
	// Parse HTTP header
	//

	// Parse request line : "method uri protocol"
	{
		int nStreamStatus;
		char *pszReqMethod, *pszReqUri, *pszHttpVer, *pszTmp;

		// read line
		nStreamStatus = streamGets(szLineBuf, sizeof(szLineBuf), nSockFd, nTimeout*1000);
		if(nStreamStatus == 0) { // timeout
			req->nReqStatus = -1;
			return req;
		} else if(nStreamStatus < 0) { // connection closed
			req->nReqStatus = -2;
			return req;
		}

		// parse line
		pszReqMethod = strtok(szLineBuf, " ");
		pszReqUri = strtok(NULL, " ");
		pszHttpVer = strtok(NULL, " ");
		pszTmp = strtok(NULL, " ");

		if(pszReqMethod == NULL || pszReqUri == NULL || pszHttpVer == NULL || pszTmp != NULL) {
			DEBUG("Invalid request line.");
			return req;
		}

		//DEBUG("pszReqMethod %s", pszReqMethod);
		//DEBUG("pszReqUri %s", pszReqUri);
		//DEBUG("pszHttpVer %s", pszHttpVer);

		//
		// request method
		//
		qStrUpper(pszReqMethod);
		req->pszRequestMethod = strdup(pszReqMethod);

		//
		// http version
		//
		qStrUpper(pszHttpVer);
		if(strcmp(pszHttpVer, HTTP_PROTOCOL_09)
			&& strcmp(pszHttpVer, HTTP_PROTOCOL_10)
			&& strcmp(pszHttpVer, HTTP_PROTOCOL_11)
		) {
			DEBUG("Unknown protocol: %s", pszHttpVer);
			return req;
		}
		req->pszHttpVersion = strdup(pszHttpVer);

		//
		// request uri
		//

		// if request has only path
		if(pszReqUri[0] == '/') {
			req->pszRequestUri = strdup(pszReqUri);
		// if request has full uri format
		} else if(!strncasecmp(pszReqUri, "HTTP://", 7)) {
			// divide uri into host and path
			pszTmp = strstr(pszReqUri + 8, "/");
			if(pszTmp == NULL) {	// No path, ex) http://a.b.c:80
				httpHeaderSetStr(req->pHeaders, "Host", pszReqUri+8);
				req->pszRequestUri = strdup("/");
			} else {		// Has path, ex) http://a.b.c:80/100
				*pszTmp = '\0';
				httpHeaderSetStr(req->pHeaders, "Host", pszReqUri+8);
				*pszTmp = '/';
				req->pszRequestUri = strdup(pszTmp);
			}
		// invalid format
		} else {
			DEBUG("Unable to parse uri: %s", pszReqUri);
			return req;
		}

		// request path
		req->pszRequestPath = strdup(req->pszRequestUri);

		// remove query string from request path
		pszTmp = strstr(req->pszRequestPath, "?");
		if(pszTmp != NULL) {
			*pszTmp ='\0';
			req->pszQueryString = strdup(pszTmp + 1);
		} else {
			req->pszQueryString = strdup("");
		}

		// decode path
		qDecodeUrl(req->pszRequestPath);

		// check path
		if(isValidPathname(req->pszRequestPath) == false) {
			DEBUG("Invalid URI format : %s", req->pszRequestUri);
			return req;
		}
		correctPathname(req->pszRequestPath);
	}

	// Parse parameter headers : "key: value"
	while(true) {
		// read line
		if(streamGets(szLineBuf, sizeof(szLineBuf), nSockFd, nTimeout*1000) <= 0) return req;
		if(strlen(szLineBuf) == 0) break; // detect line-feed

		// separate :
		char *tmp = strstr(szLineBuf, ":");
		if(tmp == NULL) {
			DEBUG("Request header field is missing ':' separator.");
			return req;
		}

		*tmp = '\0';
		char *name = qStrUpper(qStrTrim(szLineBuf)); // to capital letters
		char *value = qStrTrim(tmp + 1);

		// put
		httpHeaderSetStr(req->pHeaders, name, value);
	}

	// parse host
	req->pszRequestHost = _getCorrectedHostname(httpHeaderGetStr(req->pHeaders, "HOST"));
	if(req->pszRequestHost == NULL) {
		DEBUG("Can't find host information.");
		return req;
	}
	httpHeaderSetStr(req->pHeaders, "Host", req->pszRequestHost);

	// Parse Contents
	if(httpHeaderGetStr(req->pHeaders, "CONTENT-LENGTH") != NULL) {
		req->nContentsLength = (off_t)atoll(httpHeaderGetStr(req->pHeaders, "CONTENT-LENGTH"));

		// do not load into memory in case of PUT and POST method
		if(strcmp(req->pszRequestMethod, "PUT")
		&& strcmp(req->pszRequestMethod, "POST")
		&& req->nContentsLength <= MAX_HTTP_MEMORY_CONTENTS) {
			if(req->nContentsLength == 0) {
				req->pContents = strdup("");
			} else {
				// allocate memory
				req->pContents = (char *)malloc(req->nContentsLength + 1);
				if(req->pContents == NULL) {
					LOG_WARN("Memory allocation failed.");
					return req;
				}

				// save into memory
				int nReaded = streamGetb(req->pContents, nSockFd, req->nContentsLength, nTimeout*1000);
				if(nReaded >= 0) req->pContents[nReaded] = '\0';
				DEBUG("[Contents] %s", req->pContents);

				if(req->nContentsLength != nReaded) {
					DEBUG("Connection is closed before request completion.");
					free(req->pContents);
					req->nContentsLength = -1;
					return req;
				}
			}
		}
	}

	// set document root
	req->pszDocRoot = strdup(g_conf.szDataDir);

	// change flag to normal state
	req->nReqStatus = 1;

	return req;
}

bool httpRequestFree(struct HttpRequest *req) {
	if(req == NULL) return false;

	if(req->pszDocRoot != NULL) free(req->pszDocRoot);

	if(req->pszRequestMethod != NULL) free(req->pszRequestMethod);
	if(req->pszRequestUri != NULL) free(req->pszRequestUri);
	if(req->pszHttpVersion != NULL) free(req->pszHttpVersion);

	if(req->pszRequestHost != NULL) free(req->pszRequestHost);
	if(req->pszRequestPath != NULL) free(req->pszRequestPath);
	if(req->pszQueryString != NULL) free(req->pszQueryString);

	if(req->pHeaders != NULL) req->pHeaders->free(req->pHeaders);
	if(req->pContents != NULL) free(req->pContents);

	free(req);

	return true;
}

static char *_getCorrectedHostname(const char *pszRequestHost) {
	char *pszHost = NULL;

	if(pszRequestHost != NULL) {
		pszHost = strdup(pszRequestHost);
		qStrLower(pszHost);

		// if port number is 80, take it off
		char *pszTmp = strstr(pszHost, ":");
		if(pszTmp != NULL && !strcmp(pszTmp, ":80")) *pszTmp = '\0';
	}

	return pszHost;
}
