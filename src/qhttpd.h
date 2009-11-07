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
#define PRG_VERSION				"1.2.1"

//
// MAXIMUM INTERNAL LIMITATIONS
//

#define MAX_CHILDS				(256)
#define MAX_SEMAPHORES				(1+2)
#define MAX_SEMAPHORES_LOCK_SECS		(10)		// maximum secondes which semaphores can be locked

#define MAX_HTTP_MEMORY_CONTENTS		(1024*1024)	// if the contents size is less than this, do not use temporary file
#define	MAX_SHUTDOWN_WAIT			(5000)		// maximum ms for waiting input stream after socket shutdown
#define	MAX_LINGER_TIMEOUT			(15)

#define	MAX_USERCOUNTER				(10)		// amount of custom counter in shared memory for customizing purpose

#define MAX_LOGLEVEL				(4)

#define URL_MAX					(PATH_MAX)
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
	char	szConfigFile[PATH_MAX];

	char	szRunDir[PATH_MAX];
	char	szLogDir[PATH_MAX];
	char	szDataDir[PATH_MAX];
	char	szTmpDir[PATH_MAX];

	char	szMimeFile[PATH_MAX];

	char	szPidfile[PATH_MAX];
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
	bool	bIgnoreOverConnection;

	int	nResponseExpires;

	bool	bEnableLua;
	char	szLuaScript[PATH_MAX];

	char	szErrorLog[64+1];
	char	szAccessLog[64+1];
	int	nLogRotate;
	int	nLogLevel;

	bool	bStatusEnable;
	char	szStatusUrl[URL_MAX];
} Config;

//
// SHARED STRUCTURES
//

struct SharedData {
	// daemon info
	time_t	nStartTime;
	int	nTotalLaunched;			// total launched childs counter
	int	nCurrentChilds;			// currently working childs counter

	int	nTotalConnected;		// total connection counter
	int	nTotalRequests;			// total processed requests counter

	// child info
	struct child {
		int     nPid;			// pid, 0 means empty slot
		int	nTotalConnected;	// total connection counter for this slot
		int	nTotalRequests;		// total processed requests counter for this slot
		time_t  nStartTime;		// start time for this slot
		bool	bExit;			// flag for request exit after done working request

		struct {			// connected client information
			bool	bConnected;	// flag for connection established
			time_t  nStartTime;	// connection established time
			time_t  nEndTime;	// connection closed time
			int	nTotalRequests; // keep-alive requests counter

			int     nSockFd;	// socket descriptor
			char    szAddr[15+1];	// client IP address
			unsigned int nAddr;	// client IP address
			int     nPort;		// client port number

			bool	bRun;			// flag for working
			struct	timeval tvReqTime;	// request time
			struct	timeval tvResTime;	// response time
			char	szReqInfo[1024+1];	// additional request information
			int	nResponseCode;		// response code
		} conn;
	} child[MAX_CHILDS];

	// extra info
	int	nUserCounter[MAX_USERCOUNTER];
};

//
// HTTP STRUCTURES
//
struct HttpRequest {
	// connection info
	int	nSockFd;		// socket descriptor
	int	nTimeout;		// timeout value for this request

	// request status
	int	nReqStatus;		// request status 1:ok, 0:bad request, -1:timeout, -2:connection closed

	// request line
	char*	pszRequestMethod;	// request method		ex) GET
	char*	pszRequestUri;		// url+query.			ex) /data%20path?query=the%20value
	char*	pszHttpVersion;		// version			ex) HTTP/1.1

	// parsed request information
	char*	pszRequestHost;		// host				ex) www.domain.com
	char*	pszRequestPath;		// decoded path			ex) /data path
	char*	pszQueryString;		// query string			ex) query=the%20value

	// request header
	Q_ENTRY *pHeaders;		// request headers

	// contents
	off_t	nContentsLength;	// contents length 0:no contents, n>0:has contents
	char*	pContents;		// contents data if parsed (if contents does not parsed : nContentsLength>0 && pContents==NULL)
};

struct HttpResponse {
	bool	bOut;			// flag for response out already

	char*	pszHttpVersion;		// response protocol
	int	nResponseCode;		// response code

	Q_ENTRY* pHeaders;		// response headers

	char	*pszContentType;	// contents mime type
	off_t	nContentLength;		// contents length
	char*	pContent;		// contents data
	bool	bChunked;		// flag for chunked data out
};

//
// DEFINITION FUNCTIONS
//
#define	CONST_STRLEN(x)		(sizeof(x) - 1)

