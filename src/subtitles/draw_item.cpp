#include "stdafx.h"
#include "draw_item.h"
#include <vector>
#include <algorithm>
#include <boost/shared_ptr.hpp>
#include "xy_overlay_paint_machine.h"
#include "xy_clipper_paint_machine.h"
#include "../SubPic/ISubPic.h"
#include "xy_bitmap.h"

#if ENABLE_XY_LOG_TRACE_DRAW
#  define TRACE_DRAW(msg) XY_LOG_TRACE(msg)
#else
#  define TRACE_DRAW(msg)
#endif

using namespace std;

//////////////////////////////////////////////////////////////////////////
//
// DrawItem
// 
CRectCoor2 DrawItem::GetDirtyRect()
{
    CRectCoor2 r;
    if(!switchpts || !fBody && !fBorder) return(r);

    ASSERT(overlay_paint_machine);
    CRectCoor2 overlay_rect = overlay_paint_machine->CalcDirtyRect();

    // Remember that all subtitle coordinates are specified in 1/8 pixels
    // (x+4)>>3 rounds to nearest whole pixel.
    // ??? What is xsub, ysub, mOffsetX and mOffsetY ?
    int x = (xsub + overlay_rect.left + 4)>>3;
    int y = (ysub + overlay_rect.top + 4)>>3;
    int w = overlay_rect.Width()>>3;
    int h = overlay_rect.Height()>>3;
    r = clip_rect & CRect(x, y, x+w, y+h);
    r &= clipper->CalcDirtyRect();

    //expand the rect, so that it is possible to perform chroma sub-sampling
    r.left &= ~1;
    r.right = (r.right + 1)&~1;
    r.top &= ~1;
    r.bottom = (r.bottom + 1)&~1;
    return r;
}

CRectCoor2 DrawItem::Draw( XyBitmap* bitmap, DrawItem& draw_item, const CRectCoor2& clip_rect )
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

CRectCoor2 DrawItem::AdditionDraw( XyBitmap *bitmap, DrawItem& draw_item, const CRectCoor2& clip_rect )
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

    Rasterizer::AdditionDraw(bitmap, overlay, result, alpha.get(),
        draw_item.xsub, draw_item.ysub, draw_item.switchpts, draw_item.fBody, draw_item.fBorder);
    return result;
}

DrawItem* DrawItem::CreateDrawItem( const SharedPtrOverlayPaintMachine& overlay_paint_machine, const CRect& clipRect,
    const SharedPtrCClipperPaintMachine &clipper, int xsub, int ysub, const DWORD* switchpts, bool fBody, bool fBorder )
{
    DrawItem* result              = DEBUG_NEW DrawItem();
    result->overlay_paint_machine = overlay_paint_machine;
    result->clip_rect             = clipRect;
    result->clipper               = clipper;
    result->xsub                  = xsub;
    result->ysub                  = ysub;

    memcpy(result->switchpts, switchpts, sizeof(result->switchpts));
    result->fBody   = fBody;
    result->fBorder = fBorder;

    result->m_key.reset( DEBUG_NEW DrawItemHashKey(*result) );
    result->m_key->UpdateHashValue();

    return result;
}

const SharedPtrDrawItemHashKey& DrawItem::GetHashKey()
{
    return m_key;
}

bool DrawItem::CheckOverlap( const DrawItem& a, const DrawItem& b )
{
    return a.fBody==true || b.fBorder==true || 
        OverlayPaintMachine::CheckOverlap(*a.overlay_paint_machine, *b.overlay_paint_machine);
}

//////////////////////////////////////////////////////////////////////////
//
// CompositeDrawItem
// 

