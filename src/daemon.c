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
// PRIVATE FUNCTION PROTOTYPES
/////////////////////////////////////////////////////////////////////////
static int m_nBindSockFd = -1;
static bool ignoreConnection(int nSockFd, long int nTimeoutMs);

/////////////////////////////////////////////////////////////////////////
// PUBLIC FUNCTIONS
/////////////////////////////////////////////////////////////////////////

void daemonStart(bool nDaemonize) {
	// init signal
	daemonSignalInit(daemonSignal);

	// open log
	g_errlog = qLog(g_conf.szErrorLog, g_conf.nLogRotate, true);
	g_acclog = qLog(g_conf.szAccessLog, g_conf.nLogRotate, true);
	if (g_errlog == NULL || g_acclog == NULL) {
		printf("Can't open log file.\n");
		daemonEnd(EXIT_FAILURE);
	}
	g_errlog->duplicate(g_errlog, stdout,  true);
	g_acclog->duplicate(g_acclog, stdout, false);

	// entering daemon mode
	if (nDaemonize) {
		LOG_INFO("Entering daemon mode.");
		daemon(false, false); // after this line, parent's pid will be changed.
		g_errlog->duplicate(g_errlog, stdout, false); // do not screen out any more
	}

	// save pid
	if(qCountSave(g_conf.szPidFile, getpid()) == false) {
		LOG_ERR("Can't create pid file.");
		daemonEnd(EXIT_FAILURE);
	}

	// init semaphore
	if ((g_semid = qSemInit(g_conf.szPidFile, 's', MAX_SEMAPHORES, true)) < 0) {
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

	// load mime
	if(mimeInit(g_conf.szMimeFile) == false) {
		LOG_WARN("Can't load mimetypes from %s", g_conf.szMimeFile);
	}

	// init socket
	int nSockFd;
	if ((nSockFd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		LOG_ERR("Can't create socket.");
		daemonEnd(EXIT_FAILURE);
	} else {
		int so_reuseaddr = 1;
		int so_sndbufsize = 0;
		int so_rcvbufsize = 0;

		if (so_reuseaddr > 0) setsockopt(nSockFd, SOL_SOCKET, SO_REUSEADDR, &so_reuseaddr, sizeof(so_reuseaddr));
		if (so_sndbufsize > 0) setsockopt(nSockFd, SOL_SOCKET, SO_SNDBUF, &so_sndbufsize, sizeof(so_sndbufsize));
		if (so_rcvbufsize > 0) setsockopt(nSockFd, SOL_SOCKET, SO_RCVBUF, &so_rcvbufsize, sizeof(so_rcvbufsize));
	}

	// set to non-block socket
	m_nBindSockFd = nSockFd; // store bind sock id
	fcntl(nSockFd, F_SETFL, O_NONBLOCK);

	// bind
	struct sockaddr_in svrAddr;		// server address information
	svrAddr.sin_family = AF_INET;		// host byte order
	svrAddr.sin_port = htons(g_conf.nPort);	// short, network byte order
	svrAddr.sin_addr.s_addr = INADDR_ANY;	// auto-fill with my IP
	memset((void *)&(svrAddr.sin_zero), 0, sizeof(svrAddr.sin_zero)); // zero the rest of the struct

	if (bind(nSockFd, (struct sockaddr *)&svrAddr, sizeof(struct sockaddr)) == -1) {
		LOG_ERR("Can't bind port %d (errno: %d)", g_conf.nPort, errno);
		daemonEnd(EXIT_FAILURE);
	}
	LOG_INFO("  - Binding port %d succeed.", g_conf.nPort);

	// listen
	if (listen(nSockFd, g_conf.nMaxPending) == -1) {
		LOG_ERR("Can't listen port %d.", g_conf.nPort);
		daemonEnd(EXIT_FAILURE);
	}

#ifdef ENABLE_HOOK
	// after init hook
	if(hookAfterDaemonInit() == false) {
		LOG_ERR("Hook failed.");
		daemonEnd(EXIT_FAILURE);
	}
#endif

	// starting.
	LOG_SYS("%s %s is ready on the port %d.", g_prgname, g_prgversion, g_conf.nPort);

	// prefork management
	int nIgnoredConn = 0;
	while (true) {
		// signal handling
		daemonSignalHandler();

		// get child count
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
				else if(nIdleChilds <= 0 && g_conf.bIgnoreOverConnection == true) { // ignore connectin
					while(ignoreConnection(nSockFd, 0) == true) {
						nIgnoredConn++;
						LOG_WARN("Maximum connection reached. Connection ignored. (%d)", nIgnoredConn);
					}
				}
			} else if(nIdleChilds > g_conf.nMaxSpareServers) { // too much idle childs
				if(nCurrentChilds > g_conf.nStartServers) nChildFlag = -1;
			}
		}

		//DEBUG("%d %d %d %d", nCurrentChilds, nWorkingChilds, nIdleChilds, nChildFlag);

		// launch or kill childs
		if(nChildFlag > 0) {
			// launching  spare server
			DEBUG("Launching spare server. (working:%d, total:%d)", nWorkingChilds, nCurrentChilds);
			int nCpid = fork();
			if (nCpid < 0) { // error
				LOG_ERR("Can't create child.");
				sleep(1);
			} else if (nCpid == 0) { // this is child
				DEBUG("Child %d launched", getpid());

				// main job
				childStart(nSockFd);

				// safety code, never reached.
				exit(EXIT_FAILURE);

			} else { // this is parent
				int nWait;

				// wait for the child register itself to shared pool.
				for(nWait = 0; nWait < 1000; nWait++) {
					//DEBUG("%d %d", nTotalLaunched, poolGetTotalLaunched());
					if(nTotalLaunched != poolGetTotalLaunched()) break;
					DEBUG("Waiting child registered at pool. [%d]", nWait+1);
					usleep(1*1000);
				}
				if(nWait == 1000) {
					LOG_WARN("Delayed child launching.");
				}
			}
		} else if(nChildFlag < 0) { // removing child
			static time_t nLastSec = 0;

			// removing 1 child per sec
			if(nLastSec != time(NULL)) {
				if(poolSetIdleExitReqeust(1) != 1) {
					LOG_WARN("Can't set exit flag.");
				}
				nLastSec = time(NULL);
			} else {
				usleep(1 * 1000);
			}
		} else { // no need to increase spare server
			static time_t nLastSec = 0;

			// periodic job here
			if(nLastSec != time(NULL)) {
				// safety code : check semaphore dead-lock bug
				static int nSemLockCnt[MAX_SEMAPHORES];
				int i;
				for(i = 0; i < MAX_SEMAPHORES; i++) {
					if(qSemCheck(g_semid, i) == true) {
						nSemLockCnt[i]++;
						if(nSemLockCnt[i] > MAX_SEMAPHORES_LOCK_SECS) {
							LOG_ERR("Force to unlock semaphore no %d", i);
							qSemLeave(g_semid, i);	// force to unlock
							nSemLockCnt[i] = 0;
						}
					} else {
						nSemLockCnt[i] = 0;
					}
				}

				// check pool
				if(poolCheck() == true) {
					LOG_WARN("Child count mismatch. fixed.");
				}

#ifdef ENABLE_HOOK
				if(hookWhileDaemonIdle() < 0) {
					LOG_ERR("Hook failed.");
				}
#endif

				// update running time
				nLastSec = time(NULL);
			} else {
				//DEBUG("sleeping...");
				usleep(1 * 1000);
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
		sleep(1);

		// waitpid
		while(waitpid(-1, NULL, WNOHANG) > 0);
	}

#ifdef ENABLE_HOOK
	if(hookBeforeDaemonEnd() == false) {
		LOG_ERR("Hook failed.");
	}
#endif

	// close bind sock
	if(m_nBindSockFd >= 0) {
		close(m_nBindSockFd);
		m_nBindSockFd = -1;
	}

	// destroy mime
  	if (mimeFree() == false) {
  		LOG_WARN("Can't destroy mime types.");
  	}

	// destroy shared memory
	if (poolFree() == false) {
		LOG_WARN("Can't destroy child management pool .");
	} else {
		LOG_INFO("Child management pool destroied.");
	}

	// destroy semaphore
	int i;
	for(i = 0; i < MAX_SEMAPHORES; i++) qSemLeave(g_semid, i);	// force to unlock every semaphores
	if (qSemFree(g_semid) == false) {
		LOG_WARN("Can't destroy semaphore.");
	} else {
		LOG_INFO("Semaphore destroied.");
	}

	// remove pid file
	if (qFileExist(g_conf.szPidFile) == true && unlink(g_conf.szPidFile) != 0) {
		LOG_WARN("Can't remove pid file %s", g_conf.szPidFile);
	}

	// final
	LOG_SYS("%s Terminated.", g_prgname);

	// close log
	if(g_acclog != NULL) g_acclog->free(g_acclog);
	if(g_errlog != NULL) g_errlog->free(g_errlog);

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

		struct ServerConfig newconf;
		memset((void*)&newconf, 0, sizeof(newconf));
		bool bConfigLoadStatus = loadConfig(&newconf, g_conf.szConfigFile);
#ifdef ENABLE_HOOK
		bConfigLoadStatus = hookAfterConfigLoaded(&newconf, bConfigLoadStatus);
#endif
		if(bConfigLoadStatus == true) {
			g_conf = newconf;
			LOG_INFO("Configuration re-loaded.");
		} else {
			LOG_ERR("Can't reload configuration.");
		}

		// reload mime
		mimeFree();
		if(mimeInit(g_conf.szMimeFile) == false) {
			LOG_ERR("Can't load mimetypes from %s", g_conf.szMimeFile);
		}

#ifdef ENABLE_HOOK
		// hup hook
		if(hookAfterDaemonSIGHUP() == false) {
			LOG_ERR("Hook failed.");
		}
#endif
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

static bool ignoreConnection(int nSockFd, long int nTimeoutMs) {
	struct sockaddr_in connAddr;
	socklen_t nConnLen = sizeof(connAddr);
	int nNewSockFd;

	// wait connection
        if(qSocketWaitReadable(nSockFd, nTimeoutMs) <= 0) return false;

	// accept connection
	if((nNewSockFd = accept(nSockFd, (struct sockaddr *)&connAddr, &nConnLen)) == -1) return false;

	// caughted connection
	streamPrintf(nNewSockFd, "%s %d %s\r\n", HTTP_PROTOCOL_11, HTTP_RESCODE_SERVICE_UNAVAILABLE, httpResponseGetMsg(HTTP_RESCODE_SERVICE_UNAVAILABLE));
	streamPrintf(nNewSockFd, "Content-Length: 0\r\n");
	streamPrintf(nNewSockFd, "Connection: close\r\n");
	streamPrintf(nNewSockFd, "\r\n");
	close(nNewSockFd);

	return true;
}
