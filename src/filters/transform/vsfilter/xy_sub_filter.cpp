#include "stdafx.h"
#include "xy_sub_filter.h"
#include "DirectVobSubFilter.h"
#include "VSFilter.h"
#include "DirectVobSubPropPage.h"
#include "SubtitleInputPin2.h"
#include "../../../subtitles/xy_bitmap.h"
#include "../../../subtitles/hdmv_subtitle_provider.h"
#include "../../../SubPic/SimpleSubPicProviderImpl.h"
#include "../../../subpic/SimpleSubPicWrapper.h"
#include "../../../subpic/color_conv_table.h"

#include "CAutoTiming.h"
#include "xy_logger.h"

#include "moreuuids.h"

#if ENABLE_XY_LOG_RENDERER_REQUEST
#  define TRACE_RENDERER_REQUEST(msg) XY_LOG_TRACE(msg)
#else
#  define TRACE_RENDERER_REQUEST(msg)
#endif

using namespace DirectVobSubXyOptions;

const SubRenderOptionsImpl::OptionMap options[] = 
{
    {"name",           SubRenderOptionsImpl::OPTION_TYPE_STRING, STRING_NAME                 },
    {"version",        SubRenderOptionsImpl::OPTION_TYPE_STRING, STRING_VERSION              },
    {"yuvMatrix",      SubRenderOptionsImpl::OPTION_TYPE_STRING, STRING_YUV_MATRIX           },
    {"combineBitmaps", SubRenderOptionsImpl::OPTION_TYPE_BOOL,   BOOL_COMBINE_BITMAPS        },
    {"useDestAlpha",   SubRenderOptionsImpl::OPTION_TYPE_BOOL,   BOOL_SUB_FRAME_USE_DST_ALPHA},
    {0}
};

////////////////////////////////////////////////////////////////////////////
//
// Constructor
//

XySubFilter::XySubFilter( LPUNKNOWN punk, 
    HRESULT* phr, const GUID& clsid /*= __uuidof(XySubFilter)*/ )
    : CBaseFilter(NAME("XySubFilter"), punk, &m_csFilter, clsid)
    , CDirectVobSub(XyVobFilterOptions)
    , SubRenderOptionsImpl(::options, this)
    , m_curSubStream(NULL)
    , m_not_first_pause(false)
    , m_hSystrayThread(0)
    , m_consumer_options_read(false)
    , m_context_id(0)
    , m_last_requested(-1)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    XY_LOG_INFO(XY_LOG_VAR_2_STR(this));
    m_xy_str_opt[STRING_NAME] = L"xy_sub_filter";

    theApp.WriteProfileString(ResStr(IDS_R_DEFTEXTPATHES), _T("Hint"), _T("The first three are fixed, but you can add more up to ten entries."));

    CString tmp;
    tmp.Format(ResStr(IDS_RP_PATH), 0);
    theApp.WriteProfileString(ResStr(IDS_R_DEFTEXTPATHES), tmp, _T("."));
    tmp.Format(ResStr(IDS_RP_PATH), 1);
    theApp.WriteProfileString(ResStr(IDS_R_DEFTEXTPATHES), tmp, _T("c:\\subtitles"));
    tmp.Format(ResStr(IDS_RP_PATH), 2);
    theApp.WriteProfileString(ResStr(IDS_R_DEFTEXTPATHES), tmp, _T(".\\subtitles"));

    m_fLoading = true;

    m_video_yuv_matrix_decided_by_sub = ColorConvTable::NONE;
    m_video_yuv_range_decided_by_sub = ColorConvTable::RANGE_NONE;

    m_pSubtitleInputPin.Add(DEBUG_NEW SubtitleInputPin2(this, m_pLock, &m_csSubLock, phr));
    ASSERT(SUCCEEDED(*phr));
    if(phr && FAILED(*phr)) return;

    m_frd.ThreadStartedEvent.Create(0, FALSE, FALSE, 0);
    m_frd.EndThreadEvent.Create(0, FALSE, FALSE, 0);
    m_frd.RefreshEvent.Create(0, FALSE, FALSE, 0);
    CAMThread::Create();

    WaitForSingleObject(m_frd.ThreadStartedEvent, INFINITE);

    m_tbid.WndCreatedEvent.Create(0, FALSE, FALSE, 0);
    m_tbid.hSystrayWnd = NULL;
    m_tbid.graph = NULL;
    m_tbid.fRunOnce = false;
    m_tbid.fShowIcon = (theApp.m_AppName.Find(_T("zplayer"), 0) < 0 || m_xy_bool_opt[BOOL_ENABLE_ZP_ICON]);
}

XySubFilter::~XySubFilter()
{
    XY_LOG_INFO(XY_LOG_VAR_2_STR(this));
    m_frd.EndThreadEvent.Set();
    CAMThread::Close();

    for(unsigned i = 0; i < m_pSubtitleInputPin.GetCount(); i++)
        delete m_pSubtitleInputPin[i];

    m_sub_provider = NULL;
    ::DeleteSystray(&m_hSystrayThread, &m_tbid);
    _ASSERTE(_CrtCheckMemory());
}

STDMETHODIMP XySubFilter::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
    CheckPointer(ppv, E_POINTER);

    return
        QI(IDirectVobSub)
        QI(IDirectVobSub2)
        QI(IXyOptions)
        QI(IFilterVersion)
        QI(ISpecifyPropertyPages)
        QI(IAMStreamSelect)
        QI(ISubRenderProvider)
        (riid==__uuidof(ISubRenderOptions)) ? GetInterface((ISubRenderProvider*)this, ppv) :
        __super::NonDelegatingQueryInterface(riid, ppv);
}

//
// CBaseFilter
//
CBasePin* XySubFilter::GetPin(int n)
{
    if(n >= 0 && n < (int)m_pSubtitleInputPin.GetCount())
        return m_pSubtitleInputPin[n];

    return NULL;
}

int XySubFilter::GetPinCount()
{
    return m_pSubtitleInputPin.GetCount();
}

STDMETHODIMP XySubFilter::JoinFilterGraph(IFilterGraph* pGraph, LPCWSTR pName)
{
    XY_LOG_INFO(XY_LOG_VAR_2_STR(this));
    XY_LOG_INFO("JoinFilterGraph. pGraph:"<<(void*)pGraph);
    if(pGraph)
    {
        BeginEnumFilters(pGraph, pEF, pBF)
        {
            if(pBF != (IBaseFilter*)this && CComQIPtr<IDirectVobSub>(pBF))
            {
                CLSID clsid;
                pBF->GetClassID(&clsid);
                if (clsid==__uuidof(XySubFilter))
                {
                    return E_FAIL;
                }
            }
        }
        EndEnumFilters;
    }
    else
    {
        if (m_consumer)
        {
            if (FAILED(m_consumer->Disconnect())) {
                XY_LOG_ERROR("Failed to disconnect consumer");
            }
            m_consumer = NULL;
        }
        ::DeleteSystray(&m_hSystrayThread, &m_tbid);
    }

    return __super::JoinFilterGraph(pGraph, pName);
}

STDMETHODIMP XySubFilter::QueryFilterInfo( FILTER_INFO* pInfo )
{
    XY_LOG_INFO(XY_LOG_VAR_2_STR(this));
    CheckPointer(pInfo, E_POINTER);
    ValidateReadWritePtr(pInfo, sizeof(FILTER_INFO));

    HRESULT hr = __super::QueryFilterInfo(pInfo);
    if (SUCCEEDED(hr))
    {
        LPWSTR consumer_name = NULL;
        if (m_consumer) {
            int name_len = 0;
            hr = m_consumer->GetString("name", &consumer_name, &name_len);
            if (FAILED(hr))
            {
                LocalFree(consumer_name);
                return hr;
            }
        }

        LPWSTR test = NULL;
        int chars = 0;
        HRESULT test_hr = this->GetString("yuvMatrix", &test, &chars);
        ASSERT(SUCCEEDED(test_hr));
        CStringW new_name;
        if (consumer_name) {
            new_name.Format(L"XySubFilter (Connected with %s, %s)", consumer_name, test);
        }
        else {
            new_name.Format(L"XySubFilter (%s)", test);
        }
        LocalFree(test);
        LocalFree(consumer_name);
        wcscpy_s(pInfo->achName, countof(pInfo->achName)-1, new_name.GetString());
    }
    return hr;
}

STDMETHODIMP XySubFilter::Pause()
{
    XY_LOG_INFO(XY_LOG_VAR_2_STR(this));
    CAutoLock lck(&m_csFilter);
    HRESULT hr = NOERROR;

    if (!m_not_first_pause)
    {
        if (m_pSubtitleInputPin.GetCount()<=1)
        {
            //no pins connected, hence it is likely we not yet try to scan for external subtitles
            //but if m_xy_str_opt[STRING_FILE_NAME] has been specified, we do NOT want to overwrite it
            if (m_xy_str_opt[STRING_FILE_NAME].IsEmpty())
            {
                LoadExternalSubtitle(m_pGraph);
            }
            if(!m_hSystrayThread && !m_xy_bool_opt[BOOL_HIDE_TRAY_ICON])
            {
                m_tbid.graph = m_pGraph;
                m_tbid.dvs = static_cast<IDirectVobSub*>(this);

                DWORD tid;
                m_hSystrayThread = CreateThread(0, 0, SystrayThreadProc, &m_tbid, 0, &tid);
                XY_LOG_INFO("Systray thread created "<<m_hSystrayThread);
            }
        }
        hr = FindAndConnectConsumer(m_pGraph);
        if (FAILED(hr))
        {
            XY_LOG_ERROR("Failed when find and connect consumer");
            return hr;
        }
        m_not_first_pause = true;
    }

    if (m_State == State_Paused) {
        ASSERT(m_not_first_pause);
        // (This space left deliberately blank)
    }
    // If we have no input pins

    else if (m_pSubtitleInputPin.GetCount()<=1) {
        m_State = State_Paused;
    }
    // if we have an input pin

    else {
        if (m_State == State_Stopped) {
            CAutoLock lck2(&m_csReceive);
            hr = StartStreaming();
        }
        if (SUCCEEDED(hr)) {
            hr = CBaseFilter::Pause();
        }
    }

    return hr;
}

//
// XyOptionsImpl
//

HRESULT XySubFilter::OnOptionChanged( unsigned field )
{
    HRESULT hr = CDirectVobSub::OnOptionChanged(field);
    if (FAILED(hr))
    {
        return hr;
    }
    switch(field)
    {
    case INT_SUBPIXEL_POS_LEVEL:
    case SIZE_ASS_PLAY_RESOLUTION:
    case SIZE_USER_SPECIFIED_LAYOUT_SIZE:
    case DOUBLE_FPS:
    case INT_SELECTED_LANGUAGE:
    case BOOL_SUB_FRAME_USE_DST_ALPHA:
        m_context_id++;
        break;
    case STRING_FILE_NAME:
        if (!Open())
        {
            m_xy_str_opt[STRING_FILE_NAME].Empty();
            hr = E_FAIL;
            break;
        }
        m_context_id++;
        break;
    case BOOL_OVERRIDE_PLACEMENT:
    case SIZE_PLACEMENT_PERC:
    case INT_ASPECT_RATIO_SETTINGS:
        m_context_id++;
        UpdateSubtitle(false);
        break;
    case BOOL_HIDE_SUBTITLES:
        UpdateSubtitle(false);
        break;
    case BOOL_VOBSUBSETTINGS_BUFFER:
    case BOOL_VOBSUBSETTINGS_ONLY_SHOW_FORCED_SUBS:
    case BOOL_VOBSUBSETTINGS_POLYGONIZE:
    case BIN2_TEXT_SETTINGS:
    case BIN2_SUBTITLE_TIMING:
        m_context_id++;
        if (m_last_requested!=-1)
        {
            XY_LOG_WARN("Some subtitle frames are cached already!");
            m_last_requested = -1;
        }
        InvalidateSubtitle();
        break;
    }

    return hr;
}

