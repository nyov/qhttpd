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

static bool _addXmlResponseStart(Q_OBSTACK *obXml);
static bool _addXmlResponseFile(Q_OBSTACK *obXml, const char *pszUriPath, struct stat *pFileStat);
static bool _addXmlResponseFileHead(Q_OBSTACK *obXml, const char *pszUriPath, struct stat *pFileStat);
static bool _addXmlResponseFileInfo(Q_OBSTACK *obXml, const char *pszUriPath, struct stat *pFileStat);
static bool _addXmlResponseFileProp(Q_OBSTACK *obXml, struct stat *pFileStat);
static bool _addXmlResponseFileTail(Q_OBSTACK *obXml);
static bool _addXmlResponseEnd(Q_OBSTACK *obXml);
static char *_getXmlEntry(char *pszXml, char *pszEntryName);

/*
 * WebDAV method - PROPFIND
 */
#define	CHUNK_SIZE			(10 * 1024)
int httpMethodPropfind(struct HttpRequest *pReq, struct HttpResponse *pRes) {
	if(g_conf.methods.bPropfind == false) return response405(pReq, pRes);

	// generate abs path
	char szFilePath[PATH_MAX];
	httpRequestGetSysPath(pReq, szFilePath, sizeof(szFilePath), pReq->pszRequestPath);

	// get file stat
	struct stat filestat;
	if(stat(szFilePath, &filestat) < 0) {
		return response404(pReq, pRes);
	}

	// parse DEPTH header
	int nDepth = httpHeaderGetInt(pReq->pHeaders, "DEPTH");
	if(nDepth == 1) {
		// if the file is not a directory file.
		if(!S_ISDIR(filestat.st_mode)) {
			return response404(pReq, pRes);
		}
	} else if(nDepth > 1) {
		// not supported
		return response400(pReq, pRes);
	}

	//
	// generate XML response
	//

	// set response as chunked
	httpResponseSetCode(pRes, HTTP_CODE_MULTI_STATUS, pReq, true);
	httpResponseSetContent(pRes, "text/xml; charset=\"utf-8\"", NULL, 0);
	httpResponseSetContentChunked(pRes, true); // set transfer-encoding to chunked
	httpResponseOut(pRes, pReq->nSockFd);

	// init xml buffer
	Q_OBSTACK *obXml = qObstack();
	_addXmlResponseStart(obXml);

	// locate requested file info at the beginning
	_addXmlResponseFile(obXml, pReq->pszRequestPath, &filestat);

	// append sub files if requested
	if(nDepth > 0) {
		DIR *pDir = opendir(szFilePath);
		if(pDir != NULL) {
			struct dirent *pFile;
			while((pFile = readdir(pDir)) != NULL) {
				if(!strcmp(pFile->d_name, ".") || !strcmp(pFile->d_name, "..")) continue;

				// flush buffer if buffered xml is large than max chunk size
				size_t nChunkSize = obXml->getSize(obXml);
				if(nChunkSize >= CHUNK_SIZE) {
					void *pChunk = obXml->getFinal(obXml, NULL);
					httpResponseOutChunk(pReq->nSockFd, pChunk, nChunkSize);
					free(pChunk);
					obXml->free(obXml);
					obXml = qObstack();
				}

				// generate request path
				char szSubRequestPath[PATH_MAX];
				snprintf(szSubRequestPath, sizeof(szSubRequestPath), "%s/%s", (strcmp(pReq->pszRequestPath, "/"))?pReq->pszRequestPath:"", pFile->d_name);
				szSubRequestPath[sizeof(szSubRequestPath) - 1] = '\0';

				// generate abs path
				char szSubFilePath[PATH_MAX];
				httpRequestGetSysPath(pReq, szSubFilePath, sizeof(szSubFilePath), szSubRequestPath);

				// get file stat
				struct stat filestat;
				if(stat(szSubFilePath, &filestat) < 0) continue;

				_addXmlResponseFile(obXml, szSubRequestPath, &filestat);
			}

			closedir(pDir);
		}
	}

	// close XML
	_addXmlResponseEnd(obXml);

	// flush buffer	if necessary
	size_t nChunkSize = obXml->getSize(obXml);
	if(nChunkSize > 0) {
		void *pChunk = obXml->getFinal(obXml, NULL);
		httpResponseOutChunk(pReq->nSockFd, pChunk, nChunkSize);
		free(pChunk);
	}
	obXml->free(obXml);

	// end of chunk
	httpResponseOutChunk(pReq->nSockFd, NULL, 0);

	return HTTP_CODE_MULTI_STATUS;
}

/*
 * WebDAV method - PROPPATCH
 */
