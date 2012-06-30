#pragma once

#include "ISubPic.h"
#include "ISimpleSubPic.h"

//
// SimpleSubPicWrapper
//

class SimpleSubPicWrapper: public CUnknown, public ISimpleSubPic
{
public:
    SimpleSubPicWrapper(ISubPicEx * inner_obj);
    SimpleSubPicWrapper(ISubPic* inner_obj);

    DECLARE_IUNKNOWN;
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);

    STDMETHODIMP AlphaBlt(SubPicDesc* target);
private:
    CComPtr<ISubPicEx> m_inner_obj;
    CComPtr<ISubPic> m_inner_obj2;
};

class SimpleSubPicProvider: public CUnknown, public ISimpleSubPicProvider
{
public:
    SimpleSubPicProvider(ISubPicQueueEx * inner_obj);
    SimpleSubPicProvider(ISubPicQueue* inner_obj);
    
    DECLARE_IUNKNOWN;
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);

    STDMETHODIMP_(bool) LookupSubPic(REFERENCE_TIME now /*[in]*/, ISimpleSubPic** output_subpic/*[out]*/);
private:
    CComPtr<ISubPicQueueEx> m_inner_obj;
    CComPtr<ISubPicQueue> m_inner_obj2;
};
