#include "stdafx.h"
#include "HdmvSub.h"
#include "DVBSub.h"
#include "hdmv_subtitle_provider.h"
#include "../subpic/XySubRenderFrameWrapper.h"
#include "IDirectVobSubXy.h"

#if ENABLE_XY_LOG_HDMVSUB
#define TRACE_SUB_PROVIDER(msg)		XY_LOG_TRACE(msg)
#else
#define TRACE_SUB_PROVIDER(msg)
#endif

//
// HdmvSubtitleProviderImpl
//
HdmvSubtitleProviderImpl::HdmvSubtitleProviderImpl( CBaseSub* pSub )
    : m_pSub(pSub)
    , m_consumer(NULL)
    , m_use_dst_alpha(false)
{

}

HdmvSubtitleProviderImpl::~HdmvSubtitleProviderImpl( void )
{
}

STDMETHODIMP HdmvSubtitleProviderImpl::Connect( IXyOptions *consumer )
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

STDMETHODIMP HdmvSubtitleProviderImpl::RequestFrame( IXySubRenderFrame**subRenderFrame, REFERENCE_TIME now )
{
    CheckPointer(subRenderFrame, E_POINTER);
    *subRenderFrame = NULL;

    HRESULT hr = NOERROR;

    POSITION pos;
    CSize MaxTextureSize, VideoSize;
    CPoint VideoTopLeft;

    pos = m_pSub->GetStartPosition(now);

    if (!pos)
    {
        return S_FALSE;
    }

    bool use_dst_alpha = false;
    hr = m_consumer->XyGetBool(DirectVobSubXyOptions::BOOL_SUB_FRAME_USE_DST_ALPHA, &use_dst_alpha);
    if (m_use_dst_alpha!=use_dst_alpha)
    {
        XY_LOG_INFO("Alpha type changed from "<<m_use_dst_alpha<<" to "<<use_dst_alpha);
        Invalidate(-1);
        m_use_dst_alpha = use_dst_alpha;
    }

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

    hr = Render( now, pos );
    if (SUCCEEDED(hr) && m_xy_sub_render_frame)
    {
        *subRenderFrame = m_xy_sub_render_frame;
        (*subRenderFrame)->AddRef();
    }

    return hr;
}

STDMETHODIMP HdmvSubtitleProviderImpl::Invalidate( REFERENCE_TIME rtInvalidate /*= -1*/ )
{
    if (m_pSubPic )
    {
        if (m_pSubPic->GetStart()>=rtInvalidate) {
            m_pSubPic = NULL;

            //fix me: important!
            m_xy_sub_render_frame = NULL;
        }
        else if (m_pSubPic->GetStop()>rtInvalidate)
        {
            m_pSubPic->SetStop(rtInvalidate);
        }
    }
    return S_OK;
}

HRESULT HdmvSubtitleProviderImpl::ResetAllocator()
{
    const int MAX_SUBPIC_QUEUE_LENGTH = 1;

    m_allocator = DEBUG_NEW CPooledSubPicAllocator(MSP_RGB32, m_cur_output_size, MAX_SUBPIC_QUEUE_LENGTH + 1);
    ASSERT(m_allocator);

    m_allocator->SetCurSize(m_cur_output_size);
    m_allocator->SetCurVidRect(CRect(CPoint(0,0),m_cur_output_size));
    return S_OK;
}

HRESULT HdmvSubtitleProviderImpl::Render( REFERENCE_TIME now, POSITION pos )
{
    REFERENCE_TIME start = m_pSub->GetStart(pos);
    REFERENCE_TIME stop = m_pSub->GetStop(pos);

    if (!(start <= now && now < stop))
    {
        return S_FALSE;
    }

    if(m_pSubPic && m_pSubPic->GetStart() <= now && now < m_pSubPic->GetStop())
    {
        return S_OK;
    }

    //should always re-alloc one for the old be in used by the consumer
    m_pSubPic = NULL;
    ASSERT(m_allocator);
    if(FAILED(m_allocator->AllocDynamicEx(&m_pSubPic))) {
        XY_LOG_ERROR("Failed to allocate subpic");
        return E_FAIL;
    }

    ASSERT(m_pSubPic);

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
        if (!dirty_rect.IsRectEmpty())
        {
            hr = m_pSubPic->GetDirtyRect(&dirty_rect);
            ASSERT(SUCCEEDED(hr));
            if (!m_use_dst_alpha)
            {
                hr = mem_subpic->FlipAlphaValue(dirty_rect);//fixme: mem_subpic.type is now MSP_RGBA_F, not MSP_RGBA
                ASSERT(SUCCEEDED(hr));
                if (FAILED(hr))
                {
                    XY_LOG_ERROR("Failed. "<<XY_LOG_VAR_2_STR(hr));
                    return hr;
                }
            }
            hr = S_OK;
        }
        else
        {
            hr = S_FALSE;
        }
    }
    if (hr == S_OK)
    {
        CRect video_rect(CPoint(0,0), m_cur_output_size);
        m_xy_sub_render_frame = DEBUG_NEW XySubRenderFrameWrapper(mem_subpic, video_rect, video_rect, now, &hr);
    }
    else
    {
        m_xy_sub_render_frame = NULL;
    }
    return hr;
}

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
        m_helper = DEBUG_NEW HdmvSubtitleProviderImpl(m_pSub);
        break;
    case ST_HDMV :
        m_pSub = DEBUG_NEW CHdmvSub();
        if (name.IsEmpty() || (name == _T("Unknown"))) m_name = "HDMV Embedded Subtitle";
        m_helper = DEBUG_NEW HdmvSubtitleProviderImpl(m_pSub);
        break;
    default :
        ASSERT (FALSE);
        m_pSub = NULL;
        m_helper = NULL;
    }
    m_rtStart = 0;
}

