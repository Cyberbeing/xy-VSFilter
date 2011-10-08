/*
 *  Copyright (C) 2003-2006 Gabest
 *  http://www.gabest.org
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
#include <math.h>
#include <time.h>
#include "RTS.h"
#include "cache_manager.h"
#include "../SubPic/MemSubPic.h"
#include "../subpic/color_conv_table.h"
#include "subpixel_position_controler.h"

// WARNING: this isn't very thread safe, use only one RTS a time.
static HDC g_hDC;
static int g_hDC_refcnt = 0;

enum XY_MSP_SUBTYPE {XY_AYUV, XY_AUYV};
static inline DWORD rgb2yuv(DWORD argb, XY_MSP_SUBTYPE type)
{
    const ColorConvTable* color_conv_table = ColorConvTable::GetDefaultColorConvTable();
    DWORD axxv;
    int r = (argb & 0x00ff0000) >> 16;
    int g = (argb & 0x0000ff00) >> 8;
    int b = (argb & 0x000000ff);
    int y = (color_conv_table->c2y_cyb * b + color_conv_table->c2y_cyg * g + color_conv_table->c2y_cyr * r + 0x108000) >> 16;
    int scaled_y = (y-16) * color_conv_table->cy_cy;
    int u = ((((b<<16) - scaled_y) >> 10) * color_conv_table->c2y_cu + 0x800000 + 0x8000) >> 16;
    int v = ((((r<<16) - scaled_y) >> 10) * color_conv_table->c2y_cv + 0x800000 + 0x8000) >> 16;
    DbgLog((LOG_TRACE, 5, TEXT("argb=%x r=%d %x g=%d %x b=%d %x y=%d %x u=%d %x v=%d %x"), argb, r, r, g, g, b, b, y, y, u, u, v, v));
    u *= (u>0);
    u = 255 - (255-u)*(u<256);
    v *= (v>0);
    v = 255 - (255-v)*(v<256);
    DbgLog((LOG_TRACE, 5, TEXT("u=%x v=%x"), u, v));
    if(type==XY_AYUV)
        axxv = (argb & 0xff000000) | (y<<16) | (u<<8) | v;
    else
        axxv = (argb & 0xff000000) | (y<<8) | (u<<16) | v;
    DbgLog((LOG_TRACE, 5, TEXT("axxv=%x"), axxv));
    return axxv;
    //return argb;
}
static long revcolor(long c)
{
    return ((c&0xff0000)>>16) + (c&0xff00) + ((c&0xff)<<16);
}

//////////////////////////////////////////////////////////////////////////////////////////////

// CMyFont

CMyFont::CMyFont(const FwSTSStyle& style)
{
    LOGFONT lf;
    memset(&lf, 0, sizeof(lf));
    lf <<= style.get();
    lf.lfHeight = (LONG)(style.get().fontSize+0.5);
    lf.lfOutPrecision = OUT_TT_PRECIS;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfQuality = ANTIALIASED_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH|FF_DONTCARE;
    if(!CreateFontIndirect(&lf))
    {
        _tcscpy(lf.lfFaceName, _T("Arial"));
        CreateFontIndirect(&lf);
    }
    HFONT hOldFont = SelectFont(g_hDC, *this);
    TEXTMETRIC tm;
    GetTextMetrics(g_hDC, &tm);
    m_ascent = ((tm.tmAscent + 4) >> 3);
    m_descent = ((tm.tmDescent + 4) >> 3);
    SelectFont(g_hDC, hOldFont);
}

// CWord

CWord::CWord(const FwSTSStyle& style, const CStringW& str, int ktype, int kstart, int kend)
    : m_style(style), m_str(str)
    , m_width(0), m_ascent(0), m_descent(0)
    , m_ktype(ktype), m_kstart(kstart), m_kend(kend)
    , m_fDrawn(false), m_p(INT_MAX, INT_MAX)
    , m_fLineBreak(false), m_fWhiteSpaceChar(false)
    //, m_pOpaqueBox(NULL)
{
    if(m_str.get().IsEmpty())
    {
        m_fWhiteSpaceChar = m_fLineBreak = true;
    }
    FwCMyFont font(m_style);
    m_ascent = (int)(m_style.get().fontScaleY/100*font.get().m_ascent);
    m_descent = (int)(m_style.get().fontScaleY/100*font.get().m_descent);
    m_width = 0;
}

CWord::~CWord()
{
    //if(m_pOpaqueBox) delete m_pOpaqueBox;
}

bool CWord::Append(const SharedPtrCWord& w)
{
    if(!(m_style == w->m_style)
            || m_fLineBreak || w->m_fLineBreak
            || w->m_kstart != w->m_kend || m_ktype != w->m_ktype) return(false);
    m_fWhiteSpaceChar = m_fWhiteSpaceChar && w->m_fWhiteSpaceChar;
    CStringW temp = m_str.get();
    temp += w->m_str.get();
    m_str = temp;
    m_width += w->m_width;
    m_fDrawn = false;
    m_p = CPoint(INT_MAX, INT_MAX);
    return(true);
}

void CWord::Paint( SharedPtrCWord word, const CPoint& p, const CPoint& org, OverlayList* overlay_list )
{
    if(!word->m_str.get() || overlay_list==NULL) return;

    CPoint psub = SubpixelPositionControler::GetGlobalControler().GetSubpixel(p);
    CPoint trans_org = org - p;
    OverlayKey overlay_key(*word, psub, trans_org);
    bool need_transform = word->NeedTransform();
    if(!need_transform)
    {
        overlay_key.m_org.x=0;
        overlay_key.m_org.y=0;
    }   
    OverlayMruCache* overlay_cache = CacheManager::GetOverlayMruCache();
    OverlayMruCache::hashed_cache_const_iterator iter = overlay_cache->hash_find(overlay_key);
    if(iter==overlay_cache->hash_end())    
    {
        word->DoPaint(psub, trans_org, &(overlay_list->overlay));
        OverlayMruItem item(overlay_key, overlay_list->overlay);
        CacheManager::GetOverlayMruCache()->update_cache(item);
    }
    else
    {
        overlay_list->overlay = iter->overlay;
    }    
    if(word->m_style.get().borderStyle == 1)
    {
        if(!word->CreateOpaqueBox()) return;
        overlay_list->next = new OverlayList();
        Paint(word->m_pOpaqueBox, p, org, overlay_list->next);
    }
}

void CWord::DoPaint(const CPoint& psub, const CPoint& trans_org, SharedPtrOverlay* overlay)
{
    overlay->reset(new Overlay());

    OverlayNoBlurKey overlay_no_blur_key(*this, psub, trans_org);

    OverlayNoBlurMruCache* overlay_no_blur_cache = CacheManager::GetOverlayNoBlurMruCache();
    OverlayNoBlurMruCache::hashed_cache_const_iterator iter = overlay_no_blur_cache->hash_find(overlay_no_blur_key);
    
    SharedPtrOverlay raterize_result;
    if(iter==overlay_no_blur_cache->hash_end())
    {
        raterize_result.reset(new Overlay());
        if(!m_fDrawn)
        {
            //get outline path, if not cached, create it and cache a copy, else copy from cache
            SharedPtrPathData path_data(new PathData());        
            PathDataCacheKey path_data_key(*this);
            PathDataMruCache* path_data_cache = CacheManager::GetPathDataMruCache();
            PathDataMruCache::hashed_cache_const_iterator iter = path_data_cache->hash_find(path_data_key);
            if(iter==path_data_cache->hash_end())    
            {
                if(!CreatePath(path_data)) return;

                SharedPtrPathData data(new PathData());
                *data = *path_data;//important! copy not ref
                PathDataMruItem item(path_data_key, data);
                CacheManager::GetPathDataMruCache()->update_cache(item);
            }
            else
            {
                *path_data = *(iter->path_data); //important! copy not ref
            } 

            bool need_transform = NeedTransform();
            if(need_transform)
                Transform(path_data, CPoint(trans_org.x*8, trans_org.y*8));

            if(!ScanConvert(path_data)) return;
            if(m_style.get().borderStyle == 0 && (m_style.get().outlineWidthX+m_style.get().outlineWidthY > 0))
            {
                if(!CreateWidenedRegion((int)(m_style.get().outlineWidthX+0.5), (int)(m_style.get().outlineWidthY+0.5))) return;
            }
            else if(m_style.get().borderStyle == 1)
            {
                if(!CreateOpaqueBox()) return;
            }
            m_fDrawn = true;      
        }
        if(!Rasterize(psub.x, psub.y, raterize_result)) return;

        OverlayNoBlurMruItem item(overlay_no_blur_key, raterize_result);
        CacheManager::GetOverlayNoBlurMruCache()->update_cache(item);
    }
    else
    {
        raterize_result = iter->overlay;
    }    
    if(!Blur(*raterize_result, m_style.get().fBlur, m_style.get().fGaussianBlur, *overlay))
    {
        *overlay = raterize_result;
    }
    m_p = psub;
}

bool CWord::NeedTransform()
{
    return (fabs(m_style.get().fontScaleX - 100) > 0.000001) ||
           (fabs(m_style.get().fontScaleY - 100) > 0.000001) ||
           (fabs(m_style.get().fontAngleX) > 0.000001) ||
           (fabs(m_style.get().fontAngleY) > 0.000001) ||
           (fabs(m_style.get().fontAngleZ) > 0.000001) ||
           (fabs(m_style.get().fontShiftX) > 0.000001) ||
           (fabs(m_style.get().fontShiftY) > 0.000001);
}

void CWord::Transform(SharedPtrPathData path_data, const CPoint& org)
{
	// CPUID from VDub
	bool fSSE2 = !!(g_cpuid.m_flags & CCpuID::sse2);

	if(fSSE2) {	// SSE code
		Transform_SSE2(path_data, org);
	} else		// C-code
		Transform_C(path_data, org);
}

void CWord::Transform_C(SharedPtrPathData path_data, const CPoint &org )
{
	double scalex = m_style.get().fontScaleX/100;
	double scaley = m_style.get().fontScaleY/100;

	double caz = cos((3.1415/180)*m_style.get().fontAngleZ);
	double saz = sin((3.1415/180)*m_style.get().fontAngleZ);
	double cax = cos((3.1415/180)*m_style.get().fontAngleX);
	double sax = sin((3.1415/180)*m_style.get().fontAngleX);
	double cay = cos((3.1415/180)*m_style.get().fontAngleY);
	double say = sin((3.1415/180)*m_style.get().fontAngleY);

#ifdef _VSMOD
	// patch m003. random text points
	double xrnd = m_style.get().mod_rand.X*100;
	double yrnd = m_style.get().mod_rand.Y*100;
	double zrnd = m_style.get().mod_rand.Z*100;

	srand(m_style.get().mod_rand.Seed);

	// patch m008. distort
	int xsz,ysz;
	double dst1x,dst1y,dst2x,dst2y,dst3x,dst3y;
	int minx = INT_MAX, miny = INT_MAX, maxx = -INT_MAX, maxy = -INT_MAX;

	bool is_dist = m_style.get().mod_distort.enabled;
	if (is_dist) {
		for(int i = 0; i < path_data->mPathPoints; i++) {
			if(minx > path_data->mpPathPoints[i].x) {
				minx = path_data->mpPathPoints[i].x;
			}
			if(miny > path_data->mpPathPoints[i].y) {
				miny = path_data->mpPathPoints[i].y;
			}
			if(maxx < path_data->mpPathPoints[i].x) {
				maxx = path_data->mpPathPoints[i].x;
			}
			if(maxy < path_data->mpPathPoints[i].y) {
				maxy = path_data->mpPathPoints[i].y;
			}
		}

		xsz = max(maxx - minx, 0);
		ysz = max(maxy - miny, 0);

		dst1x = m_style.get().mod_distort.pointsx[0];
		dst1y = m_style.get().mod_distort.pointsy[0];
		dst2x = m_style.get().mod_distort.pointsx[1];
		dst2y = m_style.get().mod_distort.pointsy[1];
		dst3x = m_style.get().mod_distort.pointsx[2];
		dst3y = m_style.get().mod_distort.pointsy[2];
	}
#endif

	for (int i = 0; i < path_data->mPathPoints; i++) {
		double x, y, z, xx, yy, zz;

		x = path_data->mpPathPoints[i].x;
		y = path_data->mpPathPoints[i].y;
#ifdef _VSMOD
		// patch m002. Z-coord
		z = m_style.get().mod_z;

		double u, v;
		if (is_dist) {
			u = (x-minx) / xsz;
			v = (y-miny) / ysz;

			x = minx+(0 + (dst1x - 0)*u + (dst3x-0)*v+(0+dst2x-dst1x-dst3x)*u*v)*xsz;
			y = miny+(0 + (dst1y - 0)*u + (dst3y-0)*v+(0+dst2y-dst1y-dst3y)*u*v)*ysz;
			//P = P0 + (P1 - P0)u + (P3 - P0)v + (P0 + P2 - P1 - P3)uv
		}

		// patch m003. random text points
		x = xrnd > 0 ? (xrnd - rand() % (int)(xrnd * 2 + 1)) / 100.0 + x : x;
		y = yrnd > 0 ? (yrnd - rand() % (int)(yrnd * 2 + 1)) / 100.0 + y : y;
		z = zrnd > 0 ? (zrnd - rand() % (int)(zrnd * 2 + 1)) / 100.0 + z : z;
#else
		z = 0;
#endif
		double _x = x;
		x = scalex * (x + m_style.get().fontShiftX * y) - org.x;
		y = scaley * (y + m_style.get().fontShiftY * _x) - org.y;

		xx = x*caz + y*saz;
		yy = -(x*saz - y*caz);
		zz = z;

		x = xx;
		y = yy*cax + zz*sax;
		z = yy*sax - zz*cax;

		xx = x*cay + z*say;
		yy = y;
		zz = x*say - z*cay;

		zz = max(zz, -19000);

		x = (xx * 20000) / (zz + 20000);
		y = (yy * 20000) / (zz + 20000);

		path_data->mpPathPoints[i].x = (LONG)(x + org.x + 0.5);
		path_data->mpPathPoints[i].y = (LONG)(y + org.y + 0.5);
	}
}

void CWord::Transform_SSE2(SharedPtrPathData path_data, const CPoint &org )
{
	// __m128 union data type currently not supported with Intel C++ Compiler, so just call C version
#ifdef __ICL
	Transform_C(org);
#else
	// SSE code
	// speed up ~1.5-1.7x
	double scalex = m_style.get().fontScaleX/100;
	double scaley = m_style.get().fontScaleY/100;

	double caz = cos((3.1415/180)*m_style.get().fontAngleZ);
	double saz = sin((3.1415/180)*m_style.get().fontAngleZ);
	double cax = cos((3.1415/180)*m_style.get().fontAngleX);
	double sax = sin((3.1415/180)*m_style.get().fontAngleX);
	double cay = cos((3.1415/180)*m_style.get().fontAngleY);
	double say = sin((3.1415/180)*m_style.get().fontAngleY);

	__m128 __xshift = _mm_set_ps1(m_style.get().fontShiftX);
	__m128 __yshift = _mm_set_ps1(m_style.get().fontShiftY);

	__m128 __xorg = _mm_set_ps1(org.x);
	__m128 __yorg = _mm_set_ps1(org.y);

	__m128 __xscale = _mm_set_ps1(scalex);
	__m128 __yscale = _mm_set_ps1(scaley);

#ifdef _VSMOD
	// patch m003. random text points
	double xrnd = m_style.get().mod_rand.X*100;
	double yrnd = m_style.get().mod_rand.Y*100;
	double zrnd = m_style.get().mod_rand.Z*100;

	srand(m_style.get().mod_rand.Seed);

	__m128 __xsz = _mm_setzero_ps();
	__m128 __ysz = _mm_setzero_ps();

	__m128 __dst1x, __dst1y, __dst213x, __dst213y, __dst3x, __dst3y;

	__m128 __miny;
	__m128 __minx = _mm_set_ps(INT_MAX, INT_MAX, 0, 0);
	__m128 __max = _mm_set_ps(-INT_MAX, -INT_MAX, 1, 1);

	bool is_dist = m_style.get().mod_distort.enabled;
	if(is_dist) {
		for(int i = 0; i < path_data->mPathPoints; i++) {
			__m128 __point = _mm_set_ps(path_data->mpPathPoints[i].x, path_data->mpPathPoints[i].y, 0, 0);
			__minx = _mm_min_ps(__minx, __point);
			__max = _mm_max_ps(__max, __point);
		}

		__m128 __zero = _mm_setzero_ps();
		__max = _mm_sub_ps(__max, __minx); // xsz, ysz, 1, 1
		__max = _mm_max_ps(__max, __zero);

		__xsz = _mm_shuffle_ps(__max, __max, _MM_SHUFFLE(3,3,3,3));
		__ysz = _mm_shuffle_ps(__max, __max, _MM_SHUFFLE(2,2,2,2));

		__miny = _mm_shuffle_ps(__minx, __minx, _MM_SHUFFLE(2,2,2,2));
		__minx = _mm_shuffle_ps(__minx, __minx, _MM_SHUFFLE(3,3,3,3));

		__dst1x = _mm_set_ps1(m_style.get().mod_distort.pointsx[0]);
		__dst1y = _mm_set_ps1(m_style.get().mod_distort.pointsy[0]);
		__dst3x = _mm_set_ps1(m_style.get().mod_distort.pointsx[2]);
		__dst3y = _mm_set_ps1(m_style.get().mod_distort.pointsy[2]);
		__dst213x = _mm_set_ps1(m_style.get().mod_distort.pointsx[1]); // 2 - 1 - 3
		__dst213x = _mm_sub_ps(__dst213x, __dst1x);
		__dst213x = _mm_sub_ps(__dst213x, __dst3x);

		__dst213y = _mm_set_ps1(m_style.get().mod_distort.pointsy[1]);
		__dst213x = _mm_sub_ps(__dst213y, __dst1y);
		__dst213x = _mm_sub_ps(__dst213y, __dst3y);
	}
#endif

	__m128 __caz = _mm_set_ps1(caz);
	__m128 __saz = _mm_set_ps1(saz);
	__m128 __cax = _mm_set_ps1(cax);
	__m128 __sax = _mm_set_ps1(sax);
	__m128 __cay = _mm_set_ps1(cay);
	__m128 __say = _mm_set_ps1(say);

	// this can be paralleled for openmp
	int mPathPointsD4 = path_data->mPathPoints / 4;
	int mPathPointsM4 = path_data->mPathPoints % 4;
        
	for(ptrdiff_t i = 0; i < mPathPointsD4 + 1; i++) {
        POINT* const temp_points = path_data->mpPathPoints + 4 * i;

		__m128 __pointx, __pointy;
		// we can't use load .-.
		if(i != mPathPointsD4) {
			__pointx = _mm_set_ps(temp_points[0].x, temp_points[1].x, temp_points[2].x, temp_points[3].x);
			__pointy = _mm_set_ps(temp_points[0].y, temp_points[1].y, temp_points[2].y, temp_points[3].y);
		} else { // last cycle
			switch(mPathPointsM4) {
				default:
				case 0:
					continue;
				case 1:
					__pointx = _mm_set_ps(temp_points[0].x, 0, 0, 0);
					__pointy = _mm_set_ps(temp_points[0].y, 0, 0, 0);
					break;
				case 2:
					__pointx = _mm_set_ps(temp_points[0].x, temp_points[1].x, 0, 0);
					__pointy = _mm_set_ps(temp_points[0].y, temp_points[1].y, 0, 0);
					break;
				case 3:
					__pointx = _mm_set_ps(temp_points[0].x, temp_points[1].x, temp_points[2].x, 0);
					__pointy = _mm_set_ps(temp_points[0].y, temp_points[1].y, temp_points[2].y, 0);
					break;
			}
		}

#ifdef _VSMOD
		__m128 __pointz = _mm_set_ps1(m_style.get().mod_z);

		// distort
		if(is_dist) {
			//P = P0 + (P1 - P0)u + (P3 - P0)v + (P0 + P2 - P1 - P3)uv
			__m128 __u = _mm_sub_ps(__pointx, __minx);
			__m128 __v = _mm_sub_ps(__pointy, __miny);
			__m128 __1_xsz = _mm_rcp_ps(__xsz);
			__m128 __1_ysz = _mm_rcp_ps(__ysz);
			__u = _mm_mul_ps(__u, __1_xsz);
			__v = _mm_mul_ps(__v, __1_ysz);

			// x
			__pointx = _mm_mul_ps(__dst213x, __u);
			__pointx = _mm_mul_ps(__pointx, __v);

			__m128 __tmpx = _mm_mul_ps(__dst3x, __v);
			__pointx = _mm_add_ps(__pointx, __tmpx);
			__tmpx = _mm_mul_ps(__dst1x, __u);
			__pointx = _mm_add_ps(__pointx, __tmpx);

			__pointx = _mm_mul_ps(__pointx, __xsz);
			__pointx = _mm_add_ps(__pointx, __minx);

			// y
			__pointy = _mm_mul_ps(__dst213y, __u);
			__pointy = _mm_mul_ps(__pointy, __v);

			__m128 __tmpy = _mm_mul_ps(__dst3y, __v);
			__pointy = _mm_add_ps(__pointy, __tmpy);
			__tmpy = _mm_mul_ps(__dst1y, __u);
			__pointy = _mm_add_ps(__pointy, __tmpy);

			__pointy = _mm_mul_ps(__pointy, __ysz);
			__pointy = _mm_add_ps(__pointy, __miny);
		}

		// randomize
		if(xrnd!=0 || yrnd!=0 || zrnd!=0) {
			__declspec(align(16)) float rx[4], ry[4], rz[4];
			for(int k=0; k<4; k++) {
				rx[k] = xrnd > 0 ? (xrnd - rand() % (int)(xrnd * 2 + 1)) : 0;
				ry[k] = yrnd > 0 ? (yrnd - rand() % (int)(yrnd * 2 + 1)) : 0;
				rz[k] = zrnd > 0 ? (zrnd - rand() % (int)(zrnd * 2 + 1)) : 0;
			}
			__m128 __001 = _mm_set_ps1(0.01f);

			if(xrnd!=0) {
				__m128 __rx = _mm_load_ps(rx);
				__rx = _mm_mul_ps(__rx, __001);
				__pointx = _mm_add_ps(__pointx, __rx);
			}

			if(yrnd!=0) {
				__m128 __ry = _mm_load_ps(ry);
				__ry = _mm_mul_ps(__ry, __001);
				__pointy = _mm_add_ps(__pointy, __ry);
			}

			if(zrnd!=0) {
				__m128 __rz = _mm_load_ps(rz);
				__rz = _mm_mul_ps(__rz, __001);
				__pointz = _mm_add_ps(__pointz, __rz);
			}
		}
#else
		__m128 __pointz = _mm_set_ps1(0);
#endif

		// scale and shift
		__m128 __tmpx;
		if(m_style.get().fontShiftX!=0) {
			__tmpx = _mm_mul_ps(__xshift, __pointy);
			__tmpx = _mm_add_ps(__tmpx, __pointx);
		} else {
			__tmpx = __pointx;
		}
		__tmpx = _mm_mul_ps(__tmpx, __xscale);
		__tmpx = _mm_sub_ps(__tmpx, __xorg);

		__m128 __tmpy;
		if(m_style.get().fontShiftY!=0) {
			__tmpy = _mm_mul_ps(__yshift, __pointx);
			__tmpy = _mm_add_ps(__tmpy, __pointy);
		} else {
			__tmpy = __pointy;
		}
		__tmpy = _mm_mul_ps(__tmpy, __yscale);
		__tmpy = _mm_sub_ps(__tmpy, __yorg);

		// rotate
		__m128 __xx = _mm_mul_ps(__tmpx, __caz);
		__m128 __yy = _mm_mul_ps(__tmpy, __saz);
		__pointx = _mm_add_ps(__xx, __yy);
		__xx = _mm_mul_ps(__tmpx, __saz);
		__yy = _mm_mul_ps(__tmpy, __caz);
		__pointy = _mm_sub_ps(__yy, __xx);

		__m128 __zz = _mm_mul_ps(__pointz, __sax);
		__yy = _mm_mul_ps(__pointy, __cax);
		__pointy = _mm_add_ps(__yy, __zz);
		__zz = _mm_mul_ps(__pointz, __cax);
		__yy = _mm_mul_ps(__pointy, __sax);
		__pointz = _mm_sub_ps(__zz, __yy);

		__xx = _mm_mul_ps(__pointx, __cay);
		__zz = _mm_mul_ps(__pointz, __say);
		__pointx = _mm_add_ps(__xx, __zz);
		__xx = _mm_mul_ps(__pointx, __say);
		__zz = _mm_mul_ps(__pointz, __cay);
		__pointz = _mm_sub_ps(__xx, __zz);

		__zz = _mm_set_ps1(-19000);
		__pointz = _mm_max_ps(__pointz, __zz);

		__m128 __20000 = _mm_set_ps1(20000);
		__zz = _mm_add_ps(__pointz, __20000);
		__zz = _mm_rcp_ps(__zz);

		__pointx = _mm_mul_ps(__pointx, __20000);
		__pointx = _mm_mul_ps(__pointx, __zz);

		__pointy = _mm_mul_ps(__pointy, __20000);
		__pointy = _mm_mul_ps(__pointy, __zz);

		__pointx = _mm_add_ps(__pointx, __xorg);
		__pointy = _mm_add_ps(__pointy, __yorg);

		__m128 __05 = _mm_set_ps1(0.5);

		__pointx = _mm_add_ps(__pointx, __05);
		__pointy = _mm_add_ps(__pointy, __05);

		if(i == mPathPointsD4) { // last cycle
			for(int k=0; k<mPathPointsM4; k++) {
				temp_points[k].x = static_cast<LONG>(__pointx.m128_f32[3-k]);
				temp_points[k].y = static_cast<LONG>(__pointy.m128_f32[3-k]);
			}
		} else {
			for(int k=0; k<4; k++) {
				temp_points[k].x = static_cast<LONG>(__pointx.m128_f32[3-k]);
				temp_points[k].y = static_cast<LONG>(__pointy.m128_f32[3-k]);
			}
		}
	}
#endif // __ICL
}

bool CWord::CreateOpaqueBox()
{
    if(m_pOpaqueBox) return(true);
    STSStyle style = m_style.get();
    style.borderStyle = 0;
    style.outlineWidthX = style.outlineWidthY = 0;
    style.colors[0] = m_style.get().colors[2];
    style.alpha[0] = m_style.get().alpha[2];
    int w = (int)(m_style.get().outlineWidthX + 0.5);
    int h = (int)(m_style.get().outlineWidthY + 0.5);
    CStringW str;
    str.Format(L"m %d %d l %d %d %d %d %d %d",
               -w, -h,
               m_width+w, -h,
               m_width+w, m_ascent+m_descent+h,
               -w, m_ascent+m_descent+h);
    m_pOpaqueBox.reset( new CPolygon(FwSTSStyle(style), str, 0, 0, 0, 1.0/8, 1.0/8, 0) );
    return(!!m_pOpaqueBox);
}

// CText

CText::CText(const FwSTSStyle& style, const CStringW& str, int ktype, int kstart, int kend)
    : CWord(style, str, ktype, kstart, kend)
{
    if(m_str == L" ")
    {
        m_fWhiteSpaceChar = true;
    }
    FwCMyFont font(m_style);
    HFONT hOldFont = SelectFont(g_hDC, font.get());
    if(m_style.get().fontSpacing || (long)GetVersion() < 0)
    {
        bool bFirstPath = true;
        for(LPCWSTR s = m_str.get(); *s; s++)
        {
            CSize extent;
            if(!GetTextExtentPoint32W(g_hDC, s, 1, &extent)) {SelectFont(g_hDC, hOldFont); ASSERT(0); return;}
            m_width += extent.cx + (int)m_style.get().fontSpacing;
        }
//          m_width -= (int)m_style.get().fontSpacing; // TODO: subtract only at the end of the line
    }
    else
    {
        CSize extent;
        if(!GetTextExtentPoint32W(g_hDC, m_str.get(), wcslen(m_str.get()), &extent)) {SelectFont(g_hDC, hOldFont); ASSERT(0); return;}
        m_width += extent.cx;
    }
    m_width = (int)(m_style.get().fontScaleX/100*m_width + 4) >> 3;
    SelectFont(g_hDC, hOldFont);
}

CText::CText( const CText& src ):CWord(src.m_style, src.m_str, src.m_ktype, src.m_kstart, src.m_kend)
{
    m_width = src.m_width;
}

SharedPtrCWord CText::Copy()
{
    SharedPtrCWord result(new CText(*this));
	return result;
}

bool CText::Append(const SharedPtrCWord& w)
{
    boost::shared_ptr<CText> p = boost::dynamic_pointer_cast<CText>(w);
    return (p && CWord::Append(w));
}

bool CText::CreatePath(SharedPtrPathData path_data)
{
    FwCMyFont font(m_style);
    HFONT hOldFont = SelectFont(g_hDC, font.get());
    int width = 0;
    if(m_style.get().fontSpacing || (long)GetVersion() < 0)
    {
        bool bFirstPath = true;
        for(LPCWSTR s = m_str.get(); *s; s++)
        {
            CSize extent;
            if(!GetTextExtentPoint32W(g_hDC, s, 1, &extent)) {SelectFont(g_hDC, hOldFont); ASSERT(0); return(false);}
            path_data->PartialBeginPath(g_hDC, bFirstPath);
            bFirstPath = false;
            TextOutW(g_hDC, 0, 0, s, 1);
            path_data->PartialEndPath(g_hDC, width, 0);
            width += extent.cx + (int)m_style.get().fontSpacing;
        }
    }
    else
    {
        CSize extent;
        if(!GetTextExtentPoint32W(g_hDC, m_str.get(), m_str.get().GetLength(), &extent)) {SelectFont(g_hDC, hOldFont); ASSERT(0); return(false);}
        path_data->BeginPath(g_hDC);
        TextOutW(g_hDC, 0, 0, m_str.get(), m_str.get().GetLength());
        path_data->EndPath(g_hDC);
    }
    SelectFont(g_hDC, hOldFont);
    return(true);
}

// CPolygon

CPolygon::CPolygon(const FwSTSStyle& style, CStringW str, int ktype, int kstart, int kend, double scalex, double scaley, int baseline)
    : CWord(style, str, ktype, kstart, kend)
    , m_scalex(scalex), m_scaley(scaley), m_baseline(baseline)
{
    ParseStr();
}

CPolygon::CPolygon(CPolygon& src) : CWord(src.m_style, src.m_str.get(), src.m_ktype, src.m_kstart, src.m_kend)
{
	m_scalex = src.m_scalex;
	m_scaley = src.m_scaley;
	m_baseline = src.m_baseline;
	m_width = src.m_width;
 	m_ascent = src.m_ascent;
 	m_descent = src.m_descent;
	m_pathTypesOrg.Copy(src.m_pathTypesOrg);
	m_pathPointsOrg.Copy(src.m_pathPointsOrg);
}
CPolygon::~CPolygon()
{
}

SharedPtrCWord CPolygon::Copy()
{
    SharedPtrCWord result(DNew CPolygon(*this));
	return result;
}

bool CPolygon::Append(const SharedPtrCWord& w)
{
    int width = m_width;
    boost::shared_ptr<CPolygon> p = boost::dynamic_pointer_cast<CPolygon>(w);
    if(!p) return(false);
    // TODO
    return(false);
    return(true);
}

bool CPolygon::GetLONG(CStringW& str, LONG& ret)
{
    LPWSTR s = (LPWSTR)(LPCWSTR)str, e = s;
    ret = wcstol(str, &e, 10);
    str = str.Mid(e - s);
    return(e > s);
}

bool CPolygon::GetPOINT(CStringW& str, POINT& ret)
{
    return(GetLONG(str, ret.x) && GetLONG(str, ret.y));
}

bool CPolygon::ParseStr()
{
    if(m_pathTypesOrg.GetCount() > 0) return(true);
    CPoint p;
    int j, lastsplinestart = -1, firstmoveto = -1, lastmoveto = -1;
    CStringW str = m_str.get();
    str.SpanIncluding(L"mnlbspc 0123456789");
    str.Replace(L"m", L"*m");
    str.Replace(L"n", L"*n");
    str.Replace(L"l", L"*l");
    str.Replace(L"b", L"*b");
    str.Replace(L"s", L"*s");
    str.Replace(L"p", L"*p");
    str.Replace(L"c", L"*c");
    int k = 0;
    for(CStringW s = str.Tokenize(L"*", k); !s.IsEmpty(); s = str.Tokenize(L"*", k))
    {
        WCHAR c = s[0];
        s.TrimLeft(L"mnlbspc ");
        switch(c)
        {
        case 'm':
            lastmoveto = m_pathTypesOrg.GetCount();
            if(firstmoveto == -1) firstmoveto = lastmoveto;
            while(GetPOINT(s, p)) {m_pathTypesOrg.Add(PT_MOVETO); m_pathPointsOrg.Add(p);}
            break;
        case 'n':
            while(GetPOINT(s, p)) {m_pathTypesOrg.Add(PT_MOVETONC); m_pathPointsOrg.Add(p);}
            break;
        case 'l':
            while(GetPOINT(s, p)) {m_pathTypesOrg.Add(PT_LINETO); m_pathPointsOrg.Add(p);}
            break;
        case 'b':
            j = m_pathTypesOrg.GetCount();
            while(GetPOINT(s, p)) {m_pathTypesOrg.Add(PT_BEZIERTO); m_pathPointsOrg.Add(p); j++;}
            j = m_pathTypesOrg.GetCount() - ((m_pathTypesOrg.GetCount()-j)%3);
            m_pathTypesOrg.SetCount(j);
            m_pathPointsOrg.SetCount(j);
            break;
        case 's':
            {
                j = lastsplinestart = m_pathTypesOrg.GetCount();
                int i = 3;
                while(i-- && GetPOINT(s, p)) {m_pathTypesOrg.Add(PT_BSPLINETO); m_pathPointsOrg.Add(p); j++;}
                if(m_pathTypesOrg.GetCount()-lastsplinestart < 3) {m_pathTypesOrg.SetCount(lastsplinestart); m_pathPointsOrg.SetCount(lastsplinestart); lastsplinestart = -1;}
            }            
            // no break here
        case 'p':
            while(GetPOINT(s, p)) {m_pathTypesOrg.Add(PT_BSPLINEPATCHTO); m_pathPointsOrg.Add(p); j++;}
            break;
        case 'c':
            if(lastsplinestart > 0)
            {
                m_pathTypesOrg.Add(PT_BSPLINEPATCHTO);
                m_pathTypesOrg.Add(PT_BSPLINEPATCHTO);
                m_pathTypesOrg.Add(PT_BSPLINEPATCHTO);
                p = m_pathPointsOrg[lastsplinestart-1]; // we need p for temp storage, because operator [] will return a reference to CPoint and Add() may reallocate its internal buffer (this is true for MFC 7.0 but not for 6.0, hehe)
                m_pathPointsOrg.Add(p);
                p = m_pathPointsOrg[lastsplinestart];
                m_pathPointsOrg.Add(p);
                p = m_pathPointsOrg[lastsplinestart+1];
                m_pathPointsOrg.Add(p);
                lastsplinestart = -1;
            }
            break;
        default:
            break;
        }
    }
    /*
        LPCWSTR str = m_str;
        while(*str)
        {
            while(*str && *str != 'm' && *str != 'n' && *str != 'l' && *str != 'b' && *str != 's' && *str != 'p' && *str != 'c') str++;

            if(!*str) break;

            switch(*str++)
            {
            case 'm':
                lastmoveto = m_pathTypesOrg.GetCount();
                if(firstmoveto == -1) firstmoveto = lastmoveto;
                while(GetPOINT(str, p)) {m_pathTypesOrg.Add(PT_MOVETO); m_pathPointsOrg.Add(p);}
                break;
            case 'n':
                while(GetPOINT(str, p)) {m_pathTypesOrg.Add(PT_MOVETONC); m_pathPointsOrg.Add(p);}
                break;
            case 'l':
                while(GetPOINT(str, p)) {m_pathTypesOrg.Add(PT_LINETO); m_pathPointsOrg.Add(p);}
                break;
            case 'b':
                j = m_pathTypesOrg.GetCount();
                while(GetPOINT(str, p)) {m_pathTypesOrg.Add(PT_BEZIERTO); m_pathPointsOrg.Add(p); j++;}
                j = m_pathTypesOrg.GetCount() - ((m_pathTypesOrg.GetCount()-j)%3);
                m_pathTypesOrg.SetCount(j); m_pathPointsOrg.SetCount(j);
                break;
            case 's':
                j = lastsplinestart = m_pathTypesOrg.GetCount();
                i = 3;
                while(i-- && GetPOINT(str, p)) {m_pathTypesOrg.Add(PT_BSPLINETO); m_pathPointsOrg.Add(p); j++;}
                if(m_pathTypesOrg.GetCount()-lastsplinestart < 3) {m_pathTypesOrg.SetCount(lastsplinestart); m_pathPointsOrg.SetCount(lastsplinestart); lastsplinestart = -1;}
                // no break here
            case 'p':
                while(GetPOINT(str, p)) {m_pathTypesOrg.Add(PT_BSPLINEPATCHTO); m_pathPointsOrg.Add(p); j++;}
                break;
            case 'c':
                if(lastsplinestart > 0)
                {
                    m_pathTypesOrg.Add(PT_BSPLINEPATCHTO);
                    m_pathTypesOrg.Add(PT_BSPLINEPATCHTO);
                    m_pathTypesOrg.Add(PT_BSPLINEPATCHTO);
                    p = m_pathPointsOrg[lastsplinestart-1]; // we need p for temp storage, because operator [] will return a reference to CPoint and Add() may reallocate its internal buffer (this is true for MFC 7.0 but not for 6.0, hehe)
                    m_pathPointsOrg.Add(p);
                    p = m_pathPointsOrg[lastsplinestart];
                    m_pathPointsOrg.Add(p);
                    p = m_pathPointsOrg[lastsplinestart+1];
                    m_pathPointsOrg.Add(p);
                    lastsplinestart = -1;
                }
                break;
            default:
                break;
            }

            if(firstmoveto > 0) break;
        }
    */
    if(lastmoveto == -1 || firstmoveto > 0)
    {
        m_pathTypesOrg.RemoveAll();
        m_pathPointsOrg.RemoveAll();
        return(false);
    }
    int minx = INT_MAX, miny = INT_MAX, maxx = -INT_MAX, maxy = -INT_MAX;
    for(size_t i = 0; i < m_pathTypesOrg.GetCount(); i++)
    {
        m_pathPointsOrg[i].x = (int)(64 * m_scalex * m_pathPointsOrg[i].x);
        m_pathPointsOrg[i].y = (int)(64 * m_scaley * m_pathPointsOrg[i].y);
        if(minx > m_pathPointsOrg[i].x) minx = m_pathPointsOrg[i].x;
        if(miny > m_pathPointsOrg[i].y) miny = m_pathPointsOrg[i].y;
        if(maxx < m_pathPointsOrg[i].x) maxx = m_pathPointsOrg[i].x;
        if(maxy < m_pathPointsOrg[i].y) maxy = m_pathPointsOrg[i].y;
    }
    m_width = max(maxx - minx, 0);
    m_ascent = max(maxy - miny, 0);
    int baseline = (int)(64 * m_scaley * m_baseline);
    m_descent = baseline;
    m_ascent -= baseline;
    m_width = ((int)(m_style.get().fontScaleX/100 * m_width) + 4) >> 3;
    m_ascent = ((int)(m_style.get().fontScaleY/100 * m_ascent) + 4) >> 3;
    m_descent = ((int)(m_style.get().fontScaleY/100 * m_descent) + 4) >> 3;
    return(true);
}

