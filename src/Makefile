## Copyright 2008 The qDecoder Project. All rights reserved.
##
## Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that the following conditions
## are met:
##
## 1. Redistributions of source code must retain the above copyright
##    notice, this list of conditions and the following disclaimer.
## 2. Redistributions in binary form must reproduce the above copyright
##    notice, this list of conditions and the following disclaimer in the
##    documentation and/or other materials provided with the distribution.
##
## THIS SOFTWARE IS PROVIDED BY THE QDECODER PROJECT ``AS IS'' AND ANY
## EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
## WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
## DISCLAIMED. IN NO EVENT SHALL THE QDECODER PROJECT BE LIABLE FOR ANY
## DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
## (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
## LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
## ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
## (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
## THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

## 3rd party libraries
QDECODER_INC = -I/usr/local/include
QDECODER_LIB = /usr/local/lib/libqDecoder.a

LUA_INC = -I/usr/local/include
LUA_LIB = /usr/local/lib/liblua.a -lm
## comment out below line on FreeBSD
LUA_LIB += -ldl

## Compiler arguments
CC	= gcc
CFLAGS  = -Wall
DEFS	+= ${DEFS_CT}
DEFS	+= -D_GNU_SOURCE -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64

INCLUDE = -I./				\
	  -I/usr/include		\
	  -I/usr/local/include

LIB	= -L/usr/local/lib

OBJ	= main.o version.o config.o daemon.o child.o pool.o mime.o script.o \
	  http_main.o http_request.o http_response.o http_header.o http_method.o http_status.o http_accesslog.o \
	  stream.o util.o

## qDecoder library
INCLUDE	+= ${QDECODER_INC}
LIB	+= ${QDECODER_LIB}

## Enable hook
#DEFS	+= -DENABLE_HOOK
#OBJ	+= hook.o

## Lua script engine
DEFS	+= -DENABLE_LUA
INCLUDE	+= ${LUA_INC}
LIB	+= ${LUA_LIB}

## For release build
ifeq ($(BUILD), debug)
	CFLAGS += -g -O
	DEFS   += -DBUILD_DEBUG
endif

## Make Library
all:	qhttpd

qhttpd: ${OBJ}
	${CC} ${CFLAGS} ${OBJ} ${LIB} -o qhttpd

reall: clean all

debug:
	$(MAKE) BUILD="debug"

install: all
	install -m 0755 qhttpd ../bin/

## Compile Module
.SUFFIXES: .c .cpp .o

.c.o:
	${CC} ${CFLAGS} ${INCLUDE} ${DEFS} -c -o $@ $<

.cpp.o:
	${CC} ${CFLAGS} ${INCLUDE} ${DEFS} -c -o $@ $<

## Clear Module
clean:
	rm -f ${OBJ} qhttpd
