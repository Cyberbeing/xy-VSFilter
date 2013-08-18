#ifndef __DRAW_ITEM_21D18040_C396_4CA5_BFCE_5616A63F2C56_H__
#define __DRAW_ITEM_21D18040_C396_4CA5_BFCE_5616A63F2C56_H__

#include <WTypes.h>
#include "RTS.h"
#include <atlcoll.h>
#include <boost/shared_ptr.hpp>
#include "xy_bitmap.h"

class OverlayPaintMachine;
typedef ::boost::shared_ptr<OverlayPaintMachine> SharedPtrOverlayPaintMachine;

class CClipperPaintMachine;
typedef ::boost::shared_ptr<CClipperPaintMachine> SharedPtrCClipperPaintMachine;

class DrawItemHashKey;
typedef ::boost::shared_ptr<DrawItemHashKey> SharedPtrDrawItemHashKey;

struct DrawItem
{
public:
    SharedPtrOverlayPaintMachine  overlay_paint_machine;
    CRect clip_rect;
    SharedPtrCClipperPaintMachine clipper;
    int   xsub;
    int   ysub;
    DWORD switchpts[6];
    bool  fBody;
    bool  fBorder;

    bool  use_addition_draw;

    SharedPtrDrawItemHashKey m_key;
public:
    CRectCoor2 GetDirtyRect();
    static CRectCoor2 Draw        ( XyBitmap *bitmap, DrawItem& draw_item, const CRectCoor2& clip_rect );
    static CRectCoor2 AlphaBltDraw( XyBitmap *bitmap, DrawItem& draw_item, const CRectCoor2& clip_rect );
    static CRectCoor2 AdditionDraw( XyBitmap *bitmap, DrawItem& draw_item, const CRectCoor2& clip_rect );
    const SharedPtrDrawItemHashKey& GetHashKey();

    static DrawItem* CreateDrawItem(const SharedPtrOverlayPaintMachine& overlay_paint_machine,
        const CRect& clipRect,
        const SharedPtrCClipperPaintMachine &clipper,
        int xsub, int ysub,
        const DWORD* switchpts, bool fBody, bool fBorder);
};

typedef ::boost::shared_ptr<DrawItem> SharedPtrDrawItem;

typedef CAtlList<DrawItem> DrawItemList;

typedef CAtlList<DrawItemList> DrawItemListList;


struct CompositeDrawItem;
typedef CAtlList<CompositeDrawItem> CompositeDrawItemList;
typedef CAtlList<CompositeDrawItemList> CompositeDrawItemListList;

struct CompositeDrawItem
{
public:
    SharedPtrDrawItem shadow;
    SharedPtrDrawItem outline;
    SharedPtrDrawItem body;
public:
    static CRectCoor2 GetDirtyRect( CompositeDrawItem& item );

    static void Draw(XySubRenderFrame**output, CompositeDrawItemListList& compDrawItemListList);
};

struct GroupedDrawItems
{
public:
    CAtlList<SharedPtrDrawItem> draw_item_list;
    CRectCoor2 clip_rect;
public:
    void Draw(SharedPtrXyBitmap *bitmap, int *bitmap_identity_num);

    void CreateHashKey(GroupedDrawItemsHashKey *key);
};

#endif // __DRAW_ITEM_21D18040_C396_4CA5_BFCE_5616A63F2C56_H__
