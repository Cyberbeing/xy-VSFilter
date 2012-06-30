#include "stdafx.h"
#include "draw_item.h"
#include <vector>
#include <algorithm>
#include <boost/shared_ptr.hpp>
#include "xy_overlay_paint_machine.h"
#include "xy_clipper_paint_machine.h"
#include "../SubPic/ISubPic.h"
#include "xy_bitmap.h"

using namespace std;

//////////////////////////////////////////////////////////////////////////
//
// DrawItem
// 
CRect DrawItem::GetDirtyRect()
{
    CRect r;
    if(!switchpts || !fBody && !fBorder) return(r);

    ASSERT(overlay_paint_machine);
    CRect overlay_rect = overlay_paint_machine->CalcDirtyRect();

    // Remember that all subtitle coordinates are specified in 1/8 pixels
    // (x+4)>>3 rounds to nearest whole pixel.
    // ??? What is xsub, ysub, mOffsetX and mOffsetY ?
    int x = (xsub + overlay_rect.left + 4)>>3;
    int y = (ysub + overlay_rect.top + 4)>>3;
    int w = overlay_rect.Width()>>3;
    int h = overlay_rect.Height()>>3;
    r = clip_rect & CRect(x, y, x+w, y+h);
    r &= clipper->CalcDirtyRect();
    return r;
}

CRect DrawItem::Draw( XyBitmap* bitmap, DrawItem& draw_item, const CRect& clip_rect )
{
    CRect result;
    SharedPtrGrayImage2 alpha_mask;
    draw_item.clipper->Paint(&alpha_mask);
    
    SharedPtrOverlay overlay;
    ASSERT(draw_item.overlay_paint_machine);
    draw_item.overlay_paint_machine->Paint(&overlay);

    const SharedPtrByte& alpha = Rasterizer::CompositeAlphaMask(overlay, draw_item.clip_rect & clip_rect, alpha_mask.get(),
        draw_item.xsub, draw_item.ysub, draw_item.switchpts, draw_item.fBody, draw_item.fBorder,
        &result);

    Rasterizer::Draw(bitmap, overlay, result, alpha.get(),
        draw_item.xsub, draw_item.ysub, draw_item.switchpts, draw_item.fBody, draw_item.fBorder);
    return result;
}

DrawItem* DrawItem::CreateDrawItem( const SharedPtrOverlayPaintMachine& overlay_paint_machine, const CRect& clipRect,
    const SharedPtrCClipperPaintMachine &clipper, int xsub, int ysub, const DWORD* switchpts, bool fBody, bool fBorder )
{
    DrawItem* result = new DrawItem();
    result->overlay_paint_machine = overlay_paint_machine;
    result->clip_rect = clipRect;
    result->clipper = clipper;
    result->xsub = xsub;
    result->ysub = ysub;

    memcpy(result->switchpts, switchpts, sizeof(result->switchpts));
    result->fBody = fBody;
    result->fBorder = fBorder;

    result->m_key.reset( new DrawItemHashKey(*result) );
    result->m_key->UpdateHashValue();

    return result;
}

