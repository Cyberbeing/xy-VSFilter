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
