#ifndef __DRAW_ITEM_21D18040_C396_4CA5_BFCE_5616A63F2C56_H__
#define __DRAW_ITEM_21D18040_C396_4CA5_BFCE_5616A63F2C56_H__

#include <WTypes.h>
#include "RTS.h"
#include <atlcoll.h>
#include <boost/shared_ptr.hpp>

class OverlayPaintMachine;
typedef ::boost::shared_ptr<OverlayPaintMachine> SharedPtrOverlayPaintMachine;

class CClipperPaintMachine;
typedef ::boost::shared_ptr<CClipperPaintMachine> SharedPtrCClipperPaintMachine;

struct DrawItem
{
public:
    SharedPtrOverlayPaintMachine overlay_paint_machine;
    CRect clip_rect;
    SharedPtrCClipperPaintMachine clipper;
    int xsub;
    int ysub;
    DWORD switchpts[6];
    bool fBody;
    bool fBorder;
public:
    CRect GetDirtyRect();
    static CRect Draw( XyBitmap *bitmap, DrawItem& draw_item, const CRect& clip_rect );

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
    static CRect GetDirtyRect( CompositeDrawItem& item );

    static void Draw(SubPicDesc& spd, CompositeDrawItemListList& compDrawItemListList);
};

struct GroupedDrawItems
{
public:
    CAtlList<SharedPtrDrawItem> draw_item_list;
    CRect clip_rect;
public:
    void Draw(SubPicDesc& spd);
};

#endif // __DRAW_ITEM_21D18040_C396_4CA5_BFCE_5616A63F2C56_H__
