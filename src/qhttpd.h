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
#define PRG_NAME				"qhttpd"
#define PRG_VERSION				"1.0.1"

//
// MAXIMUM INTERNAL LIMITATIONS
//

#define MAX_CHILDS				(128)
#define MAX_SEMAPHORES				(1+2)
#define MAX_MAX_SEMAPHORES_LOCK_SECS		(10)		// maximum secondes which semaphores can be locked
#define LINEBUF_SIZE				(1024)
#define SYSPATH_SIZE				MAX_PATH_LEN
#define HTTP_MAX_MEMORY_CONTENTS		(1024*1024)	// if the contents size is less than this, do not use temporary file
#define	MAX_USERCOUNTER				(10)		// amount of custom counter in shared memory for customizing purpose
#define	MAX_SHUTDOWN_WAIT			(2000)		// maximum ms for waiting input stream after socket shutdown
#define	MAX_LINGER_TIMEOUT			(15)

#define MAX_LOGLEVEL				(4)

#define	MAX_PATH_LEN				(1024)
#define	MAX_FILEPATH_LENGTH			(1024*4)
#define	MAX_FILENAME_LENGTH			(256)

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
	int	nTotalLaunched;			// 생성한 차일드 프로세스 수
	int	nCurrentChilds;			// 현재 구동되는 차일드 프로세스 수

	int	nTotalConnected;		// 접속한 총 클라이언트 수
	int	nTotalRequests;			// 처리한 총 요청수

	// child info
	struct child {
		int     nPid;			// 프로세스ID, 0은 empty slot
		int	nTotalConnected;	// 접속한 클라이언트 수
		int	nTotalRequests;		// 처리한 요청수
		time_t  nStartTime;		// 프로세스 시작 시간
		bool	bExit;			// 이 플레그가 설정되어 있으면, 처리를 마치고 종료

		struct {		// 접속된 클라이언트 정보
			bool	bConnected;	// 클라이언트 접속 여부
			time_t  nStartTime;	// 클라이언트 접속 시간
			time_t  nEndTime;	// 클라이언트 종료 시간
			int	nTotalRequests; // Keep Alive 시 클라이언트가 보낸 요청수

			int     nSockFd;	// 클라이언트와의 소켓
			char    szAddr[15+1];	// 클라이언트 IP 주소
			unsigned int nAddr;	// 클라이언트 IP 주소(숫자)
			int     nPort;		// 클라이언트 포트

			bool	bRun;			// 요청 처리중
			struct	timeval tvReqTime;	// 요청 시간
			struct	timeval tvResTime;	// 응답 시간
			char	szReqInfo[1024+1];	// 접속 정보
			int	nResponseCode;		// 응답 코드

		} conn;

		//time_t  nLastUpdated;		// 마지막 갱신 시간
	} child[MAX_CHILDS];

	// extra info
	int	nUserCounter[MAX_USERCOUNTER];
};

//
// HTTP STRUCTURES
//
struct HttpRequest {
	// connection info
	int	nSockFd;		// 연결소켓
	int	nTimeout;		// 타임아웃값

	// request status
	int	nReqStatus;		// 1:정상, 0: bad request, -1:timeout, -2: connection closed

	// request line
	char	*pszRequestHost;	// host				ex) www.cdnetwork.co.kr
	char	*pszRequestMethod;	// request method		ex) GET
	char	*pszRequestUri;		// url+query.			ex) /100/my%20data/my.gif?query=the%20value
	char	*pszRequestUrl;		// decoded url			ex) /100/my data/my.gif
	char	*pszQueryString;	// query string			ex) query=the%20value
	char	*pszHttpVersion;	// HTTP/?.?

	// request header
	Q_ENTRY *pHeaders;		// request headers

	// contents
	uint64_t nContentsLength;	// 컨텐츠 유무 및 사이즈. 0:컨텐츠 없음, n>0 컨텐츠 있음
	char	*pContents;		// 컴텐츠가 파싱된 경우 NULL 이 아님.
					// nContentsLength > 0 면서 pContents가 NULL 일 수 있음
};

struct HttpResponse {
	bool	bOut;			// 응답을 했는지 여부

	char	*pszHttpVersion;	// 응답 프로토콜
	int	nResponseCode;		// 응답 코드

	Q_ENTRY *pHeaders;		// 응답 헤더들

	char	*pszContentType;
	uint64_t nContentLength;
	char	*pContent;
	bool	bChunked;
};

//
// DEFINITION FUNCTIONS
//
#define _LOG(log, level, prestr, fmt, args...)	do {			\
	if (g_errlog != NULL && g_loglevel >= level) {			\
		qLog(log, "%s(%d):" prestr fmt,				\
		qGetTimeStr(0), getpid(), ##args);			\
	}								\
} while(0)

#define _LOG2(log, level, prestr, fmt, args...)	do {			\
	if (g_errlog != NULL && g_loglevel >= level) {			\
		qLog(log, "%s(%d):" prestr fmt " (%s:%d)",		\
		qGetTimeStr(0), getpid(), ##args, __FILE__, __LINE__);	\
	}								\
} while(0)

#define LOG_SYS(fmt, args...)	_LOG(g_errlog, 0, " ", fmt, ##args)
#define LOG_ERR(fmt, args...)	_LOG2(g_errlog, 1, " [ERROR] ", fmt, ##args)
#define LOG_WARN(fmt, args...)	_LOG2(g_errlog, 2, " [WARN] ", fmt, ##args)
#define LOG_INFO(fmt, args...)	_LOG(g_errlog, 3, " [INFO] ", fmt, ##args)

