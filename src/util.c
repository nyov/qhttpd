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

/**
 * IP 주소를 숫자로 변환한다.
 *
 * @param szIp	도트로 구분된 IP 주소 문자열
 *
 * @return IP주소에 대한 정수값, 실패시 0
 */
unsigned int convIp2Uint(char *szIp) {
	char szBuf[15+1];
	char *pszToken;
	int nTokenCnt = 0;
	unsigned int nAddr = 0;

	// check length
	if (strlen(szIp) > 15) return 0;

	// copy to buffer
	strcpy(szBuf, szIp);
	for (pszToken = qStrtok(szBuf, ".", NULL); pszToken != NULL; pszToken = qStrtok(NULL, ".", NULL)) {
		nTokenCnt++;

		if (nTokenCnt == 1) nAddr += (unsigned int)(atoi(pszToken)) * 0x1000000;
		else if (nTokenCnt == 2) nAddr += (unsigned int)(atoi(pszToken)) * 0x10000;
		else if (nTokenCnt == 3) nAddr += (unsigned int)(atoi(pszToken)) * 0x100;
		else if (nTokenCnt == 4) nAddr += (unsigned int)(atoi(pszToken));
		else return -1;
		// unsigned로 변경
	}
	if (nTokenCnt != 4) return 0;

	return nAddr;
}


/**
 * usleep()의 다른 구현으로 차이점은
 * ㄱ) 인터럽트시에 못잔 시간만큼 한번 더 시도하는 점
 * ㄴ) 덜 잤을시엔 못 잔 시간을 복귀함
 *
 * @return 성공시 0, 덜 잤으면 남은 시간 (microsec)
 */
unsigned long int microSleep(unsigned long int microsec) {
	struct timespec req, rem;

	req.tv_sec = microsec / 1000000;
        req.tv_nsec = (microsec % 1000000) * 1000;

	if (nanosleep(&req, &rem) == 0) return 0;
	else if (nanosleep(&rem, &req) == 0) return 0; // 다 못잤으면 한번만 더 잠
	return ((req.tv_sec * 1000000) + (req.tv_nsec / 1000)); // 두번 잤는데도 다 못 자면 못잔 시간 복귀
}

/**
 * unsigned long long으로 문자열을 변환한다.
 * atoll()과의 차이점은
 * ㄱ) unsigned 범위를 지원한다는 점이고
 * ㄴ) 변환중에 문자가 나올경우엔 아예 0을 복귀한다는 점
 * ㄷ) overflow가 발생될 경우 0을 복귀한다는 점
 *
 * @return unsigned long long
 */
uint64_t convStr2Uint64(char *szNumStr) {
	uint64_t n, tmp;

	if(szNumStr == NULL) {
		DEBUG("Null pointer input.");
		return 0;
	}
	for (n = 0; *szNumStr != '\0'; szNumStr++) {
		if(*szNumStr < '0' || *szNumStr > '9') return 0;
		tmp = n * 10 + (*szNumStr - '0');
		if(tmp < n) return 0; // overflow
		n = tmp;
	}

	return n;
}

/**
 * 파일이름에서 확장자를 얻는다. 확장자는 소문자로 변환되며,
 * 6자 이상의 확장자는 무시된다.
 *
 * @return 파일의 확장자 문자열 포인터, 확장자가 없으면 NULL.
 *
 * @note 리턴 문자열은 사용자가 free 해 줘야함.
 */
char *getExtestionFromFilename(char *szFilename, bool bIncludeDot) {
	int i;

	for(i = strlen(szFilename) - 1; i >= 0; i--) {
		if(szFilename[i] == '.') {
			char *pszExt = szFilename + i + 1;
			int nLen = strlen(pszExt);

			// 확장자가 너무 길거나 특수문자가 있으면 없는걸로 침
			if(nLen > 6 || qStr09AZaz(pszExt) == false) break;

			char *pszNewExt = (char *)malloc(nLen+1+1);
			if(bIncludeDot == true) strcpy(pszNewExt, pszExt-1);
			else strcpy(pszNewExt, pszExt);
			qStrlwr(pszNewExt);
			return pszNewExt;
		} else if(szFilename[i] == '/') break;
	}

	return strdup("");
}

float diffTimeval(struct timeval *t1, struct timeval *t0) {
	struct timeval nowt;
	float nDiff;

	if(t1 == NULL) {
		gettimeofday(&nowt, NULL);
		t1 = &nowt;
	}

	nDiff = t1->tv_sec - t0->tv_sec;
	if(t1->tv_usec - t0->tv_usec != 0) nDiff += (float)(t1->tv_usec - t0->tv_usec) / 1000000;

	//DEBUG("%ld.%ld %ld.%ld", t1->tv_sec, t1->tv_usec, t0->tv_sec, t0->tv_usec);

	return nDiff;
}


/**
 * 파일명/폴더명이 정확한지 체크
 */
 bool isCorrectFilename(char *pszPath) {
	if(pszPath == NULL) return false;

	int nLen = strlen(pszPath);
	if(nLen == 0 || nLen > MAX_FILENAME_LENGTH) return false;
	else if(qStrtok(pszPath, "\\/:*?\"<>|", NULL) != pszPath) return false;
	return true;
}


/**
 * 경로가 정확한지 체크
 */
bool isCorrectPath(char *pszPath) {
	if(pszPath == NULL) return false;

	int nLen = strlen(pszPath);
	if(nLen == 0 || nLen > MAX_FILEPATH_LENGTH) return false;
	else if(pszPath[0] != '/') return false;
	else if(qStrtok(pszPath, "\\:*?\"<>|", NULL) != pszPath) return false;

	// 슬래쉬로 구분되는 각 폴더명의 최대 길이 체크
	char *t;
	int n;
	for(n = 0, t = pszPath; *t != '\0'; t++) {
		if(*t == '/') {
			n = 0;
			continue;
		}

		if(n == 0 && *t == '.') return false;	// 폴더명의 첫글자가 . 이면 오류
		else if(n >= MAX_FILENAME_LENGTH) return false;
		n++;
	}

	return true;
}

/**
 * 경로 조정.
 * nsIsCorrectPath()를 한번 하고 호출한다는 전제
 */
void correctPath(char *pszPath) {
	// 공백 제거
	qRemoveSpace(pszPath);

	// 중복 슬래쉬 제거
	while(strstr(pszPath, "//") != NULL) qStrReplace("sr", pszPath, "//", "/");

	// 테일링 슬래쉬 제거
	int nLen = strlen(pszPath);
	if(nLen <= 1) return;
	if(pszPath[nLen - 1] == '/') pszPath[nLen - 1] = '\0';
}
