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
	fprintf(stderr, "Usage: %s [-h] [-d] [-D] -c configfile\n", g_prgname);
	fprintf(stderr, "  -c filepath	Set configuration file.\n");
	fprintf(stderr, "  -d		Run as debugging mode.\n");
	fprintf(stderr, "  -D		Do not daemonize.\n");
	fprintf(stderr, "  -v		Version info.\n");
	fprintf(stderr, "  -h		Display this help message and exit.\n");
}

void printVersion(void) {
#ifdef BUILD_DEBUG
#define	MSG_BUILD	"DEBUG"
#else
#define	MSG_BUILD	"RELEASE"
#endif

#ifdef _LARGEFILE_SOURCE
#define	MSG_LFS		" --enable-lfs"
#else
#define	MSG_LFS		""
#endif

#ifdef ENABLE_LUA
#define	MSG_LUA		" --enable-lua"
#else
#define	MSG_LUA		""
#endif

#ifdef ENABLE_HOOK
#define	MSG_HOOK	" --enable-hook"
#else
#define	MSG_HOOK	""
#endif

	fprintf(stderr, "%s v%s (%s; %s; " MSG_BUILD ";" MSG_LFS MSG_LUA MSG_HOOK ")\n",
	    g_prgname, g_prgversion, __DATE__, __TIME__);
}
