#include "stdafx.h"
#include "XySubRenderProviderWrapper.h"

XySubRenderProviderWrapper::XySubRenderProviderWrapper( ISubPicProviderEx *provider
    , HRESULT* phr/*=NULL*/ )
    : CUnknown(NAME("XySubRenderProviderWrapper"), NULL, phr)
    , m_provider(provider)
    , m_consumer(NULL)
    , m_use_dst_alpha(false)
{
    HRESULT hr = NOERROR;
    if (!provider)
    {
        hr = E_INVALIDARG;
    }

    if (phr)
    {
        *phr = hr;
    }
}

STDMETHODIMP XySubRenderProviderWrapper::NonDelegatingQueryInterface( REFIID riid, void** ppv )
{
    return
        QI(IXySubRenderProvider)
        __super::NonDelegatingQueryInterface(riid, ppv);
}

STDMETHODIMP XySubRenderProviderWrapper::Connect( IXyOptions *consumer )
{
    HRESULT hr = NOERROR;
    if (consumer)
    {
        hr = m_consumer != consumer ? S_OK : S_FALSE;
        m_consumer = consumer;
    }
    else
    {
        hr = E_INVALIDARG;
    }
    return hr;
}

STDMETHODIMP XySubRenderProviderWrapper::RequestFrame( IXySubRenderFrame**subRenderFrame, REFERENCE_TIME now  )
{
    ASSERT(m_consumer);
    double fps;
    CheckPointer(subRenderFrame, E_POINTER);
    *subRenderFrame = NULL;

    HRESULT hr = m_consumer->XyGetDouble(DirectVobSubXyOptions::DOUBLE_FPS, &fps);
    if (FAILED(hr))
    {
        XY_LOG_ERROR("Failed to get fps. "<<XY_LOG_VAR_2_STR(hr));
        return hr;
    }

    CRect output_rect, subtitle_target_rect;
    CSize original_video_size;
    bool use_dst_alpha = false;
    ASSERT(m_consumer);
    hr = m_consumer->XyGetSize(DirectVobSubXyOptions::SIZE_ORIGINAL_VIDEO, &original_video_size);
    ASSERT(SUCCEEDED(hr));
    hr = m_consumer->XyGetBool(DirectVobSubXyOptions::BOOL_SUB_FRAME_USE_DST_ALPHA, &use_dst_alpha);

    if (m_original_video_size!=original_video_size
        || m_use_dst_alpha!=use_dst_alpha)
    {
        if (m_use_dst_alpha==use_dst_alpha)
        {
            XY_LOG_WARN("Original video size changed from "<<m_original_video_size<<" to "<<original_video_size);
        }
        else
        {
            XY_LOG_INFO("'Use dst alpha' option changed from "<<m_use_dst_alpha<<" to "<<use_dst_alpha);
        }
        Invalidate();
        m_original_video_size = original_video_size;
        m_use_dst_alpha = use_dst_alpha;
    }
    if(!m_pSubPic)
    {
        if (!m_allocator)
        {
            ResetAllocator();
        }
        if(FAILED(m_allocator->AllocDynamicEx(&m_pSubPic))) {
            XY_LOG_ERROR("Failed to allocate subpic");
            return E_FAIL;
        }
    }

    POSITION pos = m_provider->GetStartPosition(now, fps);
    if (!pos)
    {
        return S_FALSE;
    }
    REFERENCE_TIME start = m_provider->GetStart(pos, fps);
    REFERENCE_TIME stop = m_provider->GetStop(pos, fps); //fixme: have to get start stop twice
    if (!(start <= now && now < stop))
    {
        return S_FALSE;
    }

    hr = Render( now, pos, fps );
    if (FAILED(hr))
    {
        return hr;
    }
    if (m_xy_sub_render_frame)
    {
        *subRenderFrame = m_xy_sub_render_frame;
        (*subRenderFrame)->AddRef();
    }

    return hr;
}

