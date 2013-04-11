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