HRESULT XySubFilter::OnOptionReading( unsigned field )
{
    HRESULT hr = CDirectVobSub::OnOptionReading(field);
    if (FAILED(hr))
    {
        return hr;
    }
    switch(field)
    {
    case INT_LANGUAGE_COUNT:
        UpdateLanguageCount();
        break;
    }
    return hr;
}

//
// IXyOptions
//

STDMETHODIMP XySubFilter::XyGetInt( unsigned field, int *value )
{
    CAutoLock cAutoLock(&m_propsLock);
    HRESULT hr = CDirectVobSub::XyGetInt(field, value);
    if(hr != NOERROR)
    {
        return hr;
    }
    switch(field)
    {
    case DirectVobSubXyOptions::INT_MAX_BITMAP_COUNT2:
        if (m_xy_bool_opt[BOOL_COMBINE_BITMAPS])
        {
            *value = 1;
        }
        else
        {
            *value = m_xy_int_opt[DirectVobSubXyOptions::INT_MAX_BITMAP_COUNT];
        }
        break;
    }
    return hr;
}

STDMETHODIMP XySubFilter::XyGetString( unsigned field, LPWSTR *value, int *chars )
{
    switch(field)
    {
    case STRING_CONNECTED_CONSUMER:
        if (m_consumer)
        {
            return m_consumer->GetString("name", value, chars);
        }
        break;
    case STRING_CONSUMER_VERSION:
        if (m_consumer)
        {
            return m_consumer->GetString("version", value, chars);
        }
        break;

    case STRING_NAME:
    case STRING_VERSION:
        //they're read only, no need to lock
        return XyOptionsImpl::XyGetString(field, value, chars);
        break;
    case STRING_YUV_MATRIX:
    case BOOL_COMBINE_BITMAPS:
    case BOOL_SUB_FRAME_USE_DST_ALPHA:
        {
            CAutoLock cAutolock(&m_csProviderFields);//do NOT hold m_csSubLock so that it is faster
            return XyOptionsImpl::XyGetString(field, value, chars);
        }
        break;
    }
    CAutoLock cAutolock(&m_csSubLock);
    return CDirectVobSub::XyGetString(field, value,chars);
}

STDMETHODIMP XySubFilter::XySetInt( unsigned field, int value )
{
    CAutoLock cAutolock(&m_csSubLock);
    HRESULT hr = CDirectVobSub::XySetInt(field, value);
    if(hr != NOERROR)
    {
        return hr;
    }
    switch(field)
    {
    case DirectVobSubXyOptions::INT_COLOR_SPACE:
    case DirectVobSubXyOptions::INT_YUV_RANGE:
        SetYuvMatrix();
        break;
    case DirectVobSubXyOptions::INT_OVERLAY_CACHE_MAX_ITEM_NUM:
        CacheManager::GetOverlayMruCache()->SetMaxItemNum(m_xy_int_opt[field]);
        break;
    case DirectVobSubXyOptions::INT_SCAN_LINE_DATA_CACHE_MAX_ITEM_NUM:
        CacheManager::GetScanLineData2MruCache()->SetMaxItemNum(m_xy_int_opt[field]);
        break;
    case DirectVobSubXyOptions::INT_PATH_DATA_CACHE_MAX_ITEM_NUM:
        CacheManager::GetPathDataMruCache()->SetMaxItemNum(m_xy_int_opt[field]);
        break;
    case DirectVobSubXyOptions::INT_OVERLAY_NO_BLUR_CACHE_MAX_ITEM_NUM:
        CacheManager::GetOverlayNoBlurMruCache()->SetMaxItemNum(m_xy_int_opt[field]);
        break;
    case DirectVobSubXyOptions::INT_BITMAP_MRU_CACHE_ITEM_NUM:
        CacheManager::GetBitmapMruCache()->SetMaxItemNum(m_xy_int_opt[field]);
        break;
    case DirectVobSubXyOptions::INT_CLIPPER_MRU_CACHE_ITEM_NUM:
        CacheManager::GetClipperAlphaMaskMruCache()->SetMaxItemNum(m_xy_int_opt[field]);
        break;
    case DirectVobSubXyOptions::INT_TEXT_INFO_CACHE_ITEM_NUM:
        CacheManager::GetTextInfoCache()->SetMaxItemNum(m_xy_int_opt[field]);
        break;
    case DirectVobSubXyOptions::INT_ASS_TAG_LIST_CACHE_ITEM_NUM:
        CacheManager::GetAssTagListMruCache()->SetMaxItemNum(m_xy_int_opt[field]);
        break;
    case DirectVobSubXyOptions::INT_SUBPIXEL_VARIANCE_CACHE_ITEM_NUM:
        CacheManager::GetSubpixelVarianceCache()->SetMaxItemNum(m_xy_int_opt[field]);
        break;
    case DirectVobSubXyOptions::INT_SUBPIXEL_POS_LEVEL:
        SubpixelPositionControler::GetGlobalControler().SetSubpixelLevel( static_cast<SubpixelPositionControler::SUBPIXEL_LEVEL>(m_xy_int_opt[field]) );
        break;
    case DirectVobSubXyOptions::INT_LAYOUT_SIZE_OPT:
        //fix me: is it really supported?
        break;
    case DirectVobSubXyOptions::INT_MAX_BITMAP_COUNT2:
        hr = E_INVALIDARG;
        break;
    case DirectVobSubXyOptions::INT_SELECTED_LANGUAGE:
        UpdateSubtitle(false);
        break;
    default:
        break;
    }

    return hr;
}

STDMETHODIMP XySubFilter::XySetBool(unsigned field, bool      value)
{
    switch(field)
    {
    case BOOL_COMBINE_BITMAPS:
    case BOOL_SUB_FRAME_USE_DST_ALPHA:
        {
            CAutoLock cAutolock1(&m_csSubLock);
            CAutoLock cAutolock2(&m_csProviderFields);
            return XyOptionsImpl::XySetBool(field, value);
        }
        break;
    }
    return CDirectVobSub::XySetBool(field, value);
}

//
// IDirectVobSub
//
STDMETHODIMP XySubFilter::get_LanguageName(int iLanguage, WCHAR** ppName)
{
    HRESULT hr = CDirectVobSub::get_LanguageName(iLanguage, ppName);

    if(!ppName) return E_POINTER;

    if(hr == NOERROR)
    {
        CAutoLock cAutolock(&m_csSubLock);

        hr = E_INVALIDARG;

        int i = iLanguage;

        POSITION pos = m_pSubStreams.GetHeadPosition();
        while(i >= 0 && pos)
        {
            CComPtr<ISubStream> pSubStream = m_pSubStreams.GetNext(pos);

            if(i < pSubStream->GetStreamCount())
            {
                pSubStream->GetStreamInfo(i, ppName, NULL);
                hr = NOERROR;
                break;
            }

            i -= pSubStream->GetStreamCount();
        }
    }

    return hr;
}

STDMETHODIMP XySubFilter::get_CachesInfo(CachesInfo* caches_info)
{
    XY_LOG_INFO(caches_info);
    CAutoLock cAutolock(&m_csSubLock);
    HRESULT hr = CDirectVobSub::get_CachesInfo(caches_info);

    caches_info->path_cache_cur_item_num    = CacheManager::GetPathDataMruCache()->GetCurItemNum();
    caches_info->path_cache_hit_count       = CacheManager::GetPathDataMruCache()->GetCacheHitCount();
    caches_info->path_cache_query_count     = CacheManager::GetPathDataMruCache()->GetQueryCount();
    caches_info->scanline_cache2_cur_item_num= CacheManager::GetScanLineData2MruCache()->GetCurItemNum();
    caches_info->scanline_cache2_hit_count   = CacheManager::GetScanLineData2MruCache()->GetCacheHitCount();
    caches_info->scanline_cache2_query_count = CacheManager::GetScanLineData2MruCache()->GetQueryCount();
    caches_info->non_blur_cache_cur_item_num= CacheManager::GetOverlayNoBlurMruCache()->GetCurItemNum();
    caches_info->non_blur_cache_hit_count   = CacheManager::GetOverlayNoBlurMruCache()->GetCacheHitCount();
    caches_info->non_blur_cache_query_count = CacheManager::GetOverlayNoBlurMruCache()->GetQueryCount();
    caches_info->overlay_cache_cur_item_num = CacheManager::GetOverlayMruCache()->GetCurItemNum();
    caches_info->overlay_cache_hit_count    = CacheManager::GetOverlayMruCache()->GetCacheHitCount();
    caches_info->overlay_cache_query_count  = CacheManager::GetOverlayMruCache()->GetQueryCount();

    caches_info->bitmap_cache_cur_item_num  = CacheManager::GetBitmapMruCache()->GetCurItemNum();
    caches_info->bitmap_cache_hit_count     = CacheManager::GetBitmapMruCache()->GetCacheHitCount();
    caches_info->bitmap_cache_query_count   = CacheManager::GetBitmapMruCache()->GetQueryCount();

    caches_info->interpolate_cache_cur_item_num = CacheManager::GetSubpixelVarianceCache()->GetCurItemNum();
    caches_info->interpolate_cache_hit_count    = CacheManager::GetSubpixelVarianceCache()->GetCacheHitCount();
    caches_info->interpolate_cache_query_count  = CacheManager::GetSubpixelVarianceCache()->GetQueryCount();    
    caches_info->text_info_cache_cur_item_num   = CacheManager::GetTextInfoCache()->GetCurItemNum();
    caches_info->text_info_cache_hit_count      = CacheManager::GetTextInfoCache()->GetCacheHitCount();
    caches_info->text_info_cache_query_count    = CacheManager::GetTextInfoCache()->GetQueryCount();

    caches_info->word_info_cache_cur_item_num   = CacheManager::GetAssTagListMruCache()->GetCurItemNum();
    caches_info->word_info_cache_hit_count      = CacheManager::GetAssTagListMruCache()->GetCacheHitCount();
    caches_info->word_info_cache_query_count    = CacheManager::GetAssTagListMruCache()->GetQueryCount();    

    caches_info->scanline_cache_cur_item_num = CacheManager::GetScanLineDataMruCache()->GetCurItemNum();
    caches_info->scanline_cache_hit_count    = CacheManager::GetScanLineDataMruCache()->GetCacheHitCount();
    caches_info->scanline_cache_query_count  = CacheManager::GetScanLineDataMruCache()->GetQueryCount();

    caches_info->overlay_key_cache_cur_item_num = CacheManager::GetOverlayNoOffsetMruCache()->GetCurItemNum();
    caches_info->overlay_key_cache_hit_count = CacheManager::GetOverlayNoOffsetMruCache()->GetCacheHitCount();
    caches_info->overlay_key_cache_query_count = CacheManager::GetOverlayNoOffsetMruCache()->GetQueryCount();

    caches_info->clipper_cache_cur_item_num = CacheManager::GetClipperAlphaMaskMruCache()->GetCurItemNum();
    caches_info->clipper_cache_hit_count    = CacheManager::GetClipperAlphaMaskMruCache()->GetCacheHitCount();
    caches_info->clipper_cache_query_count  = CacheManager::GetClipperAlphaMaskMruCache()->GetQueryCount();

    return hr;
}