bool CPolygon::CreatePath(SharedPtrPathData path_data)
{
    int len = m_pathTypesOrg.GetCount();
    if(len == 0) return(false);
    if(path_data->mPathPoints != len)
    {
        path_data->mpPathTypes = (BYTE*)realloc(path_data->mpPathTypes, len*sizeof(BYTE));
        path_data->mpPathPoints = (POINT*)realloc(path_data->mpPathPoints, len*sizeof(POINT));
        if(!path_data->mpPathTypes || !path_data->mpPathPoints) return(false);
        path_data->mPathPoints = len;
    }
    memcpy(path_data->mpPathTypes, m_pathTypesOrg.GetData(), len*sizeof(BYTE));
    memcpy(path_data->mpPathPoints, m_pathPointsOrg.GetData(), len*sizeof(POINT));
    return(true);
}

// CClipper

CClipper::CClipper(CStringW str, CSize size, double scalex, double scaley, bool inverse)
    : m_polygon( new CPolygon(FwSTSStyle(), str, 0, 0, 0, scalex, scaley, 0) )
{    
    m_size.cx = m_size.cy = 0;
    m_pAlphaMask = NULL;
    if(size.cx < 0 || size.cy < 0 || !(m_pAlphaMask = new BYTE[size.cx*size.cy])) return;
    m_size = size;
    m_inverse = inverse;
    memset(m_pAlphaMask, 0, size.cx*size.cy);
    OverlayList overlay_list;
    CWord::Paint( m_polygon, CPoint(0, 0), CPoint(0, 0), &overlay_list );
    int w = overlay_list.overlay->mOverlayWidth, h = overlay_list.overlay->mOverlayHeight;
    int x = (overlay_list.overlay->mOffsetX+4)>>3, y = (overlay_list.overlay->mOffsetY+4)>>3;
    int xo = 0, yo = 0;
    if(x < 0) {xo = -x; w -= -x; x = 0;}
    if(y < 0) {yo = -y; h -= -y; y = 0;}
    if(x+w > m_size.cx) w = m_size.cx-x;
    if(y+h > m_size.cy) h = m_size.cy-y;
    if(w <= 0 || h <= 0) return;
    const BYTE* src = overlay_list.overlay->mpOverlayBuffer.body + (overlay_list.overlay->mOverlayPitch * yo + xo);
    BYTE* dst = m_pAlphaMask + m_size.cx * y + x;
    while(h--)
    {
        //for(int wt=0; wt<w; ++wt)
        //  dst[wt] = src[wt];
        memcpy(dst, src, w);
        src += overlay_list.overlay->mOverlayPitch;
        dst += m_size.cx;
    }
    if(inverse)
    {
        BYTE* dst = m_pAlphaMask;
        for(int i = size.cx*size.cy; i>0; --i, ++dst)
            *dst = 0x40 - *dst; // mask is 6 bit
    }
}

