#include "stdafx.h"
#include "xy_sub_filter.h"
#include "DirectVobSubFilter.h"
#include "VSFilter.h"
#include "DirectVobSubPropPage.h"
#include "TextInputPin.h"

#include "../../../subpic/color_conv_table.h"

#include "CAutoTiming.h"
#include "xy_logger.h"

#include "..\..\..\..\include\moreuuids.h"

using namespace DirectVobSubXyOptions;

////////////////////////////////////////////////////////////////////////////
//
// Constructor
//

XySubFilter::XySubFilter( CDirectVobSubFilter *p_dvs, LPUNKNOWN punk )
    : CUnknown( NAME("XySubFilter"), punk )
    , m_nSubtitleId(-1)
    , m_dvs(p_dvs)
{
    m_script_selected_yuv = CSimpleTextSubtitle::YCbCrMatrix_AUTO;
    m_script_selected_range = CSimpleTextSubtitle::YCbCrRange_AUTO;

    HRESULT hr = S_OK;
    m_pTextInput.Add(new CTextInputPin(m_dvs, m_dvs->m_pLock, &m_csSubLock, &hr));
    ASSERT(SUCCEEDED(hr));

    CAMThread::Create();
    m_frd.EndThreadEvent.Create(0, FALSE, FALSE, 0);
    m_frd.RefreshEvent.Create(0, FALSE, FALSE, 0);
}

XySubFilter::~XySubFilter()
{
    m_frd.EndThreadEvent.Set();
    CAMThread::Close();

    for(unsigned i = 0; i < m_pTextInput.GetCount(); i++)
        delete m_pTextInput[i];

    if(m_simple_provider)
    {
        DbgLog((LOG_TRACE, 3, "~XySubFilter::Invalidate"));
        m_simple_provider->Invalidate();
    }
    m_simple_provider = NULL;
}

STDMETHODIMP XySubFilter::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
    CheckPointer(ppv, E_POINTER);

    return
        QI(IDirectVobSub)
        QI(IDirectVobSub2)
        QI(IDirectVobSubXy)
        QI(IFilterVersion)
        QI(ISpecifyPropertyPages)
		QI(IAMStreamSelect)
		__super::NonDelegatingQueryInterface(riid, ppv);
}

//
// IDirectVobSubXy
//
STDMETHODIMP XySubFilter::XySetBool( int field, bool value )
{
    CAutoLock cAutolock(&m_dvs->m_csQueueLock);
    HRESULT hr = CDirectVobSub::XySetBool(field, value);
    if(hr != NOERROR)
    {
        return hr;
    }
    switch(field)
    {
    case DirectVobSubXyOptions::BOOL_FOLLOW_UPSTREAM_PREFERRED_ORDER:
        m_dvs->m_donot_follow_upstream_preferred_order = !m_xy_bool_opt[BOOL_FOLLOW_UPSTREAM_PREFERRED_ORDER];
        break;
    default:
        hr = E_NOTIMPL;
        break;
    }
    return hr;
}

STDMETHODIMP XySubFilter::XySetInt( int field, int value )
{
    CAutoLock cAutolock(&m_dvs->m_csQueueLock);
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
    case DirectVobSubXyOptions::INT_SUBPIXEL_POS_LEVEL:
        SubpixelPositionControler::GetGlobalControler().SetSubpixelLevel( static_cast<SubpixelPositionControler::SUBPIXEL_LEVEL>(m_xy_int_opt[field]) );
        break;
    default:
        hr = E_NOTIMPL;
        break;
    }

    return hr;
}

//
// IDirectVobSub
//
STDMETHODIMP XySubFilter::put_FileName(WCHAR* fn)
{
    AMTRACE((TEXT(__FUNCTION__),0));
    HRESULT hr = CDirectVobSub::put_FileName(fn);

    if(hr == S_OK && !Open())
    {
        m_FileName.Empty();
        hr = E_FAIL;
    }

    return hr;
}

STDMETHODIMP XySubFilter::get_LanguageCount(int* nLangs)
{
    HRESULT hr = CDirectVobSub::get_LanguageCount(nLangs);

    if(hr == NOERROR && nLangs)
    {
        CAutoLock cAutolock(&m_dvs->m_csQueueLock);

        *nLangs = 0;
        POSITION pos = m_pSubStreams.GetHeadPosition();
        while(pos) (*nLangs) += m_pSubStreams.GetNext(pos)->GetStreamCount();
    }

    return hr;
}

