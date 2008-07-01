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

#ifndef _QHTTPD_H
#define _QHTTPD_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <signal.h>
#include <netdb.h>
#include <libgen.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "qDecoder.h"

//
// PROGRAM SPECIFIC DEFINITIONS
//
#define PRG_INFO				"The qDecoder Project"
#define PRG_NAME				"qhttpd"
#define PRG_VERSION				"1.1.0"

//
// MAXIMUM INTERNAL LIMITATIONS
//

#define MAX_CHILDS				(128)
#define MAX_SEMAPHORES				(1+2)
#define MAX_MAX_SEMAPHORES_LOCK_SECS		(10)		// maximum secondes which semaphores can be locked

#define MAX_HTTP_MEMORY_CONTENTS		(1024*1024)	// if the contents size is less than this, do not use temporary file
#define	MAX_SHUTDOWN_WAIT			(2000)		// maximum ms for waiting input stream after socket shutdown
#define	MAX_LINGER_TIMEOUT			(15)

#define	MAX_USERCOUNTER				(10)		// amount of custom counter in shared memory for customizing purpose

#define MAX_LOGLEVEL				(4)

#define	MAX_PATH_LEN				(1024)
#define	MAX_FILEPATH_LENGTH			(1024*4-1)
#define	MAX_FILENAME_LENGTH			(256-1)
#define LINEBUF_SIZE				(1024)

//
// HTTP RESPONSE CODES
//
#define HTTP_NO_RESPONSE			(0)
#define HTTP_CONTINUE				(100)
#define HTTP_RESCODE_OK				(200)
#define HTTP_RESCODE_CREATED			(201)
#define	HTTP_NO_CONTENT				(204)
#define HTTP_MULTI_STATUS			(207)
#define HTTP_RESCODE_MOVED_TEMPORARILY		(302)
#define HTTP_RESCODE_NOT_MODIFIED		(304)
#define HTTP_RESCODE_BAD_REQUEST		(400)
#define HTTP_RESCODE_FORBIDDEN			(403)
#define HTTP_RESCODE_NOT_FOUND			(404)
#define HTTP_RESCODE_METHOD_NOT_ALLOWED		(405)
#define HTTP_RESCODE_REQUEST_TIME_OUT		(408)
#define HTTP_RESCODE_REQUEST_URI_TOO_LONG	(414)
#define HTTP_RESCODE_INTERNAL_SERVER_ERROR	(500)
#define HTTP_RESCODE_NOT_IMPLEMENTED		(501)
#define HTTP_RESCODE_SERVICE_UNAVAILABLE	(503)

#define	HTTP_PROTOCOL_09			"HTTP/0.9"
#define	HTTP_PROTOCOL_10			"HTTP/1.0"
#define	HTTP_PROTOCOL_11			"HTTP/1.1"

//
// CONFIGURATION STRUCTURES
//

typedef struct {
	char	szConfigFile[MAX_PATH_LEN+1];

	char	szRunDir[MAX_PATH_LEN+1];
	char	szLogDir[MAX_PATH_LEN+1];
	char	szDataDir[MAX_PATH_LEN+1];
	char	szTmpDir[MAX_PATH_LEN+1];

	char	szMimeFile[MAX_PATH_LEN+1];

	char	szPidfile[MAX_PATH_LEN+1];
	int	nPort;
	int	nMaxpending;

	int	nStartServers;
	int	nMinSpareServers;
	int	nMaxSpareServers;
	int	nMaxIdleSeconds;
	int	nMaxClients;
	int	nMaxRequestsPerChild;

	bool	bKeepAliveEnable;
	int	nConnectionTimeout;

	int	nResponseExpires;

	char	szErrorLog[64+1];
	char	szAccessLog[64+1];
	int	nLogRotate;
	int	nLogLevel;

	bool	bStatusEnable;
	char	szStatusUrl[MAX_PATH_LEN+1];
} Config;

//
// SHARED STRUCTURES
//

struct SharedData {
	// daemon info
	time_t	nStartTime;
	int	nTotalLaunched;			// ������ ���ϵ� ���μ��� ��
	int	nCurrentChilds;			// ���� �����Ǵ� ���ϵ� ���μ��� ��

	int	nTotalConnected;		// ������ �� Ŭ���̾�Ʈ ��
	int	nTotalRequests;			// ó���� �� ��û��

	// child info
	struct child {
		int     nPid;			// ���μ���ID, 0�� empty slot
		int	nTotalConnected;	// ������ Ŭ���̾�Ʈ ��
		int	nTotalRequests;		// ó���� ��û��
		time_t  nStartTime;		// ���μ��� ���� �ð�
		bool	bExit;			// �� �÷��װ� �����Ǿ� ������, ó���� ��ġ�� ����

		struct {		// ���ӵ� Ŭ���̾�Ʈ ����
			bool	bConnected;	// Ŭ���̾�Ʈ ���� ����
			time_t  nStartTime;	// Ŭ���̾�Ʈ ���� �ð�
			time_t  nEndTime;	// Ŭ���̾�Ʈ ���� �ð�
			int	nTotalRequests; // Keep Alive �� Ŭ���̾�Ʈ�� ���� ��û��

			int     nSockFd;	// Ŭ���̾�Ʈ���� ����
			char    szAddr[15+1];	// Ŭ���̾�Ʈ IP �ּ�
			unsigned int nAddr;	// Ŭ���̾�Ʈ IP �ּ�(����)
			int     nPort;		// Ŭ���̾�Ʈ ��Ʈ

			bool	bRun;			// ��û ó����
			struct	timeval tvReqTime;	// ��û �ð�
			struct	timeval tvResTime;	// ���� �ð�
			char	szReqInfo[1024+1];	// ���� ����
			int	nResponseCode;		// ���� �ڵ�

		} conn;

		//time_t  nLastUpdated;		// ������ ���� �ð�
	} child[MAX_CHILDS];

	// extra info
	int	nUserCounter[MAX_USERCOUNTER];
};

//
// HTTP STRUCTURES
//
struct HttpRequest {
	// connection info
	int	nSockFd;		// �������
	int	nTimeout;		// Ÿ�Ӿƿ���

	// request status
	int	nReqStatus;		// 1:����, 0: bad request, -1:timeout, -2: connection closed

	// request line
	char*	pszRequestHost;	// host				ex) www.cdnetwork.co.kr
	char*	pszRequestMethod;	// request method		ex) GET
	char*	pszRequestUri;		// url+query.			ex) /100/my%20data/my.gif?query=the%20value
	char*	pszRequestUrl;		// decoded url			ex) /100/my data/my.gif
	char*	pszQueryString;	// query string			ex) query=the%20value
	char*	pszHttpVersion;	// HTTP/?.?

	// request header
	Q_ENTRY *pHeaders;		// request headers

	// contents
	size_t	nContentsLength;	// ������ ���� �� ������. 0:������ ����, n>0 ������ ����
	char*	pContents;		// �������� �Ľ̵� ��� NULL �� �ƴ�.
					// nContentsLength > 0 �鼭 pContents�� NULL �� �� ����
};

struct HttpResponse {
	bool	bOut;			// ������ �ߴ��� ����

	char*	pszHttpVersion;		// ���� ��������
	int	nResponseCode;		// ���� �ڵ�

	Q_ENTRY* pHeaders;		// ���� �����

	char*	pszContentType;
	size_t	nContentLength;
	char*	pContent;
	bool	bChunked;
};

//
// DEFINITION FUNCTIONS
//
#define	CONST_STRLEN(x)		(sizeof(x) - 1)

#define _LOG(log, level, prestr, fmt, args...)	do {					\
	if (g_errlog != NULL && g_loglevel >= level) {					\
		char _timestr[14+1];							\
		qTimeGetLocalStrf(_timestr, sizeof(_timestr), 0, "%Y%m%d%H%M%S");	\
		qLog(log, "%s(%d):" prestr fmt,						\
			_timestr, getpid(), ##args);					\
	}										\
} while(false)

#define _LOG2(log, level, prestr, fmt, args...)	do {					\
	if (g_errlog != NULL && g_loglevel >= level) {					\
		char _timestr[14+1];							\
		qTimeGetLocalStrf(_timestr, sizeof(_timestr), 0, "%Y%m%d%H%M%S");	\
		qLog(log, "%s(%d):" prestr fmt " (%s:%d)",				\
			_timestr, getpid(), ##args, __FILE__, __LINE__);		\
		fflush(stdout);								\
	}										\
} while(false)

#define LOG_SYS(fmt, args...)	_LOG(g_errlog, 0, " ", fmt, ##args)
#define LOG_ERR(fmt, args...)	_LOG2(g_errlog, 1, " [ERROR] ", fmt, ##args)
#define LOG_WARN(fmt, args...)	_LOG2(g_errlog, 2, " [WARN] ", fmt, ##args)
#define LOG_INFO(fmt, args...)	_LOG(g_errlog, 3, " [INFO] ", fmt, ##args)

