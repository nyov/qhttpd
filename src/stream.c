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
 * 스트림의 읽기 버퍼에 읽을 수 있는 자료가 있는지 조사
 *
 * @param nTimeoutMs	ms>0 timeout 시간
 *			  0  non-block 상태확인
 *			ms<0 무한정 기다림
 *
 * @return	n>0 읽을것이 있을 경우
 *		0 타임아웃
 *		-1 오류(연결 끊김 등)
 *
 */
int streamWaitReadable(int nSockFd, int nTimeoutMs) {
	return qSocketWaitReadable(nSockFd, nTimeoutMs);
}

/**
 * 스트림을 읽는다.
 *
 * @return	n>0 읽은 바이트 수
 *		0 타임아웃시
 *		-1 에러(연결 끊김 등)
 */
ssize_t streamRead(void *pszBuffer, int nSockFd, size_t nSize, int nTimeoutMs) {
	ssize_t nReaded = qSocketRead(pszBuffer, nSockFd, nSize, nTimeoutMs);
	if(nReaded > 0) DEBUG("[RX] (binary, readed %zd bytes)", nReaded);
	return nReaded;
}

/**
 * 스트림에서 한줄을 읽는다.
 *
 * @return	n>0 읽은 바이트 수
 *		0 타임아웃시
 *		-1 에러(연결 끊김 등)
 */
ssize_t streamGets(char *pszStr, int nSockFd, size_t nSize, int nTimeoutMs) {
	ssize_t nReaded = qSocketGets(pszStr, nSockFd, nSize, nTimeoutMs);
	if(nReaded > 0) DEBUG("[RX] %s", pszStr);
	return nReaded;
}

/**
 * 스트림에서 n Byte를 읽기 시도한다.
 *
 * @return	n>0 읽은 바이트 수
 *		0 타임아웃시
 *		-1 에러(연결 끊김 등)
 */
ssize_t streamGetb(char *pszBuffer, int nSockFd, size_t nSize, int nTimeoutMs) {
	ssize_t nReaded = qSocketSaveIntoMemory(pszBuffer, nSockFd, nSize, nTimeoutMs);
	DEBUG("[RX] (binary, readed/request=%zd/%zu bytes)", nReaded, nSize);
	return nReaded;
}

/*
 * 스트림으로 포맷된 문자열을 보낸다.
 *
 * @return	보낸 바이트 수, 실패시 -1
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
 * 스트림에 LF를 덧붙여 한줄을 보낸다.
 *
 * @return	n>=0 보낸 바이트 수
 *		-1 에러(연결 끊김 등)
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
 * 스트림에서 n Byte를 읽어, 파일로 저장한다.
 *
 * @return	n>0 읽은 바이트 수
 *		0 timeout
 *		n<0 에러
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
