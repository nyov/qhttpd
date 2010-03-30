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

/////////////////////////////////////////////////////////////////////////
// GLOBAL VARIABLES
/////////////////////////////////////////////////////////////////////////
char*		g_prginfo = PRG_INFO;
char*		g_prgname = PRG_NAME;
char*		g_prgversion = PRG_VERSION;

bool		g_debug = false;	// debug message on/off flag
sigset_t	g_sigflags;		// signals received

struct ServerConfig	g_conf;		// configuration structure
int		g_semid = -1;		// semaphore id
Q_LOG*		g_errlog = NULL;	// error log
Q_LOG*		g_acclog = NULL;	// access log
int		g_loglevel = 0;		// log level

/////////////////////////////////////////////////////////////////////////
// PUBLIC FUNCTIONS
/////////////////////////////////////////////////////////////////////////

/**
 * Egis Positioning Daemon Daemon
 *
 * @author Seung-young Kim
 */
int main(int argc, char *argv[]) {
	char szConfigFile[PATH_MAX];
	bool nDaemonize = true;

#ifdef ENABLE_HOOK
	if(hookBeforeMainInit() == false) {
		return EXIT_FAILURE;
	}
#endif
	// initialize
	qStrCpy(szConfigFile, sizeof(szConfigFile), DEF_CONFIG);

	// parse command line arguments
	int nOpt;
	while ((nOpt = getopt(argc, argv, "hvdDc:")) != -1) {
		switch (nOpt) {
			case 'd': {
				#ifndef BUILD_DEBUG
				fprintf(stderr, "ERROR: Sorry, does not support debugging mode. Please re-compile with -DBUILD_DEBUG\n");
				return EXIT_FAILURE;
				#endif

				fprintf(stderr, "Entering debugging mode.\n");
				g_debug = true; // common �ҽ��� ������� ��
				g_loglevel  = MAX_LOGLEVEL;
				nDaemonize = false;
				break;
			}
			case 'D': {
				fprintf(stderr, "Entering console mode.\n");
				nDaemonize = false;
				break;
			}
			case 'c': {
				qStrCpy(szConfigFile, sizeof(szConfigFile), optarg);
				break;
			}
			case 'v': {
				printVersion();
				return EXIT_SUCCESS;
			}
			case 'h': {
				printUsages();
				return EXIT_SUCCESS;
			}
			default: {
				printUsages();
				return EXIT_FAILURE;
			}
		}
	}

	// parse configuration
	memset((void*)&g_conf, 0, sizeof(g_conf));
	bool bConfigLoadStatus = loadConfig(&g_conf, szConfigFile);
#ifdef ENABLE_HOOK
	bConfigLoadStatus = hookAfterConfigLoaded(&g_conf, bConfigLoadStatus);
#endif
	if(bConfigLoadStatus == false) {
		fprintf(stderr, "ERROR: An error occured while loading configuration \"%s\".\n", szConfigFile);
		fprintf(stderr, "       Try using '-c' option. Use '-h' option for help message.\n");
		return EXIT_FAILURE;
	}

	// check & parse config
	if(checkConfig(&g_conf) == false) {
		fprintf(stderr, "ERROR: Failed to verify configuration.");
		return EXIT_FAILURE;
	}

	// check pid file
	if(qFileExist(g_conf.szPidFile) == true) {
		fprintf(stderr, "ERROR: pid file(%s) exists. already running?\n", g_conf.szPidFile);
		return EXIT_FAILURE;
	}

	// set log level
	if(g_loglevel == 0) g_loglevel = g_conf.nLogLevel;

	// start daemon services
	daemonStart(nDaemonize); // never return

	return EXIT_SUCCESS;
}