HRESULT XySubRenderProviderWrapper::Render( REFERENCE_TIME now, POSITION pos, double fps )
{
    ASSERT(m_pSubPic);
    if(m_pSubPic->GetStart() <= now && now < m_pSubPic->GetStop())
    {
        return S_OK;
    }
    HRESULT hr = E_FAIL;

    if(FAILED(m_provider->Lock())) {
        return hr;
    }

    CMemSubPic * mem_subpic = dynamic_cast<CMemSubPic*>((ISubPicEx *)m_pSubPic);
    ASSERT(mem_subpic);
    SubPicDesc spd;
    if(SUCCEEDED(m_pSubPic->Lock(spd)))
    {
        CAtlList<CRect> rectList;
        DWORD color = 0xFF000000;
        if(SUCCEEDED(m_pSubPic->ClearDirtyRect(color)))
        {
            hr = m_provider->RenderEx(spd, now, fps, rectList);

            REFERENCE_TIME start = m_provider->GetStart(pos, fps);
            REFERENCE_TIME stop = m_provider->GetStop(pos, fps);
            XY_LOG_TRACE(XY_LOG_VAR_2_STR(start)<<XY_LOG_VAR_2_STR(stop));

            m_pSubPic->SetStart(start);
            m_pSubPic->SetStop(stop);
        }
        else
        {
            rectList.AddHead(CRect(CPoint(0,0),m_original_video_size));
            XY_LOG_ERROR("Failed to clear subpic!");
        }
        m_pSubPic->UnlockEx(&rectList);
        CRect dirty_rect;
        hr = m_pSubPic->GetDirtyRect(&dirty_rect);
        ASSERT(SUCCEEDED(hr));
        if (!m_use_dst_alpha)
        {
            hr = mem_subpic->FlipAlphaValue(dirty_rect);//fixme: mem_subpic.type is now MSP_RGBA_F, not MSP_RGBA
            ASSERT(SUCCEEDED(hr));
        }
    }

    m_provider->Unlock();

    if (FAILED(hr))
    {
        return hr;
    }

    ASSERT(SUCCEEDED(hr));
    if (FAILED(hr))
    {
        return hr;
    }
    CRect video_rect(CPoint(0,0), m_original_video_size);
    m_xy_sub_render_frame = DEBUG_NEW XySubRenderFrameWrapper(mem_subpic, video_rect, video_rect, now, &hr);
    return hr;
}

HRESULT XySubRenderProviderWrapper::ResetAllocator()
{
    const int MAX_SUBPIC_QUEUE_LENGTH = 1;

    m_allocator = DEBUG_NEW CPooledSubPicAllocator(MSP_RGB32, m_original_video_size, MAX_SUBPIC_QUEUE_LENGTH + 1);
    ASSERT(m_allocator);

    m_allocator->SetCurSize(m_original_video_size);
    m_allocator->SetCurVidRect(CRect(CPoint(0,0),m_original_video_size));
    return S_OK;
}

STDMETHODIMP XySubRenderProviderWrapper::Invalidate( REFERENCE_TIME rtInvalidate /*= -1*/ )
{
    if (m_pSubPic )
    {
        if (m_pSubPic->GetStart()>=rtInvalidate) {
            m_pSubPic = NULL;

            //fix me: important!
            m_xy_sub_render_frame = NULL;
            m_allocator = NULL;
        }
        else if (m_pSubPic->GetStop()>rtInvalidate)
        {
            m_pSubPic->SetStop(rtInvalidate);
        }
    }

    return S_OK;
}

//
// XySubRenderProviderWrapper2
//
XySubRenderProviderWrapper2::XySubRenderProviderWrapper2( ISubPicProviderEx2 *provider
    , HRESULT* phr/*=NULL*/ )
    : CUnknown(NAME("XySubRenderProviderWrapper2"), NULL, phr)
    , m_provider(provider)
    , m_consumer(NULL)
    , m_start(0), m_stop(0)
    , m_fps(0)
    , m_max_bitmap_count2(0)
    , m_use_dst_alpha(0)
{
    HRESULT hr = NOERROR;
    if (!provider)
    {
        hr = E_INVALIDARG;
    }

    if (phr)
    {
        *phr = hr;
    }
}

STDMETHODIMP XySubRenderProviderWrapper2::NonDelegatingQueryInterface( REFIID riid, void** ppv )
{
    return
        QI(IXySubRenderProvider)
        __super::NonDelegatingQueryInterface(riid, ppv);
}

STDMETHODIMP XySubRenderProviderWrapper2::Connect( IXyOptions *consumer )
{
    HRESULT hr = NOERROR;
    if (consumer)
    {
        hr = m_consumer != consumer ? S_OK : S_FALSE;
        m_consumer = consumer;
    }
    else
    {
        hr = E_INVALIDARG;
    }
    return hr;
}

