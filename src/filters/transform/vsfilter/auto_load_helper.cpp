#include "stdafx.h"
#include "VSFilter.h"
#include "auto_load_helper.h"
#include "moreuuids.h"
#include "xy_sub_filter.h"
#include "DirectVobSubFilter.h"
#include "IDirectVobSubXy.h"

#if ENABLE_XY_LOG_TRACE_GRAPH
#  define XY_DUMP_GRAPH(graph,level) xy_logger::XyDumpGraph(graph,level)
#else
#  define XY_DUMP_GRAPH(graph,level)
#endif

////////////////////////////////////////////////////////////////////////////

bool ShouldWeAutoLoad(IFilterGraph* pGraph)
{
    XY_LOG_INFO(_T(""));

    HRESULT hr = NOERROR;

    int level =   CDirectVobSub::LOADLEVEL_WHEN_NEEDED;
    CString str_load_level = theApp.GetProfileString(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_LOADLEVEL), _T("when_needed"));
    str_load_level.MakeLower();
    if (str_load_level.Compare(_T("always"))==0)
    {
        level = CDirectVobSub::LOADLEVEL_ALWAYS;
    }
    else if (str_load_level.Compare(_T("disabled"))==0)
    {
        level = CDirectVobSub::LOADLEVEL_DISABLED;
    }

    bool load_external = !!theApp.GetProfileInt(   ResStr(IDS_R_GENERAL), ResStr(IDS_RG_EXTERNALLOAD ), 1);
    bool load_web      = !!theApp.GetProfileInt(   ResStr(IDS_R_GENERAL), ResStr(IDS_RG_WEBLOAD      ), 0);
    bool load_embedded = !!theApp.GetProfileInt(   ResStr(IDS_R_GENERAL), ResStr(IDS_RG_EMBEDDEDLOAD ), 1);
    CString load_exts  =   theApp.GetProfileString(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_LOAD_EXT_LIST),
        _T("ass;ssa;srt;sub;idx;sup;txt;usf;xss;ssf;smi;psb;rt"));

    if(level < 0 || level >= DirectVobSubImpl::LOADLEVEL_COUNT || level==DirectVobSubImpl::LOADLEVEL_DISABLED) {
        XY_LOG_DEBUG("Disabled by load setting: "<<XY_LOG_VAR_2_STR(level));
        return false;
    }

    if(level == DirectVobSubImpl::LOADLEVEL_ALWAYS)
    {
        return true;
    }

    // find text stream on known splitters

    bool have_subtitle_pin = false;
    CComPtr<IBaseFilter> pBF;
    if( load_embedded && (
        (pBF = FindFilter(CLSID_OggSplitter, pGraph)) || (pBF = FindFilter(CLSID_AviSplitter, pGraph))
        || (pBF = FindFilter(L"{34293064-02F2-41D5-9D75-CC5967ACA1AB}", pGraph)) // matroska demux
        || (pBF = FindFilter(L"{0A68C3B5-9164-4a54-AFAF-995B2FF0E0D4}", pGraph)) // matroska source
        || (pBF = FindFilter(L"{149D2E01-C32E-4939-80F6-C07B81015A7A}", pGraph)) // matroska splitter
        || (pBF = FindFilter(L"{55DA30FC-F16B-49fc-BAA5-AE59FC65F82D}", pGraph)) // Haali Media Splitter
        || (pBF = FindFilter(L"{564FD788-86C9-4444-971E-CC4A243DA150}", pGraph)) // Haali Media Splitter (AR)
        || (pBF = FindFilter(L"{8F43B7D9-9D6B-4F48-BE18-4D787C795EEA}", pGraph)) // Haali Simple Media Splitter
        || (pBF = FindFilter(L"{52B63861-DC93-11CE-A099-00AA00479A58}", pGraph)) // 3ivx splitter
        || (pBF = FindFilter(L"{6D3688CE-3E9D-42F4-92CA-8A11119D25CD}", pGraph)) // our ogg source
        || (pBF = FindFilter(L"{9FF48807-E133-40AA-826F-9B2959E5232D}", pGraph)) // our ogg splitter
        || (pBF = FindFilter(L"{803E8280-F3CE-4201-982C-8CD8FB512004}", pGraph)) // dsm source
        || (pBF = FindFilter(L"{0912B4DD-A30A-4568-B590-7179EBB420EC}", pGraph)) // dsm splitter
        || (pBF = FindFilter(L"{3CCC052E-BDEE-408a-BEA7-90914EF2964B}", pGraph)) // mp4 source
        || (pBF = FindFilter(L"{61F47056-E400-43d3-AF1E-AB7DFFD4C4AD}", pGraph)) // mp4 splitter
        || (pBF = FindFilter(L"{171252A0-8820-4AFE-9DF8-5C92B2D66B04}", pGraph)) // LAV splitter
        || (pBF = FindFilter(L"{B98D13E7-55DB-4385-A33D-09FD1BA26338}", pGraph)) // LAV Splitter Source
        || (pBF = FindFilter(L"{E436EBB5-524F-11CE-9F53-0020AF0BA770}", pGraph)) // Solveig matroska splitter
        || (pBF = FindFilter(L"{1365BE7A-C86A-473C-9A41-C0A6E82C9FA3}", pGraph)) // MPC-HC MPEG PS/TS/PVA source
        || (pBF = FindFilter(L"{DC257063-045F-4BE2-BD5B-E12279C464F0}", pGraph)) // MPC-HC MPEG splitter
        || (pBF = FindFilter(L"{529A00DB-0C43-4f5b-8EF2-05004CBE0C6F}", pGraph)) // AV splitter
        || (pBF = FindFilter(L"{D8980E15-E1F6-4916-A10F-D7EB4E9E10B8}", pGraph)) // AV source
        ) )
    {
        BeginEnumPins(pBF, pEP, pPin)
        {
            BeginEnumMediaTypes(pPin, pEM, pmt)
            {
                if (pmt->majortype == MEDIATYPE_Text || pmt->majortype == MEDIATYPE_Subtitle)
                {
                    have_subtitle_pin = true;
                    break;
                }
            }
            EndEnumMediaTypes(pmt)
                if (have_subtitle_pin) break;
        }
        EndEnumPins
    }

    if (load_embedded && have_subtitle_pin)
    {
        return true;
    }

    if (load_external || load_web)
    {
        // find file name
        CStringW fn;

        BeginEnumFilters(pGraph, pEF, pBF)
        {
            if(CComQIPtr<IFileSourceFilter> pFSF = pBF)
            {
                LPOLESTR fnw = NULL;
                if(!pFSF || FAILED(pFSF->GetCurFile(&fnw, NULL)) || !fnw)
                    continue;
                fn = CStringW(fnw);
                CoTaskMemFree(fnw);
                break;
            }
        }
        EndEnumFilters
            XY_LOG_INFO(L"fn:"<<fn.GetString());

        if((load_external || load_web) && (load_web || !(wcsstr(fn, L"http://") || wcsstr(fn, L"mms://"))))
        {
            CAtlArray<CString> paths;

            for(int i = 0; i < 10; i++)
            {
                CString tmp;
                tmp.Format(IDS_RP_PATH, i);
                CString path = theApp.GetProfileString(ResStr(IDS_R_DEFTEXTPATHES), tmp);
                if(!path.IsEmpty()) paths.Add(path);
            }

            CAtlArray<SubFile> file_list;
            GetSubFileNames(fn, paths, load_exts, file_list);

            return !file_list.IsEmpty();
        }

    }

    return false;
}

