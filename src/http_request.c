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
	struct HttpRequest *pReq;
	char szLineBuf[URI_MAX + 32];

	// initialize request structure
	pReq = (struct HttpRequest*)malloc(sizeof(struct HttpRequest));
	if(pReq == NULL) return NULL;

	// initialize response structure
	memset((void *)pReq, 0, sizeof(struct HttpRequest));

	// set initial values
	pReq->nSockFd = nSockFd;
	pReq->nTimeout = nTimeout;

	pReq->nReqStatus = 0;
	pReq->nContentsLength = -1;

	pReq->pHeaders = qEntry();
	if(pReq->pHeaders == NULL) return pReq;

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
			pReq->nReqStatus = -1;
			return pReq;
		} else if(nStreamStatus < 0) { // connection closed
			pReq->nReqStatus = -2;
			return pReq;
		}

		// parse line
		pszReqMethod = strtok(szLineBuf, " ");
		pszReqUri = strtok(NULL, " ");
		pszHttpVer = strtok(NULL, " ");
		pszTmp = strtok(NULL, " ");

		if(pszReqMethod == NULL || pszReqUri == NULL || pszHttpVer == NULL || pszTmp != NULL) {
			DEBUG("Invalid request line.");
			return pReq;
		}

		//DEBUG("pszReqMethod %s", pszReqMethod);
		//DEBUG("pszReqUri %s", pszReqUri);
		//DEBUG("pszHttpVer %s", pszHttpVer);

		//
		// request method
		//
		qStrUpper(pszReqMethod);
		pReq->pszRequestMethod = strdup(pszReqMethod);

		//
		// http version
		//
		qStrUpper(pszHttpVer);
		if(strcmp(pszHttpVer, HTTP_PROTOCOL_09)
			&& strcmp(pszHttpVer, HTTP_PROTOCOL_10)
			&& strcmp(pszHttpVer, HTTP_PROTOCOL_11)
		) {
			DEBUG("Unknown protocol: %s", pszHttpVer);
			return pReq;
		}
		pReq->pszHttpVersion = strdup(pszHttpVer);

		//
		// request uri
		//

		// if request has only path
		if(pszReqUri[0] == '/') {
			pReq->pszRequestUri = strdup(pszReqUri);
		// if request has full uri format
		} else if(!strncasecmp(pszReqUri, "HTTP://", 7)) {
			// divide uri into host and path
			pszTmp = strstr(pszReqUri + 8, "/");
			if(pszTmp == NULL) {	// No path, ex) http://a.b.c:80
				httpHeaderSetStr(pReq->pHeaders, "Host", pszReqUri+8);
				pReq->pszRequestUri = strdup("/");
			} else {		// Has path, ex) http://a.b.c:80/100
				*pszTmp = '\0';
				httpHeaderSetStr(pReq->pHeaders, "Host", pszReqUri+8);
				*pszTmp = '/';
				pReq->pszRequestUri = strdup(pszTmp);
			}
		// invalid format
		} else {
			DEBUG("Unable to parse uri: %s", pszReqUri);
			return pReq;
		}

		// request path
		pReq->pszRequestPath = strdup(pReq->pszRequestUri);

		// remove query string from request path
		pszTmp = strstr(pReq->pszRequestPath, "?");
		if(pszTmp != NULL) {
			*pszTmp ='\0';
			pReq->pszQueryString = strdup(pszTmp + 1);
		} else {
			pReq->pszQueryString = strdup("");
		}

		// decode path
		qDecodeUrl(pReq->pszRequestPath);

		// check path
		if(isValidPathname(pReq->pszRequestPath) == false) {
			DEBUG("Invalid URI format : %s", pReq->pszRequestUri);
			return pReq;
		}
		correctPathname(pReq->pszRequestPath);
	}

	// Parse parameter headers : "key: value"
	while(true) {
		// read line
		if(streamGets(szLineBuf, sizeof(szLineBuf), nSockFd, nTimeout*1000) <= 0) return pReq;
		if(strlen(szLineBuf) == 0) break; // detect line-feed

		// separate :
		char *tmp = strstr(szLineBuf, ":");
		if(tmp == NULL) {
			DEBUG("Request header field is missing ':' separator.");
			return pReq;
		}

		*tmp = '\0';
		char *name = qStrUpper(qStrTrim(szLineBuf)); // to capital letters
		char *value = qStrTrim(tmp + 1);

		// put
		httpHeaderSetStr(pReq->pHeaders, name, value);
	}

	// parse host
	pReq->pszRequestHost = _getCorrectedHostname(httpHeaderGetStr(pReq->pHeaders, "HOST"));
	if(pReq->pszRequestHost == NULL) {
		DEBUG("Can't find host information.");
		return pReq;
	}
	httpHeaderSetStr(pReq->pHeaders, "Host", pReq->pszRequestHost);

	// Parse Contents
	if(httpHeaderGetStr(pReq->pHeaders, "CONTENT-LENGTH") != NULL) {
		pReq->nContentsLength = (off_t)atoll(httpHeaderGetStr(pReq->pHeaders, "CONTENT-LENGTH"));

		// do not load into memory in case of PUT and POST method
		if(strcmp(pReq->pszRequestMethod, "PUT")
		&& strcmp(pReq->pszRequestMethod, "POST")
		&& pReq->nContentsLength <= MAX_HTTP_MEMORY_CONTENTS) {
			if(pReq->nContentsLength == 0) {
				pReq->pContents = strdup("");
			} else {
				// allocate memory
				pReq->pContents = (char *)malloc(pReq->nContentsLength + 1);
				if(pReq->pContents == NULL) {
					LOG_WARN("Memory allocation failed.");
					return pReq;
				}

				// save into memory
				int nReaded = streamGetb(pReq->pContents, nSockFd, pReq->nContentsLength, nTimeout*1000);
				if(nReaded >= 0) pReq->pContents[nReaded] = '\0';
				DEBUG("%s", pReq->pContents);

				if(pReq->nContentsLength != nReaded) {
					DEBUG("Connection is closed before request completion.");
					free(pReq->pContents);
					pReq->nContentsLength = -1;
					return pReq;
				}
			}
		}
	}

	// set document root
	pReq->pszDocumentRoot = strdup(g_conf.szDocumentRoot);
	if(IS_EMPTY_STRING(g_conf.szDirectoryIndex) == false) {
		pReq->pszDirectoryIndex = strdup(g_conf.szDirectoryIndex);
	}

	// change flag to normal state
	pReq->nReqStatus = 1;

	return pReq;
}