STDMETHODIMP XySubRenderProviderWrapper2::RequestFrame( IXySubRenderFrame**subRenderFrame, REFERENCE_TIME now )
{
    ASSERT(m_consumer);
    double fps;
    CheckPointer(subRenderFrame, E_POINTER);
    *subRenderFrame = NULL;

    HRESULT hr = m_consumer->XyGetDouble(DirectVobSubXyOptions::DOUBLE_FPS, &fps);
    if (FAILED(hr))
    {
        XY_LOG_ERROR("Failed to get fps. "<<XY_LOG_VAR_2_STR(hr));
        return hr;
    }
    m_fps = fps;//fix me: invalidate

    CRect output_rect, subtitle_target_rect;
    CSize original_video_size;
    bool use_dst_alpha = false;
    bool combine_bitmap = false;
    ASSERT(m_consumer);
    hr = m_consumer->XyGetRect(DirectVobSubXyOptions::RECT_VIDEO_OUTPUT, &output_rect);
    ASSERT(SUCCEEDED(hr));
    hr = m_consumer->XyGetRect(DirectVobSubXyOptions::RECT_SUBTITLE_TARGET, &subtitle_target_rect);
    ASSERT(SUCCEEDED(hr));
    ASSERT(output_rect==subtitle_target_rect);
    hr = m_consumer->XyGetSize(DirectVobSubXyOptions::SIZE_ORIGINAL_VIDEO, &original_video_size);
    ASSERT(SUCCEEDED(hr));
    hr = m_consumer->XyGetInt(DirectVobSubXyOptions::INT_MAX_BITMAP_COUNT2, &m_max_bitmap_count2);
    ASSERT(SUCCEEDED(hr));
    hr = m_consumer->XyGetBool(DirectVobSubXyOptions::BOOL_SUB_FRAME_USE_DST_ALPHA, &use_dst_alpha);

    bool should_invalidate = false;
    bool should_invalidate_allocator = false;
    if (m_output_rect!=output_rect 
        || m_subtitle_target_rect!=subtitle_target_rect
        || m_original_video_size!=original_video_size
        || m_use_dst_alpha!=use_dst_alpha)
    {
        should_invalidate = true;
        should_invalidate_allocator = (m_subtitle_target_rect!=subtitle_target_rect)==TRUE;

        m_output_rect = output_rect;
        m_original_video_size = original_video_size;
        m_subtitle_target_rect = subtitle_target_rect;
        m_use_dst_alpha = use_dst_alpha;
    }

    if (m_xy_sub_render_frame)
    {
        int count = 0;
        hr = m_xy_sub_render_frame->GetBitmapCount(&count);
        should_invalidate = (count>m_max_bitmap_count2);
    }
    if (should_invalidate)
    {
        XY_LOG_INFO("Output rects or max_bitmap_count or alpha type changed.");
        Invalidate();
    }
    if (should_invalidate_allocator)
    {
        CSize max_size(m_subtitle_target_rect.right, m_subtitle_target_rect.bottom);

        m_allocator = DEBUG_NEW CPooledSubPicAllocator(MSP_RGB32, max_size,2);
        ASSERT(m_allocator);

        m_allocator->SetCurSize(max_size);
        m_allocator->SetCurVidRect(CRect(CPoint(0,0),max_size));
    }

    POSITION pos = m_provider->GetStartPosition(now, fps);
    if (!pos)
    {
        XY_LOG_TRACE("No subtitles at "<<XY_LOG_VAR_2_STR(now));
        return S_FALSE;
    }
    REFERENCE_TIME start = m_provider->GetStart(pos, fps);
    REFERENCE_TIME stop = m_provider->GetStop(pos, fps); //fixme: have to get start stop twice
    if (!(start <= now && now < stop))
    {
        XY_LOG_TRACE("No subtitles at "<<XY_LOG_VAR_2_STR(now));
        return S_FALSE;
    }

    hr = Render( now, pos );
    if (FAILED(hr))
    {
        return hr;
    }
    if (m_xy_sub_render_frame)
    {
        *subRenderFrame = m_xy_sub_render_frame;
        (*subRenderFrame)->AddRef();
    }
    return hr;
}

STDMETHODIMP XySubRenderProviderWrapper2::Invalidate( REFERENCE_TIME rtInvalidate /*= -1*/ )
{
    if (m_start>=rtInvalidate)
    {
        m_xy_sub_render_frame = NULL;
        m_start = m_stop = 0;
    }
    else if (m_stop>rtInvalidate)
    {
        m_stop = rtInvalidate;
    }


    return S_OK;
}