int httpMethodProppatch(struct HttpRequest *pReq, struct HttpResponse *pRes) {
	if(g_conf.methods.bProppatch == false) return response405(pReq, pRes);

	// generate abs path
	char szFilePath[PATH_MAX];
	httpRequestGetSysPath(pReq, szFilePath, sizeof(szFilePath), pReq->pszRequestPath);

	// get file stat
	struct stat filestat;
	if(stat(szFilePath, &filestat) < 0) {
		return response404(pReq, pRes);
	}

	//
	// create XML response
	//
	Q_OBSTACK *obXml = qObstack();
	_addXmlResponseStart(obXml);
	_addXmlResponseFileHead(obXml, pReq->pszRequestPath, &filestat);
	_addXmlResponseFileProp(obXml, &filestat);
	_addXmlResponseFileTail(obXml);
	_addXmlResponseEnd(obXml);

	// set response
	size_t nXmlSize;
	char *pszXmlData = (char*)obXml->getFinal(obXml, &nXmlSize);

	httpResponseSetCode(pRes, HTTP_CODE_MULTI_STATUS, pReq, true);
	httpResponseSetContent(pRes, "text/xml; charset=\"utf-8\"", pszXmlData, nXmlSize);

	// free
	free(pszXmlData);
	obXml->free(obXml);

	return HTTP_CODE_MULTI_STATUS;
}

/*
 * WebDAV method - MKCOL
 */
int httpMethodMkcol(struct HttpRequest *pReq, struct HttpResponse *pRes) {
	if(g_conf.methods.bMkcol == false) return response405(pReq, pRes);

	// generate abs path
	char szFilePath[PATH_MAX];
	httpRequestGetSysPath(pReq, szFilePath, sizeof(szFilePath), pReq->pszRequestPath);

	// try to create directory
	if(sysMkdir(szFilePath, DEF_DIR_MODE) != 0) {
		return response403(pReq, pRes);
	}

	// success
	return response201(pReq, pRes);
}

/*
 * WebDAV method - MOVE
 */
int httpMethodMove(struct HttpRequest *pReq, struct HttpResponse *pRes) {
	if(g_conf.methods.bMove == false) return response405(pReq, pRes);

	// fetch destination header
	const char *pszDestination = httpHeaderGetStr(pReq->pHeaders, "DESTINATION");
	if(pszDestination == NULL) return response400(pReq, pRes);

	// decode url encoded uri
	char *pszTmp = strdup(pszDestination);
	qDecodeUrl(pszTmp);
	char szDestUri[PATH_MAX];
	qStrCpy(szDestUri, sizeof(szDestUri), pszTmp);
	free(pszTmp);

	// parse destination header
	char *pszDestPath = szDestUri;
	if(!strncasecmp(szDestUri, "HTTP://", CONST_STRLEN("HTTP://"))) {
		pszDestPath = strstr(szDestUri + CONST_STRLEN("HTTP://"), "/");
		if(pszDestPath == NULL) return response400(pReq, pRes);
	} else if(pszDestPath[0] != '/') {
		return response400(pReq, pRes);
	}

	// generate system path
	char szOldPath[PATH_MAX], szNewPath[PATH_MAX];
	httpRequestGetSysPath(pReq, szOldPath, sizeof(szOldPath), pReq->pszRequestPath);
	httpRequestGetSysPath(pReq, szNewPath, sizeof(szNewPath), pszDestPath);

	// move
	if(sysRename(szOldPath, szNewPath) != 0) {
		return response500(pReq, pRes);
	}

	return response201(pReq, pRes);
}

/*
 * WebDAV method - DELETE
 */
int httpMethodDelete(struct HttpRequest *pReq, struct HttpResponse *pRes) {
	if(g_conf.methods.bDelete == false) return response405(pReq, pRes);

	// generate abs path
	char szFilePath[PATH_MAX];
	httpRequestGetSysPath(pReq, szFilePath, sizeof(szFilePath), pReq->pszRequestPath);

	// file info
	struct stat filestat;
	if (sysStat(szFilePath, &filestat) < 0) {
		return response404(pReq, pRes);
	}

	// remove
	if(S_ISDIR(filestat.st_mode)) {
		if(sysRmdir(szFilePath) != 0) {
			return response403(pReq, pRes);
		}
	} else {
		if(sysUnlink(szFilePath) != 0) {
			return response403(pReq, pRes);
		}
	}

	return response204(pReq, pRes); // no contents
}

/*
 * WebDAV method - LOCK
 */
