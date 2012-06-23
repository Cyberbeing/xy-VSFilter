#include "stdafx.h"
#include "xy_bitmap.h"
#include "xy_malloc.h"
#include "../SubPic/ISubPic.h"


XyBitmap::~XyBitmap()
{
    xy_free(bits);
}

XyBitmap * XyBitmap::CreateBitmap( const CRect& target_rect, MemLayout layout )
{
    XyBitmap *result = new XyBitmap();
    if (result==NULL)
    {
        ASSERT(0);
        return result;
    }
    result->type = layout;
    result->x = target_rect.left;
    result->y = target_rect.top;
    result->w = target_rect.Width();
    result->h = target_rect.Height();    
    int w16 = (result->w + 15) & ~15;
        
    switch (result->type)
    {
    case PACK:
        result->pitch = w16*4;
        result->bits = xy_malloc(4*w16*result->h, (result->x*4)&15);
        result->plans[0] = reinterpret_cast<BYTE*>(result->bits);
        break;
    case PLANNA:
        result->bits = xy_malloc(4*w16*result->h, result->x&15);
        result->pitch = w16;
        result->plans[0] = reinterpret_cast<BYTE*>(result->bits);
        result->plans[1] = result->plans[0] + result->pitch * result->h;
        result->plans[2] = result->plans[1] + result->pitch * result->h;
        result->plans[3] = result->plans[2] + result->pitch * result->h;        
        break;
    default:
        ASSERT(0);
        result->bits = NULL;
        break;
    }
    ClearBitmap(result);
    return result;
}

void XyBitmap::ClearBitmap( XyBitmap *bitmap )
{
    if (!bitmap)
        return;
    if (bitmap->type==XyBitmap::PLANNA)
    {
        memset(bitmap->plans[0], 0xFF, bitmap->h * bitmap->pitch);
        memset(bitmap->plans[1], 0, bitmap->h * bitmap->pitch * 3);//assuming the other 2 plans lied right after plan 1
    }
    else
    {
        BYTE * p = bitmap->plans[0];
        for (int i=0;i<bitmap->h;i++, p+=bitmap->pitch)
        {
            memsetd(p, 0xFF000000, bitmap->w*4);
        }        
    }
}

void XyBitmap::BitBlt( SubPicDesc& spd, const XyBitmap& bitmap )
{
    ASSERT( spd.type==MSP_AYUV_PLANAR ? bitmap.type==XyBitmap::PLANNA : bitmap.type==XyBitmap::PACK );

    CRect r(0, 0, spd.w, spd.h);

    int x = bitmap.x;
    int y = bitmap.y;
    int w = bitmap.w;
    int h = bitmap.h;
    int x_src = 0, y_src = 0;

    if(x < r.left) {x_src = r.left-x; w -= r.left-x; x = r.left;}
    if(y < r.top) {y_src = r.top-y; h -= r.top-y; y = r.top;}
    if(x+w > r.right) w = r.right-x;
    if(y+h > r.bottom) h = r.bottom-y;

    BYTE* dst = reinterpret_cast<BYTE*>(spd.bits) + spd.pitch * y + ((x*spd.bpp)>>3);

    if (bitmap.type==PLANNA)
    {
        const BYTE* src_A = bitmap.plans[0] + y_src*bitmap.pitch + x_src;
        const BYTE* src_Y = bitmap.plans[1] + y_src*bitmap.pitch + x_src;
        const BYTE* src_U = bitmap.plans[2] + y_src*bitmap.pitch + x_src;
        const BYTE* src_V = bitmap.plans[3] + y_src*bitmap.pitch + x_src;

        BYTE* dst_A = dst;
        BYTE* dst_Y = dst_A + spd.pitch*spd.h;
        BYTE* dst_U = dst_Y + spd.pitch*spd.h;
        BYTE* dst_V = dst_U + spd.pitch*spd.h;
                
        for (int i=0;i<h;i++)
        {
            memcpy(dst_A, src_A, w);
            dst_A += spd.pitch;
            src_A += bitmap.pitch;
        }
        for (int i=0;i<h;i++)
        {
            memcpy(dst_Y, src_Y, w);
            dst_Y += spd.pitch;
            src_Y += bitmap.pitch;
        }
        for (int i=0;i<h;i++)
        {
            memcpy(dst_U, src_U, w);
            dst_U += spd.pitch;
            src_U += bitmap.pitch;
        }
        for (int i=0;i<h;i++)
        {
            memcpy(dst_V, src_V, w);
            dst_V += spd.pitch;
            src_V += bitmap.pitch;
        }
    }
    else if (bitmap.type==PACK)
    {
        const BYTE* src = bitmap.plans[0] + y_src*bitmap.pitch + x_src*4;
        for (int i=0;i<h;i++)
        {
            memcpy(dst, src, 4*w);
            dst += spd.pitch;
            src += bitmap.pitch;
        }
    }
}
