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

	// grammer check
	if(p1 == NULL || p2 == NULL || p2 < p1) {
		free(pszRange);
		return false;
	}

	// multi-range is not supported
	if(p3 == NULL) p3 = pszRange + strlen(pszRange);
	else *p3 = '\0';


	// parse
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
