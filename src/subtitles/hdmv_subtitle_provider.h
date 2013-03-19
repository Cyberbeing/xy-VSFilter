#pragma once

#include "Rasterizer.h"
#include "../SubPic/SubPicProviderImpl.h"
#include "HdmvSub.h"
#include "BaseSub.h"


class __declspec(uuid("F316F98F-BDB5-42AA-A253-8D6C8AA20754"))
HdmvSubtitleProvider : public CUnknown, public IXySubRenderProvider, public ISubStream
{
public:
    HdmvSubtitleProvider(CCritSec* pLock, SUBTITLE_TYPE nType, const CString& name, LCID lcid);
    ~HdmvSubtitleProvider(void);

    DECLARE_IUNKNOWN
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);

    // IXySubRenderProvider
    STDMETHODIMP Connect( IXyOptions *consumer );
    STDMETHODIMP RequestFrame(IXySubRenderFrame**subRenderFrame, REFERENCE_TIME now);
    STDMETHODIMP Invalidate(REFERENCE_TIME rtInvalidate = -1);

    // IPersist
    STDMETHODIMP GetClassID(CLSID* pClassID);

    // ISubStream
    STDMETHODIMP_(int) GetStreamCount();
    STDMETHODIMP GetStreamInfo(int i, WCHAR** ppName, LCID* pLCID);
    STDMETHODIMP_(int) GetStream();
    STDMETHODIMP SetStream(int iStream);
    STDMETHODIMP Reload();

    //fix me: add to an interface
    HRESULT ParseSample (IMediaSample* pSample);
    HRESULT NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate);
    HRESULT EndOfStream(void);

    HRESULT SetYuvType(CBaseSub::ColorType colorType, CBaseSub::YuvRangeType yuvRangeType);
private:
    HRESULT ResetAllocator();
    HRESULT Render( REFERENCE_TIME now, POSITION pos );
private :
    CString         m_name;
    LCID            m_lcid;
    REFERENCE_TIME  m_rtStart;

    CBaseSub*       m_pSub;
    CCritSec        m_csCritSec;

    IXyOptions *m_consumer;
    CSize m_cur_output_size;
    CComPtr<ISubPicExAllocator> m_allocator;
    CComPtr<ISubPicEx> m_pSubPic;
    CComPtr<IXySubRenderFrame> m_xy_sub_render_frame;
};