STDMETHODIMP XySubFilter::get_LanguageName(int iLanguage, WCHAR** ppName)
{
    HRESULT hr = CDirectVobSub::get_LanguageName(iLanguage, ppName);

    if(!ppName) return E_POINTER;

    if(hr == NOERROR)
    {
        CAutoLock cAutolock(&m_dvs->m_csQueueLock);

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

STDMETHODIMP XySubFilter::put_SelectedLanguage(int iSelected)
{
    HRESULT hr = CDirectVobSub::put_SelectedLanguage(iSelected);

    if(hr == NOERROR)
    {
        UpdateSubtitle(false);
    }

    return hr;
}

STDMETHODIMP XySubFilter::put_HideSubtitles(bool fHideSubtitles)
{
    HRESULT hr = CDirectVobSub::put_HideSubtitles(fHideSubtitles);

    if(hr == NOERROR)
    {
        UpdateSubtitle(false);
    }

    return hr;
}

STDMETHODIMP XySubFilter::put_PreBuffering(bool fDoPreBuffering)
{
    HRESULT hr = CDirectVobSub::put_PreBuffering(fDoPreBuffering);

    if(hr == NOERROR)
    {
        DbgLog((LOG_TRACE, 3, "put_PreBuffering => InitSubPicQueue"));
        m_dvs->InitSubPicQueue();
    }

    return hr;
}

STDMETHODIMP XySubFilter::put_Placement(bool fOverridePlacement, int xperc, int yperc)
{
    DbgLog((LOG_TRACE, 3, "%s(%d) %s", __FILE__, __LINE__, __FUNCTION__));
    HRESULT hr = CDirectVobSub::put_Placement(fOverridePlacement, xperc, yperc);

    if(hr == NOERROR)
    {
        //DbgLog((LOG_TRACE, 3, "%d %s:UpdateSubtitle(true)", __LINE__, __FUNCTION__));
        //UpdateSubtitle(true);
        UpdateSubtitle(false);
    }

    return hr;
}

STDMETHODIMP XySubFilter::put_VobSubSettings(bool fBuffer, bool fOnlyShowForcedSubs, bool fReserved)
{
    HRESULT hr = CDirectVobSub::put_VobSubSettings(fBuffer, fOnlyShowForcedSubs, fReserved);

    if(hr == NOERROR)
    {
        InvalidateSubtitle();
    }

    return hr;
}

STDMETHODIMP XySubFilter::put_TextSettings(void* lf, int lflen, COLORREF color, bool fShadow, bool fOutline, bool fAdvancedRenderer)
{
    HRESULT hr = CDirectVobSub::put_TextSettings(lf, lflen, color, fShadow, fOutline, fAdvancedRenderer);

    if(hr == NOERROR)
    {
        InvalidateSubtitle();
    }

    return hr;
}

STDMETHODIMP XySubFilter::put_SubtitleTiming(int delay, int speedmul, int speeddiv)
{
    HRESULT hr = CDirectVobSub::put_SubtitleTiming(delay, speedmul, speeddiv);

    if(hr == NOERROR)
    {
        InvalidateSubtitle();
    }

    return hr;
}

STDMETHODIMP XySubFilter::get_CachesInfo(CachesInfo* caches_info)
{
    CAutoLock cAutoLock(&m_dvs->m_csQueueLock);
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
    CAutoLock cAutoLock(&m_dvs->m_csQueueLock);
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
    HRESULT hr = CDirectVobSub::get_MediaFPS(fEnabled, fps);

    CComQIPtr<IMediaSeeking> pMS = m_dvs->m_pGraph;
    double rate;
    if(pMS && SUCCEEDED(pMS->GetRate(&rate)))
    {
        m_MediaFPS = rate * m_dvs->m_fps;
        if(fps) *fps = m_MediaFPS;
    }

    return hr;
}

STDMETHODIMP XySubFilter::put_MediaFPS(bool fEnabled, double fps)
{
    HRESULT hr = CDirectVobSub::put_MediaFPS(fEnabled, fps);

    CComQIPtr<IMediaSeeking> pMS = m_dvs->m_pGraph;
    if(pMS)
    {
        if(hr == NOERROR)
        {
            hr = pMS->SetRate(m_fMediaFPSEnabled ? m_MediaFPS / m_dvs->m_fps : 1.0);
        }

        double dRate;
        if(SUCCEEDED(pMS->GetRate(&dRate)))
            m_MediaFPS = dRate * m_dvs->m_fps;
    }

    return hr;
}

STDMETHODIMP XySubFilter::get_ZoomRect(NORMALIZEDRECT* rect)
{
    return E_NOTIMPL;
}

STDMETHODIMP XySubFilter::put_ZoomRect(NORMALIZEDRECT* rect)
{
    return E_NOTIMPL;
}

STDMETHODIMP XySubFilter::HasConfigDialog(int iSelected)
{
    int nLangs;
    if(FAILED(get_LanguageCount(&nLangs))) return E_FAIL;
    return E_FAIL;
    // TODO: temporally disabled since we don't have a new textsub/vobsub editor dlg for dvs yet
    // return(nLangs >= 0 && iSelected < nLangs ? S_OK : E_FAIL);
}

STDMETHODIMP XySubFilter::ShowConfigDialog(int iSelected, HWND hWndParent)
{
    // TODO: temporally disabled since we don't have a new textsub/vobsub editor dlg for dvs yet
    return(E_FAIL);
}

//
// IDirectVobSub2
//
STDMETHODIMP XySubFilter::put_TextSettings(STSStyle* pDefStyle)
{
    HRESULT hr = CDirectVobSub::put_TextSettings(pDefStyle);

    if(hr == NOERROR)
    {
        UpdateSubtitle(false);
    }

    return hr;
}

STDMETHODIMP XySubFilter::put_AspectRatioSettings(CSimpleTextSubtitle::EPARCompensationType* ePARCompensationType)
{
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
    CheckPointer(pPages, E_POINTER);

    pPages->cElems = 8;
    pPages->pElems = (GUID*)CoTaskMemAlloc(sizeof(GUID)*pPages->cElems);

    if(pPages->pElems == NULL) return E_OUTOFMEMORY;

    int i = 0;
    pPages->pElems[i++] = __uuidof(CDVSMainPPage);
    pPages->pElems[i++] = __uuidof(CDVSGeneralPPage);
    pPages->pElems[i++] = __uuidof(CDVSMiscPPage);
    pPages->pElems[i++] = __uuidof(CDVSMorePPage);
    pPages->pElems[i++] = __uuidof(CDVSTimingPPage);
    pPages->pElems[i++] = __uuidof(CDVSColorPPage);
    pPages->pElems[i++] = __uuidof(CDVSPathsPPage);
    pPages->pElems[i++] = __uuidof(CDVSAboutPPage);

    return NOERROR;
}

//
// IAMStreamSelect
//
STDMETHODIMP XySubFilter::Count(DWORD* pcStreams)
{
	if(!pcStreams) return E_POINTER;

	*pcStreams = 0;

	int nLangs = 0;
	if(SUCCEEDED(get_LanguageCount(&nLangs)))
		(*pcStreams) += nLangs;

	(*pcStreams) += 2; // enable ... disable

	(*pcStreams) += 2; // normal flipped

	return S_OK;
}

STDMETHODIMP XySubFilter::Enable(long lIndex, DWORD dwFlags)
{
	if(!(dwFlags & AMSTREAMSELECTENABLE_ENABLE))
		return E_NOTIMPL;

	int nLangs = 0;
	get_LanguageCount(&nLangs);

	if(!(lIndex >= 0 && lIndex < nLangs+2+2))
		return E_INVALIDARG;

	int i = lIndex-1;

	if(i == -1 && !m_dvs->m_fLoading) // we need this because when loading something stupid media player pushes the first stream it founds, which is "enable" in our case
	{
		put_HideSubtitles(false);
	}
	else if(i >= 0 && i < nLangs)
	{
		put_HideSubtitles(false);
		put_SelectedLanguage(i);

		WCHAR* pName = NULL;
		if(SUCCEEDED(get_LanguageName(i, &pName)))
		{
			UpdatePreferedLanguages(CString(pName));
			if(pName) CoTaskMemFree(pName);
		}
	}
	else if(i == nLangs && !m_dvs->m_fLoading)
	{
		put_HideSubtitles(true);
	}
	else if((i == nLangs+1 || i == nLangs+2) && !m_dvs->m_fLoading)
	{
		put_Flip(i == nLangs+2, m_fFlipSubtitles);
	}

	return S_OK;
}

STDMETHODIMP XySubFilter::Info(long lIndex, AM_MEDIA_TYPE** ppmt, DWORD* pdwFlags, LCID* plcid, DWORD* pdwGroup, WCHAR** ppszName, IUnknown** ppObject, IUnknown** ppUnk)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
    const int FLAG_CMD = 1;
    const int FLAG_EXTERNAL_SUB = 2;
    const int FLAG_PICTURE_CMD = 4;
    const int FLAG_VISIBILITY_CMD = 8;
    
    const int GROUP_NUM_BASE = 0x648E40;//random number

	int nLangs = 0;
	get_LanguageCount(&nLangs);

	if(!(lIndex >= 0 && lIndex < nLangs+2+2))
		return E_INVALIDARG;

	int i = lIndex-1;

	if(ppmt) *ppmt = CreateMediaType(&m_dvs->m_pInput->CurrentMediaType());

	if(pdwFlags)
	{
		*pdwFlags = 0;

		if(i == -1 && !m_fHideSubtitles
		|| i >= 0 && i < nLangs && i == m_iSelectedLanguage
		|| i == nLangs && m_fHideSubtitles
		|| i == nLangs+1 && !m_fFlipPicture
		|| i == nLangs+2 && m_fFlipPicture)
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
            if( SUCCEEDED(GetIsEmbeddedSubStream(i, &isEmbedded)) )
            {
                if(isEmbedded)
                {
                    *pdwGroup = GROUP_NUM_BASE & ~(FLAG_CMD | FLAG_EXTERNAL_SUB);
                }
                else
                {
                    *pdwGroup = (GROUP_NUM_BASE & ~FLAG_CMD) | FLAG_EXTERNAL_SUB;
                }
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
            get_LanguageName(i, ppszName);
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
			wcscpy(*ppszName, str);
		}
	}

	if(ppObject) *ppObject = NULL;

	if(ppUnk) *ppUnk = NULL;

	return S_OK;
}

//
// FRD
// 
void XySubFilter::SetupFRD(CStringArray& paths, CAtlArray<HANDLE>& handles)
{
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
    SetThreadPriority(m_hThread, THREAD_PRIORITY_LOWEST/*THREAD_PRIORITY_BELOW_NORMAL*/);

    CStringArray paths;
    CAtlArray<HANDLE> handles;

    SetupFRD(paths, handles);

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
            m_dvs->IsSubtitleReloaderLocked(&fLocked);
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
                        if(FILE* f = _tfopen(fn, _T("rb+")))
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
// XySubFilter
// 
void XySubFilter::SetYuvMatrix()
{
    ColorConvTable::YuvMatrixType yuv_matrix = ColorConvTable::BT601;
    ColorConvTable::YuvRangeType yuv_range = ColorConvTable::RANGE_TV;

    if ( m_xy_int_opt[INT_COLOR_SPACE]==CDirectVobSub::YuvMatrix_AUTO )
    {
        switch(m_script_selected_yuv)
        {
        case CSimpleTextSubtitle::YCbCrMatrix_BT601:
            yuv_matrix = ColorConvTable::BT601;
            break;
        case CSimpleTextSubtitle::YCbCrMatrix_BT709:
            yuv_matrix = ColorConvTable::BT709;
            break;
        case CSimpleTextSubtitle::YCbCrMatrix_AUTO:
        default:        
            yuv_matrix = ColorConvTable::BT601;
            break;
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
            yuv_matrix = (m_dvs->m_w > m_bt601Width || m_dvs->m_h > m_bt601Height) ? ColorConvTable::BT709 : ColorConvTable::BT601;
            break;
        }
    }

    if( m_xy_int_opt[INT_YUV_RANGE]==CDirectVobSub::YuvRange_Auto )
    {
        switch(m_script_selected_range)
        {
        case CSimpleTextSubtitle::YCbCrRange_PC:
            yuv_range = ColorConvTable::RANGE_PC;
            break;
        case CSimpleTextSubtitle::YCbCrRange_TV:
            yuv_range = ColorConvTable::RANGE_TV;
            break;
        case CSimpleTextSubtitle::YCbCrRange_AUTO:
        default:        
            yuv_range = ColorConvTable::RANGE_TV;
            break;
        }
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
}

bool XySubFilter::Open()
{
    AMTRACE((TEXT(__FUNCTION__),0));
    XY_AUTO_TIMING(TEXT("XySubFilter::Open"));

    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    CAutoLock cAutolock(&m_dvs->m_csQueueLock);

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
    GetSubFileNames(m_FileName, paths, ret);

    for(unsigned i = 0; i < ret.GetCount(); i++)
    {
        if(m_frd.files.Find(ret[i].fn))
            continue;

        CComPtr<ISubStream> pSubStream;

        if(!pSubStream)
        {
            //            CAutoTiming t(TEXT("CRenderedTextSubtitle::Open"), 0);
            XY_AUTO_TIMING(TEXT("CRenderedTextSubtitle::Open"));
            CAutoPtr<CRenderedTextSubtitle> pRTS(new CRenderedTextSubtitle(&m_csSubLock));
            if(pRTS && pRTS->Open(ret[i].fn, DEFAULT_CHARSET) && pRTS->GetStreamCount() > 0)
            {
                pSubStream = pRTS.Detach();
                m_frd.files.AddTail(ret[i].fn + _T(".style"));
            }
        }

        if(!pSubStream)
        {
            CAutoTiming t(TEXT("CVobSubFile::Open"), 0);
            CAutoPtr<CVobSubFile> pVSF(new CVobSubFile(&m_csSubLock));
            if(pVSF && pVSF->Open(ret[i].fn) && pVSF->GetStreamCount() > 0)
            {
                pSubStream = pVSF.Detach();
                m_frd.files.AddTail(ret[i].fn.Left(ret[i].fn.GetLength()-4) + _T(".sub"));
            }
        }

        if(!pSubStream)
        {
            CAutoTiming t(TEXT("ssf::CRenderer::Open"), 0);
            CAutoPtr<ssf::CRenderer> pSSF(new ssf::CRenderer(&m_csSubLock));
            if(pSSF && pSSF->Open(ret[i].fn) && pSSF->GetStreamCount() > 0)
            {
                pSubStream = pSSF.Detach();
            }
        }

        if(pSubStream)
        {
            m_pSubStreams.AddTail(pSubStream);
            m_fIsSubStreamEmbeded.AddTail(false);
            m_frd.files.AddTail(ret[i].fn);
        }
    }

    for(unsigned i = 0; i < m_pTextInput.GetCount(); i++)
    {
        if(m_pTextInput[i]->IsConnected())
        {
            m_pSubStreams.AddTail(m_pTextInput[i]->GetSubStream());
            m_fIsSubStreamEmbeded.AddTail(true);
        }
    }

    if(S_FALSE == put_SelectedLanguage(FindPreferedLanguage()))
        UpdateSubtitle(false); // make sure pSubPicProvider of our queue gets updated even if the stream number hasn't changed

    m_frd.RefreshEvent.Set();

    return(m_pSubStreams.GetCount() > 0);
}

void XySubFilter::UpdateSubtitle(bool fApplyDefStyle/*= true*/)
{
    CAutoLock cAutolock(&m_dvs->m_csQueueLock);

    if(!m_simple_provider) return;

    InvalidateSubtitle();

    CComPtr<ISubStream> pSubStream;

    if(!m_fHideSubtitles)
    {
        int i = m_iSelectedLanguage;

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
}

void XySubFilter::SetSubtitle( ISubStream* pSubStream, bool fApplyDefStyle /*= true*/ )
{
    DbgLog((LOG_TRACE, 3, "%s(%d): %s", __FILE__, __LINE__, __FUNCTION__));
    DbgLog((LOG_TRACE, 3, "\tpSubStream:%x fApplyDefStyle:%d", pSubStream, (int)fApplyDefStyle));
    CAutoLock cAutolock(&m_dvs->m_csQueueLock);

    CSize playres(0,0);
    m_script_selected_yuv = CSimpleTextSubtitle::YCbCrMatrix_AUTO;
    m_script_selected_range = CSimpleTextSubtitle::YCbCrRange_AUTO;
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
                pVSS->SetAlignment(m_fOverridePlacement, m_PlacementXperc, m_PlacementYperc, 1, 1);
                pVSS->m_fOnlyShowForcedSubs = m_fOnlyShowForcedVobSubs;
            }
        }
        else if(clsid == __uuidof(CVobSubStream))
        {
            CVobSubSettings* pVSS = dynamic_cast<CVobSubStream*>(pSubStream);

            if(fApplyDefStyle)
            {
                pVSS->SetAlignment(m_fOverridePlacement, m_PlacementXperc, m_PlacementYperc, 1, 1);
                pVSS->m_fOnlyShowForcedSubs = m_fOnlyShowForcedVobSubs;
            }
        }
        else if(clsid == __uuidof(CRenderedTextSubtitle))
        {
            CRenderedTextSubtitle* pRTS = dynamic_cast<CRenderedTextSubtitle*>(pSubStream);

            if(fApplyDefStyle || pRTS->m_fUsingAutoGeneratedDefaultStyle)
            {
                STSStyle s = m_defStyle;

                if(m_fOverridePlacement)
                {
                    s.scrAlignment = 2;
                    int w = pRTS->m_dstScreenSize.cx;
                    int h = pRTS->m_dstScreenSize.cy;
                    CRect tmp_rect = s.marginRect.get();
                    int mw = w - tmp_rect.left - tmp_rect.right;
                    tmp_rect.bottom = h - MulDiv(h, m_PlacementYperc, 100);
                    tmp_rect.left = MulDiv(w, m_PlacementXperc, 100) - mw/2;
                    tmp_rect.right = w - (tmp_rect.left + mw);
                    s.marginRect = tmp_rect;
                }

                pRTS->SetDefaultStyle(s);
            }

            pRTS->m_ePARCompensationType = m_ePARCompensationType;
            if (m_dvs->m_CurrentVIH2.dwPictAspectRatioX != 0 && m_dvs->m_CurrentVIH2.dwPictAspectRatioY != 0&& m_dvs->m_CurrentVIH2.bmiHeader.biWidth != 0 && m_dvs->m_CurrentVIH2.bmiHeader.biHeight != 0)
            {
                pRTS->m_dPARCompensation = ((double)abs(m_dvs->m_CurrentVIH2.bmiHeader.biWidth) / (double)abs(m_dvs->m_CurrentVIH2.bmiHeader.biHeight)) /
                    ((double)abs((long)m_dvs->m_CurrentVIH2.dwPictAspectRatioX) / (double)abs((long)m_dvs->m_CurrentVIH2.dwPictAspectRatioY));

            }
            else
            {
                pRTS->m_dPARCompensation = 1.00;
            }

            m_script_selected_yuv = pRTS->m_eYCbCrMatrix;
            m_script_selected_range = pRTS->m_eYCbCrRange;
            pRTS->Deinit();
            playres = pRTS->m_dstScreenSize;
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
                m_iSelectedLanguage = i + pSubStream2->GetStream();
                break;
            }

            i += pSubStream2->GetStreamCount();
        }
    }

    m_nSubtitleId = reinterpret_cast<DWORD_PTR>(pSubStream);

    SetYuvMatrix();

    XySetSize(SIZE_ASS_PLAY_RESOLUTION, playres);
    if(m_simple_provider)
        m_simple_provider->SetSubPicProvider(CComQIPtr<ISubPicProviderEx>(pSubStream));
}

