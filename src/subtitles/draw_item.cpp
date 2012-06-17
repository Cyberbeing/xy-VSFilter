#include "stdafx.h"
#include "draw_item.h"
#include <vector>

using namespace std;

//////////////////////////////////////////////////////////////////////////
//
// DrawItem
// 
CRect DrawItem::GetDirtyRect( DrawItem& draw_item )
{
    //fix me: intersect with clipper rect 
    return Rasterizer::DryDraw(draw_item.overlay, draw_item.clip_rect, 
        draw_item.xsub, draw_item.ysub, draw_item.switchpts, draw_item.fBody, draw_item.fBorder);
}

CRect DrawItem::Draw( SubPicDesc& spd, DrawItem& draw_item )
{
    CRect result;
    const SharedPtrGrayImage2& alpha_mask = CClipper::GetAlphaMask(draw_item.clipper);
    const SharedPtrByte& alpha = Rasterizer::CompositeAlphaMask(spd, draw_item.overlay, draw_item.clip_rect, alpha_mask.get(),
        draw_item.xsub, draw_item.ysub, draw_item.switchpts, draw_item.fBody, draw_item.fBorder,
        &result);
    Rasterizer::Draw(spd, draw_item.overlay, result, alpha.get(),
        draw_item.xsub, draw_item.ysub, draw_item.switchpts, draw_item.fBody, draw_item.fBorder);
    return result;
}

DrawItem* DrawItem::CreateDrawItem( const SharedPtrOverlay& overlay, const CRect& clipRect,
    const SharedPtrCClipper &clipper, int xsub, int ysub, const DWORD* switchpts, bool fBody, bool fBorder )
{
    DrawItem* result = new DrawItem();
    result->overlay = overlay;
    result->clip_rect = clipRect;
    result->clipper = clipper;
    result->xsub = xsub;
    result->ysub = ysub;

    memcpy(result->switchpts, switchpts, sizeof(result->switchpts));
    result->fBody = fBody;
    result->fBorder = fBorder;
    return result;
}

//////////////////////////////////////////////////////////////////////////
//
// DrawItemEx
// 

bool cmp_draw_order(DrawItemEx* lhs, DrawItemEx* rhs)
{
    return lhs->draw_order < rhs->draw_order;
}

//////////////////////////////////////////////////////////////////////////
//
// CompositeDrawItem
// 

CRect CompositeDrawItem::GetDirtyRect( CompositeDrawItem& item )
{
    CRect result;    
    if (item.shadow)
    {
        result |= DrawItem::GetDirtyRect(*item.shadow);
    }
    if (item.outline)
    {
        result |= DrawItem::GetDirtyRect(*item.outline);
    }
    if (item.body)
    {
        result |= DrawItem::GetDirtyRect(*item.body);
    }
    return result;
}

void CompositeDrawItem::Draw( SubPicDesc& spd, CompositeDrawItemListList& compDrawItemListList )
{    
    DrawItemExList draw_item_ex_list;
    CreateDrawItemExList(compDrawItemListList, &draw_item_ex_list);

    typedef std::vector<DrawItemEx*> DrawItemExVec;
    DrawItemExVec draw_item_ex_vec( draw_item_ex_list.GetCount() );
    POSITION pos = draw_item_ex_list.GetHeadPosition();
    DrawItemExVec::iterator iter=draw_item_ex_vec.begin();
    while( iter != draw_item_ex_vec.end() )
    {
        *iter = &(draw_item_ex_list.GetNext(pos));
        iter++;
    }
    std::sort(draw_item_ex_vec.begin(), draw_item_ex_vec.end(), cmp_draw_order);

    iter=draw_item_ex_vec.begin();
    while( iter != draw_item_ex_vec.end() )
    {
        DrawItem &item = *((*iter)->item);
        DrawItem::Draw(spd, item);
		iter++;
    }
}

void CompositeDrawItem::CreateDrawItemExList( CompositeDrawItemListList& compDrawItemListList, DrawItemExList *output )
{
    ASSERT(output!=NULL);
    POSITION list_pos = compDrawItemListList.GetHeadPosition();
    int comp_item_id = 1;
    while(list_pos)
    {
        CompositeDrawItemList& compDrawItemList = compDrawItemListList.GetNext(list_pos);
        int count = compDrawItemList.GetCount();

        POSITION item_pos = compDrawItemList.GetHeadPosition();
        for ( ; item_pos; comp_item_id++ )
        {
            CompositeDrawItem& comp_draw_item = compDrawItemList.GetNext(item_pos);
            CRect dirty_rect = CompositeDrawItem::GetDirtyRect(comp_draw_item);
            if(comp_draw_item.shadow)
            {
                DrawItemEx &draw_item_ex = output->GetAt(output->AddTail());
                draw_item_ex.draw_order = comp_item_id;
                draw_item_ex.item = comp_draw_item.shadow;
                draw_item_ex.dirty_rect = dirty_rect;
            }
            if(comp_draw_item.outline)
            {
                DrawItemEx &draw_item_ex = output->GetAt(output->AddTail());
                draw_item_ex.draw_order = comp_item_id+count;
                draw_item_ex.item = comp_draw_item.outline;
                draw_item_ex.dirty_rect = dirty_rect;
            }
            if(comp_draw_item.body)
            {
                DrawItemEx &draw_item_ex = output->GetAt(output->AddTail());
                draw_item_ex.draw_order = comp_item_id+2*count;
                draw_item_ex.item = comp_draw_item.body;
                draw_item_ex.dirty_rect = dirty_rect;
            }
        }
        comp_item_id += 2*count;
    }
}
