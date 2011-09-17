// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS	// some CString constructors will be explicit

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN		// Exclude rarely-used stuff from Windows headers
#endif

#if _MSC_VER >= 1400 // VS2005 changed result of OnNCHitTest
#define HITTEST_RET LRESULT
#else
#define HITTEST_RET UINT
#endif

#include <afx.h>
#include <afxwin.h>         // MFC core and standard components

// TODO: reference additional headers your program requires here
