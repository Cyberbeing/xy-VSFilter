#include "stdafx.h"
#include "SimpleSubPicProviderImpl.h"
#include "SimpleSubPicWrapper.h"


//////////////////////////////////////////////////////////////////////////
//
// SimpleSubPicProvider
// 

SimpleSubPicProvider::SimpleSubPicProvider( ISubPicExAllocator* pAllocator, HRESULT* phr )
    : CUnknown(NAME("CSubPicQueueImpl"), NULL)
    , m_pAllocator(pAllocator)
    , m_rtNow(0)
    , m_fps(25.0)
{
    if(phr) {
        *phr = S_OK;
    }

    if(!m_pAllocator) {
        if(phr) {
            *phr = E_FAIL;
        }
        return;
    }
    m_prefered_colortype.AddTail(MSP_AYUV_PLANAR);
    m_prefered_colortype.AddTail(MSP_AYUV);
    m_prefered_colortype.AddTail(MSP_XY_AUYV);
    m_prefered_colortype.AddTail(MSP_RGBA);
}

SimpleSubPicProvider::~SimpleSubPicProvider()
{

}

STDMETHODIMP SimpleSubPicProvider::NonDelegatingQueryInterface( REFIID riid, void** ppv )
{
    return
        QI(ISimpleSubPicProvider)
        __super::NonDelegatingQueryInterface(riid, ppv);
}

STDMETHODIMP SimpleSubPicProvider::SetSubPicProvider( IUnknown* subpic_provider )
{
    CComQIPtr<ISubPicProviderEx> tmp = subpic_provider;
    if (tmp)
    {
        CAutoLock cAutoLock(&m_csSubPicProvider);
        m_pSubPicProviderEx = tmp;

        if(m_pSubPicProviderEx!=NULL && m_pAllocator!=NULL)
        {
            POSITION pos = m_prefered_colortype.GetHeadPosition();
            while(pos!=NULL)
            {
                int color_type = m_prefered_colortype.GetNext(pos);
                if( m_pSubPicProviderEx->IsColorTypeSupported( color_type ) &&
                    m_pAllocator->IsSpdColorTypeSupported( color_type ) )
                {
                    m_pAllocator->SetSpdColorType(color_type);
                    break;
                }
            }
        }
        Invalidate();

        return S_OK;
    }
    return E_NOTIMPL;
}

STDMETHODIMP SimpleSubPicProvider::GetSubPicProvider( IUnknown** subpic_provider )
{
    if(!subpic_provider) {
        return E_POINTER;
    }

    CAutoLock cAutoLock(&m_csSubPicProvider);

    if(m_pSubPicProviderEx) {
        *subpic_provider = m_pSubPicProviderEx;
        (*subpic_provider)->AddRef();
    }

    return !!*subpic_provider ? S_OK : E_FAIL;
}

STDMETHODIMP SimpleSubPicProvider::SetFPS( double fps )
{
    m_fps = fps;

    return S_OK;
}

STDMETHODIMP SimpleSubPicProvider::SetTime( REFERENCE_TIME rtNow )
{
    m_rtNow = rtNow;

    return S_OK;
}

STDMETHODIMP SimpleSubPicProvider::Invalidate( REFERENCE_TIME rtInvalidate /*= -1*/ )
{
    CAutoLock cQueueLock(&m_csLock);

    if( m_pSubPic && m_pSubPic->GetStop() >= rtInvalidate)
    {
        m_pSubPic = NULL;
    }
    return S_OK;
}

STDMETHODIMP_(bool) SimpleSubPicProvider::LookupSubPic( REFERENCE_TIME now /*[in]*/, ISimpleSubPic** output_subpic /*[out]*/ )
{    
    if(output_subpic!=NULL)
    {
        CComPtr<ISubPicEx> temp;
        bool result = LookupSubPicEx(now, &temp);
        (*output_subpic = new SimpleSubPicWrapper(temp))->AddRef();
        return result;
    }
    else
    {
        return LookupSubPicEx(now, NULL);
    }
}

