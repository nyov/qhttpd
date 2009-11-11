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

struct HttpResponse *httpResponseCreate(void) {
	struct HttpResponse *res;

	// Response 구조체 생성
	res = (struct HttpResponse *)malloc(sizeof(struct HttpResponse));
	if(res == NULL) return NULL;

	// Response 구조체 초기화
	memset((void *)res, 0, sizeof(struct HttpResponse));
	res->bOut = false;
	res->pHeaders = qEntry();
	if(res->pHeaders == NULL) return NULL;

	return res;
}

int httpResponseSetSimple(struct HttpRequest *req, struct HttpResponse *res, int nResCode, bool nKeepAlive, const char *format, ...) {
	httpResponseSetCode(res, nResCode, req, nKeepAlive);

	if(format != NULL) {
		char szMsg[1024];
		va_list arglist;

		va_start(arglist, format);
		vsnprintf(szMsg, sizeof(szMsg)-1, format, arglist);
		szMsg[sizeof(szMsg)-1] = '\0';
		va_end(arglist);

		httpResponseSetContentHtml(res, szMsg);
	}

	return res->nResponseCode;
}

/**
 * @param pszHttpVer 응답 프로토콜 버젼, NULL시 요청 프로토콜 버젼으로 설정
 * @param bKeepAlive Keep-Alive 여부, true시에도 요청이 HTTP/1.1이 아니거나, Keep-Alive 요청이 없을경우엔 자동 false 됨
 */
bool httpResponseSetCode(struct HttpResponse *res, int nResCode, struct HttpRequest *req, bool bKeepAlive) {
	char *pszHttpVer;

	// Version setting
	pszHttpVer = req->pszHttpVersion;
	if(pszHttpVer == NULL) pszHttpVer = HTTP_PROTOCOL_11;

	// default headers
	httpHeaderSetStr(res->pHeaders, "Date", qTimeGetGmtStaticStr(0));
	httpHeaderSetStrf(res->pHeaders, "Server", "%s/%s (%s)", g_prgname, g_prgversion, g_prginfo);

	// decide to turn on/off keep-alive
	if(g_conf.bKeepAliveEnable == true && bKeepAlive == true) {
		//if(strcmp(pszHttpVer, HTTP_PROTOCOL_11)) bKeepAlive = false; // HTTP/1.1이 아니면 - Jaguar 5000 호환성 문제로 제거
		if(httpHeaderHasStr(req->pHeaders, "CONNECTION", "KEEP-ALIVE") == false
		&& httpHeaderHasStr(req->pHeaders, "CONNECTION", "TE") == false) bKeepAlive = false; // 요청이 keep-alive가 아니면
	} else {
		bKeepAlive = false;
	}

	// Set keep-alive header
	if(bKeepAlive == true) {
		httpHeaderSetStr(res->pHeaders, "Connection", "Keep-Alive");
		httpHeaderSetStrf(res->pHeaders, "Keep-Alive", "timeout=%d", req->nTimeout);

		pszHttpVer = HTTP_PROTOCOL_11;
	} else {
		httpHeaderSetStr(res->pHeaders, "Connection", "close");
	}

	// Set response code
	if(res->pszHttpVersion != NULL) free(res->pszHttpVersion);
	res->pszHttpVersion = strdup(pszHttpVer);
	res->nResponseCode = nResCode;

	return true;
}

bool httpResponseSetContent(struct HttpResponse *res, const char *pszContentType, const char *pContent, size_t nContentLength) {
	// content-type
	if(res->pszContentType != NULL) free(res->pszContentType);
	res->pszContentType = (pszContentType != NULL) ? strdup(pszContentType) : NULL;

	// content
	if(res->pContent != NULL) free(res->pContent);
	if(pContent == NULL) {
		res->pContent = NULL;
	} else {
		res->pContent = (char *)malloc(nContentLength + 1);
		if(res->pContent == NULL) return false;
		memcpy((void *)res->pContent, pContent, nContentLength);
		res->pContent[nContentLength] = '\0';
	}

	// content-length
	res->nContentLength = nContentLength;

	return true;
}

bool httpResponseSetContentChunked(struct HttpResponse *res, bool bChunked) {
	res->bChunked = bChunked;
	if(bChunked == true) {
		if(res->pContent != NULL) {
			free(res->pContent);
			res->pContent = NULL;
		}

		if(res->nContentLength != 0) {
			res->nContentLength = 0;
		}
	}

	return true;
}

bool httpResponseSetContentHtml(struct HttpResponse *res, const char *pszMsg) {
	char szContent[1024];

	snprintf(szContent, sizeof(szContent)-1,
		"<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">\r\n"
		"<html>\r\n"
		"<head><title>%d %s</title></head>\r\n"
		"<body>\r\n"
		"<h1>%d %s</h1>\r\n"
		"<p>%s</p>\r\n"
		"<hr>\r\n"
		"<address>%s %s/%s</address>\r\n"
		"</body></html>",
		res->nResponseCode, httpResponseGetMsg(res->nResponseCode),
		res->nResponseCode, httpResponseGetMsg(res->nResponseCode),
		pszMsg,
		g_prginfo, g_prgname, g_prgversion
	);
	//szContent[sizeof(szContent)-1] = '\0';

	return httpResponseSetContent(res, "text/html", szContent, strlen(szContent));
}

