#include "stdafx.h"
#include "HdmvSub.h"
#include "DVBSub.h"
#include "hdmv_subtitle_provider.h"
#include "../subpic/XySubRenderFrameWrapper.h"

#if ENABLE_XY_LOG_HDMVSUB
#define TRACE_SUB_PROVIDER(msg)		XY_LOG_TRACE(msg)
#else
#define TRACE_SUB_PROVIDER(msg)
#endif

//
// HdmvSubtitleProvider
//
HdmvSubtitleProvider::HdmvSubtitleProvider( CCritSec* pLock, SUBTITLE_TYPE nType
                                           , const CString& name, LCID lcid )
                                           : CUnknown(NAME("HdmvSubtitleProvider"), NULL)
                                           , m_name(name), m_lcid(lcid)
{
    switch (nType) {
    case ST_DVB :
        m_pSub = DEBUG_NEW CDVBSub();
        if (name.IsEmpty() || (name == _T("Unknown"))) m_name = "DVB Embedded Subtitle";
        break;
    case ST_HDMV :
        m_pSub = DEBUG_NEW CHdmvSub();
        if (name.IsEmpty() || (name == _T("Unknown"))) m_name = "HDMV Embedded Subtitle";
        break;
    default :
        ASSERT (FALSE);
        m_pSub = NULL;
    }
    m_rtStart = 0;
}

HdmvSubtitleProvider::~HdmvSubtitleProvider( void )
{
    delete m_pSub;
}

STDMETHODIMP HdmvSubtitleProvider::NonDelegatingQueryInterface( REFIID riid, void** ppv )
{
    CheckPointer(ppv, E_POINTER);
    *ppv = NULL;

    return
        QI(IPersist)
        QI(ISubStream)
        QI(ISubPicProvider)
        __super::NonDelegatingQueryInterface(riid, ppv);
}

// IXySubRenderProvider

STDMETHODIMP HdmvSubtitleProvider::Connect( IXyOptions *consumer )
{
    CAutoLock cAutoLock(&m_csCritSec);
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

STDMETHODIMP HdmvSubtitleProvider::RequestFrame( IXySubRenderFrame**subRenderFrame, REFERENCE_TIME now )
{
    CAutoLock cAutoLock(&m_csCritSec);
    CheckPointer(subRenderFrame, E_POINTER);
    *subRenderFrame = NULL;

    HRESULT hr = NOERROR;

    POSITION pos;
    CSize MaxTextureSize, VideoSize;
    CPoint VideoTopLeft;
    pos = m_pSub->GetStartPosition(now - m_rtStart);

    hr = m_pSub->GetTextureSize(pos, MaxTextureSize, VideoSize, VideoTopLeft);
    if (SUCCEEDED(hr))
    {
        ASSERT(MaxTextureSize==VideoSize && VideoTopLeft==CPoint(0,0));
        if (!(MaxTextureSize==VideoSize && VideoTopLeft==CPoint(0,0)))
        {
            XY_LOG_ERROR("We don't know how to deal with this sizes."
                <<XY_LOG_VAR_2_STR(MaxTextureSize)<<XY_LOG_VAR_2_STR(VideoSize)
                <<XY_LOG_VAR_2_STR(VideoTopLeft));
            return E_FAIL;
        }
    }
    else
    {
        XY_LOG_ERROR("Failed to get sizes info");
        return hr;
    }
    if (!m_allocator || m_cur_output_size!=MaxTextureSize)
    {
        m_cur_output_size = MaxTextureSize;
        if (m_cur_output_size.cx!=0 && m_cur_output_size.cy!=0)
        {
            hr = ResetAllocator();
            ASSERT(SUCCEEDED(hr));
        }
        else
        {
            XY_LOG_WARN("Invalid output size "<<m_cur_output_size);
            return S_FALSE;
        }
    }
    ASSERT(m_allocator);
    if(!m_pSubPic)
    {
        if(FAILED(m_allocator->AllocDynamicEx(&m_pSubPic))) {
            XY_LOG_ERROR("Failed to allocate subpic");
            return E_FAIL;
        }
    }

    hr = Render( now, pos );
    if (SUCCEEDED(hr) && m_xy_sub_render_frame)
    {
        *subRenderFrame = m_xy_sub_render_frame;
        (*subRenderFrame)->AddRef();
    }

    return hr;
}

STDMETHODIMP HdmvSubtitleProvider::Invalidate( REFERENCE_TIME rtInvalidate /*= -1*/ )
{
    return S_OK;
}

// IPersist

STDMETHODIMP HdmvSubtitleProvider::GetClassID(CLSID* pClassID)
{
    return pClassID ? *pClassID = __uuidof(this), S_OK : E_POINTER;
}

// ISubStream

STDMETHODIMP_(int) HdmvSubtitleProvider::GetStreamCount()
{
    return (1);
}

STDMETHODIMP HdmvSubtitleProvider::GetStreamInfo(int iStream, WCHAR** ppName, LCID* pLCID)
{
    if(iStream != 0) {
        return E_INVALIDARG;
    }

    if(ppName) {
        *ppName = (WCHAR*)CoTaskMemAlloc((m_name.GetLength()+1)*sizeof(WCHAR));
        if(!(*ppName)) {
            return E_OUTOFMEMORY;
        }

        wcscpy_s (*ppName, m_name.GetLength()+1, CStringW(m_name));
    }

    if(pLCID) {
        *pLCID = m_lcid;
    }

    return S_OK;
}

STDMETHODIMP_(int) HdmvSubtitleProvider::GetStream()
{
    return(0);
}

STDMETHODIMP HdmvSubtitleProvider::SetStream(int iStream)
{
    return iStream == 0 ? S_OK : E_FAIL;
}

STDMETHODIMP HdmvSubtitleProvider::Reload()
{
    return S_OK;
}

HRESULT HdmvSubtitleProvider::ParseSample (IMediaSample* pSample)
{
    CAutoLock cAutoLock(&m_csCritSec);
    HRESULT		hr;

    hr = m_pSub->ParseSample (pSample);
    return hr;
}

HRESULT HdmvSubtitleProvider::NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate)
{
    CAutoLock cAutoLock(&m_csCritSec);

    m_pSub->Reset();
    m_rtStart = tStart;
    return S_OK;
}