int httpMethodLock(struct HttpRequest *pReq, struct HttpResponse *pRes) {
	if(g_conf.methods.bLock == false) return response405(pReq, pRes);

	if(pReq->nContentsLength <= 0) return response400(pReq, pRes); // bad request

	// parse request XML
	char *pszLocktype = _getXmlEntry(pReq->pContents, "locktype");
	char *pszLockscope = _getXmlEntry(pReq->pContents, "lockscope");
	char *pszOwner = _getXmlEntry(pReq->pContents, "owner");
	char *pszTimeout = _getXmlEntry(pReq->pContents, "timeout");

	// create unique token
	char szToken[32+1];
	char *pszUnique = qStrUnique(NULL);
	qStrCpy(szToken, sizeof(szToken), pszUnique);
	free(pszUnique);

	Q_OBSTACK *obXml = qObstack();
	obXml->growStr(obXml, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n");
	obXml->growStr(obXml, "<D:prop xmlns:D=\"DAV:\">\r\n");
	obXml->growStr(obXml, "  <D:lockdiscovery>\r\n");
	obXml->growStr(obXml, "    <D:activelock>\r\n");

	if(pszLocktype != NULL)
	obXml->growStrf(obXml,"      <D:locktype><D:%s/></D:locktype>\r\n", pszLocktype);
	if(pszLockscope != NULL)
	obXml->growStrf(obXml,"      <D:lockscope><D:%s/></D:lockscope>\r\n", pszLockscope);
	if(pszOwner != NULL)
	obXml->growStrf(obXml,"      <D:owner>%s</D:owner>\r\n", pszOwner);
	if(pszTimeout != NULL)
	obXml->growStrf(obXml,"      <D:timeout>%s</D:timeout>\r\n", pszTimeout);

	obXml->growStr(obXml, "      <D:depth>0</D:depth>\r\n");
	obXml->growStrf(obXml,"      <D:locktoken><D:href>opaquelocktoken:%s</D:href></D:locktoken>\r\n", szToken);
	obXml->growStr(obXml, "    </D:activelock>\r\n");
	obXml->growStr(obXml, "  </D:lockdiscovery>\r\n");
	obXml->growStr(obXml, "</D:prop>\r\n");

	// set response
	size_t nXmlSize;
	char *pszXmlData = (char*)obXml->getFinal(obXml, &nXmlSize);

	httpResponseSetCode(pRes, HTTP_CODE_OK, pReq, true);
	httpHeaderSetStrf(pRes->pHeaders, "Lock-Token", "<opaquelocktoken:%s>", szToken);
	httpResponseSetContent(pRes, "text/xml; charset=\"utf-8\"", pszXmlData, nXmlSize);
	free(pszXmlData);

	// free
	obXml->free(obXml);
	if(pszLocktype != NULL) free(pszLocktype);
	if(pszLockscope != NULL) free(pszLockscope);
	if(pszOwner != NULL) free(pszOwner);
	if(pszTimeout != NULL) free(pszTimeout);

	return HTTP_CODE_OK;
}

/*
 * WebDAV method - UNLOCK
 */
int httpMethodUnlock(struct HttpRequest *pReq, struct HttpResponse *pRes) {
	if(g_conf.methods.bUnlock == false) return response405(pReq, pRes);

        return response204(pReq, pRes);
}

//
// internal static functions
//

static bool _addXmlResponseStart(Q_OBSTACK *obXml) {
	obXml->growStr(obXml, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n");
	obXml->growStr(obXml, "<D:multistatus xmlns:D=\"DAV:\">\r\n");

	return true;
}

static bool _addXmlResponseFile(Q_OBSTACK *obXml, const char *pszUriPath, struct stat *pFileStat) {
	_addXmlResponseFileHead(obXml, pszUriPath, pFileStat);
	_addXmlResponseFileInfo(obXml, pszUriPath, pFileStat);
	_addXmlResponseFileTail(obXml);
	return true;
}

static bool _addXmlResponseFileHead(Q_OBSTACK *obXml, const char *pszUriPath, struct stat *pFileStat) {
	char *pszEncPath = qEncodeUrl(pszUriPath);

	char *pszTailSlash = "";
	if(S_ISDIR(pFileStat->st_mode) && strcmp(pszUriPath, "/")) pszTailSlash = "/";

	obXml->growStr(obXml, "  <D:response xmlns:ns0=\"DAV:\" xmlns:ns1=\"urn:schemas-microsoft-com:\">\r\n");
	obXml->growStrf(obXml,"    <D:href>%s%s</D:href>\r\n", pszEncPath, pszTailSlash);
	obXml->growStr(obXml, "    <D:propstat>\r\n");
	obXml->growStr(obXml, "      <D:prop>\r\n");

	free(pszEncPath);
	return true;
}

static bool _addXmlResponseFileInfo(Q_OBSTACK *obXml, const char *pszUriPath, struct stat *pFileStat) {
	// resource type
	const char *pszResourceType;
	const char *pszContentType;
	if(S_ISDIR(pFileStat->st_mode)) {
		pszResourceType = "<D:collection/>";
		pszContentType = "httpd/unix-directory";
	} else {
		pszResourceType = "";
		pszContentType = mimeDetect(pszUriPath);
	}

	// the creationdate property specifies the use of the ISO 8601 date format [ISO-8601].
	char szLastModified[sizeof(char) * (CONST_STRLEN("YYYY-MM-DDThh:mm:ssZ") + 1)];
	qTimeGetGmtStrf(szLastModified, sizeof(szLastModified), pFileStat->st_mtime, "%Y-%m-%dT%H:%M:%SZ");

	// etag
	char szEtag[ETAG_MAX];
	getEtag(szEtag, sizeof(szEtag), pszUriPath, pFileStat);

	// out
	obXml->growStrf(obXml,"        <ns0:resourcetype>%s</ns0:resourcetype>\r\n", pszResourceType);

	obXml->growStrf(obXml,"        <ns0:getcontentlength>%jd</ns0:getcontentlength>\r\n", pFileStat->st_size);
	obXml->growStrf(obXml,"        <ns0:creationdate>%s</ns0:creationdate>\r\n", szLastModified);
	obXml->growStrf(obXml,"        <ns0:getlastmodified>%s</ns0:getlastmodified>\r\n", qTimeGetGmtStaticStr(pFileStat->st_mtime));
	obXml->growStrf(obXml,"        <ns0:getetag>\"%s\"</ns0:getetag>\r\n", szEtag);

	obXml->growStr(obXml, "        <D:supportedlock>\r\n");
	obXml->growStr(obXml, "          <D:lockentry>\r\n");
	obXml->growStr(obXml, "            <D:lockscope><D:exclusive/></D:lockscope>\r\n");
	obXml->growStr(obXml, "            <D:locktype><D:write/></D:locktype>\r\n");
	obXml->growStr(obXml, "          </D:lockentry>\r\n");
	obXml->growStr(obXml, "          <D:lockentry>\r\n");
	obXml->growStr(obXml, "            <D:lockscope><D:shared/></D:lockscope>\r\n");
	obXml->growStr(obXml, "            <D:locktype><D:write/></D:locktype>\r\n");
	obXml->growStr(obXml, "          </D:lockentry>\r\n");
	obXml->growStr(obXml, "        </D:supportedlock>\r\n");

	obXml->growStr(obXml, "        <D:lockdiscovery></D:lockdiscovery>\r\n");
	obXml->growStrf(obXml,"        <D:getcontenttype>%s</D:getcontenttype>\r\n", pszContentType);

	return true;
}

static bool _addXmlResponseFileProp(Q_OBSTACK *obXml, struct stat *pFileStat) {
	//obXml->growStr(obXml, "      <ns1:Win32LastModifiedTime/>\r\n");
	//obXml->growStr(obXml, "      <ns1:Win32FileAttributes/>\r\n");
	obXml->growStrf(obXml,"        <ns1:Win32CreationTime>%s</ns1:Win32CreationTime>\r\n", qTimeGetGmtStaticStr(pFileStat->st_mtime));
	obXml->growStrf(obXml,"        <ns1:Win32LastAccessTime>%s</ns1:Win32LastAccessTime>\r\n", qTimeGetGmtStaticStr(pFileStat->st_atime));
	obXml->growStrf(obXml,"        <ns1:Win32LastModifiedTime>%s</ns1:Win32LastModifiedTime>\r\n", qTimeGetGmtStaticStr(pFileStat->st_mtime));
	obXml->growStr(obXml,"         <ns1:Win32FileAttributes>00000020</ns1:Win32FileAttributes>\r\n");

	return true;
}

static bool _addXmlResponseFileTail(Q_OBSTACK *obXml) {
	obXml->growStr(obXml, "      </D:prop>\r\n");
	obXml->growStrf(obXml,"      <D:status>%s %d %s</D:status>\r\n", HTTP_PROTOCOL_11, HTTP_CODE_OK, httpResponseGetMsg(HTTP_CODE_OK));
	obXml->growStr(obXml, "    </D:propstat>\r\n");
	obXml->growStr(obXml, "  </D:response>\r\n");

	return true;
}

static bool _addXmlResponseEnd(Q_OBSTACK *obXml) {
	obXml->growStr(obXml, "</D:multistatus>\r\n");
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
