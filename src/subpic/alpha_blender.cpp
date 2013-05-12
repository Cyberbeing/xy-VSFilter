/*
*	Copyright (C) 2003-2006 Gabest
*	http://www.gabest.org
*
*  This Program is free software; you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation; either version 2, or (at your option)
*  any later version.
*
*  This Program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with GNU Make; see the file COPYING.  If not, write to
*  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
*  http://www.gnu.org/copyleft/gpl.html
*
*/

#include "stdafx.h"
#include "alpha_blender.h"
#include "MemSubPic.h"
#include <boost/scoped_ptr.hpp>

BaseAlphaBlender* BaseAlphaBlender::s_default_alpha_blender = NULL;

BaseAlphaBlender* BaseAlphaBlender::SetupDefaultAlphaBlender( int src_type, int dst_type, int cpu_flags )
{
    static ::boost::scoped_ptr<BaseAlphaBlender> s_auto_cleaner;
    s_default_alpha_blender = CreateAlphaBlender(src_type, dst_type, cpu_flags);
    s_auto_cleaner.reset(s_default_alpha_blender);
    return s_default_alpha_blender;
}

BaseAlphaBlender* BaseAlphaBlender::CreateAlphaBlender( int src_type, int dst_type, int cpu_flags )
{
    if (src_type==MSP_RGBA && dst_type==MSP_RGB32)
    {
        return DEBUG_NEW AlphaBlenderArgbC();
    }
    return NULL;
}


void BaseAlphaBlender::AlphaBlend( const XySubPicPlan src_plans[], CRect src_rect, const XySubPicPlan dst_plans[], CPoint dst_pos )
{
    ASSERT(src_plans&&dst_plans);
    ClipRect(src_plans[0].w, src_plans[0].h, dst_plans[0].w, abs(dst_plans[0].h), &src_rect, &dst_pos);
    if (src_rect.Width()<=0 || src_rect.Height()<=0)
    {
        return;
    }
    AlphaBlendNoClip(src_plans, src_rect, dst_plans, dst_pos);
}

void BaseAlphaBlender::AlphaBlend(const XySubPicPlan src_plans[], const XySubPicPlan dst_plans[], POINT dst_pos)
{
    ASSERT(src_plans&&dst_plans);
    CRect src_rect(0,0,src_plans[0].w, src_plans[0].h);
    if (dst_pos.x < 0)
    {
        dst_pos.x = 0;
    }
    if (dst_pos.y < 0)
    {
        dst_pos.y = 0;
    }
    LimitPoint(dst_plans[0].w-dst_pos.x, abs(dst_plans[0].h)-dst_pos.y, src_rect.BottomRight());
    if (src_rect.Width()<=0 || src_rect.Height()<=0)
    {
        return;
    }
    AlphaBlendNoClip(src_plans, src_rect, dst_plans, dst_pos);
}

void BaseAlphaBlender::AlphaBlend(const XySubPicPlan src_plans[], const XySubPicPlan dst_plans[], POINT dst_pos, SIZE size)
{
    ASSERT(src_plans&&dst_plans);
    int dst_limit_w = dst_plans[0].w-dst_pos.x;
    if (size.cx > dst_limit_w)
    {
        size.cx = dst_limit_w;
    }
    if (size.cx > src_plans[0].w)
    {
        size.cx = src_plans[0].w;
    }
    int dst_limit_h = abs(dst_plans[0].h)-dst_pos.y;
    if (size.cy > dst_limit_h)
    {
        size.cy = dst_limit_h;
    }
    if (size.cy > src_plans[0].h)
    {
        size.cy = src_plans[0].h;
    }
    CRect src_rect(0,0,size.cx,size.cy);
    if (dst_pos.x < 0)
    {
        src_rect.left -= dst_pos.x;
        dst_pos.x = 0;
    }
    if (dst_pos.y < 0)
    {
        src_rect.top -= dst_pos.y;
        dst_pos.y = 0;
    }
    if (src_rect.Width()<=0 || src_rect.Height()<=0)
    {
        return;
    }
    AlphaBlendNoClip(src_plans, src_rect, dst_plans, dst_pos);
}

void BaseAlphaBlender::ClipTopLeftToZero( CPoint *src_point, CPoint *dst_point )
{
    if (src_point->x < 0)
    {
        dst_point->x -= src_point->x;
        src_point->x = 0;
    }
    if (src_point->y < 0)
    {
        dst_point->y -= src_point->y;
        src_point->y = 0;
    }
    if (dst_point->x < 0)
    {
        src_point->x -= dst_point->x;
        dst_point->x = 0;
    }
    if (dst_point->y < 0)
    {
        src_point->y -= dst_point->y;
        dst_point->y = 0;
    }
}

void BaseAlphaBlender::LimitPoint( int limit_x, int limit_y, CPoint &p )
{
    if (p.x > limit_x)
    {
        p.x = limit_x;
    }
    if (p.y > limit_y)
    {
        p.y = limit_y;
    }
}

void BaseAlphaBlender::ClipRect( int src_w, int src_h, int dst_w, int dst_h, CRect *src_rect, CPoint *dst_point )
{
    ASSERT(src_rect&&dst_point);
    ClipTopLeftToZero(&(src_rect->TopLeft()), dst_point);
    LimitPoint(src_w, src_h, src_rect->BottomRight());
    LimitPoint(dst_w-dst_point->x+src_rect->left, dst_h-dst_point->y+src_rect->top
        , src_rect->BottomRight());
    if (src_rect->Width()<0)
    {
        src_rect->right = src_rect->left;
    }
    if (src_rect->Height()<0)
    {
        src_rect->bottom = src_rect->top;
    }
}

//
// AlphaBlenderArgbC
//
void AlphaBlenderArgbC::AlphaBlendNoClip( const XySubPicPlan src_plans[], const CRect& src_rect
    , const XySubPicPlan dst_plans[], CPoint dst_pos )
{
    const XySubPicPlan& src = src_plans[0];
    const XySubPicPlan& dst = dst_plans[0]; // copy, because we might modify it

    int w = src_rect.Width(), h = src_rect.Height();
    const BYTE *s = (BYTE*)src.bits + src.pitch * src_rect.top + src_rect.left*4;
    BYTE       *d = (BYTE*)dst.bits + dst_pos.x*4;
    int dst_pitch = dst.pitch;
    if (dst.h > 0)
    {
        d += dst_pitch * dst_pos.y;
    }
    else
    {
        int dh = -dst.h;
        d += dst_pitch*(dh - dst_pos.y - 1);
        dst_pitch = -dst_pitch;
    }
    AlphaBlend(s, w, h, src.pitch, d, dst_pitch);
}

void AlphaBlenderArgbC::AlphaBlend( const BYTE *src, int w, int h, int pitch, BYTE *dst, int dst_pitch )
{
    for(int i = 0; i < h; i++, src += pitch, dst += dst_pitch)
    {
        const BYTE* s2 = src;
        const BYTE* s2end = s2 + w*4;
        DWORD* d2 = (DWORD*)dst;
        for(; s2 < s2end; s2 += 4, d2++)
        {
            if(s2[3] < 0xff)
            {
                *d2 = (((((*d2&0x00ff00ff)*s2[3])>>8) + (*((DWORD*)s2)&0x00ff00ff))&0x00ff00ff)
                    | (((((*d2&0x0000ff00)*s2[3])>>8) + (*((DWORD*)s2)&0x0000ff00))&0x0000ff00);
            }
        }
    }
}
