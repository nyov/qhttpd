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
	const char *t = qEntryGetStr(e, n);			\
	if(t == NULL) {						\
		DEBUG("No such entry : %s", n);			\
		qEntryFree(e);					\
		return false;					\
	}							\
	qStrCpy(d, sizeof(d), t, sizeof(d));			\
} while (false)


#define fetch2Int(e, d, n)					\
do {								\
	const char *t = qEntryGetStr(e, n);			\
	if(t == NULL) {						\
		DEBUG("No such entry : %s", n);			\
		qEntryFree(e);					\
		return false;					\
	}							\
	d = atoi(t);						\
} while (false)

#define fetch2Bool(e, d, n)					\
do {								\
	const char *t = qEntryGetStr(e, n);			\
	if(t == NULL) {						\
		DEBUG("No such entry : %s", n);			\
		qEntryFree(e);					\
		return false;					\
	}							\
	d = (!strcasecmp(t, "YES") || !strcasecmp(t, "TRUE") || \
	    !strcasecmp(t, "ON") ) ? true : false;		\
} while (false)

/**
 * configuration parser
 *
 * @param pConf		Config structure
 * @param pszFilePath	file path of egis.conf
 * @return true if successful otherwise returns false
 */
bool loadConfig(Config *pConf, char *pszFilePath) {
	if (pszFilePath == NULL || !strcmp(pszFilePath, "")) {
		return false;
	}

	// parse configuration file
	Q_ENTRY *entry = qConfigParseFile(NULL, pszFilePath, '=');
	if (entry == NULL) {
		return false;
	}

	// copy to structure
	qStrCpy(pConf->szConfigFile, sizeof(pConf->szConfigFile), pszFilePath, sizeof(pConf->szConfigFile));

	fetch2Str(entry, pConf->szRunDir, "qhttpd.RunDir");
	fetch2Str(entry, pConf->szLogDir, "qhttpd.LogDir");
	fetch2Str(entry, pConf->szDataDir, "qhttpd.DataDir");
	fetch2Str(entry, pConf->szTmpDir, "qhttpd.TmpDir");

	fetch2Str(entry, pConf->szMimeFile, "qhttpd.MimeFile");

	fetch2Str(entry, pConf->szPidfile, "qhttpd.PidFile");
	fetch2Int(entry, pConf->nPort, "qhttpd.Port");
	fetch2Int(entry, pConf->nMaxpending, "qhttpd.MaxPending");

	fetch2Int(entry, pConf->nStartServers, "qhttpd.StartServers");
	fetch2Int(entry, pConf->nMinSpareServers, "qhttpd.MinSpareServers");
	fetch2Int(entry, pConf->nMaxSpareServers, "qhttpd.MaxSpareServers");
	fetch2Int(entry, pConf->nMaxIdleSeconds, "qhttpd.MaxIdleSeconds");
	fetch2Int(entry, pConf->nMaxClients, "qhttpd.MaxClients");
	fetch2Int(entry, pConf->nMaxRequestsPerChild, "qhttpd.MaxRequestsPerChild");

	fetch2Bool(entry, pConf->bKeepAliveEnable, "qhttpd.KeepAliveEnable");
	fetch2Int(entry, pConf->nConnectionTimeout, "qhttpd.ConnectionTimeout");

	fetch2Int(entry, pConf->nResponseExpires, "qhttpd.ResponseExpires");

	fetch2Bool(entry, pConf->bEnableLua, "qhttpd.EnableLua");
	fetch2Str(entry, pConf->szLuaScript, "qhttpd.LuaScript");

	fetch2Str(entry, pConf->szErrorLog, "qhttpd.ErrorLog");
	fetch2Str(entry, pConf->szAccessLog, "qhttpd.AccessLog");
	fetch2Int(entry, pConf->nLogRotate, "qhttpd.LogRotate");
	fetch2Int(entry, pConf->nLogLevel, "qhttpd.LogLevel");

	fetch2Bool(entry, pConf->bStatusEnable, "qhttpd.StatusEnable");
	fetch2Str(entry, pConf->szStatusUrl, "qhttpd.StatusUrl");

	//
	// free resources
	//
	qEntryFree(entry);

	return true;
}