CClipper::~CClipper()
{
    if(m_pAlphaMask) delete [] m_pAlphaMask;
    m_pAlphaMask = NULL;
}

// CLine

CLine::~CLine()
{
    //POSITION pos = GetHeadPosition();
    //while(pos) delete GetNext(pos);
}

void CLine::Compact()
{
    POSITION pos = GetHeadPosition();
    while(pos)
    {
        SharedPtrCWord w = GetNext(pos);
        if(!w->m_fWhiteSpaceChar) break;
        m_width -= w->m_width;
//        delete w;
        RemoveHead();
    }
    pos = GetTailPosition();
    while(pos)
    {
        SharedPtrCWord w = GetPrev(pos);
        if(!w->m_fWhiteSpaceChar) break;
        m_width -= w->m_width;
//        delete w;
        RemoveTail();
    }
    if(IsEmpty()) return;
    CLine l;
    l.AddTailList(this);
    RemoveAll();
    SharedPtrCWord last;
    pos = l.GetHeadPosition();
    while(pos)
    {
        SharedPtrCWord w = l.GetNext(pos);
        if(!last || !last->Append(w))
            AddTail(last = w->Copy());
    }
    m_ascent = m_descent = m_borderX = m_borderY = 0;
    pos = GetHeadPosition();
    while(pos)
    {
        SharedPtrCWord w = GetNext(pos);
        if(m_ascent < w->m_ascent) m_ascent = w->m_ascent;
        if(m_descent < w->m_descent) m_descent = w->m_descent;
        if(m_borderX < w->m_style.get().outlineWidthX) m_borderX = (int)(w->m_style.get().outlineWidthX+0.5);
        if(m_borderY < w->m_style.get().outlineWidthY) m_borderY = (int)(w->m_style.get().outlineWidthY+0.5);
    }
}

CRect CLine::PaintShadow(SubPicDesc& spd, CRect& clipRect, BYTE* pAlphaMask, CPoint p, CPoint org, int time, int alpha)
{
    CRect bbox(0, 0, 0, 0);
    POSITION pos = GetHeadPosition();
    while(pos)
    {
        SharedPtrCWord w = GetNext(pos);
        if(w->m_fLineBreak) return(bbox); // should not happen since this class is just a line of text without any breaks
        if(w->m_style.get().shadowDepthX != 0 || w->m_style.get().shadowDepthY != 0)
        {
            int x = p.x + (int)(w->m_style.get().shadowDepthX+0.5);
            int y = p.y + m_ascent - w->m_ascent + (int)(w->m_style.get().shadowDepthY+0.5);
            DWORD a = 0xff - w->m_style.get().alpha[3];
            if(alpha > 0) a = MulDiv(a, 0xff - alpha, 0xff);
            COLORREF shadow = revcolor(w->m_style.get().colors[3]) | (a<<24);
            DWORD sw[6] = {shadow, -1};
            //xy
            if(spd.type == MSP_YUY2)
            {
                sw[0] =rgb2yuv(sw[0], XY_AUYV);
            }
            else if(spd.type == MSP_AYUV || spd.type == MSP_YV12 || spd.type == MSP_IYUV)
            {
                sw[0] =rgb2yuv(sw[0], XY_AYUV);
            }
            OverlayList overlay_list;
            CWord::Paint(w, CPoint(x, y), org, &overlay_list);
            if(w->m_style.get().borderStyle == 0)
            {
                bbox |= w->Draw(spd, overlay_list.overlay, clipRect, pAlphaMask, x, y, sw,
                                w->m_ktype > 0 || w->m_style.get().alpha[0] < 0xff,
                                (w->m_style.get().outlineWidthX+w->m_style.get().outlineWidthY > 0) && !(w->m_ktype == 2 && time < w->m_kstart));
            }
            else if(w->m_style.get().borderStyle == 1 && w->m_pOpaqueBox)
            {
                bbox |= w->m_pOpaqueBox->Draw(spd, overlay_list.next->overlay, clipRect, pAlphaMask, x, y, sw, true, false);
            }
        }
        p.x += w->m_width;
    }
    return(bbox);
}

