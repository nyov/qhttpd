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
int streamWaitReadable(int nSockFd, int nTimeoutMs) {
	return qSocketWaitReadable(nSockFd, nTimeoutMs);
}

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
ssize_t streamSave(int nFileFd, int nSockFd, size_t nSize, int nTimeoutMs) {
	ssize_t nSaved = qSocketSaveIntoFile(nFileFd, nSockFd, nSize, nTimeoutMs);
	DEBUG("[RX] (binary,wrote/request=%zd/%zu bytes)", nSaved, nSize);
	return nSaved;
}

ssize_t streamSendfile(int nSockFd, const char *pszFilePath, off_t nOffset, size_t nSize) {
	ssize_t nSent = qSocketSendfile(nSockFd, pszFilePath, nOffset, nSize);
	DEBUG("[TX] (sendfile %s %zd bytes)", pszFilePath, nSent);
	return nSent;
}
