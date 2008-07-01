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
 * http method - OPTIONS
 */
int httpMethodOptions(struct HttpRequest *req, struct HttpResponse *res) {
	httpResponseSetCode(res, HTTP_RESCODE_OK, req, true);
	httpResponseSetHeader(res, "Allow", "OPTIONS,GET,HEAD");
	httpResponseSetContent(res, "httpd/unix-directory", 0, NULL);

	return HTTP_RESCODE_OK;
}

/*
 * http method - GET
 */
int httpMethodGet(struct HttpRequest *req, struct HttpResponse *res) {
	char szFilePath[MAX_PATH_LEN];
	snprintf(szFilePath, sizeof(szFilePath), "%s%s", g_conf.szDataDir, req->pszRequestUrl);
	return httpProcessGetNormalFile(req, res, szFilePath, mimeDetect(szFilePath));
}

int httpProcessGetNormalFile(struct HttpRequest *req, struct HttpResponse *res, const char *pszFilePath, const char *pszContentType) {
	struct stat filestat;
	int nFileFd = -1;
	size_t nFilesize = 0;

	off_t nRangeOffset1, nRangeOffset2;
	size_t nRangeSize;
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
	const char *pszIfModifiedSince = httpHeaderGetValue(req->pHeaders, "IF-MODIFIED-SINCE");
	if(pszIfModifiedSince != NULL) {
		time_t nUnivTime = qTimeParseGmtStr(pszIfModifiedSince);
		if(nUnivTime >= 0 && nUnivTime > filestat.st_mtime) { // succeed to parsing header && file does not modified
			return response304(req, res); // Not modified
		}
	}

	// check Range header
	const char *pszRange = httpHeaderGetValue(req->pHeaders, "RANGE");
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
	httpResponseSetContent(res, pszContentType, nRangeSize, NULL);

	httpResponseSetHeader(res, "Accept-Ranges", "bytes");
	httpResponseSetHeaderf(res, "Last-Modified", "%s", qTimeGetGmtStaticStr(filestat.st_mtime));
	//httpResponseSetHeaderf(res, "ETag", "\"%s\"", );

	if(bRangeRequest == true) {
		httpResponseSetHeaderf(res, "Content-Range", "bytes %zu-%zu/%zu", (size_t)nRangeOffset1, (size_t)nRangeOffset2, nFilesize);
	}

	if(g_conf.nResponseExpires > 0) { // enable client caching
		httpResponseSetHeaderf(res, "Cache-Control", "max-age=%d", g_conf.nResponseExpires);
		httpResponseSetHeaderf(res, "Expires", "%s", qTimeGetGmtStaticStr(time(NULL)+g_conf.nResponseExpires));
	}

	httpResponseOut(res, req->nSockFd);

	//
	// send file
	//

	if(nFilesize > 0) {
		ssize_t nSent = streamSendfile(req->nSockFd, pszFilePath, nRangeOffset1, nRangeSize);
		if(nSent != nRangeSize) {
			LOG_INFO("Connection closed by foreign host. (%zd/%zu)", nSent, nRangeSize);
		}
	}

	return HTTP_RESCODE_OK;
}

/*
 * http method - HEAD
 */
int httpMethodHead(struct HttpRequest *req, struct HttpResponse *res) {
	struct stat filestat;
	char szFilePath[MAX_PATH_LEN];

	// file path
	snprintf(szFilePath, sizeof(szFilePath), "%s%s", g_conf.szDataDir, req->pszRequestUrl);

	// file info
	if (stat(szFilePath, &filestat) < 0) return response404(req, res);

	// print out response
	httpResponseSetCode(res, HTTP_RESCODE_OK, req, true);

	httpResponseSetHeader(res, "Accept-Ranges", "bytes");
	httpResponseSetHeaderf(res, "Last-Modified", "%s", qTimeGetGmtStaticStr(filestat.st_mtime));
	//httpResponseSetHeaderf(res, "ETag", "\"%s\"", );

	return HTTP_RESCODE_OK;
}

/*
 * method not implemented
 */
int httpMethodNotImplemented(struct HttpRequest *req, struct HttpResponse *res) {
	return response501(req, res);
}