CRect CLine::PaintOutline(SubPicDesc& spd, CRect& clipRect, BYTE* pAlphaMask, CPoint p, CPoint org, int time, int alpha)
{
    CRect bbox(0, 0, 0, 0);
    POSITION pos = GetHeadPosition();
    while(pos)
    {
        SharedPtrCWord w = GetNext(pos);
        if(w->m_fLineBreak) return(bbox); // should not happen since this class is just a line of text without any breaks
        if(w->m_style.get().outlineWidthX+w->m_style.get().outlineWidthY > 0 && !(w->m_ktype == 2 && time < w->m_kstart))
        {
            int x = p.x;
            int y = p.y + m_ascent - w->m_ascent;
            DWORD aoutline = w->m_style.get().alpha[2];
            if(alpha > 0) aoutline += MulDiv(alpha, 0xff - w->m_style.get().alpha[2], 0xff);
            COLORREF outline = revcolor(w->m_style.get().colors[2]) | ((0xff-aoutline)<<24);
            DWORD sw[6] = {outline, -1};
            //xy
            if(spd.type == MSP_YUY2)
            {
                sw[0] =rgb2yuv(sw[0], XY_AUYV);
            }
            else if(spd.type == MSP_AYUV || spd.type == MSP_YV12 || spd.type == MSP_IYUV)
            {
                sw[0] =rgb2yuv(sw[0], XY_AYUV);
            }
            OverlayList overlay_list;
            CWord::Paint(w, CPoint(x, y), org, &overlay_list);
            if(w->m_style.get().borderStyle == 0)
            {
                bbox |= w->Draw(spd, overlay_list.overlay, clipRect, pAlphaMask, x, y, sw, !w->m_style.get().alpha[0] && !w->m_style.get().alpha[1] && !alpha, true);
            }
            else if(w->m_style.get().borderStyle == 1 && w->m_pOpaqueBox)
            {
                bbox |= w->m_pOpaqueBox->Draw(spd, overlay_list.next->overlay, clipRect, pAlphaMask, x, y, sw, true, false);
            }
        }
        p.x += w->m_width;
    }
    return(bbox);
}

CRect CLine::PaintBody(SubPicDesc& spd, CRect& clipRect, BYTE* pAlphaMask, CPoint p, CPoint org, int time, int alpha)
{
    CRect bbox(0, 0, 0, 0);
    POSITION pos = GetHeadPosition();
    while(pos)
    {
        SharedPtrCWord w = GetNext(pos);
        if(w->m_fLineBreak) return(bbox); // should not happen since this class is just a line of text without any breaks
        int x = p.x;
        int y = p.y + m_ascent - w->m_ascent;
        // colors
        DWORD aprimary = w->m_style.get().alpha[0];
        if(alpha > 0) aprimary += MulDiv(alpha, 0xff - w->m_style.get().alpha[0], 0xff);
        COLORREF primary = revcolor(w->m_style.get().colors[0]) | ((0xff-aprimary)<<24);
        DWORD asecondary = w->m_style.get().alpha[1];
        if(alpha > 0) asecondary += MulDiv(alpha, 0xff - w->m_style.get().alpha[1], 0xff);
        COLORREF secondary = revcolor(w->m_style.get().colors[1]) | ((0xff-asecondary)<<24);
        DWORD sw[6] = {primary, 0, secondary};
        // karaoke
        double t;
        if(w->m_ktype == 0 || w->m_ktype == 2)
        {
            t = time < w->m_kstart ? 0 : 1;
        }
        else if(w->m_ktype == 1)
        {
            if(time < w->m_kstart) t = 0;
            else if(time < w->m_kend)
            {
                t = 1.0 * (time - w->m_kstart) / (w->m_kend - w->m_kstart);
                double angle = fmod(w->m_style.get().fontAngleZ, 360.0);
                if(angle > 90 && angle < 270)
                {
                    t = 1-t;
                    COLORREF tmp = sw[0];
                    sw[0] = sw[2];
                    sw[2] = tmp;
                }
            }
            else t = 1.0;
        }
        if(t >= 1)
        {
            sw[1] = 0xffffffff;
        }
        sw[3] = (int)(w->m_style.get().outlineWidthX + t*w->m_width) >> 3;
        sw[4] = sw[2];
        sw[5] = 0x00ffffff;
        //xy
        if(spd.type == MSP_YUY2)
        {
            sw[0] =rgb2yuv(sw[0], XY_AUYV);
            sw[2] =rgb2yuv(sw[2], XY_AUYV);
            sw[4] =rgb2yuv(sw[4], XY_AUYV);
        }
        else if(spd.type == MSP_AYUV || spd.type == MSP_YV12 || spd.type == MSP_IYUV)
        {
            sw[0] =rgb2yuv(sw[0], XY_AYUV);
            sw[2] =rgb2yuv(sw[2], XY_AYUV);
            sw[4] =rgb2yuv(sw[4], XY_AYUV);
        }
        OverlayList overlay_list;
        CWord::Paint(w, CPoint(x, y), org, &overlay_list);
        bbox |= w->Draw(spd, overlay_list.overlay, clipRect, pAlphaMask, x, y, sw, true, false);
        p.x += w->m_width;
    }
    return(bbox);
}


// CSubtitle

CSubtitle::CSubtitle()
{
    memset(m_effects, 0, sizeof(Effect*)*EF_NUMBEROFEFFECTS);
    m_pClipper = NULL;
    m_clipInverse = false;
    m_scalex = m_scaley = 1;
    m_fAnimated2 = false;
}

CSubtitle::~CSubtitle()
{
    Empty();
}

void CSubtitle::Empty()
{
    POSITION pos = GetHeadPosition();
    while(pos) delete GetNext(pos);
//    pos = m_words.GetHeadPosition();
//    while(pos) delete m_words.GetNext(pos);
    for(int i = 0; i < EF_NUMBEROFEFFECTS; i++) {if(m_effects[i]) delete m_effects[i];}
    memset(m_effects, 0, sizeof(Effect*)*EF_NUMBEROFEFFECTS);
    if(m_pClipper) delete m_pClipper;
    m_pClipper = NULL;
}

int CSubtitle::GetFullWidth()
{
    int width = 0;
    POSITION pos = m_words.GetHeadPosition();
    while(pos) width += m_words.GetNext(pos)->m_width;
    return(width);
}

int CSubtitle::GetFullLineWidth(POSITION pos)
{
    int width = 0;
    while(pos)
    {
        SharedPtrCWord w = m_words.GetNext(pos);
        if(w->m_fLineBreak) break;
        width += w->m_width;
    }
    return(width);
}

int CSubtitle::GetWrapWidth(POSITION pos, int maxwidth)
{
    if(m_wrapStyle == 0 || m_wrapStyle == 3)
    {
        if(maxwidth > 0)
        {
//          int fullwidth = GetFullWidth();
            int fullwidth = GetFullLineWidth(pos);
            int minwidth = fullwidth / ((abs(fullwidth) / maxwidth) + 1);
            int width = 0, wordwidth = 0;
            while(pos && width < minwidth)
            {
                SharedPtrCWord w = m_words.GetNext(pos);
                wordwidth = w->m_width;
                if(abs(width + wordwidth) < abs(maxwidth)) width += wordwidth;
            }
            maxwidth = width;
            if(m_wrapStyle == 3 && pos) maxwidth -= wordwidth;
        }
    }
    else if(m_wrapStyle == 1)
    {
//      maxwidth = maxwidth;
    }
    else if(m_wrapStyle == 2)
    {
        maxwidth = INT_MAX;
    }
    return(maxwidth);
}

CLine* CSubtitle::GetNextLine(POSITION& pos, int maxwidth)
{
    if(pos == NULL) return(NULL);
    CLine* ret = new CLine();
    if(!ret) return(NULL);
    ret->m_width = ret->m_ascent = ret->m_descent = ret->m_borderX = ret->m_borderY = 0;
    maxwidth = GetWrapWidth(pos, maxwidth);
    bool fEmptyLine = true;
    while(pos)
    {
        SharedPtrCWord w = m_words.GetNext(pos);
        if(ret->m_ascent < w->m_ascent) ret->m_ascent = w->m_ascent;
        if(ret->m_descent < w->m_descent) ret->m_descent = w->m_descent;
        if(ret->m_borderX < w->m_style.get().outlineWidthX) ret->m_borderX = (int)(w->m_style.get().outlineWidthX+0.5);
        if(ret->m_borderY < w->m_style.get().outlineWidthY) ret->m_borderY = (int)(w->m_style.get().outlineWidthY+0.5);
        if(w->m_fLineBreak)
        {
            if(fEmptyLine) {ret->m_ascent /= 2; ret->m_descent /= 2; ret->m_borderX = ret->m_borderY = 0;}
            ret->Compact();
            return(ret);
        }
        fEmptyLine = false;
        bool fWSC = w->m_fWhiteSpaceChar;
        int width = w->m_width;
        POSITION pos2 = pos;
        while(pos2)
        {
            if(m_words.GetAt(pos2)->m_fWhiteSpaceChar != fWSC
                    || m_words.GetAt(pos2)->m_fLineBreak) break;
            SharedPtrCWord w2 = m_words.GetNext(pos2);
            width += w2->m_width;
        }
        if((ret->m_width += width) <= maxwidth || ret->IsEmpty())
        {
            ret->AddTail(w->Copy());
            while(pos != pos2)
            {
                ret->AddTail(m_words.GetNext(pos)->Copy());
            }
            pos = pos2;
        }
        else
        {
            if(pos) m_words.GetPrev(pos);
            else pos = m_words.GetTailPosition();
            ret->m_width -= width;
            break;
        }
    }
    ret->Compact();
    return(ret);
}

void CSubtitle::CreateClippers(CSize size)
{
    size.cx >>= 3;
    size.cy >>= 3;
    if(m_effects[EF_BANNER] && m_effects[EF_BANNER]->param[2])
    {
        int width = m_effects[EF_BANNER]->param[2];
        int w = size.cx, h = size.cy;
        if(!m_pClipper)
        {
            CStringW str;
            str.Format(L"m %d %d l %d %d %d %d %d %d", 0, 0, w, 0, w, h, 0, h);
            m_pClipper = new CClipper(str, size, 1, 1, false);
            if(!m_pClipper) return;
        }
        int da = (64<<8)/width;
        BYTE* am = m_pClipper->m_pAlphaMask;
        for(int j = 0; j < h; j++, am += w)
        {
            int a = 0;
            int k = min(width, w);
            for(int i = 0; i < k; i++, a += da)
                am[i] = (am[i]*a)>>14;
            a = 0x40<<8;
            k = w-width;
            if(k < 0) {a -= -k*da; k = 0;}
            for(int i = k; i < w; i++, a -= da)
                am[i] = (am[i]*a)>>14;
        }
    }
    else if(m_effects[EF_SCROLL] && m_effects[EF_SCROLL]->param[4])
    {
        int height = m_effects[EF_SCROLL]->param[4];
        int w = size.cx, h = size.cy;
        if(!m_pClipper)
        {
            CStringW str;
            str.Format(L"m %d %d l %d %d %d %d %d %d", 0, 0, w, 0, w, h, 0, h);
            m_pClipper = new CClipper(str, size, 1, 1, false);
            if(!m_pClipper) return;
        }
        int da = (64<<8)/height;
        int a = 0;
        int k = m_effects[EF_SCROLL]->param[0]>>3;
        int l = k+height;
        if(k < 0) {a += -k*da; k = 0;}
        if(l > h) {l = h;}
        if(k < h)
        {
            BYTE* am = &m_pClipper->m_pAlphaMask[k*w];
            memset(m_pClipper->m_pAlphaMask, 0, am - m_pClipper->m_pAlphaMask);
            for(int j = k; j < l; j++, a += da)
            {
                for(int i = 0; i < w; i++, am++)
                    *am = ((*am)*a)>>14;
            }
        }
        da = -(64<<8)/height;
        a = 0x40<<8;
        l = m_effects[EF_SCROLL]->param[1]>>3;
        k = l-height;
        if(k < 0) {a += -k*da; k = 0;}
        if(l > h) {l = h;}
        if(k < h)
        {
            BYTE* am = &m_pClipper->m_pAlphaMask[k*w];
            int j = k;
            for(; j < l; j++, a += da)
            {
                for(int i = 0; i < w; i++, am++)
                    *am = ((*am)*a)>>14;
            }
            memset(am, 0, (h-j)*w);
        }
    }
}

void CSubtitle::MakeLines(CSize size, CRect marginRect)
{
    CSize spaceNeeded(0, 0);
    bool fFirstLine = true;
    m_topborder = m_bottomborder = 0;
    CLine* l = NULL;
    POSITION pos = m_words.GetHeadPosition();
    while(pos)
    {
        l = GetNextLine(pos, size.cx - marginRect.left - marginRect.right);
        if(!l) break;
        if(fFirstLine) {m_topborder = l->m_borderY; fFirstLine = false;}
        spaceNeeded.cx = max(l->m_width+l->m_borderX, spaceNeeded.cx);
        spaceNeeded.cy += l->m_ascent + l->m_descent;
        AddTail(l);
    }
    if(l) m_bottomborder = l->m_borderY;
    m_rect = CRect(
                 CPoint((m_scrAlignment%3) == 1 ? marginRect.left
                        : (m_scrAlignment%3) == 2 ? (marginRect.left + (size.cx - marginRect.right) - spaceNeeded.cx + 1) / 2
                        : (size.cx - marginRect.right - spaceNeeded.cx),
                        m_scrAlignment <= 3 ? (size.cy - marginRect.bottom - spaceNeeded.cy)
                        : m_scrAlignment <= 6 ? (marginRect.top + (size.cy - marginRect.bottom) - spaceNeeded.cy + 1) / 2
                        : marginRect.top),
                 spaceNeeded);
}

// CScreenLayoutAllocator

void CScreenLayoutAllocator::Empty()
{
    m_subrects.RemoveAll();
}

void CScreenLayoutAllocator::AdvanceToSegment(int segment, const CAtlArray<int>& sa)
{
    POSITION pos = m_subrects.GetHeadPosition();
    while(pos)
    {
        POSITION prev = pos;
        SubRect& sr = m_subrects.GetNext(pos);
        bool fFound = false;
        if(abs(sr.segment - segment) <= 1) // using abs() makes it possible to play the subs backwards, too :)
        {
            for(size_t i = 0; i < sa.GetCount() && !fFound; i++)
            {
                if(sa[i] == sr.entry)
                {
                    sr.segment = segment;
                    fFound = true;
                }
            }
        }
        if(!fFound) m_subrects.RemoveAt(prev);
    }
}

CRect CScreenLayoutAllocator::AllocRect(CSubtitle* s, int segment, int entry, int layer, int collisions)
{
    // TODO: handle collisions == 1 (reversed collisions)
    POSITION pos = m_subrects.GetHeadPosition();
    while(pos)
    {
        SubRect& sr = m_subrects.GetNext(pos);
        if(sr.segment == segment && sr.entry == entry)
        {
            return(sr.r + CRect(0, -s->m_topborder, 0, -s->m_bottomborder));
        }
    }
    CRect r = s->m_rect + CRect(0, s->m_topborder, 0, s->m_bottomborder);
    bool fSearchDown = s->m_scrAlignment > 3;
    bool fOK;
    do
    {
        fOK = true;
        pos = m_subrects.GetHeadPosition();
        while(pos)
        {
            SubRect& sr = m_subrects.GetNext(pos);
            if(layer == sr.layer && !(r & sr.r).IsRectEmpty())
            {
                if(fSearchDown)
                {
                    r.bottom = sr.r.bottom + r.Height();
                    r.top = sr.r.bottom;
                }
                else
                {
                    r.top = sr.r.top - r.Height();
                    r.bottom = sr.r.top;
                }
                fOK = false;
            }
        }
    }
    while(!fOK);
    SubRect sr;
    sr.r = r;
    sr.segment = segment;
    sr.entry = entry;
    sr.layer = layer;
    m_subrects.AddTail(sr);
    return(sr.r + CRect(0, -s->m_topborder, 0, -s->m_bottomborder));
}

// CRenderedTextSubtitle

CAtlMap<CStringW, CRenderedTextSubtitle::AssCmdType, CStringElementTraits<CStringW>> CRenderedTextSubtitle::m_cmdMap;

CRenderedTextSubtitle::CRenderedTextSubtitle(CCritSec* pLock)
    : ISubPicProviderImpl(pLock)
{
    if( m_cmdMap.IsEmpty() )
    {
        InitCmdMap();
    }
    m_size = CSize(0, 0);
    if(g_hDC_refcnt == 0)
    {
        g_hDC = CreateCompatibleDC(NULL);
        SetBkMode(g_hDC, TRANSPARENT);
        SetTextColor(g_hDC, 0xffffff);
        SetMapMode(g_hDC, MM_TEXT);
    }
    g_hDC_refcnt++;
}

