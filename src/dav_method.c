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
 * webdav method - OPTIONS
 */

static bool _addXmlResponseStart(Q_OBSTACK *obXml);
static bool _addXmlResponseFile(Q_OBSTACK *obXml, struct EgisFileInfo *pFileInfo);
static bool _addXmlResponseFileHead(Q_OBSTACK *obXml, struct EgisFileInfo *pFileInfo);
static bool _addXmlResponseFileInfo(Q_OBSTACK *obXml, struct EgisFileInfo *pFileInfo);
static bool _addXmlResponseFileProp(Q_OBSTACK *obXml, struct EgisFileInfo *pFileInfo);
static bool _addXmlResponseFileTail(Q_OBSTACK *obXml);
static bool _addXmlResponseEnd(Q_OBSTACK *obXml);
static char *_getXmlEntry(char *pszXml, char *pszEntryName);

static bool _getFileUrl(char *pUri, int nSize, struct EgisFileLoc *pFileLoc, char *pszRequestPath);
static bool _addParams(char *pUri, int nSize, int nServiceId, const char *pszRequestPath, Q_ENTRY *pHeaders, Q_ENTRY *pQueries, int nAuth);


/*
 * dav method - PROPFIND
 */
#define	CHUNK_SIZE			(10 * 1024)
int davMethodPropfind(struct HttpRequest *req, struct HttpResponse *res) {
	if(g_conf.methods.bPropfind == false) return response405(req, res);

	// generate abs path
	char szFilePath[PATH_MAX];
	snprintf(szFilePath, sizeof(szFilePath), "%s%s", g_conf.szDataDir, req->pszRequestPath);
	szFilePath[sizeof(szFilePath) - 1] = '\0';

	// get file stat
	struct stat filestat;
	if(stat(szFilePath, &filestat) < 0) {
		return response404(req, res);
	}

	Q_OBSTACK *obDirList = NULL;

	//
	// 해당 URL에 대한 파일(폴더) 정보 요청
	//
	int nDepth = httpHeaderGetInt(req->pHeaders, "DEPTH");
	if(nDepth == 1) {
		// if the file is not a directory file.
		if(!S_ISDIR(filestat.st_mode)) {
			return response404(req,res);
		}
	} else if(nDepth > 1) {
		// not supported
		return response400(req, res);
	}

	//
	// generate XML response
	//

	// set response as chunked
	httpResponseSetCode(res, HTTP_CODE_MULTI_STATUS, req, true);
	httpResponseSetContent(res, "text/xml; charset=\"utf-8\"", 0, NULL);
	httpResponseSetContentChunked(res, true); // set transfer-encoding to chunked
	httpResponseOut(res, req->nSockFd);

	// init xml buffer
	Q_OBSTACK *obXml = qObstack();
	_addXmlResponseStart(obXml);

	// locate requested file info at the beginning
	_addXmlResponseFile(obXml, &fileInfo);

	// append sub files if requested
	if(nDepth > 0) {
		int i, nNum = qObstackGetNum(obDirList);
		struct EgisFileInfo *pDirList = (struct EgisFileInfo *)qObstackGetFinal(obDirList);
		for(i = 0; i < nNum; i++) {
			if(qObstackGetSize(obXml) >= CHUNK_SIZE) { // chunk 사이즈보다 크면 flush 시킴
				httpResponseOutChunk(req->nSockFd, (char *)qObstackFinish(obXml), qObstackGetSize(obXml));
				qObstackFree(obXml);
				obXml = qObstackInit();
			}

			_addXmlResponseFile(obXml, &pDirList[i]);
		}
	}

	// 3) 마무리
	_addXmlResponseEnd(obXml);
	httpResponseOutChunk(req->nSockFd, (char *)qObstackFinish(obXml), qObstackGetSize(obXml));
	qObstackFree(obXml);

	httpResponseOutChunk(req->nSockFd, NULL, 0);	// 더 이상 자료가 없음

	//
	// 뒷정리
	//
	qObstackFree(obDirList);

	return HTTP_MULTI_STATUS;
}

/*
 * dav method - PROPPATCH
 */