#ifdef BUILD_DEBUG
#define DEBUG(fmt, args...)								\
	do {										\
		_LOG2(g_errlog, MAX_LOGLEVEL, " [DEBUG] ", fmt, ##args);		\
	} while (false)
#else
#define DEBUG(fms, args...)
#endif // BUILD_DEBUG

//
// PROTO-TYPES
//

// main.c
extern	int		main(int argc, char *argv[]);

// version.c
extern	void		printUsages(void);
extern	void		printVersion(void);

// config.c
extern	bool		loadConfig(Config *pConf, char *pszFilePath);

// daemon.c
extern	void		daemonStart(bool nDaemonize);
extern	void		daemonEnd(int nStatus);
extern	void		daemonSignalInit(void *func);
extern	void		daemonSignal(int signo);
extern	void		daemonSignalHandler(void);

// pool.c
extern	bool		poolInit(int nMaxChild);
extern	bool		poolFree(void);
extern	struct SharedData* poolGetShm(void);
extern	int		poolSendSignal(int signo);

extern	bool		poolCheck(void);
extern	int		poolGetTotalLaunched(void);
extern	int		poolGetCurrentChilds(void);
extern	int		poolGetWorkingChilds(void);
extern	int		poolSetIdleExitReqeust(int nNum);
extern	int		poolSetExitReqeustAll(void);

extern	bool		poolChildReg(void);
extern	bool		poolChildDel(int nPid);
extern	int		poolGetMySlotId(void);
extern	bool		poolGetExitRequest(void);
extern	bool		poolSetExitRequest(void);

extern	int		poolGetChildTotalRequests(void);

extern	bool		poolSetConnInfo(int nSockFd);
extern	bool		poolSetConnRequest(struct HttpRequest *req);
extern	bool		poolSetConnResponse(struct HttpResponse *res);
extern	bool		poolClearConnInfo(void);
extern	char*		poolGetConnAddr(void);
extern	unsigned int	poolGetConnNaddr(void);
extern	int		poolGetConnPort(void);
extern	time_t		poolGetConnReqTime(void);

// child.c
extern	void		childStart(int nSockFd);
extern	void		childEnd(int nStatus);
extern	void		childSignalInit(void *func);
extern	void		childSignal(int signo);
extern	void		childSignalHandler(void);

// child_main.c
extern	int		childMain(int nSockFd);

// http_request.c
extern	struct	HttpRequest*	httpRequestParse(int nSockFd, int nTimeout);
extern	bool		httpRequestFree(struct HttpRequest *req);

// http_response.c
extern	struct HttpResponse* httpResponseCreate(void);
extern	int		httpResponseSetSimple(struct HttpRequest *req, struct HttpResponse *res, int nResCode, bool nKeepAlive, const char *format, ...);
extern	bool		httpResponseSetCode(struct HttpResponse *res, int nResCode, struct HttpRequest *req, bool bKeepAlive);
extern	bool		httpResponseSetContent(struct HttpResponse *res, const char *pszContentType, size_t nContentLength, const char *pContent);
extern	bool		httpResponseSetContentHtml(struct HttpResponse *res, const char *pszMsg);
extern	bool		httpResponseSetContentChunked(struct HttpResponse *res, bool bChunked);
extern	bool		httpResponseSetHeader(struct HttpResponse *res, const char *pszName, const char *pszValue);
extern	bool		httpResponseSetHeaderf(struct HttpResponse *res, const char *pszName, const char *format, ...);
extern	bool		httpResponseOut(struct HttpResponse *res, int nSockFd);
extern	int		httpResponseOutChunk(int nSockFd, const char *pszData, int nSize);
extern	void		httpResponseFree(struct HttpResponse *res);
extern	const char*	httpResponseGetMsg(int nResCode);

#define response201(req, res)	httpResponseSetSimple(req, res, HTTP_RESCODE_CREATED, true, httpResponseGetMsg(HTTP_RESCODE_CREATED));
#define response204(req, res)	httpResponseSetSimple(req, res, HTTP_NO_CONTENT, true, NULL);
#define response304(req, res)	httpResponseSetSimple(req, res, HTTP_RESCODE_NOT_MODIFIED, true, NULL);
#define response400(req, res)	httpResponseSetSimple(req, res, HTTP_RESCODE_BAD_REQUEST, false, httpResponseGetMsg(HTTP_RESCODE_BAD_REQUEST))
#define response403(req, res)	httpResponseSetSimple(req, res, HTTP_RESCODE_FORBIDDEN, true, httpResponseGetMsg(HTTP_RESCODE_FORBIDDEN))
#define response404(req, res)	httpResponseSetSimple(req, res, HTTP_RESCODE_NOT_FOUND, true, httpResponseGetMsg(HTTP_RESCODE_NOT_FOUND))
#define response404nc(req, res)	httpResponseSetSimple(req, res, HTTP_RESCODE_NOT_FOUND, true, NULL)
#define response405(req, res)	httpResponseSetSimple(req, res, HTTP_RESCODE_METHOD_NOT_ALLOWED, true, httpResponseGetMsg(HTTP_RESCODE_METHOD_NOT_ALLOWED))
#define response414(req, res)	httpResponseSetSimple(req, res, HTTP_RESCODE_REQUEST_URI_TOO_LONG, true, httpResponseGetMsg(HTTP_RESCODE_REQUEST_URI_TOO_LONG))
#define response500(req, res)	httpResponseSetSimple(req, res, HTTP_RESCODE_INTERNAL_SERVER_ERROR, false, httpResponseGetMsg(HTTP_RESCODE_INTERNAL_SERVER_ERROR))
#define response501(req, res)	httpResponseSetSimple(req, res, HTTP_RESCODE_NOT_IMPLEMENTED, false, httpResponseGetMsg(HTTP_RESCODE_NOT_IMPLEMENTED))
#define response503(req, res)	httpResponseSetSimple(req, res, HTTP_RESCODE_SERVICE_UNAVAILABLE, true, httpResponseGetMsg(HTTP_RESCODE_SERVICE_UNAVAILABLE))

// http_header.c
extern	const char*	httpHeaderGetValue(Q_ENTRY *entries, const char *name);
extern	int		httpHeaderGetInt(Q_ENTRY *entries, const char *name);
extern	bool		httpHeaderHasString(Q_ENTRY *entries, const char *name, const char *str);
extern	bool		httpHeaderParseRange(const char *pszRangeHeader, size_t nFilesize, off_t *pnRangeOffset1, off_t *pnRangeOffset2, size_t *pnRangeSize);

// http_handler.c
struct HttpResponse *httpHandler(struct HttpRequest *req);

// http_method.c
extern	int		httpMethodOptions(struct HttpRequest *req, struct HttpResponse *res);
extern	int		httpMethodGet(struct HttpRequest *req, struct HttpResponse *res);
extern	int		httpProcessGetNormalFile(struct HttpRequest *req, struct HttpResponse *res, const char *pszFilePath, const char *pszContentType);
extern	int		httpMethodHead(struct HttpRequest *req, struct HttpResponse *res);
extern	int		httpMethodNotImplemented(struct HttpRequest *req, struct HttpResponse *res);

// http_status.c
extern	Q_OBSTACK*	httpGetStatusHtml(void);

// http_accesslog.c
extern	bool		httpAccessLog(struct HttpRequest *req, struct HttpResponse *res);

// mime.c
extern	bool		mimeInit(const char *pszFilepath);
extern	bool		mimeFree(void);
extern	const char*	mimeDetect(const char *pszFilename);

// stream.c
extern	int		streamWaitReadable(int nSockFd, int nTimeoutMs);
extern	ssize_t		streamRead(void *pszBuffer, int nSockFd, size_t nSize, int nTimeoutMs);
extern	ssize_t		streamGets(char *pszStr, int nSockFd, size_t nSize, int nTimeoutMs);
extern	ssize_t		streamGetb(char *pszBuffer, int nSockFd, size_t nSize, int nTimeoutMs);
extern	ssize_t		streamPrintf(int nSockFd, const char *format, ...);
extern	ssize_t		streamPuts(int nSockFd, const char *pszStr);
extern	ssize_t		streamWrite(int nSockFd, const void *pszBuffer, size_t nSize);
extern	ssize_t		streamSave(int nFileFd, int nSockFd, size_t nSize, int nTimeoutMs);
extern	ssize_t		streamSendfile(int nSockFd, const char *pszFilePath, off_t nOffset, size_t nSize);

// hook.c
extern	bool		hookBeforeMainInit(void);
extern	bool		hookAfterConfigLoaded(void);
extern	bool		hookAfterDaemonInit(void);
extern	int		hookWhileDaemonIdle(void);
extern	bool		hookBeforeDaemonEnd(void);
extern	bool		hookAfterDaemonSIGHUP(void);
extern	bool		hookAfterChildInit(void);
extern	bool		hookBeforeChildEnd(void);
extern	bool		hookAfterConnEstablished(void);
extern	int		hookMethodHandler(struct HttpRequest *req, struct HttpResponse *res);

// util.c
extern	unsigned int	convIp2Uint(const char *szIp);
extern	float		diffTimeval(struct timeval *t1, struct timeval *t0);
extern	bool		isCorrectPath(const char *pszPath);
extern	void		correctPath(char *pszPath);

//
// GLOBAL VARIABLES
//
extern Config	g_conf;
extern int	g_semid;
extern Q_LOG	*g_errlog;
extern Q_LOG	*g_acclog;
extern int	g_loglevel;
extern sigset_t	g_sigflags;

#endif	// _QHTTPD_H