STDMETHODIMP XySubFilter::get_XyFlyWeightInfo( XyFlyWeightInfo* xy_fw_info )
{
    XY_LOG_INFO(xy_fw_info);
    CAutoLock cAutolock(&m_csSubLock);
    HRESULT hr = CDirectVobSub::get_XyFlyWeightInfo(xy_fw_info);

    xy_fw_info->xy_fw_string_w.cur_item_num = XyFwStringW::GetCacher()->GetCurItemNum();
    xy_fw_info->xy_fw_string_w.hit_count = XyFwStringW::GetCacher()->GetCacheHitCount();
    xy_fw_info->xy_fw_string_w.query_count = XyFwStringW::GetCacher()->GetQueryCount();

    xy_fw_info->xy_fw_grouped_draw_items_hash_key.cur_item_num = XyFwGroupedDrawItemsHashKey::GetCacher()->GetCurItemNum();
    xy_fw_info->xy_fw_grouped_draw_items_hash_key.hit_count = XyFwGroupedDrawItemsHashKey::GetCacher()->GetCacheHitCount();
    xy_fw_info->xy_fw_grouped_draw_items_hash_key.query_count = XyFwGroupedDrawItemsHashKey::GetCacher()->GetQueryCount();

    return hr;
}

STDMETHODIMP XySubFilter::get_MediaFPS(bool* fEnabled, double* fps)
{
    XY_LOG_INFO(XY_LOG_VAR_2_STR(fEnabled)<<XY_LOG_VAR_2_STR(fps));
    HRESULT hr = CDirectVobSub::get_MediaFPS(fEnabled, fps);

    CComQIPtr<IMediaSeeking> pMS = m_pGraph;
    double rate;
    if(pMS && SUCCEEDED(pMS->GetRate(&rate)))
    {
        m_xy_double_opt[DOUBLE_MEDIA_FPS] = rate * m_xy_double_opt[DOUBLE_FPS];
        if(fps) *fps = m_xy_double_opt[DOUBLE_MEDIA_FPS];
    }

    return hr;
}

STDMETHODIMP XySubFilter::put_MediaFPS(bool fEnabled, double fps)
{
    XY_LOG_INFO(XY_LOG_VAR_2_STR(fEnabled)<<XY_LOG_VAR_2_STR(fps));
    HRESULT hr = CDirectVobSub::put_MediaFPS(fEnabled, fps);

    CComQIPtr<IMediaSeeking> pMS = m_pGraph;
    if (pMS)
    {
        if (hr == NOERROR)
        {
            hr = pMS->SetRate(m_xy_bool_opt[BOOL_MEDIA_FPS_ENABLED] ? m_xy_double_opt[DOUBLE_MEDIA_FPS] / m_xy_double_opt[DOUBLE_FPS] : 1.0);
        }

        double dRate;
        if (SUCCEEDED(pMS->GetRate(&dRate)))
            m_xy_double_opt[DOUBLE_MEDIA_FPS] = dRate * m_xy_double_opt[DOUBLE_FPS];
    }

    return hr;
}

//
// IDirectVobSub2
//
STDMETHODIMP XySubFilter::put_TextSettings(STSStyle* pDefStyle)
{
    XY_LOG_INFO(pDefStyle);
    HRESULT hr = CDirectVobSub::put_TextSettings(pDefStyle);

    if(hr == NOERROR)
    {
        UpdateSubtitle(false);
    }

    return hr;
}

STDMETHODIMP XySubFilter::put_AspectRatioSettings(CSimpleTextSubtitle::EPARCompensationType* ePARCompensationType)
{
    XY_LOG_INFO(ePARCompensationType);
    HRESULT hr = CDirectVobSub::put_AspectRatioSettings(ePARCompensationType);

    if(hr == NOERROR)
    {
        //DbgLog((LOG_TRACE, 3, "%d %s:UpdateSubtitle(true)", __LINE__, __FUNCTION__));
        //UpdateSubtitle(true);
        UpdateSubtitle(false);
    }

    return hr;
}

//
// ISpecifyPropertyPages
// 
STDMETHODIMP XySubFilter::GetPages(CAUUID* pPages)
{
    XY_LOG_INFO(pPages);
    CheckPointer(pPages, E_POINTER);

    pPages->cElems = 5;
    pPages->pElems = (GUID*)CoTaskMemAlloc(sizeof(GUID)*pPages->cElems);

    if(pPages->pElems == NULL) return E_OUTOFMEMORY;

    int i = 0;
    pPages->pElems[i++] = __uuidof(CXySubFilterMainPPage);
    pPages->pElems[i++] = __uuidof(CXySubFilterMorePPage);
    pPages->pElems[i++] = __uuidof(CXySubFilterTimingPPage);
    pPages->pElems[i++] = __uuidof(CXySubFilterPathsPPage);
    pPages->pElems[i++] = __uuidof(CXySubFilterAboutPPage);

    return NOERROR;
}

//
// IAMStreamSelect
//
STDMETHODIMP XySubFilter::Count(DWORD* pcStreams)
{
    XY_LOG_INFO(pcStreams);
    if(!pcStreams) return E_POINTER;

    *pcStreams = 0;

    int nLangs = 0;
    if(SUCCEEDED(get_LanguageCount(&nLangs)))
        (*pcStreams) += nLangs;

    (*pcStreams) += 2; // enable ... disable

    //fix me: support subtitle flipping
    //(*pcStreams) += 2; // normal flipped

    return S_OK;
}

STDMETHODIMP XySubFilter::Enable(long lIndex, DWORD dwFlags)
{
    XY_LOG_INFO(XY_LOG_VAR_2_STR(lIndex)<<XY_LOG_VAR_2_STR(dwFlags));
    HRESULT hr = NOERROR;
    if(!(dwFlags & AMSTREAMSELECTENABLE_ENABLE))
        return E_NOTIMPL;

    int nLangs = 0;
    hr = get_LanguageCount(&nLangs);
    CHECK_N_LOG(hr, "Failed to get option");

    if(!(lIndex >= 0 && lIndex < nLangs+2 /* +2 fix me: support subtitle flipping */))
        return E_INVALIDARG;

    int i = lIndex-1;

    if(i == -1) // we need this because when loading something stupid media player pushes the first stream it founds, which is "enable" in our case
    {
        if (!m_fLoading)
        {
            hr = put_HideSubtitles(false);
        }
        else
        {
            //fix me: support even when loading
            XY_LOG_ERROR("Do NOT do this when we are loading");
            return E_FAIL;
        }
    }
    else if(i >= 0 && i < nLangs)
    {
        hr = put_HideSubtitles(false);
        CHECK_N_LOG(hr, "Failed to set option");
        hr = put_SelectedLanguage(i);
        CHECK_N_LOG(hr, "Failed to set option");

        WCHAR* pName = NULL;
        hr = get_LanguageName(i, &pName);
        if(SUCCEEDED(hr))
        {
            UpdatePreferedLanguages(CString(pName));
            if(pName) CoTaskMemFree(pName);
        }
    }
    else if(i == nLangs)
    {
        if (!m_fLoading)
        {
            hr = put_HideSubtitles(true);
        }
        else
        {
            XY_LOG_ERROR("Do NOT do this when we are loading");
            return E_FAIL;
        }
    }
    else if(i == nLangs+1 || i == nLangs+2)
    {
        if (!m_fLoading)
        {
            hr = put_Flip(i == nLangs+2, m_xy_bool_opt[BOOL_FLIP_SUBTITLE]);
        }
        else
        {
            XY_LOG_ERROR("Do NOT do this when we are loading");
            return E_FAIL;
        }
    }
    CHECK_N_LOG(hr, "Failed "<<XY_LOG_VAR_2_STR(lIndex)<<XY_LOG_VAR_2_STR(dwFlags));
    return hr;
}

STDMETHODIMP XySubFilter::Info(long lIndex, AM_MEDIA_TYPE** ppmt, DWORD* pdwFlags, LCID* plcid, DWORD* pdwGroup, WCHAR** ppszName, IUnknown** ppObject, IUnknown** ppUnk)
{
    XY_LOG_INFO(XY_LOG_VAR_2_STR(lIndex)<<XY_LOG_VAR_2_STR(ppmt)
        <<XY_LOG_VAR_2_STR(pdwFlags)<<XY_LOG_VAR_2_STR(plcid)<<XY_LOG_VAR_2_STR(pdwGroup)<<XY_LOG_VAR_2_STR(ppszName)
        <<XY_LOG_VAR_2_STR(ppObject)<<XY_LOG_VAR_2_STR(ppUnk));
    HRESULT hr = NOERROR;
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    const int FLAG_CMD = 1;
    const int FLAG_EXTERNAL_SUB = 2;
    const int FLAG_PICTURE_CMD = 4;
    const int FLAG_VISIBILITY_CMD = 8;

    const int GROUP_NUM_BASE = 0x648E40;//random number

    int nLangs = 0;
    hr = get_LanguageCount(&nLangs);
    CHECK_N_LOG(hr, "Failed to get option");

    if(!(lIndex >= 0 && lIndex < nLangs+2 /* +2 fix me: support subtitle flipping */))
        return E_INVALIDARG;

    int i = lIndex-1;

    if(ppmt) 
    {
        //fix me: not supported yet
        *ppmt = NULL;
        //if(i >= 0 && i < nLangs)
        //{
        //    *ppmt = CreateMediaType(&m_pSubtitleInputPin[i]->m_mt);
        //}
    }

    if(pdwFlags)
    {
        *pdwFlags = 0;

        if(i == -1 && !m_xy_bool_opt[BOOL_HIDE_SUBTITLES]
            || i >= 0 && i < nLangs && i == m_xy_int_opt[INT_SELECTED_LANGUAGE]
            || i == nLangs && m_xy_bool_opt[BOOL_HIDE_SUBTITLES]
            || i == nLangs+1 && !m_xy_bool_opt[BOOL_FLIP_PICTURE]
            || i == nLangs+2 && m_xy_bool_opt[BOOL_FLIP_PICTURE])
        {
            *pdwFlags |= AMSTREAMSELECTINFO_ENABLED;
        }
    }

    if(plcid) *plcid = 0;

    if(pdwGroup)
    {
        *pdwGroup = GROUP_NUM_BASE;
        if(i == -1)
        {
            *pdwGroup = GROUP_NUM_BASE | FLAG_CMD | FLAG_VISIBILITY_CMD;
        }
        else if(i >= 0 && i < nLangs)
        {
            bool isEmbedded = false;
            hr = GetIsEmbeddedSubStream(i, &isEmbedded);
            ASSERT(SUCCEEDED(hr));
            if(isEmbedded)
            {
                *pdwGroup = GROUP_NUM_BASE & ~(FLAG_CMD | FLAG_EXTERNAL_SUB);
            }
            else
            {
                *pdwGroup = (GROUP_NUM_BASE & ~FLAG_CMD) | FLAG_EXTERNAL_SUB;
            }
        }
        else if(i == nLangs)
        {
            *pdwGroup = GROUP_NUM_BASE | FLAG_CMD | FLAG_VISIBILITY_CMD;
        }
        else if(i == nLangs+1)
        {
            *pdwGroup = GROUP_NUM_BASE | FLAG_CMD | FLAG_PICTURE_CMD;
        }
        else if(i == nLangs+2)
        {
            *pdwGroup = GROUP_NUM_BASE | FLAG_CMD | FLAG_PICTURE_CMD;
        }
    }

    if(ppszName)
    {
        *ppszName = NULL;

        CStringW str;
        if(i == -1) str = ResStr(IDS_M_SHOWSUBTITLES);
        else if(i >= 0 && i < nLangs)
        {
            hr = get_LanguageName(i, ppszName);
            CHECK_N_LOG(hr, "Failed to get option");
        }
        else if(i == nLangs)
        {
            str = ResStr(IDS_M_HIDESUBTITLES);
        }
        else if(i == nLangs+1)
        {
            str = ResStr(IDS_M_ORIGINALPICTURE);
        }
        else if(i == nLangs+2)
        {
            str = ResStr(IDS_M_FLIPPEDPICTURE);
        }

        if(!str.IsEmpty())
        {
            *ppszName = (WCHAR*)CoTaskMemAlloc((str.GetLength()+1)*sizeof(WCHAR));
            if(*ppszName == NULL) return S_FALSE;
            wcscpy_s(*ppszName, str.GetLength()+1, str);
        }
    }

    if(ppObject) *ppObject = NULL;

    if(ppUnk) *ppUnk = NULL;

    return hr;
}

