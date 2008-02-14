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

bool httpAccessLog(struct HttpRequest *req, struct HttpResponse *res) {
	if(req->pszRequestMethod == NULL) return false;

	char *pszReferer, *pszAgent, *pszLocation;

	pszReferer = httpHeaderGetValue(req->pHeaders, "REFERER");
	pszAgent = httpHeaderGetValue(req->pHeaders, "USER-AGENT");
	pszLocation = httpHeaderGetValue(res->pHeaders, "LOCATION");

	qLog(g_acclog, "%s - - [%s] \"%s %s %s\" %d %ju \"%s\" \"%s\" \"%s\"",
		poolGetConnAddr(), qGetLocaltimeStr(poolGetConnReqTime()),
		req->pszRequestMethod, req->pszRequestUri, req->pszHttpVersion,
		res->nResponseCode, res->nContentLength,
		(pszReferer != NULL) ? pszReferer : "-",
		(pszAgent != NULL) ? pszAgent : "-",
		(pszLocation != NULL) ? pszLocation : "-"
	);

	return true;
}
