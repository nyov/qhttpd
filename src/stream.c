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

ssize_t streamRead(void *pszBuffer, int nSockFd, size_t nSize, int nTimeoutMs) {
	ssize_t nReaded = qIoRead(pszBuffer, nSockFd, nSize, nTimeoutMs);
	if(nReaded > 0) DEBUG("[RX] (binary, readed %zd bytes)", nReaded);
	return nReaded;
}

ssize_t streamGets(char *pszStr, size_t nSize, int nSockFd, int nTimeoutMs) {
	ssize_t nReaded = qIoGets(pszStr, nSize, nSockFd, nTimeoutMs);
	if(nReaded > 0) DEBUG("[RX] %s", pszStr);
	return nReaded;
}

ssize_t streamGetb(char *pszBuffer, int nSockFd, size_t nSize, int nTimeoutMs) {
	ssize_t nReaded = qIoRead(pszBuffer, nSockFd, nSize, nTimeoutMs);
	DEBUG("[RX] (binary, readed/request=%zd/%zu bytes)", nReaded, nSize);
	return nReaded;
}

ssize_t streamPrintf(int nSockFd, const char *format, ...) {
	char szBuf[1024];
	va_list arglist;

	va_start(arglist, format);
	vsnprintf(szBuf, sizeof(szBuf)-1, format, arglist);
	szBuf[sizeof(szBuf)-1] = '\0';
	va_end(arglist);

	ssize_t nSent = qIoWrite(nSockFd, szBuf, strlen(szBuf), 0);
	if(nSent >= 0) DEBUG("[TX] %s", qStrTrim(szBuf));
	else DEBUG("[TX-ERR] %s", qStrTrim(szBuf));

	return nSent;
}

ssize_t streamPuts(int nSockFd, const char *pszStr) {
	ssize_t nSent = qIoPuts(nSockFd, pszStr, 0);
	if(nSent >= 0) DEBUG("[TX] %s", pszStr);
	else DEBUG("[TX-ERR] %s", pszStr);
	return nSent;
}

ssize_t streamWrite(int nSockFd, const void *pszBuffer, size_t nSize) {
	ssize_t nSent = qIoWrite(nSockFd, pszBuffer, nSize, 0);
	DEBUG("[TX] (binary, sent/request=%zd/%zu bytes)\n%s", nSent, nSize, (char *)pszBuffer);
	return nSent;
}

off_t streamSave(int nFd, int nSockFd, off_t nSize, int nTimeoutMs) {
	off_t nSaved = qIoSend(nFd, nSockFd, nSize, nTimeoutMs);
	DEBUG("[RX] (save %jd/%jd bytes)", nSaved, nSize);
	return nSaved;
}

off_t streamSend(int nSockFd, int nFd, off_t nSize, int nTimeoutMs) {
	off_t nSent = qIoSend(nSockFd, nFd, nSize, nTimeoutMs);
        DEBUG("[TX] (send %jd/%jd bytes)", nSent, nSize);
	return nSent;
}