int davMethodProppatch(struct EgisRequest *ereq, struct HttpRequest *req, struct HttpResponse *res) {
	// 권한 체크
	if(aclGetPriv(ereq->nServiceId, poolGetConnNaddr()) < PRIV_WRITER) {
		return response403(req, res);
	}

	struct EgisFileInfo fileInfo;

	//
	// 해당 URL에 대한 파일(폴더) 정보 획득
	//
	bool bFound = nsGetFileInfo(g_db, ereq->nServiceId, ereq->pszRequestPath, &fileInfo);
	if(bFound == false) { // 파일이나 폴더가 없음
		return response404(req,res);
	}

	//
	// XML 응답 생성
	//
	Q_OBSTACK *obXml = qObstackInit();
	_addXmlResponseStart(obXml);
	_addXmlResponseFileHead(obXml, &fileInfo);
	_addXmlResponseFileProp(obXml, &fileInfo);
	_addXmlResponseFileTail(obXml);
	_addXmlResponseEnd(obXml);

	// HTTP 응답 설정
	httpResponseSetCode(res, HTTP_MULTI_STATUS, req, true);
	httpResponseSetContent(res, "text/xml; charset=\"utf-8\"", qObstackGetSize(obXml), (char *)qObstackFinish(obXml));

	// 뒷정리
	qObstackFree(obXml);

	return HTTP_MULTI_STATUS;
}

/*
 * dav method - MKCOL
 */
int davMethodMkcol(struct EgisRequest *ereq, struct HttpRequest *req, struct HttpResponse *res) {
	// 권한 체크
	if(aclGetPriv(ereq->nServiceId, poolGetConnNaddr()) < PRIV_WRITER) {
		return response403(req, res);
	}

	// 폴더 생성
	if(nsMkdir(g_db, ereq->nServiceId, ereq->pszRequestPath) == false) {
		return response403(req,res);
	}

	// 응답 설정
	return response201(req, res);
}

/*
 * dav method - MOVE
 */
int davMethodMove(struct EgisRequest *ereq, struct HttpRequest *req, struct HttpResponse *res) {
	// 권한 체크
	if(aclGetPriv(ereq->nServiceId, poolGetConnNaddr()) < PRIV_ADMIN) {
		return response403(req, res);
	}

	// fetch destination header
	const char *pszDestUri = httpHeaderGetStr(req->pHeaders, "DESTINATION");
	if(pszDestUri == NULL) return response400(req, res);

	// decode url encoded uri
	char *pszDestUriDec = strdup(pszDestUri);
	qDecodeUrl(pszDestUriDec);

	// parse destination header
	char *pszDestPath = pszDestUriDec;
	if(!strncasecmp(pszDestUriDec, "HTTP://", 7)) {
		pszDestPath = strstr(pszDestUriDec + 8, "/");
		if(pszDestUri == NULL) return response400(req, res);
	} else if(pszDestPath[0] != '/') {
		return response400(req, res);
	}

	// divide uri to service id & url
	char *pszDestUrl;
	int  nServiceId;

	char *pszTmp = strstr(pszDestPath + 1, "/");
	if(pszTmp == NULL) {
	        return response500(req, res); // 서비스ID가 없음
	} else {
	        *pszTmp = '\0';
	        nServiceId = atoi(pszDestPath + 1);
	        *pszTmp = '/';

	        if(nServiceId == 0 || nServiceId != ereq->nServiceId) return response500(req, res);
	        pszDestUrl = pszTmp;
	}

	// file check
	struct EgisFileInfo fileInfo;
	if(nsGetFileInfo(g_db, ereq->nServiceId, ereq->pszRequestPath, &fileInfo) == false) {
		return response404nc(req, res);
	}

	// move
	if(nsMove(g_db, &fileInfo, pszDestUrl) == false) {
		free(pszDestUriDec);
		return response500(req, res);
	}

	free(pszDestUriDec);
	return response201(req, res);
}

/*
 * dav method - DELETE
 */
