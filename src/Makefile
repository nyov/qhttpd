###########################################################################
## qHttpd - Specific Purpose Web Server             http://www.qDecoder.org
##
## Copyright (C) 2007 Seung-young Kim.
##
## This program is free software: you can redistribute it and/or modify
## it under the terms of the GNU Lesser General Public License as published
## by the Free Software Foundation, either version 3 of the License, or
## (at your option) any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU Lesser General Public License for more details.
##
## You should have received a copy of the GNU Lesser General Public License
## along with this program.  If not, see <http://www.gnu.org/licenses/>.
###########################################################################

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

OBJDEF	= main.o version.o config.o daemon.o child.o child_main.o pool.o  mime.o stream.o hook.o util.o \
	  http_request.o http_response.o http_header.o http_handler.o http_method.o http_status.o http_accesslog.o
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