#ifdef BUILD_DEBUG
#define DEBUG(fmt, args...)						\
	do {								\
		_LOG2(g_errlog, MAX_LOGLEVEL, " [DEBUG] ", fmt, ##args); \
	} while (0)
#else
#define DEBUG(fms, args...)
#endif // BUILD_DEBUG

//
// PROTO-TYPES
//

// main.c
int	main(int argc, char *argv[]);

// version.c
void	printUsages(void);
void	printVersion(void);

// config.c
bool	loadConfig(char	*pszFilePath, Config *pConf);

// signal.c
void	signalInit(void *func);

// daemon.c
void	daemonStart(bool nDaemonize);
void	daemonEnd(int nStatus);
void	daemonSignalInit(void *func);
void	daemonSignal(int signo);
void	daemonSignalHandler(void);

// child.c
void	childStart(int nSockFd);
void	childEnd(int nStatus);
void	childSignalInit(void *func);
void	childSignal(int signo);
void	childSignalHandler(void);

// child_main.c
int	childMain(int nSockFd);

// pool.c
bool	poolInit(int nMaxChild);
bool	poolFree(void);
struct SharedData *poolGetShm(void);
int	poolSendSignal(int signo);

int	poolGetTotalLaunched(void);
int	poolGetCurrentChilds(void);
int	poolGetWorkingChilds(void);
int	poolSetIdleExitReqeust(int nNum);
int	poolSetExitReqeustAll(void);

bool	poolChildReg(void);
bool	poolChildDel(int nPid);
int	poolGetMySlotId(void);
bool	poolGetExitRequest(void);
bool	poolSetExitRequest(void);

int	poolGetChildTotalRequests(void);

bool	poolSetConnInfo(int nSockFd);
bool	poolSetConnRequest(struct HttpRequest *req);
bool	poolSetConnResponse(struct HttpResponse *res);
bool	poolClearConnInfo(void);
char	*poolGetConnAddr(void);
unsigned int poolGetConnNaddr(void);
int	poolGetConnPort(void);
time_t	poolGetConnReqTime(void);

// http_request.c
struct	HttpRequest *httpRequestParse(int nSockFd, int nTimeout);
void	httpRequestFree(struct HttpRequest *req);

// http_response.c
struct	HttpResponse *httpResponseCreate(void);
int	httpResponseSetSimple(struct HttpRequest *req, struct HttpResponse *res, int nResCode, bool nKeepAlive, char *format, ...);
bool	httpResponseSetCode(struct HttpResponse *res, int nResCode, struct HttpRequest *req, bool bKeepAlive);
bool	httpResponseSetContent(struct HttpResponse *res, char *pszContentType, uint64_t nContentLength, char *pContent);
bool	httpResponseSetContentHtml(struct HttpResponse *res, char *pszMsg);
bool	httpResponseSetContentChunked(struct HttpResponse *res, bool bChunked);
bool	httpResponseSetHeader(struct HttpResponse *res, char *pszName, char *pszValue);
bool	httpResponseSetHeaderf(struct HttpResponse *res, char *pszName, char *format, ...);
bool	httpResponseOut(struct HttpResponse *res, int nSockFd);
int	httpResponseOutChunk(int nSockFd, char *pszData, int nSize);
void	httpResponseFree(struct HttpResponse *res);
char	*httpResponseGetMsg(int nResCode);

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
char	*httpHeaderGetValue(Q_ENTRY *entries, char *name);
int	httpHeaderGetInt(Q_ENTRY *entries, char *name);
bool	httpHeaderHasString(Q_ENTRY *entries, char *name, char *str);
bool	httpHeaderParseRange(char *pszRangeHeader, uint64_t nFilesize, uint64_t *pnRangeOffset1, uint64_t *pnRangeOffset2, uint64_t *pnRangeSize);

// http_handler.c
struct HttpResponse *httpHandler(struct HttpRequest *req);

// http_method.c
int	httpMethodOptions(struct HttpRequest *req, struct HttpResponse *res);
int	httpMethodGet(struct HttpRequest *req, struct HttpResponse *res);
int	httpProcessGetNormalFile(struct HttpRequest *req, struct HttpResponse *res, char *pszFilePath, char *pszContentType);
int	httpMethodHead(struct HttpRequest *req, struct HttpResponse *res);
int	httpMethodNotImplemented(struct HttpRequest *req, struct HttpResponse *res);

// http_status.c
Q_OBSTACK *httpGetStatusHtml(void);

// http_accesslog.c
bool	httpAccessLog(struct HttpRequest *req, struct HttpResponse *res);

// mime.c
bool	mimeInit(char *pszFilepath);
bool	mimeFree(void);
char	*mimeDetect(char *pszFilename);

// util.c
unsigned int convIp2Uint(char *szIp);
unsigned long int microSleep(unsigned long microsec);
uint64_t convStr2Uint64(char *szNumStr);
char	*getExtentionFromFilename(char *szFilename, bool bIncludeDot);
float	diffTimeval(struct timeval *t1, struct timeval *t0);

bool	isCorrectFilename(char *pszPath);
bool	isCorrectPath(char *pszPath);
void	correctPath(char *pszPath);

// hook.c
bool	hookBeforeMainInit(void);
bool	hookAfterConfigLoaded(void);
bool	hookAfterDaemonInit(void);
int	hookWhileDaemonIdle(void);
bool	hookBeforeDaemonEnd(void);
bool	hookAfterDaemonSIGHUP(void);
bool	hookAfterChildInit(void);
bool	hookBeforeChildEnd(void);
bool	hookAfterConnEstablished(void);
int	hookMethodHandler(struct HttpRequest *req, struct HttpResponse *res);

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
