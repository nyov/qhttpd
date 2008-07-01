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
	// 파일 전송
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
	// 파일 오픈 및 정보 확인
	//

	// 파일 오픈
	nFileFd = open(pszFilePath, O_RDONLY , 0);
	if(nFileFd < 0) {
		LOG_INFO("File open failed. %s", pszFilePath);
		return response404(req, res);
	}

	// 파일 정보
	if (fstat(nFileFd, &filestat) < 0) {
		LOG_INFO("File stat failed. %s", pszFilePath);
		return response404(req, res);
	}

	// 파일 사이즈
	nFilesize = (uint64_t)filestat.st_size;

	// 일반 파일인지 체크
	if(S_ISREG(filestat.st_mode) == 0) {
		close(nFileFd);
		return response403(req, res);
	}

	// 파일 닫기
	close(nFileFd);

	//
	// 헤더 처리 부분
	//

	// If-Modified-Since 헤더
	const char *pszIfModifiedSince = httpHeaderGetValue(req->pHeaders, "IF-MODIFIED-SINCE");
	if(pszIfModifiedSince != NULL) {
		time_t nUnivTime = qTimeParseGmtStr(pszIfModifiedSince);
		if(nUnivTime >= 0 && nUnivTime > filestat.st_mtime) { // 해석 성공 && 파일이 변경이 없음
			return response304(req, res); // Not modified
		}
	}

	// Range 헤더
	const char *pszRange = httpHeaderGetValue(req->pHeaders, "RANGE");
	if(pszRange != NULL) {
		bRangeRequest = httpHeaderParseRange(pszRange, nFilesize, &nRangeOffset1, &nRangeOffset2, &nRangeSize);
	}

	// Range가 없거나, 파싱 오류시
	if(bRangeRequest == false) {
		nRangeOffset1 = 0;
		nRangeOffset2 = nFilesize - 1;
		nRangeSize = nFilesize;
	}

	//
	// 응답 헤더 출력
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
	// 파일 전송
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

	// 파일 경로
	snprintf(szFilePath, sizeof(szFilePath), "%s%s", g_conf.szDataDir, req->pszRequestUrl);

	// 파일 정보
	if (stat(szFilePath, &filestat) < 0) return response404(req, res);

	// @todo
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
