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
	if(mimeInit(g_conf.szMimeFile, g_conf.szMimeDefault) == false) {
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
	bzero(&(svrAddr.sin_zero), 8);        // zero the rest of the struct

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
		int nChildFlag = 0;

		int nTotalLaunched = poolGetTotalLaunched();
		int nCurrentChilds = poolGetCurrentChilds();
		int nWorkingChilds = poolGetWorkingChilds();
		int nIdleChilds = nCurrentChilds - nWorkingChilds;

		if(nCurrentChilds < g_conf.nMaxClients) {
			if(nCurrentChilds < g_conf.nStartServers) {
				nChildFlag = 1;
			} else {
				if(nIdleChilds < g_conf.nMinSpareServers) nChildFlag = 1;
				else if(nCurrentChilds > g_conf.nStartServers
					&& nIdleChilds > g_conf.nMaxSpareServers) nChildFlag = -1;
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
					microSleep(100);
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
				// 세마포 데드락 체크
				static int nSemLockCnt[MAX_SEMAPHORES];
				int i;
				for(i = 0; i < MAX_SEMAPHORES; i++) {
					if(qSemCheck(g_semid, i) == true) {
						nSemLockCnt[i]++;
						if(nSemLockCnt[i] > MAX_MAX_SEMAPHORES_LOCK_SECS) {
							LOG_ERR("Force to unlock semaphore no %d", i);
							qSemLeave(g_semid, i);	// force to unlock
							nSemLockCnt[i] = 0;
						}
					} else {
						nSemLockCnt[i] = 0;
					}
				}

				// 훅
				int nJobCnt = hookWhileDaemonIdle();
				if(nJobCnt < 0) {
					LOG_ERR("Hook failed.");
				}

				// 실행시간 갱신
				nLastSec = time(NULL);
			} else {
				microSleep(1 * 1000);
			}


		}
	}

	daemonEnd(EXIT_SUCCESS);
}

void daemonEnd(int nStatus) {
	static bool bAlready = false;

	if(bAlready == true) return;
	bAlready = true;

	int nCurrentChilds, nWait;
	for(nWait = 0; nWait < 15 && (nCurrentChilds = poolGetCurrentChilds()) > 0; nWait++) {
		LOG_INFO("Waiting childs. %d actives.", nCurrentChilds);
		poolSetIdleExitReqeust(nCurrentChilds);
		microSleep(500*1000);
	}
	for(nWait = 0; nWait < 3 && (nCurrentChilds = poolGetCurrentChilds()) > 0; nWait++) {
		LOG_WARN("%d childs are still alive. Sending termination signal.", nCurrentChilds);
		int nSigSent = poolSendSignal(SIGTERM);
		LOG_INFO("Sending termination signal to childs %d/%d", nSigSent, nCurrentChilds);
		sleep(1);
	}
	if((nCurrentChilds = poolGetCurrentChilds()) > 0) {
		LOG_WARN("%d childs are still alive. Give up!", nCurrentChilds);
	}
	//sleep(1);

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
	signal(SIGINT, func);
	signal(SIGTERM, func);
	signal(SIGHUP, func);
	signal(SIGCHLD, func);

	signal(SIGUSR1, func);
	signal(SIGUSR2, func);

	// 무시할 시그널들
	signal(SIGPIPE, func);
}

void daemonSignal(int signo) {
	switch (signo) {
		case SIGINT : {
			LOG_INFO("Caughted SIGINT ");
			daemonEnd(EXIT_SUCCESS);
			break;
		}
		case SIGTERM : {
			LOG_INFO("Caughted SIGTERM ");
			daemonEnd(EXIT_SUCCESS);
			break;
		}
		case SIGHUP : {
			LOG_INFO("Caughted SIGHUP");

			// reload config
			if(loadConfig(g_conf.szConfigFile, &g_conf) == false) {
				LOG_ERR("Can't reload configuration file %s", g_conf.szConfigFile);
			}

			// reload mime
			mimeFree();
			if(mimeInit(g_conf.szMimeFile, g_conf.szMimeDefault) == false) {
				LOG_ERR("Can't load mimetypes from %s", g_conf.szConfigFile);
			}

			// config hook
			if(hookAfterConfigLoaded() == false || hookAfterDaemonSIGHUP() == false) {
				LOG_ERR("Hook failed.");
			}

			// re-launch childs
			poolSetExitReqeustAll();

			LOG_INFO("Re-loaded.");
			break;
		}
		case SIGCHLD : {
			int nChildPid;
			int nChildStatus;

			DEBUG("Caughted SIGCHLD");

			while((nChildPid = waitpid((pid_t)-1, &nChildStatus, WNOHANG)) > 0) {
				DEBUG("Detecting child(%d) terminated. Status : %d", nChildPid, nChildStatus);

				// if child is killed unexpectly such like SIGKILL, we remove child info here
				if (poolChildDel(nChildPid) == true) {
					LOG_WARN("Child %d killed unexpectly.", nChildPid);
				}
			}

			break;
		}
		case SIGUSR1 : {
			LOG_INFO("Caughted SIGUSR1");
			break;
		}
		case SIGUSR2 : {
			LOG_INFO("Caughted SIGUSR2");
			break;
		}
		default : {
			LOG_WARN("Unexpected signal caughted : signo=%d", signo);
			break;
		}
	}
}
