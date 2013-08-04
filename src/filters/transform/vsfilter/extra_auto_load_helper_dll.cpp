#include "stdafx.h"

#include "xy_logger.h"

#include "..\..\..\DSUtil\MediaTypes.h"

#include <initguid.h>
#include "..\..\..\..\include\moreuuids.h"

#include "VSFilter.h"

#include "auto_load_helper.h"

[uuid("f844754a-440c-4684-a6f9-f5f80aacd59b")]
class XySubFilterExtraAutoLoader : public XySubFilterAutoLoader
{
public:
    XySubFilterExtraAutoLoader(LPUNKNOWN punk, HRESULT* phr, const GUID& clsid = __uuidof(XySubFilterExtraAutoLoader))
        : XySubFilterAutoLoader(punk, phr, clsid) {}

    //IBaseFilter
    STDMETHODIMP QueryFilterInfo( FILTER_INFO* pInfo )
    {
        CheckPointer(pInfo, E_POINTER);
        ValidateReadWritePtr(pInfo, sizeof(FILTER_INFO));

        HRESULT hr = __super::QueryFilterInfo(pInfo);
        if (SUCCEEDED(hr))
        {
            wcscpy_s(pInfo->achName, countof(pInfo->achName)-1, L"XySubFilterExtraAutoLoader");
        }
        return hr;
    }
};

//////////////////////////////////////////////////////////////////////////////////////////


const AMOVIESETUP_MEDIATYPE sudPinTypesIn[] = {
    {&MEDIATYPE_Video   , &GUID_NULL}
};

const AMOVIESETUP_PIN sudpPins[] =
{
    {L"Input", TRUE, FALSE, FALSE, TRUE, &CLSID_NULL, NULL, countof(sudPinTypesIn), sudPinTypesIn}
};

/*const*/ AMOVIESETUP_FILTER sudFilter = {
    &__uuidof(XySubFilterExtraAutoLoader), L"XySubFilterExtraAutoLoader", 0xffffffff, countof(sudpPins), sudpPins
};

CFactoryTemplate g_Templates[] =
{
    {sudFilter.strName, sudFilter.clsID, CreateInstance<XySubFilterExtraAutoLoader>, NULL, &sudFilter},
};

int g_cTemplates = countof(g_Templates);

//////////////////////////////

STDAPI DllRegisterServer()
{
    //important! This line is necessary.
    //Without it, MPC-HC crash when a none-registered XySubFilterExtraAutoLoader add to external filters
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    return AMovieDllRegisterServer2(TRUE);
}

STDAPI DllUnregisterServer()
{
    return AMovieDllRegisterServer2(FALSE);
}