/**
 * @param value NULL일 경우 name에 해당하는 헤더를 삭제
 */
bool httpResponseOut(struct HttpResponse *res, int nSockFd) {
	if(res->pszHttpVersion == NULL || res->nResponseCode == 0 || res->bOut == true) return false;

	//
	// 헤더 설정 및 조정
	//

	if(res->bChunked == true) {
		httpHeaderSetStr(res->pHeaders, "Transfer-Encoding", "chunked");
	} else {
		httpHeaderSetStrf(res->pHeaders, "Content-Length", "%jd", res->nContentLength);
	}

	// Content-Type 헤더
	if(res->pszContentType != NULL) {
		httpHeaderSetStr(res->pHeaders, "Content-Type", res->pszContentType);
	}

	// Date 헤더 - 서버 시각
	httpHeaderSetStr(res->pHeaders, "Date", qTimeGetGmtStaticStr(0));

	//
	// Print out
	//

	// first line is response code
	streamPrintf(nSockFd, "%s %d %s\r\n",
		res->pszHttpVersion,
		res->nResponseCode,
		httpResponseGetMsg(res->nResponseCode)
	);

	// print out headers
	Q_ENTRY *tbl = res->pHeaders;
	Q_NLOBJ_T obj;
	memset((void*)&obj, 0, sizeof(obj)); // must be cleared before call
	tbl->lock(tbl);
	while(tbl->getNext(tbl, &obj, NULL, false) == true) {
		streamPrintf(nSockFd, "%s: %s\r\n", obj.name, (char*)obj.data);
	}
	tbl->unlock(tbl);

	// 헤더 끝. 공백 라인
	streamPrintf(nSockFd, "\r\n");

	// 컨텐츠 출력
	if(res->nContentLength > 0 && res->pContent != NULL) {
		streamWrite(nSockFd, res->pContent, res->nContentLength);
		//streamPrintf(nSockFd, "\r\n");
	}

	res->bOut = true;
	return true;
}

/*
 * chunk 데이터를 모두 보낸후엔 nSize를 0으로 설정하여 콜
 *
 * @return	요청 데이터 중 스트림으로 발송을 한 옥텟 수 (발송한 청크 헤더는 포함하지 않음)
 */
int httpResponseOutChunk(int nSockFd, const char *pszData, int nSize) {
	int nSent = 0;
	streamPrintf(nSockFd, "%x\r\n", nSize);
	if(nSize > 0) {
		nSent = streamWrite(nSockFd, pszData, nSize);
	}
	streamPrintf(nSockFd, "\r\n");

	return nSent;
}

void httpResponseFree(struct HttpResponse *res) {
	if(res == NULL) return;

	if(res->pszHttpVersion != NULL) free(res->pszHttpVersion);
	if(res->pHeaders) res->pHeaders->free(res->pHeaders);
	if(res->pszContentType != NULL) free(res->pszContentType);
	if(res->pContent != NULL) free(res->pContent);
	free(res);
}

const char *httpResponseGetMsg(int nResCode) {
	switch(nResCode) {
		case HTTP_CONTINUE			: return "Continue";
		case HTTP_RESCODE_OK			: return "Ok";
		case HTTP_RESCODE_CREATED		: return "Created";
		case HTTP_NO_CONTENT			: return "No content";
		case HTTP_MULTI_STATUS			: return "Multi status";
		case HTTP_RESCODE_MOVED_TEMPORARILY	: return "Moved temporarily";
		case HTTP_RESCODE_NOT_MODIFIED		: return "Not modified";
		case HTTP_RESCODE_BAD_REQUEST		: return "Bad request";
		case HTTP_RESCODE_FORBIDDEN		: return "Forbidden";
		case HTTP_RESCODE_NOT_FOUND		: return "Not found";
		case HTTP_RESCODE_METHOD_NOT_ALLOWED	: return "Method not allowed";
		case HTTP_RESCODE_REQUEST_TIME_OUT	: return "Request time out";
		case HTTP_RESCODE_REQUEST_URI_TOO_LONG	: return "Request URI too long";
		case HTTP_RESCODE_INTERNAL_SERVER_ERROR	: return "Internal server error";
		case HTTP_RESCODE_NOT_IMPLEMENTED	: return "Not implemented";
		case HTTP_RESCODE_SERVICE_UNAVAILABLE	: return "Service unavailable";
		default : LOG_WARN("PLEASE DEFINE THE MESSAGE FOR %d RESPONSE", nResCode);
	}
	return "";
}
