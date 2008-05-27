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
// PUBLIC FUNCTIONS
/////////////////////////////////////////////////////////////////////////

void daemonStart(bool nDaemonize) {
	int nSockFd = 0;
	struct sockaddr_in svrAddr;     // server address information

	// init signal
	daemonSignalInit(daemonSignal);

	// open log
	g_errlog = qLogOpen(g_conf.szLogDir, g_conf.szErrorLog, g_conf.nLogRotate, true);
	g_acclog = qLogOpen(g_conf.szLogDir, g_conf.szAccessLog, g_conf.nLogRotate, true);
	if (g_errlog == NULL) {
		printf("Can't open error log file %s/%s\n", g_conf.szLogDir, g_conf.szErrorLog);
		daemonEnd(EXIT_FAILURE);
	} else if (g_acclog == NULL) {
		printf("Can't open access log file %s/%s\n", g_conf.szLogDir, g_conf.szAccessLog);
		daemonEnd(EXIT_FAILURE);
	}

	qLogSetConsole(g_errlog, true);
	qLogSetConsole(g_acclog, false);

	LOG_INFO("Initializing %s...", PRG_NAME);

	// load mime
	if(mimeInit(g_conf.szMimeFile) == false) {
		LOG_WARN("Can't load mimetypes from %s", g_conf.szMimeFile);
	}

	// init socket
	if ((nSockFd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		LOG_ERR("Can't create socket.");
		daemonEnd(EXIT_FAILURE);
	} else {
		int so_reuseaddr = 1;
		int so_sndbufsize = 0;
		int so_rcvbufsize = 0;

		if (so_reuseaddr) setsockopt(nSockFd, SOL_SOCKET, SO_REUSEADDR, &so_reuseaddr, sizeof(so_reuseaddr));
		if (so_sndbufsize > 0) setsockopt(nSockFd, SOL_SOCKET, SO_SNDBUF, &so_sndbufsize, sizeof(so_sndbufsize));
		if (so_rcvbufsize > 0) setsockopt(nSockFd, SOL_SOCKET, SO_RCVBUF, &so_rcvbufsize, sizeof(so_rcvbufsize));

		// set to non-block socket
                fcntl(nSockFd, F_SETFL, O_NONBLOCK);
	}

	// set information
	svrAddr.sin_family = AF_INET;         // host byte order
	svrAddr.sin_port = htons(g_conf.nPort);  // short, network byte order
	svrAddr.sin_addr.s_addr = INADDR_ANY; // auto-fill with my IP
	memset((void *)&(svrAddr.sin_zero), 0, sizeof(svrAddr.sin_zero)); // zero the rest of the struct

	// bind
	if (bind(nSockFd, (struct sockaddr *)&svrAddr, sizeof(struct sockaddr)) == -1) {
		LOG_ERR("Can't bind port %d (errno: %d)", g_conf.nPort, errno);
		daemonEnd(EXIT_FAILURE);
	}
	LOG_INFO("  - Binding port %d succeed.", g_conf.nPort);

	// entering daemon mode
	if (nDaemonize) {
		LOG_INFO("Entering daemon mode.");
		daemon(false, false); // after this line, parent's pid will be changed.
		qLogSetConsole(g_errlog, false); // do not screen out any more
	}

	// save pid
	if(qCountSave(g_conf.szPidfile, getpid()) == false) {
		LOG_ERR("Can't create pid file.");
		daemonEnd(EXIT_FAILURE);
	}

	// init semaphore
	if ((g_semid = qSemInit(g_conf.szPidfile, 's', MAX_SEMAPHORES, true)) < 0) {
		LOG_ERR("Can't initialize semaphore.");
		daemonEnd(EXIT_FAILURE);
	}
	LOG_INFO("  - Semaphore created.");

	// init shared memory
	if (poolInit(g_conf.nMaxClients) == false) {
		LOG_ERR("Can't initialize child management pool.");
		daemonEnd(EXIT_FAILURE);
	}
	LOG_INFO("  - Child management pool created.");

	// after init hook
	if(hookAfterDaemonInit() == false) {
		LOG_ERR("Hook failed.");
		daemonEnd(EXIT_FAILURE);
	}

	// listen
	if (listen(nSockFd, g_conf.nMaxpending) == -1) {
		LOG_ERR("Can't listen port %d.", g_conf.nPort);
		daemonEnd(EXIT_FAILURE);
	}

	// starting.
	LOG_INFO("%s %s is ready on the port %d.", PRG_NAME, PRG_VERSION, g_conf.nPort);

	// prefork management
	while (true) {
		int nTotalLaunched = poolGetTotalLaunched();
		int nCurrentChilds = poolGetCurrentChilds();
		int nWorkingChilds = poolGetWorkingChilds();
		int nIdleChilds = nCurrentChilds - nWorkingChilds;

		// increase or decrease childs
		int nChildFlag = 0;
		if(nCurrentChilds < g_conf.nStartServers) { // should be launched at least start servers
			nChildFlag = 1;
		} else {
			if(nIdleChilds < g_conf.nMinSpareServers) { // not enough idle childs
				if(nCurrentChilds < g_conf.nMaxClients) nChildFlag = 1;
			} else if(nIdleChilds > g_conf.nMaxSpareServers) { // too much idle childs
				if(nCurrentChilds > g_conf.nStartServers) nChildFlag = -1;
			}
		}

		//DEBUG("%d %d %d %d", nCurrentChilds, nWorkingChilds, nIdleChilds, nChildFlag);

		// launch or kill childs
		if(nChildFlag > 0) {
			// 예비 차일드를 생성
			DEBUG("Launching spare server. (working:%d, total:%d)", nWorkingChilds, nCurrentChilds);
			int nCpid = fork();
			if (nCpid < 0) { // 오류
				LOG_ERR("Can't create child.");
				sleep(1);
			} else if (nCpid == 0) { // 자식
				DEBUG("Child %d launched", getpid());

				// 차일드 메인
				childStart(nSockFd);

				// 이 코드는 안전장치로, 절대 수행되지 않음.
				exit(EXIT_FAILURE);

			} else { // 부모
				int nWait;

				// 차일드가 공유슬롯에 등록되기를 기다림
				for(nWait = 0; nWait < 1000; nWait++) {
					//DEBUG("%d %d", nTotalLaunched, poolGetTotalLaunched());
					if(nTotalLaunched != poolGetTotalLaunched()) break;
					DEBUG("Waiting child registered at pool. [%d]", nWait+1);
					microSleep(1000);
				}
				if(nWait == 1000) {
					LOG_WARN("Delayed child launching.");
				}
			}
		} else if(nChildFlag < 0) {	// 스페어 차일드 제거
			static time_t nLastSec = 0;

			// 차일드 제거는 1초에 한개씩만
			if(nLastSec != time(NULL)) {
				if(poolSetIdleExitReqeust(1) != 1) {
					LOG_WARN("Can't set exit flag.");
				}
				nLastSec = time(NULL);
			}
		} else { // 스페어 증감이 필요치 않을경우
			static time_t nLastSec = 0;

			if(nLastSec != time(NULL)) {
				// idle hook
				int nJobCnt = hookWhileDaemonIdle();
				if(nJobCnt < 0) {
					LOG_ERR("Hook failed.");
				}

				// update last called
				nLastSec = time(NULL);
			} else {
				microSleep(1 * 1000); // 1/1000 sec
			}
		}

		// signal handling
		daemonSignalHandler();
	}

	daemonEnd(EXIT_SUCCESS);
}

void daemonEnd(int nStatus) {
	static bool bAlready = false;

	if(bAlready == true) return;
	bAlready = true;

	int nCurrentChilds, nWait;
	for(nWait = 15; nWait >= 0 && (nCurrentChilds = poolGetCurrentChilds()) > 0; nWait--) {
		if(nWait > 5) {
			LOG_INFO("Soft shutting down [%d]. Waiting %d childs.", nWait-5, nCurrentChilds);
			poolSetIdleExitReqeust(nCurrentChilds);
		} else if(nWait > 0) {
			LOG_INFO("Hard shutting down [%d]. Waiting %d childs.", nWait, nCurrentChilds);
			kill(0, SIGTERM);
		} else {
			LOG_WARN("%d childs are still alive. Give up!", nCurrentChilds);
		}

		// sleep
		unsigned long int nSleepLeft = 1 * 1000 * 1000;
		while((nSleepLeft = microSleep(nSleepLeft)) > 0);

		// waitpid
		while(waitpid(-1, NULL, WNOHANG) > 0);
	}

	// hook
	if(hookBeforeDaemonEnd() == false) {
		LOG_ERR("Hook failed.");
	}

	// destroy mime
	if (mimeFree() == false) {
		LOG_WARN("Can't destroy mime types.");
	}

	// destroy shared memory
	if (poolFree() == false) {
		LOG_WARN("Can't destroy child management pool .");
	}

	// destroy semaphore
	int i;
	for(i = 0; i < MAX_SEMAPHORES; i++) qSemLeave(g_semid, i);	// force to unlock every semaphores
	if (qSemFree(g_semid) == false) {
		LOG_WARN("Can't destroy semaphore.");
	}

	// remove pid file
	if (qCheckFile(g_conf.szPidfile) == true) {
		if(unlink(g_conf.szPidfile) != 0) LOG_WARN("Can't remove pid file %s", g_conf.szPidfile);
	}

	// final
	LOG_INFO("%s Terminated.", PRG_NAME);

	// close log
	if(g_acclog != NULL) qLogClose(g_acclog);
if(g_errlog != NULL) qLogClose(g_errlog);

	exit(nStatus);
}

void daemonSignalInit(void *func) {
	// init sigaction
	struct sigaction sa;
	sa.sa_handler = func;
	sa.sa_flags = 0;
	sigemptyset (&sa.sa_mask);

	// to handle
	sigaction(SIGCHLD, &sa, NULL);
	sigaction(SIGHUP, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);

	sigaction(SIGUSR1, &sa, NULL);
	sigaction(SIGUSR2, &sa, NULL);

	// to ignore
	signal(SIGPIPE, SIG_IGN);

	// reset signal flags;
	sigemptyset(&g_sigflags);
}

void daemonSignal(int signo) {
	sigaddset(&g_sigflags, signo);
}

void daemonSignalHandler(void) {
	if(sigismember(&g_sigflags, SIGCHLD)) {
		sigdelset(&g_sigflags, SIGCHLD);
		DEBUG("Caughted SIGCHLD");

		int nChildPid;
		int nChildStatus = 0;
		while((nChildPid = waitpid(-1, &nChildStatus, WNOHANG)) > 0) {
			DEBUG("Detecting child(%d) terminated. Status : %d", nChildPid, nChildStatus);

			// if child is killed unexpectly such like SIGKILL, we remove child info here
			if (poolChildDel(nChildPid) == true) {
				LOG_WARN("Child %d killed unexpectly.", nChildPid);
			}
		}
	} else if(sigismember(&g_sigflags, SIGHUP)) {
		sigdelset(&g_sigflags, SIGHUP);
		LOG_INFO("Caughted SIGHUP");

		// reload config
		if(loadConfig(g_conf.szConfigFile, &g_conf) == false) {
			LOG_ERR("Can't reload configuration file %s", g_conf.szConfigFile);
		}

		// reload mime
		mimeFree();
		if(mimeInit(g_conf.szMimeFile) == false) {
			LOG_ERR("Can't load mimetypes from %s", g_conf.szConfigFile);
		}

		// config hook
		if(hookAfterConfigLoaded() == false || hookAfterDaemonSIGHUP() == false) {
			LOG_ERR("Hook failed.");
		}

		// re-launch childs
		poolSetExitReqeustAll();

		LOG_SYS("Reloaded.");
	} else if(sigismember(&g_sigflags, SIGTERM)) {
		sigdelset(&g_sigflags, SIGTERM);
		LOG_INFO("Caughted SIGTERM");

		daemonEnd(EXIT_SUCCESS);
	} else if(sigismember(&g_sigflags, SIGINT)) {
		sigdelset(&g_sigflags, SIGINT);
		LOG_INFO("Caughted SIGINT");

		daemonEnd(EXIT_SUCCESS);
	} else if(sigismember(&g_sigflags, SIGUSR1)) {
		sigdelset(&g_sigflags, SIGUSR1);
		LOG_INFO("Caughted SIGUSR1");

		if(g_loglevel < MAX_LOGLEVEL) g_loglevel++;
		poolSendSignal(SIGUSR1);
		LOG_SYS("Increasing log-level to %d.", g_loglevel);

	} else if(sigismember(&g_sigflags, SIGUSR2)) {
		sigdelset(&g_sigflags, SIGUSR2);
		LOG_INFO("Caughted SIGUSR2");

		if(g_loglevel > 0) g_loglevel--;
		poolSendSignal(SIGUSR2);
		LOG_SYS("Decreasing log-level to %d.", g_loglevel);
	}
}