char *httpRequestGetSysPath(struct HttpRequest *pReq, char *pszBuf, size_t nBufSize, const char *pszPath) {
	if(pReq == NULL || pReq->nReqStatus != 1) return NULL;

	// generate abs path
	snprintf(pszBuf, nBufSize, "%s%s", pReq->pszDocumentRoot, pszPath);
	pszBuf[nBufSize - 1] = '\0';

	return pszBuf;
}

bool httpRequestFree(struct HttpRequest *pReq) {
	if(pReq == NULL) return false;

	if(pReq->pszDocumentRoot != NULL) free(pReq->pszDocumentRoot);
	if(pReq->pszDirectoryIndex != NULL) free(pReq->pszDirectoryIndex);

	if(pReq->pszRequestMethod != NULL) free(pReq->pszRequestMethod);
	if(pReq->pszRequestUri != NULL) free(pReq->pszRequestUri);
	if(pReq->pszHttpVersion != NULL) free(pReq->pszHttpVersion);

	if(pReq->pszRequestHost != NULL) free(pReq->pszRequestHost);
	if(pReq->pszRequestPath != NULL) free(pReq->pszRequestPath);
	if(pReq->pszQueryString != NULL) free(pReq->pszQueryString);

	if(pReq->pHeaders != NULL) pReq->pHeaders->free(pReq->pHeaders);
	if(pReq->pContents != NULL) free(pReq->pContents);

	free(pReq);

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
