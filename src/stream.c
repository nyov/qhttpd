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

ssize_t streamRead(void *pszBuffer, int nSockFd, size_t nSize, int nTimeoutMs) {
	ssize_t nReaded = qIoRead(pszBuffer, nSockFd, nSize, nTimeoutMs);
#ifdef BUILD_DEBUG
	if(nReaded > 0) DEBUG("[RX] (binary, readed %zd bytes)", nReaded);
#endif
	return nReaded;
}

ssize_t streamGets(char *pszStr, size_t nSize, int nSockFd, int nTimeoutMs) {
	ssize_t nReaded = qIoGets(pszStr, nSize, nSockFd, nTimeoutMs);
#ifdef BUILD_DEBUG
	if(nReaded > 0) DEBUG("[RX] %s", pszStr);
#endif
	return nReaded;
}

ssize_t streamGetb(char *pszBuffer, int nSockFd, size_t nSize, int nTimeoutMs) {
	ssize_t nReaded = qIoRead(pszBuffer, nSockFd, nSize, nTimeoutMs);
	DEBUG("[RX] (binary, readed/request=%zd/%zu bytes)", nReaded, nSize);
	return nReaded;
}

off_t streamSave(int nFd, int nSockFd, off_t nSize, int nTimeoutMs) {
	off_t nSaved = qIoSend(nFd, nSockFd, nSize, nTimeoutMs);
	DEBUG("[RX] (save %jd/%jd bytes)", nSaved, nSize);
	return nSaved;
}

ssize_t streamPrintf(int nSockFd, const char *format, ...) {
        char *pszBuf;
        DYNAMIC_VSPRINTF(pszBuf, format);
        if(pszBuf == NULL) return -1;

	ssize_t nSent = qIoWrite(nSockFd, pszBuf, strlen(pszBuf), 0);

#ifdef BUILD_DEBUG
	if(nSent > 0) DEBUG("[TX] %s", pszBuf);
	else DEBUG("[TX-ERR] %s", pszBuf);
#endif
        free(pszBuf);

	return nSent;
}

ssize_t streamPuts(int nSockFd, const char *pszStr) {
	ssize_t nSent = qIoPuts(nSockFd, pszStr, 0);

#ifdef BUILD_DEBUG
	if(nSent > 0) DEBUG("[TX] %s", pszStr);
	else DEBUG("[TX-ERR] %s", pszStr);
#endif

	return nSent;
}

ssize_t streamStackOut(int nSockFd, Q_OBSTACK *obstack) {
	ssize_t nWritten = obstack->writeFinal(obstack, nSockFd);

#ifdef BUILD_DEBUG
	if(g_debug) {
		char *pszStr = (char *)obstack->getFinal(obstack, NULL);
		if(pszStr != NULL) {
			if(nWritten > 0) DEBUG("[TX] %s", pszStr);
			else DEBUG("[TX-ERR] %s", pszStr);
			free(pszStr);
		}
	}
#endif

	return nWritten;
}

ssize_t streamWrite(int nSockFd, const void *pszBuffer, size_t nSize, int nTimeoutMs) {
	ssize_t nWritten = qIoWrite(nSockFd, pszBuffer, nSize, nTimeoutMs);
	DEBUG("[TX] (binary, written/request=%zd/%zu bytes)", nWritten, nSize);

	return nWritten;
}

ssize_t streamWritev(int nSockFd,  const struct iovec *pVector, int nCount) {
	ssize_t nWritten = writev(nSockFd, pVector, nCount);
	DEBUG("[TX] (binary, written=%zd bytes, %d vectors)", nWritten, nCount);

	return nWritten;
}

off_t streamSend(int nSockFd, int nFd, off_t nSize, int nTimeoutMs) {
	off_t nSent = qIoSend(nSockFd, nFd, nSize, nTimeoutMs);
	DEBUG("[TX] (send %jd/%jd bytes)", nSent, nSize);

	return nSent;
}
