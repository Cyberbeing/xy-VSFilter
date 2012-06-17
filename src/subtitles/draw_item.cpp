#include "stdafx.h"
#include "draw_item.h"

CRect DrawItem::DryDraw( SubPicDesc& spd, DrawItem& draw_item )
{
    return Rasterizer::DryDraw(spd, draw_item.overlay, draw_item.clip_rect, NULL,
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

DrawItem* DrawItem::CreateDrawItem( SubPicDesc& spd, const SharedPtrOverlay& overlay, const CRect& clipRect,
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
