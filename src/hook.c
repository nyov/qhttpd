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
 * $Id: hook.c 132 2010-03-11 05:29:17Z wolkykim $
 */

#ifdef ENABLE_HOOK

#include "qhttpd.h"

//
// process management daemon
//

bool hookBeforeMainInit(void) {

	// changing program name example
	/*
	g_prginfo = "YOUR_PROGRAM_INFO"
	g_prgname = "YOUR_PROGRAM_NAME";
	g_prgversion = "YOUR_PROGRAM_VERSION";
	*/

	return true;
}

bool hookAfterConfigLoaded(struct ServerConfig *config, bool bConfigLoadSucceed) {

	// configuration overing example
	/*
	config->nPort = 8080;
	*/

	return true;
}

bool hookAfterDaemonInit(void) {
	return true;
}

// return : number of jobs did, in case of error return -1
int hookWhileDaemonIdle(void) {
	return 0;
}

bool hookBeforeDaemonEnd(void) {
	return true;
}

bool hookAfterDaemonSIGHUP(void) {
	return true;
}

//
// for each connection
//

bool hookAfterChildInit(void) {
	return true;
}

bool hookBeforeChildEnd(void) {
	return true;
}

bool hookAfterConnEstablished(int nSockFd) {
	return true;
}

//
// request & response hooking
//
// 1. parse request.
// 2.   call luaRequestHandler(), if LUA is enabled
// 3.   call hookRequestHandler(), if HOOK is enabled.
// 4.   call default request handler
// 5.   call hookResponseHandler(), if HOOK is enabled.
// 6.   call luaResponseHandler(), if LUA is enabled
// 7. response out

// return response code if you set response, otherwise return 0 for bypassing to next step.
int hookRequestHandler(struct HttpRequest *pReq, struct HttpResponse *pRes) {
	// example: adding or overriding your new method
	/*
	int nResCode = 0;
	if(!strcmp(pReq->pszRequestMethod, "YOUR_METHOD_XXX")) {
		nResCode = hookMethodXXX(req, res);
	}

	return nResCode;
	*/

	// example: change document root
	/*
	free(pReq->pszDocRoot);
	pReq->pszDocRoot = strdup("/NEW_DOCUMENT_ROOT");
	return 0; // pass to default method handler
	*/

	return 0;
}

bool hookResponseHandler(struct HttpRequest *pReq, struct HttpResponse *pRes) {
	// returns false if you want to log out failure during this call.
	return true;
}

#endif
