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

#ifdef ENABLE_HOOK

#include "qhttpd.h"

//
// process management daemon
//

bool hookBeforeMainInit(void) {

	/*
	g_prginfo = "PROGRAM_INFO"
	g_prgname = "PROGRAM_NAME";
	g_prgversion = "PROGRAM_VERSION";
	*/

	return true;
}

bool hookAfterConfigLoaded(struct ServerConfig *config, bool bConfigLoadSucceed) {
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
// 2.   hook> hookRequestHandler();
// 3.    lua> luaRequestHandler();
// 4. native request handler
// 5.   hook> hookResponseHandler()
// 6.    lua> luaResponseHandler()
// 7. response out

// in case of bad request, hook will not be called.
// return response code, only when you set response code.
// in case of modifying headers and such modifications, return 0.
int hookRequestHandler(struct HttpRequest *req, struct HttpResponse *res) {
	return 0;
}

// in case of bad request, hook will not be called.
bool hookResponseHandler(struct HttpRequest *req, struct HttpResponse *res) {
	return true;
}

#endif