//
// DummyInputPin
//

XyAutoLoaderDummyInputPin::XyAutoLoaderDummyInputPin( CBaseFilter *pFilter, CCritSec *pLock, HRESULT *phr, LPCWSTR pName )
    : CBaseInputPin(NAME("DummyInputPin"), pFilter, pLock, phr, pName)
{

}

HRESULT XyAutoLoaderDummyInputPin::CheckMediaType( const CMediaType * )
{
    return E_NOTIMPL;
}

//
//  XySubFilterAutoLoader
//

XySubFilterAutoLoader::XySubFilterAutoLoader( LPUNKNOWN punk, HRESULT* phr
    , const GUID& clsid /*= __uuidof(XySubFilterAutoLoader)*/ )
    : CBaseFilter(NAME("XySubFilterAutoLoader"), punk, &m_pLock, clsid)
{
    m_pin = DEBUG_NEW XyAutoLoaderDummyInputPin(this, &m_pLock, phr, NULL);;
}

XySubFilterAutoLoader::~XySubFilterAutoLoader()
{
    XY_LOG_DEBUG(_T(""));
    delete m_pin;
}

CBasePin* XySubFilterAutoLoader::GetPin( int n )
{
    if (n==0)
    {
        return m_pin;
    }
    return NULL;
}

int XySubFilterAutoLoader::GetPinCount()
{
    return 1;
}