#define _LOG(log, level, prestr, fmt, args...)	do {					\
	if (g_loglevel >= level) {							\
		char _timestr[14+1];							\
		qTimeGetLocalStrf(_timestr, sizeof(_timestr), 0, "%Y%m%d%H%M%S");	\
		if(log != NULL)								\
			log->writef(log, "%s(%d):" prestr fmt					\
			, _timestr, getpid(), ##args);					\
		else 									\
			printf("%s(%d):" prestr fmt "\n"				\
			, _timestr, getpid(), ##args);					\
	}										\
} while(0)

#define _LOG2(log, level, prestr, fmt, args...)	do {					\
	if (g_loglevel >= level) {							\
		char _timestr[14+1];							\
		qTimeGetLocalStrf(_timestr, sizeof(_timestr), 0, "%Y%m%d%H%M%S");	\
		if(log != NULL)								\
			log->writef(log, "%s(%d):" prestr fmt " (%s:%d)"			\
			, _timestr, getpid(), ##args, __FILE__, __LINE__);		\
		else									\
			printf("%s(%d):" prestr fmt " (%s:%d)\n"			\
			, _timestr, getpid(), ##args, __FILE__, __LINE__);		\
	}										\
} while(0)

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

// http_main.c
extern	int		httpMain(int nSockFd);
extern	int		httpRequestHandler(struct HttpRequest *req, struct HttpResponse *res);
extern	int		httpSpecialRequestHandler(struct HttpRequest *req, struct HttpResponse *res);

// http_request.c
extern	struct	HttpRequest*	httpRequestParse(int nSockFd, int nTimeout);
extern	bool		httpRequestFree(struct HttpRequest *req);

// http_response.c
extern	struct HttpResponse* httpResponseCreate(void);
extern	int		httpResponseSetSimple(struct HttpRequest *req, struct HttpResponse *res, int nResCode, bool nKeepAlive, const char *format, ...);
extern	bool		httpResponseSetCode(struct HttpResponse *res, int nResCode, struct HttpRequest *req, bool bKeepAlive);
extern	bool		httpResponseSetContent(struct HttpResponse *res, const char *pszContentType, off_t nContentLength, const char *pContent);
extern	bool		httpResponseSetContentHtml(struct HttpResponse *res, const char *pszMsg);
extern	bool		httpResponseSetContentChunked(struct HttpResponse *res, bool bChunked);
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
extern	const char*	httpHeaderGetStr(Q_ENTRY *entries, const char *pszName);
extern	int		httpHeaderGetInt(Q_ENTRY *entries, const char *pszName);
extern	bool		httpHeaderSetStr(Q_ENTRY *entries, const char *pszName, const char *pszValue);
extern	bool		httpHeaderSetStrf(Q_ENTRY *entries, const char *pszName, const char *format, ...);
extern	bool		httpHeaderRemove(Q_ENTRY *entries, const char *pszName);
extern	bool		httpHeaderHasStr(Q_ENTRY *entries, const char *pszName, const char *pszValue);
extern	bool		httpHeaderParseRange(const char *pszRangeHeader, off_t nFilesize, off_t *pnRangeOffset1, off_t *pnRangeOffset2, off_t *pnRangeSize);

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
extern	off_t		streamSave(int nFd, int nSockFd, off_t nSize, int nTimeoutMs);
extern	off_t		streamSendfile(int nSockFd, int nFd, off_t nOffset, off_t nSize);

// hook.c
#ifdef ENABLE_HOOK
extern	bool		hookBeforeMainInit(void);
extern	bool		hookAfterConfigLoaded(void);
extern	bool		hookAfterDaemonInit(void);
extern	int		hookWhileDaemonIdle(void);
extern	bool		hookBeforeDaemonEnd(void);
extern	bool		hookAfterDaemonSIGHUP(void);

extern	bool		hookAfterChildInit(void);
extern	bool		hookBeforeChildEnd(void);
extern	bool		hookAfterConnEstablished(int nSockFd);

extern	int		hookRequestHandler(struct HttpRequest *req, struct HttpResponse *res);
extern	bool		hookResponseHandler(struct HttpRequest *req, struct HttpResponse *res);
#endif

// script.c
#ifdef ENABLE_LUA
extern	bool		luaInit(const char *pszScriptPath);
extern	bool		luaFree(void);
extern	int		luaRequestHandler(struct HttpRequest *req, struct HttpResponse *res);
extern	bool		luaResponseHandler(struct HttpRequest *req, struct HttpResponse *res);
#endif

// util.c
extern	unsigned int	convIp2Uint(const char *szIp);
extern	float		diffTimeval(struct timeval *t1, struct timeval *t0);
extern	bool		isCorrectPath(const char *pszPath);
extern	void		correctPath(char *pszPath);

//
// GLOBAL VARIABLES
//
extern bool	g_debug;
extern Config	g_conf;
extern int	g_semid;
extern Q_LOG	*g_errlog;
extern Q_LOG	*g_acclog;
extern int	g_loglevel;
extern sigset_t	g_sigflags;

#endif	// _QHTTPD_H
