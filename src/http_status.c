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

Q_OBSTACK *httpGetStatusHtml(void) {
	struct SharedData *pShm = NULL;
	pShm = poolGetShm();
	if(pShm == NULL) return NULL;

	char *pszBuildMode = "RELEASE";
#ifdef BUILD_DEBUG
	pszBuildMode = "DEBUG";
#endif

	Q_OBSTACK *obHtml = qObstack();
	obHtml->growStr(obHtml, "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2 Final//EN\">\r\n");
	obHtml->growStr(obHtml, "<html>\r\n");
	obHtml->growStr(obHtml, "<head>\r\n");
	obHtml->growStrf(obHtml,"  <title>%s/%s Status</title>\r\n", g_prgname, g_prgversion);
	obHtml->growStr(obHtml, "  <meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />\r\n");
	obHtml->growStr(obHtml, "  <style type=\"text/css\">\r\n");
	obHtml->growStr(obHtml, "    body,td,th { font-size:14px; }\r\n");
	obHtml->growStr(obHtml, "  </style>\r\n");
	obHtml->growStr(obHtml, "</head>\r\n");
	obHtml->growStr(obHtml, "<body>\r\n");

	char *pszCurrentTime = qTimeGetLocalStr(0);
	char *pszStartTime = qTimeGetLocalStr(pShm->nStartTime);
	obHtml->growStrf(obHtml,"<h1>%s/%s Status</h1>\r\n", g_prgname, g_prgversion);
	obHtml->growStr(obHtml, "<dl>\r\n");
	obHtml->growStrf(obHtml,"  <dt>Server Version: %s v%s (%s; %s; %s)</dt>\r\n", g_prgname, g_prgversion, __DATE__, __TIME__, pszBuildMode);
	obHtml->growStrf(obHtml,"  <dt>Current Time: %s\r\n", pszCurrentTime);
	obHtml->growStrf(obHtml,"  , Start Time: %s</dt>\r\n", pszStartTime);
	obHtml->growStrf(obHtml,"  <dt>Total Connections : %d\r\n", pShm->nTotalConnected);
	obHtml->growStrf(obHtml,"  , Total Requests : %d</dt>\r\n", pShm->nTotalRequests);
	obHtml->growStrf(obHtml,"  , Total Launched: %d\r\n", pShm->nTotalLaunched);
	obHtml->growStrf(obHtml,"  , Running Servers: %d</dt>\r\n", pShm->nRunningChilds);
	obHtml->growStrf(obHtml,"  , Working Servers: %d</dt>\r\n", pShm->nWorkingChilds);
	obHtml->growStrf(obHtml,"  <dt>Start Servers: %d\r\n", g_conf.nStartServers);
	obHtml->growStrf(obHtml,"  , Min Spare Servers: %d\r\n", g_conf.nMinSpareServers);
	obHtml->growStrf(obHtml,"  , Max Spare Servers: %d\r\n", g_conf.nMaxSpareServers);
	obHtml->growStrf(obHtml,"  , Max Clients: %d</dt>\r\n", g_conf.nMaxClients);
	obHtml->growStr(obHtml, "</dl>\r\n");
	free(pszCurrentTime);
	free(pszStartTime);

	obHtml->growStr(obHtml, "<table width='100%%' border=1 cellpadding=1 cellspacing=0>\r\n");
	obHtml->growStr(obHtml, "  <tr>\r\n");
	obHtml->growStr(obHtml, "    <th colspan=5>Server Information</th>\r\n");
	obHtml->growStr(obHtml, "    <th colspan=5>Current Connection</th>\r\n");
	obHtml->growStr(obHtml, "    <th colspan=4>Request Information</th>\r\n");
	obHtml->growStr(obHtml, "  </tr>\r\n");

	obHtml->growStr(obHtml, "  <tr>\r\n");
	obHtml->growStr(obHtml, "    <th>SNO</th>\r\n");
	obHtml->growStr(obHtml, "    <th>PID</th>\r\n");
	obHtml->growStr(obHtml, "    <th>Started</th>\r\n");
	obHtml->growStr(obHtml, "    <th>Conns</th>\r\n");
	obHtml->growStr(obHtml, "    <th>Reqs</th>\r\n");

	obHtml->growStr(obHtml, "    <th>Status</th>\r\n");
	obHtml->growStr(obHtml, "    <th>Client IP</th>\r\n");
	obHtml->growStr(obHtml, "    <th>Conn Time</th>\r\n");
	obHtml->growStr(obHtml, "    <th>Runs</th>\r\n");
	obHtml->growStr(obHtml, "    <th>Reqs</th>\r\n");

	obHtml->growStr(obHtml, "    <th>Request Information</th>\r\n");
	obHtml->growStr(obHtml, "    <th>Res</th>\r\n");
	obHtml->growStr(obHtml, "    <th>Req Time</th>\r\n");
	obHtml->growStr(obHtml, "    <th>Runs</th>\r\n");
	obHtml->growStr(obHtml, "  </tr>\r\n");

	int i, j;
	for(i = j = 0; i <  g_conf.nMaxClients; i++) {
		if(pShm->child[i].nPid == 0) continue;
		j++;

		char *pszStatus = "-";
		int nConnRuns = 0;
		float nReqRuns = 0;

		if(pShm->child[i].conn.bConnected == true) {
			if(pShm->child[i].conn.bRun == true) pszStatus = "W";
			else pszStatus = "K";
		}

		if(pShm->child[i].conn.nEndTime >= pShm->child[i].conn.nStartTime) nConnRuns = difftime(pShm->child[i].conn.nEndTime, pShm->child[i].conn.nStartTime);
		else if(pShm->child[i].conn.nStartTime > 0) nConnRuns = difftime(time(NULL), pShm->child[i].conn.nStartTime);

		if(pShm->child[i].conn.bRun == true) nReqRuns = getDiffTimeval(NULL, &pShm->child[i].conn.tvReqTime);
		else nReqRuns = getDiffTimeval(&pShm->child[i].conn.tvResTime, &pShm->child[i].conn.tvReqTime);

		char szTimeStr[sizeof(char) * (CONST_STRLEN("YYYYMMDDhhmmss")+1)];
		obHtml->growStr(obHtml, "  <tr align=center>\r\n");
		obHtml->growStrf(obHtml,"    <td>%d</td>\r\n", j);
		obHtml->growStrf(obHtml,"    <td>%d</td>\r\n", pShm->child[i].nPid);
		obHtml->growStrf(obHtml,"    <td>%s</td>\r\n", qTimeGetGmtStrf(szTimeStr, sizeof(szTimeStr), pShm->child[i].nStartTime, "%Y%m%d%H%M%S"));
		obHtml->growStrf(obHtml,"    <td align=right>%d</td>\r\n", pShm->child[i].nTotalConnected);
		obHtml->growStrf(obHtml,"    <td align=right>%d</td>\r\n", pShm->child[i].nTotalRequests);

		obHtml->growStrf(obHtml,"    <td>%s</td>\r\n", pszStatus);
		obHtml->growStrf(obHtml,"    <td align=left>%s:%d</td>\r\n", pShm->child[i].conn.szAddr, pShm->child[i].conn.nPort);
		obHtml->growStrf(obHtml,"    <td>%s</td>\r\n", (pShm->child[i].conn.nStartTime > 0) ? qTimeGetGmtStrf(szTimeStr, sizeof(szTimeStr), pShm->child[i].conn.nStartTime, "%Y%m%d%H%M%S") : "&nbsp;");
		obHtml->growStrf(obHtml,"    <td align=right>%ds</td>\r\n", nConnRuns);
		obHtml->growStrf(obHtml,"    <td align=right>%d</td>\r\n", pShm->child[i].conn.nTotalRequests);

		obHtml->growStrf(obHtml,"    <td align=left>%s&nbsp;</td>\r\n", pShm->child[i].conn.szReqInfo);
		if(pShm->child[i].conn.nResponseCode == 0) obHtml->growStrf(obHtml,"    <td>&nbsp;</td>\r\n");
		else obHtml->growStrf(obHtml,"    <td>%d</td>\r\n", pShm->child[i].conn.nResponseCode);
		obHtml->growStrf(obHtml,"    <td>%s</td>\r\n", (pShm->child[i].conn.tvReqTime.tv_sec > 0) ? qTimeGetGmtStrf(szTimeStr, sizeof(szTimeStr), pShm->child[i].conn.tvReqTime.tv_sec, "%Y%m%d%H%M%S") : "&nbsp;");
		obHtml->growStrf(obHtml,"    <td align=right>%.1fms</td>\r\n", (nReqRuns * 1000));
		obHtml->growStr(obHtml, "  </tr>\r\n");
	}
	obHtml->growStr(obHtml, "</table>\r\n");

	obHtml->growStr(obHtml, "<hr>\r\n");
	obHtml->growStrf(obHtml,"%s v%s, %s\r\n", g_prgname, g_prgversion, g_prginfo);
	obHtml->growStr(obHtml, "</body>\r\n");
	obHtml->growStr(obHtml, "</html>\r\n");

	return obHtml;
}