//
// FRD
// 
void XySubFilter::SetupFRD(CStringArray& paths, CAtlArray<HANDLE>& handles)
{
    XY_LOG_INFO(XY_LOG_VAR_2_STR(paths.GetCount())<<XY_LOG_VAR_2_STR(handles.GetCount()));
    CAutoLock cAutolock(&m_csSubLock);

    for(unsigned i = 2; i < handles.GetCount(); i++)
    {
        FindCloseChangeNotification(handles[i]);
    }

    paths.RemoveAll();
    handles.RemoveAll();

    handles.Add(m_frd.EndThreadEvent);
    handles.Add(m_frd.RefreshEvent);

    m_frd.mtime.SetCount(m_frd.files.GetCount());

    POSITION pos = m_frd.files.GetHeadPosition();
    for(unsigned i = 0; pos; i++)
    {
        CString fn = m_frd.files.GetNext(pos);

        CFileStatus status;
        if(CFileGetStatus(fn, status))
            m_frd.mtime[i] = status.m_mtime;

        fn.Replace('\\', '/');
        fn = fn.Left(fn.ReverseFind('/')+1);

        bool fFound = false;

        for(int j = 0; !fFound && j < paths.GetCount(); j++)
        {
            if(paths[j] == fn) fFound = true;
        }

        if(!fFound)
        {
            paths.Add(fn);

            HANDLE h = FindFirstChangeNotification(fn, FALSE, FILE_NOTIFY_CHANGE_LAST_WRITE);
            if(h != INVALID_HANDLE_VALUE) handles.Add(h);
        }
    }
}

DWORD XySubFilter::ThreadProc()
{
    XY_LOG_INFO(XY_LOG_VAR_2_STR(this));
    SetThreadPriority(m_hThread, THREAD_PRIORITY_LOWEST/*THREAD_PRIORITY_BELOW_NORMAL*/);

    CStringArray paths;
    CAtlArray<HANDLE> handles;

    SetupFRD(paths, handles);
    m_frd.ThreadStartedEvent.Set();
    while(1)
    {
        DWORD idx = WaitForMultipleObjects(handles.GetCount(), handles.GetData(), FALSE, INFINITE);

        if(idx == (WAIT_OBJECT_0 + 0)) // m_frd.hEndThreadEvent
        {
            break;
        }
        if(idx == (WAIT_OBJECT_0 + 1)) // m_frd.hRefreshEvent
        {
            SetupFRD(paths, handles);
        }
        else if(idx >= (WAIT_OBJECT_0 + 2) && idx < (WAIT_OBJECT_0 + handles.GetCount()))
        {
            bool fLocked = true;
            IsSubtitleReloaderLocked(&fLocked);
            if(fLocked) continue;

            if(FindNextChangeNotification(handles[idx - WAIT_OBJECT_0]) == FALSE)
                break;

            int j = 0;

            POSITION pos = m_frd.files.GetHeadPosition();
            for(int i = 0; pos && j == 0; i++)
            {
                CString fn = m_frd.files.GetNext(pos);

                CFileStatus status;
                if(CFileGetStatus(fn, status) && m_frd.mtime[i] != status.m_mtime)
                {
                    for(j = 0; j < 10; j++)
                    {
                        FILE* f = NULL;
                        if(!_tfopen_s(&f, fn, _T("rb+")))
                        {
                            fclose(f);
                            j = 0;
                            break;
                        }
                        else
                        {
                            Sleep(100);
                            j++;
                        }
                    }
                }
            }

            if(j > 0)
            {
                SetupFRD(paths, handles);
            }
            else
            {
                Sleep(500);

                POSITION pos = m_frd.files.GetHeadPosition();
                for(int i = 0; pos; i++)
                {
                    CFileStatus status;
                    if(CFileGetStatus(m_frd.files.GetNext(pos), status)
                        && m_frd.mtime[i] != status.m_mtime)
                    {
                        Open();
                        SetupFRD(paths, handles);
                        break;
                    }
                }
            }
        }
        else
        {
            break;
        }
    }

    for(unsigned i = 2; i < handles.GetCount(); i++)
    {
        FindCloseChangeNotification(handles[i]);
    }

    return 0;
}

//
// ISubRenderProvider
//

STDMETHODIMP XySubFilter::RequestFrame( REFERENCE_TIME start, REFERENCE_TIME stop, LPVOID context )
{
    XY_LOG_INFO(XY_LOG_VAR_2_STR(this));
    for (int i=0,n=m_pSubtitleInputPin.GetCount();i<n;i++)
    {
        if (!m_pSubtitleInputPin[i]->WaitTillNotReceiving(5))
        {
            XY_LOG_ERROR("Still receiving subtitle samples for pin "<<XY_LOG_VAR_2_STR(i));
        }
    }
    CAutoLock cAutoLock(&m_csSubLock);
    TRACE_RENDERER_REQUEST("Lock acuqired"<<XY_LOG_VAR_2_STR(&m_csSubLock));

    ASSERT(m_consumer);
    HRESULT hr;

    hr = UpdateParamFromConsumer();
    if (FAILED(hr))
    {
        XY_LOG_ERROR("Failed to get parameters from consumer.");
        return hr;
    }

    //
    start = (start - 10000i64*m_SubtitleDelay) * m_SubtitleSpeedMul / m_SubtitleSpeedDiv; // no, it won't overflow if we use normal parameters (__int64 is enough for about 2000 hours if we multiply it by the max: 65536 as m_SubtitleSpeedMul)
    stop = (stop - 10000i64*m_SubtitleDelay) * m_SubtitleSpeedMul / m_SubtitleSpeedDiv;
    REFERENCE_TIME now = start; //fix me: (start + stop) / 2;
    m_last_requested = now;

    CComPtr<IXySubRenderFrame> sub_render_frame;
    if(m_sub_provider)
    {
        CComPtr<ISimpleSubPic> pSubPic;
        TRACE_RENDERER_REQUEST("Look up subpic for "<<ReftimeToCString(now)<<"("<<now<<")");

        hr = m_sub_provider->RequestFrame(&sub_render_frame, now);
        TRACE_RENDERER_REQUEST("Returned "<<XY_LOG_VAR_2_STR(hr)<<XY_LOG_VAR_2_STR(sub_render_frame));
        if (FAILED(hr))
        {
            return hr;
        }
        if (sub_render_frame)
        {
            sub_render_frame = DEBUG_NEW XySubRenderFrameWrapper2(sub_render_frame, m_context_id);
#ifdef SUBTITLE_FRAME_DUMP_FILE
            static int s_count=0;
            if (s_count<10 && s_count%2==0) {
                CStringA dump_file;
                dump_file.Format("%s%lld",SUBTITLE_FRAME_DUMP_FILE,start);
                DumpSubRenderFrame(sub_render_frame, dump_file.GetString());
            }
            s_count++;
#endif
        }
        if(m_xy_bool_opt[BOOL_FLIP_SUBTITLE])
        {
            //fix me:
            ASSERT(0);
        }
    }

    //fix me: print osd message
    return m_consumer->DeliverFrame(start, stop, context, sub_render_frame);
}

STDMETHODIMP XySubFilter::Disconnect( void )
{
    XY_LOG_INFO(XY_LOG_VAR_2_STR(this));
    static bool entered = false;
    //If the consumer also calls provider->Disconnect inside its Disconnect implementation, 
    //we won't fall into a dead loop.
    if (!entered)
    {
        entered = true;
        CAutoLock cAutoLock(&m_csSubLock);
        ASSERT(m_consumer);
        if (m_consumer)
        {
            HRESULT hr = m_consumer->Disconnect();
            if (FAILED(hr))
            {
                XY_LOG_WARN("Failed to disconnect with consumer!");
            }
            m_consumer = NULL;
        }
        else
        {
            XY_LOG_ERROR("No consumer connected. It is expected to be called by a consumer!");
        }
        entered = false;
        return S_OK;
    }
    return S_FALSE;
}

