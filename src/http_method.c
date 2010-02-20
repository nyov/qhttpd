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

/*
 * http method - OPTIONS
 */
int httpMethodOptions(struct HttpRequest *pReq, struct HttpResponse *pRes) {
	if(g_conf.methods.bOptions == false) return response405(pReq, pRes);

	httpResponseSetCode(pRes, HTTP_CODE_OK, pReq, true);
	httpHeaderSetStr(pRes->pHeaders, "Allow", g_conf.szAllowedMethods);
	httpResponseSetContent(pRes, "httpd/unix-directory", "", 0);

	return HTTP_CODE_OK;
}

/*
 * http method - HEAD
 */
int httpMethodHead(struct HttpRequest *pReq, struct HttpResponse *pRes) {
	if(g_conf.methods.bHead == false) return response405(pReq, pRes);

	// generate abs path
	char szFilePath[PATH_MAX];
	httpRequestGetSysPath(pReq, szFilePath, sizeof(szFilePath), pReq->pszRequestPath);

	// get file stat
	struct stat filestat;
	if (sysStat(szFilePath, &filestat) < 0) {
		return response404(pReq, pRes);
	}

	// do action
	int nResCode = HTTP_CODE_FORBIDDEN;
	const char *pszHtmlMsg = NULL;

	if(S_ISREG(filestat.st_mode)) {
		nResCode = HTTP_CODE_OK;

		// get Etag
		char szEtag[ETAG_MAX];
		getEtag(szEtag, sizeof(szEtag), pReq->pszRequestPath, &filestat);

		// set headers
		httpHeaderSetStr(pRes->pHeaders, "Accept-Ranges", "bytes");
		httpHeaderSetStrf(pRes->pHeaders, "Last-Modified", "%s", qTimeGetGmtStaticStr(filestat.st_mtime));
		httpHeaderSetStrf(pRes->pHeaders, "ETag", "\"%s\"", szEtag);
		httpHeaderSetExpire(pRes->pHeaders, g_conf.nResponseExpires);
	} else {
		pszHtmlMsg = httpResponseGetMsg(nResCode);
	}

	// set response
	httpResponseSetSimple(pReq, pRes, nResCode, true, pszHtmlMsg);
	return nResCode;
}

/*
 * http method - GET
 */
int httpMethodGet(struct HttpRequest *pReq, struct HttpResponse *pRes) {
	if(g_conf.methods.bGet == false) return response405(pReq, pRes);

	// generate abs path
	char szFilePath[PATH_MAX];
	httpRequestGetSysPath(pReq, szFilePath, sizeof(szFilePath), pReq->pszRequestPath);

	// get file stat
	struct stat filestat;
	if (sysStat(szFilePath, &filestat) < 0) {
		return response404(pReq, pRes);
	}

 	// is directory?
 	if(S_ISDIR(filestat.st_mode) && pReq->pszDirectoryIndex != NULL) {
 		qStrCatf(szFilePath, "/%s", pReq->pszDirectoryIndex);
 		if (sysStat(szFilePath, &filestat) < 0) {
			return response404(pReq, pRes);
		}
 	}

	// do action
	int nResCode = HTTP_CODE_FORBIDDEN;
	if(S_ISREG(filestat.st_mode)) {
		// open file
		int nFd = sysOpen(szFilePath, O_RDONLY , 0);
		if(nFd < 0) return response404(pReq, pRes);

		// send file
		nResCode = httpRealGet(pReq, pRes, nFd, &filestat, mimeDetect(szFilePath));

		// close file
		sysClose(nFd);
	}

	// set response except of 200 and 304
	if(pRes->nResponseCode == 0) {
		httpResponseSetSimple(pReq, pRes, nResCode, true, httpResponseGetMsg(nResCode));
	}

	return nResCode;
}

/*
 * returns expected response code. it do not send response except of HTTP_CODE_OK and HTTP_CODE_NOT_MODIFIED.
 */