HRESULT HdmvSubtitleProvider::SetYuvType( CBaseSub::ColorType colorType, CBaseSub::YuvRangeType yuvRangeType )
{
    CAutoLock cAutoLock(&m_csCritSec);
    return m_pSub->SetYuvType(colorType, yuvRangeType);
}

HRESULT HdmvSubtitleProvider::ResetAllocator()
{
    const int MAX_SUBPIC_QUEUE_LENGTH = 1;

    m_allocator = new CPooledSubPicAllocator(MSP_RGB32, m_cur_output_size, MAX_SUBPIC_QUEUE_LENGTH + 1);
    ASSERT(m_allocator);

    m_allocator->SetCurSize(m_cur_output_size);
    m_allocator->SetCurVidRect(CRect(CPoint(0,0),m_cur_output_size));
    return S_OK;
}

HRESULT HdmvSubtitleProvider::Render( REFERENCE_TIME now, POSITION pos )
{
    REFERENCE_TIME start = m_pSub->GetStart(pos);
    REFERENCE_TIME stop = m_pSub->GetStop(pos);
    if (!(start <= now && now < stop))
    {
        return S_FALSE;
    }

    ASSERT(m_pSubPic);
    if(m_pSubPic->GetStart() <= now && now < m_pSubPic->GetStop())
    {
        return S_OK;
    }
    HRESULT hr = E_FAIL;

    CMemSubPic * mem_subpic = dynamic_cast<CMemSubPic*>((ISubPicEx *)m_pSubPic);
    ASSERT(mem_subpic);
    SubPicDesc spd;
    if(SUCCEEDED(m_pSubPic->Lock(spd)))
    {
        CRect dirty_rect;
        DWORD color = 0xFF000000;
        if(SUCCEEDED(m_pSubPic->ClearDirtyRect(color)))
        {
            m_pSub->Render(spd, now, dirty_rect);
            
            TRACE_SUB_PROVIDER(XY_LOG_VAR_2_STR(start)<<XY_LOG_VAR_2_STR(stop));

            m_pSubPic->SetStart(start);
            m_pSubPic->SetStop(stop);
        }
        hr = m_pSubPic->Unlock(&dirty_rect);
        ASSERT(SUCCEEDED(hr));
        if (FAILED(hr))
        {
            XY_LOG_ERROR("Failed to unlock subtitle."
                <<XY_LOG_VAR_2_STR(hr)<<XY_LOG_VAR_2_STR(dirty_rect));
        }
        hr = m_pSubPic->GetDirtyRect(&dirty_rect);
        ASSERT(SUCCEEDED(hr));
        hr = mem_subpic->FlipAlphaValue(dirty_rect);//fixme: mem_subpic.type is now MSP_RGBA_F, not MSP_RGBA
        ASSERT(SUCCEEDED(hr));
        if (FAILED(hr))
        {
            XY_LOG_ERROR("Failed. "<<XY_LOG_VAR_2_STR(hr));
            return hr;
        }
    }
    CRect video_rect(CPoint(0,0), m_cur_output_size);
    m_xy_sub_render_frame = new XySubRenderFrameWrapper(mem_subpic, video_rect, video_rect, now, &hr);
    return hr;
}
