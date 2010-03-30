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
 * $Id: http_header.c 132 2010-03-11 05:29:17Z wolkykim $
 */

#include "qhttpd.h"

const char *httpHeaderGetStr(Q_ENTRY *entries, const char *pszName) {
	return (char*)entries->getStrCase(entries, pszName, false);
}

int httpHeaderGetInt(Q_ENTRY *entries, const char *pszName) {
	return entries->getIntCase(entries, pszName);
}

bool httpHeaderSetStr(Q_ENTRY *entries, const char *pszName, const char *pszValue) {
	if(pszValue != NULL) entries->putStr(entries, pszName, pszValue, true);
	else entries->remove(entries, pszName);

	return true;
}

bool httpHeaderSetStrf(Q_ENTRY *entries, const char *pszName, const char *pszFormat, ...) {
	char *pszValue;
	DYNAMIC_VSPRINTF(pszValue, pszFormat);
	if(pszValue == NULL) return false;

	bool bRet = httpHeaderSetStr(entries, pszName, pszValue);
	free(pszValue);

	return bRet;
}

bool httpHeaderRemove(Q_ENTRY *entries, const char *pszName) {
	if(entries->remove(entries, pszName) > 0) return true;
	return false;
}

bool httpHeaderHasStr(Q_ENTRY *entries, const char *pszName, const char *pszValue) {
	const char *pszVal = entries->getStrCase(entries, pszName, false);
	if(pszVal == NULL) return false;

	if(strcasestr(pszVal, pszValue) != NULL) return true;
	return false;
}

bool httpHeaderParseRange(const char *pszRangeHeader, off_t nFilesize, off_t *pnRangeOffset1, off_t *pnRangeOffset2, off_t *pnRangeSize) {
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

bool httpHeaderSetExpire(Q_ENTRY *entries, int nExpire) {
	// cache control
	if(nExpire > 0) {
		httpHeaderSetStrf(entries, "Cache-Control", "max-age=%d", nExpire);
		httpHeaderSetStrf(entries, "Expires", "%s", qTimeGetGmtStaticStr(time(NULL) + nExpire));
		return true;
	}

	return false;
}