int httpRealGet(struct HttpRequest *pReq, struct HttpResponse *pRes, int nFd, struct stat *pStat, const char *pszContentType) {
	// get size
	off_t nFilesize = pStat->st_size;

	// get Etag
	char szEtag[ETAG_MAX];
	getEtag(szEtag, sizeof(szEtag), pReq->pszRequestPath, pStat);

	//
	// header handling section
	//

	// check If-Modified-Since header
	const char *pszIfModifiedSince = httpHeaderGetStr(pReq->pHeaders, "IF-MODIFIED-SINCE");
	if(pszIfModifiedSince != NULL) {
		time_t nUnivTime = qTimeParseGmtStr(pszIfModifiedSince);

		// if succeed to parsing header && file does not modified
		if(nUnivTime >= 0 && nUnivTime > pStat->st_mtime) {
			httpHeaderSetStrf(pRes->pHeaders, "ETag", "\"%s\"", szEtag);
			httpHeaderSetExpire(pRes->pHeaders, g_conf.nResponseExpires);
			return httpResponseSetSimple(pReq, pRes, HTTP_CODE_NOT_MODIFIED, true, NULL);
		}
	}

	// check If-None-Match header
	const char *pszIfNoneMatch = httpHeaderGetStr(pReq->pHeaders, "IF-NONE-MATCH");
	if(pszIfNoneMatch != NULL) {
		char *pszMatchEtag = strdup(pszIfNoneMatch);
		qStrUnchar(pszMatchEtag, '"', '"');

		// if ETag is same
		if(!strcmp(pszMatchEtag, szEtag)) {
			httpHeaderSetStrf(pRes->pHeaders, "ETag", "\"%s\"", szEtag);
			httpHeaderSetExpire(pRes->pHeaders, g_conf.nResponseExpires);
			return httpResponseSetSimple(pReq, pRes, HTTP_CODE_NOT_MODIFIED, true, NULL);
		}
		free(pszMatchEtag);
	}

	// check Range header
	off_t nRangeOffset1, nRangeOffset2, nRangeSize;
	bool bRangeRequest = false;
	const char *pszRange = httpHeaderGetStr(pReq->pHeaders, "RANGE");
	if(pszRange != NULL) {
		bRangeRequest = httpHeaderParseRange(pszRange, nFilesize, &nRangeOffset1, &nRangeOffset2, &nRangeSize);
	}

	// in case of no Range header or parsing failure
	if(bRangeRequest == false) {
		nRangeOffset1 = 0;
		nRangeOffset2 = nFilesize - 1;
		nRangeSize = nFilesize;
	}

	//
	// set response headers
	//

	httpResponseSetCode(pRes, (bRangeRequest == false) ? HTTP_CODE_OK : HTTP_CODE_PARTIAL_CONTENT, pReq, true);
	httpResponseSetContent(pRes, pszContentType, NULL, nRangeSize);

	httpHeaderSetStr(pRes->pHeaders, "Accept-Ranges", "bytes");
	httpHeaderSetStrf(pRes->pHeaders, "Last-Modified", "%s", qTimeGetGmtStaticStr(pStat->st_mtime));
	httpHeaderSetStrf(pRes->pHeaders, "ETag", "\"%s\"", szEtag);
	httpHeaderSetExpire(pRes->pHeaders, g_conf.nResponseExpires);

	if(bRangeRequest == true) {
		httpHeaderSetStrf(pRes->pHeaders, "Content-Range", "bytes %jd-%jd/%jd", nRangeOffset1, nRangeOffset2, nFilesize);
	}

	// print out headers
	httpResponseOut(pRes, pReq->nSockFd);

	//
	// print out data
	//
	if(nFilesize > 0) {
		if(nRangeOffset1 > 0) {
			lseek(nFd, nRangeOffset1, SEEK_SET);
		}

		off_t nSent = streamSend(pReq->nSockFd, nFd, nRangeSize, pReq->nTimeout*1000);
		if(nSent != nRangeSize) {
			LOG_INFO("Connection closed by foreign host. (%s/%jd/%jd/%jd)", pReq->pszRequestPath, nSent, nRangeOffset1, nRangeSize);
		}
	}

	return HTTP_CODE_OK;
}