CRenderedTextSubtitle::~CRenderedTextSubtitle()
{
    Deinit();
    g_hDC_refcnt--;
    if(g_hDC_refcnt == 0) DeleteDC(g_hDC);
}

void CRenderedTextSubtitle::InitCmdMap()
{
    if( m_cmdMap.IsEmpty() )
    {
        m_cmdMap.SetAt(L"1c",        CMD_1c);
        m_cmdMap.SetAt(L"2c",        CMD_2c);
        m_cmdMap.SetAt(L"3c",        CMD_3c);
        m_cmdMap.SetAt(L"4c",        CMD_4c);
        m_cmdMap.SetAt(L"1a",        CMD_1a);
        m_cmdMap.SetAt(L"2a",        CMD_2a);
        m_cmdMap.SetAt(L"3a",        CMD_3a);
        m_cmdMap.SetAt(L"4a",        CMD_4a);
        m_cmdMap.SetAt(L"alpha",     CMD_alpha);
        m_cmdMap.SetAt(L"an",        CMD_an);
        m_cmdMap.SetAt(L"a",         CMD_a);
        m_cmdMap.SetAt(L"blur",      CMD_blur);
        m_cmdMap.SetAt(L"bord",      CMD_bord);
        m_cmdMap.SetAt(L"be",        CMD_be);
        m_cmdMap.SetAt(L"b",         CMD_b);
        m_cmdMap.SetAt(L"clip",      CMD_clip);
        m_cmdMap.SetAt(L"iclip",     CMD_iclip);
        m_cmdMap.SetAt(L"c",         CMD_c);
        m_cmdMap.SetAt(L"fade",      CMD_fade);
        m_cmdMap.SetAt(L"fad",       CMD_fad);
        m_cmdMap.SetAt(L"fax",       CMD_fax);
        m_cmdMap.SetAt(L"fay",       CMD_fay);
        m_cmdMap.SetAt(L"fe",        CMD_fe);
        m_cmdMap.SetAt(L"fn",        CMD_fn);
        m_cmdMap.SetAt(L"frx",       CMD_frx);
        m_cmdMap.SetAt(L"fry",       CMD_fry);
        m_cmdMap.SetAt(L"frz",       CMD_frz);
        m_cmdMap.SetAt(L"fr",        CMD_fr);
        m_cmdMap.SetAt(L"fscx",      CMD_fscx);
        m_cmdMap.SetAt(L"fscy",      CMD_fscy);
        m_cmdMap.SetAt(L"fsc",       CMD_fsc);
        m_cmdMap.SetAt(L"fsp",       CMD_fsp);
        m_cmdMap.SetAt(L"fs",        CMD_fs);
        m_cmdMap.SetAt(L"i",         CMD_i);
        m_cmdMap.SetAt(L"kt",        CMD_kt);
        m_cmdMap.SetAt(L"kf",        CMD_kf);
        m_cmdMap.SetAt(L"K",         CMD_K);
        m_cmdMap.SetAt(L"ko",        CMD_ko);
        m_cmdMap.SetAt(L"k",         CMD_k);
        m_cmdMap.SetAt(L"move",      CMD_move);
        m_cmdMap.SetAt(L"org",       CMD_org);
        m_cmdMap.SetAt(L"pbo",       CMD_pbo);
        m_cmdMap.SetAt(L"pos",       CMD_pos);
        m_cmdMap.SetAt(L"p",         CMD_p);
        m_cmdMap.SetAt(L"q",         CMD_q);
        m_cmdMap.SetAt(L"r",         CMD_r);
        m_cmdMap.SetAt(L"shad",      CMD_shad);
        m_cmdMap.SetAt(L"s",         CMD_s);
        m_cmdMap.SetAt(L"t",         CMD_t);
        m_cmdMap.SetAt(L"u",         CMD_u);
        m_cmdMap.SetAt(L"xbord",     CMD_xbord);
        m_cmdMap.SetAt(L"xshad",     CMD_xshad);
        m_cmdMap.SetAt(L"ybord",     CMD_ybord);
        m_cmdMap.SetAt(L"yshad",     CMD_yshad);
    }
}

void CRenderedTextSubtitle::Copy(CSimpleTextSubtitle& sts)
{
    __super::Copy(sts);
    m_size = CSize(0, 0);
    if(CRenderedTextSubtitle* pRTS = dynamic_cast<CRenderedTextSubtitle*>(&sts))
    {
        m_size = pRTS->m_size;
    }
}

void CRenderedTextSubtitle::Empty()
{
    Deinit();
    __super::Empty();
}

void CRenderedTextSubtitle::OnChanged()
{
    __super::OnChanged();
    POSITION pos = m_subtitleCache.GetStartPosition();
    while(pos)
    {
        int i;
        CSubtitle* s;
        m_subtitleCache.GetNextAssoc(pos, i, s);
        delete s;
    }
    m_subtitleCache.RemoveAll();
    m_sla.Empty();
}

bool CRenderedTextSubtitle::Init(CSize size, CRect vidrect)
{
    Deinit();
    m_size = CSize(size.cx*8, size.cy*8);
    m_vidrect = CRect(vidrect.left*8, vidrect.top*8, vidrect.right*8, vidrect.bottom*8);
    m_sla.Empty();
    return(true);
}

void CRenderedTextSubtitle::Deinit()
{
    POSITION pos = m_subtitleCache.GetStartPosition();
    while(pos)
    {
        int i;
        CSubtitle* s;
        m_subtitleCache.GetNextAssoc(pos, i, s);
        delete s;
    }
    m_subtitleCache.RemoveAll();
    m_sla.Empty();
    m_size = CSize(0, 0);
    m_vidrect.SetRectEmpty();

    CacheManager::GetPathDataMruCache()->clear();
    CacheManager::GetCWordMruCache()->clear();
    CacheManager::GetOverlayNoBlurMruCache()->clear();
    CacheManager::GetOverlayMruCache()->clear();
}

void CRenderedTextSubtitle::ParseEffect(CSubtitle* sub, CString str)
{
    str.Trim();
    if(!sub || str.IsEmpty()) return;
    const TCHAR* s = _tcschr(str, ';');
    if(!s) {s = (LPTSTR)(LPCTSTR)str; s += str.GetLength()-1;}
    s++;
    CString effect = str.Left(s - str);
    if(!effect.CompareNoCase(_T("Banner;")))
    {
        int delay, lefttoright = 0, fadeawaywidth = 0;
        if(_stscanf(s, _T("%d;%d;%d"), &delay, &lefttoright, &fadeawaywidth) < 1) return;
        Effect* e = new Effect;
        if(!e) return;
        sub->m_effects[e->type = EF_BANNER] = e;
        e->param[0] = (int)(max(1.0*delay/sub->m_scalex, 1));
        e->param[1] = lefttoright;
        e->param[2] = (int)(sub->m_scalex*fadeawaywidth);
        sub->m_wrapStyle = 2;
    }
    else if(!effect.CompareNoCase(_T("Scroll up;")) || !effect.CompareNoCase(_T("Scroll down;")))
    {
        int top, bottom, delay, fadeawayheight = 0;
        if(_stscanf(s, _T("%d;%d;%d;%d"), &top, &bottom, &delay, &fadeawayheight) < 3) return;
        if(top > bottom) {int tmp = top; top = bottom; bottom = tmp;}
        Effect* e = new Effect;
        if(!e) return;
        sub->m_effects[e->type = EF_SCROLL] = e;
        e->param[0] = (int)(sub->m_scaley*top*8);
        e->param[1] = (int)(sub->m_scaley*bottom*8);
        e->param[2] = (int)(max(1.0*delay/sub->m_scaley, 1));
        e->param[3] = (effect.GetLength() == 12);
        e->param[4] = (int)(sub->m_scaley*fadeawayheight);
    }
}

void CRenderedTextSubtitle::ParseString(CSubtitle* sub, CStringW str, const FwSTSStyle& style)
{
    if(!sub) return;
    str.Replace(L"\\N", L"\n");
    str.Replace(L"\\n", (sub->m_wrapStyle < 2 || sub->m_wrapStyle == 3) ? L" " : L"\n");
    str.Replace(L"\\h", L"\x00A0");
    CWordMruCache* word_mru_cache=CacheManager::GetCWordMruCache();
    for(int ite = 0, j = 0, len = str.GetLength(); j <= len; j++)
    {
        WCHAR c = str[j];
        if(c != L'\n' && c != L' ' && c != L'\x00A0' && c != 0)
            continue;
        if(ite < j)
        {
            CWordCacheKey word_cache_key(style, str.Mid(ite, j-ite), m_ktype, m_kstart, m_kend);                        
            CWordMruCache::hashed_cache_const_iterator iter = word_mru_cache->hash_find(word_cache_key);
            if( iter != word_mru_cache->hash_end() )            
            {
                sub->m_words.AddTail(iter->word);
            }
            else if(PCWord tmp_ptr = new CText(style, str.Mid(ite, j-ite), m_ktype, m_kstart, m_kend))
            {
                SharedPtrCWord w(tmp_ptr);
                sub->m_words.AddTail(w);
                CWordMruItem item(word_cache_key, w);
                word_mru_cache->update_cache(item);
            }
            else
            {
                ///TODO: overflow handling
            }
            m_kstart = m_kend;
        }
        if(c == L'\n')
        {
            CWordCacheKey word_cache_key(style, CStringW(), m_ktype, m_kstart, m_kend);
            CWordMruCache::hashed_cache_const_iterator iter = word_mru_cache->hash_find(word_cache_key);
            if( iter != word_mru_cache->hash_end() )            
            {
                sub->m_words.AddTail(iter->word);
            }
            else if(PCWord tmp_ptr = new CText(style, CStringW(), m_ktype, m_kstart, m_kend))
            {
                SharedPtrCWord w(tmp_ptr);
                sub->m_words.AddTail(w);
                CWordMruItem item(word_cache_key, w);
                word_mru_cache->update_cache(item);
            }
            else
            {
                ///TODO: overflow handling
            }
            m_kstart = m_kend;
        }
        else if(c == L' ' || c == L'\x00A0')
        {
            CWordCacheKey word_cache_key(style, CStringW(c), m_ktype, m_kstart, m_kend);            
            CWordMruCache::hashed_cache_const_iterator iter = word_mru_cache->hash_find(word_cache_key);
            if( iter != word_mru_cache->hash_end() ) 
            {
                sub->m_words.AddTail(iter->word);
            }
            else if(PCWord tmp_ptr = new CText(style, CStringW(c), m_ktype, m_kstart, m_kend))
            {
                SharedPtrCWord w(tmp_ptr);
                sub->m_words.AddTail(w);
                CWordMruItem item(word_cache_key, w);
                word_mru_cache->update_cache(item);
            }
            else
            {
                ///TODO: overflow handling
            }
            m_kstart = m_kend;
        }
        ite = j+1;
    }
    return;
}

void CRenderedTextSubtitle::ParsePolygon(CSubtitle* sub, CStringW str, const FwSTSStyle& style)
{
    if(!sub || !str.GetLength() || !m_nPolygon) return;

    if(PCWord tmp_ptr = new CPolygon(style, str, m_ktype, m_kstart, m_kend, sub->m_scalex/(1<<(m_nPolygon-1)), sub->m_scaley/(1<<(m_nPolygon-1)), m_polygonBaselineOffset))
    {
        SharedPtrCWord w(tmp_ptr);
        ///Todo: fix me
        //if( PCWord w_cache = m_wordCache.lookup(*w) )
        //{
        //    sub->m_words.AddTail(w_cache);
        //    delete w;
        //}
        //else
        {
            sub->m_words.AddTail(w);            
        }
        m_kstart = m_kend;
    }
}

