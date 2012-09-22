#include "stdafx.h"
#include "xy_sub_filter.h"
#include "DirectVobSubFilter.h"
#include "VSFilter.h"
#include "DirectVobSubPropPage.h"

#include "../../../subpic/color_conv_table.h"

using namespace DirectVobSubXyOptions;

////////////////////////////////////////////////////////////////////////////
//
// Constructor
//

XySubFilter::XySubFilter( CDirectVobSubFilter *p_dvs, LPUNKNOWN punk )
    : CUnknown( NAME("XySubFilter"), punk )
    , m_dvs(p_dvs)
{
    m_script_selected_yuv = CSimpleTextSubtitle::YCbCrMatrix_AUTO;
    m_script_selected_range = CSimpleTextSubtitle::YCbCrRange_AUTO;

    CAMThread::Create();
    m_frd.EndThreadEvent.Create(0, FALSE, FALSE, 0);
    m_frd.RefreshEvent.Create(0, FALSE, FALSE, 0);
}

XySubFilter::~XySubFilter()
{
    m_frd.EndThreadEvent.Set();
    CAMThread::Close();
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

    if(hr == S_OK && !m_dvs->Open())
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
        POSITION pos = m_dvs->m_pSubStreams.GetHeadPosition();
        while(pos) (*nLangs) += m_dvs->m_pSubStreams.GetNext(pos)->GetStreamCount();
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

        POSITION pos = m_dvs->m_pSubStreams.GetHeadPosition();
        while(i >= 0 && pos)
        {
            CComPtr<ISubStream> pSubStream = m_dvs->m_pSubStreams.GetNext(pos);

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
        m_dvs->UpdateSubtitle(false);
    }

    return hr;
}

STDMETHODIMP XySubFilter::put_HideSubtitles(bool fHideSubtitles)
{
    HRESULT hr = CDirectVobSub::put_HideSubtitles(fHideSubtitles);

    if(hr == NOERROR)
    {
        m_dvs->UpdateSubtitle(false);
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
        //DbgLog((LOG_TRACE, 3, "%d %s:m_dvs->UpdateSubtitle(true)", __LINE__, __FUNCTION__));
        //m_dvs->UpdateSubtitle(true);
        m_dvs->UpdateSubtitle(false);
    }

    return hr;
}

STDMETHODIMP XySubFilter::put_VobSubSettings(bool fBuffer, bool fOnlyShowForcedSubs, bool fReserved)
{
    HRESULT hr = CDirectVobSub::put_VobSubSettings(fBuffer, fOnlyShowForcedSubs, fReserved);

    if(hr == NOERROR)
    {
        m_dvs->InvalidateSubtitle();
    }

    return hr;
}

STDMETHODIMP XySubFilter::put_TextSettings(void* lf, int lflen, COLORREF color, bool fShadow, bool fOutline, bool fAdvancedRenderer)
{
    HRESULT hr = CDirectVobSub::put_TextSettings(lf, lflen, color, fShadow, fOutline, fAdvancedRenderer);

    if(hr == NOERROR)
    {
        m_dvs->InvalidateSubtitle();
    }

    return hr;
}

STDMETHODIMP XySubFilter::put_SubtitleTiming(int delay, int speedmul, int speeddiv)
{
    HRESULT hr = CDirectVobSub::put_SubtitleTiming(delay, speedmul, speeddiv);

    if(hr == NOERROR)
    {
        m_dvs->InvalidateSubtitle();
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
        m_dvs->UpdateSubtitle(false);
    }

    return hr;
}

STDMETHODIMP XySubFilter::put_AspectRatioSettings(CSimpleTextSubtitle::EPARCompensationType* ePARCompensationType)
{
    HRESULT hr = CDirectVobSub::put_AspectRatioSettings(ePARCompensationType);

    if(hr == NOERROR)
    {
        //DbgLog((LOG_TRACE, 3, "%d %s:m_dvs->UpdateSubtitle(true)", __LINE__, __FUNCTION__));
        //m_dvs->UpdateSubtitle(true);
        m_dvs->UpdateSubtitle(false);
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
			m_dvs->UpdatePreferedLanguages(CString(pName));
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
            if( SUCCEEDED(m_dvs->GetIsEmbeddedSubStream(i, &isEmbedded)) )
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
    CAutoLock cAutolock(&m_dvs->m_csSubLock);

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
                        m_dvs->Open();
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
