## Copyright 2000-2010 The qDecoder Project. All rights reserved.
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
##
## $Id: configure.ac 554 2010-03-11 05:21:43Z wolkykim $

AC_COPYRIGHT([
=============================================================================
Copyright 2000-2010 The qDecoder Project. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE QDECODER PROJECT ``AS IS'' AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE QDECODER PROJECT BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
=============================================================================
])

## Internal functions
AC_DEFUN([Q_ARG_ENABLE], [
	AC_ARG_ENABLE([$1],[AS_HELP_STRING([--enable-$1], [$2])],,[enableval=no])
	if test "$enableval" = yes; then
		AC_MSG_NOTICE(['$1' feature is enabled])
		opt_defs="$opt_defs $3"
	fi
])

AC_DEFUN([Q_ARG_DISABLE], [
	AC_ARG_ENABLE([$1],[AS_HELP_STRING([--disable-$1], [$2])],,[enableval=yes])
	if test "$enableval" = no; then
		AC_MSG_NOTICE(['$1' feature is disabled])
		opt_defs="$opt_defs $3"
	fi
])

## Initialize
PATH=$PATH:/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin

AC_INIT([qHttpd], [1 RELEASE], [http://www.qdecoder.org/])
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_FILES([Makefile src/Makefile])

## Set autoconf setting
#AC_CANONICAL_TARGET
AC_PREFIX_DEFAULT([/usr/local/qhttpd])
#AC_PRESERVE_HELP_ORDER

## Checks for programs.
AC_PROG_CC
if test $ac_compiler_gnu = yes; then
	CFLAGS="$CFLAGS -Wall"
#else
#	AC_MSG_FAILURE([GCC is required to compile qDecoder.])
fi

AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_PATH_PROG(RM, rm)

## Checks for header files.
AC_HEADER_STDC
AC_HEADER_STDBOOL
AC_HEADER_STAT
AC_HEADER_DIRENT

## Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T
AC_TYPE_OFF_T

## Checks for library functions.
#AC_CHECK_FUNCS([socket sendfile])

## Set additional arguments
opt_defs=""
opt_incs=""
opt_libs=""
opt_objs=""

## Checks for 3rd libraries.
AC_CHECK_HEADER([qDecoder.h], [], AC_MSG_ERROR([qDecoder library 10.1.2 or above must be installed.]))
AC_CHECK_LIB([qDecoder], [main], [], AC_MSG_ERROR([qDecoder library 10.1.2 or above must be installed.]))
opt_libs="$opt_libs -lqDecoder"

Q_ARG_ENABLE([luascript], [enable LUA script engine])
if test "$enableval" = yes; then
	AC_CHECK_HEADER([lua.h], [], AC_MSG_ERROR([Lua library 5.1.4 or above must be installed.]))
	AC_CHECK_LIB([lua], [main], [], AC_MSG_ERROR([Lua library 5.1.4 or above must be installed to enable Lua Script feature.]))
	opt_defs="$opt_defs -DENABLE_LUA"
	opt_libs="$opt_libs -llua -ldl -lm"
	opt_objs="$opt_objs luascript.o"
fi

Q_ARG_ENABLE([hook], [enable hook support - compile with hook.c])
if test "$enableval" = yes; then
	opt_defs="$opt_defs -DENABLE_HOOK"
	opt_objs="$opt_objs hook.o"
fi

Q_ARG_DISABLE([debug], [disable debug build - take off all debugging codes], [])
if test "$enableval" = yes; then
	#AC_MSG_NOTICE(['debug' feature is enabled])
	opt_defs="$opt_defs -DBUILD_DEBUG"
fi

AC_MSG_NOTICE([optional flags: $opt_defs $opt_incs $opt_libs])
AC_SUBST([OPT_DEFS], [$opt_defs] )
AC_SUBST([OPT_INCS], [$opt_incs] )
AC_SUBST([OPT_LIBS], [$opt_libs] )
AC_SUBST([OPT_OBJS], [$opt_objs] )

## Create Makefile
AC_OUTPUT
