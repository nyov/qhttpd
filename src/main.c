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

	// hook
	if(hookBeforeMainInit() == false) {
		return EXIT_FAILURE;
	}

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

		// config hook
		if(hookAfterConfigLoaded() == false) {
			fprintf(stderr, "Hook failed.\n");
			return EXIT_FAILURE;
		}
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
