/**************************************************************************
 * qHttpd - Specific Purpose Web Server             http://www.qDecoder.org
 *
 * Copyright (C) 2008 Seung-young Kim.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *************************************************************************/

#include "qhttpd.h"

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

	//
	// 헤더 파싱
	//

	// Parse request line : "method uri protocol"
	{
		int nStreamStatus;
		char *pszReqMethod, *pszReqUri, *pszHttpVer, *pszTmp;

		// read line
		nStreamStatus = qSocketGets(nSockFd, szLineBuf, sizeof(szLineBuf), nTimeout*1000);
		if(nStreamStatus == 0) { // timeout
			req->nReqStatus = -1;
			return req;
		} else if(nStreamStatus < 0) { // connection closed
			req->nReqStatus = -2;
			return req;
		}

		// parse line
		pszReqMethod = qStrtok(szLineBuf, " ", NULL);
		pszReqUri = qStrtok(NULL, " ", NULL);
		pszHttpVer = qStrtok(NULL, " ", NULL);
		pszTmp = qStrtok(NULL, " ", NULL);

		if(pszReqMethod == NULL || pszReqUri == NULL || pszHttpVer == NULL || pszTmp != NULL) {
			DEBUG("Invalid request line.");
			return req;
		}

		//DEBUG("pszReqMethod %s", pszReqMethod);
		//DEBUG("pszReqUri %s", pszReqUri);
		//DEBUG("pszHttpVer %s", pszHttpVer);

		// request method
		qStrupr(pszReqMethod);
		/*
		if(strcmp(pszReqMethod, "GET")
			&& strcmp(pszReqMethod, "HEAD")
			&& strcmp(pszReqMethod, "POST")
			&& strcmp(pszReqMethod, "PUT")
			&& strcmp(pszReqMethod, "DELETE")
			&& strcmp(pszReqMethod, "LINK")
			&& strcmp(pszReqMethod, "UNLINK")
			&& strcmp(pszReqMethod, "OPTIONS")
			// DAV method
			&& strcmp(pszReqMethod, "PROPFIND")
			&& strcmp(pszReqMethod, "PROPPATCH")
			&& strcmp(pszReqMethod, "COPY")
			&& strcmp(pszReqMethod, "MOVE")
			&& strcmp(pszReqMethod, "MKCOL")
			&& strcmp(pszReqMethod, "LOCK")
			&& strcmp(pszReqMethod, "UNLOCK")
		) {
			DEBUG("WARNING: Unknown request: %s", pszReqMethod);
			return req;
		}
		*/
		req->pszRequestMethod = strdup(pszReqMethod);

		// request uri
		if(pszReqUri[0] == '/') {
			req->pszRequestUri = strdup(pszReqUri);
			if(req->pszRequestUri == NULL) {
				DEBUG("Decoding failed.");
				return req;
			}
		} else if(!strncasecmp(pszReqUri, "HTTP://", 7)) {
			// URI의 호스트명은 Host 헤더로 넣고 pszRequestUri에는 경로만
			pszTmp = strstr(pszReqUri + 8, "/");
			if(pszTmp == NULL) {	// URL이 없는경우 ex) http://a.b.c
				req->pHeaders = qEntryAdd(req->pHeaders, "Host",pszReqUri+8, 1);
				req->pszRequestUri = strdup("/");
			} else {		// URL이 있는경우 ex) http://a.b.c/100
				*pszTmp = '\0';
				req->pHeaders = qEntryAdd(req->pHeaders, "Host",pszReqUri+8, 1);
				*pszTmp = '/';
				req->pszRequestUri = strdup(pszReqUri);
				if(req->pszRequestUri == NULL) {
					DEBUG("Decoding failed.");
					return req;
				}
			}
		} else {
			DEBUG("Unable to parse uri: %s", pszReqUri);
			return req;
		}

		// request url (no query string)
		req->pszRequestUrl = strdup(req->pszRequestUri);

		// remove query string from url
		pszTmp = strstr(req->pszRequestUrl, "?");
		if(pszTmp != NULL) {
			*pszTmp ='\0';
			req->pszQueryString = strdup(pszTmp + 1);
		} else {
			req->pszQueryString = strdup("");
		}

		// decode path
		qUrlDecode(req->pszRequestUrl);

		// check path
		if(isCorrectPath(req->pszRequestUrl) == false) {
			DEBUG("Invalid URI format : %s", req->pszRequestUrl);
			return req;
		}
		correctPath(req->pszRequestUrl);

		// http version
		qStrupr(pszHttpVer);
		if(strcmp(pszHttpVer, HTTP_PROTOCOL_09)
			&& strcmp(pszHttpVer, HTTP_PROTOCOL_10)
			&& strcmp(pszHttpVer, HTTP_PROTOCOL_11)
		) {
			DEBUG("Unknown protocol: %s", pszHttpVer);
			return req;
		}
		req->pszHttpVersion = strdup(pszHttpVer);

		//DEBUG("%s %d %s", req->pszRequestUri, req->nServiceId, req->pszQueryString);
	}

	// Parse parameter headers : "key: value"
	while(true) {
		char *name, *value, *tmp;

		// read line
		if(qSocketGets(nSockFd, szLineBuf, sizeof(szLineBuf), nTimeout*1000) <= 0) return req;
		if(strlen(szLineBuf) == 0) break; // detect line-feed

		// separate :
		tmp = strstr(szLineBuf, ":");
		if(tmp == NULL) {
			DEBUG("Request header field is missing ':' separator.");
			return req;
		}

		*tmp = '\0';
		name = qStrupr(qRemoveSpace(szLineBuf)); // 헤더 필드는 대문자로 저장
		value = qRemoveSpace(tmp + 1);

		// put
		Q_ENTRY *entry = qEntryAdd(req->pHeaders, name, value, 1);
		if(req->pHeaders == NULL) req->pHeaders = entry;
	}

	// Parse Contents
	if(httpHeaderGetValue(req->pHeaders, "CONTENT-LENGTH") != NULL) {
		req->nContentsLength = convStr2Uint64(httpHeaderGetValue(req->pHeaders, "CONTENT-LENGTH"));

		// PUT과 POST인 경우엔 메모리 로드하지 않음
		if(req->nContentsLength <= HTTP_MAX_MEMORY_CONTENTS
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
				int nReaded = qSocketSaveIntoMemory(nSockFd, req->nContentsLength, req->pContents, nTimeout*1000);
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

void httpRequestFree(struct HttpRequest *req) {
	if(req == NULL) return;
	if(req->pszRequestMethod != NULL) free(req->pszRequestMethod);
	if(req->pszRequestUri != NULL) free(req->pszRequestUri);
	if(req->pszRequestUrl != NULL) free(req->pszRequestUrl);
	if(req->pszQueryString != NULL) free(req->pszQueryString);
	if(req->pszHttpVersion != NULL) free(req->pszHttpVersion);

	if(req->pHeaders != NULL) qEntryFree(req->pHeaders);
	if(req->pContents != NULL) free(req->pContents);
	free(req);
}
