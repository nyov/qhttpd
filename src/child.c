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

void childStart(int nSockFd) {
	int nIdleCnt;

	// init signal
	childSignalInit(childSignal);

	// register at pool.
	if(poolChildReg() == false) {
		LOG_ERR("Can't register myself at the pool.");
		childEnd(EXIT_FAILURE);
	}

	// hook
	if(hookAfterChildInit() == false) {
		LOG_ERR("Hook failed.\n");
		childEnd(EXIT_FAILURE);
	}

	// init random
        srandom((unsigned)(time(NULL) + getpid() + nSockFd));

	nIdleCnt = 0;
        while (true) {
                struct sockaddr_in connAddr;     // client address information
                int nConnLen = sizeof(connAddr);
                int nNewSockFd;

		//
		// 컨넥션 대기 영역
		//

                // wait connection, for non-block accept
                fd_set socklist;
                struct timeval tv;

                FD_ZERO(&socklist);
                FD_SET(nSockFd, &socklist);
                tv.tv_sec = 1, tv.tv_usec = 0; // wait 1 sec

                if (select(FD_SETSIZE, &socklist, NULL, NULL, &tv) <= 0) {
                        // periodic(1 sec) job here
                        if(poolGetExitRequest() == true) {
                        	DEBUG("Caughted exit request.");
                        	break;
                        }

                        // idle time check
                        nIdleCnt++;
                        if(g_conf.nMaxIdleSeconds > 0 && nIdleCnt > g_conf.nMaxIdleSeconds
                        	&& poolGetCurrentChilds() > g_conf.nStartServers) {
                        	DEBUG("Maximum idle seconds(%d) are reached.", g_conf.nMaxIdleSeconds);
                        	break;
                	}

                	// check maximum requests
                	if(g_conf.nMaxRequestsPerChild > 0
                		&& poolGetChildTotalRequests() > g_conf.nMaxRequestsPerChild) {
                		DEBUG("Maximum requests per child are over. (%d/%d)", poolGetChildTotalRequests(), g_conf.nMaxRequestsPerChild);
                		break;
                	}

                        continue;
                }

                // new connection arrived
                nIdleCnt = 0;
                if ((nNewSockFd = accept(nSockFd, (struct sockaddr *) & connAddr, &nConnLen)) == -1) {
                        // 다른 프로세스에 의해 처리되었음
                        //DEBUG("I'm late...");
                        continue;
                }

		//
		// 컨넥션 완료 영역
		//

		// connection accepted
		DEBUG("Connection established.");

		// connection hook
		if(hookAfterConnEstablished() == true) {
			// register client information
			if(poolSetConnInfo(nNewSockFd) == true) {;
				// launch main logic
				childMain(nNewSockFd);
			}
		} else {
			LOG_ERR("Hook failed.");
		}

		// close connection
		if(shutdown(nNewSockFd, SHUT_WR) == 0) {
			char szDummyBuf[64];
			while(qSocketRead(nNewSockFd, szDummyBuf, sizeof(szDummyBuf), MAX_SHUTDOWN_WAIT) > 0) {;
				DEBUG("Throw dummy input stream.");
			}
		}
		close(nNewSockFd);

		// clear client information
		poolClearConnInfo();

		DEBUG("Closing connection.");
	}

	// ending connection
	childEnd(EXIT_SUCCESS);
}

void childEnd(int nStatus) {
	static bool bAlready = false;

	if(bAlready == true) return;
	bAlready = true;

	// hook
	if(hookBeforeChildEnd() == false) {
		LOG_ERR("Hook failed.\n");
	}

	// remove child info
	if (poolChildDel(getpid()) == false) {
		LOG_WARN("Can't find pid %d from connection list", getpid());
	}

	// quit
	LOG_INFO("Child terminated.");
	//microSleep(10); // 1/100000 sec
	exit(nStatus);
}

void childSignalInit(void *func) {
	signal(SIGINT, func);
	signal(SIGTERM, func);
	signal(SIGHUP, func);
	signal(SIGPIPE, func);

	signal(SIGUSR1, func);
	signal(SIGUSR2, func);

	// 무시할 시그널들
	//signal(SIGPIPE, func);
}

void childSignal(int signo) {
	switch (signo) {
		case SIGINT : {
			LOG_INFO("Child : Caughted SIGINT ");
			break;
		}
		case SIGTERM : {
			LOG_INFO("Child : Caughted SIGTERM");
			childEnd(EXIT_SUCCESS);
			break;
		}
		case SIGHUP : {
			LOG_INFO("Child : Caughted SIGHUP ");
			break;
		}
		case SIGPIPE : {
			LOG_INFO("Child : Caughted SIGPIPE ");
			break;
		}
		case SIGUSR1 : {
			LOG_INFO("Child : Caughted SIGUSR1 ");
			break;
		}
		case SIGUSR2 : {
			LOG_INFO("Child : Caughted SIGUSR2 ");
			break;
		}
		default : {
			LOG_WARN("Child : Unexpected signal caughted : signo=%d", signo);
			break;
		}
	}
}
