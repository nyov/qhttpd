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

#define fetch2Str(e, d, n)	do {				\
	const char *t = e->getStr(e, n, false);			\
	if(t == NULL) {						\
		DEBUG("No such entry : %s", n);			\
		e->free(e);					\
		return false;					\
	}							\
	qStrCpy(d, sizeof(d), t);				\
} while (false)


#define fetch2Int(e, d, n)					\
do {								\
	const char *t = e->getStr(e, n, false);			\
	if(t == NULL) {						\
		DEBUG("No such entry : %s", n);			\
		e->free(e);					\
		return false;					\
	}							\
	d = atoi(t);						\
} while (false)

#define fetch2Bool(e, d, n)					\
do {								\
	const char *t = e->getStr(e, n, false);			\
	if(t == NULL) {						\
		DEBUG("No such entry : %s", n);			\
		e->free(e);					\
		return false;					\
	}							\
	d = (!strcasecmp(t, "YES") || !strcasecmp(t, "TRUE") || \
	    !strcasecmp(t, "ON") ) ? true : false;		\
} while (false)

/**
 * configuration parser
 *
 * @param pConf		ServerConfig structure
 * @param pszFilePath	file path of egis.conf
 * @return true if successful otherwise returns false
 */
bool loadConfig(struct ServerConfig *pConf, char *pszFilePath) {
	if (pszFilePath == NULL || !strcmp(pszFilePath, "")) {
		return false;
	}

	// parse configuration file
	Q_ENTRY *entry = qConfigParseFile(NULL, pszFilePath, '=');
	if (entry == NULL) {
		DEBUG("Can't open file %s", pszFilePath);
		return false;
	}

	// copy to structure
	qStrCpy(pConf->szConfigFile, sizeof(pConf->szConfigFile), pszFilePath);

	fetch2Str(entry, pConf->szPidFile, "PidFile");
	fetch2Str(entry, pConf->szMimeFile, "MimeFile");

	fetch2Str(entry, pConf->szDocumentRoot, "DocumentRoot");
	fetch2Str(entry, pConf->szDirectoryIndex, "DirectoryIndex");

	fetch2Int(entry, pConf->nPort, "Port");
	fetch2Int(entry, pConf->nMaxPending, "MaxPending");

	fetch2Int(entry, pConf->nStartServers, "StartServers");
	fetch2Int(entry, pConf->nMinSpareServers, "MinSpareServers");
	fetch2Int(entry, pConf->nMaxSpareServers, "MaxSpareServers");
	fetch2Int(entry, pConf->nMaxIdleSeconds, "MaxIdleSeconds");
	fetch2Int(entry, pConf->nMaxClients, "MaxClients");
	fetch2Int(entry, pConf->nMaxRequestsPerChild, "MaxRequestsPerChild");

	fetch2Bool(entry, pConf->bKeepAliveEnable, "KeepAliveEnable");
	fetch2Int(entry, pConf->nMaxKeepAliveRequests, "MaxKeepAliveRequests");

	fetch2Int(entry, pConf->nConnectionTimeout, "ConnectionTimeout");
	fetch2Bool(entry, pConf->bIgnoreOverConnection, "IgnoreOverConnection");
	fetch2Int(entry, pConf->nResponseExpires, "ResponseExpires");

	fetch2Str(entry, pConf->szAllowedMethods, "AllowedMethods");

	fetch2Bool(entry, pConf->bEnableLua, "EnableLua");
	fetch2Str(entry, pConf->szLuaScript, "LuaScript");

	fetch2Bool(entry, pConf->bStatusEnable, "StatusEnable");
	fetch2Str(entry, pConf->szStatusUrl, "StatusUrl");

	fetch2Str(entry, pConf->szErrorLog, "ErrorLog");
	fetch2Str(entry, pConf->szAccessLog, "AccessLog");
	fetch2Int(entry, pConf->nLogRotate, "LogRotate");
	fetch2Int(entry, pConf->nLogLevel, "LogLevel");

	//
	// free resources
	//
	entry->free(entry);

	return true;
}

bool checkConfig(struct ServerConfig *pConf) {
	// allowed methods parsing
	qStrUpper(pConf->szAllowedMethods);
	if(!strcmp(pConf->szAllowedMethods, "ALL")) {
		qStrCpy(pConf->szAllowedMethods, sizeof(pConf->szAllowedMethods),
		"OPTIONS,HEAD,GET,PUT"
		","
		"PROPFIND,PROPPATCH,MKCOL,MOVE,DELETE,LOCK,UNLOCK");
	}

	if(strstr(pConf->szAllowedMethods, "OPTIONS") != NULL) pConf->methods.bOptions = true;
	if(strstr(pConf->szAllowedMethods, "HEAD") != NULL) pConf->methods.bHead = true;
	if(strstr(pConf->szAllowedMethods, "GET") != NULL) pConf->methods.bGet = true;
	if(strstr(pConf->szAllowedMethods, "PUT") != NULL) pConf->methods.bPut = true;

	if(strstr(pConf->szAllowedMethods, "PROPFIND") != NULL) pConf->methods.bPropfind = true;
	if(strstr(pConf->szAllowedMethods, "PROPPATCH") != NULL) pConf->methods.bProppatch = true;
	if(strstr(pConf->szAllowedMethods, "MKCOL") != NULL) pConf->methods.bMkcol = true;
	if(strstr(pConf->szAllowedMethods, "MOVE") != NULL) pConf->methods.bMove = true;
	if(strstr(pConf->szAllowedMethods, "DELETE") != NULL) pConf->methods.bDelete = true;
	if(strstr(pConf->szAllowedMethods, "LOCK") != NULL) pConf->methods.bLock = true;
	if(strstr(pConf->szAllowedMethods, "UNLOCK") != NULL) pConf->methods.bUnlock = true;

	return true;
}