//
// XySubFilter
// 
void XySubFilter::SetYuvMatrix()
{
    CAutoLock cAutolock(&m_csSubLock);
    CAutoLock cAutoLock(&m_csProviderFields);
    m_xy_str_opt[STRING_YUV_MATRIX] = L"None";
    if (dynamic_cast<CRenderedTextSubtitle*>(m_curSubStream)!=NULL) {
        ColorConvTable::YuvMatrixType yuv_matrix = ColorConvTable::BT601;
        ColorConvTable::YuvRangeType  yuv_range  = ColorConvTable::RANGE_TV;
        if ( m_xy_int_opt[INT_COLOR_SPACE]==CDirectVobSub::YuvMatrix_AUTO )
        {
            if (m_video_yuv_matrix_decided_by_sub!=ColorConvTable::NONE)
            {
                yuv_matrix = m_video_yuv_matrix_decided_by_sub;
            }
            else
            {
                yuv_matrix = ColorConvTable::BT601;
            }
        }
        else
        {
            switch(m_xy_int_opt[INT_COLOR_SPACE])
            {
            case CDirectVobSub::BT_601:
                yuv_matrix = ColorConvTable::BT601;
                break;
            case CDirectVobSub::BT_709:
                yuv_matrix = ColorConvTable::BT709;
                break;
            case CDirectVobSub::GUESS:
            default:
                if (m_xy_str_opt[STRING_CONSUMER_YUV_MATRIX].Right(3).CompareNoCase(L"601")==0)
                {
                    yuv_matrix = ColorConvTable::BT601;
                }
                else if (m_xy_str_opt[STRING_CONSUMER_YUV_MATRIX].Right(3).CompareNoCase(L"709")==0)
                {
                    yuv_matrix = ColorConvTable::BT709;
                }
                else
                {
                    XY_LOG_WARN(L"Can NOT get useful YUV range from consumer:"<<m_xy_str_opt[STRING_CONSUMER_YUV_MATRIX].GetString());
                    yuv_matrix = (m_xy_size_opt[SIZE_ORIGINAL_VIDEO].cx > m_bt601Width || 
                        m_xy_size_opt[SIZE_ORIGINAL_VIDEO].cy > m_bt601Height) ? ColorConvTable::BT709 : ColorConvTable::BT601;
                }
                break;
            }
        }

        if( m_xy_int_opt[INT_YUV_RANGE]==CDirectVobSub::YuvRange_Auto )
        {
            if (m_video_yuv_range_decided_by_sub!=ColorConvTable::RANGE_NONE)
                yuv_range = m_video_yuv_range_decided_by_sub;
            else
                yuv_range = ColorConvTable::RANGE_TV;
        }
        else
        {
            switch(m_xy_int_opt[INT_YUV_RANGE])
            {
            case CDirectVobSub::YuvRange_TV:
                yuv_range = ColorConvTable::RANGE_TV;
                break;
            case CDirectVobSub::YuvRange_PC:
                yuv_range = ColorConvTable::RANGE_PC;
                break;
            case CDirectVobSub::YuvRange_Auto:
                yuv_range = ColorConvTable::RANGE_TV;
                break;
            }
        }

        ColorConvTable::SetDefaultConvType(yuv_matrix, yuv_range);

        if ( m_xy_int_opt[INT_VSFILTER_COMPACT_RGB_CORRECTION]==RGB_CORRECTION_ALWAYS ||
            (m_xy_int_opt[INT_VSFILTER_COMPACT_RGB_CORRECTION]==RGB_CORRECTION_AUTO &&
            m_xy_str_opt[STRING_CONSUMER_YUV_MATRIX].CompareNoCase(L"TV.709")==0 &&
            yuv_matrix==ColorConvTable::BT601 && yuv_range==ColorConvTable::RANGE_TV ))
        {
            XySubRenderFrameCreater::GetDefaultCreater()->SetVsfilterCompactRgbCorrection(true);
            m_xy_str_opt[STRING_YUV_MATRIX] = L"None";
            return;
        }

        if (yuv_range==ColorConvTable::RANGE_TV) {
            m_xy_str_opt[STRING_YUV_MATRIX] = L"TV";
        }
        else if (yuv_range==ColorConvTable::RANGE_PC) {
            m_xy_str_opt[STRING_YUV_MATRIX] = L"PC";
        }
        else {
            XY_LOG_WARN("This is unexpected."<<XY_LOG_VAR_2_STR(yuv_range));
            m_xy_str_opt[STRING_YUV_MATRIX] = L"None";
            return;
        }
        if (yuv_matrix==ColorConvTable::BT601) {
            m_xy_str_opt[STRING_YUV_MATRIX] += L".601";
        }
        else if (yuv_matrix==ColorConvTable::BT709) {
            m_xy_str_opt[STRING_YUV_MATRIX] += L".709";
        }
        else {
            XY_LOG_WARN("This is unexpected."<<XY_LOG_VAR_2_STR(yuv_matrix));
            m_xy_str_opt[STRING_YUV_MATRIX] = L"None";
            return;
        }
    }
    else if (dynamic_cast<HdmvSubtitleProvider*>(m_curSubStream)!=NULL)
    {
        if ( m_xy_str_opt[STRING_PGS_YUV_RANGE].CompareNoCase(_T("PC"))==0 )
        {
            m_xy_str_opt[STRING_YUV_MATRIX] = L"PC";
        }
        else if ( m_xy_str_opt[STRING_PGS_YUV_RANGE].CompareNoCase(_T("TV"))==0 )
        {
            m_xy_str_opt[STRING_YUV_MATRIX] = L"TV";
        }
        else
        {
            if (m_xy_str_opt[STRING_CONSUMER_YUV_MATRIX].Left(2).CompareNoCase(L"TV")==0)
            {
                m_xy_str_opt[STRING_YUV_MATRIX] = L"TV";
            }
            else if (m_xy_str_opt[STRING_CONSUMER_YUV_MATRIX].Left(2).CompareNoCase(L"PC")==0)
            {
                m_xy_str_opt[STRING_YUV_MATRIX] = L"PC";
            }
            else
            {
                XY_LOG_WARN(L"Can NOT get useful YUV range from consumer:"<<m_xy_str_opt[STRING_CONSUMER_YUV_MATRIX].GetString());
                m_xy_str_opt[STRING_YUV_MATRIX] = L"TV";
            }
        }
        if ( m_xy_str_opt[STRING_PGS_YUV_MATRIX].CompareNoCase(_T("BT601"))==0 )
        {
            m_xy_str_opt[STRING_YUV_MATRIX] += L"601";
        }
        else if ( m_xy_str_opt[STRING_PGS_YUV_MATRIX].CompareNoCase(_T("BT709"))==0 )
        {
            m_xy_str_opt[STRING_YUV_MATRIX] += L"709";
        }
        else
        {
            if (m_xy_str_opt[STRING_CONSUMER_YUV_MATRIX].Right(3).CompareNoCase(L"601")==0)
            {
                m_xy_str_opt[STRING_YUV_MATRIX] += L"601";
            }
            else if (m_xy_str_opt[STRING_CONSUMER_YUV_MATRIX].Right(3).CompareNoCase(L"709")==0)
            {
                m_xy_str_opt[STRING_YUV_MATRIX] += L"709";
            }
            else
            {
                XY_LOG_WARN(L"Can NOT get useful YUV range from consumer:"<<m_xy_str_opt[STRING_CONSUMER_YUV_MATRIX].GetString()
                    <<"A guess based on subtitle width would be done. We do NOT know the result before reveiving a sample.");
                m_xy_str_opt[STRING_YUV_MATRIX] = L"None";
            }
        }
    }
    else if (m_curSubStream)
    {
        XY_LOG_WARN("I do NOT know how to make VSFilter compact and right.");
    }
}

bool XySubFilter::Open()
{
    XY_AUTO_TIMING(TEXT("XySubFilter::Open"));

    HRESULT hr = NOERROR;
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    CAutoLock cAutolock(&m_csSubLock);

    m_pSubStreams.RemoveAll();
    m_fIsSubStreamEmbeded.RemoveAll();

    m_frd.files.RemoveAll();

    CAtlArray<CString> paths;

    for(int i = 0; i < 10; i++)
    {
        CString tmp;
        tmp.Format(IDS_RP_PATH, i);
        CString path = theApp.GetProfileString(ResStr(IDS_R_DEFTEXTPATHES), tmp);
        if(!path.IsEmpty()) paths.Add(path);
    }

    CAtlArray<SubFile> ret;
    GetSubFileNames(m_xy_str_opt[STRING_FILE_NAME], paths, m_xy_str_opt[DirectVobSubXyOptions::STRING_LOAD_EXT_LIST], ret);

    for(unsigned i = 0; i < ret.GetCount(); i++)
    {
        if(m_frd.files.Find(ret[i].full_file_name))
            continue;

        CComPtr<ISubStream> pSubStream;

        if(!pSubStream)
        {
            //            CAutoTiming t(TEXT("CRenderedTextSubtitle::Open"), 0);
            XY_AUTO_TIMING(TEXT("CRenderedTextSubtitle::Open"));
            CAutoPtr<CRenderedTextSubtitle> pRTS(DEBUG_NEW CRenderedTextSubtitle(&m_csSubLock));
            if(pRTS && pRTS->Open(ret[i].full_file_name, DEFAULT_CHARSET) && pRTS->GetStreamCount() > 0)
            {
                pSubStream = pRTS.Detach();
                m_frd.files.AddTail(ret[i].full_file_name + _T(".style"));
            }
        }

        if(!pSubStream)
        {
            CAutoTiming t(TEXT("CVobSubFile::Open"), 0);
            CAutoPtr<CVobSubFile> pVSF(DEBUG_NEW CVobSubFile(&m_csSubLock));
            if(pVSF && pVSF->Open(ret[i].full_file_name) && pVSF->GetStreamCount() > 0)
            {
                pSubStream = pVSF.Detach();
                m_frd.files.AddTail(ret[i].full_file_name.Left(ret[i].full_file_name.GetLength()-4) + _T(".sub"));
            }
        }

        if(!pSubStream)
        {
            CAutoTiming t(TEXT("ssf::CRenderer::Open"), 0);
            CAutoPtr<ssf::CRenderer> pSSF(DEBUG_NEW ssf::CRenderer(&m_csSubLock));
            if(pSSF && pSSF->Open(ret[i].full_file_name) && pSSF->GetStreamCount() > 0)
            {
                pSubStream = pSSF.Detach();
            }
        }

        if(pSubStream)
        {
            m_pSubStreams.AddTail(pSubStream);
            m_fIsSubStreamEmbeded.AddTail(false);
            m_frd.files.AddTail(ret[i].full_file_name);
        }
    }

    for(unsigned i = 0; i < m_pSubtitleInputPin.GetCount(); i++)
    {
        if(m_pSubtitleInputPin[i]->IsConnected())
        {
            ISubStream * sub_stream = m_pSubtitleInputPin[i]->GetSubStream();
            if (sub_stream != NULL)
            {
                m_pSubStreams.AddTail(sub_stream);
                m_fIsSubStreamEmbeded.AddTail(true);
            }
        }
    }

    hr = put_SelectedLanguage(FindPreferedLanguage());
    CHECK_N_LOG(hr, "Failed to set option");
    if (S_FALSE == hr)
        UpdateSubtitle(false); // make sure pSubPicProvider of our queue gets updated even if the stream number hasn't changed

    m_frd.RefreshEvent.Set();

    return(m_pSubStreams.GetCount() > 0);
}

void XySubFilter::UpdateSubtitle(bool fApplyDefStyle/*= true*/)
{
    XY_LOG_INFO(XY_LOG_VAR_2_STR(fApplyDefStyle));
    CAutoLock cAutolock(&m_csSubLock);

    if (m_last_requested!=-1)
    {
        XY_LOG_WARN("Some subtitle frames are cached already!");
        m_last_requested = -1;
    }
    InvalidateSubtitle();

    CComPtr<ISubStream> pSubStream;

    if(!m_xy_bool_opt[BOOL_HIDE_SUBTITLES])
    {
        int i = m_xy_int_opt[INT_SELECTED_LANGUAGE];

        for(POSITION pos = m_pSubStreams.GetHeadPosition(); i >= 0 && pos; pSubStream = NULL)
        {
            pSubStream = m_pSubStreams.GetNext(pos);

            if(i < pSubStream->GetStreamCount())
            {
                CAutoLock cAutoLock(&m_csSubLock);
                pSubStream->SetStream(i);
                break;
            }

            i -= pSubStream->GetStreamCount();
        }
    }

    SetSubtitle(pSubStream, fApplyDefStyle);
    XY_LOG_INFO("SelectedSubtitle:"<<m_xy_int_opt[INT_SELECTED_LANGUAGE]
    <<" YuvMatrix:"<<m_xy_str_opt[STRING_YUV_MATRIX].GetString()
    <<" TextSubRendererVSFilterCompactRGBCorrection:"<<XySubRenderFrameCreater::GetDefaultCreater()->GetVsfilterCompactRgbCorrection());
}

