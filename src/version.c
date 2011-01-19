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

void printUsages(void) {
	printVersion();
	fprintf(stderr, "Usage: %s [-dDVh] [-c configfile]\n", g_prgname);
	fprintf(stderr, "  -c filepath	Set configuration file.\n");
	fprintf(stderr, "  -d		Run as debugging mode.\n");
	fprintf(stderr, "  -D		Do not daemonize.\n");
	fprintf(stderr, "  -V		Version info.\n");
	fprintf(stderr, "  -h		Display this help message and exit.\n");
}

void printVersion(void) {
	char *verstr = getVersion();
	fprintf(stderr, "%s\n", verstr);
	free(verstr);
}

char *getVersion(void) {
	Q_VECTOR *ver = qVector();

	ver->addStrf(ver, "%s v%s", g_prgname, g_prgversion);
	ver->addStrf(ver, " (%s %s;", __DATE__, __TIME__);

#ifdef ENABLE_DEBUG
	ver->addStr(ver, " DEBUG");
#else
	ver->addStr(ver, " RELEASE");
#endif

#ifdef ENABLE_LFS
	ver->addStr(ver, " --enable-lfs");
#endif

#ifdef ENABLE_LUA
	ver->addStr(ver, " --enable-lua");
#endif

#ifdef ENABLE_HOOK
	ver->addStr(ver, " --enable-hook");
#endif

	ver->addStr(ver, ")");

	char *final = ver->toString(ver);
	ver->free(ver);

	return final;
}
