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
#define MAX_SENDFILE_CHUNK_SIZE		(1024 * 1024)
int httpMethodGet(struct HttpRequest *req, struct HttpResponse *res) {

	// 특수 URI 체크
	if(g_conf.bStatusEnable == true) {
		if(!strcmp(req->pszRequestUrl, g_conf.szStatusUrl)) {
			Q_OBSTACK *obHtml = httpGetStatusHtml();
			if(obHtml == NULL) return response500(req, res);

			httpResponseSetCode(res, HTTP_RESCODE_OK, req, true);
			httpResponseSetContent(res, "text/html; charset=\"utf-8\"", qObstackGetSize(obHtml), (char *)qObstackFinish(obHtml));
			qObstackFree(obHtml);

			return HTTP_RESCODE_OK;
		}
	}

	// 파일 전송
	char szFilePath[SYSPATH_SIZE];
	snprintf(szFilePath, sizeof(szFilePath), "%s%s", g_conf.szDataDir, req->pszRequestUrl);
	return httpProcessGetNormalFile(req, res, szFilePath, "application/octet-stream");
}

int httpProcessGetNormalFile(struct HttpRequest *req, struct HttpResponse *res, char *pszFilePath, char *pszContentType) {
	struct stat filestat;
	int nFileFd = -1;
	uint64_t nFilesize = 0;

	uint64_t nRangeOffset1, nRangeOffset2, nRangeSize;
	bool bRangeRequest = false;

	//
	// 파일 오픈 및 정보 확인
	//

	// 파일 오픈
	nFileFd = open(pszFilePath, O_RDONLY|O_LARGEFILE , 0);
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

	//
	// 헤더 처리 부분
	//

	// If-Modified-Since 헤더
	char *pszIfModifiedSince = httpHeaderGetValue(req->pHeaders, "IF-MODIFIED-SINCE");
	if(pszIfModifiedSince != NULL) {
		time_t nUnivTime = qParseGmtimeStr(pszIfModifiedSince);
		if(nUnivTime >= 0 && nUnivTime > filestat.st_mtime) { // 해석 성공 && 파일이 변경이 없음
			close(nFileFd);
			return response304(req, res); // Not modified
		}
	}

	// Range 헤더
	char *pszRange = httpHeaderGetValue(req->pHeaders, "RANGE");
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
	httpResponseSetHeaderf(res, "Last-Modified", "%s", qGetGmtimeStr(filestat.st_mtime));
	//httpResponseSetHeaderf(res, "ETag", "\"%s\"", );

	if(bRangeRequest == true) {
		httpResponseSetHeaderf(res, "Content-Range", "bytes %ju-%ju/%ju", nRangeOffset1, nRangeOffset2, nFilesize);
	}

	if(g_conf.nResponseExpires > 0) { // enable client caching
		httpResponseSetHeaderf(res, "Cache-Control", "max-age=%d", g_conf.nResponseExpires);
		httpResponseSetHeaderf(res, "Expires", "%s", qGetGmtimeStr(time(NULL)+g_conf.nResponseExpires));
	}

	httpResponseOut(res, req->nSockFd);

	//
	// 파일 전송
	//

	if(nFilesize > 0) {
		off_t nOffset = (off_t)nRangeOffset1;	// 시작 오프셋
		uint64_t nSent = 0;		// 보낸 총 사이즈

		while(nSent < nRangeSize) {
			size_t nSendSize;	// 한번에 보낼 사이즈
			if(nRangeSize - nSent > MAX_SENDFILE_CHUNK_SIZE) nSendSize = MAX_SENDFILE_CHUNK_SIZE;
			else nSendSize = (size_t)(nRangeSize - nSent);

			ssize_t nRet = sendfile(req->nSockFd, nFileFd, &nOffset, nSendSize);
			if(nRet <= 0) {
				LOG_INFO("Connection closed by peer. (errno:%d)", errno);
				break;
			}

			nSent += nRet;
			DEBUG("[TX] (file %s, req range %ju-%ju, sent range %ju-%ju, sent bytes %ju/%ju)"
				, pszFilePath
				, nRangeOffset1, nRangeOffset2
				, (uint64_t)(nOffset-nRet), (uint64_t)(nOffset-1)
				, nSent, nRangeSize);
		}
		close(nFileFd);
	}

	return HTTP_RESCODE_OK;
}

/*
 * http method - HEAD
 */
int httpMethodHead(struct HttpRequest *req, struct HttpResponse *res) {
	struct stat filestat;
	char szFilePath[SYSPATH_SIZE];

	// 파일 경로
	snprintf(szFilePath, sizeof(szFilePath), "%s%s", g_conf.szDataDir, req->pszRequestUrl);

	// 파일 정보
	if (stat(szFilePath, &filestat) < 0) return response404(req, res);

	// @todo
	httpResponseSetCode(res, HTTP_RESCODE_OK, req, true);

	httpResponseSetHeader(res, "Accept-Ranges", "bytes");
	httpResponseSetHeaderf(res, "Last-Modified", "%s", qGetGmtimeStr(filestat.st_mtime));
	//httpResponseSetHeaderf(res, "ETag", "\"%s\"", );

	return HTTP_RESCODE_OK;
}

/*
 * method not implemented
 */
int httpMethodNotImplemented(struct HttpRequest *req, struct HttpResponse *res) {
	return response501(req, res);
}
