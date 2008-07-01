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

const char *httpHeaderGetValue(Q_ENTRY *entries, const char *name) {
	return (char*)qEntryGetStrCase(entries, name);
}

int httpHeaderGetInt(Q_ENTRY *entries, const char *name) {
	return qEntryGetIntCase(entries, name);
}

bool httpHeaderHasString(Q_ENTRY *entries, const char *name, const char *str) {
	const char *pszVal;

	pszVal = qEntryGetStrCase(entries, name);
	if(pszVal == NULL) return false;

	if(qStrCaseStr(pszVal, str) != NULL) return true;
	return false;
}

bool httpHeaderParseRange(const char *pszRangeHeader, size_t nFilesize, off_t *pnRangeOffset1, off_t *pnRangeOffset2, size_t *pnRangeSize) {
	if(pszRangeHeader == NULL) return false;

	char *pszRange = strdup(pszRangeHeader);
	char *p1, *p2, *p3;

	p1 = strstr(pszRange, "=");
	p2 = strstr(pszRange, "-");
	p3 = strstr(pszRange, ",");

	// 문법 체크
	if(p1 == NULL || p2 == NULL || p2 < p1) {
		free(pszRange);
		return false;
	}

	// 멀티레인지는 지원치 않음
	if(p3 == NULL) p3 = pszRange + strlen(pszRange);
	else *p3 = '\0';


	// 해석
	p1 += 1;
	*p2 = '\0';

	// start
	if(p1 == p2) *pnRangeOffset1 = 0;
	else *pnRangeOffset1 = (off_t)atoll(p1);

	// end
	p2 += 1;
	if(p2 == p3) *pnRangeOffset2 = nFilesize - 1;
	else *pnRangeOffset2 = (off_t)atoll(p2);

	*pnRangeSize = (*pnRangeOffset2 - *pnRangeOffset1) + 1;
	free(pszRange);

	if(*pnRangeOffset1 > *pnRangeOffset2) return false;
	return true;
}
