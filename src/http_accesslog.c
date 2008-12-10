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

bool httpAccessLog(struct HttpRequest *req, struct HttpResponse *res) {
	if(req->pszRequestMethod == NULL) return false;

	const char *pszHost = httpHeaderGetStr(req->pHeaders, "HOST");
	const char *pszReferer = httpHeaderGetStr(req->pHeaders, "REFERER");
	const char *pszAgent = httpHeaderGetStr(req->pHeaders, "USER-AGENT");
	const char *pszLocation = httpHeaderGetStr(res->pHeaders, "LOCATION");

	qLog(g_acclog, "%s - - [%s] \"%s http://%s%s %s\" %d %jd \"%s\" \"%s\" \"%s\"",
		poolGetConnAddr(),  qTimeGetLocalStaticStr(poolGetConnReqTime()),
		req->pszRequestMethod, pszHost, req->pszRequestUri, req->pszHttpVersion,
		res->nResponseCode, res->nContentLength,
		(pszReferer != NULL) ? pszReferer : "-",
		(pszAgent != NULL) ? pszAgent : "-",
		(pszLocation != NULL) ? pszLocation : "-"
	);

	return true;
}