void XySubFilter::SetSubtitle( ISubStream* pSubStream, bool fApplyDefStyle /*= true*/ )
{
    HRESULT hr = NOERROR;
    XY_LOG_INFO(XY_LOG_VAR_2_STR(pSubStream)<<XY_LOG_VAR_2_STR(fApplyDefStyle));
    CAutoLock cAutolock(&m_csSubLock);

    CSize playres(0,0);
    m_video_yuv_matrix_decided_by_sub = ColorConvTable::NONE;
    m_video_yuv_range_decided_by_sub = ColorConvTable::RANGE_NONE;
    if(pSubStream)
    {
        CAutoLock cAutolock(&m_csSubLock);

        CLSID clsid;
        pSubStream->GetClassID(&clsid);

        if(clsid == __uuidof(CVobSubFile))
        {
            CVobSubSettings* pVSS = dynamic_cast<CVobSubFile*>(pSubStream);

            if(fApplyDefStyle)
            {
                pVSS->SetAlignment(m_xy_bool_opt[BOOL_OVERRIDE_PLACEMENT], m_xy_size_opt[SIZE_PLACEMENT_PERC].cx, m_xy_size_opt[SIZE_PLACEMENT_PERC].cy, 1, 1);
                pVSS->m_fOnlyShowForcedSubs = m_xy_bool_opt[BOOL_VOBSUBSETTINGS_ONLY_SHOW_FORCED_SUBS];
            }
        }
        else if(clsid == __uuidof(CVobSubStream))
        {
            CVobSubSettings* pVSS = dynamic_cast<CVobSubStream*>(pSubStream);

            if(fApplyDefStyle)
            {
                pVSS->SetAlignment(m_xy_bool_opt[BOOL_OVERRIDE_PLACEMENT], m_xy_size_opt[SIZE_PLACEMENT_PERC].cx, m_xy_size_opt[SIZE_PLACEMENT_PERC].cy, 1, 1);
                pVSS->m_fOnlyShowForcedSubs = m_xy_bool_opt[BOOL_VOBSUBSETTINGS_ONLY_SHOW_FORCED_SUBS];
            }
        }
        else if(clsid == __uuidof(CRenderedTextSubtitle))
        {
            CRenderedTextSubtitle* pRTS = dynamic_cast<CRenderedTextSubtitle*>(pSubStream);

            if(fApplyDefStyle || pRTS->m_fUsingAutoGeneratedDefaultStyle)
            {
                STSStyle s = m_defStyle;

                if(m_xy_bool_opt[BOOL_OVERRIDE_PLACEMENT])
                {
                    s.scrAlignment = 2;
                    int w = pRTS->m_dstScreenSize.cx;
                    int h = pRTS->m_dstScreenSize.cy;
                    CRect tmp_rect = s.marginRect.get();
                    int mw = w - tmp_rect.left - tmp_rect.right;
                    tmp_rect.bottom = h - MulDiv(h, m_xy_size_opt[SIZE_PLACEMENT_PERC].cy, 100);
                    tmp_rect.left = MulDiv(w, m_xy_size_opt[SIZE_PLACEMENT_PERC].cx, 100) - mw/2;
                    tmp_rect.right = w - (tmp_rect.left + mw);
                    s.marginRect = tmp_rect;
                }

                bool succeeded = pRTS->SetDefaultStyle(s);
                if (!succeeded)
                {
                    XY_LOG_ERROR("Failed to set default style");
                }
            }

            pRTS->m_ePARCompensationType = static_cast<CSimpleTextSubtitle::EPARCompensationType>(m_xy_int_opt[INT_ASPECT_RATIO_SETTINGS]);
            if (m_xy_size_opt[SIZE_AR_ADJUSTED_VIDEO] != CSize(0,0) && m_xy_size_opt[SIZE_ORIGINAL_VIDEO] != CSize(0,0))
            {
                pRTS->m_dPARCompensation = abs(m_xy_size_opt[SIZE_ORIGINAL_VIDEO].cx * m_xy_size_opt[SIZE_AR_ADJUSTED_VIDEO].cy) /
                    (double)abs(m_xy_size_opt[SIZE_ORIGINAL_VIDEO].cy * m_xy_size_opt[SIZE_AR_ADJUSTED_VIDEO].cx);

            }
            else
            {
                pRTS->m_dPARCompensation = 1.00;
            }

            switch(pRTS->m_eYCbCrMatrix)
            {
            case CSimpleTextSubtitle::YCbCrMatrix_BT601:
                m_video_yuv_matrix_decided_by_sub = ColorConvTable::BT601;
                break;
            case CSimpleTextSubtitle::YCbCrMatrix_BT709:
                m_video_yuv_matrix_decided_by_sub = ColorConvTable::BT709;
                break;
            default:
                m_video_yuv_matrix_decided_by_sub = ColorConvTable::NONE;
                break;
            }
            switch(pRTS->m_eYCbCrRange)
            {
            case CSimpleTextSubtitle::YCbCrRange_PC:
                m_video_yuv_range_decided_by_sub = ColorConvTable::RANGE_PC;
                break;
            case CSimpleTextSubtitle::YCbCrRange_TV:
                m_video_yuv_range_decided_by_sub = ColorConvTable::RANGE_TV;
                break;
            default:
                m_video_yuv_range_decided_by_sub = ColorConvTable::RANGE_NONE;
                break;
            }
            pRTS->Deinit();
            playres = pRTS->m_dstScreenSize;
        }
        else if(clsid == __uuidof(HdmvSubtitleProvider))
        {
            HdmvSubtitleProvider *sub = dynamic_cast<HdmvSubtitleProvider*>(pSubStream);
            CompositionObject::ColorType color_type = CompositionObject::NONE;
            CompositionObject::YuvRangeType range_type = CompositionObject::RANGE_NONE;
            if ( m_xy_str_opt[STRING_PGS_YUV_RANGE].CompareNoCase(_T("PC"))==0 )
            {
                range_type = CompositionObject::RANGE_PC;
            }
            else if ( m_xy_str_opt[STRING_PGS_YUV_RANGE].CompareNoCase(_T("TV"))==0 )
            {
                range_type = CompositionObject::RANGE_TV;
            }
            else
            {
                if (m_xy_str_opt[STRING_CONSUMER_YUV_MATRIX].Left(2).CompareNoCase(L"TV")==0)
                {
                    range_type = CompositionObject::RANGE_TV;
                }
                else if (m_xy_str_opt[STRING_CONSUMER_YUV_MATRIX].Left(2).CompareNoCase(L"PC")==0)
                {
                    range_type = CompositionObject::RANGE_PC;
                }
                else
                {
                    XY_LOG_WARN(L"Can NOT get useful YUV range from consumer:"<<m_xy_str_opt[STRING_CONSUMER_YUV_MATRIX].GetString());
                    range_type = CompositionObject::RANGE_TV;
                }
            }
            if ( m_xy_str_opt[STRING_PGS_YUV_MATRIX].CompareNoCase(_T("BT601"))==0 )
            {
                color_type = CompositionObject::YUV_Rec601;
            }
            else if ( m_xy_str_opt[STRING_PGS_YUV_MATRIX].CompareNoCase(_T("BT709"))==0 )
            {
                color_type = CompositionObject::YUV_Rec709;
            }
            else
            {
                if (m_xy_str_opt[STRING_CONSUMER_YUV_MATRIX].Right(3).CompareNoCase(L"601")==0)
                {
                    color_type = CompositionObject::YUV_Rec601;
                }
                else if (m_xy_str_opt[STRING_CONSUMER_YUV_MATRIX].Right(3).CompareNoCase(L"709")==0)
                {
                    color_type = CompositionObject::YUV_Rec709;
                }
                else
                {
                    XY_LOG_WARN(L"Can NOT get useful YUV matrix from consumer:"<<m_xy_str_opt[STRING_CONSUMER_YUV_MATRIX].GetString());
                }
            }
            sub->SetYuvType(color_type, range_type);
        }
    }

    if(!fApplyDefStyle)
    {
        int i = 0;

        POSITION pos = m_pSubStreams.GetHeadPosition();
        while(pos)
        {
            CComPtr<ISubStream> pSubStream2 = m_pSubStreams.GetNext(pos);

            if(pSubStream == pSubStream2)
            {
                m_xy_int_opt[INT_SELECTED_LANGUAGE] = i + pSubStream2->GetStream();
                break;
            }

            i += pSubStream2->GetStreamCount();
        }
    }

    m_curSubStream = pSubStream;

    SetYuvMatrix();

    m_xy_size_opt[SIZE_ASS_PLAY_RESOLUTION] = playres;

    if (CComQIPtr<IXySubRenderProvider>(pSubStream))
    {
        m_sub_provider = CComQIPtr<IXySubRenderProvider>(pSubStream);
    }
    else if (CComQIPtr<ISubPicProviderEx2>(pSubStream))
    {
        m_sub_provider = DEBUG_NEW XySubRenderProviderWrapper2(CComQIPtr<ISubPicProviderEx2>(pSubStream), NULL);
    }
    else if (CComQIPtr<ISubPicProviderEx>(pSubStream))
    {
        m_sub_provider = DEBUG_NEW XySubRenderProviderWrapper(CComQIPtr<ISubPicProviderEx>(pSubStream), NULL);
    }
    else
    {
        m_sub_provider = NULL;
        if (pSubStream!=NULL) 
        {
            XY_LOG_WARN("This subtitle stream is NOT supported");
        }
    }
    if (m_sub_provider)
    {
        m_sub_provider->Connect(this);
    }
}

void XySubFilter::InvalidateSubtitle( REFERENCE_TIME rtInvalidate /*= -1*/, DWORD_PTR nSubtitleId /*= -1*/ )
{
    CAutoLock cAutolock(&m_csSubLock);

    if(m_sub_provider)
    {
        if (nSubtitleId == -1 || nSubtitleId == reinterpret_cast<DWORD_PTR>(m_curSubStream))
        {
            if (m_last_requested>rtInvalidate)
            {
                ASSERT(0);
                XY_LOG_ERROR("New subtitle samples received after a request.");
            }
            m_sub_provider->Invalidate(rtInvalidate);
        }
    }
}

void XySubFilter::InitSubPicQueue()
{
    CAutoLock cAutoLock(&m_csSubLock);

    CacheManager::GetPathDataMruCache()->SetMaxItemNum(m_xy_int_opt[INT_PATH_DATA_CACHE_MAX_ITEM_NUM]);
    CacheManager::GetScanLineData2MruCache()->SetMaxItemNum(m_xy_int_opt[INT_SCAN_LINE_DATA_CACHE_MAX_ITEM_NUM]);
    CacheManager::GetOverlayNoBlurMruCache()->SetMaxItemNum(m_xy_int_opt[INT_OVERLAY_NO_BLUR_CACHE_MAX_ITEM_NUM]);
    CacheManager::GetOverlayMruCache()->SetMaxItemNum(m_xy_int_opt[INT_OVERLAY_CACHE_MAX_ITEM_NUM]);

    XyFwGroupedDrawItemsHashKey::GetCacher()->SetMaxItemNum(m_xy_int_opt[INT_BITMAP_MRU_CACHE_ITEM_NUM]);
    CacheManager::GetBitmapMruCache()->SetMaxItemNum(m_xy_int_opt[INT_BITMAP_MRU_CACHE_ITEM_NUM]);

    CacheManager::GetClipperAlphaMaskMruCache()->SetMaxItemNum(m_xy_int_opt[INT_CLIPPER_MRU_CACHE_ITEM_NUM]);
    CacheManager::GetTextInfoCache()->SetMaxItemNum(m_xy_int_opt[INT_TEXT_INFO_CACHE_ITEM_NUM]);
    CacheManager::GetAssTagListMruCache()->SetMaxItemNum(m_xy_int_opt[INT_ASS_TAG_LIST_CACHE_ITEM_NUM]);

    SubpixelPositionControler::GetGlobalControler().SetSubpixelLevel( static_cast<SubpixelPositionControler::SUBPIXEL_LEVEL>(m_xy_int_opt[INT_SUBPIXEL_POS_LEVEL]) );

    UpdateSubtitle(false);
}


