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

static bool _addXmlResponseStart(Q_VECTOR *obXml);
static bool _addXmlResponseFile(Q_VECTOR *obXml, const char *pszUriPath, struct stat *pFileStat);
static bool _addXmlResponseFileHead(Q_VECTOR *obXml, const char *pszUriPath, struct stat *pFileStat);
static bool _addXmlResponseFileInfo(Q_VECTOR *obXml, const char *pszUriPath, struct stat *pFileStat);
static bool _addXmlResponseFileProp(Q_VECTOR *obXml, struct stat *pFileStat);
static bool _addXmlResponseFileTail(Q_VECTOR *obXml);
static bool _addXmlResponseEnd(Q_VECTOR *obXml);
static char *_getXmlEntry(char *pszXml, char *pszEntryName);

/*
 * WebDAV method - PROPFIND
 */
#define	FLUSH_CHUNK_IF_EXCEED_SIZE	(10 * 1024)
int httpMethodPropfind(struct HttpRequest *pReq, struct HttpResponse *pRes) {
	if(g_conf.methods.bPropfind == false) return response405(pRes);

	// generate abs path
	char szFilePath[PATH_MAX];
	httpRequestGetSysPath(pReq, szFilePath, sizeof(szFilePath), pReq->pszRequestPath);

	// get file stat
	struct stat filestat;
	if(sysStat(szFilePath, &filestat) < 0) {
		return response404(pRes);
	}

	// parse DEPTH header
	int nDepth = httpHeaderGetInt(pReq->pHeaders, "DEPTH");
	if(nDepth == 1) {
		// if the file is not a directory file.
		if(!S_ISDIR(filestat.st_mode)) {
			return response404(pRes);
		}
	} else if(nDepth > 1) {
		// not supported
		DEBUG("Depth(%d) more than 1 is not supported.", nDepth);
		return response501(pRes);
	}

	//
	// generate XML response
	//

	// set response as chunked
	httpResponseSetCode(pRes, HTTP_CODE_MULTI_STATUS, true);
	httpResponseSetContent(pRes, "text/xml; charset=\"utf-8\"", NULL, 0);
	httpResponseSetContentChunked(pRes, true); // set transfer-encoding to chunked
	httpResponseOut(pRes, pReq->nSockFd);

	// init xml buffer
	Q_VECTOR *obXml = qVector();
	_addXmlResponseStart(obXml);

	// locate requested file info at the beginning
	_addXmlResponseFile(obXml, pReq->pszRequestPath, &filestat);

	// append sub files if requested
	if(nDepth > 0) {
		DIR *pDir = sysOpendir(szFilePath);
		if(pDir != NULL) {
			struct dirent *pFile;
			while((pFile = sysReaddir(pDir)) != NULL) {
				if(!strcmp(pFile->d_name, ".") || !strcmp(pFile->d_name, "..")) continue;

				// flush buffer if buffered xml is large than max chunk size
				if(obXml->list->datasize(obXml->list) >= FLUSH_CHUNK_IF_EXCEED_SIZE) {
					size_t nChunkSize;
					void *pChunk = obXml->toArray(obXml, &nChunkSize);
					bool bRet = httpResponseOutChunk(pReq->nSockFd, pChunk, nChunkSize);
					DEBUG("[TX-CHUNK] %s", (char*) pChunk);
					free(pChunk);
					obXml->free(obXml);
					obXml = qVector();
					if(bRet == false) break;
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
				if(sysStat(szSubFilePath, &filestat) < 0) continue;

				_addXmlResponseFile(obXml, szSubRequestPath, &filestat);
			}

			sysClosedir(pDir);
		}
	}

	// close XML
	_addXmlResponseEnd(obXml);

	// flush buffer	if necessary
	if(obXml->size(obXml) > 0) {
		size_t nChunkSize;
		void *pChunk = obXml->toArray(obXml, &nChunkSize);
		httpResponseOutChunk(pReq->nSockFd, pChunk, nChunkSize);
		DEBUG("[TX-CHUNK] %s", (char*) pChunk);
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
	if(g_conf.methods.bProppatch == false) return response405(pRes);

	// generate abs path
	char szFilePath[PATH_MAX];
	httpRequestGetSysPath(pReq, szFilePath, sizeof(szFilePath), pReq->pszRequestPath);

	// get file stat
	struct stat filestat;
	if(sysStat(szFilePath, &filestat) < 0) {
		return response404(pRes);
	}

	//
	// create XML response
	//
	Q_VECTOR *obXml = qVector();
	_addXmlResponseStart(obXml);
	_addXmlResponseFileHead(obXml, pReq->pszRequestPath, &filestat);
	_addXmlResponseFileProp(obXml, &filestat);
	_addXmlResponseFileTail(obXml);
	_addXmlResponseEnd(obXml);

	// set response
	size_t nXmlSize;
	char *pszXmlData = (char*)obXml->toArray(obXml, &nXmlSize);

	httpResponseSetCode(pRes, HTTP_CODE_MULTI_STATUS, true);
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
	if(g_conf.methods.bMkcol == false) return response405(pRes);

	// generate abs path
	char szFilePath[PATH_MAX];
	httpRequestGetSysPath(pReq, szFilePath, sizeof(szFilePath), pReq->pszRequestPath);

	// check file or directory is already exists
	struct stat filestat;
	if (sysStat(szFilePath, &filestat) == 0) {
		// exists
		return response403(pRes);
	}

	// try to create directory
	if(sysMkdir(szFilePath, DEF_DIR_MODE) != 0) {
		return response500(pRes);
	}

	// success
	return response201(pRes);
}

/*
 * WebDAV method - MOVE
 */
int httpMethodMove(struct HttpRequest *pReq, struct HttpResponse *pRes) {
	if(g_conf.methods.bMove == false) return response405(pRes);

	// fetch destination header
	const char *pszDestination = httpHeaderGetStr(pReq->pHeaders, "DESTINATION");
	if(pszDestination == NULL) return response400(pRes);

	// decode url encoded uri
	char *pszTmp = strdup(pszDestination);
	qUrlDecode(pszTmp);
	char pszDecDestination[PATH_MAX];
	qStrCpy(pszDecDestination, sizeof(pszDecDestination), pszTmp);
	free(pszTmp);

	// parse destination header
	char *pszDestPath;
	if(pszDecDestination[0] == '/') {
		pszDestPath = pszDecDestination;
	} else if(!strncasecmp(pszDecDestination, "HTTP://", CONST_STRLEN("HTTP://"))) {
		pszDestPath = strstr(pszDecDestination + CONST_STRLEN("HTTP://"), "/");
		if(pszDestPath == NULL) return response400(pRes);
	} else {
		return response400(pRes);
	}

	// generate system path
	char szOldPath[PATH_MAX], szNewPath[PATH_MAX];
	httpRequestGetSysPath(pReq, szOldPath, sizeof(szOldPath), pReq->pszRequestPath);
	httpRequestGetSysPath(pReq, szNewPath, sizeof(szNewPath), pszDestPath);

	// move
	if(sysRename(szOldPath, szNewPath) != 0) {
		return response500(pRes);
	}

	return response201(pRes);
}

/*
 * WebDAV method - DELETE
 */
int httpMethodDelete(struct HttpRequest *pReq, struct HttpResponse *pRes) {
	if(g_conf.methods.bDelete == false) return response405(pRes);

	// generate abs path
	char szFilePath[PATH_MAX];
	httpRequestGetSysPath(pReq, szFilePath, sizeof(szFilePath), pReq->pszRequestPath);

	// file info
	struct stat filestat;
	if (sysStat(szFilePath, &filestat) < 0) {
		return response404(pRes);
	}

	// remove
	if(S_ISDIR(filestat.st_mode)) {
		if(sysRmdir(szFilePath) != 0) {
			return response403(pRes);
		}
	} else {
		if(sysUnlink(szFilePath) != 0) {
			return response403(pRes);
		}
	}

	return response204(pRes); // no contents
}

/*
 * WebDAV method - LOCK
 */
int httpMethodLock(struct HttpRequest *pReq, struct HttpResponse *pRes) {
	if(g_conf.methods.bLock == false) return response405(pRes);

	if(pReq->nContentsLength <= 0) return response400(pRes); // bad request

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

	Q_VECTOR *obXml = qVector();
	obXml->addStr(obXml, "<?xml version=\"1.0\" encoding=\"utf-8\"?>" CRLF);
	obXml->addStr(obXml, "<D:prop xmlns:D=\"DAV:\">" CRLF);
	obXml->addStr(obXml, "  <D:lockdiscovery>" CRLF);
	obXml->addStr(obXml, "    <D:activelock>" CRLF);

	if(pszLocktype != NULL)
	obXml->addStrf(obXml,"      <D:locktype><D:%s/></D:locktype>" CRLF, pszLocktype);
	if(pszLockscope != NULL)
	obXml->addStrf(obXml,"      <D:lockscope><D:%s/></D:lockscope>" CRLF, pszLockscope);
	if(pszOwner != NULL)
	obXml->addStrf(obXml,"      <D:owner>%s</D:owner>" CRLF, pszOwner);
	if(pszTimeout != NULL)
	obXml->addStrf(obXml,"      <D:timeout>%s</D:timeout>" CRLF, pszTimeout);

	obXml->addStr(obXml, "      <D:depth>0</D:depth>" CRLF);
	obXml->addStrf(obXml,"      <D:locktoken><D:href>opaquelocktoken:%s</D:href></D:locktoken>" CRLF, szToken);
	obXml->addStr(obXml, "    </D:activelock>" CRLF);
	obXml->addStr(obXml, "  </D:lockdiscovery>" CRLF);
	obXml->addStr(obXml, "</D:prop>" CRLF);

	// set response
	size_t nXmlSize;
	char *pszXmlData = (char*)obXml->toArray(obXml, &nXmlSize);

	httpResponseSetCode(pRes, HTTP_CODE_OK, true);
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
	if(g_conf.methods.bUnlock == false) return response405(pRes);

        return response204(pRes);
}

//
// internal static functions
//

static bool _addXmlResponseStart(Q_VECTOR *obXml) {
	obXml->addStr(obXml, "<?xml version=\"1.0\" encoding=\"utf-8\"?>" CRLF);
	obXml->addStr(obXml, "<D:multistatus xmlns:D=\"DAV:\">" CRLF);

	return true;
}

static bool _addXmlResponseFile(Q_VECTOR *obXml, const char *pszUriPath, struct stat *pFileStat) {
	_addXmlResponseFileHead(obXml, pszUriPath, pFileStat);
	_addXmlResponseFileInfo(obXml, pszUriPath, pFileStat);
	_addXmlResponseFileTail(obXml);
	return true;
}

static bool _addXmlResponseFileHead(Q_VECTOR *obXml, const char *pszUriPath, struct stat *pFileStat) {
	char *pszEncPath = qUrlEncode(pszUriPath, strlen(pszUriPath));

	char *pszTailSlash = "";
	if(S_ISDIR(pFileStat->st_mode) && strcmp(pszUriPath, "/")) pszTailSlash = "/";

	obXml->addStr(obXml, "  <D:response xmlns:ns0=\"DAV:\" xmlns:ns1=\"urn:schemas-microsoft-com:\">" CRLF);
	obXml->addStrf(obXml,"    <D:href>%s%s</D:href>" CRLF, pszEncPath, pszTailSlash);
	obXml->addStr(obXml, "    <D:propstat>" CRLF);
	obXml->addStr(obXml, "      <D:prop>" CRLF);

	free(pszEncPath);
	return true;
}

static bool _addXmlResponseFileInfo(Q_VECTOR *obXml, const char *pszUriPath, struct stat *pFileStat) {
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
	obXml->addStrf(obXml,"        <ns0:resourcetype>%s</ns0:resourcetype>" CRLF, pszResourceType);

	obXml->addStrf(obXml,"        <ns0:getcontentlength>%jd</ns0:getcontentlength>" CRLF, pFileStat->st_size);
	obXml->addStrf(obXml,"        <ns0:creationdate>%s</ns0:creationdate>" CRLF, szLastModified);
	obXml->addStrf(obXml,"        <ns0:getlastmodified>%s</ns0:getlastmodified>" CRLF, qTimeGetGmtStaticStr(pFileStat->st_mtime));
	obXml->addStrf(obXml,"        <ns0:getetag>\"%s\"</ns0:getetag>" CRLF, szEtag);

	obXml->addStr(obXml, "        <D:supportedlock>" CRLF);
	obXml->addStr(obXml, "          <D:lockentry>" CRLF);
	obXml->addStr(obXml, "            <D:lockscope><D:exclusive/></D:lockscope>" CRLF);
	obXml->addStr(obXml, "            <D:locktype><D:write/></D:locktype>" CRLF);
	obXml->addStr(obXml, "          </D:lockentry>" CRLF);
	obXml->addStr(obXml, "          <D:lockentry>" CRLF);
	obXml->addStr(obXml, "            <D:lockscope><D:shared/></D:lockscope>" CRLF);
	obXml->addStr(obXml, "            <D:locktype><D:write/></D:locktype>" CRLF);
	obXml->addStr(obXml, "          </D:lockentry>" CRLF);
	obXml->addStr(obXml, "        </D:supportedlock>" CRLF);

	obXml->addStr(obXml, "        <D:lockdiscovery></D:lockdiscovery>" CRLF);
	obXml->addStrf(obXml,"        <D:getcontenttype>%s</D:getcontenttype>" CRLF, pszContentType);

	return true;
}

static bool _addXmlResponseFileProp(Q_VECTOR *obXml, struct stat *pFileStat) {
	//obXml->addStr(obXml, "      <ns1:Win32LastModifiedTime/>" CRLF);
	//obXml->addStr(obXml, "      <ns1:Win32FileAttributes/>" CRLF);
	obXml->addStrf(obXml,"        <ns1:Win32CreationTime>%s</ns1:Win32CreationTime>" CRLF, qTimeGetGmtStaticStr(pFileStat->st_mtime));
	obXml->addStrf(obXml,"        <ns1:Win32LastAccessTime>%s</ns1:Win32LastAccessTime>" CRLF, qTimeGetGmtStaticStr(pFileStat->st_atime));
	obXml->addStrf(obXml,"        <ns1:Win32LastModifiedTime>%s</ns1:Win32LastModifiedTime>" CRLF, qTimeGetGmtStaticStr(pFileStat->st_mtime));
	obXml->addStr(obXml,"         <ns1:Win32FileAttributes>00000020</ns1:Win32FileAttributes>" CRLF);

	return true;
}

static bool _addXmlResponseFileTail(Q_VECTOR *obXml) {
	obXml->addStr(obXml, "      </D:prop>" CRLF);
	obXml->addStrf(obXml,"      <D:status>%s %d %s</D:status>" CRLF, HTTP_PROTOCOL_11, HTTP_CODE_OK, httpResponseGetMsg(HTTP_CODE_OK));
	obXml->addStr(obXml, "    </D:propstat>" CRLF);
	obXml->addStr(obXml, "  </D:response>" CRLF);

	return true;
}

static bool _addXmlResponseEnd(Q_VECTOR *obXml) {
	obXml->addStr(obXml, "</D:multistatus>" CRLF);
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