void XySubFilter::InvalidateSubtitle( REFERENCE_TIME rtInvalidate /*= -1*/, DWORD_PTR nSubtitleId /*= -1*/ )
{
    CAutoLock cAutolock(&m_dvs->m_csQueueLock);

    if(m_simple_provider)
    {
        if(nSubtitleId == -1 || nSubtitleId == m_nSubtitleId)
        {
            DbgLog((LOG_TRACE, 3, "InvalidateSubtitle::Invalidate"));
            m_simple_provider->Invalidate(rtInvalidate);
        }
    }
}

#define MAXPREFLANGS 5

int XySubFilter::FindPreferedLanguage( bool fHideToo /*= true*/ )
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    int nLangs;
    get_LanguageCount(&nLangs);

    if(nLangs <= 0) return(0);

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
                get_LanguageName(ret, &pName);
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
    CAutoLock cAutoLock(&m_dvs->m_csQueueLock);

    POSITION pos = m_pSubStreams.Find(pSubStream);
    if(!pos)
    {
        m_pSubStreams.AddTail(pSubStream);
        m_fIsSubStreamEmbeded.AddTail(true);//todo: fix me
    }

    int len = m_pTextInput.GetCount();
    for(unsigned i = 0; i < m_pTextInput.GetCount(); i++)
        if(m_pTextInput[i]->IsConnected()) len--;

    if(len == 0)
    {
        HRESULT hr = S_OK;
        m_pTextInput.Add(new CTextInputPin(m_dvs, m_dvs->m_pLock, &m_csSubLock, &hr));
    }
}