CRectCoor2 CompositeDrawItem::GetDirtyRect( CompositeDrawItem& item )
{
    CRectCoor2 result;
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

void CompositeDrawItem::Draw( XySubRenderFrame**output, CompositeDrawItemListList& compDrawItemListList )
{
    if (!output)
    {
        return;
    }
    *output = NULL;

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

    XySubRenderFrameCreater *render_frame_creater = XySubRenderFrameCreater::GetDefaultCreater();
    
    *output = render_frame_creater->NewXySubRenderFrame(grouped_draw_items.GetCount());
    XySubRenderFrame& sub_render_frame = **output;

    for (unsigned i=0;i<grouped_draw_items.GetCount();i++)
    {
        grouped_draw_items[i].Draw(&(sub_render_frame.m_bitmaps.GetAt(i)), &(sub_render_frame.m_bitmap_ids.GetAt(i)));
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
            rect_ex.item_ex_list.reset(DEBUG_NEW PCompositeDrawItemExList());
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
            seg.GetTail().item_ex_list.reset(DEBUG_NEW PCompositeDrawItemExList());

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
                new_item.item_ex_list.reset(DEBUG_NEW PCompositeDrawItemExList());
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

void GroupedDrawItems::Draw( SharedPtrXyBitmap *bitmap, int *bitmap_identity_num )
{
    ASSERT(bitmap && bitmap_identity_num);
    BitmapMruCache *bitmap_cache = CacheManager::GetBitmapMruCache();
    GroupedDrawItemsHashKey *key = DEBUG_NEW GroupedDrawItemsHashKey();
    CreateHashKey(key);
    XyFwGroupedDrawItemsHashKey::IdType key_id = XyFwGroupedDrawItemsHashKey(key).GetId();
    POSITION pos = bitmap_cache->Lookup( key_id );
    if (pos==NULL)
    {
        POSITION pos = draw_item_list.GetHeadPosition();
        XyBitmap *tmp = XySubRenderFrameCreater::GetDefaultCreater()->CreateBitmap(clip_rect);
        bitmap->reset(tmp);

        bool have_overlap_region = CheckOverlap();
        if (have_overlap_region)
        {
            while(pos)
            {
                DrawItem::Draw(tmp, *draw_item_list.GetNext(pos), clip_rect);
            }
        }
        else
        {
            TRACE_DRAW("AdditionDraw");
            XyBitmap::FlipAlphaValue(tmp->bits, tmp->w, tmp->h, tmp->pitch);
            while(pos)
            {
                DrawItem::AdditionDraw(tmp, *draw_item_list.GetNext(pos), clip_rect);
            }
        }
        XyColorSpace color_space;
        HRESULT hr = XySubRenderFrameCreater::GetDefaultCreater()->GetColorSpace(&color_space);
        ASSERT(SUCCEEDED(hr));
        if (color_space==XY_CS_ARGB_F && have_overlap_region)
            XyBitmap::FlipAlphaValue(tmp->bits, tmp->w, tmp->h, tmp->pitch);
#if XY_DBG_SHOW_BITMAP_BOUNDARY
        if (color_space==XY_CS_ARGB_F||color_space==XY_CS_ARGB)//not support plannar format yet
        {
            if (tmp->w>0 && tmp->h>0)
            {
                DWORD color = color_space==XY_CS_ARGB_F ? 0xff00ff00 : 0x0000ff00;
                BYTE *pixel = (BYTE *)tmp->bits;
                memsetd(pixel, color, tmp->w*4);
                int i=1;
                for (;i<tmp->h-1;i++)
                {
                    pixel += tmp->pitch;
                    *(DWORD*)pixel = color;
                    *((DWORD*)pixel+tmp->w-1) = color;
                }
                if (i<tmp->h)
                {
                    pixel += tmp->pitch;
                    memsetd(pixel, color, tmp->w*4);
                }
            }

        }
#endif
        bitmap_cache->UpdateCache(key_id, *bitmap);
    }
    else
    {
        *bitmap = bitmap_cache->GetAt(pos);
        bitmap_cache->UpdateCache(pos);
    }
    *bitmap_identity_num  = key_id;
}

bool GroupedDrawItems::CheckOverlap()
{
    if (draw_item_list.GetCount()==2)
    {
        POSITION pos =  draw_item_list.GetHeadPosition();
        DrawItem &a  = *draw_item_list.GetNext(pos);
        DrawItem &b  = *draw_item_list.GetNext(pos);
        return DrawItem::CheckOverlap(a, b);
    }
    return true;
}

void GroupedDrawItems::CreateHashKey(GroupedDrawItemsHashKey *key)
{
    ASSERT(key);
    key->m_clip_rect = clip_rect;
    GroupedDrawItemsHashKey::Keys *inner_key = DEBUG_NEW GroupedDrawItemsHashKey::Keys();
    ASSERT(inner_key);
    key->m_key.reset(inner_key);
    inner_key->SetCount(draw_item_list.GetCount());
    POSITION pos = draw_item_list.GetHeadPosition();
    for (unsigned i=0;i<inner_key->GetCount();i++)
    {
        inner_key->GetAt(i) = draw_item_list.GetNext(pos)->GetHashKey();
    }
    key->UpdateHashValue();
}
