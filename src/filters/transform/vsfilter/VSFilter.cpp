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

#include "stdafx.h"
#include "DirectVobSubFilter.h"
#include "DirectVobSubPropPage.h"
#include "VSFilter.h"
#include "..\..\..\DSUtil\MediaTypes.h"

#include <initguid.h>
#include "..\..\..\..\include\moreuuids.h"
#include "xy_sub_filter.h"

/////////////////////////////////////////////////////////////////////////////
// CVSFilterApp 

BEGIN_MESSAGE_MAP(CVSFilterApp, CWinApp)
END_MESSAGE_MAP()

CVSFilterApp::CVSFilterApp()
{
#if ENABLE_XY_LOG
    LPTSTR  strDLLPath = new TCHAR[_MAX_PATH];
    ::GetModuleFileName( reinterpret_cast<HINSTANCE>(&__ImageBase), strDLLPath, _MAX_PATH);
    CString dllPath = strDLLPath;
    dllPath += ".properties";
    xy_logger::doConfigure( dllPath.GetString() );
#endif
}

extern "C" BOOL WINAPI DllEntryPoint(HINSTANCE, ULONG, LPVOID);

BOOL CVSFilterApp::InitInstance()
{
	if(!CWinApp::InitInstance())
		return FALSE;

	SetRegistryKey(_T("Gabest"));

	DllEntryPoint(AfxGetInstanceHandle(), DLL_PROCESS_ATTACH, 0); // "DllMain" of the dshow baseclasses

	STARTUPINFO si;
	GetStartupInfo(&si);
	m_AppName = CString(si.lpTitle);
	m_AppName.Replace('\\', '/');
	m_AppName = m_AppName.Mid(m_AppName.ReverseFind('/')+1);
	m_AppName.MakeLower();

	return TRUE;
}

int CVSFilterApp::ExitInstance()
{
	DllEntryPoint(AfxGetInstanceHandle(), DLL_PROCESS_DETACH, 0); // "DllMain" of the dshow baseclasses

	return CWinApp::ExitInstance();
}

HINSTANCE CVSFilterApp::LoadAppLangResourceDLL()
{
    CString fn;
    fn.ReleaseBufferSetLength(::GetModuleFileName(m_hInstance, fn.GetBuffer(MAX_PATH), MAX_PATH));
    fn = fn.Mid(fn.ReverseFind('\\')+1);
    fn = fn.Left(fn.ReverseFind('.')+1);
    fn = fn + _T("lang");
    return ::LoadLibrary(fn);
}

CVSFilterApp theApp;

//////////////////////////////////////////////////////////////////////////

const AMOVIESETUP_MEDIATYPE sudPinTypesIn[] =
{
    {&MEDIATYPE_NULL, &MEDIASUBTYPE_NULL},
    {&MEDIATYPE_Video, &MEDIASUBTYPE_YUY2},
    {&MEDIATYPE_Video, &MEDIASUBTYPE_YV12},
    {&MEDIATYPE_Video, &MEDIASUBTYPE_I420},
    {&MEDIATYPE_Video, &MEDIASUBTYPE_IYUV},
    {&MEDIATYPE_Video, &MEDIASUBTYPE_P010},
    {&MEDIATYPE_Video, &MEDIASUBTYPE_P016},
    {&MEDIATYPE_Video, &MEDIASUBTYPE_NV12},
    {&MEDIATYPE_Video, &MEDIASUBTYPE_NV21},
    {&MEDIATYPE_Video, &MEDIASUBTYPE_AYUV},
    {&MEDIATYPE_Video, &MEDIASUBTYPE_RGB32},
    {&MEDIATYPE_Video, &MEDIASUBTYPE_RGB565},
    {&MEDIATYPE_Video, &MEDIASUBTYPE_RGB555},
    {&MEDIATYPE_Video, &MEDIASUBTYPE_RGB24},
};

const AMOVIESETUP_MEDIATYPE sudPinTypesIn2[] =
{
    {&MEDIATYPE_Text, &MEDIASUBTYPE_None},
    {&MEDIATYPE_Text, &GUID_NULL},
    {&MEDIATYPE_Subtitle, &MEDIASUBTYPE_None},
    {&MEDIATYPE_Subtitle, &MEDIASUBTYPE_UTF8},
    {&MEDIATYPE_Subtitle, &MEDIASUBTYPE_SSA},
    {&MEDIATYPE_Subtitle, &MEDIASUBTYPE_ASS},
    {&MEDIATYPE_Subtitle, &MEDIASUBTYPE_ASS2},
    {&MEDIATYPE_Subtitle, &MEDIASUBTYPE_SSF},
    {&MEDIATYPE_Subtitle, &MEDIASUBTYPE_VOBSUB},
    {&MEDIATYPE_Subtitle, &MEDIASUBTYPE_HDMVSUB},
    {&MEDIATYPE_Subtitle, &MEDIASUBTYPE_DVB_SUBTITLES}
};