STDMETHODIMP SimpleSubPicProvider::GetStats( int& nSubPics, REFERENCE_TIME& rtNow, REFERENCE_TIME& rtStart, REFERENCE_TIME& rtStop )
{
    CAutoLock cAutoLock(&m_csLock);

    nSubPics = 0;
    rtNow = m_rtNow;
    rtStart = rtStop = 0;

    if(m_pSubPic)
    {
        nSubPics = 1;
        rtStart = m_pSubPic->GetStart();
        rtStop = m_pSubPic->GetStop();
    }

    return S_OK;
}

STDMETHODIMP SimpleSubPicProvider::GetStats( int nSubPic, REFERENCE_TIME& rtStart, REFERENCE_TIME& rtStop )
{
    CAutoLock cAutoLock(&m_csLock);

    if(!m_pSubPic || nSubPic != 0)
        return E_INVALIDARG;

    rtStart = m_pSubPic->GetStart();
    rtStop = m_pSubPic->GetStop();

    return S_OK;
}

bool SimpleSubPicProvider::LookupSubPicEx(REFERENCE_TIME rtNow, ISubPicEx** ppSubPic)
{
    if(!ppSubPic)
        return(false);

    *ppSubPic = NULL;

    CComPtr<ISubPicEx> pSubPic;

    {
        CAutoLock cAutoLock(&m_csLock);

        if(!m_pSubPic)
        {
            if(FAILED(m_pAllocator->AllocDynamicEx(&m_pSubPic))) {
                return false;
            }
        }

        pSubPic = m_pSubPic;
    }

    if(pSubPic->GetStart() <= rtNow && rtNow < pSubPic->GetStop())
    {
        (*ppSubPic = pSubPic)->AddRef();
    }
    else
    {
        CComPtr<ISubPicProviderEx> pSubPicProvider;
        if(SUCCEEDED(GetSubPicProviderEx(&pSubPicProvider)) && pSubPicProvider
            && SUCCEEDED(pSubPicProvider->Lock()))
        {
            double fps = m_fps;

            if(POSITION pos = pSubPicProvider->GetStartPosition(rtNow, fps))
            {
                REFERENCE_TIME rtStart = pSubPicProvider->GetStart(pos, fps);
                REFERENCE_TIME rtStop = pSubPicProvider->GetStop(pos, fps);

                if(rtStart <= rtNow && rtNow < rtStop)
                {
                    if(m_pAllocator->IsDynamicWriteOnly())
                    {
                        CComPtr<ISubPicEx> pStatic;
                        if(SUCCEEDED(m_pAllocator->GetStaticEx(&pStatic))
                            && SUCCEEDED(RenderTo(pStatic, rtNow, rtNow+1, fps, true))
                            && SUCCEEDED(pStatic->CopyTo(pSubPic)))
                            (*ppSubPic = pSubPic)->AddRef();
                    }
                    else
                    {
                        if(SUCCEEDED(RenderTo(m_pSubPic, rtNow, rtNow+1, fps, true)))
                            (*ppSubPic = pSubPic)->AddRef();
                    }
                }
            }

            pSubPicProvider->Unlock();

            if(*ppSubPic)
            {
                CAutoLock cAutoLock(&m_csLock);

                m_pSubPic = *ppSubPic;
            }
        }
    }
    return(!!*ppSubPic);
}

HRESULT SimpleSubPicProvider::GetSubPicProviderEx( ISubPicProviderEx** pSubPicProviderEx )
{
    if(!pSubPicProviderEx) {
        return E_POINTER;
    }

    CAutoLock cAutoLock(&m_csSubPicProvider);

    if(m_pSubPicProviderEx) {
        *pSubPicProviderEx = m_pSubPicProviderEx;
        (*pSubPicProviderEx)->AddRef();
    }

    return !!*pSubPicProviderEx ? S_OK : E_FAIL;
}