HRESULT XySubFilter::UpdateParamFromConsumer( bool getNameAndVersion/*=false*/ )
{

#define GET_PARAM_FROM_CONSUMER(func, param, ret_addr) \
    hr = func(param, ret_addr);\
    if (FAILED(hr))\
    {\
        XY_LOG_ERROR("Failed to get "param" from consumer.");\
        return E_UNEXPECTED;\
    }

    HRESULT hr = NOERROR;

    if (getNameAndVersion)
    {
        LPWSTR str;
        int len;
        hr = m_consumer->GetString("name", &str, &len);
        if (FAILED(hr))
        {
            XY_LOG_ERROR("Failed to get name from consumer");
        }
        else
        {
            m_xy_str_opt[STRING_CONNECTED_CONSUMER] = CStringW(str, len);
        }
        hr = m_consumer->GetString("version", &str, &len);
        if (FAILED(hr))
        {
            XY_LOG_ERROR("Failed to get version from consumer");
        }
        else
        {
            m_xy_str_opt[STRING_CONSUMER_VERSION] = CStringW(str, len);
        }
    }
    SIZE originalVideoSize;
    GET_PARAM_FROM_CONSUMER(m_consumer->GetSize, "originalVideoSize", &originalVideoSize);

    SIZE arAdjustedVideoSize;
    GET_PARAM_FROM_CONSUMER(m_consumer->GetSize, "arAdjustedVideoSize", &arAdjustedVideoSize);

    RECT videoOutputRect;
    GET_PARAM_FROM_CONSUMER(m_consumer->GetRect, "videoOutputRect", &videoOutputRect);

    RECT subtitleTargetRect;
    GET_PARAM_FROM_CONSUMER(m_consumer->GetRect, "subtitleTargetRect", &subtitleTargetRect);

    LPWSTR str;
    int len;
    hr = m_consumer->GetString("yuvMatrix", &str, &len);
    if (FAILED(hr)) {
        XY_LOG_ERROR("Failed to get yuvMatrix from consumer.");
    }
    CStringW consumer_yuv_matrix(str, len);
    LocalFree(str);

    ULONGLONG rt_fps;
    hr = m_consumer->GetUlonglong("frameRate", &rt_fps);
    if (FAILED(hr))
    {
        XY_LOG_ERROR("Failed to get fps from consumer.");
        rt_fps = 10000000.0/24.0;
    }
    //fix me: is it right?
    //fix me: use REFERENCE_TIME instead?
    double fps = 10000000.0/rt_fps;
    bool update_subtitle = false;

    if (m_xy_size_opt[SIZE_ORIGINAL_VIDEO]!=originalVideoSize)
    {
        XY_LOG_INFO("Size original video changed from "<<m_xy_size_opt[SIZE_ORIGINAL_VIDEO]
            <<" to "<<originalVideoSize);
        m_xy_size_opt[SIZE_ORIGINAL_VIDEO] = originalVideoSize;
        update_subtitle = true;
    }
    if (m_xy_size_opt[SIZE_AR_ADJUSTED_VIDEO]!=arAdjustedVideoSize)
    {
        XY_LOG_INFO("Size AR adjusted video changed from "<<m_xy_size_opt[SIZE_AR_ADJUSTED_VIDEO]
            <<" to "<<arAdjustedVideoSize);
        m_xy_size_opt[SIZE_AR_ADJUSTED_VIDEO] = arAdjustedVideoSize;
        update_subtitle = true;
    }
    if (m_xy_rect_opt[RECT_VIDEO_OUTPUT]!=videoOutputRect)
    {
        XY_LOG_INFO("Video output rect changed from "<<m_xy_rect_opt[RECT_VIDEO_OUTPUT]
            <<" to "<<videoOutputRect);
        m_xy_rect_opt[RECT_VIDEO_OUTPUT] =videoOutputRect;
        update_subtitle = true;
    }
    if (m_xy_rect_opt[RECT_SUBTITLE_TARGET]!=subtitleTargetRect)
    {
        XY_LOG_INFO("Subtitle target rect changed from "<<m_xy_rect_opt[RECT_SUBTITLE_TARGET]
            <<" to "<<subtitleTargetRect);
        m_xy_rect_opt[RECT_SUBTITLE_TARGET] =subtitleTargetRect;
        update_subtitle = true;
    }
    if (m_xy_double_opt[DOUBLE_FPS]!=fps)
    {
        XY_LOG_INFO("FPS changed from "<<m_xy_double_opt[DOUBLE_FPS]
            <<" to "<<fps);
        hr = XySetDouble(DOUBLE_FPS, fps);
        CHECK_N_LOG(hr, "Failed to set option");
        ASSERT(SUCCEEDED(hr));
    }
    if (m_xy_str_opt[STRING_CONSUMER_YUV_MATRIX]!=consumer_yuv_matrix) {
        XY_LOG_INFO(L"Consumer yuv matrix changed from "<<m_xy_str_opt[STRING_CONSUMER_YUV_MATRIX].GetString()
        <<L" to "<<consumer_yuv_matrix.GetString());
        m_xy_str_opt[STRING_CONSUMER_YUV_MATRIX] = consumer_yuv_matrix;
    }
    update_subtitle &= m_consumer_options_read;
    if (update_subtitle)
    {
        UpdateSubtitle(false);
    }
    m_consumer_options_read = true;
    return hr;
}

#define MAXPREFLANGS 5

int XySubFilter::FindPreferedLanguage( bool fHideToo /*= true*/ )
{
    XY_LOG_INFO(XY_LOG_VAR_2_STR(fHideToo));
    HRESULT hr = NOERROR;
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    int nLangs = 0;
    hr = get_LanguageCount(&nLangs);
    CHECK_N_LOG(hr, "Failed to get option");

    if(nLangs <= 0) {
        XY_LOG_WARN("There is NO languages yet");
        return(0);
    }

    for(int i = 0; i < MAXPREFLANGS; i++)
    {
        CString tmp;
        tmp.Format(IDS_RL_LANG, i);

        CString lang = theApp.GetProfileString(ResStr(IDS_R_PREFLANGS), tmp);

        if(!lang.IsEmpty())
        {
            for(int ret = 0; ret < nLangs; ret++)
            {
                CString l;
                WCHAR* pName = NULL;
                hr = get_LanguageName(ret, &pName);
                CHECK_N_LOG(hr, "Failed to get option");

                l = pName;
                CoTaskMemFree(pName);

                if(!l.CompareNoCase(lang)) return(ret);
            }
        }
    }

    return(0);
}

void XySubFilter::UpdatePreferedLanguages(CString l)
{
    XY_LOG_INFO(l);
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    CString langs[MAXPREFLANGS+1];

    int i = 0, j = 0, k = -1;
    for(; i < MAXPREFLANGS; i++)
    {
        CString tmp;
        tmp.Format(IDS_RL_LANG, i);

        langs[j] = theApp.GetProfileString(ResStr(IDS_R_PREFLANGS), tmp);

        if(!langs[j].IsEmpty())
        {
            if(!langs[j].CompareNoCase(l)) k = j;
            j++;
        }
    }

    if(k == -1)
    {
        langs[k = j] = l;
        j++;
    }

    // move the selected to the top of the list

    while(k > 0)
    {
        CString tmp = langs[k]; langs[k] = langs[k-1]; langs[k-1] = tmp;
        k--;
    }

    // move "Hide subtitles" to the last position if it wasn't our selection

    CString hidesubs;
    hidesubs.LoadString(IDS_M_HIDESUBTITLES);

    for(k = 1; k < j; k++)
    {
        if(!langs[k].CompareNoCase(hidesubs)) break;
    }

    while(k < j-1)
    {
        CString tmp = langs[k]; langs[k] = langs[k+1]; langs[k+1] = tmp;
        k++;
    }

    for(i = 0; i < j; i++)
    {
        CString tmp;
        tmp.Format(IDS_RL_LANG, i);

        theApp.WriteProfileString(ResStr(IDS_R_PREFLANGS), tmp, langs[i]);
    }
}

void XySubFilter::AddSubStream(ISubStream* pSubStream)
{
    XY_LOG_INFO(pSubStream);
    CAutoLock cAutolock(&m_csSubLock);

    int len = m_pSubtitleInputPin.GetCount();
    for(unsigned i = 0; i < m_pSubtitleInputPin.GetCount(); i++)
        if(m_pSubtitleInputPin[i]->IsConnected()) len--;

    if(len == 0)
    {
        HRESULT hr = S_OK;
        m_pSubtitleInputPin.Add(DEBUG_NEW SubtitleInputPin2(this, m_pLock, &m_csSubLock, &hr));
    }

    if (pSubStream!=NULL)
    {
        POSITION pos = m_pSubStreams.Find(pSubStream);
        if(!pos)
        {
            m_pSubStreams.AddTail(pSubStream);
            m_fIsSubStreamEmbeded.AddTail(true);//todo: fix me
        }

        UpdateSubtitle(false);
    }
}

void XySubFilter::RemoveSubStream(ISubStream* pSubStream)
{
    XY_LOG_INFO(pSubStream);
    CAutoLock cAutolock(&m_csSubLock);
    if (pSubStream!=NULL)
    {
        POSITION pos = m_pSubStreams.GetHeadPosition();
        POSITION pos2 = m_fIsSubStreamEmbeded.GetHeadPosition();
        while(pos!=NULL)
        {
            if( m_pSubStreams.GetAt(pos)==pSubStream )
            {
                m_pSubStreams.RemoveAt(pos);
                m_fIsSubStreamEmbeded.RemoveAt(pos2);
                break;
            }
            else
            {
                m_pSubStreams.GetNext(pos);
                m_fIsSubStreamEmbeded.GetNext(pos2);
            }
        }
        UpdateSubtitle(false);
    }
}

HRESULT XySubFilter::CheckInputType( const CMediaType* pmt )
{
    bool result = m_xy_int_opt[INT_LOAD_SETTINGS_LEVEL]==LOADLEVEL_ALWAYS 
        || (m_xy_int_opt[INT_LOAD_SETTINGS_LEVEL]!=LOADLEVEL_DISABLED && (
        (m_xy_bool_opt[BOOL_LOAD_SETTINGS_EMBEDDED] && !SubtitleInputPin2::IsDummy(pmt)) ||
        (m_xy_bool_opt[BOOL_LOAD_SETTINGS_EXTENAL]||m_xy_bool_opt[BOOL_LOAD_SETTINGS_WEB] && SubtitleInputPin2::IsDummy(pmt))
        ));
    return result ? S_OK : VFW_E_TYPE_NOT_ACCEPTED;
}

HRESULT XySubFilter::CompleteConnect( SubtitleInputPin2* pSubPin, IPin* pReceivePin )
{
    ASSERT(m_pGraph);
    LoadExternalSubtitle(m_pGraph);
    if(!m_hSystrayThread && !m_xy_bool_opt[BOOL_HIDE_TRAY_ICON])
    {
        m_tbid.graph = m_pGraph;
        m_tbid.dvs = static_cast<IDirectVobSub*>(this);

        DWORD tid;
        m_hSystrayThread = CreateThread(0, 0, SystrayThreadProc, &m_tbid, 0, &tid);
        XY_LOG_INFO("Systray thread created "<<m_hSystrayThread);
    }
    return S_OK;
}

HRESULT XySubFilter::GetIsEmbeddedSubStream( int iSelected, bool *fIsEmbedded )
{
    CAutoLock cAutolock(&m_csSubLock);

    HRESULT hr = E_INVALIDARG;
    if (!fIsEmbedded)
    {
        return S_FALSE;
    }

    int i = iSelected;
    *fIsEmbedded = false;

    POSITION pos = m_pSubStreams.GetHeadPosition();
    POSITION pos2 = m_fIsSubStreamEmbeded.GetHeadPosition();
    while(i >= 0 && pos)
    {
        CComPtr<ISubStream> pSubStream = m_pSubStreams.GetNext(pos);
        bool isEmbedded = m_fIsSubStreamEmbeded.GetNext(pos2);
        if(i < pSubStream->GetStreamCount())
        {
            hr = NOERROR;
            *fIsEmbedded = isEmbedded;
            break;
        }

        i -= pSubStream->GetStreamCount();
    }
    return hr;
}

bool XySubFilter::LoadExternalSubtitle(IFilterGraph* pGraph)
{
    XY_LOG_INFO(pGraph);

    bool fRet = false;
    HRESULT hr = NOERROR;
    int level;
    bool m_fExternalLoad, m_fWebLoad, m_fEmbeddedLoad;
    hr = get_LoadSettings(&level, &m_fExternalLoad, &m_fWebLoad, &m_fEmbeddedLoad);
    if (FAILED(hr))
    {
        XY_LOG_ERROR("Failed to get option");
        return false;
    }

    if(level < 0 || level >= LOADLEVEL_COUNT || level==LOADLEVEL_DISABLED) {
        XY_LOG_DEBUG("Disabled by load setting: "<<XY_LOG_VAR_2_STR(level));
        return false;
    }

    if(level == LOADLEVEL_ALWAYS)
        m_fExternalLoad = m_fWebLoad = m_fEmbeddedLoad = true;

    // find file name

    CStringW fn;

    BeginEnumFilters(pGraph, pEF, pBF)
    {
        if(CComQIPtr<IFileSourceFilter> pFSF = pBF)
        {
            LPOLESTR fnw = NULL;
            if(!pFSF || FAILED(pFSF->GetCurFile(&fnw, NULL)) || !fnw)
                continue;
            fn = CString(fnw);
            CoTaskMemFree(fnw);
            break;
        }
    }
    EndEnumFilters;
    XY_LOG_INFO(L"fn:"<<fn.GetString());
    if((m_fExternalLoad || m_fWebLoad) && (m_fWebLoad || !(wcsstr(fn, L"http://") || wcsstr(fn, L"mms://"))))
    {
        bool fTemp = m_xy_bool_opt[BOOL_HIDE_SUBTITLES];
        fRet = !fn.IsEmpty() && SUCCEEDED(put_FileName((LPWSTR)(LPCWSTR)fn))
            || SUCCEEDED(put_FileName(L"c:\\tmp.srt"));
        if(fTemp) m_xy_bool_opt[BOOL_HIDE_SUBTITLES] = true;
    }

    return fRet;
}

