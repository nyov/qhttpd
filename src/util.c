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

unsigned int convIp2Uint(const char *szIp) {
	char szBuf[15+1];
	char *pszToken;
	int nTokenCnt = 0;
	unsigned int nAddr = 0;

	// check length
	if (strlen(szIp) > 15) return 0;

	// copy to buffer
	strcpy(szBuf, szIp);
	for (pszToken = strtok(szBuf, "."); pszToken != NULL; pszToken = strtok(NULL, ".")) {
		nTokenCnt++;

		if (nTokenCnt == 1) nAddr += (unsigned int)(atoi(pszToken)) * 0x1000000;
		else if (nTokenCnt == 2) nAddr += (unsigned int)(atoi(pszToken)) * 0x10000;
		else if (nTokenCnt == 3) nAddr += (unsigned int)(atoi(pszToken)) * 0x100;
		else if (nTokenCnt == 4) nAddr += (unsigned int)(atoi(pszToken));
		else return -1;
		// unsigned�� ����
	}
	if (nTokenCnt != 4) return 0;

	return nAddr;
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
 * ��ΰ� ��Ȯ���� üũ
 */
bool isCorrectPath(const char *pszPath) {
	if(pszPath == NULL) return false;

	int nLen = strlen(pszPath);
	if(nLen == 0 || nLen >= MAX_FILEPATH_LENGTH) return false;
	else if(pszPath[0] != '/') return false;
	else if(strpbrk(pszPath, "\\:*?\"<>|") != NULL) return false;

	// �������� ���еǴ� �� �������� �ִ� ���� üũ
	char *t;
	int n;
	for(n = 0, t = (char *)pszPath; *t != '\0'; t++) {
		if(*t == '/') {
			n = 0;
			continue;
		}

		if(n == 0 && *t == '.') return false;	// �������� ù���ڰ� . �̸� ����
		else if(n >= MAX_FILENAME_LENGTH) {
			DEBUG("Filename too long.");
			return false;
		}
		n++;
	}

	return true;
}

/**
 * ��� ����.
 * nsIsCorrectPath()�� �ѹ� �ϰ� ȣ���Ѵٴ� ����
 */
void correctPath(char *pszPath) {
	// ���� ����
	qStrTrim(pszPath);

	// �ߺ� ������ ����
	while(strstr(pszPath, "//") != NULL) qStrReplace("sr", pszPath, "//", "/");

	// ���ϸ� ������ ����
	int nLen = strlen(pszPath);
	if(nLen <= 1) return;
	if(pszPath[nLen - 1] == '/') pszPath[nLen - 1] = '\0';
}
