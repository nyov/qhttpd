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

	// initialize response structure
	res = (struct HttpResponse *)malloc(sizeof(struct HttpResponse));
	if(res == NULL) return NULL;

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
 * @param pszHttpVer response protocol version, can be NULL to set response protocol as request protocol
 * @param bKeepAlive Keep-Alive. automatically turned of if request is not HTTP/1.1 or there is no keep-alive request.
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
		bKeepAlive = false;
		if(httpHeaderHasStr(req->pHeaders, "CONNECTION", "KEEP-ALIVE") == true
		|| httpHeaderHasStr(req->pHeaders, "CONNECTION", "TE") == true) {
			bKeepAlive = true;
		}
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

bool httpResponseSetContent(struct HttpResponse *res, const char *pszContentType, const char *pContent, off_t nContentsLength) {
	// content-type
	if(res->pszContentType != NULL) free(res->pszContentType);
	res->pszContentType = (pszContentType != NULL) ? strdup(pszContentType) : NULL;

	// content
	if(res->pContent != NULL) free(res->pContent);
	if(pContent == NULL) {
		res->pContent = NULL;
	} else {
		res->pContent = (char *)malloc(nContentsLength + 1);
		if(res->pContent == NULL) return false;
		memcpy((void *)res->pContent, pContent, nContentsLength);
		res->pContent[nContentsLength] = '\0';
	}

	// content-length
	res->nContentsLength = nContentsLength;

	return true;
}

bool httpResponseSetContentChunked(struct HttpResponse *res, bool bChunked) {
	res->bChunked = bChunked;
	if(bChunked == true) {
		if(res->pContent != NULL) {
			free(res->pContent);
			res->pContent = NULL;
		}

		if(res->nContentsLength != 0) {
			res->nContentsLength = 0;
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

bool httpResponseOut(struct HttpResponse *res, int nSockFd) {
	if(res->pszHttpVersion == NULL || res->nResponseCode == 0 || res->bOut == true) return false;

	//
	// set headers
	//

	if(res->bChunked == true) {
		httpHeaderSetStr(res->pHeaders, "Transfer-Encoding", "chunked");
	} else if(res->nContentsLength > 0) {
		httpHeaderSetStrf(res->pHeaders, "Content-Length", "%jd", res->nContentsLength);
	}

	// Content-Type header
	if(res->pszContentType != NULL) {
		httpHeaderSetStr(res->pHeaders, "Content-Type", res->pszContentType);
	}

	// Date header
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

	// end of headers
	streamPrintf(nSockFd, "\r\n");

	// print out contents binary
	if(res->nContentsLength > 0 && res->pContent != NULL) {
		streamWrite(nSockFd, res->pContent, res->nContentsLength);
		//streamPrintf(nSockFd, "\r\n");
	}

	res->bOut = true;
	return true;
}

/*
 * call after sending every chunk data with nSize 0.
 *
 * @return	a number of octets sent (do not include bytes which are sent for chunk boundary string)
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
		case HTTP_CODE_CONTINUE			: return "Continue";
		case HTTP_CODE_OK			: return "OK";
		case HTTP_CODE_CREATED			: return "Created";
		case HTTP_CODE_NO_CONTENT		: return "No content";
		case HTTP_CODE_PARTIAL_CONTENT		: return "Partial Content";
		case HTTP_CODE_MULTI_STATUS		: return "Multi Status";
		case HTTP_CODE_MOVED_TEMPORARILY	: return "Moved Temporarily";
		case HTTP_CODE_NOT_MODIFIED		: return "Not Modified";
		case HTTP_CODE_BAD_REQUEST		: return "Bad Request";
		case HTTP_CODE_FORBIDDEN		: return "Forbidden";
		case HTTP_CODE_NOT_FOUND		: return "Not Found";
		case HTTP_CODE_METHOD_NOT_ALLOWED	: return "Method Not Allowed";
		case HTTP_CODE_REQUEST_TIME_OUT		: return "Request Time Out";
		case HTTP_CODE_REQUEST_URI_TOO_LONG	: return "Request URI Too Long";
		case HTTP_CODE_INTERNAL_SERVER_ERROR	: return "Internal Server Error";
		case HTTP_CODE_NOT_IMPLEMENTED		: return "Not Implemented";
		case HTTP_CODE_SERVICE_UNAVAILABLE	: return "Service Unavailable";
		default : LOG_WARN("PLEASE DEFINE THE MESSAGE FOR %d RESPONSE", nResCode);
	}

	return "";
}