const AMOVIESETUP_MEDIATYPE sudPinTypesOut[] =
{
	{&MEDIATYPE_Video, &MEDIASUBTYPE_None},
};

const AMOVIESETUP_PIN sudpPins[] =
{
    {L"Input", FALSE, FALSE, FALSE, FALSE, &CLSID_NULL, NULL, countof(sudPinTypesIn), sudPinTypesIn},
    {L"Output", FALSE, TRUE, FALSE, FALSE, &CLSID_NULL, NULL, countof(sudPinTypesOut), sudPinTypesOut},
    {L"Input2", TRUE, FALSE, FALSE, TRUE, &CLSID_NULL, NULL, countof(sudPinTypesIn2), sudPinTypesIn2}
};

const AMOVIESETUP_PIN sudpPins2[] =
{
    {L"Input", TRUE, FALSE, FALSE, TRUE, &CLSID_NULL, NULL, countof(sudPinTypesIn2), sudPinTypesIn2}
};

/*const*/ AMOVIESETUP_FILTER sudFilter[] =
{
    {&__uuidof(CDirectVobSubFilter), L"DirectVobSub", MERIT_DO_NOT_USE, countof(sudpPins), sudpPins},
    {&__uuidof(CDirectVobSubFilter2), L"DirectVobSub (auto-loading version)", MERIT_PREFERRED+2, countof(sudpPins), sudpPins},
    {&__uuidof(XySubFilter), L"XySubFilter", MERIT_PREFERRED+2, countof(sudpPins2), sudpPins2}, 
};

// Normally nobody would like to have DirectVodSubFilter and XySubFilter working together.
// They'll be packed into two different DLLs.
// But we'll still make it possible to have both them register (and work together?).
#ifndef XY_SUB_FILTER_DLL
// DirectVodSubFilter set
CFactoryTemplate g_Templates[] =
{
    {sudFilter[0].strName, sudFilter[0].clsID, CreateInstance<CDirectVobSubFilter>, NULL, &sudFilter[0]},
    {sudFilter[1].strName, sudFilter[1].clsID, CreateInstance<CDirectVobSubFilter2>, NULL, &sudFilter[1]},
    {L"DVSMainPPage", &__uuidof(CDVSMainPPage), CreateInstance<CDVSMainPPage>},
    {L"DVSGeneralPPage", &__uuidof(CDVSGeneralPPage), CreateInstance<CDVSGeneralPPage>},
    {L"DVSMiscPPage", &__uuidof(CDVSMiscPPage), CreateInstance<CDVSMiscPPage>},
    {L"DVSMorePPage", &__uuidof(CDVSMorePPage), CreateInstance<CDVSMorePPage>},
    {L"DVSTimingPPage", &__uuidof(CDVSTimingPPage), CreateInstance<CDVSTimingPPage>},
    {L"DVSZoomPPage", &__uuidof(CDVSZoomPPage), CreateInstance<CDVSZoomPPage>},
    {L"DVSColorPPage", &__uuidof(CDVSColorPPage), CreateInstance<CDVSColorPPage>},
    {L"DVSPathsPPage", &__uuidof(CDVSPathsPPage), CreateInstance<CDVSPathsPPage>},
    {L"DVSAboutPPage", &__uuidof(CDVSAboutPPage), CreateInstance<CDVSAboutPPage>},
};
#else
// XySubFilter set
CFactoryTemplate g_Templates[] =
{
    {sudFilter[2].strName, sudFilter[2].clsID, CreateInstance<XySubFilter>, NULL, &sudFilter[2]},
    //fix me: may conflicts with DirectVodSubFilter set if people register both DirectVodSubFilter and XySubFilter
    {L"DVSMainPPage", &__uuidof(CDVSMainPPage), CreateInstance<CDVSMainPPage>},
    {L"DVSGeneralPPage", &__uuidof(CDVSGeneralPPage), CreateInstance<CDVSGeneralPPage>},
    {L"DVSMiscPPage", &__uuidof(CDVSMiscPPage), CreateInstance<CDVSMiscPPage>},
    {L"DVSMorePPage", &__uuidof(CDVSMorePPage), CreateInstance<CDVSMorePPage>},
    {L"DVSTimingPPage", &__uuidof(CDVSTimingPPage), CreateInstance<CDVSTimingPPage>},
    {L"DVSZoomPPage", &__uuidof(CDVSZoomPPage), CreateInstance<CDVSZoomPPage>},
    {L"DVSColorPPage", &__uuidof(CDVSColorPPage), CreateInstance<CDVSColorPPage>},
    {L"DVSPathsPPage", &__uuidof(CDVSPathsPPage), CreateInstance<CDVSPathsPPage>},
    {L"DVSAboutPPage", &__uuidof(CDVSAboutPPage), CreateInstance<CDVSAboutPPage>},
};
#endif

