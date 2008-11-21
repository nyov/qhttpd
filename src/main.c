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

/////////////////////////////////////////////////////////////////////////
// GLOBAL VARIABLES
/////////////////////////////////////////////////////////////////////////
bool	g_debug = false;		// common 소스의 디버깅을 켜고 끔

Config	g_conf;				// 설정파일 구조체
int	g_semid = -1;			// 세마포ID
Q_LOG	*g_errlog = NULL;		// 에러로그 구조체 포인터
Q_LOG	*g_acclog = NULL;		// 전송로그 구조체 포인터
int	g_loglevel = 0;			// 로그 레벨
sigset_t g_sigflags;			// 처리할 시그널 셋

/////////////////////////////////////////////////////////////////////////
// PUBLIC FUNCTIONS
/////////////////////////////////////////////////////////////////////////

/**
 * Egis Positioning Daemon Daemon
 *
 * @author Seung-young Kim
 */
int main(int argc, char *argv[]) {
	char szConfigFile[MAX_PATH_LEN];
	bool nDaemonize = true;

#ifdef ENABLE_HOOK
	if(hookBeforeMainInit() == false) {
		return EXIT_FAILURE;
	}
#endif
	// initialize
	strcpy(szConfigFile, "");

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
				g_debug = true; // common 소스의 디버깅을 켬
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
				strcpy(szConfigFile, optarg);
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
	if (loadConfig(&g_conf, szConfigFile)) {
		//fprintf(stderr, "Configuration loaded.\n");

#ifdef ENABLE_HOOK
		// config hook
		if(hookAfterConfigLoaded() == false) {
			fprintf(stderr, "Hook failed.\n");
			return EXIT_FAILURE;
		}
#endif
	} else {
		fprintf(stderr, "ERROR: Can't load configuration file %s\n", szConfigFile);
		printUsages();
		return EXIT_FAILURE;
	}

	// check pid file
	if(qFileExist(g_conf.szPidfile) == true) {
		fprintf(stderr, "ERROR: pid file(%s) exists. already running?\n", g_conf.szPidfile);
		return EXIT_FAILURE;
	}

	// set log level
	if(g_loglevel == 0) g_loglevel = g_conf.nLogLevel;

	// start daemon services
	daemonStart(nDaemonize); // never return

	return EXIT_SUCCESS;
}