HRESULT XySubFilter::StartStreaming()
{
    XY_LOG_INFO("");
    /* WARNING: calls to m_pGraph member functions from within this function will generate deadlock with Haali
     * Video Renderer in MPC. Reason is that CAutoLock's variables in IFilterGraph functions are overriden by
     * CFGManager class.
     */

    m_fLoading = false;

    m_tbid.fRunOnce = true;

    InitSubPicQueue();

    HRESULT hr = put_MediaFPS(m_xy_bool_opt[BOOL_MEDIA_FPS_ENABLED], m_xy_double_opt[DOUBLE_MEDIA_FPS]);
    CHECK_N_LOG(hr, "Failed to set option");

    return S_OK;
}

HRESULT XySubFilter::FindAndConnectConsumer(IFilterGraph* pGraph)
{
    XY_LOG_INFO(pGraph);
    HRESULT hr = NOERROR;
    CComPtr<ISubRenderConsumer> consumer;
    ULONG meric = 0;
    if(pGraph)
    {
        BeginEnumFilters(pGraph, pEF, pBF)
        {
            if(pBF != (IBaseFilter*)this)
            {
                CComQIPtr<ISubRenderConsumer> new_consumer(pBF);
                if (new_consumer)
                {
                    ULONG new_meric = 0;
                    HRESULT hr2 = new_consumer->GetMerit(&new_meric);
                    if (SUCCEEDED(hr2))
                    {
                        if (meric < new_meric)
                        {
                            consumer = new_consumer;
                            meric = new_meric;
                        }
                    }
                    else
                    {
                        XY_LOG_ERROR("Failed to get meric from consumer. "<<XY_LOG_VAR_2_STR(pBF));
                        if (!consumer)
                        {
                            XY_LOG_WARN("No consumer found yet.");
                            consumer = new_consumer;
                        }
                    }
                }
            }
        }
        EndEnumFilters;//Add a ; so that my editor can do a correct auto-indent
        if (consumer)
        {
            hr = consumer->Connect(this);
            if (FAILED(hr))
            {
                XY_LOG_ERROR("Failed to connect "<<XY_LOG_VAR_2_STR(consumer)<<XY_LOG_VAR_2_STR(hr));
                return hr;
            }
            m_consumer = consumer;
            m_consumer_options_read = false;
            hr = UpdateParamFromConsumer(true);
            if (FAILED(hr))
            {
                XY_LOG_WARN("Failed to read consumer field.");
                if (FAILED(consumer->Disconnect()))
                {
                    XY_LOG_ERROR("Failed to disconnect consumer");
                }
                m_consumer = NULL;
                return hr;
            }
            XY_LOG_INFO("Connected with "<<XY_LOG_VAR_2_STR(consumer)<<" "
                <<DumpConsumerInfo().GetString()<<" as "<<DumpProviderInfo().GetString());
        }
        else
        {
            XY_LOG_INFO("No consumer found.");
            hr = S_FALSE;
        }
    }
    else
    {
        XY_LOG_ERROR("Unexpected parameter. "<<"pGraph:"<<(void*)pGraph);
        hr = E_INVALIDARG;
    }
    return hr;
}

void XySubFilter::UpdateLanguageCount()
{
    CAutoLock cAutolock(&m_csSubLock);

    int tmp = 0;
    POSITION pos = m_pSubStreams.GetHeadPosition();
    while(pos) {
        tmp += m_pSubStreams.GetNext(pos)->GetStreamCount();
    }
    m_xy_int_opt[INT_LANGUAGE_COUNT] = tmp;
}

CStringW XySubFilter::DumpProviderInfo()
{
    CAutoLock cAutoLock(&m_csProviderFields);
    CStringW strTemp;
    strTemp.Format(L"name:'%ls' version:'%ls' yuvMatrix:'%ls' combineBitmaps:%ls",
        m_xy_str_opt[STRING_NAME]      , m_xy_str_opt[STRING_VERSION],
        m_xy_str_opt[STRING_YUV_MATRIX], m_xy_bool_opt[BOOL_COMBINE_BITMAPS]?L"True":L"False");
    return strTemp;
}

CStringW XySubFilter::DumpConsumerInfo()
{
    CStringW strTemp;
    strTemp.Format(L"name:'%ls' version:'%ls' yuvMatrix:'%ls'",
        m_xy_str_opt[STRING_CONNECTED_CONSUMER], m_xy_str_opt[STRING_CONSUMER_VERSION],
        m_xy_str_opt[STRING_CONSUMER_YUV_MATRIX]);
    return strTemp;
}

//
// DummyExternalSubtitleOutputPin
//

DummyExternalSubtitleOutputPin::DummyExternalSubtitleOutputPin( ExternalSubtitleDetector* pFilter, HRESULT* phr )
    : CSourceStream(NAME("DummyExternalSubtitleOutputPin"), phr, pFilter, L"DummyOutput")
{

}

HRESULT DummyExternalSubtitleOutputPin::CheckMediaType( const CMediaType* pmt )
{
    XY_LOG_DEBUG(XY_LOG_VAR_2_STR(pmt));
    return __super::CheckMediaType(pmt);
}

HRESULT DummyExternalSubtitleOutputPin::DecideBufferSize( IMemAllocator * pAlloc, ALLOCATOR_PROPERTIES *pProp )
{
    XY_LOG_DEBUG("Align:"<<pProp->cbAlign<<" Buffer:"<<pProp->cbBuffer<<" Prefix:"<<pProp->cbPrefix<<" Buffers:"<<pProp->cBuffers);
    ALLOCATOR_PROPERTIES Actual;

    pProp->cBuffers = pProp->cBuffers > 0 ? pProp->cBuffers : 1;
    pProp->cbBuffer = pProp->cbBuffer > 0 ? pProp->cbBuffer : 1;
    pProp->cbAlign  = 1;
    pProp->cbPrefix = 0;

    HRESULT hr = pAlloc->SetProperties(pProp,&Actual);
    XY_LOG_DEBUG("Align:"<<Actual.cbAlign<<" Buffer:"<<Actual.cbBuffer<<" Prefix:"<<Actual.cbPrefix<<" Buffers:"<<Actual.cBuffers);
    return hr;
    //return S_FALSE;
}

HRESULT DummyExternalSubtitleOutputPin::GetMediaType( CMediaType *pMediaType )
{
    pMediaType->majortype  = MEDIATYPE_Subtitle;
    pMediaType->subtype    = MEDIASUBTYPE_EXTERNAL_SUB_DUMMY;
    pMediaType->formattype = GUID_NULL;
    return S_OK;
}

HRESULT DummyExternalSubtitleOutputPin::FillBuffer( IMediaSample *pSamp )
{
    return S_FALSE;
}

//
//  ExternalSubtitleDetector
//

ExternalSubtitleDetector::ExternalSubtitleDetector( LPUNKNOWN punk, HRESULT* phr
    , const GUID& clsid /*= __uuidof(ExternalSubtitleDetector)*/ )
    : CSource(NAME("ExternalSubtitleDetector"), punk, clsid)
    , m_external_sub_found(false)
{
    m_output_pin = DEBUG_NEW DummyExternalSubtitleOutputPin(this, phr);
}

ExternalSubtitleDetector::~ExternalSubtitleDetector()
{
    XY_LOG_DEBUG(_T(""));
}

CBasePin* ExternalSubtitleDetector::GetPin( int n )
{
    if (n == 0 && m_external_sub_found)
        return __super::GetPin(0);

    return NULL;
}

int ExternalSubtitleDetector::GetPinCount()
{
    return m_external_sub_found ? 1 : 0;
}

STDMETHODIMP ExternalSubtitleDetector::JoinFilterGraph( IFilterGraph* pGraph, LPCWSTR pName )
{
    XY_LOG_INFO(XY_LOG_VAR_2_STR(this)<<"pGraph:"<<(void*)pGraph);
    HRESULT hr = NOERROR;
    if(pGraph)
    {
        CComPtr<IBaseFilter> xy_sub_filter;
        BeginEnumFilters(pGraph, pEF, pBF)
        {
            if (pBF != (IBaseFilter*)this)
            {
                CLSID clsid;
                pBF->GetClassID(&clsid);
                if (clsid==__uuidof(ExternalSubtitleDetector))
                {
                    XY_LOG_INFO("Another ExternalSubtitleDetector filter has been added to the graph.");
                    return E_FAIL;
                }
                if (clsid==__uuidof(XySubFilter))
                {
                    xy_sub_filter = pBF;
                }
            }
        }
        EndEnumFilters;
        hr = __super::JoinFilterGraph(pGraph, pName);
        if (hr!=NOERROR)
        {
            return hr;
        }
        m_external_sub_found = ExternalSubtitleExists(pGraph);
        if (m_external_sub_found)
        {
            if (!xy_sub_filter)
            {
                hr = xy_sub_filter.CoCreateInstance(__uuidof(XySubFilter));
                if (FAILED(hr))
                {
                    XY_LOG_ERROR("Failed to create XySubFilter."<<XY_LOG_VAR_2_STR(hr));
                    return S_OK;
                }
                hr = pGraph->AddFilter(xy_sub_filter, L"XySubFilter(AutoLoad)");
                if (FAILED(hr))
                {
                    XY_LOG_ERROR("Failed to AddFilter. "<<XY_LOG_VAR_2_STR(xy_sub_filter)<<XY_LOG_VAR_2_STR(hr));
                    return S_OK;
                }
            }
            XySubFilter * tmp = dynamic_cast<XySubFilter*>( (IBaseFilter*)xy_sub_filter );
            int n = tmp->GetPinCount();
            IPin *pIn = tmp->GetPin(n-1);
            ASSERT(pIn);
            CMediaType mt;
            ASSERT( SUCCEEDED(m_output_pin->GetMediaType(&mt)) );
            hr = pGraph->ConnectDirect(m_output_pin, pIn, &mt);
            if (FAILED(hr))
            {
                XY_LOG_ERROR("Failed to connect xy_sub_filter!"<<XY_LOG_VAR_2_STR(hr));
                return S_OK;
            }
        }
    }
    else
    {
        m_external_sub_found = false;
        hr = __super::JoinFilterGraph(pGraph, pName);
    }

    return hr;
}

STDMETHODIMP ExternalSubtitleDetector::QueryFilterInfo( FILTER_INFO* pInfo )
{
    CheckPointer(pInfo, E_POINTER);
    ValidateReadWritePtr(pInfo, sizeof(FILTER_INFO));

    HRESULT hr = __super::QueryFilterInfo(pInfo);
    if (SUCCEEDED(hr))
    {
        wcscpy_s(pInfo->achName, countof(pInfo->achName)-1, L"ExternalSubtitleDetector");
    }
    return hr;
}

bool ExternalSubtitleDetector::ExternalSubtitleExists( IFilterGraph* pGraph )
{
    XY_LOG_INFO(_T(""));

    bool fRet = false;
    HRESULT hr = NOERROR;

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
    CAtlArray<CString> paths;

    for(int i = 0; i < 10; i++)
    {
        CString tmp;
        tmp.Format(IDS_RP_PATH, i);
        CString path = theApp.GetProfileString(ResStr(IDS_R_DEFTEXTPATHES), tmp);
        if(!path.IsEmpty()) paths.Add(path);
    }
    CString load_ext_list = theApp.GetProfileString(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_LOAD_EXT_LIST), _T("ass;ssa;srt;sub;idx;sup;txt;usf;xss;ssf;smi;psb;rt"));

    CAtlArray<SubFile> ret;
    GetSubFileNames(WToT(fn), paths, load_ext_list, ret);

    return ret.GetCount()>0;
}
