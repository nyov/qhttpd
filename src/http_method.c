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
int httpMethodOptions(struct HttpRequest *req, struct HttpResponse *res) {
	if(g_conf.methods.bOptions == false) return response403(req, res);

	httpResponseSetCode(res, HTTP_CODE_OK, req, true);
	httpHeaderSetStr(res->pHeaders, "Allow", g_conf.szAllowedMethods);
	httpResponseSetContent(res, "httpd/unix-directory", NULL, 0);

	return HTTP_CODE_OK;
}

/*
 * http method - HEAD
 */
int httpMethodHead(struct HttpRequest *req, struct HttpResponse *res) {
	if(g_conf.methods.bHead == false) return response403(req, res);

	// file path
	char szFilePath[PATH_MAX];
	snprintf(szFilePath, sizeof(szFilePath), "%s%s", g_conf.szDataDir, req->pszRequestPath);

	// file info
	struct stat filestat;
	if (stat(szFilePath, &filestat) < 0) return response404(req, res);

	// print out response
	httpResponseSetCode(res, HTTP_CODE_OK, req, true);

	httpHeaderSetStr(res->pHeaders, "Accept-Ranges", "bytes");
	httpHeaderSetStrf(res->pHeaders, "Last-Modified", "%s", qTimeGetGmtStaticStr(filestat.st_mtime));
	//httpResponseSetHeaderf(res, "ETag", "\"%s\"", );

	return HTTP_CODE_OK;
}

/*
 * http method - GET
 */
int httpMethodGet(struct HttpRequest *req, struct HttpResponse *res) {
	if(g_conf.methods.bGet == false) return response403(req, res);

	//
	// file open section
	//

	// generate abs path
	char szFilePath[PATH_MAX];
	snprintf(szFilePath, sizeof(szFilePath), "%s%s", g_conf.szDataDir, req->pszRequestPath);

	// open file
	int nFd = open(szFilePath, O_RDONLY , 0);
	if(nFd < 0) {
		LOG_INFO("File open failed. %s", szFilePath);
		return response404(req, res);
	}

	// send file
	int nResCode = httpRealGet(req, res, nFd, mimeDetect(szFilePath));

	// close file
	close(nFd);

	// response
	if(nResCode != HTTP_CODE_OK) {
		httpResponseSetSimple(req, res, nResCode, true, httpResponseGetMsg(nResCode));
	}

	return nResCode;
}

/*
 * returns supposed response status except HTTP_CODE_OK.
 */
int httpRealGet(struct HttpRequest *req, struct HttpResponse *res, int nFd, const char *pszContentType) {

	// get info
	struct stat filestat;
	if (fstat(nFd, &filestat) < 0) {
		LOG_INFO("File %s stat failed.", req->pszRequestPath);
		return HTTP_CODE_NOT_FOUND;
	}

	// get size
	off_t nFilesize = (uint64_t)filestat.st_size;

	// is normal file
	if(S_ISREG(filestat.st_mode) == 0) {
		return HTTP_CODE_FORBIDDEN;
	}

	//
	// header handling section
	//

	// check If-Modified-Since header
	const char *pszIfModifiedSince = httpHeaderGetStr(req->pHeaders, "IF-MODIFIED-SINCE");
	if(pszIfModifiedSince != NULL) {
		time_t nUnivTime = qTimeParseGmtStr(pszIfModifiedSince);
		if(nUnivTime >= 0 && nUnivTime > filestat.st_mtime) { // succeed to parsing header && file does not modified
			return HTTP_CODE_NOT_MODIFIED;
		}
	}

	// check Range header
	off_t nRangeOffset1, nRangeOffset2, nRangeSize;
	bool bRangeRequest = false;
	const char *pszRange = httpHeaderGetStr(req->pHeaders, "RANGE");
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
	// print out response headers
	//

	httpResponseSetCode(res, HTTP_CODE_OK, req, true);
	httpResponseSetContent(res, pszContentType, NULL, nRangeSize);

	httpHeaderSetStr(res->pHeaders, "Accept-Ranges", "bytes");
	httpHeaderSetStrf(res->pHeaders, "Last-Modified", "%s", qTimeGetGmtStaticStr(filestat.st_mtime));
	//httpHeaderSetStrf(res->pHeaders, "ETag", "\"%s\"", );

	if(bRangeRequest == true) {
		httpHeaderSetStrf(res->pHeaders, "Content-Range", "bytes %jd-%jd/%jd", nRangeOffset1, nRangeOffset2, nFilesize);
	}

	if(g_conf.nResponseExpires > 0) { // enable client caching
		httpHeaderSetStrf(res->pHeaders, "Cache-Control", "max-age=%d", g_conf.nResponseExpires);
		httpHeaderSetStrf(res->pHeaders, "Expires", "%s", qTimeGetGmtStaticStr(time(NULL)+g_conf.nResponseExpires));
	}

	httpResponseOut(res, req->nSockFd);

	//
	// send file
	//
	if(nFilesize > 0) {
		if(nRangeOffset1 > 0) {
			lseek(nFd, nRangeOffset1, SEEK_SET);
		}

		off_t nSent = streamSend(req->nSockFd, nFd, nRangeSize);
		if(nSent != nRangeSize) {
			LOG_INFO("Connection closed by foreign host. (%s/%jd/%jd/%jd)", req->pszRequestPath, nSent, nRangeOffset1, nRangeSize);
		}
	}

	return HTTP_CODE_OK;
}

