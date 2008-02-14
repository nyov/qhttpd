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

void printUsages(void) {
	printVersion();
	printf("Usage: %s [-h] [-d] [-D] -c configfile\n", PRG_NAME);
	printf("  -h            Display this help message and exit.\n");
	printf("  -v            Version info.\n");
	printf("  -d            Run as debugging mode.\n");
	printf("  -D            Do not daemonize.\n");
	printf("  -c configfile Set configuration file.\n");
}

void printVersion(void) {
	char *pszBuildMode = "RELEASE";
#ifdef BUILD_DEBUG
	pszBuildMode = "DEBUG";
#endif

	printf("%s v%s (%s; %s; %s)\n",
	    PRG_NAME, PRG_VERSION, __DATE__, __TIME__,
	    pszBuildMode);
}
