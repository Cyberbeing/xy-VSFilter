#ifndef __DRAW_ITEM_21D18040_C396_4CA5_BFCE_5616A63F2C56_H__
#define __DRAW_ITEM_21D18040_C396_4CA5_BFCE_5616A63F2C56_H__

#include <WTypes.h>
#include "RTS.h"
#include <atlcoll.h>

struct DrawItem
{
public:
    SharedPtrOverlay overlay;
    CRect clip_rect;
    SharedPtrCClipper clipper;
    int xsub;
    int ysub;
    DWORD switchpts[6];
    bool fBody;
    bool fBorder;
public:
    static CRect GetDirtyRect(DrawItem& draw_item);
    static CRect Draw( SubPicDesc& spd, DrawItem& draw_item );

    static DrawItem* CreateDrawItem(const SharedPtrOverlay& overlay,
        const CRect& clipRect,
        const SharedPtrCClipper &clipper,
        int xsub, int ysub,
        const DWORD* switchpts, bool fBody, bool fBorder);
};

typedef ::boost::shared_ptr<DrawItem> SharedPtrDrawItem;

typedef CAtlList<DrawItem> DrawItemList;

struct DrawItemEx
{
    SharedPtrDrawItem item;
    CRect dirty_rect;
    int draw_order;
};

typedef CAtlList<DrawItemEx> DrawItemExList;

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
private:
    static void CreateDrawItemExList( CompositeDrawItemListList& compDrawItemListList, DrawItemExList *output );
};

#endif // __DRAW_ITEM_21D18040_C396_4CA5_BFCE_5616A63F2C56_H__