STDMETHODIMP XySubFilterAutoLoader::JoinFilterGraph( IFilterGraph* pGraph, LPCWSTR pName )
{
    XY_LOG_INFO(XY_LOG_VAR_2_STR(this)<<"pGraph:"<<(void*)pGraph);
    HRESULT hr = NOERROR;
    if(pGraph)
    {
        bool found_consumer = false;
        bool found_vsfilter = false;
        BeginEnumFilters(pGraph, pEF, pBF)
        {
            if(pBF != (IBaseFilter*)this)
            {
                CLSID clsid;
                pBF->GetClassID(&clsid);
                if (clsid==__uuidof(XySubFilterAutoLoader))
                {
                    XY_LOG_INFO("Another XySubFilterAutoLoader filter has been added to the graph.");
                    return E_FAIL;
                }
                if (clsid==__uuidof(XySubFilter))
                {
                    return E_FAIL;
                }
                if (clsid==__uuidof(CDirectVobSubFilter) || clsid==__uuidof(CDirectVobSubFilter2))
                {
                    XY_LOG_INFO("I see VSFilter");
                    found_vsfilter = true;
                }
                if (CComQIPtr<ISubRenderConsumer>(pBF))
                {
                    XY_LOG_INFO("I see a consumer");
                    found_consumer = true;
                }
            }
        }
        EndEnumFilters;
        XY_DUMP_GRAPH(pGraph, 0);
        if (found_vsfilter && !found_consumer)
        {
            XY_LOG_DEBUG("VSFilter is here but NO consumer found. We'll leave.");
            return E_FAIL;
        }
        bool cannot_failed = false;
        if (ShouldWeAutoLoad(pGraph))
        {
            CComPtr<IBaseFilter> filter;
            hr = filter.CoCreateInstance(__uuidof(XySubFilter));
            if (FAILED(hr))
            {
                XY_LOG_ERROR("Failed to create XySubFilter."<<XY_LOG_VAR_2_STR(hr));
                return E_FAIL;
            }
            CComQIPtr<IXyOptions> xy_sub_filter(filter);
            hr = xy_sub_filter->XySetBool(DirectVobSubXyOptions::BOOL_BE_AUTO_LOADED     , true);
            if (FAILED(hr))
            {
                XY_LOG_ERROR("Failed to set option BOOL_BE_AUTO_LOADED");
                return E_FAIL;
            }
            hr = xy_sub_filter->XySetBool(DirectVobSubXyOptions::BOOL_GET_RID_OF_VSFILTER, true);
            if (FAILED(hr))
            {
                XY_LOG_ERROR("Failed to set option BOOL_GET_RID_OF_VSFILTER");
                return E_FAIL;
            }
            hr = pGraph->AddFilter(filter, L"XySubFilter(AutoLoad)");
            cannot_failed = true;//return E_FAIL after pGraph->AddFilter causes crashes
            if (FAILED(hr))
            {
                XY_LOG_ERROR("Failed to AddFilter. "<<XY_LOG_VAR_2_STR(xy_sub_filter)<<XY_LOG_VAR_2_STR(hr));
            }
        }
        else
        {
            XY_LOG_DEBUG("Should NOT auto load. We'll leave.");
        }
        hr = __super::JoinFilterGraph(pGraph, pName);
        return cannot_failed ? S_OK : hr;
    }
    else
    {
        hr = __super::JoinFilterGraph(pGraph, pName);
    }

    return hr;
}

STDMETHODIMP XySubFilterAutoLoader::QueryFilterInfo( FILTER_INFO* pInfo )
{
    CheckPointer(pInfo, E_POINTER);
    ValidateReadWritePtr(pInfo, sizeof(FILTER_INFO));

    HRESULT hr = __super::QueryFilterInfo(pInfo);
    if (SUCCEEDED(hr))
    {
        wcscpy_s(pInfo->achName, countof(pInfo->achName)-1, L"XySubFilterAutoLoader");
    }
    return hr;
}

HRESULT XySubFilterAutoLoader::GetMerit( const GUID& clsid, DWORD *merit )
{
    CheckPointer(merit, E_POINTER);

    LPOLESTR    str;
    StringFromCLSID(clsid, &str);
    CString     str_clsid(str);
    CString     key_name;
    if (str) CoTaskMemFree(str);

    /*
    Load the FilterData buffer, then change the merit value and
    write it back.
    */

    key_name.Format(_T("CLSID\\{083863F1-70DE-11d0-BD40-00A0C911CE86}\\Instance\\%s"), str_clsid);
    CRegKey key;
    if (key.Open(HKEY_CLASSES_ROOT, key_name, KEY_READ) == ERROR_SUCCESS) { 
        ULONG size = 256*1024;
        BYTE  *largebuf = (BYTE*)malloc(size);
        LONG  lret;

        if (!largebuf) { XY_LOG_DEBUG(key_name<<" "<<lret); key.Close(); return E_FAIL; }

        lret = key.QueryBinaryValue(_T("FilterData"), largebuf, &size);
        if (lret != ERROR_SUCCESS) { XY_LOG_DEBUG(key_name<<" "<<lret); free(largebuf); key.Close(); return E_FAIL; }

        // change the merit
        DWORD *dwbuf = (DWORD*)largebuf;
        *merit = dwbuf[1];

        free(largebuf);

    } else {
        XY_LOG_DEBUG(key_name.GetString());
        return E_FAIL;
    }
    key.Close();
    return S_OK;
}