/*
 * http method - PUT
 */
int httpMethodPut(struct HttpRequest *pReq, struct HttpResponse *pRes) {
	if(g_conf.methods.bPut == false) return response405(pReq, pRes);
	if(pReq->nContentsLength < 0) return response400(pReq, pRes);

	// generate abs path
	char szFilePath[PATH_MAX];
	httpRequestGetSysPath(pReq, szFilePath, sizeof(szFilePath), pReq->pszRequestPath);

	// open file for writing
	int nFd = sysOpen(szFilePath, O_WRONLY|O_CREAT|O_TRUNC, DEF_FILE_MODE);
	if(nFd < 0) {
		return response403(pReq, pRes); // forbidden - can't open file
	}

	// receive file
	int nResCode = httpRealPut(pReq, pRes, nFd);

	// close file
	sysClose(nFd);

	// response
	bool bKeepAlive = false;
	if(nResCode == HTTP_CODE_CREATED) bKeepAlive = true;
	httpResponseSetSimple(pReq, pRes, nResCode, bKeepAlive, httpResponseGetMsg(nResCode));
	return nResCode;
}

/*
 * Only return supposed response status, does not generate response message
 */
int httpRealPut(struct HttpRequest *pReq, struct HttpResponse *pRes, int nFd) {
	// header check
	if(httpHeaderHasStr(pReq->pHeaders, "EXPECT", "100-CONTINUE") == true) {
		streamPrintf(pReq->nSockFd, "%s %d %s\r\n\r\n", pReq->pszHttpVersion, HTTP_CODE_CONTINUE, httpResponseGetMsg(HTTP_CODE_CONTINUE));
	}

	// save
	if(pReq->nContentsLength > 0) {
		off_t nSaved = streamSave(nFd, pReq->nSockFd, pReq->nContentsLength, pReq->nTimeout*1000);

		if(nSaved != pReq->nContentsLength) {
			LOG_INFO("Broken pipe. %jd/%jd, errno=%d", nSaved, pReq->nContentsLength, errno);
			return HTTP_CODE_BAD_REQUEST;
		}

		DEBUG("File %s saved. (%jd/%jd)", pReq->pszRequestPath, nSaved, pReq->nContentsLength);

	} else if(httpHeaderHasStr(pReq->pHeaders, "TRANSFER-ENCODING", "CHUNKED") == true) {
		off_t nSaved = 0;
		bool bCompleted = false;
		while(true) {
			// read chunk size
			char szLineBuf[64];
			if(streamGets(szLineBuf, sizeof(szLineBuf), pReq->nSockFd, pReq->nTimeout * 1000) <= 0) break;

			// parse chunk size
			int nChunkSize = 0;
			sscanf(szLineBuf, "%x", &nChunkSize);
			if(nChunkSize == 0) {
				// end of transfer
				bCompleted = true;
				break;
			} else if(nChunkSize < 0) {
				// parsing failure
				break;
			}

			// save chunk
			off_t nChunkSaved = streamSave(nFd, pReq->nSockFd, (size_t)nChunkSize, pReq->nTimeout*1000);
			if(nChunkSaved != nChunkSize) break;

			nSaved += nChunkSaved;

			// read tailing CRLF
			if(streamGets(szLineBuf, sizeof(szLineBuf), pReq->nSockFd, pReq->nTimeout * 1000) <= 0) break;
		}

		if(bCompleted == false) {
			LOG_INFO("Broken pipe. %jd/chunked, errno=%d", nSaved, errno);
			return HTTP_CODE_BAD_REQUEST;
		}

		DEBUG("File %s saved. (%jd/chunked)", pReq->pszRequestPath, nSaved);
	} else {
		return HTTP_CODE_BAD_REQUEST;
	}

	// response
	return HTTP_CODE_CREATED;
}

/*
 * method not implemented
 */
int httpMethodNotImplemented(struct HttpRequest *pReq, struct HttpResponse *pRes) {
	return response501(pReq, pRes);
}
