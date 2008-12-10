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
	char szLineBuf[LINEBUF_SIZE*4];

	// Request 구조체 생성
	req = (struct HttpRequest*)malloc(sizeof(struct HttpRequest));
	if(req == NULL) return NULL;

	// Request 구조체 초기화
	memset((void *)req, 0, sizeof(struct HttpRequest));
	// 0과 NULL 이외의 초기값이 들어가야 하는 부분 초기화

	req->nSockFd = nSockFd;
	req->nTimeout = nTimeout;

	req->nReqStatus = 0;
	req->nContentsLength = -1;

	req->pHeaders = qEntryInit();
	if(req->pHeaders == NULL) return req;

	//
	// 헤더 파싱
	//

	// Parse request line : "method uri protocol"
	{
		int nStreamStatus;
		char *pszReqMethod, *pszReqUri, *pszHttpVer, *pszTmp;

		// read line
		nStreamStatus = streamGets(szLineBuf, nSockFd, sizeof(szLineBuf), nTimeout*1000);
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
		if(isCorrectPath(req->pszRequestPath) == false) {
			DEBUG("Invalid URI format : %s", req->pszRequestUri);
			return req;
		}
		correctPath(req->pszRequestPath);
	}

	// Parse parameter headers : "key: value"
	while(true) {
		// read line
		if(streamGets(szLineBuf, nSockFd, sizeof(szLineBuf), nTimeout*1000) <= 0) return req;
		if(strlen(szLineBuf) == 0) break; // detect line-feed

		// separate :
		char *tmp = strstr(szLineBuf, ":");
		if(tmp == NULL) {
			DEBUG("Request header field is missing ':' separator.");
			return req;
		}

		*tmp = '\0';
		char *name = qStrUpper(qStrTrim(szLineBuf)); // 헤더 필드는 대문자로 저장
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

		// PUT과 POST인 경우엔 메모리 로드하지 않음
		if(req->nContentsLength <= MAX_HTTP_MEMORY_CONTENTS
			&& strcmp(req->pszRequestMethod, "PUT")	&& strcmp(req->pszRequestMethod, "POST")) {

			if(req->nContentsLength == 0) {
				req->pContents = strdup("");
			} else {
				// 메모리 할당
				req->pContents = (char *)malloc(req->nContentsLength + 1);
				if(req->pContents == NULL) {
					LOG_WARN("Memory allocation failed.");
					return req;
				}

				// 메모리에 저장
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

	// 정상 요청으로 플래그 변경
	req->nReqStatus = 1;

	return req;
}

bool httpRequestFree(struct HttpRequest *req) {
	if(req == NULL) return false;
	if(req->pszRequestMethod != NULL) free(req->pszRequestMethod);
	if(req->pszRequestUri != NULL) free(req->pszRequestUri);
	if(req->pszHttpVersion != NULL) free(req->pszHttpVersion);

	if(req->pszRequestHost != NULL) free(req->pszRequestHost);
	if(req->pszRequestPath != NULL) free(req->pszRequestPath);
	if(req->pszQueryString != NULL) free(req->pszQueryString);

	if(req->pHeaders != NULL) qEntryFree(req->pHeaders);
	if(req->pContents != NULL) free(req->pContents);
	free(req);

	return true;
}

static char *_getCorrectedHostname(const char *pszRequestHost) {
	char *pszHost = NULL;

	if(pszRequestHost != NULL) {
		pszHost = strdup(pszRequestHost);
		qStrLower(pszHost);

		// 디폴트 포트 80이 붙어온 경우엔 제거
		char *pszTmp = strstr(pszHost, ":");
		if(pszTmp != NULL && !strcmp(pszTmp, ":80")) *pszTmp = '\0';
	}

	return pszHost;
}
