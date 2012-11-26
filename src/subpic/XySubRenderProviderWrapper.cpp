#include "stdafx.h"
#include "XySubRenderProviderWrapper.h"

XySubRenderProviderWrapper::XySubRenderProviderWrapper( ISubPicProviderEx *provider, IXyOptions *consumer
    , HRESULT* phr/*=NULL*/ )
    : CUnknown(NAME("XySubRenderProviderWrapper"), NULL, phr)
    , m_provider(provider)
    , m_consumer(consumer)
{
    HRESULT hr = NOERROR;
    if (!provider || !consumer)
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

STDMETHODIMP XySubRenderProviderWrapper::RequestFrame( IXySubRenderFrame**subRenderFrame, REFERENCE_TIME now  )
{
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
    ASSERT(m_consumer);
    hr = m_consumer->XyGetSize(DirectVobSubXyOptions::SIZE_ORIGINAL_VIDEO, &original_video_size);
    ASSERT(SUCCEEDED(hr));

    if (m_original_video_size!=original_video_size)
    {
        Invalidate();
        m_original_video_size = original_video_size;
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
        m_pSubPic->UnlockEx(&rectList);
        CRect dirty_rect;
        hr = m_pSubPic->GetDirtyRect(&dirty_rect);
        ASSERT(SUCCEEDED(hr));
        hr = mem_subpic->FlipAlphaValue(dirty_rect);//fixme: mem_subpic.type is now MSP_RGBA_F, not MSP_RGBA
        ASSERT(SUCCEEDED(hr));
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
    m_xy_sub_render_frame = new XySubRenderFrameWrapper(mem_subpic, video_rect, video_rect, now, &hr);
    return hr;
}

HRESULT XySubRenderProviderWrapper::ResetAllocator()
{
    const int MAX_SUBPIC_QUEUE_LENGTH = 1;

    m_allocator = new CPooledSubPicAllocator(MSP_RGB32, m_original_video_size, MAX_SUBPIC_QUEUE_LENGTH + 1);
    ASSERT(m_allocator);

    m_allocator->SetCurSize(m_original_video_size);
    m_allocator->SetCurVidRect(CRect(CPoint(0,0),m_original_video_size));
    return S_OK;
}

STDMETHODIMP XySubRenderProviderWrapper::Invalidate( REFERENCE_TIME rtInvalidate /*= -1*/ )
{
    m_pSubPic = NULL;
    m_xy_sub_render_frame = NULL;
    m_allocator = NULL;
    return S_OK;
}

//
// XySubRenderProviderWrapper2
//
XySubRenderProviderWrapper2::XySubRenderProviderWrapper2( ISubPicProviderEx2 *provider, IXyOptions *consumer 
    , HRESULT* phr/*=NULL*/ )
    : CUnknown(NAME("XySubRenderProviderWrapper"), NULL, phr)
    , m_provider(provider)
    , m_consumer(consumer)
    , m_start(0), m_stop(0)
    , m_fps(0)
{
    HRESULT hr = NOERROR;
    if (!provider || !consumer)
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

STDMETHODIMP XySubRenderProviderWrapper2::RequestFrame( IXySubRenderFrame**subRenderFrame, REFERENCE_TIME now )
{
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
    ASSERT(m_consumer);
    hr = m_consumer->XyGetRect(DirectVobSubXyOptions::RECT_VIDEO_OUTPUT, &output_rect);
    ASSERT(SUCCEEDED(hr));
    hr = m_consumer->XyGetRect(DirectVobSubXyOptions::RECT_SUBTITLE_TARGET, &subtitle_target_rect);
    ASSERT(SUCCEEDED(hr));
    ASSERT(output_rect==subtitle_target_rect);
    hr = m_consumer->XyGetSize(DirectVobSubXyOptions::SIZE_ORIGINAL_VIDEO, &original_video_size);
    ASSERT(SUCCEEDED(hr));

    if (m_output_rect!=output_rect || m_subtitle_target_rect!=subtitle_target_rect
        || m_original_video_size!=original_video_size)
    {
        Invalidate();
        m_output_rect = output_rect;
        m_original_video_size = original_video_size;
        m_subtitle_target_rect = subtitle_target_rect;
    }

    POSITION pos = m_provider->GetStartPosition(now, fps);
    if (!pos)
    {
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
    m_xy_sub_render_frame = NULL;
    m_start = m_stop = 0;

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

    hr = m_provider->RenderEx(&m_xy_sub_render_frame, MSP_RGBA_F
        , m_output_rect, m_subtitle_target_rect
        , m_original_video_size, now, m_fps);

    ASSERT(SUCCEEDED(hr));

    m_start = m_provider->GetStart(pos, m_fps);
    m_stop = m_provider->GetStop(pos, m_fps);
    XY_LOG_TRACE(XY_LOG_VAR_2_STR(m_start)<<XY_LOG_VAR_2_STR(m_stop));

    m_provider->Unlock();
    return hr;
}