int g_cTemplates = countof(g_Templates);

//////////////////////////////
/*removeme*/
extern void JajDeGonoszVagyok();

extern void RegisterXySubFilterAsAutoLoad();

CString XyUuidToString(const UUID& in_uuid)
{
    CString ret;
    ret.Format(_T("{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}"),
        in_uuid.Data1, in_uuid.Data2, in_uuid.Data3, in_uuid.Data4[0], in_uuid.Data4[1], 
        in_uuid.Data4[2], in_uuid.Data4[3], in_uuid.Data4[4], 
        in_uuid.Data4[5], in_uuid.Data4[6], in_uuid.Data4[7]);
    return ret;
}

void RegisterXySubFilterAsAutoLoad()
{
    HKEY hKey;

    if(RegCreateKeyEx(HKEY_CLASSES_ROOT, _T("Autoload.SubtitleProvider"), 0, NULL, REG_OPTION_NON_VOLATILE,
        KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS)
    {
        CString in_uuid = XyUuidToString(*sudFilter[2].clsID);
        if(RegSetValueEx(hKey, _T(""), NULL, REG_SZ, reinterpret_cast<const BYTE*>(in_uuid.GetString()), in_uuid.GetLength()*2+1) != ERROR_SUCCESS)
        {
            MessageBox(NULL, _T("Failed to install as autoload subtitle provider"), _T("Warning"), 0);
        }
        RegCloseKey(hKey);
    }
    else
    {
        MessageBox(NULL, _T("Failed to install as autoload subtitle provider"), _T("Warning"), 0);
    }
}

STDAPI DllRegisterServer()
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	if(theApp.GetProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_SEENDIVXWARNING), 0) != 1)
		theApp.WriteProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_SEENDIVXWARNING), 0);

	if(theApp.GetProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_VMRZOOMENABLED), -1) == -1)
		theApp.WriteProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_VMRZOOMENABLED), 0);

	if(theApp.GetProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_ENABLEZPICON), -1) == -1)
		theApp.WriteProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_ENABLEZPICON), 0);

#ifndef XY_SUB_FILTER_DLL
    /*removeme*/
    JajDeGonoszVagyok();
#endif

#ifdef XY_SUB_FILTER_DLL
    RegisterXySubFilterAsAutoLoad();
#endif

    return AMovieDllRegisterServer2(TRUE);
}

STDAPI DllUnregisterServer()
{
//	DVS_WriteProfileInt2(IDS_R_GENERAL, IDS_RG_SEENDIVXWARNING, 0);

    //ToDo: reset auto subtitle provider setting

	return AMovieDllRegisterServer2(FALSE);
}

void CALLBACK DirectVobSub(HWND hwnd, HINSTANCE hinst, LPSTR lpszCmdLine, int nCmdShow)
{
    if(FAILED(::CoInitialize(0))) return;

    CComPtr<IBaseFilter> pFilter;
    CComQIPtr<ISpecifyPropertyPages> pSpecify;

    if(SUCCEEDED(pFilter.CoCreateInstance(__uuidof(CDirectVobSubFilter))) && (pSpecify = pFilter))
    {
        ShowPPage(pFilter, hwnd);
    }

    ::CoUninitialize();
}

void CALLBACK XySubFilterConfiguration(HWND hwnd, HINSTANCE hinst, LPSTR lpszCmdLine, int nCmdShow)
{
    if(FAILED(::CoInitialize(0))) return;

    CComPtr<IBaseFilter> pFilter;
    CComQIPtr<ISpecifyPropertyPages> pSpecify;

    if(SUCCEEDED(pFilter.CoCreateInstance(__uuidof(XySubFilter))) && (pSpecify = pFilter))
    {
        ShowPPage(pFilter, hwnd);
    }

    ::CoUninitialize();
}