HRESULT SimpleSubPicProvider::RenderTo( ISubPicEx* pSubPic, REFERENCE_TIME rtStart, REFERENCE_TIME rtStop, double fps, BOOL bIsAnimated )
{
    HRESULT hr = E_FAIL;

    if(!pSubPic) {
        return hr;
    }

    CComPtr<ISubPicProviderEx> pSubPicProviderEx;
    if(FAILED(GetSubPicProviderEx(&pSubPicProviderEx)) || !pSubPicProviderEx) {
        return hr;
    }

    if(FAILED(pSubPicProviderEx->Lock())) {
        return hr;
    }

    SubPicDesc spd;
    if(SUCCEEDED(pSubPic->Lock(spd)))
    {
        DWORD color = 0xFF000000;
        if(SUCCEEDED(pSubPic->ClearDirtyRect(color)))
        {
            CAtlList<CRect> rectList;

            hr = pSubPicProviderEx->RenderEx(spd, bIsAnimated ? rtStart : ((rtStart+rtStop)/2), fps, rectList);

            POSITION pos = pSubPicProviderEx->GetStartPosition(rtStart, fps);

            pSubPicProviderEx->GetStartStop(pos, fps, rtStart, rtStop);

            pSubPic->SetStart(rtStart);
            pSubPic->SetStop(rtStop);

            pSubPic->Unlock(&rectList);
        }
    }

    pSubPicProviderEx->Unlock();

    return hr;
}

//////////////////////////////////////////////////////////////////////////
//
// SimpleSubPicProvider2
//

SimpleSubPicProvider2::SimpleSubPicProvider2( ISubPicExAllocator* pAllocator, HRESULT* phr, const int *prefered_colortype/*=NULL*/, int prefered_colortype_num/*=0*/ )
    : CUnknown(NAME("SimpleSubPicProvider2"), NULL)
    , m_ex_provider(pAllocator, phr)
    , m_old_provider(pAllocator, phr, prefered_colortype, prefered_colortype_num)
{
    m_cur_provider = &m_ex_provider;
}

SimpleSubPicProvider2::~SimpleSubPicProvider2()
{

}

STDMETHODIMP SimpleSubPicProvider2::NonDelegatingQueryInterface( REFIID riid, void** ppv )
{
    return
        QI(ISimpleSubPicProvider)
        __super::NonDelegatingQueryInterface(riid, ppv);
}

STDMETHODIMP SimpleSubPicProvider2::SetSubPicProvider( IUnknown* subpic_provider )
{
    CComQIPtr<ISubPicProviderEx> tmp = subpic_provider;
    if (tmp)
    {
        m_cur_provider = &m_ex_provider;
    }
    else
    {
        m_cur_provider = &m_old_provider;
    }
    return m_cur_provider->SetSubPicProvider(subpic_provider);
}

STDMETHODIMP SimpleSubPicProvider2::GetSubPicProvider( IUnknown** subpic_provider )
{
    return m_cur_provider->GetSubPicProvider(subpic_provider);
}

STDMETHODIMP SimpleSubPicProvider2::SetFPS( double fps )
{
    return m_cur_provider->SetFPS(fps);
}

STDMETHODIMP SimpleSubPicProvider2::SetTime( REFERENCE_TIME rtNow )
{
    return m_cur_provider->SetTime(rtNow);
}

STDMETHODIMP SimpleSubPicProvider2::Invalidate( REFERENCE_TIME rtInvalidate /*= -1*/ )
{
    return m_cur_provider->Invalidate(rtInvalidate);
}

STDMETHODIMP_(bool) SimpleSubPicProvider2::LookupSubPic( REFERENCE_TIME now /*[in]*/, ISimpleSubPic** output_subpic/*[out]*/ )
{
    return m_cur_provider->LookupSubPic(now, output_subpic);
}

STDMETHODIMP SimpleSubPicProvider2::GetStats( int& nSubPics, REFERENCE_TIME& rtNow, REFERENCE_TIME& rtStart, REFERENCE_TIME& rtStop )
{
    return m_cur_provider->GetStats(nSubPics, rtNow, rtStart, rtStop);
}

STDMETHODIMP SimpleSubPicProvider2::GetStats( int nSubPic, REFERENCE_TIME& rtStart, REFERENCE_TIME& rtStop )
{
    return m_cur_provider->GetStats(nSubPic, rtStart, rtStop);
}