HdmvSubtitleProvider::~HdmvSubtitleProvider( void )
{
    delete m_helper;
    delete m_pSub;
}

STDMETHODIMP HdmvSubtitleProvider::NonDelegatingQueryInterface( REFIID riid, void** ppv )
{
    CheckPointer(ppv, E_POINTER);
    *ppv = NULL;

    return
        QI(IPersist)
        QI(ISubStream)
        QI(IXySubRenderProvider)
        __super::NonDelegatingQueryInterface(riid, ppv);
}

// IXySubRenderProvider

STDMETHODIMP HdmvSubtitleProvider::Connect( IXyOptions *consumer )
{
    CAutoLock cAutoLock(&m_csCritSec);
    return m_helper->Connect(consumer);
}

STDMETHODIMP HdmvSubtitleProvider::RequestFrame( IXySubRenderFrame**subRenderFrame, REFERENCE_TIME now )
{
    CAutoLock cAutoLock(&m_csCritSec);
    return m_helper->RequestFrame(subRenderFrame, now - m_rtStart);
}

STDMETHODIMP HdmvSubtitleProvider::Invalidate( REFERENCE_TIME rtInvalidate /*= -1*/ )
{
    CAutoLock cAutoLock(&m_csCritSec);
    return m_helper->Invalidate(rtInvalidate);
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

HRESULT HdmvSubtitleProvider::EndOfStream( void )
{
    CAutoLock cAutoLock(&m_csCritSec);

    m_pSub->EndOfStream();
    return S_OK;
}

HRESULT HdmvSubtitleProvider::SetYuvType( CBaseSub::ColorType colorType, CBaseSub::YuvRangeType yuvRangeType )
{
    CAutoLock cAutoLock(&m_csCritSec);
    Invalidate(-1);
    return m_pSub->SetYuvType(colorType, yuvRangeType);
}

//
// SupFileSubtitleProvider
//
SupFileSubtitleProvider::SupFileSubtitleProvider()
    : CUnknown(NAME("SupSubFile"), NULL)
    , m_pSub(NULL)
    , m_helper(NULL)
{
}

SupFileSubtitleProvider::~SupFileSubtitleProvider()
{
    CAMThread::Close();

    delete m_pSub;
    delete m_helper;
}

static UINT64 ReadByte(CFile* mfile, size_t count = 1)
{
    if (count <= 0 || count >= 64) {
        return 0;
    }
    UINT64 ret = 0;
    BYTE buf[64];
    if (mfile->Read(buf, count) != count) {
        return 0;
    }
    for(size_t i = 0; i<count; i++) {
        ret = (ret << 8) + (buf[i] & 0xff);
    }

    return ret;
}

static CString StripPath(CString path)
{
    CString p = path;
    p.Replace('\\', '/');
    p = p.Mid(p.ReverseFind('/')+1);
    return (p.IsEmpty() ? path : p);
}

bool SupFileSubtitleProvider::Open(CString fn, CString subName /*= _T("")*/)
{
    if (!m_fname.IsEmpty())
    {
        return false;
    }

    CFile f;
    if (!f.Open(fn, CFile::modeRead|CFile::typeBinary|CFile::shareDenyNone)) {
        return false;
    }

    WORD sync = 0;
    sync = (WORD)ReadByte(&f, 2);
    if (sync != 'PG') {
        return false;
    }

    m_fname   = fn;
    m_subname = subName;
    if (m_subname.IsEmpty())
    {
        m_subname = fn.Left(fn.ReverseFind('.'));
        m_subname = m_subname.Mid(m_subname.ReverseFind('/')+1);
        m_subname = m_subname.Mid(m_subname.ReverseFind('.')+1);
    }

    {
        CAutoLock cAutoLock(&m_csCritSec);

        delete m_pSub;
        delete m_helper;
        m_pSub    = DEBUG_NEW CHdmvSub();
        m_helper  = DEBUG_NEW HdmvSubtitleProviderImpl(m_pSub);
    }

    return CAMThread::Create()==TRUE;
}

DWORD SupFileSubtitleProvider::ThreadProc()
{
    CFile f;
    if (!f.Open(m_fname, CFile::modeRead|CFile::typeBinary|CFile::shareDenyNone)) {
        return 1;
    }

    f.SeekToBegin();

    CMemFile sub;
    sub.SetLength(f.GetLength());
    sub.SeekToBegin();

    int len;
    BYTE buff[65536];
    while ((len = f.Read(buff, sizeof(buff))) > 0) {
        sub.Write(buff, len);
    }
    f.Close();
    sub.SeekToBegin();

    WORD sync              = 0;
    USHORT size            = 0;
    REFERENCE_TIME rtStart = 0;
    REFERENCE_TIME rtStop  = 0;

    CAutoLock cAutoLock(&m_csCritSec);
    while (sub.GetPosition() < (sub.GetLength() - 10)) {
        sync = (WORD)ReadByte(&sub, 2);
        if (sync == 'PG') {
            rtStart = UINT64(ReadByte(&sub, 4) * 1000 / 9);
            rtStop = UINT64(ReadByte(&sub, 4) * 1000 / 9);
            sub.Seek(1, CFile::current); // Segment type
            size = ReadByte(&sub, 2) + 3;    // Segment size
            sub.Seek(-3, CFile::current);
            sub.Read(buff, size);
            m_pSub->ParseSample(buff, size, rtStart, rtStop);
        } else {
            break;
        }
    }

    sub.Close();

    return 0;
}

STDMETHODIMP SupFileSubtitleProvider::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
    CheckPointer(ppv, E_POINTER);
    *ppv = NULL;

    return
        QI(IPersist)
        QI(ISubStream)
        QI(IXySubRenderProvider)
        __super::NonDelegatingQueryInterface(riid, ppv);
}

