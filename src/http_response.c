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

struct HttpResponse *httpResponseCreate(void) {
	struct HttpResponse *res;

	// Response 구조체 생성
	res = (struct HttpResponse *)malloc(sizeof(struct HttpResponse));
	if(res == NULL) return NULL;

	// Response 구조체 초기화
	memset((void *)res, 0, sizeof(struct HttpResponse));
	res->bOut = false;

	return res;
}

int httpResponseSetSimple(struct HttpRequest *req, struct HttpResponse *res, int nResCode, bool nKeepAlive, char *format, ...) {
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
	httpResponseSetHeader(res, "Date", qGetGmtimeStr(0));
	httpResponseSetHeaderf(res, "Server", "%s/%s", PRG_NAME, PRG_VERSION);

	// decide to turn on/off keep-alive
	if(g_conf.bKeepAliveEnable == true && bKeepAlive == true) {
		//if(strcmp(pszHttpVer, HTTP_PROTOCOL_11)) bKeepAlive = false; // HTTP/1.1이 아니면 - Jaguar 5000 호환성 문제로 제거
		if(httpHeaderHasString(req->pHeaders, "CONNECTION", "KEEP-ALIVE") == false
		&& httpHeaderHasString(req->pHeaders, "CONNECTION", "TE") == false) bKeepAlive = false; // 요청이 keep-alive가 아니면
	} else {
		bKeepAlive = false;
	}

	// Set keep-alive header
	if(bKeepAlive == true) {
		httpResponseSetHeader(res, "Connection", "Keep-Alive");
		httpResponseSetHeaderf(res, "Keep-Alive", "timeout=%d", req->nTimeout);

		pszHttpVer = HTTP_PROTOCOL_11;
	} else {
		httpResponseSetHeader(res, "Connection", "close");
	}

	// Set response code
	if(res->pszHttpVersion != NULL) free(res->pszHttpVersion);
	res->pszHttpVersion = strdup(pszHttpVer);
	res->nResponseCode = nResCode;

	// set to no content
	httpResponseSetContent(res, NULL, 0, NULL);

	return true;
}

bool httpResponseSetContent(struct HttpResponse *res, char *pszContentType, uint64_t nContentLength, char *pContent) {
	// content-type
	if(res->pszContentType != NULL) free(res->pszContentType);
	res->pszContentType = (pszContentType != NULL) ? strdup(pszContentType) :  NULL;

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
		if(res->pszContentType != NULL) {
			free(res->pszContentType);
			res->pszContentType = NULL;
		}

		if(res->pContent != NULL) {
			free(res->pContent);
			res->pContent = NULL;
		}
	}

	return true;
}

bool httpResponseSetContentHtml(struct HttpResponse *res, char *pszMsg) {
	char szContent[1024];

	snprintf(szContent, sizeof(szContent)-1,
		"<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">\r\n"
		"<html>\r\n"
		"<head><title>%d %s</title></head>\r\n"
		"<body>\r\n"
		"<h1>%d %s</h1>\r\n"
		"<p>%s</p>\r\n"
		"<hr>\r\n"
		"<address>%s/%s</address>\r\n"
		"</body></html>",
		res->nResponseCode, httpResponseGetMsg(res->nResponseCode),
		res->nResponseCode, httpResponseGetMsg(res->nResponseCode),
		pszMsg,
		PRG_NAME, PRG_VERSION
	);
	//szContent[sizeof(szContent)-1] = '\0';

	return httpResponseSetContent(res, "text/html", strlen(szContent), szContent);
}

/**
 * @param value NULL일 경우 name에 해당하는 헤더를 삭제
 */
bool httpResponseSetHeader(struct HttpResponse *res, char *pszName, char *pszValue) {
	if(res->pHeaders == NULL) {
		if(pszValue != NULL) res->pHeaders = qEntryAdd(NULL, pszName, pszValue, 1);
	} else {
		if(pszValue != NULL) qEntryAdd(res->pHeaders, pszName, pszValue, 1);
		else qEntryRemove(res->pHeaders, pszName);
	}

	return true;
}

bool httpResponseSetHeaderf(struct HttpResponse *res, char *pszName, char *format, ...) {
	char szValue[1024];
	va_list arglist;

	va_start(arglist, format);
	vsnprintf(szValue, sizeof(szValue)-1, format, arglist);
	szValue[sizeof(szValue)-1] = '\0';
	va_end(arglist);

	return httpResponseSetHeader(res, pszName, szValue);
}

bool httpResponseOut(struct HttpResponse *res, int nSockFd) {
	Q_ENTRY *entries;

	if(res->pszHttpVersion == NULL || res->nResponseCode == 0 || res->bOut == true) return false;

	//
	// 헤더 설정 및 조정
	//

	if(res->bChunked == true) {
		httpResponseSetHeader(res, "Transfer-Encoding", "chunked");
	} else {
		httpResponseSetHeaderf(res, "Content-Length", "%ju", res->nContentLength);
	}

	// Content-Type 헤더
	if(res->pszContentType != NULL) {
		httpResponseSetHeader(res, "Content-Type", res->pszContentType);
	}

	// Date 헤더 - 서버 시각
	httpResponseSetHeaderf(res, "Date", "%s", qGetGmtimeStr(time(NULL)));

	//
	// 출력
	//

	// 첫번째 라인은 응답 코드
	qSocketPrintf(nSockFd, "%s %d %s\r\n",
		res->pszHttpVersion,
		res->nResponseCode,
		httpResponseGetMsg(res->nResponseCode)
	);

	// 기타 헤더 출력
	for (entries = res->pHeaders; entries != NULL; entries = entries->next) {
		qSocketPrintf(nSockFd, "%s: %s\r\n", entries->name, entries->value);
	}

	// 헤더 끝. 공백 라인
	qSocketPrintf(nSockFd, "\r\n");

	// 컨텐츠 출력
	if(res->nContentLength > 0 && res->pContent != NULL) {
		qSocketWrite(nSockFd, res->pContent, res->nContentLength);
		//qSocketPrintf(nSockFd, "\r\n");
	}

	res->bOut = true;
	return true;
}

/*
 * chunk 데이터를 모두 보낸후엔 nSize를 0으로 설정하여 콜
 *
 * @return	요청 데이터 중 스트림으로 발송을 한 옥텟 수 (발송한 청크 헤더는 포함하지 않음)
 */
int httpResponseOutChunk(int nSockFd, char *pszData, int nSize) {
	int nSent = 0;
	qSocketPrintf(nSockFd, "%x\r\n", nSize);
	if(nSize > 0) {
		nSent = qSocketWrite(nSockFd, pszData, nSize);
	}
	qSocketPrintf(nSockFd, "\r\n");

	return nSent;
}

void httpResponseFree(struct HttpResponse *res) {
	if(res == NULL) return;

	if(res->pszHttpVersion != NULL) free(res->pszHttpVersion);
	if(res->pHeaders) qEntryFree(res->pHeaders);
	if(res->pszContentType != NULL) free(res->pszContentType);
	if(res->pContent != NULL) free(res->pContent);
	free(res);
}

char *httpResponseGetMsg(int nResCode) {
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
