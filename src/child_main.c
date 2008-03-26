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

/////////////////////////////////////////////////////////////////////////
// PUBLIC FUNCTIONS
/////////////////////////////////////////////////////////////////////////

int qSocketWaitWriteDone(int sockfd, int timeoutms) {
	struct timeval tv;
	fd_set readfds;

	// time to wait
	FD_ZERO(&readfds);
	FD_SET(sockfd, &readfds);
	if (timeoutms > 0) {
		tv.tv_sec = (timeoutms / 1000), tv.tv_usec = ((timeoutms % 1000) * 1000);
		if (select(FD_SETSIZE, &readfds, NULL, NULL, &tv) < 0) return -1;
	} else if (timeoutms == 0) { // just poll
		tv.tv_sec = 0, tv.tv_usec = 0;
		if (select(FD_SETSIZE, &readfds, NULL, NULL, &tv) < 0) return -1;
	} else { //  blocks indefinitely
		if (select(FD_SETSIZE, &readfds, NULL, NULL, NULL) < 0) return -1;
	}

	if (!FD_ISSET(sockfd, &readfds)) return 0; // timeout

	return 1;
}

int childMain(int nSockFd) {
	bool bKeepAlive = false;

	do {
		/////////////////////////////////////////////////////////
		// Pre-processing Block
		/////////////////////////////////////////////////////////

		struct HttpRequest *req = NULL;
		struct HttpResponse *res = NULL;

		// reset keep-alive
		bKeepAlive = false;

		/////////////////////////////////////////////////////////
		// Request processing Block
		/////////////////////////////////////////////////////////

		// parse request
		req = httpRequestParse(nSockFd, g_conf.nConnectionTimeout);
		if(req == NULL) {
			LOG_ERR("System Error #1.");
			break;
		}

		if(req->nReqStatus >= 0) {
			poolSetConnRequest(req); // set request information

			// handle request
			res = httpHandler(req);
			if(res == NULL) {
				LOG_ERR("System Error #2.");
				break;
			}
			poolSetConnResponse(res); // set response information

			// serialize & stream out
			httpResponseOut(res, nSockFd);

			// logging
			httpAccessLog(req, res);

			// check keep-alive
			if(httpHeaderHasString(res->pHeaders, "CONNECTION", "KEEP-ALIVE") == true) bKeepAlive = true;
		}

		/////////////////////////////////////////////////////////
		// Post-processing Block
		/////////////////////////////////////////////////////////

		// free resources
		if(req != NULL) httpRequestFree(req);
		if(res != NULL) httpResponseFree(res);
	} while(bKeepAlive == true);

	return 0;
}
