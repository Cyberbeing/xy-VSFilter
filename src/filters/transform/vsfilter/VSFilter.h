/* 
 *	Copyright (C) 2003-2006 Gabest
 *	http://www.gabest.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *   
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *   
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. 
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#pragma once

#include "resource.h"

class CVSFilterApp : public CWinApp
{
public:
	CVSFilterApp();

	CString m_AppName;

    virtual
    UINT GetProfileInt(LPCTSTR lpszSection, LPCTSTR lpszEntry, int nDefault);
    UINT GetProfileInt(UINT    idSection  , LPCTSTR lpszEntry, int nDefault);
    UINT GetProfileInt(LPCTSTR lpszSection, UINT    idEntry,   int nDefault);
    UINT GetProfileInt(UINT    idSection  , UINT    idEntry,   int nDefault);

    virtual
    BOOL WriteProfileInt(LPCTSTR lpszSection, LPCTSTR lpszEntry, int nValue);
    BOOL WriteProfileInt(UINT    idSection  , LPCTSTR lpszEntry, int nValue);
    BOOL WriteProfileInt(LPCTSTR lpszSection, UINT    idEntry,   int nValue);
    BOOL WriteProfileInt(UINT    idSection  , UINT    idEntry,   int nValue);

    virtual
    CString GetProfileString(LPCTSTR lpszSection, LPCTSTR lpszEntry, LPCTSTR lpszDefault = NULL);
    CString GetProfileString(UINT    idSection  , LPCTSTR lpszEntry, LPCTSTR lpszDefault = NULL);
    CString GetProfileString(LPCTSTR lpszSection, UINT    idEntry,   LPCTSTR lpszDefault = NULL);
    CString GetProfileString(UINT    idSection  , UINT    idEntry,   LPCTSTR lpszDefault = NULL);

    virtual
    BOOL WriteProfileString(LPCTSTR lpszSection, LPCTSTR lpszEntry, LPCTSTR lpszValue);
    BOOL WriteProfileString(UINT    idSection  , LPCTSTR lpszEntry, LPCTSTR lpszValue);
    BOOL WriteProfileString(LPCTSTR lpszSection, UINT    idEntry,   LPCTSTR lpszValue);
    BOOL WriteProfileString(UINT    idSection  , UINT    idEntry,   LPCTSTR lpszValue);

    virtual
    BOOL GetProfileBinary(LPCTSTR lpszSection, LPCTSTR lpszEntry, LPBYTE* ppData, UINT* pBytes);
    BOOL GetProfileBinary(UINT    idSection  , LPCTSTR lpszEntry, LPBYTE* ppData, UINT* pBytes);
    BOOL GetProfileBinary(LPCTSTR lpszSection, UINT    idEntry,   LPBYTE* ppData, UINT* pBytes);
    BOOL GetProfileBinary(UINT    idSection  , UINT    idEntry,   LPBYTE* ppData, UINT* pBytes);

    virtual
    BOOL WriteProfileBinary(LPCTSTR lpszSection, LPCTSTR lpszEntry, LPBYTE pData, UINT nBytes);
    BOOL WriteProfileBinary(UINT    idSection  , LPCTSTR lpszEntry, LPBYTE pData, UINT nBytes);
    BOOL WriteProfileBinary(LPCTSTR lpszSection, UINT    idEntry,   LPBYTE pData, UINT nBytes);
    BOOL WriteProfileBinary(UINT    idSection  , UINT    idEntry,   LPBYTE pData, UINT nBytes);
protected:
	HINSTANCE LoadAppLangResourceDLL();

public:
	BOOL InitInstance();
	BOOL ExitInstance();

	DECLARE_MESSAGE_MAP()
};

extern CVSFilterApp theApp;

#define ResStr(id) CString(MAKEINTRESOURCE(id))
