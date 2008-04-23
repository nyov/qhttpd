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
 * @return	HttpResponse pointer
 *		NULL : system error
 */
struct HttpResponse *httpHandler(struct HttpRequest *req) {
	if(req == NULL) return NULL;

	// create response
	struct HttpResponse *res = httpResponseCreate();
	if(res == NULL) return NULL;

	// filter bad request
	if(req->nReqStatus == 0) { // bad request
		DEBUG("Bad request.");

		// @todo: bad request answer
		httpResponseSetCode(res, HTTP_RESCODE_BAD_REQUEST, req, false);
		httpResponseSetContentHtml(res, "Your browser sent a request that this server could not understand.");
		return res;
	} else if(req->nReqStatus < 0) { // timeout or connection closed
		DEBUG("Connection timeout.");
		return res;
	}

	// check server-status request
	if(g_conf.bEgisdatadStatusEnable == true
	&& !strcmp(req->pszRequestMethod, "GET")
	&& !strcmp(req->pszRequestUrl, g_conf.szEgisdatadStatusUrl)) {
		Q_OBSTACK *obHtml = httpGetStatusHtml();
		if(obHtml == NULL) {
			response500(req, res);
			return res;
		}

		httpResponseSetCode(res, HTTP_RESCODE_OK, req, true);
		httpResponseSetContent(res, "text/html; charset=\"utf-8\"", qObstackGetSize(obHtml), (char *)qObstackFinish(obHtml));
		qObstackFree(obHtml);

		return res;
	}

	// handle method
	int nResCode = 0;

	// method hooking
	nResCode = hookMethodHandler(req, res);
	if(nResCode != 0) return res;

	// native methods
	if(!strcmp(req->pszRequestMethod, "OPTIONS")) {
		nResCode = httpMethodOptions(req, res);
	} else if(!strcmp(req->pszRequestMethod, "GET")) {
		nResCode = httpMethodGet(req, res);
	} else if(!strcmp(req->pszRequestMethod, "HEAD")) {
		nResCode = httpMethodHead(req, res);
	} else {
		nResCode = httpMethodNotImplemented(req, res);
	}

	return res;
}
