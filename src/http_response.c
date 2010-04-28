/*
 * Copyright 2008-2010 The qDecoder Project. All rights reserved.
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
 *
 * $Id$
 */

#include "qhttpd.h"

struct HttpResponse *httpResponseCreate(struct HttpRequest *pReq) {
	struct HttpResponse *pRes;

	// initialize response structure
	pRes = (struct HttpResponse *)malloc(sizeof(struct HttpResponse));
	if(pRes == NULL) return NULL;

	Q_ENTRY *pHeaders = qEntry();
	if(pHeaders == NULL) {
		free(pRes);
		return NULL;
	}

	memset((void *)pRes, 0, sizeof(struct HttpResponse));
	pRes->pHeaders = pHeaders;
	pRes->pReq = pReq;

	return pRes;
}

int httpResponseSetSimple(struct HttpResponse *pRes, int nResCode, bool nKeepAlive, const char *pszText) {
	httpResponseSetCode(pRes, nResCode, nKeepAlive);

	if(pszText != NULL) {
		httpResponseSetContentHtml(pRes, pszText);
	}

	return pRes->nResponseCode;
}

/**
 * @param pszHttpVer response protocol version, can be NULL to set response protocol as request protocol
 * @param bKeepAlive Keep-Alive. automatically turned of if request is not HTTP/1.1 or there is no keep-alive request.
 */
bool httpResponseSetCode(struct HttpResponse *pRes, int nResCode, bool bKeepAlive) {
	struct HttpRequest *pReq = pRes->pReq;

	// version setting
	char *pszHttpVer = NULL;
	if(pReq != NULL) {
		pszHttpVer = pReq->pszHttpVersion;
	}
	if(pszHttpVer == NULL) pszHttpVer = HTTP_PROTOCOL_11;

	// default headers
	httpHeaderSetStr(pRes->pHeaders, "Date", qTimeGetGmtStaticStr(0));
	httpHeaderSetStrf(pRes->pHeaders, "Server", "%s/%s (%s)", g_prgname, g_prgversion, g_prginfo);

	// decide to turn on/off keep-alive
	if(pReq != NULL
	&& g_conf.bEnableKeepAlive == true && bKeepAlive == true) {
		bKeepAlive = false;

		if(!strcmp(pszHttpVer, HTTP_PROTOCOL_11)) {
			if(httpHeaderHasStr(pReq->pHeaders, "CONNECTION", "CLOSE") == false) {
				bKeepAlive = true;
			}
		} else {
			if(httpHeaderHasStr(pReq->pHeaders, "CONNECTION", "KEEP-ALIVE") == true
			|| httpHeaderHasStr(pReq->pHeaders, "CONNECTION", "TE") == true) {
				bKeepAlive = true;
			}
		}
	} else {
		bKeepAlive = false;
	}

	// Set keep-alive header
	if(bKeepAlive == true) {
		httpHeaderSetStr(pRes->pHeaders, "Connection", "Keep-Alive");
		httpHeaderSetStrf(pRes->pHeaders, "Keep-Alive", "timeout=%d", pReq->nTimeout);
	} else {
		httpHeaderSetStr(pRes->pHeaders, "Connection", "close");
	}

	// Set response code
	if(pRes->pszHttpVersion != NULL) free(pRes->pszHttpVersion);
	pRes->pszHttpVersion = strdup(pszHttpVer);
	pRes->nResponseCode = nResCode;

	return true;
}

bool httpResponseSetContent(struct HttpResponse *pRes, const char *pszContentType, const char *pContent, off_t nContentsLength) {
	// content-type
	if(pRes->pszContentType != NULL) free(pRes->pszContentType);
	pRes->pszContentType = (pszContentType != NULL) ? strdup(pszContentType) : NULL;

	// content
	if(pRes->pContent != NULL) free(pRes->pContent);
	if(pContent == NULL) {
		pRes->pContent = NULL;
	} else {
		pRes->pContent = (char *)malloc(nContentsLength + 1);
		if(pRes->pContent == NULL) return false;
		memcpy((void *)pRes->pContent, pContent, nContentsLength);
		pRes->pContent[nContentsLength] = '\0'; // for debugging purpose
	}

	// content-length
	pRes->nContentsLength = nContentsLength;

	return true;
}

