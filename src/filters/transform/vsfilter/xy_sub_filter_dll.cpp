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
#include "xy_sub_filter.h"
#include "DirectVobSubPropPage.h"
#include "VSFilter.h"
#include "..\..\..\DSUtil\MediaTypes.h"

#include <initguid.h>
#include "..\..\..\..\include\moreuuids.h"

const AMOVIESETUP_MEDIATYPE sudPinTypesIn[] =
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

const AMOVIESETUP_PIN sudpPins[] =
{
    {L"Input", TRUE, FALSE, FALSE, TRUE, &CLSID_NULL, NULL, countof(sudPinTypesIn), sudPinTypesIn}
};

/*const*/ AMOVIESETUP_FILTER sudFilter[] =
{
    {&__uuidof(XySubFilter), L"XySubFilter", MERIT_DO_NOT_USE, countof(sudpPins), sudpPins}, 
};

CFactoryTemplate g_Templates[] =
{
    {sudFilter[0].strName, sudFilter[0].clsID, CreateInstance<XySubFilter>, NULL, &sudFilter[0]},
    {L"XySubFilterMainPPage", &__uuidof(CXySubFilterMainPPage), CreateInstance<CXySubFilterMainPPage>},
    {L"XySubFilterMorePPage", &__uuidof(CXySubFilterMorePPage), CreateInstance<CXySubFilterMorePPage>},
    {L"XySubFilterTimingPPage", &__uuidof(CXySubFilterTimingPPage), CreateInstance<CXySubFilterTimingPPage>},
    {L"XySubFilterPathsPPage", &__uuidof(CXySubFilterPathsPPage), CreateInstance<CXySubFilterPathsPPage>},
    {L"XySubFilterAboutPPage", &__uuidof(CXySubFilterAboutPPage), CreateInstance<CXySubFilterAboutPPage>},
};

int g_cTemplates = countof(g_Templates);

//////////////////////////////

extern void RegisterXySubFilterAsAutoLoad();


void RegisterXySubFilterAsAutoLoad()
{
    HKEY hKey;

    if(RegCreateKeyEx(HKEY_CLASSES_ROOT, _T("Autoload.SubtitleProvider"), 0, NULL, REG_OPTION_NON_VOLATILE,
        KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS)
    {
        CString in_uuid = XyUuidToString(*sudFilter[0].clsID);
        if(RegSetValueEx(hKey, _T("XySubFilter"), NULL, REG_SZ, reinterpret_cast<const BYTE*>(in_uuid.GetString()), in_uuid.GetLength()*2+1) != ERROR_SUCCESS)
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

    RegisterXySubFilterAsAutoLoad();

    return AMovieDllRegisterServer2(TRUE);
}

STDAPI DllUnregisterServer()
{
    //ToDo: reset auto subtitle provider setting

    return AMovieDllRegisterServer2(FALSE);
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
