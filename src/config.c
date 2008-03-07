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

/**
 * configuration parser
 *
 * @param pszFilePath	egis.conf 파일 경로, NULL 입력시 디폴트로
 * @param pConf		Config 구조체 포인터
 * @return 성공시 true, 실패시 false
 */

#define fetch2Str(e, d, n)	do {				\
	char *t = qfGetValue(e, n);				\
	if(t == NULL) {						\
		DEBUG("No such entry : %s", n);			\
		qfFree(e);					\
		return false;					\
	}							\
	qStrncpy(d, t, sizeof(d));				\
} while (false)


#define fetch2Int(e, d, n)					\
do {								\
	char *t = qfGetValue(e, n);				\
	if(t == NULL) {						\
		DEBUG("No such entry : %s", n);			\
		qfFree(e);					\
		return false;					\
	}							\
	d = atoi(t);						\
} while (false)

#define fetch2Bool(e, d, n)					\
do {								\
	char *t = qfGetValue(e, n);				\
	if(t == NULL) {						\
		DEBUG("No such entry : %s", n);			\
		qfFree(e);					\
		return false;					\
	}							\
	d = (!strcasecmp(t, "YES") || !strcasecmp(t, "TRUE") || \
	    !strcasecmp(t, "ON") ) ? true : false;		\
} while (false)

bool loadConfig(char *pszFilePath, Config *pConf) {
	if (pszFilePath == NULL || !strcmp(pszFilePath, "")) {
		return false;
	}

	// 설정 파일 로드
	Q_ENTRY *entry = qfDecoder(pszFilePath);
	if (entry == NULL) {
		return false;
	}

	// 구조체에 저장
	qStrncpy(pConf->szConfigFile, pszFilePath, sizeof(pConf->szConfigFile));

	fetch2Str(entry, pConf->szRunDir, "qhttpd.RunDir");
	fetch2Str(entry, pConf->szLogDir, "qhttpd.LogDir");
	fetch2Str(entry, pConf->szDataDir, "qhttpd.DataDir");
	fetch2Str(entry, pConf->szTmpDir, "qhttpd.TmpDir");

	fetch2Str(entry, pConf->szMimeFile, "qhttpd.MimeFile");
	fetch2Str(entry, pConf->szMimeDefault, "qhttpd.MimeDefault");

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

	fetch2Str(entry, pConf->szErrorLog, "qhttpd.ErrorLog");
	fetch2Str(entry, pConf->szAccessLog, "qhttpd.AccessLog");
	fetch2Int(entry, pConf->nLogRotate, "qhttpd.LogRotate");
	fetch2Int(entry, pConf->nLogLevel, "qhttpd.LogLevel");

	fetch2Bool(entry, pConf->bStatusEnable, "qhttpd.StatusEnable");
	fetch2Str(entry, pConf->szStatusUrl, "qhttpd.StatusUrl");

	//
	// 설정 파일 해제
	//
	qfFree(entry);

	return true;
}