// IXySubRenderProvider

STDMETHODIMP SupFileSubtitleProvider::Connect( IXyOptions *consumer )
{
    CAutoLock cAutoLock(&m_csCritSec);
    return m_helper->Connect(consumer);
}

STDMETHODIMP SupFileSubtitleProvider::RequestFrame( IXySubRenderFrame**subRenderFrame, REFERENCE_TIME now )
{
    CAutoLock cAutoLock(&m_csCritSec);
    return m_helper->RequestFrame(subRenderFrame, now);
}

STDMETHODIMP SupFileSubtitleProvider::Invalidate( REFERENCE_TIME rtInvalidate /*= -1*/ )
{
    CAutoLock cAutoLock(&m_csCritSec);
    return m_helper->Invalidate(rtInvalidate);
}

// IPersist

STDMETHODIMP SupFileSubtitleProvider::GetClassID(CLSID* pClassID)
{
    return pClassID ? *pClassID = __uuidof(this), S_OK : E_POINTER;
}

// ISubStream

STDMETHODIMP_(int) SupFileSubtitleProvider::GetStreamCount()
{
    return 1;
}

STDMETHODIMP SupFileSubtitleProvider::GetStreamInfo(int iStream, WCHAR** ppName, LCID* pLCID)
{
    if (iStream != 0) {
        return E_INVALIDARG;
    }

    if (ppName) {
        *ppName = (WCHAR*)CoTaskMemAlloc((m_subname.GetLength()+1)*sizeof(WCHAR));
        if (!(*ppName)) {
            return E_OUTOFMEMORY;
        }

        wcscpy_s (*ppName, m_subname.GetLength()+1, CStringW(m_subname));
    }

    if (pLCID) {
        *pLCID = 0;
    }

    return S_OK;
}

STDMETHODIMP_(int) SupFileSubtitleProvider::GetStream()
{
    return 0;
}

STDMETHODIMP SupFileSubtitleProvider::SetStream(int iStream)
{
    return iStream == 0 ? S_OK : E_FAIL;
}

STDMETHODIMP SupFileSubtitleProvider::Reload()
{
    CAutoLock cAutoLock(&m_csCritSec);
    return Open(m_fname, m_subname) ? S_OK : E_FAIL;
}

HRESULT SupFileSubtitleProvider::SetYuvType( CBaseSub::ColorType colorType, CBaseSub::YuvRangeType yuvRangeType )
{
    CAutoLock cAutoLock(&m_csCritSec);
    Invalidate(-1);
    return m_pSub->SetYuvType(colorType, yuvRangeType);
}

