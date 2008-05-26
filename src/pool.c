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
// PRIVATE VARIABLES
/////////////////////////////////////////////////////////////////////////
static struct SharedData *m_pShm = NULL;
static int m_nShmId = -1;

static int m_nMaxChild = 0;
static int m_nMySlotId = -1; // for this process

/////////////////////////////////////////////////////////////////////////
// PRIVATE FUNCTION PROTOTYPES
/////////////////////////////////////////////////////////////////////////

static bool poolInitData(void);
static void poolInitSlot(int nSlotId);
static int poolFindSlot(int nPid);

/////////////////////////////////////////////////////////////////////////
// PUBLIC FUNCTIONS
/////////////////////////////////////////////////////////////////////////

// returns true, success
// returns false, fail
bool poolInit(int nMaxChild) {
	struct SharedData *pShm;
	int nShmId;

	if(nMaxChild > MAX_CHILDS) {
		LOG_ERR("The number of maximum childs is too big. (System maximum: %d)", MAX_CHILDS);
		return false;
	}

	nShmId = qShmInit(g_conf.szPidfile, 'p', sizeof(struct SharedData), true);
	if(nShmId < 0) return false;

	pShm = (struct SharedData *)qShmGet(nShmId);
	if(pShm == NULL) return false;

	m_nShmId = nShmId;
	m_pShm = pShm;
	m_nMaxChild = nMaxChild;
	poolInitData();
	return true;
}

bool poolFree(void) {
	if(m_nShmId >= 0) {
		qShmFree(m_nShmId);
		m_nShmId = -1;
		m_pShm = NULL;
	}
	return true;
}

struct SharedData *poolGetShm(void) {
	return m_pShm;
}

int poolSendSignal(int signo) {
	int i, n;

	n = 0;
	for (i = 0; i < m_nMaxChild; i++) {
		if (m_pShm->child[i].nPid == 0) continue;
		if (kill(m_pShm->child[i].nPid, signo) == 0) n++;
	}

	return n;
}

/*
 * 차일드 누적 구동 수를 얻음
 *
 * @return 차일드 누적 구동 수
 */
int poolGetTotalLaunched(void) {
	if(m_pShm == NULL) return 0;
	return m_pShm->nTotalLaunched;
}

/*
 * 구동중인 프로세스 수를 얻음
 *
 * @return 구동중인 프로세스 수
 */
int poolGetCurrentChilds(void) {
	if(m_pShm == NULL) return 0;
	return m_pShm->nCurrentChilds;
}

/*
 * 서비스중인 프로세스 수를 얻음
 *
 * @return 서비스중인 프로세스 수
 */
int poolGetWorkingChilds(void) {
	if(m_pShm == NULL) return 0;

	int i, working;
	for (i = working = 0; i < m_nMaxChild; i++) {
		if (m_pShm->child[i].nPid > 0 && m_pShm->child[i].conn.bConnected == true) working++;
	}

	return working;
}

/*
 * IDLE 프로세스에게 종료 프레그를 설정
 *
 * @return 종료 프레그를 설정한 프로세스 수
 */
int poolSetIdleExitReqeust(int nNum) {
	int i, nCnt = 0;
	for (i = 0; i < m_nMaxChild; i++) {
		if (m_pShm->child[i].nPid > 0 && m_pShm->child[i].conn.bConnected == false) {
			m_pShm->child[i].bExit = true;
			nCnt++;
			if(nCnt >= nNum) break;
		}
	}

	return nCnt;;
}

/*
 * 모든 프로세스에게 종료 프레그를 설정
 *
 * @return 종료 프레그를 설정한 프로세스 수
 */
int poolSetExitReqeustAll(void) {
	int i, nCnt = 0;
	for (i = 0; i < m_nMaxChild; i++) {
		if (m_pShm->child[i].nPid > 0) {
			m_pShm->child[i].bExit = true;
			nCnt++;
		}
	}

	return nCnt;;
}

/////////////////////////////////////////////////////////////////////////
// MEMBER FUNCTIONS - child management
/////////////////////////////////////////////////////////////////////////

/*
 * called by child
 * @note
 * 성공여부와 상관없이 nTotalLaunched는 항상 1 증가
 */
bool poolChildReg(void) {
	if (m_nMySlotId >= 0) {
		LOG_WARN("already registered.");
		return false;
	}

	qSemEnter(g_semid, 1);

	// find empty slot
	int nSlot = poolFindSlot(0);
	if (nSlot < 0) {
		LOG_WARN("Shared Pool FULL. Maximum connection reached.");

		// set global info
		m_pShm->nTotalLaunched++;

		qSemLeave(g_semid, 1);
		return false;
	}

	// clear slot
	poolInitSlot(nSlot);
	m_pShm->child[nSlot].nPid = getpid();
	m_pShm->child[nSlot].nStartTime = time(NULL);
	m_pShm->child[nSlot].conn.bConnected = false;

	// set global info
	m_pShm->nTotalLaunched++;
	m_pShm->nCurrentChilds++;

	// set member variable
	m_nMySlotId = nSlot;

	qSemLeave(g_semid, 1);
	return true;
}

// called by daemon
bool poolChildDel(int nPid) {
	int nSlot;

	qSemEnter(g_semid, 1);

	nSlot = poolFindSlot(nPid);
	if (nSlot < 0) {
		//DEBUG("Can't find slot for pid %d.", nPid);
		qSemLeave(g_semid, 1);
		return false;
	}

	// clear rest of all
	//poolInitSlot(nSlot);
	m_pShm->child[nSlot].nPid = 0; // 세부 데이터의 크리어 여부는 재 사용시에 행함
	m_pShm->nCurrentChilds--;

	qSemLeave(g_semid, 1);
	return true;
}

int poolGetMySlotId(void) {
	return m_nMySlotId;
}

