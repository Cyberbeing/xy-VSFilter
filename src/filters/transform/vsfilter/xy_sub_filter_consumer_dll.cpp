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
#include "xy_sub_filter_consumer.h"
#include "DirectVobSubPropPage.h"
#include "VSFilter.h"
#include "..\..\..\DSUtil\MediaTypes.h"

#include <initguid.h>
#include "..\..\..\..\include\moreuuids.h"

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

const AMOVIESETUP_MEDIATYPE sudPinTypesOut[] =
{
    {&MEDIATYPE_Video, &MEDIASUBTYPE_None},
};

const AMOVIESETUP_PIN sudpPins[] =
{
    {L"Input", FALSE, FALSE, FALSE, FALSE, &CLSID_NULL, NULL, countof(sudPinTypesIn), sudPinTypesIn},
    {L"Output", FALSE, TRUE, FALSE, FALSE, &CLSID_NULL, NULL, countof(sudPinTypesOut), sudPinTypesOut},
};

/*const*/ AMOVIESETUP_FILTER sudFilter[] =
{
    {&__uuidof(XySubFilterConsumer), L"XySubFilterConsumer", MERIT_DO_NOT_USE, countof(sudpPins), sudpPins},
};

CFactoryTemplate g_Templates[] =
{
    {sudFilter[0].strName, sudFilter[0].clsID, CreateInstance<XySubFilterConsumer>, NULL, &sudFilter[0]},
    {L"XySubFilterConsumerGeneralPPage", &__uuidof(CXySubFilterConsumerGeneralPPage), CreateInstance<CXySubFilterConsumerGeneralPPage>},
    {L"XySubFilterConsumerMiscPPage", &__uuidof(CXySubFilterConsumerMiscPPage), CreateInstance<CXySubFilterConsumerMiscPPage>},
    {L"XySubFilterConsumerColorPPage", &__uuidof(CXySubFilterConsumerColorPPage), CreateInstance<CXySubFilterConsumerColorPPage>},
    {L"XySubFilterConsumerAboutPPage", &__uuidof(CXySubFilterConsumerAboutPPage), CreateInstance<CXySubFilterConsumerAboutPPage>},
};

int g_cTemplates = countof(g_Templates);

//////////////////////////////

STDAPI DllRegisterServer()
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    return AMovieDllRegisterServer2(TRUE);
}

STDAPI DllUnregisterServer()
{
    return AMovieDllRegisterServer2(FALSE);
}

void CALLBACK XySubFilterConsumerConfiguration(HWND hwnd, HINSTANCE hinst, LPSTR lpszCmdLine, int nCmdShow)
{
    if(FAILED(::CoInitialize(0))) return;

    CComPtr<IBaseFilter> pFilter;
    CComQIPtr<ISpecifyPropertyPages> pSpecify;

    if(SUCCEEDED(pFilter.CoCreateInstance(__uuidof(XySubFilterConsumer))) && (pSpecify = pFilter))
    {
        ShowPPage(pFilter, hwnd);
    }

    ::CoUninitialize();
}