HRESULT XySubRenderProviderWrapper2::Render( REFERENCE_TIME now, POSITION pos )
{
    if(m_start <= now && now < m_stop && m_xy_sub_render_frame)
    {
        return S_FALSE;
    }

    m_xy_sub_render_frame = NULL;
    HRESULT hr = E_FAIL;

    if(FAILED(m_provider->Lock())) {
        return hr;
    }

    int spd_type = m_use_dst_alpha ? MSP_RGBA : MSP_RGBA_F;
    hr = m_provider->RenderEx(&m_xy_sub_render_frame, spd_type, 
        m_output_rect, m_subtitle_target_rect,
        m_original_video_size, now, m_fps);
    ASSERT(SUCCEEDED(hr));

    bool should_combine = false;
    if (m_xy_sub_render_frame)
    {
        int count = 0;
        hr = m_xy_sub_render_frame->GetBitmapCount(&count);
        ASSERT(SUCCEEDED(hr));
        should_combine = count > m_max_bitmap_count2;
    }
    if (should_combine)
    {
        hr = CombineBitmap(now);
        ASSERT(SUCCEEDED(hr));
    }

    m_start = m_provider->GetStart(pos, m_fps);
    m_stop = m_provider->GetStop(pos, m_fps);
    XY_LOG_TRACE(XY_LOG_VAR_2_STR(m_start)<<XY_LOG_VAR_2_STR(m_stop));

    m_provider->Unlock();
    return hr;
}

HRESULT XySubRenderProviderWrapper2::CombineBitmap(REFERENCE_TIME now)
{
    static int s_combined_bitmap_id = 0x10000000;//fixme: important! Uncombined bitmap id MUST < 0x10000000.

    XY_LOG_TRACE(now);
    HRESULT hr = NOERROR;
    if (m_xy_sub_render_frame)
    {
        m_subpic = NULL;
        hr = m_allocator->AllocDynamicEx(&m_subpic);
        if(FAILED(hr) || !m_subpic) {
            XY_LOG_FATAL("Failed to allocate subpic. "<<XY_LOG_VAR_2_STR(hr));
            return hr;
        }

        int count = 0;
        hr = m_xy_sub_render_frame->GetBitmapCount(&count);
        ASSERT(SUCCEEDED(hr));
        if (count==1)
        {
            return S_OK;
        }
        SubPicDesc spd;
        hr = m_subpic->Lock(spd);
        if (FAILED(hr))
        {
            XY_LOG_ERROR("Failed to lock spd. "<<XY_LOG_VAR_2_STR(hr));
            return hr;
        }
        DWORD color = 0x00000000;
        hr = m_subpic->ClearDirtyRect(color);
        if(FAILED(hr))
        {
            XY_LOG_ERROR("Failed to clear dirty rect. "<<XY_LOG_VAR_2_STR(hr));
            return hr;
        }
        CRect dirty_rect;
        for (int i=0;i<count;i++)
        {
            POINT pos;
            SIZE size;
            LPCVOID pixels;
            int pitch;
            hr = m_xy_sub_render_frame->GetBitmap(i, NULL, &pos, &size, &pixels, &pitch );
            if (FAILED(hr))
            {
                XY_LOG_ERROR("Failed to get bitmap. "<<XY_LOG_VAR_2_STR(hr));
                return hr;
            }
            ASSERT(SUCCEEDED(hr));
            dirty_rect |= CRect(pos, size);
            XyBitmap::BltPack(spd, pos, size, pixels, pitch);
        }
        hr = m_subpic->Unlock(&dirty_rect);
        if (FAILED(hr))
        {
            XY_LOG_ERROR("Failed to unlock. "<<XY_LOG_VAR_2_STR(dirty_rect)<<XY_LOG_VAR_2_STR(hr));
            return hr;
        }
        hr = m_subpic->GetDirtyRect(&dirty_rect);
        ASSERT(SUCCEEDED(hr));
        CMemSubPic * mem_subpic = dynamic_cast<CMemSubPic*>((ISubPicEx *)m_subpic);
        ASSERT(mem_subpic);

        m_xy_sub_render_frame = DEBUG_NEW XySubRenderFrameWrapper(mem_subpic, m_output_rect, m_subtitle_target_rect
            , s_combined_bitmap_id++, &hr);
        return hr;
    }
    return hr;
}
