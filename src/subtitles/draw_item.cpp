#include "stdafx.h"
#include "draw_item.h"
#include <boost/shared_ptr.hpp>

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

CRect DrawItem::Draw( SubPicDesc& spd, DrawItem& draw_item, const CRect& clip_rect )
{
    CRect result;
    const SharedPtrGrayImage2& alpha_mask = CClipper::GetAlphaMask(draw_item.clipper);
    const SharedPtrByte& alpha = Rasterizer::CompositeAlphaMask(spd, draw_item.overlay, draw_item.clip_rect & clip_rect, alpha_mask.get(),
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

struct GroupedDrawItems
{
    CAtlList<SharedPtrDrawItem> draw_item_list;
    CRect clip_rect;
};

//temporary data struct for the complex dirty rect splitting and draw item grouping algorithm 
struct CompositeDrawItemEx
{
    CompositeDrawItem *item;
    CAtlList<int> rect_id_list;
};


typedef CAtlList<CompositeDrawItemEx*> PCompositeDrawItemExList;
typedef CAtlArray<CompositeDrawItemEx> CompositeDrawItemExVec;
typedef CAtlArray<CompositeDrawItemExVec> CompositeDrawItemExTree;

typedef ::boost::shared_ptr<PCompositeDrawItemExList> SharedPCompositeDrawItemExList;

class XyRectEx: public CRect
{
public:
    SharedPCompositeDrawItemExList item_ex_list;

    void SetRect(const RECT& rect)
    {
        __super::operator=(rect);
    }
    void SetRect(
        _In_ int x1,
        _In_ int y1,
        _In_ int x2,
        _In_ int y2) throw()
    {
        __super::SetRect(x1,y1,x2,y2);
    }
};

typedef CAtlList<XyRectEx> XyRectExList;

void MergeRects(const XyRectExList& input, XyRectExList* output);
void CreateDrawItemExTree( CompositeDrawItemListList& input, 
    CompositeDrawItemExTree *out_draw_item_ex_tree, 
    XyRectExList *out_rect_ex_list );

void CompositeDrawItem::Draw( SubPicDesc& spd, CompositeDrawItemListList& compDrawItemListList )
{
    CompositeDrawItemExTree draw_item_ex_tree;
    XyRectExList rect_ex_list;
    CreateDrawItemExTree(compDrawItemListList, &draw_item_ex_tree, &rect_ex_list);

    XyRectExList grouped_rect_exs;
    MergeRects(rect_ex_list, &grouped_rect_exs);

    CAtlArray<GroupedDrawItems> grouped_draw_items;

    grouped_draw_items.SetCount(grouped_rect_exs.GetCount());

    POSITION pos = grouped_rect_exs.GetHeadPosition();
    for(int rect_id=0;pos;rect_id++)
    {
        XyRectEx& item = grouped_rect_exs.GetNext(pos);
        grouped_draw_items.GetAt(rect_id).clip_rect = item;
        POSITION pos_item = item.item_ex_list->GetHeadPosition();
        while(pos_item)
        {
            CompositeDrawItemEx* draw_item_ex = item.item_ex_list->GetNext(pos_item);
            if( draw_item_ex->rect_id_list.GetTail() != rect_id )//wipe out repeated item. this safe since the list has a dummy item -1
                draw_item_ex->rect_id_list.AddTail(rect_id);
        }
    }
    for (int i=0;i<draw_item_ex_tree.GetCount();i++)
    {
        CompositeDrawItemExVec &draw_item_ex_vec = draw_item_ex_tree[i];
        for (int j=0;j<draw_item_ex_vec.GetCount();j++)
        {
            CompositeDrawItemEx &draw_item_ex = draw_item_ex_vec[j];
            if (draw_item_ex.item->shadow)
            {
                pos = draw_item_ex.rect_id_list.GetHeadPosition();;
                draw_item_ex.rect_id_list.GetNext(pos);
                while(pos)
                {
                    int id = draw_item_ex.rect_id_list.GetNext(pos);
                    grouped_draw_items[id].draw_item_list.AddTail( draw_item_ex.item->shadow );
                }
            }
        }
        for (int j=0;j<draw_item_ex_vec.GetCount();j++)
        {
            CompositeDrawItemEx &draw_item_ex = draw_item_ex_vec[j];
            if (draw_item_ex.item->outline)
            {
                pos = draw_item_ex.rect_id_list.GetHeadPosition();;
                draw_item_ex.rect_id_list.GetNext(pos);
                while(pos)
                {
                    int id = draw_item_ex.rect_id_list.GetNext(pos);
                    grouped_draw_items[id].draw_item_list.AddTail( draw_item_ex.item->outline );
                }
            }
        }
        for (int j=0;j<draw_item_ex_vec.GetCount();j++)
        {
            CompositeDrawItemEx &draw_item_ex = draw_item_ex_vec[j];
            if (draw_item_ex.item->body)
            {
                pos = draw_item_ex.rect_id_list.GetHeadPosition();;
                draw_item_ex.rect_id_list.GetNext(pos);
                while(pos)
                {
                    int id = draw_item_ex.rect_id_list.GetNext(pos);
                    grouped_draw_items[id].draw_item_list.AddTail( draw_item_ex.item->body );
                }
            }            
        }
    }

    for (int i=0;i<grouped_draw_items.GetCount();i++)
    {
        GroupedDrawItems& item = grouped_draw_items[i];
        pos = item.draw_item_list.GetHeadPosition();
        while(pos)
        {
            DrawItem::Draw(spd, *item.draw_item_list.GetNext(pos), item.clip_rect);
        }
    }
}

void CreateDrawItemExTree( CompositeDrawItemListList& input,
    CompositeDrawItemExTree *out_draw_item_ex_tree, 
    XyRectExList *out_rect_ex_list )
{
    ASSERT(out_draw_item_ex_tree!=NULL && out_rect_ex_list!=NULL);

    int list_count = input.GetCount();
    out_draw_item_ex_tree->SetCount(list_count);
    POSITION list_pos = input.GetHeadPosition();
    for (int list_id=0;list_id<list_count;list_id++)
    {
        CompositeDrawItemList& compDrawItemList = input.GetNext(list_pos);
        int count = compDrawItemList.GetCount();
        CompositeDrawItemExVec& out_draw_item_exs = out_draw_item_ex_tree->GetAt(list_id);
        out_draw_item_exs.SetCount(count);
        POSITION item_pos = compDrawItemList.GetHeadPosition();
        for (int item_id=0; item_id<count; item_id++ )
        {
            CompositeDrawItem& comp_draw_item = compDrawItemList.GetNext(item_pos);            
            CompositeDrawItemEx& comp_draw_item_ex = out_draw_item_exs.GetAt(item_id);
            comp_draw_item_ex.item = &comp_draw_item;
            comp_draw_item_ex.rect_id_list.AddHead(-1);//dummy head

            XyRectEx &rect_ex = out_rect_ex_list->GetAt(out_rect_ex_list->AddTail());
            rect_ex.item_ex_list.reset(new PCompositeDrawItemExList());
            rect_ex.item_ex_list->AddTail( &comp_draw_item_ex);
            rect_ex.SetRect( CompositeDrawItem::GetDirtyRect(comp_draw_item) );
        }
    }
}

void MergeRects(const XyRectExList& input, XyRectExList* output)
{
    int input_count = input.GetCount();
    if(output==NULL || input_count==0)
        return;

    XyRectEx& result = output->GetAt(output->AddTail());
    result.item_ex_list.reset(new PCompositeDrawItemExList());
    POSITION pos = input.GetHeadPosition();
    while(pos)
    {
        const XyRectEx& item = input.GetNext(pos);
        result.SetRect(result | item);
        result.item_ex_list->AddTailList(item.item_ex_list.get());
    }
}