const SharedPtrDrawItemHashKey& DrawItem::GetHashKey()
{
    return m_key;
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
        result |= item.shadow->GetDirtyRect();
    }
    if (item.outline)
    {
        result |= item.outline->GetDirtyRect();
    }
    if (item.body)
    {
        result |= *item.body->GetDirtyRect();
    }
    return result;
}

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
    for (unsigned i=0;i<draw_item_ex_tree.GetCount();i++)
    {
        CompositeDrawItemExVec &draw_item_ex_vec = draw_item_ex_tree[i];
        for (unsigned j=0;j<draw_item_ex_vec.GetCount();j++)
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
        for (unsigned j=0;j<draw_item_ex_vec.GetCount();j++)
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
        for (unsigned j=0;j<draw_item_ex_vec.GetCount();j++)
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
    
    XyBitmap::MemLayout bitmap_layout = XyBitmap::PACK;
    XySubRenderFrame sub_render_frame;
    sub_render_frame.m_output_rect.SetRect(0,0,spd.w,spd.h);
    sub_render_frame.m_clip_rect = sub_render_frame.m_output_rect;
    switch(spd.type)
    {
    case MSP_AYUV_PLANAR:
        sub_render_frame.m_xy_color_space = XY_CS_AYUV_PLANAR;
        bitmap_layout = XyBitmap::PLANNA;
        break;
    case MSP_XY_AUYV:
        sub_render_frame.m_xy_color_space = XY_CS_AUYV;
        bitmap_layout = XyBitmap::PACK;
        break;
    case MSP_AYUV:
        sub_render_frame.m_xy_color_space = XY_CS_AYUV;
        bitmap_layout = XyBitmap::PACK;
        break;
    default:
        sub_render_frame.m_xy_color_space = XY_CS_ARGB;
        bitmap_layout = XyBitmap::PACK;
        break;
    }
    sub_render_frame.m_bitmaps.SetCount(grouped_draw_items.GetCount());
    sub_render_frame.m_bitmap_ids.SetCount(grouped_draw_items.GetCount());

    for (unsigned i=0;i<grouped_draw_items.GetCount();i++)
    {
        grouped_draw_items[i].Draw(bitmap_layout, &sub_render_frame.m_bitmaps.GetAt(i), &sub_render_frame.m_bitmap_ids.GetAt(i));
        XyBitmap::AlphaBlt(spd, *sub_render_frame.m_bitmaps.GetAt(i));
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

    typedef CAtlList<XyRectEx> Segment;

    struct BreakPoint
    {
        int x;
        const XyRectEx* rect;

        inline bool operator<(const BreakPoint& breakpoint ) const
        {
            return (x < breakpoint.x);
        }
    };

    std::vector<int> vertical_breakpoints(2*input_count);
    std::vector<BreakPoint> herizon_breakpoints(input_count);

    POSITION pos = input.GetHeadPosition();
    for(int i=0; i<input_count; i++)
    {
        const XyRectEx& rect = input.GetNext(pos);
        vertical_breakpoints[2*i]=rect.top;
        vertical_breakpoints[2*i+1]=rect.bottom;

        herizon_breakpoints[i].x = rect.left;
        herizon_breakpoints[i].rect = &rect;
    }

    std::sort(vertical_breakpoints.begin(), vertical_breakpoints.end());
    std::sort(herizon_breakpoints.begin(), herizon_breakpoints.end());

    CAtlArray<Segment> tempSegments;
    tempSegments.SetCount(vertical_breakpoints.size()-1);
    int prev = vertical_breakpoints[0], count = 0;
    for(unsigned ptr = 1; ptr<vertical_breakpoints.size(); ptr++)
    {
        if(vertical_breakpoints[ptr] != prev)
        {
            Segment& seg = tempSegments[count];
            seg.AddTail();
            seg.GetTail().SetRect(INT_MIN, prev, INT_MIN, vertical_breakpoints[ptr]);
            seg.GetTail().item_ex_list.reset(new PCompositeDrawItemExList());

            prev = vertical_breakpoints[ptr];
            count++;
        }
    }

    for(int i=0; i<input_count; i++)
    {
        const XyRectEx& rect = *herizon_breakpoints[i].rect;

        int start = 0, mid, end = count;

        while(start<end)
        {
            mid = (start+end)>>1;
            if(tempSegments[mid].GetTail().top < rect.top)
            {
                start = mid+1;
            }
            else
            {
                end = mid;
            }
        }
        for(; start < count; start++)
        {
            CAtlList<XyRectEx>& cur_line = tempSegments[start];
            XyRectEx & item = cur_line.GetTail();
            if (item.top >= rect.bottom)
            {
                break;
            }
            if (item.right<rect.left)
            {
                cur_line.AddTail();
                XyRectEx & new_item = cur_line.GetTail();
                new_item.SetRect( rect.left, item.top, rect.right, item.bottom );
                new_item.item_ex_list.reset(new PCompositeDrawItemExList());
                new_item.item_ex_list->AddTailList( rect.item_ex_list.get() );
            }
            else
            {
                if (item.right<rect.right)
                {
                    item.right = rect.right;
                }
                item.item_ex_list->AddTailList( rect.item_ex_list.get() );
            }
        }
    }

    for (int i=count-1;i>0;i--)
    {
        CAtlList<XyRectEx>& cur_line = tempSegments[i];
        CAtlList<XyRectEx>& upper_line = tempSegments[i-1];

        POSITION pos_cur_line = cur_line.GetTailPosition();
        XyRectEx *cur_rect = &cur_line.GetPrev(pos_cur_line);
        if(cur_rect->top == upper_line.GetTail().bottom)
        {
            POSITION pos = upper_line.GetTailPosition();                          
            while(pos)
            {
                XyRectEx& upper_rect = upper_line.GetPrev(pos);
                while( upper_rect.right<cur_rect->right || upper_rect.left<cur_rect->left )
                {
                    output->AddHead(*cur_rect);
                    //if(!cur_line.IsEmpty())
                    cur_rect = &cur_line.GetPrev(pos_cur_line);
                }
                if(!pos_cur_line)
                    break;

                if(upper_rect.right==cur_rect->right && upper_rect.left==cur_rect->left)
                {
                    upper_rect.bottom = cur_rect->bottom;
                    upper_rect.item_ex_list->AddTailList( cur_rect->item_ex_list.get() );
                    //if(!cur_line.IsEmpty())
                    cur_rect = &cur_line.GetPrev(pos_cur_line);
                }
                //else if ( upper_rect.right>cur_rect.right || upper_rect.left>cur_rect.left )             
            }
        }
        while(pos_cur_line)
        {
            output->AddHead(*cur_rect);
            cur_rect = &cur_line.GetPrev(pos_cur_line);
        }
    }
    if(count>0)
    {
        CAtlList<XyRectEx>& cur_line = tempSegments[0];
        POSITION pos_cur_line = cur_line.GetTailPosition();
        XyRectEx *cur_rect = &cur_line.GetPrev(pos_cur_line);
        while(pos_cur_line)
        {
            output->AddHead(*cur_rect);
            cur_rect = &cur_line.GetPrev(pos_cur_line);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//
// GroupedDrawItems
// 

void GroupedDrawItems::Draw( XyBitmap::MemLayout bitmap_layout, SharedPtrXyBitmap *bitmap, int *bitmap_identity_num )
{
    static int id=0;

    ASSERT(bitmap && bitmap_identity_num);
    BitmapMruCache *bitmap_cache = CacheManager::GetBitmapMruCache();
    GroupedDrawItemsHashKey key = GetHashKey();
    POSITION pos = bitmap_cache->Lookup(key);
    if (pos==NULL)
    {
        POSITION pos = draw_item_list.GetHeadPosition();
        XyBitmap *tmp = XyBitmap::CreateBitmap(clip_rect, bitmap_layout );
        bitmap->reset(tmp);
        while(pos)
        {
            DrawItem::Draw(tmp, *draw_item_list.GetNext(pos), clip_rect);
        }
        bitmap_cache->UpdateCache(key, *bitmap);
    }
    else
    {
        *bitmap = bitmap_cache->GetAt(pos);
        bitmap_cache->UpdateCache(pos);
    }
    *bitmap_identity_num  = id++;//fix me: not really support id yet
}

GroupedDrawItemsHashKey GroupedDrawItems::GetHashKey()
{
    GroupedDrawItemsHashKey key;
    key.m_clip_rect = clip_rect;
    GroupedDrawItemsHashKey::Keys *inner_key = new GroupedDrawItemsHashKey::Keys();
    ASSERT(inner_key);
    key.m_key.reset(inner_key);
    inner_key->SetCount(draw_item_list.GetCount());
    POSITION pos = draw_item_list.GetHeadPosition();
    for (unsigned i=0;i<inner_key->GetCount();i++)
    {
        inner_key->GetAt(i) = draw_item_list.GetNext(pos)->GetHashKey();
    }
    key.UpdateHashValue();
    return key;
}

