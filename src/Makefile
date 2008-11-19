# Copyright 2008 The qDecoder Project. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE QDECODER PROJECT ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE QDECODER PROJECT BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Compiler arguments
CC	= gcc
CFLAGS  = -Wall
DEFS_CT	=
#DEFS_64	= -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
DEFS	= ${DEFS_64} ${DEFS_CT}

INCLUDE = -I./				\
	  -I/usr/include		\
	  -I/usr/local/include/		\

#LIB	= -L/usr/local/lib -lqDecoder -lz -lm
LIB	= -L/usr/local/lib /usr/local/lib/libqDecoder.a -lz -lm

OBJDEF	= main.o version.o config.o daemon.o child.o pool.o hook.o mime.o \
	  http_main.o http_request.o http_response.o http_header.o http_method.o http_status.o http_accesslog.o \
	  stream.o util.o
OBJ	= ${OBJDEF}

## Make Library
all:	qhttpd

qhttpd: ${OBJ}
	${CC} ${CFLAGS} ${OBJ} ${LIB} -o qhttpd

reall: clean all

debug:
	$(MAKE) CFLAGS="-Wall -g -O" DEFS_CT="-DBUILD_DEBUG"

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
