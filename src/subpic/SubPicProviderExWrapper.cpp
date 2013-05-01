/************************************************************************/
/* author: xy                                                           */
/* date: 20111013                                                       */
/************************************************************************/

#include "stdafx.h"
#include "SubPicProviderExWrapper.h"
#include "../DSUtil/DSUtil.h"

CSubPicProviderExWrapper* CSubPicProviderExWrapper::GetSubPicProviderExWrapper(ISubPicProvider* const inner_provider)
{
    return DEBUG_NEW CSubPicProviderExWrapper(inner_provider);
}

CSubPicProviderExWrapper::CSubPicProviderExWrapper(ISubPicProvider* const inner_provider)
    : CUnknown(NAME("CSubPicProviderExWrapper"), NULL),
    _inner_provider(inner_provider)
{
}

CSubPicProviderExWrapper::~CSubPicProviderExWrapper()
{
}


// ISubPicProvider

STDMETHODIMP CSubPicProviderExWrapper::NonDelegatingQueryInterface( REFIID riid, void** ppv )
{
    return
        QI(ISubPicProvider)
        QI(ISubPicProviderEx)
        __super::NonDelegatingQueryInterface(riid, ppv);    
}

STDMETHODIMP CSubPicProviderExWrapper::Lock()
{
    return _inner_provider->Lock();
}

STDMETHODIMP CSubPicProviderExWrapper::Unlock()
{
    return _inner_provider->Unlock();
}

STDMETHODIMP_(POSITION) CSubPicProviderExWrapper::GetStartPosition( REFERENCE_TIME rt, double fps )
{
    return _inner_provider->GetStartPosition(rt, fps);
}

STDMETHODIMP_(POSITION) CSubPicProviderExWrapper::GetNext( POSITION pos )
{
    return _inner_provider->GetNext(pos);
}

STDMETHODIMP_(REFERENCE_TIME) CSubPicProviderExWrapper::GetStart( POSITION pos, double fps )
{
    return _inner_provider->GetStart(pos, fps);
}

STDMETHODIMP_(REFERENCE_TIME) CSubPicProviderExWrapper::GetStop( POSITION pos, double fps )
{
    return _inner_provider->GetStop(pos, fps);
}

STDMETHODIMP_(bool) CSubPicProviderExWrapper::IsAnimated( POSITION pos )
{
    return _inner_provider->IsAnimated(pos);
}

STDMETHODIMP CSubPicProviderExWrapper::Render( SubPicDesc& spd, REFERENCE_TIME rt, double fps, RECT& bbox )
{
    return _inner_provider->Render( spd, rt, fps, bbox );
}

STDMETHODIMP CSubPicProviderExWrapper::GetTextureSize( POSITION pos, SIZE& MaxTextureSize, SIZE& VirtualSize, POINT& VirtualTopLeft )
{
    return _inner_provider->GetTextureSize( pos, MaxTextureSize, VirtualSize, VirtualTopLeft );
}

// ISubPicProviderEx

STDMETHODIMP_(VOID) CSubPicProviderExWrapper::GetStartStop( POSITION pos, double fps, /*out*/REFERENCE_TIME& start, /*out*/REFERENCE_TIME& stop )
{
    start = _inner_provider->GetStart(pos, fps);
    stop = _inner_provider->GetStop(pos, fps);
}

STDMETHODIMP CSubPicProviderExWrapper::RenderEx( SubPicDesc& spd, REFERENCE_TIME rt, double fps, CAtlList<CRect>& rectList )
{
    CRect cRect;
    HRESULT hr = Render(spd, rt, fps, cRect);
    if(SUCCEEDED(hr))
        rectList.AddTail(cRect);
    return hr;
}

STDMETHODIMP_(bool) CSubPicProviderExWrapper::IsColorTypeSupported( int type )
{
    return type==MSP_RGBA;
}

STDMETHODIMP_(int) CSubPicProviderExWrapper::SetOutputColorType( int type )
{
    (type);
    return MSP_RGBA;
}
