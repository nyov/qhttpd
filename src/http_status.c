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

Q_OBSTACK *httpGetStatusHtml(void) {
	struct SharedData *pShm = NULL;
	pShm = poolGetShm();
	if(pShm == NULL) return NULL;

	char *pszBuildMode = "RELEASE";
#ifdef BUILD_DEBUG
	pszBuildMode = "DEBUG";
#endif

	Q_OBSTACK *obHtml = qObstackInit();
	qObstackGrowStr(obHtml, "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2 Final//EN\">\r\n");
	qObstackGrowStr(obHtml, "<html>\r\n");
	qObstackGrowStr(obHtml, "<head>\r\n");
	qObstackGrowStrf(obHtml,"  <title>%s/%s Status</title>\r\n", PRG_NAME, PRG_VERSION);
	qObstackGrowStr(obHtml, "  <meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />\r\n");
	qObstackGrowStr(obHtml, "  <style type=\"text/css\">\r\n");
	qObstackGrowStr(obHtml, "    body,td,th { font-size:14px; }\r\n");
	qObstackGrowStr(obHtml, "  </style>\r\n");
	qObstackGrowStr(obHtml, "</head>\r\n");
	qObstackGrowStr(obHtml, "<body>\r\n");

	char *pszCurrentTime = qTimeGetLocalStr(0);
	char *pszStartTime = qTimeGetLocalStr(pShm->nStartTime);
	qObstackGrowStrf(obHtml,"<h1>%s/%s Status</h1>\r\n", PRG_NAME, PRG_VERSION);
	qObstackGrowStr(obHtml, "<dl>\r\n");
	qObstackGrowStrf(obHtml,"  <dt>Server Version: %s v%s (%s; %s; %s)</dt>\r\n", PRG_NAME, PRG_VERSION, __DATE__, __TIME__, pszBuildMode);
	qObstackGrowStrf(obHtml,"  <dt>Current Time: %s\r\n", pszCurrentTime);
	qObstackGrowStrf(obHtml,"  , Start Time: %s</dt>\r\n", pszStartTime);
	qObstackGrowStrf(obHtml,"  <dt>Total Connections : %d\r\n", pShm->nTotalConnected);
	qObstackGrowStrf(obHtml,"  , Total Requests : %d</dt>\r\n", pShm->nTotalRequests);
	qObstackGrowStrf(obHtml,"  , Total Launched: %d\r\n", pShm->nTotalLaunched);
	qObstackGrowStrf(obHtml,"  , Current Servers: %d</dt>\r\n", pShm->nCurrentChilds);
	qObstackGrowStrf(obHtml,"  , Working Servers: %d</dt>\r\n", poolGetWorkingChilds());
	qObstackGrowStrf(obHtml,"  <dt>Start Servers: %d\r\n", g_conf.nStartServers);
	qObstackGrowStrf(obHtml,"  , Min Spare Servers: %d\r\n", g_conf.nMinSpareServers);
	qObstackGrowStrf(obHtml,"  , Max Spare Servers: %d\r\n", g_conf.nMaxSpareServers);
	qObstackGrowStrf(obHtml,"  , Max Clients: %d</dt>\r\n", g_conf.nMaxClients);
	qObstackGrowStr(obHtml, "</dl>\r\n");
	free(pszCurrentTime);
	free(pszStartTime);

	qObstackGrowStr(obHtml, "<table width='100%%' border=1 cellpadding=1 cellspacing=0>\r\n");
	qObstackGrowStr(obHtml, "  <tr>\r\n");
	qObstackGrowStr(obHtml, "    <th colspan=5>Server Information</th>\r\n");
	qObstackGrowStr(obHtml, "    <th colspan=5>Current Connection</th>\r\n");
	qObstackGrowStr(obHtml, "    <th colspan=4>Request Information</th>\r\n");
	qObstackGrowStr(obHtml, "  </tr>\r\n");

	qObstackGrowStr(obHtml, "  <tr>\r\n");
	qObstackGrowStr(obHtml, "    <th>SNO</th>\r\n");
	qObstackGrowStr(obHtml, "    <th>PID</th>\r\n");
	qObstackGrowStr(obHtml, "    <th>Started</th>\r\n");
	qObstackGrowStr(obHtml, "    <th>Conns</th>\r\n");
	qObstackGrowStr(obHtml, "    <th>Reqs</th>\r\n");

	qObstackGrowStr(obHtml, "    <th>Status</th>\r\n");
	qObstackGrowStr(obHtml, "    <th>Client IP</th>\r\n");
	qObstackGrowStr(obHtml, "    <th>Conn Time</th>\r\n");
	qObstackGrowStr(obHtml, "    <th>Runs</th>\r\n");
	qObstackGrowStr(obHtml, "    <th>Reqs</th>\r\n");

	qObstackGrowStr(obHtml, "    <th>Request Information</th>\r\n");
	qObstackGrowStr(obHtml, "    <th>Res</th>\r\n");
	qObstackGrowStr(obHtml, "    <th>Req Time</th>\r\n");
	qObstackGrowStr(obHtml, "    <th>Runs</th>\r\n");
	qObstackGrowStr(obHtml, "  </tr>\r\n");

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

		if(pShm->child[i].conn.bRun == true) nReqRuns = diffTimeval(NULL, &pShm->child[i].conn.tvReqTime);
		else nReqRuns = diffTimeval(&pShm->child[i].conn.tvResTime, &pShm->child[i].conn.tvReqTime);

		char szTimeStr[sizeof(char) * (CONST_STRLEN("YYYYMMDDhhmmss")+1)];
		qObstackGrowStr(obHtml, "  <tr align=center>\r\n");
		qObstackGrowStrf(obHtml,"    <td>%d</td>\r\n", j);
		qObstackGrowStrf(obHtml,"    <td>%d</td>\r\n", pShm->child[i].nPid);
		qObstackGrowStrf(obHtml,"    <td>%s</td>\r\n", qTimeGetGmtStrf(szTimeStr, sizeof(szTimeStr), pShm->child[i].nStartTime, "%Y%m%d%H%M%S"));
		qObstackGrowStrf(obHtml,"    <td align=right>%d</td>\r\n", pShm->child[i].nTotalConnected);
		qObstackGrowStrf(obHtml,"    <td align=right>%d</td>\r\n", pShm->child[i].nTotalRequests);

		qObstackGrowStrf(obHtml,"    <td>%s</td>\r\n", pszStatus);
		qObstackGrowStrf(obHtml,"    <td align=left>%s:%d</td>\r\n", pShm->child[i].conn.szAddr, pShm->child[i].conn.nPort);
		qObstackGrowStrf(obHtml,"    <td>%s</td>\r\n", (pShm->child[i].conn.nStartTime > 0) ? qTimeGetGmtStrf(szTimeStr, sizeof(szTimeStr), pShm->child[i].conn.nStartTime, "%Y%m%d%H%M%S") : "&nbsp;");
		qObstackGrowStrf(obHtml,"    <td align=right>%ds</td>\r\n", nConnRuns);
		qObstackGrowStrf(obHtml,"    <td align=right>%d</td>\r\n", pShm->child[i].conn.nTotalRequests);

		qObstackGrowStrf(obHtml,"    <td align=left>%s&nbsp;</td>\r\n", pShm->child[i].conn.szReqInfo);
		if(pShm->child[i].conn.nResponseCode == 0) qObstackGrowStrf(obHtml,"    <td>&nbsp;</td>\r\n");
		else qObstackGrowStrf(obHtml,"    <td>%d</td>\r\n", pShm->child[i].conn.nResponseCode);
		qObstackGrowStrf(obHtml,"    <td>%s</td>\r\n", (pShm->child[i].conn.tvReqTime.tv_sec > 0) ? qTimeGetGmtStrf(szTimeStr, sizeof(szTimeStr), pShm->child[i].conn.tvReqTime.tv_sec, "%Y%m%d%H%M%S") : "&nbsp;");
		qObstackGrowStrf(obHtml,"    <td align=right>%.1fms</td>\r\n", (nReqRuns * 1000));
		qObstackGrowStr(obHtml, "  </tr>\r\n");
	}
	qObstackGrowStr(obHtml, "</table>\r\n");

	qObstackGrowStr(obHtml, "<hr>\r\n");
	qObstackGrowStrf(obHtml,"%s v%s, %s\r\n", PRG_NAME, PRG_VERSION, PRG_INFO);
	qObstackGrowStr(obHtml, "</body>\r\n");
	qObstackGrowStr(obHtml, "</html>\r\n");

	return obHtml;
}
