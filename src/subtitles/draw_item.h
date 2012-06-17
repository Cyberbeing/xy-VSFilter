#ifndef __DRAW_ITEM_21D18040_C396_4CA5_BFCE_5616A63F2C56_H__
#define __DRAW_ITEM_21D18040_C396_4CA5_BFCE_5616A63F2C56_H__

#include <WTypes.h>
#include "RTS.h"

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
    static CRect DryDraw(SubPicDesc& spd, DrawItem& draw_item);
    static CRect Draw( SubPicDesc& spd, DrawItem& draw_item );

    static DrawItem* CreateDrawItem(SubPicDesc& spd,
        const SharedPtrOverlay& overlay,
        const CRect& clipRect,
        const SharedPtrCClipper &clipper,
        int xsub, int ysub,
        const DWORD* switchpts, bool fBody, bool fBorder);
};

typedef ::boost::shared_ptr<DrawItem> SharedPtrDrawItem;

struct CompositeDrawItem
{
    SharedPtrDrawItem shadow;
    SharedPtrDrawItem outline;
    SharedPtrDrawItem body;
};


#endif // __DRAW_ITEM_21D18040_C396_4CA5_BFCE_5616A63F2C56_H__