bool CRenderedTextSubtitle::ParseSSATag(CSubtitle* sub, CStringW str, STSStyle& style, const STSStyle& org, bool fAnimate)
{
    if(!sub) return(false);
    int nTags = 0, nUnrecognizedTags = 0;
    for(int i = 0, j; (j = str.Find(L'\\', i)) >= 0; i = j)
    {
        CStringW cmd;
        for(WCHAR c = str[++j]; c && c != L'(' && c != L'\\'; cmd += c, c = str[++j]);
        cmd.Trim();
        if(cmd.IsEmpty()) continue;
        CAtlArray<CStringW> params;
        if(str[j] == L'(')
        {
            CStringW param;
            for(WCHAR c = str[++j]; c && c != L')'; param += c, c = str[++j]);
            param.Trim();
            while(!param.IsEmpty())
            {
                int i = param.Find(L','), j = param.Find(L'\\');
                if(i >= 0 && (j < 0 || i < j))
                {
                    CStringW s = param.Left(i).Trim();
                    if(!s.IsEmpty()) params.Add(s);
                    param = i+1 < param.GetLength() ? param.Mid(i+1) : L"";
                }
                else
                {
                    param.Trim();
                    if(!param.IsEmpty()) params.Add(param);
                    param.Empty();
                }
            }
        }

        AssCmdType cmd_type = CMD_COUNT;
        int cmd_length = min(MAX_CMD_LENGTH, cmd.GetLength());
        for( ;cmd_length>=MIN_CMD_LENGTH;cmd_length-- )
        {
            if( m_cmdMap.Lookup(cmd.Left(cmd_length), cmd_type) )
                break;
        }
        if(cmd_length<MIN_CMD_LENGTH)
            cmd_type = CMD_COUNT;
        switch( cmd_type )
        {
        case CMD_fax:
        case CMD_fay:
        case CMD_fe:
        case CMD_fn:
        case CMD_frx:
        case CMD_fry:
        case CMD_frz:
        case CMD_fr:
        case CMD_fscx:
        case CMD_fscy:
        case CMD_fsc:
        case CMD_fsp:
        case CMD_fs:
        case CMD_i:
        case CMD_kt:
        case CMD_kf:
        case CMD_K:
        case CMD_ko:
        case CMD_k:
        case CMD_pbo:
        case CMD_p:
        case CMD_q:
        case CMD_r:
        case CMD_shad:
        case CMD_s:
        case CMD_an:
        case CMD_a:
        case CMD_blur:
        case CMD_bord:
        case CMD_be:
        case CMD_b:
        case CMD_u:
        case CMD_xbord:
        case CMD_xshad:
        case CMD_ybord:
        case CMD_yshad:
//        default:
            params.Add(cmd.Mid(cmd_length));
            break;
        case CMD_c:
        case CMD_1c :
        case CMD_2c :
        case CMD_3c :
        case CMD_4c :
        case CMD_1a :
        case CMD_2a :
        case CMD_3a :
        case CMD_4a :
        case CMD_alpha:
            params.Add(cmd.Mid(cmd_length).Trim(L"&H"));
            break;
        case CMD_clip:
        case CMD_iclip:
        case CMD_fade:
        case CMD_fad:
        case CMD_move:
        case CMD_org:
        case CMD_pos:
        case CMD_t:
            break;
        case CMD_COUNT:
            nUnrecognizedTags++;
            break;
        }
        nTags++;
        // TODO: call ParseStyleModifier(cmd, params, ..) and move the rest there
        CStringW p = params.GetCount() > 0 ? params[0] : L"";
        switch ( cmd_type )
        {
        case CMD_1c :
        case CMD_2c :
        case CMD_3c :
        case CMD_4c :
            {
                int i = cmd[0] - L'1';
                DWORD c = wcstol(p, NULL, 16);
                style.colors[i] = !p.IsEmpty()
                                  ? (((int)CalcAnimation(c&0xff, style.colors[i]&0xff, fAnimate))&0xff
                                     |((int)CalcAnimation(c&0xff00, style.colors[i]&0xff00, fAnimate))&0xff00
                                     |((int)CalcAnimation(c&0xff0000, style.colors[i]&0xff0000, fAnimate))&0xff0000)
                                  : org.colors[i];
                break;
            }
        case CMD_1a :
        case CMD_2a :
        case CMD_3a :
        case CMD_4a :
            {
                int i = cmd[0] - L'1';
                style.alpha[i] = !p.IsEmpty()
                                 ? (BYTE)CalcAnimation(wcstol(p, NULL, 16), style.alpha[i], fAnimate)
                                 : org.alpha[i];
                break;
            }
        case CMD_alpha:
            {
                for(int i = 0; i < 4; i++)
                {
                    style.alpha[i] = !p.IsEmpty()
                                     ? (BYTE)CalcAnimation(wcstol(p, NULL, 16), style.alpha[i], fAnimate)
                                     : org.alpha[i];
                }
                break;
            }
        case CMD_an:
            {
                int n = wcstol(p, NULL, 10);
                if(sub->m_scrAlignment < 0)
                    sub->m_scrAlignment = (n > 0 && n < 10) ? n : org.scrAlignment;
                break;
            }
        case CMD_a:
            {
                int n = wcstol(p, NULL, 10);
                if(sub->m_scrAlignment < 0)
                    sub->m_scrAlignment = (n > 0 && n < 12) ? ((((n-1)&3)+1)+((n&4)?6:0)+((n&8)?3:0)) : org.scrAlignment;
                break;
            }
        case CMD_blur:
            {
                double n = CalcAnimation(wcstod(p, NULL), style.fGaussianBlur, fAnimate);
                style.fGaussianBlur = !p.IsEmpty()
                                      ? (n < 0 ? 0 : n)
                                          : org.fGaussianBlur;
                break;
            }
        case CMD_bord:
            {
                double dst = wcstod(p, NULL);
                double nx = CalcAnimation(dst, style.outlineWidthX, fAnimate);
                style.outlineWidthX = !p.IsEmpty()
                                      ? (nx < 0 ? 0 : nx)
                                          : org.outlineWidthX;
                double ny = CalcAnimation(dst, style.outlineWidthY, fAnimate);
                style.outlineWidthY = !p.IsEmpty()
                                      ? (ny < 0 ? 0 : ny)
                                          : org.outlineWidthY;
                break;
            }
        case CMD_be:
            {
                int n = (int)(CalcAnimation(wcstol(p, NULL, 10), style.fBlur, fAnimate)+0.5);
                style.fBlur = !p.IsEmpty()
                              ? n
                              : org.fBlur;
                break;
            }
        case CMD_b:
            {
                int n = wcstol(p, NULL, 10);
                style.fontWeight = !p.IsEmpty()
                                   ? (n == 0 ? FW_NORMAL : n == 1 ? FW_BOLD : n >= 100 ? n : org.fontWeight)
                                       : org.fontWeight;
                break;
            }
        case CMD_clip:
        case CMD_iclip:
            {
                bool invert = (cmd_type == CMD_iclip);
                if(params.GetCount() == 1 && !sub->m_pClipper)
                {
                    sub->m_pClipper = new CClipper(params[0], CSize(m_size.cx>>3, m_size.cy>>3), sub->m_scalex, sub->m_scaley, invert);
                }
                else if(params.GetCount() == 2 && !sub->m_pClipper)
                {
                    int scale = max(wcstol(p, NULL, 10), 1);
                    sub->m_pClipper = new CClipper(params[1], CSize(m_size.cx>>3, m_size.cy>>3), sub->m_scalex/(1<<(scale-1)), sub->m_scaley/(1<<(scale-1)), invert);
                }
                else if(params.GetCount() == 4)
                {
                    CRect r;
                    sub->m_clipInverse = invert;
                    r.SetRect(
                        wcstol(params[0], NULL, 10),
                        wcstol(params[1], NULL, 10),
                        wcstol(params[2], NULL, 10),
                        wcstol(params[3], NULL, 10));
                    CPoint o(0, 0);
                    if(sub->m_relativeTo == 1) // TODO: this should also apply to the other two clippings above
                    {
                        o.x = m_vidrect.left>>3;
                        o.y = m_vidrect.top>>3;
                    }
                    sub->m_clip.SetRect(
                        (int)CalcAnimation(sub->m_scalex*r.left + o.x, sub->m_clip.left, fAnimate),
                        (int)CalcAnimation(sub->m_scaley*r.top + o.y, sub->m_clip.top, fAnimate),
                        (int)CalcAnimation(sub->m_scalex*r.right + o.x, sub->m_clip.right, fAnimate),
                        (int)CalcAnimation(sub->m_scaley*r.bottom + o.y, sub->m_clip.bottom, fAnimate));
                }
                break;
            }
        case CMD_c:
            {
                DWORD c = wcstol(p, NULL, 16);
                style.colors[0] = !p.IsEmpty()
                                  ? (((int)CalcAnimation(c&0xff, style.colors[0]&0xff, fAnimate))&0xff
                                     |((int)CalcAnimation(c&0xff00, style.colors[0]&0xff00, fAnimate))&0xff00
                                     |((int)CalcAnimation(c&0xff0000, style.colors[0]&0xff0000, fAnimate))&0xff0000)
                                  : org.colors[0];
                break;
            }
        case CMD_fade:
        case CMD_fad:
            {
                if(params.GetCount() == 7 && !sub->m_effects[EF_FADE])// {\fade(a1=param[0], a2=param[1], a3=param[2], t1=t[0], t2=t[1], t3=t[2], t4=t[3])
                {
                    if(Effect* e = new Effect)
                    {
                        for(int i = 0; i < 3; i++)
                            e->param[i] = wcstol(params[i], NULL, 10);
                        for(int i = 0; i < 4; i++)
                            e->t[i] = wcstol(params[3+i], NULL, 10);
                        sub->m_effects[EF_FADE] = e;
                    }
                }
                else if(params.GetCount() == 2 && !sub->m_effects[EF_FADE]) // {\fad(t1=t[1], t2=t[2])
                {
                    if(Effect* e = new Effect)
                    {
                        e->param[0] = e->param[2] = 0xff;
                        e->param[1] = 0x00;
                        for(int i = 1; i < 3; i++)
                            e->t[i] = wcstol(params[i-1], NULL, 10);
                        e->t[0] = e->t[3] = -1; // will be substituted with "start" and "end"
                        sub->m_effects[EF_FADE] = e;
                    }
                }
                break;
            }
        case CMD_fax:
            {
                style.fontShiftX = !p.IsEmpty()
                                   ? CalcAnimation(wcstod(p, NULL), style.fontShiftX, fAnimate)
                                   : org.fontShiftX;
                break;
            }
        case CMD_fay:
            {
                style.fontShiftY = !p.IsEmpty()
                                   ? CalcAnimation(wcstod(p, NULL), style.fontShiftY, fAnimate)
                                   : org.fontShiftY;
                break;
            }
        case CMD_fe:
            {
                int n = wcstol(p, NULL, 10);
                style.charSet = !p.IsEmpty()
                                ? n
                                : org.charSet;
                break;
            }
        case CMD_fn:
            {
                if(!p.IsEmpty() && p != L'0')
                    style.fontName = CString(p).Trim();
                else
                    style.fontName = org.fontName;
                break;
            }
        case CMD_frx:
            {
                style.fontAngleX = !p.IsEmpty()
                                   ? CalcAnimation(wcstod(p, NULL), style.fontAngleX, fAnimate)
                                   : org.fontAngleX;
                break;
            }
        case CMD_fry:
            {
                style.fontAngleY = !p.IsEmpty()
                                   ? CalcAnimation(wcstod(p, NULL), style.fontAngleY, fAnimate)
                                   : org.fontAngleY;
                break;
            }
        case CMD_frz:
        case CMD_fr:
            {
                style.fontAngleZ = !p.IsEmpty()
                                   ? CalcAnimation(wcstod(p, NULL), style.fontAngleZ, fAnimate)
                                   : org.fontAngleZ;
                break;
            }
        case CMD_fscx:
            {
                double n = CalcAnimation(wcstol(p, NULL, 10), style.fontScaleX, fAnimate);
                style.fontScaleX = !p.IsEmpty()
                                   ? ((n < 0) ? 0 : n)
                                       : org.fontScaleX;
                break;
            }
        case CMD_fscy:
            {
                double n = CalcAnimation(wcstol(p, NULL, 10), style.fontScaleY, fAnimate);
                style.fontScaleY = !p.IsEmpty()
                                   ? ((n < 0) ? 0 : n)
                                       : org.fontScaleY;
                break;
            }
        case CMD_fsc:
            {
                style.fontScaleX = org.fontScaleX;
                style.fontScaleY = org.fontScaleY;
                break;
            }
        case CMD_fsp:
            {
                style.fontSpacing = !p.IsEmpty()
                                    ? CalcAnimation(wcstod(p, NULL), style.fontSpacing, fAnimate)
                                    : org.fontSpacing;
                break;
            }
        case CMD_fs:
            {
                if(!p.IsEmpty())
                {
                    if(p[0] == L'-' || p[0] == L'+')
                    {
                        double n = CalcAnimation(style.fontSize + style.fontSize*wcstol(p, NULL, 10)/10, style.fontSize, fAnimate);
                        style.fontSize = (n > 0) ? n : org.fontSize;
                    }
                    else
                    {
                        double n = CalcAnimation(wcstol(p, NULL, 10), style.fontSize, fAnimate);
                        style.fontSize = (n > 0) ? n : org.fontSize;
                    }
                }
                else
                {
                    style.fontSize = org.fontSize;
                }
                break;
            }
        case CMD_i:
            {
                int n = wcstol(p, NULL, 10);
                style.fItalic = !p.IsEmpty()
                                ? (n == 0 ? false : n == 1 ? true : org.fItalic)
                                    : org.fItalic;
                break;
            }
        case CMD_kt:
            {
                m_kstart = !p.IsEmpty()
                           ? wcstol(p, NULL, 10)*10
                           : 0;
                m_kend = m_kstart;
                break;
                sub->m_fAnimated2 = true;//fix me: define m_fAnimated m_fAnimated2 strictly
            }
        case CMD_kf:
        case CMD_K:
            {
                m_ktype = 1;
                m_kstart = m_kend;
                m_kend += !p.IsEmpty()
                          ? wcstol(p, NULL, 10)*10
                          : 1000;
                sub->m_fAnimated2 = true;//fix me: define m_fAnimated m_fAnimated2 strictly
                break;
            }
        case CMD_ko:
            {
                m_ktype = 2;
                m_kstart = m_kend;
                m_kend += !p.IsEmpty()
                          ? wcstol(p, NULL, 10)*10
                          : 1000;
                sub->m_fAnimated2 = true;//fix me: define m_fAnimated m_fAnimated2 strictly
                break;
            }
        case CMD_k:
            {
                m_ktype = 0;
                m_kstart = m_kend;
                m_kend += !p.IsEmpty()
                          ? wcstol(p, NULL, 10)*10
                          : 1000;
                sub->m_fAnimated2 = true;//fix me: define m_fAnimated m_fAnimated2 strictly
                break;
            }
        case CMD_move: // {\move(x1=param[0], y1=param[1], x2=param[2], y2=param[3][, t1=t[0], t2=t[1]])}
            {
                if((params.GetCount() == 4 || params.GetCount() == 6) && !sub->m_effects[EF_MOVE])
                {
                    if(Effect* e = new Effect)
                    {
                        e->param[0] = (int)(sub->m_scalex*wcstod(params[0], NULL)*8);
                        e->param[1] = (int)(sub->m_scaley*wcstod(params[1], NULL)*8);
                        e->param[2] = (int)(sub->m_scalex*wcstod(params[2], NULL)*8);
                        e->param[3] = (int)(sub->m_scaley*wcstod(params[3], NULL)*8);
                        e->t[0] = e->t[1] = -1;
                        if(params.GetCount() == 6)
                        {
                            for(int i = 0; i < 2; i++)
                                e->t[i] = wcstol(params[4+i], NULL, 10);
                        }
                        sub->m_effects[EF_MOVE] = e;
                    }
                }
                break;
            }
        case CMD_org: // {\org(x=param[0], y=param[1])}
            {
                if(params.GetCount() == 2 && !sub->m_effects[EF_ORG])
                {
                    if(Effect* e = new Effect)
                    {
                        e->param[0] = (int)(sub->m_scalex*wcstod(params[0], NULL)*8);
                        e->param[1] = (int)(sub->m_scaley*wcstod(params[1], NULL)*8);
                        sub->m_effects[EF_ORG] = e;
                    }
                }
                break;
            }
        case CMD_pbo:
            {
                m_polygonBaselineOffset = wcstol(p, NULL, 10);
                break;
            }
        case CMD_pos:
            {
                if(params.GetCount() == 2 && !sub->m_effects[EF_MOVE])
                {
                    if(Effect* e = new Effect)
                    {
                        e->param[0] = e->param[2] = (int)(sub->m_scalex*wcstod(params[0], NULL)*8);
                        e->param[1] = e->param[3] = (int)(sub->m_scaley*wcstod(params[1], NULL)*8);
                        e->t[0] = e->t[1] = 0;
                        sub->m_effects[EF_MOVE] = e;
                    }
                }
                break;
            }
        case CMD_p:
            {
                int n = wcstol(p, NULL, 10);
                m_nPolygon = (n <= 0 ? 0 : n);
                break;
            }
        case CMD_q:
            {
                int n = wcstol(p, NULL, 10);
                sub->m_wrapStyle = !p.IsEmpty() && (0 <= n && n <= 3)
                                   ? n
                                   : m_defaultWrapStyle;
                break;
            }
        case CMD_r:
            {
                STSStyle* val;
                style = (!p.IsEmpty() && m_styles.Lookup(FwString(WToT(p)), val) && val) ? *val : org;
                break;
            }
        case CMD_shad:
            {
                double dst = wcstod(p, NULL);
                double nx = CalcAnimation(dst, style.shadowDepthX, fAnimate);
                style.shadowDepthX = !p.IsEmpty()
                                     ? (nx < 0 ? 0 : nx)
                                         : org.shadowDepthX;
                double ny = CalcAnimation(dst, style.shadowDepthY, fAnimate);
                style.shadowDepthY = !p.IsEmpty()
                                     ? (ny < 0 ? 0 : ny)
                                         : org.shadowDepthY;
                break;
            }
        case CMD_s:
            {
                int n = wcstol(p, NULL, 10);
                style.fStrikeOut = !p.IsEmpty()
                                   ? (n == 0 ? false : n == 1 ? true : org.fStrikeOut)
                                       : org.fStrikeOut;
                break;
            }
        case CMD_t: // \t([<t1>,<t2>,][<accel>,]<style modifiers>)
            {
                p.Empty();
                m_animStart = m_animEnd = 0;
                m_animAccel = 1;
                if(params.GetCount() == 1)
                {
                    p = params[0];
                }
                else if(params.GetCount() == 2)
                {
                    m_animAccel = wcstod(params[0], NULL);
                    p = params[1];
                }
                else if(params.GetCount() == 3)
                {
                    m_animStart = (int)wcstod(params[0], NULL);
                    m_animEnd = (int)wcstod(params[1], NULL);
                    p = params[2];
                }
                else if(params.GetCount() == 4)
                {
                    m_animStart = wcstol(params[0], NULL, 10);
                    m_animEnd = wcstol(params[1], NULL, 10);
                    m_animAccel = wcstod(params[2], NULL);
                    p = params[3];
                }
                ParseSSATag(sub, p, style, org, true);
                sub->m_fAnimated = true;
                break;
            }
        case CMD_u:
            {
                int n = wcstol(p, NULL, 10);
                style.fUnderline = !p.IsEmpty()
                                   ? (n == 0 ? false : n == 1 ? true : org.fUnderline)
                                       : org.fUnderline;
                break;
            }
        case CMD_xbord:
            {
                double dst = wcstod(p, NULL);
                double nx = CalcAnimation(dst, style.outlineWidthX, fAnimate);
                style.outlineWidthX = !p.IsEmpty()
                                      ? (nx < 0 ? 0 : nx)
                                          : org.outlineWidthX;
                break;
            }
        case CMD_xshad:
            {
                double dst = wcstod(p, NULL);
                double nx = CalcAnimation(dst, style.shadowDepthX, fAnimate);
                style.shadowDepthX = !p.IsEmpty()
                                     ? nx
                                     : org.shadowDepthX;
                break;
            }
        case CMD_ybord:
            {
                double dst = wcstod(p, NULL);
                double ny = CalcAnimation(dst, style.outlineWidthY, fAnimate);
                style.outlineWidthY = !p.IsEmpty()
                                      ? (ny < 0 ? 0 : ny)
                                          : org.outlineWidthY;
                break;
            }
        case CMD_yshad:
            {
                double dst = wcstod(p, NULL);
                double ny = CalcAnimation(dst, style.shadowDepthY, fAnimate);
                style.shadowDepthY = !p.IsEmpty()
                                     ? ny
                                     : org.shadowDepthY;
                break;
            }
        default:
            break;
        }
    }
//  return(nUnrecognizedTags < nTags);
    return(true); // there are ppl keeping coments inside {}, lets make them happy now
}

