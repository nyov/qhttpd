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

char	*m_mimedefault = NULL;
Q_ENTRY *m_mimelist = NULL;

bool mimeInit(char *pszFilepath, char *pszMimeDefault) {
	if(m_mimelist != NULL) return false;

	m_mimedefault = pszMimeDefault;
	m_mimelist = qEntryLoad(pszFilepath, false);


	if(m_mimelist == NULL) return false;
	return true;
}

bool mimeFree(void) {
	if(m_mimelist == NULL) return false;
	qEntryFree(m_mimelist);
	m_mimelist = NULL;
	return true;
}

char *mimeDetect(char *pszFilename) {
	if(pszFilename == NULL || m_mimelist == NULL) return m_mimedefault;

	char *pszExt = getExtestionFromFilename(pszFilename, false);
	char *mimetype = qEntryGetValueNoCase(m_mimelist, pszExt);
	free(pszExt);

	if(mimetype == NULL) return m_mimedefault;
	return mimetype;
}
