#include "stdafx.h"
#include "SimpleSubPicWrapper.h"

//////////////////////////////////////////////////////////////////////////
//
// SimpleSubPicWrapper
// 

SimpleSubPicWrapper::SimpleSubPicWrapper( ISubPicEx * inner_obj )
    : CUnknown(NAME("SimpleSubPicWrapper"), NULL)
    , m_inner_obj(inner_obj)
{

}

SimpleSubPicWrapper::SimpleSubPicWrapper( ISubPic* inner_obj )
    : CUnknown(NAME("SimpleSubPicWrapper"), NULL)
    , m_inner_obj2(inner_obj)
{
    
}

STDMETHODIMP SimpleSubPicWrapper::NonDelegatingQueryInterface( REFIID riid, void** ppv )
{
    return
        QI(ISimpleSubPic)
        __super::NonDelegatingQueryInterface(riid, ppv);    
}

STDMETHODIMP SimpleSubPicWrapper::AlphaBlt(SubPicDesc* target)
{
    if (m_inner_obj)
    {
        CAtlList<const CRect> rect_list;
        m_inner_obj->GetDirtyRects(rect_list);

        POSITION pos = rect_list.GetHeadPosition();
        while(pos!=NULL)
        {
            const CRect& rect = rect_list.GetNext(pos);
            m_inner_obj->AlphaBlt(rect, rect, target);
        }
    }
    else if (m_inner_obj2)
    {
        CRect r;
        m_inner_obj2->GetDirtyRect(r);

        m_inner_obj2->AlphaBlt(r, r, target);
    }

    return S_OK;
}

//////////////////////////////////////////////////////////////////////////
//
// SimpleSubPicProvider
// 

SimpleSubPicProvider::SimpleSubPicProvider( ISubPicQueueEx * inner_obj )
    : CUnknown(NAME("SimpleSubPicProvider"), NULL)
    , m_inner_obj(inner_obj)
{

}

SimpleSubPicProvider::SimpleSubPicProvider( ISubPicQueue* inner_obj )
    : CUnknown(NAME("SimpleSubPicProvider"), NULL)
    , m_inner_obj2(inner_obj)
{

}

STDMETHODIMP SimpleSubPicProvider::NonDelegatingQueryInterface( REFIID riid, void** ppv )
{
    return
        QI(ISimpleSubPicProvider)
        __super::NonDelegatingQueryInterface(riid, ppv);    
}

STDMETHODIMP_(bool) SimpleSubPicProvider::LookupSubPic( REFERENCE_TIME now /*[in]*/, ISimpleSubPic** output_subpic /*[out]*/ )
{
    bool result = false;
    if (m_inner_obj)
    {
        CComPtr<ISubPicEx> subpic;
        result = m_inner_obj->LookupSubPicEx(now, &subpic);
        if (result && subpic)
        {
            (*output_subpic=new SimpleSubPicWrapper(subpic))->AddRef();
        }
    }
    else if (m_inner_obj2)
    {
        CComPtr<ISubPic> subpic;
        result = m_inner_obj2->LookupSubPic(now, &subpic);
        if (result && subpic)
        {
            (*output_subpic=new SimpleSubPicWrapper(subpic))->AddRef();
        }
    }
    return result;
}