bool CRenderedTextSubtitle::ParseHtmlTag(CSubtitle* sub, CStringW str, STSStyle& style, STSStyle& org)
{
    if(str.Find(L"!--") == 0)
        return(true);
    bool fClosing = str[0] == L'/';
    str.Trim(L" /");
    int i = str.Find(L' ');
    if(i < 0) i = str.GetLength();
    CStringW tag = str.Left(i).MakeLower();
    str = str.Mid(i).Trim();
    CAtlArray<CStringW> attribs, params;
    while((i = str.Find(L'=')) > 0)
    {
        attribs.Add(str.Left(i).Trim().MakeLower());
        str = str.Mid(i+1);
        for(i = 0; _istspace(str[i]); i++);
        str = str.Mid(i);
        if(str[0] == L'\"') {str = str.Mid(1); i = str.Find(L'\"');}
        else i = str.Find(L' ');
        if(i < 0) i = str.GetLength();
        params.Add(str.Left(i).Trim().MakeLower());
        str = str.Mid(i+1);
    }
    if(tag == L"text")
        ;
    else if(tag == L"b" || tag == L"strong")
        style.fontWeight = !fClosing ? FW_BOLD : org.fontWeight;
    else if(tag == L"i" || tag == L"em")
        style.fItalic = !fClosing ? true : org.fItalic;
    else if(tag == L"u")
        style.fUnderline = !fClosing ? true : org.fUnderline;
    else if(tag == L"s" || tag == L"strike" || tag == L"del")
        style.fStrikeOut = !fClosing ? true : org.fStrikeOut;
    else if(tag == L"font")
    {
        if(!fClosing)
        {
            for(size_t i = 0; i < attribs.GetCount(); i++)
            {
                if(params[i].IsEmpty()) continue;
                int nColor = -1;
                if(attribs[i] == L"face")
                {
                    style.fontName = params[i];
                }
                else if(attribs[i] == L"size")
                {
                    if(params[i][0] == L'+')
                        style.fontSize += wcstol(params[i], NULL, 10);
                    else if(params[i][0] == L'-')
                        style.fontSize -= wcstol(params[i], NULL, 10);
                    else
                        style.fontSize = wcstol(params[i], NULL, 10);
                }
                else if(attribs[i] == L"color")
                {
                    nColor = 0;
                }
                else if(attribs[i] == L"outline-color")
                {
                    nColor = 2;
                }
                else if(attribs[i] == L"outline-level")
                {
                    style.outlineWidthX = style.outlineWidthY = wcstol(params[i], NULL, 10);
                }
                else if(attribs[i] == L"shadow-color")
                {
                    nColor = 3;
                }
                else if(attribs[i] == L"shadow-level")
                {
                    style.shadowDepthX = style.shadowDepthY = wcstol(params[i], NULL, 10);
                }
                if(nColor >= 0 && nColor < 4)
                {
                    CString key = WToT(params[i]).TrimLeft(L'#');
                    DWORD val;
                    if(g_colors.Lookup(key, val))
                        style.colors[nColor] = val;
                    else if((style.colors[nColor] = _tcstol(key, NULL, 16)) == 0)
                        style.colors[nColor] = 0x00ffffff;  // default is white
                    style.colors[nColor] = ((style.colors[nColor]>>16)&0xff)|((style.colors[nColor]&0xff)<<16)|(style.colors[nColor]&0x00ff00);
                }
            }
        }
        else
        {
            style.fontName = org.fontName;
            style.fontSize = org.fontSize;
            memcpy(style.colors, org.colors, sizeof(style.colors));
        }
    }
    else if(tag == L"k" && attribs.GetCount() == 1 && attribs[0] == L"t")
    {
        m_ktype = 1;
        m_kstart = m_kend;
        m_kend += wcstol(params[0], NULL, 10);
    }
    else
        return(false);
    return(true);
}

double CRenderedTextSubtitle::CalcAnimation(double dst, double src, bool fAnimate)
{
    int s = m_animStart ? m_animStart : 0;
    int e = m_animEnd ? m_animEnd : m_delay;
    if(fabs(dst-src) >= 0.0001 && fAnimate)
    {
        if(m_time < s) dst = src;
        else if(s <= m_time && m_time < e)
        {
            double t = pow(1.0 * (m_time - s) / (e - s), m_animAccel);
            dst = (1 - t) * src + t * dst;
        }
//      else dst = dst;
    }
    return(dst);
}

CSubtitle* CRenderedTextSubtitle::GetSubtitle(int entry)
{
    CSubtitle* sub;
    if(m_subtitleCache.Lookup(entry, sub))
    {
        if(sub->m_fAnimated) {delete sub; sub = NULL;}
        else return(sub);
    }
    sub = new CSubtitle();
    if(!sub) return(NULL);
    CStringW str = GetStrW(entry, true);
    STSStyle stss, orgstss;
    GetStyle(entry, &stss);
    if (stss.fontScaleX == stss.fontScaleY && m_dPARCompensation != 1.0)
    {
        switch(m_ePARCompensationType)
        {
        case EPCTUpscale:
            if (m_dPARCompensation < 1.0)
                stss.fontScaleY /= m_dPARCompensation;
            else
                stss.fontScaleX *= m_dPARCompensation;
            break;
        case EPCTDownscale:
            if (m_dPARCompensation < 1.0)
                stss.fontScaleX *= m_dPARCompensation;
            else
                stss.fontScaleY /= m_dPARCompensation;
            break;
        case EPCTAccurateSize:
            stss.fontScaleX *= m_dPARCompensation;
            break;
        }
    }
    orgstss = stss;
    sub->m_clip.SetRect(0, 0, m_size.cx>>3, m_size.cy>>3);
    sub->m_scrAlignment = -stss.scrAlignment;
    sub->m_wrapStyle = m_defaultWrapStyle;
    sub->m_fAnimated = false;
    sub->m_relativeTo = stss.relativeTo;
    sub->m_scalex = m_dstScreenSize.cx > 0 ? 1.0 * (stss.relativeTo == 1 ? m_vidrect.Width() : m_size.cx) / (m_dstScreenSize.cx*8) : 1.0;
    sub->m_scaley = m_dstScreenSize.cy > 0 ? 1.0 * (stss.relativeTo == 1 ? m_vidrect.Height() : m_size.cy) / (m_dstScreenSize.cy*8) : 1.0;
    m_animStart = m_animEnd = 0;
    m_animAccel = 1;
    m_ktype = m_kstart = m_kend = 0;
    m_nPolygon = 0;
    m_polygonBaselineOffset = 0;
    ParseEffect(sub, GetAt(entry).effect);
    while(!str.IsEmpty())
    {
        bool fParsed = false;
        int i;
        if(str[0] == L'{' && (i = str.Find(L'}')) > 0)
        {
            if(fParsed = ParseSSATag(sub, str.Mid(1, i-1), stss, orgstss))
                str = str.Mid(i+1);
        }
        else if(str[0] == L'<' && (i = str.Find(L'>')) > 0)
        {
            if(fParsed = ParseHtmlTag(sub, str.Mid(1, i-1), stss, orgstss))
                str = str.Mid(i+1);
        }
        if(fParsed)
        {
            i = str.FindOneOf(L"{<");
            if(i < 0) i = str.GetLength();
            if(i == 0) continue;
        }
        else
        {
            i = str.Mid(1).FindOneOf(L"{<");
            if(i < 0) i = str.GetLength()-1;
            i++;
        }
        STSStyle tmp = stss;
        tmp.fontSize = sub->m_scaley*tmp.fontSize*64;
        tmp.fontSpacing = sub->m_scalex*tmp.fontSpacing*64;
        tmp.outlineWidthX *= (m_fScaledBAS ? sub->m_scalex : 1) * 8;
        tmp.outlineWidthY *= (m_fScaledBAS ? sub->m_scaley : 1) * 8;
        tmp.shadowDepthX *= (m_fScaledBAS ? sub->m_scalex : 1) * 8;
        tmp.shadowDepthY *= (m_fScaledBAS ? sub->m_scaley : 1) * 8;
        FwSTSStyle fw_tmp(tmp);
        if(m_nPolygon)
        {
            ParsePolygon(sub, str.Left(i), fw_tmp);
        }
        else
        {
            ParseString(sub, str.Left(i), fw_tmp);
        }
        str = str.Mid(i);
    }
    sub->m_fAnimated2 |= sub->m_fAnimated;
    if( sub->m_effects[EF_FADE] || sub->m_effects[EF_BANNER] || sub->m_effects[EF_SCROLL]
            || sub->m_effects[EF_MOVE] )
        sub->m_fAnimated2 = true;
    // just a "work-around" solution... in most cases nobody will want to use \org together with moving but without rotating the subs
    if(sub->m_effects[EF_ORG] && (sub->m_effects[EF_MOVE] || sub->m_effects[EF_BANNER] || sub->m_effects[EF_SCROLL]))
        sub->m_fAnimated = true;
    sub->m_scrAlignment = abs(sub->m_scrAlignment);
    STSEntry stse = GetAt(entry);
    CRect marginRect = stse.marginRect;
    if(marginRect.left == 0) marginRect.left = orgstss.marginRect.get().left;
    if(marginRect.top == 0) marginRect.top = orgstss.marginRect.get().top;
    if(marginRect.right == 0) marginRect.right = orgstss.marginRect.get().right;
    if(marginRect.bottom == 0) marginRect.bottom = orgstss.marginRect.get().bottom;
    marginRect.left = (int)(sub->m_scalex*marginRect.left*8);
    marginRect.top = (int)(sub->m_scaley*marginRect.top*8);
    marginRect.right = (int)(sub->m_scalex*marginRect.right*8);
    marginRect.bottom = (int)(sub->m_scaley*marginRect.bottom*8);
    if(stss.relativeTo == 1)
    {
        marginRect.left += m_vidrect.left;
        marginRect.top += m_vidrect.top;
        marginRect.right += m_size.cx - m_vidrect.right;
        marginRect.bottom += m_size.cy - m_vidrect.bottom;
    }
    sub->CreateClippers(m_size);
    sub->MakeLines(m_size, marginRect);
    m_subtitleCache[entry] = sub;
    return(sub);
}

//

STDMETHODIMP CRenderedTextSubtitle::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
    CheckPointer(ppv, E_POINTER);
    *ppv = NULL;
    return
        QI(IPersist)
        QI(ISubStream)
        QI(ISubPicProvider)
        __super::NonDelegatingQueryInterface(riid, ppv);
}

// ISubPicProvider

STDMETHODIMP_(POSITION) CRenderedTextSubtitle::GetStartPosition(REFERENCE_TIME rt, double fps)
{
    //DbgLog((LOG_TRACE, 3, "rt:%lu", (ULONG)rt/10000));
    m_fps = fps;//fix me: check is fps changed and do some re-init thing
    int iSegment;
    int subIndex = 1;//If a segment has animate effect then it corresponds to several subpics.
    //subIndex, 1 based, indicates which subpic the result corresponds to.
    rt /= 10000i64;
    const STSSegment *stss = SearchSubs((int)rt, fps, &iSegment, NULL);
    if(stss==NULL)
        return NULL;
    else if(stss->animated)
    {
        int start = TranslateSegmentStart(iSegment, fps);
        if(rt > start)
            subIndex = (rt-start)/RTS_ANIMATE_SUBPIC_DUR + 1;
    }
    //DbgLog((LOG_TRACE, 3, "animated:%d seg:%d idx:%d DUR:%d rt:%lu", stss->animated, iSegment, subIndex, RTS_ANIMATE_SUBPIC_DUR, (ULONG)rt/10000));
    return (POSITION)(subIndex | (iSegment<<RTS_POS_SEGMENT_INDEX_BITS));
    //if(iSegment < 0) iSegment = 0;
    //return(GetNext((POSITION)iSegment));
}

STDMETHODIMP_(POSITION) CRenderedTextSubtitle::GetNext(POSITION pos)
{
    int iSegment = ((int)pos>>RTS_POS_SEGMENT_INDEX_BITS);
    int subIndex = ((int)pos & RTS_POS_SUB_INDEX_MASK);
    const STSSegment *stss = GetSegment(iSegment);
    ASSERT(stss!=NULL && stss->subs.GetCount()>0);
    //DbgLog((LOG_TRACE, 3, "stss:%x count:%d", stss, stss->subs.GetCount()));
    if(!stss->animated)
    {
        iSegment++;
        subIndex = 1;
    }
    else
    {
        int start, end;
        TranslateSegmentStartEnd(iSegment, m_fps, start, end);
        if(start+RTS_ANIMATE_SUBPIC_DUR*subIndex < end)
            subIndex++;
        else
        {
            iSegment++;
            subIndex = 1;
        }
    }
    if(GetSegment(iSegment) != NULL)
    {
        ASSERT(GetSegment(iSegment)->subs.GetCount()>0);
        return (POSITION)(subIndex | (iSegment<<RTS_POS_SEGMENT_INDEX_BITS));
    }
    else
        return NULL;
}

//@return: <0 if segment not found
STDMETHODIMP_(REFERENCE_TIME) CRenderedTextSubtitle::GetStart(POSITION pos, double fps)
{
    //return(10000i64 * TranslateSegmentStart((int)pos-1, fps));
    int iSegment = ((int)pos>>RTS_POS_SEGMENT_INDEX_BITS);
    int subIndex = ((int)pos & RTS_POS_SUB_INDEX_MASK);
    int start = TranslateSegmentStart(iSegment, fps);
    const STSSegment *stss = GetSegment(iSegment);
    if(stss!=NULL)
    {
        return (start + (subIndex-1)*RTS_ANIMATE_SUBPIC_DUR)*10000i64;
    }
    else
    {
        return -1;
    }
}

//@return: <0 if segment not found
STDMETHODIMP_(REFERENCE_TIME) CRenderedTextSubtitle::GetStop(POSITION pos, double fps)
{
//  return(10000i64 * TranslateSegmentEnd((int)pos-1, fps));
    int iSegment = ((int)pos>>RTS_POS_SEGMENT_INDEX_BITS);
    int subIndex = ((int)pos & RTS_POS_SUB_INDEX_MASK);
    int start, end, ret;
    TranslateSegmentStartEnd(iSegment, fps, start, end);
    const STSSegment *stss = GetSegment(iSegment);
    if(stss!=NULL)
    {
        if(!stss->animated)
            ret = end;
        else
        {
            ret = start+subIndex*RTS_ANIMATE_SUBPIC_DUR;
            if(ret > end)
                ret = end;
        }
        return ret*10000i64;
    }
    else
        return -1;
}

