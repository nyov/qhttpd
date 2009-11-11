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

static int _httpProcessGetNormalFile(struct HttpRequest *req, struct HttpResponse *res, const char *pszFilePath, const char *pszContentType);

/*
 * http method - OPTIONS
 */
int httpMethodOptions(struct HttpRequest *req, struct HttpResponse *res) {
	if(g_conf.methods.bOptions == false) return response403(req, res);

	httpResponseSetCode(res, HTTP_RESCODE_OK, req, true);
	httpHeaderSetStr(res->pHeaders, "Allow", "OPTIONS,GET,HEAD");
	httpResponseSetContent(res, "httpd/unix-directory", NULL, 0);

	return HTTP_RESCODE_OK;
}

/*
 * http method - HEAD
 */
int httpMethodHead(struct HttpRequest *req, struct HttpResponse *res) {
	if(g_conf.methods.bHead == false) return response403(req, res);

	struct stat filestat;
	char szFilePath[PATH_MAX];

	// file path
	snprintf(szFilePath, sizeof(szFilePath), "%s%s", g_conf.szDataDir, req->pszRequestPath);

	// file info
	if (stat(szFilePath, &filestat) < 0) return response404(req, res);

	// print out response
	httpResponseSetCode(res, HTTP_RESCODE_OK, req, true);

	httpHeaderSetStr(res->pHeaders, "Accept-Ranges", "bytes");
	httpHeaderSetStrf(res->pHeaders, "Last-Modified", "%s", qTimeGetGmtStaticStr(filestat.st_mtime));
	//httpResponseSetHeaderf(res, "ETag", "\"%s\"", );

	return HTTP_RESCODE_OK;
}

/*
 * http method - GET
 */
int httpMethodGet(struct HttpRequest *req, struct HttpResponse *res) {
	if(g_conf.methods.bGet == false) return response403(req, res);

	char szFilePath[PATH_MAX];
	snprintf(szFilePath, sizeof(szFilePath), "%s%s", g_conf.szDataDir, req->pszRequestPath);
	return _httpProcessGetNormalFile(req, res, szFilePath, mimeDetect(szFilePath));
}

static int _httpProcessGetNormalFile(struct HttpRequest *req, struct HttpResponse *res, const char *pszFilePath, const char *pszContentType) {
	struct stat filestat;
	int nFileFd = -1;
	off_t nFilesize = 0;

	off_t nRangeOffset1, nRangeOffset2;
	off_t nRangeSize;
	bool bRangeRequest = false;

	//
	// file open section
	//

	// open file
	nFileFd = open(pszFilePath, O_RDONLY , 0);
	if(nFileFd < 0) {
		LOG_INFO("File open failed. %s", pszFilePath);
		return response404(req, res);
	}

	// get info
	if (fstat(nFileFd, &filestat) < 0) {
		LOG_INFO("File stat failed. %s", pszFilePath);
		return response404(req, res);
	}

	// get size
	nFilesize = (uint64_t)filestat.st_size;

	// is normal file
	if(S_ISREG(filestat.st_mode) == 0) {
		close(nFileFd);
		return response403(req, res);
	}

	// close file
	close(nFileFd);

	//
	// header handling section
	//

	// check If-Modified-Since header
	const char *pszIfModifiedSince = httpHeaderGetStr(req->pHeaders, "IF-MODIFIED-SINCE");
	if(pszIfModifiedSince != NULL) {
		time_t nUnivTime = qTimeParseGmtStr(pszIfModifiedSince);
		if(nUnivTime >= 0 && nUnivTime > filestat.st_mtime) { // succeed to parsing header && file does not modified
			return response304(req, res); // Not modified
		}
	}

	// check Range header
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

	httpResponseSetCode(res, HTTP_RESCODE_OK, req, true);
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
		int nFd = open(pszFilePath, O_RDONLY , 0);
		if(nFd >= 0) {
			off_t nSent = streamSendfile(req->nSockFd, nFd, nRangeOffset1, nRangeSize);
			close(nFd);
			if(nSent != nRangeSize) {
				LOG_INFO("Connection closed by foreign host. (%jd/%jd/%jd)", nSent, nRangeOffset1, nRangeSize);
			}
		}
	}

	return HTTP_RESCODE_OK;
}

/*
 * http method - PUT
 */
int httpMethodPut(struct HttpRequest *req, struct HttpResponse *res) {
	if(g_conf.methods.bPut == false) return response403(req, res);

	return response501(req, res);
}

/*
 * method not implemented
 */
int httpMethodNotImplemented(struct HttpRequest *req, struct HttpResponse *res) {
	return response501(req, res);
}
