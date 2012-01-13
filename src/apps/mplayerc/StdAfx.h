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

// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

// VS2005 changed result of OnNCHitTest
#if _MSC_VER >= 1400

#include "../../../../include/stdafx_common.h"
#undef _ATL_CSTRING_EXPLICIT_CONSTRUCTORS
#include "../../../../include/stdafx_common_afx2.h"
#include "../../../../include/stdafx_common_dshow.h"

#include <afxdlgs.h>
#include <Shlwapi.h>
#include <atlcoll.h>
#include <atlpath.h>

#include "..\..\..\include\qt\qt.h"
#include "..\..\ui\ui.h"
#include "..\..\DSUtil\DSUtil.h"

template <class T = CString, class S = CString>
class CAtlStringMap : public CAtlMap<S, T, CStringElementTraits<S> > {};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STDAFX_H__C76533D6_6242_4BEB_8FD3_C6BE58F07224__INCLUDED_)