void XySubFilter::RemoveSubStream(ISubStream* pSubStream)
{
    CAutoLock cAutoLock(&m_dvs->m_csQueueLock);

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
}

HRESULT XySubFilter::GetIsEmbeddedSubStream( int iSelected, bool *fIsEmbedded )
{
    CAutoLock cAutolock(&m_dvs->m_csQueueLock);

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

bool XySubFilter::ShouldWeAutoload(IFilterGraph* pGraph)
{
    XY_AUTO_TIMING(_T("XySubFilter::ShouldWeAutoload"));
    TCHAR blacklistedapps[][32] =
    {
        _T("WM8EUTIL."), // wmp8 encoder's dummy renderer releases the outputted media sample after calling Receive on its input pin (yes, even when dvobsub isn't registered at all)
        _T("explorer."), // as some users reported thumbnail preview loads dvobsub, I've never experienced this yet...
        _T("producer."), // this is real's producer
        _T("GoogleDesktopIndex."), // Google Desktop
        _T("GoogleDesktopDisplay."), // Google Desktop
        _T("GoogleDesktopCrawl."), // Google Desktop
    };

    for(int i = 0; i < countof(blacklistedapps); i++)
    {
        if(theApp.m_AppName.Find(blacklistedapps[i]) >= 0)
            return(false);
    }

    int level;
    bool m_fExternalLoad, m_fWebLoad, m_fEmbeddedLoad;
    get_LoadSettings(&level, &m_fExternalLoad, &m_fWebLoad, &m_fEmbeddedLoad);

    if(level < 0 || level >= 2) return(false);

    bool fRet = false;

    if(level == 1)
        fRet = m_fExternalLoad = m_fWebLoad = m_fEmbeddedLoad = true;

    // find text stream on known splitters

    if(!fRet && m_fEmbeddedLoad)
    {
        CComPtr<IBaseFilter> pBF;
        if((pBF = FindFilter(CLSID_OggSplitter, pGraph)) || (pBF = FindFilter(CLSID_AviSplitter, pGraph))
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
            ) 
        {
            BeginEnumPins(pBF, pEP, pPin)
            {
                BeginEnumMediaTypes(pPin, pEM, pmt)
                {
                    if(pmt->majortype == MEDIATYPE_Text || pmt->majortype == MEDIATYPE_Subtitle)
                    {
                        fRet = true;
                        break;
                    }
                }
                EndEnumMediaTypes(pmt)
                    if(fRet) break;
            }
            EndEnumFilters
        }
    }

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
    EndEnumFilters

        if((m_fExternalLoad || m_fWebLoad) && (m_fWebLoad || !(wcsstr(fn, L"http://") || wcsstr(fn, L"mms://"))))
        {
            bool fTemp = m_fHideSubtitles;
            fRet = !fn.IsEmpty() && SUCCEEDED(put_FileName((LPWSTR)(LPCWSTR)fn))
                || SUCCEEDED(put_FileName(L"c:\\tmp.srt"))
                || fRet;
            if(fTemp) m_fHideSubtitles = true;
        }

        return(fRet);
}
