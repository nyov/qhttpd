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

/*
 * ��Ʈ���� �б� ���ۿ� ���� �� �ִ� �ڷᰡ �ִ��� ����
 *
 * @param nTimeoutMs	ms>0 timeout �ð�
 *			  0  non-block ����Ȯ��
 *			ms<0 ������ ��ٸ�
 *
 * @return	n>0 �������� ���� ���
 *		0 Ÿ�Ӿƿ�
 *		-1 ����(���� ���� ��)
 *
 */

/**
 * ��Ʈ���� �д´�.
 *
 * @return	n>0 ���� ����Ʈ ��
 *		0 Ÿ�Ӿƿ���
 *		-1 ����(���� ���� ��)
 */
ssize_t streamRead(void *pszBuffer, int nSockFd, size_t nSize, int nTimeoutMs) {
	ssize_t nReaded = qSocketRead(pszBuffer, nSockFd, nSize, nTimeoutMs);
	if(nReaded > 0) DEBUG("[RX] (binary, readed %zd bytes)", nReaded);
	return nReaded;
}

/**
 * ��Ʈ������ ������ �д´�.
 *
 * @return	n>0 ���� ����Ʈ ��
 *		0 Ÿ�Ӿƿ���
 *		-1 ����(���� ���� ��)
 */
ssize_t streamGets(char *pszStr, int nSockFd, size_t nSize, int nTimeoutMs) {
	ssize_t nReaded = qSocketGets(pszStr, nSockFd, nSize, nTimeoutMs);
	if(nReaded > 0) DEBUG("[RX] %s", pszStr);
	return nReaded;
}

/**
 * ��Ʈ������ n Byte�� �б� �õ��Ѵ�.
 *
 * @return	n>0 ���� ����Ʈ ��
 *		0 Ÿ�Ӿƿ���
 *		-1 ����(���� ���� ��)
 */
ssize_t streamGetb(char *pszBuffer, int nSockFd, size_t nSize, int nTimeoutMs) {
	ssize_t nReaded = qSocketSaveIntoMemory(pszBuffer, nSockFd, nSize, nTimeoutMs);
	DEBUG("[RX] (binary, readed/request=%zd/%zu bytes)", nReaded, nSize);
	return nReaded;
}

/*
 * ��Ʈ������ ���˵� ���ڿ��� ������.
 *
 * @return	���� ����Ʈ ��, ���н� -1
 */
ssize_t streamPrintf(int nSockFd, const char *format, ...) {
	char szBuf[1024];
	va_list arglist;

	va_start(arglist, format);
	vsnprintf(szBuf, sizeof(szBuf)-1, format, arglist);
	szBuf[sizeof(szBuf)-1] = '\0';
	va_end(arglist);

	ssize_t nSent = qSocketWrite(nSockFd, szBuf, strlen(szBuf));
	if(nSent >= 0) DEBUG("[TX] %s", qStrTrim(szBuf));
	else DEBUG("[TX-ERR] %s", qStrTrim(szBuf));

	return nSent;
}

/**
 * ��Ʈ���� LF�� ���ٿ� ������ ������.
 *
 * @return	n>=0 ���� ����Ʈ ��
 *		-1 ����(���� ���� ��)
 */
ssize_t streamPuts(int nSockFd, const char *pszStr) {
	ssize_t nSent = qSocketPuts(nSockFd, pszStr);
	if(nSent >= 0) DEBUG("[TX] %s", pszStr);
	else DEBUG("[TX-ERR] %s", pszStr);
	return nSent;
}

ssize_t streamWrite(int nSockFd, const void *pszBuffer, size_t nSize) {
	ssize_t nSent = qSocketWrite(nSockFd, pszBuffer, nSize);
	DEBUG("[TX] (binary, sent/request=%zd/%zu bytes)\n%s", nSent, nSize, (char *)pszBuffer);
	return nSent;
}

/**
 * ��Ʈ������ n Byte�� �о�, ���Ϸ� �����Ѵ�.
 *
 * @return	n>0 ���� ����Ʈ ��
 *		0 timeout
 *		n<0 ����
 */
off_t streamSave(int nFd, int nSockFd, off_t nSize, int nTimeoutMs) {
	off_t nSaved = qSocketSaveIntoFile(nFd, nSockFd, nSize, nTimeoutMs);
	DEBUG("[RX] (save %jd/%jd bytes)", nSaved, nSize);
	return nSaved;
}

off_t streamSendfile(int nSockFd, int nFd, off_t nOffset, off_t nSize) {
	off_t nSent = qSocketSendfile(nSockFd, nFd, nOffset, nSize);
        DEBUG("[TX] (sendfile %jd/%jd/%jd bytes)", nSent, nOffset, nSize);
	return nSent;
}