bool httpResponseSetContentChunked(struct HttpResponse *pRes, bool bChunked) {
	pRes->bChunked = bChunked;
	if(bChunked == true) {
		if(pRes->pContent != NULL) {
			free(pRes->pContent);
			pRes->pContent = NULL;
		}

		if(pRes->nContentsLength != 0) {
			pRes->nContentsLength = 0;
		}
	}

	return true;
}

bool httpResponseSetContentHtml(struct HttpResponse *pRes, const char *pszMsg) {
	char szContent[1024];

	snprintf(szContent, sizeof(szContent)-1,
		"<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">" CRLF
		"<html>" CRLF
		"<head><title>%d %s</title></head>" CRLF
		"<body>" CRLF
		"<h1>%d %s</h1>" CRLF
		"<p>%s</p>" CRLF
		"<hr>" CRLF
		"<address>%s %s/%s</address>" CRLF
		"</body></html>",
		pRes->nResponseCode, httpResponseGetMsg(pRes->nResponseCode),
		pRes->nResponseCode, httpResponseGetMsg(pRes->nResponseCode),
		pszMsg,
		g_prginfo, g_prgname, g_prgversion
	);
	//szContent[sizeof(szContent)-1] = '\0';

	return httpResponseSetContent(pRes, "text/html", szContent, strlen(szContent));
}

bool httpResponseSetAuthRequired(struct HttpResponse *pRes, enum HttpAuthT nAuthType, const char *pszRealm) {
	if(nAuthType == HTTP_AUTH_BASIC) {
		httpHeaderSetStrf(pRes->pHeaders, "WWW-Authenticate", "Basic realm=\"%s\"", pszRealm);
		return true;
	} else if(nAuthType == HTTP_AUTH_DIGEST) {
		char *pszNonce = qStrUnique(NULL);
		httpHeaderSetStrf(pRes->pHeaders, "WWW-Authenticate", "Digest realm=\"%s\", nonce=\"%s\", algorithm=MD5, qop=\"auth\"", pszRealm, pszNonce);
		free(pszNonce);
		return true;
	}

	// not supported
	return false;
}

bool httpResponseOut(struct HttpResponse *pRes, int nSockFd) {
	if(pRes->bOut == true) return false;

	//
	// hook handling
	//
#ifdef ENABLE_HOOK
	if(hookResponseHandler(pRes->pReq, pRes) == false) {
		LOG_WARN("An error occured while processing hookResponseHandler().");
	}
#endif

#ifdef ENABLE_LUA
	if(g_conf.bEnableLua == true
	&& luaResponseHandler() == false) {
		LOG_WARN("An error occured while processing luaResponseHandler().");
	}
#endif

	// check header
	if(pRes->pszHttpVersion == NULL || pRes->nResponseCode == 0) return false;

	// set response information
	poolSetConnResponse(pRes);

	//
	// set headers
	//

	// check exit request and adjust keep-alive header
	if(poolGetExitRequest() == true
	|| (g_conf.nMaxKeepAliveRequests > 0 && poolGetChildKeepaliveRequests() >= g_conf.nMaxKeepAliveRequests))
	{
		httpHeaderSetStr(pRes->pHeaders, "Connection", "close");
	}

	// check chunked transfer header
	if(pRes->bChunked == true) {
		httpHeaderSetStr(pRes->pHeaders, "Transfer-Encoding", "chunked");
	} else if(pRes->nContentsLength > 0 || pRes->pContent != NULL) {
		httpHeaderSetStrf(pRes->pHeaders, "Content-Length", "%jd", pRes->nContentsLength);
	}

	// Content-Type header
	if(pRes->pszContentType != NULL) {
		httpHeaderSetStr(pRes->pHeaders, "Content-Type", pRes->pszContentType);
	}

	// Date header
	httpHeaderSetStr(pRes->pHeaders, "Date", qTimeGetGmtStaticStr(0));

	//
	// Print out
	//

	Q_OBSTACK *outBuf = qObstack();
	if(outBuf == NULL) return false;

	// first line is response code
	outBuf->growStrf(outBuf, "%s %d %s" CRLF,
		pRes->pszHttpVersion,
		pRes->nResponseCode,
		httpResponseGetMsg(pRes->nResponseCode)
	);

	// print out headers
	Q_ENTRY *tbl = pRes->pHeaders;
	Q_NLOBJ_T obj;
	memset((void*)&obj, 0, sizeof(obj)); // must be cleared before call
	tbl->lock(tbl);
	while(tbl->getNext(tbl, &obj, NULL, false) == true) {
		outBuf->growStrf(outBuf, "%s: %s" CRLF, obj.name, (char*)obj.data);
	}
	tbl->unlock(tbl);

	// end of headers
	outBuf->growStr(outBuf, CRLF);

	// buf flush
	streamStackOut(nSockFd, outBuf);

	// free buf
	outBuf->free(outBuf);

	// print out contents binary
	if(pRes->nContentsLength > 0 && pRes->pContent != NULL) {
		streamWrite(nSockFd, pRes->pContent, pRes->nContentsLength, g_conf.nConnectionTimeout * 1000);
		//streamPrintf(nSockFd, "%s", CRLF);
	}

	pRes->bOut = true;
	return true;
}

