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

int httpStatusResponse(struct HttpRequest *pReq, struct HttpResponse *pRes) {
	if(pReq == NULL || pRes == NULL) return 0;

	Q_VECTOR *obHtml = httpGetStatusHtml();
	if(obHtml == NULL) return response500(pRes);

	// get size
	size_t nHtmlSize = obHtml->size(obHtml);

	// set response
	httpResponseSetCode(pRes, HTTP_CODE_OK, true);
	httpResponseSetContent(pRes, "text/html; charset=\"utf-8\"", NULL, nHtmlSize);

	// print out header
	httpResponseOut(pRes, pReq->nSockFd);

	// print out contents
	streamStackOut(pReq->nSockFd, obHtml, g_conf.nConnectionTimeout * 1000);

	// free
	obHtml->free(obHtml);

	return HTTP_CODE_OK;
}

Q_VECTOR *httpGetStatusHtml(void) {
	struct SharedData *pShm = NULL;
	pShm = poolGetShm();
	if(pShm == NULL) return NULL;

	char *pszBuildMode = "RELEASE";
#ifdef BUILD_DEBUG
	pszBuildMode = "DEBUG";
#endif

	Q_VECTOR *obHtml = qVector();
	obHtml->addStr(obHtml, "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2 Final//EN\">" CRLF);
	obHtml->addStr(obHtml, "<html>" CRLF);
	obHtml->addStr(obHtml, "<head>" CRLF);
	obHtml->addStrf(obHtml,"  <title>%s/%s Status</title>" CRLF, g_prgname, g_prgversion);
	obHtml->addStr(obHtml, "  <meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />" CRLF);
	obHtml->addStr(obHtml, "  <style type=\"text/css\">" CRLF);
	obHtml->addStr(obHtml, "    body,td,th { font-size:14px; }" CRLF);
	obHtml->addStr(obHtml, "  </style>" CRLF);
	obHtml->addStr(obHtml, "</head>" CRLF);
	obHtml->addStr(obHtml, "<body>" CRLF);

	obHtml->addStrf(obHtml,"<h1>%s/%s Status</h1>" CRLF, g_prgname, g_prgversion);
	obHtml->addStr(obHtml, "<dl>" CRLF);
	obHtml->addStrf(obHtml,"  <dt>Server Version: %s v%s (%s; %s; %s)</dt>" CRLF, g_prgname, g_prgversion, __DATE__, __TIME__, pszBuildMode);
	obHtml->addStrf(obHtml,"  <dt>Current Time: %s" CRLF, qTimeGetGmtStaticStr(0));
	obHtml->addStrf(obHtml,"  , Start Time: %s</dt>" CRLF, qTimeGetGmtStaticStr(pShm->nStartTime));
	obHtml->addStrf(obHtml,"  <dt>Total Connections : %d" CRLF, pShm->nTotalConnected);
	obHtml->addStrf(obHtml,"  , Total Requests : %d</dt>" CRLF, pShm->nTotalRequests);
	obHtml->addStrf(obHtml,"  , Total Launched: %d" CRLF, pShm->nTotalLaunched);
	obHtml->addStrf(obHtml,"  , Running Servers: %d</dt>" CRLF, pShm->nRunningChilds);
	obHtml->addStrf(obHtml,"  , Working Servers: %d</dt>" CRLF, pShm->nWorkingChilds);
	obHtml->addStrf(obHtml,"  <dt>Start Servers: %d" CRLF, g_conf.nStartServers);
	obHtml->addStrf(obHtml,"  , Min Spare Servers: %d" CRLF, g_conf.nMinSpareServers);
	obHtml->addStrf(obHtml,"  , Max Spare Servers: %d" CRLF, g_conf.nMaxSpareServers);
	obHtml->addStrf(obHtml,"  , Max Clients: %d</dt>" CRLF, g_conf.nMaxClients);
	obHtml->addStr(obHtml, "</dl>" CRLF);

	obHtml->addStr(obHtml, "<table width='100%%' border=1 cellpadding=1 cellspacing=0>" CRLF);
	obHtml->addStr(obHtml, "  <tr>" CRLF);
	obHtml->addStr(obHtml, "    <th colspan=5>Server Information</th>" CRLF);
	obHtml->addStr(obHtml, "    <th colspan=5>Current Connection</th>" CRLF);
	obHtml->addStr(obHtml, "    <th colspan=4>Request Information</th>" CRLF);
	obHtml->addStr(obHtml, "  </tr>" CRLF);

	obHtml->addStr(obHtml, "  <tr>" CRLF);
	obHtml->addStr(obHtml, "    <th>SNO</th>" CRLF);
	obHtml->addStr(obHtml, "    <th>PID</th>" CRLF);
	obHtml->addStr(obHtml, "    <th>Started</th>" CRLF);
	obHtml->addStr(obHtml, "    <th>Conns</th>" CRLF);
	obHtml->addStr(obHtml, "    <th>Reqs</th>" CRLF);

	obHtml->addStr(obHtml, "    <th>Status</th>" CRLF);
	obHtml->addStr(obHtml, "    <th>Client IP</th>" CRLF);
	obHtml->addStr(obHtml, "    <th>Conn Time</th>" CRLF);
	obHtml->addStr(obHtml, "    <th>Runs</th>" CRLF);
	obHtml->addStr(obHtml, "    <th>Reqs</th>" CRLF);

	obHtml->addStr(obHtml, "    <th>Request Information</th>" CRLF);
	obHtml->addStr(obHtml, "    <th>Res</th>" CRLF);
	obHtml->addStr(obHtml, "    <th>Req Time</th>" CRLF);
	obHtml->addStr(obHtml, "    <th>Runs</th>" CRLF);
	obHtml->addStr(obHtml, "  </tr>" CRLF);

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
		obHtml->addStr(obHtml, "  <tr align=center>" CRLF);
		obHtml->addStrf(obHtml,"    <td>%d</td>" CRLF, j);
		obHtml->addStrf(obHtml,"    <td>%u</td>" CRLF, (unsigned int)pShm->child[i].nPid);
		obHtml->addStrf(obHtml,"    <td>%s</td>" CRLF, qTimeGetGmtStrf(szTimeStr, sizeof(szTimeStr), pShm->child[i].nStartTime, "%Y%m%d%H%M%S"));
		obHtml->addStrf(obHtml,"    <td align=right>%d</td>" CRLF, pShm->child[i].nTotalConnected);
		obHtml->addStrf(obHtml,"    <td align=right>%d</td>" CRLF, pShm->child[i].nTotalRequests);

		obHtml->addStrf(obHtml,"    <td>%s</td>" CRLF, pszStatus);
		obHtml->addStrf(obHtml,"    <td align=left>%s:%d</td>" CRLF, pShm->child[i].conn.szAddr, pShm->child[i].conn.nPort);
		obHtml->addStrf(obHtml,"    <td>%s</td>" CRLF, (pShm->child[i].conn.nStartTime > 0) ? qTimeGetGmtStrf(szTimeStr, sizeof(szTimeStr), pShm->child[i].conn.nStartTime, "%Y%m%d%H%M%S") : "&nbsp;");
		obHtml->addStrf(obHtml,"    <td align=right>%ds</td>" CRLF, nConnRuns);
		obHtml->addStrf(obHtml,"    <td align=right>%d</td>" CRLF, pShm->child[i].conn.nTotalRequests);

		obHtml->addStrf(obHtml,"    <td align=left>%s&nbsp;</td>" CRLF, pShm->child[i].conn.szReqInfo);
		if(pShm->child[i].conn.nResponseCode == 0) obHtml->addStr(obHtml,"    <td>&nbsp;</td>" CRLF);
		else obHtml->addStrf(obHtml,"    <td>%d</td>" CRLF, pShm->child[i].conn.nResponseCode);
		obHtml->addStrf(obHtml,"    <td>%s</td>" CRLF, (pShm->child[i].conn.tvReqTime.tv_sec > 0) ? qTimeGetGmtStrf(szTimeStr, sizeof(szTimeStr), pShm->child[i].conn.tvReqTime.tv_sec, "%Y%m%d%H%M%S") : "&nbsp;");
		obHtml->addStrf(obHtml,"    <td align=right>%.1fms</td>" CRLF, (nReqRuns * 1000));
		obHtml->addStr(obHtml, "  </tr>" CRLF);
	}
	obHtml->addStr(obHtml, "</table>" CRLF);

	obHtml->addStr(obHtml, "<hr>" CRLF);
	obHtml->addStrf(obHtml,"%s v%s, %s" CRLF, g_prgname, g_prgversion, g_prginfo);
	obHtml->addStr(obHtml, "</body>" CRLF);
	obHtml->addStr(obHtml, "</html>" CRLF);

	return obHtml;
}