//@start, @stop: -1 if segment not found; @stop may < @start if subIndex exceed uppper bound
STDMETHODIMP_(VOID) CRenderedTextSubtitle::GetStartStop(POSITION pos, double fps, /*out*/REFERENCE_TIME &start, /*out*/REFERENCE_TIME &stop)
{
    int iSegment = ((int)pos>>RTS_POS_SEGMENT_INDEX_BITS);
    int subIndex = ((int)pos & RTS_POS_SUB_INDEX_MASK);
    int tempStart, tempEnd;
    TranslateSegmentStartEnd(iSegment, fps, tempStart, tempEnd);
    start = tempStart;
    stop = tempEnd;
    const STSSegment *stss = GetSegment(iSegment);
    if(stss!=NULL)
    {
        if(stss->animated)
        {
            start += (subIndex-1)*RTS_ANIMATE_SUBPIC_DUR;
            if(start+RTS_ANIMATE_SUBPIC_DUR < stop)
                stop = start+RTS_ANIMATE_SUBPIC_DUR;
        }
        //DbgLog((LOG_TRACE, 3, "animated:%d seg:%d idx:%d start:%d stop:%lu", stss->animated, iSegment, subIndex, (ULONG)start, (ULONG)stop));
        start *= 10000i64;
        stop *= 10000i64;
    }
    else
    {
        start = -1;
        stop = -1;
    }
}

STDMETHODIMP_(bool) CRenderedTextSubtitle::IsAnimated(POSITION pos)
{
    int iSegment = ((int)pos>>RTS_POS_SEGMENT_INDEX_BITS);
    if(iSegment>=0 && iSegment<m_segments.GetCount())
        return m_segments[iSegment].animated;
    else
        return false;
    //return(true);
}

struct LSub {int idx, layer, readorder;};

static int lscomp(const void* ls1, const void* ls2)
{
    int ret = ((LSub*)ls1)->layer - ((LSub*)ls2)->layer;
    if(!ret) ret = ((LSub*)ls1)->readorder - ((LSub*)ls2)->readorder;
    return(ret);
}

STDMETHODIMP CRenderedTextSubtitle::Render(SubPicDesc& spd, REFERENCE_TIME rt, double fps, CAtlList<CRect>& rectList)
{
    CRect bbox2(0,0,0,0);
    if(m_size != CSize(spd.w*8, spd.h*8) || m_vidrect != CRect(spd.vidrect.left*8, spd.vidrect.top*8, spd.vidrect.right*8, spd.vidrect.bottom*8))
        Init(CSize(spd.w, spd.h), spd.vidrect);
    int t = (int)(rt / 10000);
    int segment;
    //const
    STSSegment* stss = SearchSubs2(t, fps, &segment);
    if(!stss) return S_FALSE;
    // clear any cached subs not in the range of +/-30secs measured from the segment's bounds
    {
        POSITION pos = m_subtitleCache.GetStartPosition();
        while(pos)
        {
            int key;
            CSubtitle* value;
            m_subtitleCache.GetNextAssoc(pos, key, value);
            STSEntry& stse = GetAt(key);
            if(stse.end <= (t-30000) || stse.start > (t+30000))
            {
                delete value;
                m_subtitleCache.RemoveKey(key);
                pos = m_subtitleCache.GetStartPosition();
            }
        }
    }
    m_sla.AdvanceToSegment(segment, stss->subs);
    CAtlArray<LSub> subs;
    for(int i = 0, j = stss->subs.GetCount(); i < j; i++)
    {
        LSub ls;
        ls.idx = stss->subs[i];
        ls.layer = GetAt(stss->subs[i]).layer;
        ls.readorder = GetAt(stss->subs[i]).readorder;
        subs.Add(ls);
    }
    qsort(subs.GetData(), subs.GetCount(), sizeof(LSub), lscomp);
    for(int i = 0, j = subs.GetCount(); i < j; i++)
    {
        int entry = subs[i].idx;
        STSEntry stse = GetAt(entry);
        {
            int start = TranslateStart(entry, fps);
            m_time = t - start;
            m_delay = TranslateEnd(entry, fps) - start;
        }
        CSubtitle* s = GetSubtitle(entry);
        if(!s) continue;
        stss->animated |= s->m_fAnimated2;
        CRect clipRect = s->m_clip;
        CRect r = s->m_rect;
        CSize spaceNeeded = r.Size();
        // apply the effects
        bool fPosOverride = false, fOrgOverride = false;
        int alpha = 0x00;
        CPoint org2;
        for(int k = 0; k < EF_NUMBEROFEFFECTS; k++)
        {
            if(!s->m_effects[k]) continue;
            switch(k)
            {
            case EF_MOVE: // {\move(x1=param[0], y1=param[1], x2=param[2], y2=param[3], t1=t[0], t2=t[1])}
                {
                    CPoint p;
                    CPoint p1(s->m_effects[k]->param[0], s->m_effects[k]->param[1]);
                    CPoint p2(s->m_effects[k]->param[2], s->m_effects[k]->param[3]);
                    int t1 = s->m_effects[k]->t[0];
                    int t2 = s->m_effects[k]->t[1];
                    if(t2 < t1) {int t = t1; t1 = t2; t2 = t;}
                    if(t1 <= 0 && t2 <= 0) {t1 = 0; t2 = m_delay;}
                    if(m_time <= t1) p = p1;
                    else if (p1 == p2) p = p1;
                    else if(t1 < m_time && m_time < t2)
                    {
                        double t = 1.0*(m_time-t1)/(t2-t1);
                        p.x = (int)((1-t)*p1.x + t*p2.x);
                        p.y = (int)((1-t)*p1.y + t*p2.y);
                    }
                    else p = p2;
                    r = CRect(
                            CPoint((s->m_scrAlignment%3) == 1 ? p.x : (s->m_scrAlignment%3) == 0 ? p.x - spaceNeeded.cx : p.x - (spaceNeeded.cx+1)/2,
                                   s->m_scrAlignment <= 3 ? p.y - spaceNeeded.cy : s->m_scrAlignment <= 6 ? p.y - (spaceNeeded.cy+1)/2 : p.y),
                            spaceNeeded);
                    if(s->m_relativeTo == 1)
                        r.OffsetRect(m_vidrect.TopLeft());
                    fPosOverride = true;
                }
                break;
            case EF_ORG: // {\org(x=param[0], y=param[1])}
                {
                    org2 = CPoint(s->m_effects[k]->param[0], s->m_effects[k]->param[1]);
                    fOrgOverride = true;
                }
                break;
            case EF_FADE: // {\fade(a1=param[0], a2=param[1], a3=param[2], t1=t[0], t2=t[1], t3=t[2], t4=t[3]) or {\fad(t1=t[1], t2=t[2])
                {
                    int t1 = s->m_effects[k]->t[0];
                    int t2 = s->m_effects[k]->t[1];
                    int t3 = s->m_effects[k]->t[2];
                    int t4 = s->m_effects[k]->t[3];
                    if(t1 == -1 && t4 == -1) {t1 = 0; t3 = m_delay-t3; t4 = m_delay;}
                    if(m_time < t1) alpha = s->m_effects[k]->param[0];
                    else if(m_time >= t1 && m_time < t2)
                    {
                        double t = 1.0 * (m_time - t1) / (t2 - t1);
                        alpha = (int)(s->m_effects[k]->param[0]*(1-t) + s->m_effects[k]->param[1]*t);
                    }
                    else if(m_time >= t2 && m_time < t3) alpha = s->m_effects[k]->param[1];
                    else if(m_time >= t3 && m_time < t4)
                    {
                        double t = 1.0 * (m_time - t3) / (t4 - t3);
                        alpha = (int)(s->m_effects[k]->param[1]*(1-t) + s->m_effects[k]->param[2]*t);
                    }
                    else if(m_time >= t4) alpha = s->m_effects[k]->param[2];
                }
                break;
            case EF_BANNER: // Banner;delay=param[0][;leftoright=param[1];fadeawaywidth=param[2]]
                {
                    int left = s->m_relativeTo == 1 ? m_vidrect.left : 0,
                        right = s->m_relativeTo == 1 ? m_vidrect.right : m_size.cx;
                    r.left = !!s->m_effects[k]->param[1]
                             ? (left/*marginRect.left*/ - spaceNeeded.cx) + (int)(m_time*8.0/s->m_effects[k]->param[0])
                             : (right /*- marginRect.right*/) - (int)(m_time*8.0/s->m_effects[k]->param[0]);
                    r.right = r.left + spaceNeeded.cx;
                    clipRect &= CRect(left>>3, clipRect.top, right>>3, clipRect.bottom);
                    fPosOverride = true;
                }
                break;
            case EF_SCROLL: // Scroll up/down(toptobottom=param[3]);top=param[0];bottom=param[1];delay=param[2][;fadeawayheight=param[4]]
                {
                    r.top = !!s->m_effects[k]->param[3]
                            ? s->m_effects[k]->param[0] + (int)(m_time*8.0/s->m_effects[k]->param[2]) - spaceNeeded.cy
                            : s->m_effects[k]->param[1] - (int)(m_time*8.0/s->m_effects[k]->param[2]);
                    r.bottom = r.top + spaceNeeded.cy;
                    CRect cr(0, (s->m_effects[k]->param[0] + 4) >> 3, spd.w, (s->m_effects[k]->param[1] + 4) >> 3);
                    if(s->m_relativeTo == 1)
                        r.top += m_vidrect.top,
                                 r.bottom += m_vidrect.top,
                                             cr.top += m_vidrect.top>>3,
                                                       cr.bottom += m_vidrect.top>>3;
                    clipRect &= cr;
                    fPosOverride = true;
                }
                break;
            default:
                break;
            }
        }
        if(!fPosOverride && !fOrgOverride && !s->m_fAnimated)
            r = m_sla.AllocRect(s, segment, entry, stse.layer, m_collisions);
        CPoint org;
        org.x = (s->m_scrAlignment%3) == 1 ? r.left : (s->m_scrAlignment%3) == 2 ? r.CenterPoint().x : r.right;
        org.y = s->m_scrAlignment <= 3 ? r.bottom : s->m_scrAlignment <= 6 ? r.CenterPoint().y : r.top;
        if(!fOrgOverride) org2 = org;
        BYTE* pAlphaMask = s->m_pClipper?s->m_pClipper->m_pAlphaMask:NULL;
        CPoint p, p2(0, r.top);
        POSITION pos;
        p = p2;
        // Rectangles for inverse clip
        CRect iclipRect[4];
        iclipRect[0] = CRect(0, 0, spd.w, clipRect.top);
        iclipRect[1] = CRect(0, clipRect.top, clipRect.left, clipRect.bottom);
        iclipRect[2] = CRect(clipRect.right, clipRect.top, spd.w, clipRect.bottom);
        iclipRect[3] = CRect(0, clipRect.bottom, spd.w, spd.h);
        int dbgTest = 0;
        bbox2 = CRect(0,0,0,0);
        pos = s->GetHeadPosition();
        while(pos)
        {
            CLine* l = s->GetNext(pos);
            p.x = (s->m_scrAlignment%3) == 1 ? org.x
                  : (s->m_scrAlignment%3) == 0 ? org.x - l->m_width
                  :                            org.x - (l->m_width/2);
            if (s->m_clipInverse)
            {
                bbox2 |= l->PaintShadow(spd, iclipRect[0], pAlphaMask, p, org2, m_time, alpha);
                bbox2 |= l->PaintShadow(spd, iclipRect[1], pAlphaMask, p, org2, m_time, alpha);
                bbox2 |= l->PaintShadow(spd, iclipRect[2], pAlphaMask, p, org2, m_time, alpha);
                bbox2 |= l->PaintShadow(spd, iclipRect[3], pAlphaMask, p, org2, m_time, alpha);
            }
            else
            {
                bbox2 |= l->PaintShadow(spd, clipRect, pAlphaMask, p, org2, m_time, alpha);
            }
            p.y += l->m_ascent + l->m_descent;
        }
        p = p2;
        pos = s->GetHeadPosition();
        while(pos)
        {
            CLine* l = s->GetNext(pos);
            p.x = (s->m_scrAlignment%3) == 1 ? org.x
                  : (s->m_scrAlignment%3) == 0 ? org.x - l->m_width
                  :                            org.x - (l->m_width/2);
            if (s->m_clipInverse)
            {
                bbox2 |= l->PaintOutline(spd, iclipRect[0], pAlphaMask, p, org2, m_time, alpha);
                bbox2 |= l->PaintOutline(spd, iclipRect[1], pAlphaMask, p, org2, m_time, alpha);
                bbox2 |= l->PaintOutline(spd, iclipRect[2], pAlphaMask, p, org2, m_time, alpha);
                bbox2 |= l->PaintOutline(spd, iclipRect[3], pAlphaMask, p, org2, m_time, alpha);
            }
            else
            {
                bbox2 |= l->PaintOutline(spd, clipRect, pAlphaMask, p, org2, m_time, alpha);
            }
            p.y += l->m_ascent + l->m_descent;
        }
        p = p2;
        pos = s->GetHeadPosition();
        while(pos)
        {
            CLine* l = s->GetNext(pos);
            p.x = (s->m_scrAlignment%3) == 1 ? org.x
                  : (s->m_scrAlignment%3) == 0 ? org.x - l->m_width
                  :                            org.x - (l->m_width/2);
            if (s->m_clipInverse)
            {
                bbox2 |= l->PaintBody(spd, iclipRect[0], pAlphaMask, p, org2, m_time, alpha);
                bbox2 |= l->PaintBody(spd, iclipRect[1], pAlphaMask, p, org2, m_time, alpha);
                bbox2 |= l->PaintBody(spd, iclipRect[2], pAlphaMask, p, org2, m_time, alpha);
                bbox2 |= l->PaintBody(spd, iclipRect[3], pAlphaMask, p, org2, m_time, alpha);
            }
            else
            {
                bbox2 |= l->PaintBody(spd, clipRect, pAlphaMask, p, org2, m_time, alpha);
            }
            //DbgLog((LOG_TRACE,3,"%d line:%x bbox2 l:%d, t:%d, r:%d, b:%d", dbgTest++, l, bbox2->left, bbox2->top, bbox2->right, bbox2->bottom));
            p.y += l->m_ascent + l->m_descent;
        }
        rectList.AddTail(bbox2);
    }
    return (subs.GetCount() && !rectList.IsEmpty()) ? S_OK : S_FALSE;
}


STDMETHODIMP CRenderedTextSubtitle::Render(SubPicDesc& spd, REFERENCE_TIME rt, double fps, RECT& bbox)
{
    CAtlList<CRect> rectList;
    HRESULT result = Render(spd, rt, fps, rectList);
    POSITION pos = rectList.GetHeadPosition();
    CRect bbox2(0,0,0,0);
    while(pos!=NULL)
    {
        bbox2 |= rectList.GetNext(pos);
    }
    bbox = bbox2;
    return result;
}

// IPersist

STDMETHODIMP CRenderedTextSubtitle::GetClassID(CLSID* pClassID)
{
    return pClassID ? *pClassID = __uuidof(this), S_OK : E_POINTER;
}

// ISubStream

STDMETHODIMP_(int) CRenderedTextSubtitle::GetStreamCount()
{
    return(1);
}

STDMETHODIMP CRenderedTextSubtitle::GetStreamInfo(int iStream, WCHAR** ppName, LCID* pLCID)
{
    if(iStream != 0) return E_INVALIDARG;
    if(ppName)
    {
        if(!(*ppName = (WCHAR*)CoTaskMemAlloc((m_name.GetLength()+1)*sizeof(WCHAR))))
            return E_OUTOFMEMORY;
        wcscpy(*ppName, CStringW(m_name));
    }
    if(pLCID)
    {
        *pLCID = 0; // TODO
    }
    return S_OK;
}

STDMETHODIMP_(int) CRenderedTextSubtitle::GetStream()
{
    return(0);
}

STDMETHODIMP CRenderedTextSubtitle::SetStream(int iStream)
{
    return iStream == 0 ? S_OK : E_FAIL;
}

STDMETHODIMP CRenderedTextSubtitle::Reload()
{
    CFileStatus s;
    if(!CFile::GetStatus(m_path, s)) return E_FAIL;
    return !m_path.IsEmpty() && Open(m_path, DEFAULT_CHARSET) ? S_OK : E_FAIL;
}