bool poolGetExitRequest(void) {
	if (m_nMySlotId < 0) return true;

	return m_pShm->child[m_nMySlotId].bExit;
}

bool poolSetExitRequest(void) {
	if (m_nMySlotId < 0) return false;

	m_pShm->child[m_nMySlotId].bExit = true;
	return true;
}

/////////////////////////////////////////////////////////////////////////
// MEMBER FUNCTIONS - child
/////////////////////////////////////////////////////////////////////////

int poolGetChildTotalRequests(void) {
	if (m_nMySlotId < 0) return -1;

	return m_pShm->child[m_nMySlotId].nTotalRequests;
}

/////////////////////////////////////////////////////////////////////////
// MEMBER FUNCTIONS - connection
/////////////////////////////////////////////////////////////////////////

bool poolSetConnInfo(int nSockFd) {
	if (m_nMySlotId < 0) return NULL;

	struct sockaddr_in sockAddr;
	socklen_t sockSize = sizeof(sockAddr);

	// get client info
	if (getpeername(nSockFd, (struct sockaddr *)&sockAddr, &sockSize) != 0) {
		LOG_WARN("getpeername() failed.");
		return false;
	}

	// set slot data
	m_pShm->child[m_nMySlotId].conn.bConnected = true;
	m_pShm->child[m_nMySlotId].conn.nStartTime = time(NULL);
	m_pShm->child[m_nMySlotId].conn.nTotalRequests = 0;

	m_pShm->child[m_nMySlotId].conn.nSockFd = nSockFd;
	strcpy(m_pShm->child[m_nMySlotId].conn.szAddr, inet_ntoa(sockAddr.sin_addr));
	m_pShm->child[m_nMySlotId].conn.nAddr = convIp2Uint(m_pShm->child[m_nMySlotId].conn.szAddr);
	m_pShm->child[m_nMySlotId].conn.nPort = (int)sockAddr.sin_port; // int is more convenience to use

	// set child info
	m_pShm->child[m_nMySlotId].nTotalConnected++;

	// set global info
	m_pShm->nTotalConnected++;
	return true;
}

bool poolSetConnRequest(struct HttpRequest *req) {
	int nReqSize = sizeof(m_pShm->child[m_nMySlotId].conn.szReqInfo);
	char *pszReqMethod = req->pszRequestMethod;
	char *pszReqUri = req->pszRequestUri;

	if(pszReqMethod == NULL) pszReqMethod = "";
	if(pszReqUri == NULL) pszReqUri = "";

	snprintf(m_pShm->child[m_nMySlotId].conn.szReqInfo, nReqSize - 1, "%s %s", pszReqMethod, pszReqUri);
	m_pShm->child[m_nMySlotId].conn.szReqInfo[nReqSize - 1] = '\0';

	m_pShm->child[m_nMySlotId].conn.bRun = true;
	m_pShm->child[m_nMySlotId].conn.nResponseCode = 0;
	gettimeofday(&m_pShm->child[m_nMySlotId].conn.tvReqTime, NULL);

	m_pShm->child[m_nMySlotId].conn.nTotalRequests++;
	m_pShm->child[m_nMySlotId].nTotalRequests++;
	m_pShm->nTotalRequests++;

	return true;
}

bool poolSetConnResponse(struct HttpResponse *res) {
	m_pShm->child[m_nMySlotId].conn.nResponseCode = res->nResponseCode;
	gettimeofday(&m_pShm->child[m_nMySlotId].conn.tvResTime, NULL);
	m_pShm->child[m_nMySlotId].conn.bRun = false;

	return true;
}

bool poolClearConnInfo(void) {
	if (m_nMySlotId < 0) return NULL;

	m_pShm->child[m_nMySlotId].conn.bConnected = false;
	m_pShm->child[m_nMySlotId].conn.nEndTime = time(NULL); // set endtime
	return true;
}

char *poolGetConnAddr(void) {
	if (m_nMySlotId < 0) return NULL;

	return m_pShm->child[m_nMySlotId].conn.szAddr;
}

unsigned int poolGetConnNaddr(void) {
	if (m_nMySlotId < 0) return -1;

	return m_pShm->child[m_nMySlotId].conn.nAddr;
}

int poolGetConnPort(void) {
	if (m_nMySlotId < 0) return -1;

	return m_pShm->child[m_nMySlotId].conn.nPort;
}

time_t poolGetConnReqTime(void) {
	if (m_nMySlotId < 0) return -1;

	return m_pShm->child[m_nMySlotId].conn.tvReqTime.tv_sec;
}

/////////////////////////////////////////////////////////////////////////
// PRIVATE FUNCTIONS - child
/////////////////////////////////////////////////////////////////////////

// clear shared memory
static bool poolInitData(void) {
	if(m_pShm == NULL) return false;

	// clear everything at the first
	memset((void *)m_pShm, 0, sizeof(struct SharedData));

	// set start time
	m_pShm->nStartTime = time(NULL);
	m_pShm->nTotalLaunched = 0;
	m_pShm->nCurrentChilds = 0;

	// clear child. we clear all available slot even we do not use slot over m_nMaxChild
	int i;
	for (i = 0; i < MAX_CHILDS; i++) poolInitSlot(i);

	return true;
}

static void poolInitSlot(int nSlotId) {
	if(m_pShm == NULL) return;

	memset((void *)&m_pShm->child[nSlotId], 0, sizeof(struct child));
	m_pShm->child[nSlotId].nPid = 0; // does not need, but to make sure
}

static int poolFindSlot(int nPid) {
	if(m_pShm == NULL) return -1;

	int i;
	for (i = 0; i < m_nMaxChild; i++) {
		if (m_pShm->child[i].nPid == nPid) return i;
	}

	return -1;
}
