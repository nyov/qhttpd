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


#define DEF_MIME_ENTRY	"_DEF_"
#define DEF_MIME_TYPE	"application/octet-stream"

Q_ENTRY *m_mimelist = NULL;

bool mimeInit(const char *pszFilepath) {
	if(m_mimelist != NULL) return false;

	m_mimelist = qConfigParseFile(NULL, pszFilepath, '=');

	if(m_mimelist == NULL) return false;
	return true;
}

bool mimeFree(void) {
	if(m_mimelist == NULL) return false;
	qEntryFree(m_mimelist);
	m_mimelist = NULL;
	return true;
}

const char *mimeDetect(const char *pszFilename) {
	if(pszFilename == NULL || m_mimelist == NULL) return DEF_MIME_TYPE;

	char *pszExt = qFileGetExt(pszFilename);
	char *pszMimetype = (char *)qEntryGetStrCase(m_mimelist, pszExt);
	free(pszExt);

	if(pszMimetype == NULL) pszMimetype = (char*)qEntryGetStr(m_mimelist, DEF_MIME_ENTRY);
	if(pszMimetype == NULL) pszMimetype = DEF_MIME_TYPE;

	return pszMimetype;
}
