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

void XyBitmap::AlphaBlt( SubPicDesc& spd, const XyBitmap& bitmap )
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

        const BYTE*  src_A1 = src_A;
        for (int i=0;i<h;i++, src_A += bitmap.pitch, dst_A += spd.pitch)
        {
            for (int j=0;j<w;j++)
            {
                dst_A[j] = (dst_A[j]*(src_A[j]+1))>>8;
            }
        }

        src_A = src_A1;
        for (int i=0;i<h;i++, src_A += bitmap.pitch, src_Y += bitmap.pitch, dst_Y += spd.pitch)
        {
            for (int j=0;j<w;j++)
            {
                dst_Y[j] = ((dst_Y[j]*(src_A[j]+1))>>8) + src_Y[j];
            }
        }
        src_A = src_A1;
        for (int i=0;i<h;i++, src_A += bitmap.pitch, src_U += bitmap.pitch, dst_U += spd.pitch)
        {
            for (int j=0;j<w;j++)
            {
                dst_U[j] = ((dst_U[j]*(src_A[j]+1))>>8) + src_U[j];
            }
        }
        src_A = src_A1;
        for (int i=0;i<h;i++, src_A += bitmap.pitch, src_V += bitmap.pitch, dst_V += spd.pitch)
        {
            for (int j=0;j<w;j++)
            {
                dst_V[j] = ((dst_V[j]*(src_A[j]+1))>>8) + src_V[j];
            }
        }
    }
    else if (bitmap.type==PACK)
    {
        const BYTE* src = bitmap.plans[0] + y_src*bitmap.pitch + x_src*4;

        for(int i=0;i<h;i++, src += bitmap.pitch, dst += spd.pitch)
        {
            const BYTE* s2 = src;
            const BYTE* s2end = s2 + w*4;
            DWORD* d2 = (DWORD*)dst;
            for(; s2 < s2end; s2 += 4, d2++)
            {
                int tmp = s2[3]+1;
                *d2 = (((((*d2&0x00ff00ff)*tmp)>>8) + (*((DWORD*)s2)&0x00ff00ff))&0x00ff00ff)
                    | (((((*d2&0x0000ff00)*tmp)>>8) + (*((DWORD*)s2)&0x0000ff00))&0x0000ff00);
            }
        }
    }
}
