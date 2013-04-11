/************************************************************************/
/* author: xy                                                           */
/* date: 20111013                                                       */
/************************************************************************/

#pragma once

#include "ISubPic.h"

class CSubPicProviderExWrapper : public CUnknown, public ISubPicProviderEx
{
private:
    CSubPicProviderExWrapper(ISubPicProvider* const inner_provider);//disable inheriting
    
    CComPtr<ISubPicProvider> _inner_provider;
public:
    static CSubPicProviderExWrapper* GetSubPicProviderExWrapper(ISubPicProvider* const inner_provider);
    virtual ~CSubPicProviderExWrapper();

    DECLARE_IUNKNOWN;
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);

    // ISubPicProvider

    STDMETHODIMP Lock();
    STDMETHODIMP Unlock();

    STDMETHODIMP_(POSITION) GetStartPosition(REFERENCE_TIME rt, double fps);
    STDMETHODIMP_(POSITION) GetNext(POSITION pos);

    STDMETHODIMP_(REFERENCE_TIME) GetStart(POSITION pos, double fps);
    STDMETHODIMP_(REFERENCE_TIME) GetStop(POSITION pos, double fps);    

    STDMETHODIMP_(bool) IsAnimated (POSITION pos);

    STDMETHODIMP Render(SubPicDesc& spd, REFERENCE_TIME rt, double fps, RECT& bbox);
    STDMETHODIMP GetTextureSize (POSITION pos, SIZE& MaxTextureSize, SIZE& VirtualSize, POINT& VirtualTopLeft);

    // ISubPicProviderEx

    STDMETHODIMP_(VOID) GetStartStop(POSITION pos, double fps, /*out*/REFERENCE_TIME& start, /*out*/REFERENCE_TIME& stop);

    STDMETHODIMP RenderEx(SubPicDesc& spd, REFERENCE_TIME rt, double fps, CAtlList<CRect>& rectList);

    STDMETHODIMP_(bool) IsColorTypeSupported(int type);
    STDMETHODIMP_(int) SetOutputColorType(int type);//Important! May failed!
};