/*
 * http method - PUT
 */
int httpMethodPut(struct HttpRequest *req, struct HttpResponse *res) {
	if(g_conf.methods.bPut == false) return response403(req, res);
	if(req->nContentsLength < 0) return response405(req, res);

	// generate abs path
	char szFilePath[PATH_MAX];
	snprintf(szFilePath, sizeof(szFilePath), "%s%s", g_conf.szDataDir, req->pszRequestPath);

	// open file for writing
	int nFd = open(szFilePath, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if(nFd < 0) {
		return response403(req, res); // forbidden - can't open file
	}

	// receive file
	int nResCode = httpRealPut(req, res, nFd);

	// close file
	close(nFd);

	// response
	bool bKeepAlive = false;
	if(nResCode == HTTP_CODE_CREATED) bKeepAlive = true;
	httpResponseSetSimple(req, res, nResCode, bKeepAlive, httpResponseGetMsg(nResCode));
	return nResCode;
}

/*
 * Only return supposed response status, does not generate response message
 */
int httpRealPut(struct HttpRequest *req, struct HttpResponse *res, int nFd) {
	// header check
	if(httpHeaderHasStr(req->pHeaders, "EXPECT", "100-CONTINUE") == true) {
		streamPrintf(req->nSockFd, "%s %d %s\r\n", req->pszHttpVersion, HTTP_CODE_CONTINUE, httpResponseGetMsg(HTTP_CODE_CONTINUE));
		streamPrintf(req->nSockFd, "\r\n");
	}

	// save
	off_t nSaved = streamSave(nFd, req->nSockFd, req->nContentsLength, req->nTimeout*1000);

	if(nSaved != req->nContentsLength) {
		LOG_INFO("Broken pipe. %jd/%jd, errno=%d", nSaved, req->nContentsLength, errno);
		return HTTP_CODE_BAD_REQUEST;
	}
	DEBUG("File %s saved. (%jd/%jd)", req->pszRequestPath, nSaved, req->nContentsLength);

	// response
	return HTTP_CODE_CREATED;
}

/*
 * extended HTTP method - DELETE
 */
int httpMethodDelete(struct HttpRequest *req, struct HttpResponse *res) {
	if(g_conf.methods.bDelete == false) return response403(req, res);

	// file path
	char szFilePath[PATH_MAX];
	snprintf(szFilePath, sizeof(szFilePath), "%s%s", g_conf.szDataDir, req->pszRequestPath);

	// file info
	struct stat filestat;
	if (stat(szFilePath, &filestat) < 0) return response404(req, res);

	// remove
	if(S_ISDIR(filestat.st_mode)) {
		if(rmdir(szFilePath) != 0) {
			return response403(req, res);
		}
	} else {
		if(unlink(szFilePath) != 0) {
			return response403(req, res);
		}
	}

	return response204(req, res); // no contents
}

/*
 * method not implemented
 */
int httpMethodNotImplemented(struct HttpRequest *req, struct HttpResponse *res) {
	return response501(req, res);
}