int davMethodDelete(struct EgisRequest *ereq, struct HttpRequest *req, struct HttpResponse *res) {
	// 권한 체크
	if(aclGetPriv(ereq->nServiceId, poolGetConnNaddr()) < PRIV_ADMIN) {
		return response403(req, res);
	}

	struct EgisFileInfo fileInfo;

	if(nsGetFileInfo(g_db, ereq->nServiceId, ereq->pszRequestPath, &fileInfo) == false) return response404(req, res);

	if(fileInfo.bDirectory == true) { // 폴더 삭제
		if(nsRmdir(g_db, &fileInfo) == false) {
			return response403(req, res); // 폴더에 파일이 남아 있음
		}
	} else { // 파일 삭제
		if(nsUnlink(g_db, &fileInfo, (g_set->nExtAct==ACT_ACTIVE)?true:false) == false) {
			return response500(req, res); // internal server error
		}
	}

	return response204(req, res); // no contents - 삭제 성공
}

/*
 * dav method - LOCK
 */
int davMethodLock(struct EgisRequest *ereq, struct HttpRequest *req, struct HttpResponse *res) {
	// 권한 체크
	if(aclGetPriv(ereq->nServiceId, poolGetConnNaddr()) < PRIV_WRITER) {
		return response403(req, res);
	}

	if(req->nContentsLength <= 0) return response400(req,res); // bad request

	// Request의 필요 아규먼트 파싱
	char *pszLocktype, *pszLockscope, *pszOwner, *pszTimeout;
	pszLocktype = _getXmlEntry(req->pContents, "locktype");
	pszLockscope = _getXmlEntry(req->pContents, "lockscope");
	pszOwner = _getXmlEntry(req->pContents, "owner");
	pszTimeout = _getXmlEntry(req->pContents, "timeout");

	// 토큰 생성
	char szToken[32+1];
	char *pszUnique = qStrUnique(NULL);
	qStrCpy(szToken, sizeof(szToken), pszUnique, sizeof(szToken));
	free(pszUnique);

	Q_OBSTACK *obXml = qObstackInit();
	qObstackGrowStr(obXml, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n");
	qObstackGrowStr(obXml, "<D:prop xmlns:D=\"DAV:\">\r\n");
	qObstackGrowStr(obXml, "  <D:lockdiscovery>\r\n");
	qObstackGrowStr(obXml, "    <D:activelock>\r\n");

	if(pszLocktype != NULL)
	qObstackGrowStrf(obXml,"      <D:locktype><D:%s/></D:locktype>\r\n", pszLocktype);
	if(pszLockscope != NULL)
	qObstackGrowStrf(obXml,"      <D:lockscope><D:%s/></D:lockscope>\r\n", pszLockscope);
	if(pszOwner != NULL)
	qObstackGrowStrf(obXml,"      <D:owner>%s</D:owner>\r\n", pszOwner);
	if(pszTimeout != NULL)
	qObstackGrowStrf(obXml,"      <D:timeout>%s</D:timeout>\r\n", pszTimeout);

	qObstackGrowStr(obXml, "      <D:depth>0</D:depth>\r\n");
	qObstackGrowStrf(obXml,"      <D:locktoken><D:href>opaquelocktoken:%s</D:href></D:locktoken>\r\n", szToken);
	qObstackGrowStr(obXml, "    </D:activelock>\r\n");
	qObstackGrowStr(obXml, "  </D:lockdiscovery>\r\n");
	qObstackGrowStr(obXml, "</D:prop>\r\n");

	// 응답 설정
	httpResponseSetCode(res, HTTP_RESCODE_OK, req, true);
	httpHeaderSetStrf(res->pHeaders, "Lock-Token", "<opaquelocktoken:%s>", szToken);
	httpResponseSetContent(res, "text/xml; charset=\"utf-8\"", qObstackGetSize(obXml), (char *)qObstackFinish(obXml));

	// 뒷정리
	qObstackFree(obXml);
	if(pszLocktype != NULL) free(pszLocktype);
	if(pszLockscope != NULL) free(pszLockscope);
	if(pszOwner != NULL) free(pszOwner);
	if(pszTimeout != NULL) free(pszTimeout);

	return HTTP_RESCODE_OK;
}

/*
 * dav method - UNLOCK
 */
int davMethodUnlock(struct EgisRequest *ereq, struct HttpRequest *req, struct HttpResponse *res) {
	// 권한 체크
	if(aclGetPriv(ereq->nServiceId, poolGetConnNaddr()) < PRIV_WRITER) {
		return response403(req, res);
	}

        return response204(req, res);
}

//
// internal static functions
//

static bool _addXmlResponseStart(Q_OBSTACK *obXml) {
	qObstackGrowStr(obXml, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n");
	qObstackGrowStr(obXml, "<D:multistatus xmlns:D=\"DAV:\">\r\n");

	return true;
}

static bool _addXmlResponseFile(Q_OBSTACK *obXml, struct EgisFileInfo *pFileInfo) {
	_addXmlResponseFileHead(obXml, pFileInfo);
	_addXmlResponseFileInfo(obXml, pFileInfo);
	_addXmlResponseFileTail(obXml);
	return true;
}

static bool _addXmlResponseFileHead(Q_OBSTACK *obXml, struct EgisFileInfo *pFileInfo) {
	char *pszEncFullpath = qEncodeUrl(pFileInfo->szFullpath);

	char *pszTailSlash = "";
	if(pFileInfo->bDirectory == true && strcmp(pFileInfo->szFullpath, "/")) pszTailSlash = "/";

	qObstackGrowStr(obXml, "  <D:response xmlns:ns0=\"DAV:\" xmlns:ns1=\"urn:schemas-qhttpd:\" xmlns:ns2=\"urn:schemas-microsoft-com:\">\r\n");
	qObstackGrowStrf(obXml,"    <D:href>/%d%s%s</D:href>\r\n", pFileInfo->nServiceId, pszEncFullpath, pszTailSlash);
	qObstackGrowStr(obXml, "    <D:propstat>\r\n");
	qObstackGrowStr(obXml, "      <D:prop>\r\n");

	free(pszEncFullpath);
	return true;
}

static bool _addXmlResponseFileInfo(Q_OBSTACK *obXml, struct EgisFileInfo *pFileInfo) {
	// resource type
	char *pszResourceType = "";
	if(pFileInfo->bDirectory == true) pszResourceType = "<D:collection/>";

	// the creationdate property specifies the use of the ISO 8601 date format [ISO-8601].
	char szCreationDate[sizeof(char) * (CONST_STRLEN("YYYY-MM-DDThh:mm:ssZ") + 1)];
	qTimeGetGmtStrf(szCreationDate, sizeof(szCreationDate), pFileInfo->nCreated, "%Y-%m-%dT%H:%M:%SZ");

	// contenttype
	char *pszContentType = "";
	if(pFileInfo->bDirectory == true) pszContentType = "httpd/unix-directory";
	else pszContentType = (char*)mimeDetect(pFileInfo->szFilename);

	// out
	qObstackGrowStrf(obXml,"        <ns0:resourcetype>%s</ns0:resourcetype>\r\n", pszResourceType);

	qObstackGrowStrf(obXml,"        <ns0:getcontentlength>%jd</ns0:getcontentlength>\r\n", pFileInfo->nFilesize);
	qObstackGrowStrf(obXml,"        <ns0:creationdate>%s</ns0:creationdate>\r\n", szCreationDate);
	qObstackGrowStrf(obXml,"        <ns0:getlastmodified>%s</ns0:getlastmodified>\r\n", qTimeGetGmtStaticStr(pFileInfo->nCreated));
	qObstackGrowStrf(obXml,"        <ns0:getetag>\"%s\"</ns0:getetag>\r\n", pFileInfo->szEtag);

	qObstackGrowStrf(obXml,"        <ns1:egisUniqueId>%ju</ns1:egisUniqueId>\r\n", pFileInfo->nFileId);
	qObstackGrowStrf(obXml,"        <ns1:egisFilesize>%jd</ns1:egisFilesize>\r\n", pFileInfo->nFilesize);
	qObstackGrowStrf(obXml,"        <ns1:egisCreated>%s</ns1:egisCreated>\r\n", qTimeGetGmtStaticStr(pFileInfo->nCreated));
	qObstackGrowStrf(obXml,"        <ns1:egisLastused>%s</ns1:egisLastused>\r\n", qTimeGetGmtStaticStr(pFileInfo->nLastused));
	qObstackGrowStrf(obXml,"        <ns1:egisChecksum>%s</ns1:egisChecksum>\r\n", pFileInfo->szChecksum);
	qObstackGrowStrf(obXml,"        <ns1:egisReplica>%d</ns1:egisReplica>\r\n", pFileInfo->nReplica);

	qObstackGrowStr(obXml, "        <D:supportedlock>\r\n");
	qObstackGrowStr(obXml, "          <D:lockentry>\r\n");
	qObstackGrowStr(obXml, "            <D:lockscope><D:exclusive/></D:lockscope>\r\n");
	qObstackGrowStr(obXml, "            <D:locktype><D:write/></D:locktype>\r\n");
	qObstackGrowStr(obXml, "          </D:lockentry>\r\n");
	qObstackGrowStr(obXml, "          <D:lockentry>\r\n");
	qObstackGrowStr(obXml, "            <D:lockscope><D:shared/></D:lockscope>\r\n");
	qObstackGrowStr(obXml, "            <D:locktype><D:write/></D:locktype>\r\n");
	qObstackGrowStr(obXml, "          </D:lockentry>\r\n");
	qObstackGrowStr(obXml, "        </D:supportedlock>\r\n");

	qObstackGrowStr(obXml, "        <D:lockdiscovery></D:lockdiscovery>\r\n");
	qObstackGrowStrf(obXml,"        <D:getcontenttype>%s</D:getcontenttype>\r\n", pszContentType);

	return true;
}

static bool _addXmlResponseFileProp(Q_OBSTACK *obXml, struct EgisFileInfo *pFileInfo) {
	//qObstackGrowStr(obXml, "      <ns2:Win32LastModifiedTime/>\r\n");
	//qObstackGrowStr(obXml, "      <ns2:Win32FileAttributes/>\r\n");
	qObstackGrowStrf(obXml,"        <ns2:Win32CreationTime>%s</ns2:Win32CreationTime>\r\n", qTimeGetGmtStaticStr(pFileInfo->nCreated));
	qObstackGrowStrf(obXml,"        <ns2:Win32LastAccessTime>%s</ns2:Win32LastAccessTime>\r\n", qTimeGetGmtStaticStr(pFileInfo->nLastused));
	qObstackGrowStrf(obXml,"        <ns2:Win32LastModifiedTime>%s</ns2:Win32LastModifiedTime>\r\n", qTimeGetGmtStaticStr(pFileInfo->nCreated));
	qObstackGrowStr(obXml,"         <ns2:Win32FileAttributes>00000020</ns2:Win32FileAttributes>\r\n");

	return true;
}

static bool _addXmlResponseFileTail(Q_OBSTACK *obXml) {
	qObstackGrowStr(obXml, "      </D:prop>\r\n");
	qObstackGrowStrf(obXml,"      <D:status>%s %d %s</D:status>\r\n", HTTP_PROTOCOL_11, HTTP_RESCODE_OK, httpResponseGetMsg(HTTP_RESCODE_OK));
	qObstackGrowStr(obXml, "    </D:propstat>\r\n");
	qObstackGrowStr(obXml, "  </D:response>\r\n");

	return true;
}

static bool _addXmlResponseEnd(Q_OBSTACK *obXml) {
	qObstackGrowStr(obXml, "</D:multistatus>\r\n");
	return true;
}

static char *_getXmlEntry(char *pszXml, char *pszEntryName) {
	char *pszTmp;

	char *pszEntryStart = (char *)malloc(strlen(pszEntryName) + 1 + 1);
	sprintf(pszEntryStart, "%s>", pszEntryName);
	pszTmp = qStrDupBetween(pszXml, pszEntryStart, "</");
	if(pszTmp != NULL) {
		qStrReplace("tr", pszTmp, "<>/", "");
		qStrTrim(pszTmp);
	}

	free(pszEntryStart);
	return pszTmp;
}

static bool _getFileUrl(char *pUri, int nSize, struct EgisFileLoc *pFileLoc, char *pszRequestPath) {
	//
	// 기본 URI 경로
	//
	if(pFileLoc->cNodeType == 'P') {
		if(g_conf.nEgisdavdSelfServiceType == 1) {	// process directly
			// 다른 P Node인 경우 - 자기 자신인 경우는 여기까지 오기전에 직접 서비스 됨
			char *pszEncUrl = qEncodeUrl(pszRequestPath);
			snprintf(pUri, nSize, "%s://%s:%d%s", g_conf.szEgisdavdServiceProtocol, pFileLoc->szAddr, g_conf.nEgisdavdPort, pszEncUrl);
			free(pszEncUrl);
			DEBUG("Redirect to P node : %s", pUri);
		} else if(g_conf.nEgisdavdSelfServiceType == 2) {	// redirect to self service port
			// save 경로로
			sprintf(pUri, "%s://%s:%d%s", g_conf.szEgisdavdServiceProtocol, pFileLoc->szAddr, g_conf.nEgisdavdSelfServicePort, pFileLoc->szPath);
		} else if(g_conf.nEgisdavdSelfServiceType == 3) {	// redirect to proxy
			// nodeif+save 경로 - ${ServiceProtocol}://${SelfProxyUri}/${NodeId}/${SAVE_PATH}
			sprintf(pUri, "%s://%s/%d%s", g_conf.szEgisdavdServiceProtocol, g_conf.szEgisdavdSelfProxyUri, g_conf.nEgisNodeId, pFileLoc->szPath);
		} else {
			LOG_ERR("Not supported service type %d.", g_conf.nEgisdavdSelfServiceType);
			return false;
		}
	} else {
		// save 경로로
		sprintf(pUri, "%s://%s:%d%s", g_conf.szEgisdavdServiceProtocol, pFileLoc->szAddr, g_conf.nEgisdavdServicePort, pFileLoc->szPath);
	}

	return true;
}

static bool _addParams(char *pUri, int nSize, int nServiceId, const char *pszRequestPath, Q_ENTRY *pHeaders, Q_ENTRY *pQueries, int nAuth) {
	int nAdded = 0;

	// 인증 처리
	if(nAuth != AUTH_DISABLED) {
		char szAuthKey[1024];
		if(authGen(AUTH_AQUA, szAuthKey, sizeof(szAuthKey), pUri, poolGetConnAddr(), g_conf.nEgisdavdAuthKeyExpire) == true) {
			(nAdded == 0) ? strcat(pUri, "?key=") : strcat(pUri, "&key=");
			strcat(pUri, szAuthKey);
			nAdded++;
		}
	}

	// metahint 처리
	if(g_conf.bEgisdavdRedirectWithMetahint == true && pszRequestPath != NULL) {
		// 브라우저 랭귀지 파싱
		char *pszConvUrl = NULL;

		const char *pszAcceptLang = httpHeaderGetStr(pHeaders, "ACCEPT-LANGUAGE");
		if(pszAcceptLang != NULL) {
			if(!strncasecmp(pszAcceptLang, "KO", 2)) {
				pszConvUrl = qStrConvEncoding(pszRequestPath, "UTF-8", "EUC-KR", 1);
			}
		}

		// 경로에 추가
		char *pszEncUrl;
		if(pszConvUrl == NULL) {
			 pszEncUrl = qEncodeUrl(pszRequestPath);
		} else {
			pszEncUrl = qEncodeUrl(pszConvUrl);
			free(pszConvUrl);
		}

		(nAdded == 0) ? strcat(pUri, "?_metahint_fn=") : strcat(pUri, "&_metahint_fn=");
		if(nServiceId > 0) {
			char szPrefix[10+1];
			snprintf(szPrefix, sizeof(szPrefix), "/%d", nServiceId);
			strcat(pUri, szPrefix);
		}
		strcat(pUri, pszEncUrl);
		free(pszEncUrl);
		nAdded++;
	}

	// Query 파라미터 추가
	const Q_NLOBJ *pObj;
	for (pObj = qEntryFirst(pQueries); pObj; pObj = qEntryNext(pQueries)) {
		// 인증이 켜져있을 경우엔 'key=' 는 덧붙이지 않음
		if(nAuth != AUTH_DISABLED && !strcmp(pObj->name, "key")) continue;

		// do not URL-encode the value for the key 'key='.
		char *pszEncValue = (!strcmp(pObj->name, "key")) ? strdup((char*)pObj->object) : qEncodeUrl((char*)pObj->object);
		(nAdded == 0) ? strcat(pUri, "?") : strcat(pUri, "&");
		strcat(pUri, pObj->name);
		strcat(pUri, "=");
		strcat(pUri, pszEncValue);
		free(pszEncValue);

		nAdded++;
	}

	return true;
}