/*
 * set nSize to 0 for final call
 *
 * @return	a number of octets sent (do not include bytes which are sent for chunk boundary string)
 */
int httpResponseOutChunk(int nSockFd, const void *pData, size_t nSize) {
	struct iovec vectors[3];
	int nVecCnt = 0;

	char szChunkSizeHead[16 + CONST_STRLEN(CRLF) + 1];
	snprintf(szChunkSizeHead, sizeof(szChunkSizeHead), "%x" CRLF, (unsigned int)nSize);

	// add to vector
	vectors[nVecCnt].iov_base = szChunkSizeHead;
	vectors[nVecCnt].iov_len = strlen(szChunkSizeHead);
	nVecCnt++;

	int nSent = 0;
	if(nSize > 0) {
		// add to vector
		vectors[nVecCnt].iov_base = (void *)pData;
		vectors[nVecCnt].iov_len = nSize;
		nVecCnt++;
	}

	// add to vector
	vectors[nVecCnt].iov_base = CRLF;
	vectors[nVecCnt].iov_len = CONST_STRLEN(CRLF);
	nVecCnt++;

	// print out
	streamWritev(nSockFd, vectors, nVecCnt);

	return nSent;
}

bool httpResponseReset(struct HttpResponse *pRes) {
	if(pRes == NULL || pRes->bOut == true) return false;

	// allocate Q_ENTRY for
	Q_ENTRY *pHeaders = qEntry();
	if(pHeaders == NULL) return false;

	if(pRes->pszHttpVersion != NULL) free(pRes->pszHttpVersion);
	if(pRes->pHeaders) pRes->pHeaders->free(pRes->pHeaders);
	if(pRes->pszContentType != NULL) free(pRes->pszContentType);
	if(pRes->pContent != NULL) free(pRes->pContent);

	memset((void *)pRes, 0, sizeof(struct HttpResponse));
	pRes->pHeaders = pHeaders;

	return true;
}

void httpResponseFree(struct HttpResponse *pRes) {
	if(pRes == NULL) return;

	if(pRes->pszHttpVersion != NULL) free(pRes->pszHttpVersion);
	if(pRes->pHeaders) pRes->pHeaders->free(pRes->pHeaders);
	if(pRes->pszContentType != NULL) free(pRes->pszContentType);
	if(pRes->pContent != NULL) free(pRes->pContent);
	free(pRes);
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
		case HTTP_CODE_UNAUTHORIZED		: return "Authorization Required";
		case HTTP_CODE_FORBIDDEN		: return "Forbidden";
		case HTTP_CODE_NOT_FOUND		: return "Not Found";
		case HTTP_CODE_METHOD_NOT_ALLOWED	: return "Method Not Allowed";
		case HTTP_CODE_REQUEST_TIME_OUT		: return "Request Time Out";
		case HTTP_CODE_GONE			: return "Gone";
		case HTTP_CODE_REQUEST_URI_TOO_LONG	: return "Request URI Too Long";
		case HTTP_CODE_LOCKED			: return "Locked";
		case HTTP_CODE_INTERNAL_SERVER_ERROR	: return "Internal Server Error";
		case HTTP_CODE_NOT_IMPLEMENTED		: return "Not Implemented";
		case HTTP_CODE_SERVICE_UNAVAILABLE	: return "Service Unavailable";
		default : LOG_WARN("PLEASE DEFINE THE MESSAGE FOR %d RESPONSE", nResCode);
	}

	return "";
}
