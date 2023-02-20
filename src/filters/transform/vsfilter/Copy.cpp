// Copyright 2003-2006 Gabest
// http://www.gabest.org
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA, or visit
// http://www.gnu.org/copyleft/gpl.html

#include "stdafx.h"
#include <math.h>
#include "DirectVobSubFilter.h"
#include "..\..\..\DSUtil\DSUtil.h"
#include "..\..\..\DSUtil\MediaTypes.h"

#include <initguid.h>
#include "../../../../include/moreuuids.h"
#include "../../subpic/color_conv_table.h"

#include "xy_logger.h"

using namespace DirectVobSubXyOptions;

static void LogSubPicStartStop( const REFERENCE_TIME& rtStart, const REFERENCE_TIME& rtStop, const CString& msg)
{
#ifdef __DO_LOG
    static REFERENCE_TIME s_rtStart = -1, s_rtStop = -1;
    if(s_rtStart!=rtStart || s_rtStop!=rtStop)
    {
        XY_LOG_INFO(msg.GetString());
        s_rtStart=rtStart;
        s_rtStop=rtStop;
    }
#endif
}

void BltLineRGB32(DWORD* d, BYTE* sub, int w, const GUID& subtype)
{
	if(subtype == MEDIASUBTYPE_YV12 || subtype == MEDIASUBTYPE_I420 || subtype == MEDIASUBTYPE_IYUV 
        || subtype == MEDIASUBTYPE_NV12 || subtype == MEDIASUBTYPE_NV21)
	{
        //TODO: Fix ME!
		BYTE* db = (BYTE*)d;
		BYTE* dbtend = db + w;

		for(; db < dbtend; sub+=4, db++)
		{
			if(sub[3] < 0xff)
			{
				int y = ColorConvTable::Rgb2Y(sub[2], sub[1], sub[0]);
				*db = y; // w/o colors 
			}
		}
	}
    else if(subtype == MEDIASUBTYPE_P010 || subtype == MEDIASUBTYPE_P016)
    {        
        //TODO: Fix ME!
        WORD* db = reinterpret_cast<WORD*>(d);
        WORD* dbtend = db + w;

        for(; db < dbtend; sub+=4, db++)
        {
            if(sub[3] < 0xff)
            {
                int y = ColorConvTable::Rgb2Y(sub[2], sub[1], sub[0])<<2;
                *db = y<<8; // w/o colors 
            }
        }
    }
	else if(subtype == MEDIASUBTYPE_YUY2)
	{
		WORD* ds = (WORD*)d;
		WORD* dstend = ds + w;

		for(; ds < dstend; sub+=4, ds++)
		{
			if(sub[3] < 0xff)
			{
				int y = ColorConvTable::Rgb2Y(sub[2], sub[1], sub[0]);
				*ds = 0x8000|y; // w/o colors 
			}
		}
	}
    else if (subtype == MEDIASUBTYPE_AYUV)
    {
        //TODO: Fix ME!
        BYTE* db = (BYTE*)d;
        db += 2;
        BYTE* dbtend = db + w;

        for(; db < dbtend; sub+=4, db+=4)
        {
            if(sub[3] < 0xff)
            {
                int y = ColorConvTable::Rgb2Y(sub[2], sub[1], sub[0]);
                *db = y; // w/o colors
            }
        }
    }
	else if(subtype == MEDIASUBTYPE_RGB555)
	{
		WORD* ds = (WORD*)d;
		WORD* dstend = ds + w;

		for(; ds < dstend; sub+=4, ds++)
		{
			if(sub[3] < 0xff)
			{
				*ds = ((*((DWORD*)sub)>>9)&0x7c00)|((*((DWORD*)sub)>>6)&0x03e0)|((*((DWORD*)sub)>>3)&0x001f);
			}
		}
	}
	else if(subtype == MEDIASUBTYPE_RGB565)
	{
		WORD* ds = (WORD*)d;
		WORD* dstend = ds + w;

		for(; ds < dstend; sub+=4, ds++)
		{
			if(sub[3] < 0xff)
			{
				*ds = ((*((DWORD*)sub)>>8)&0xf800)|((*((DWORD*)sub)>>5)&0x07e0)|((*((DWORD*)sub)>>3)&0x001f);
			}
		}
	}
	else if(subtype == MEDIASUBTYPE_RGB24)
	{
		BYTE* dt = (BYTE*)d;
		BYTE* dstend = dt + w*3;

		for(; dt < dstend; sub+=4, dt+=3)
		{
			if(sub[3] < 0xff)
			{
				dt[0] = sub[0];
				dt[1] = sub[1];
				dt[2] = sub[2];
			}
		}
	}
	else if(subtype == MEDIASUBTYPE_RGB32 || subtype == MEDIASUBTYPE_ARGB32)
	{
		DWORD* dstend = d + w;

		for(; d < dstend; sub+=4, d++)
		{
			if(sub[3] < 0xff) *d = *((DWORD*)sub)&0xffffff;
		}
	}
}

HRESULT Copy(BYTE* pSub, BYTE* pIn, CSize sub, CSize in, int bpp, const GUID& subtype, DWORD black)
{
    int wIn = in.cx, hIn = in.cy, pitchIn = wIn*bpp>>3;
    int wSub = sub.cx, hSub = sub.cy, pitchSub = wSub*bpp>>3;
    bool fScale2x = wIn*2 <= wSub;

    if(fScale2x) wIn <<= 1, hIn <<= 1;

    int left = ((wSub - wIn)>>1)&~1;
    int mid = wIn;
    int right = left + ((wSub - wIn)&1);

    int dpLeft = left*bpp>>3;
    int dpMid = mid*bpp>>3;
    int dpRight = right*bpp>>3;

    ASSERT(wSub >= wIn);

    {
        int i = 0, j = 0;

        j += (hSub - hIn) >> 1;

        for(; i < j; i++, pSub += pitchSub)
        {
            memsetd(pSub, black, dpLeft+dpMid+dpRight);
        }

        j += hIn;

        if(hIn > hSub)
            pIn += pitchIn * ((hIn - hSub) >> (fScale2x?2:1));

        if(fScale2x)
        {
            ASSERT(0);
            return E_NOTIMPL;
        }
        else
        {
            for(int k = min(j, hSub); i < k; i++, pIn += pitchIn, pSub += pitchSub)
            {
                memsetd(pSub, black, dpLeft);
                memcpy(pSub + dpLeft, pIn, dpMid);
                memsetd(pSub + dpLeft+dpMid, black, dpRight);
            }
        }

        j = hSub;

        for(; i < j; i++, pSub += pitchSub)
        {
            memsetd(pSub, black, dpLeft+dpMid+dpRight);
        }
    }

    return NOERROR;
}

HRESULT CDirectVobSubFilter::Copy(BYTE* pSub, BYTE* pIn, CSize sub, CSize in, int bpp, const GUID& subtype, DWORD black)
{
    return ::Copy(pSub, pIn, sub, in, bpp, subtype, black);	
}

void CDirectVobSubFilter::PrintMessages(BYTE* pOut)
{
	if(!m_hdc || !m_hbm)
		return;
    
	const GUID& subtype = m_pOutput->CurrentMediaType().subtype;

	BITMAPINFOHEADER bihOut;
	ExtractBIH(&m_pOutput->CurrentMediaType(), &bihOut);

	CString msg, tmp;

	if(m_fOSD)
	{
		tmp.Format(_T("in: %dx%d %s\nout: %dx%d %s\n"), 
			m_w, m_h, 
			Subtype2String(m_pInput->CurrentMediaType().subtype),
			bihOut.biWidth, bihOut.biHeight, 
			Subtype2String(m_pOutput->CurrentMediaType().subtype));
		msg += tmp;

		tmp.Format(_T("real fps: %.3f, current fps: %.3f\nmedia time: %d, subtitle time: %d [ms]\nframe number: %d (calculated)\nrate: %.4f\n"), 
			m_fps, m_fMediaFPSEnabled?m_MediaFPS:fabs(m_fps),
			(int)m_tPrev.Millisecs(), (int)(CalcCurrentTime()/10000),
			(int)(m_tPrev.m_time * m_fps / 10000000),
			m_pInput->CurrentRate());
		msg += tmp;

		CAutoLock cAutoLock(&m_csQueueLock);

		if(m_simple_provider)
		{
			int nSubPics = -1;
			REFERENCE_TIME rtNow = -1, rtStart = -1, rtStop = -1;
			m_simple_provider->GetStats(nSubPics, rtNow, rtStart, rtStop);
			tmp.Format(_T("queue stats: %I64d - %I64d [ms]\n"), rtStart/10000, rtStop/10000);
			msg += tmp;

			for(int i = 0; i < nSubPics; i++)
			{
				m_simple_provider->GetStats(i, rtStart, rtStop);
				tmp.Format(_T("%d: %I64d - %I64d [ms]\n"), i, rtStart/10000, rtStop/10000);
				msg += tmp;
			}
            LogSubPicStartStop(rtStart, rtStop, tmp);
		}
        
        //color space
        tmp.Format( _T("Colorspace: %ls %ls (%ls)\n"), 
            ColorConvTable::GetDefaultRangeType()==ColorConvTable::RANGE_PC ? _T("PC"):_T("TV"),
            ColorConvTable::GetDefaultYUVType()==ColorConvTable::BT601 ? _T("BT.601"):_T("BT.709"),
            m_xy_int_opt[INT_COLOR_SPACE]==CDirectVobSub::YuvMatrix_AUTO ? _T("Auto") :
              m_xy_int_opt[INT_COLOR_SPACE]==CDirectVobSub::GUESS ? _T("Guessed") : _T("Forced") );
        msg += tmp;

        SIZE layout_size, script_playres;
        XyGetSize(SIZE_LAYOUT_WITH, &layout_size);
        XyGetSize(SIZE_ASS_PLAY_RESOLUTION, &script_playres);
        tmp.Format( _T("Layout with size %dx%d, script playres %dx%d\n"),
            layout_size.cx, layout_size.cy, script_playres.cx, script_playres.cy );
        msg += tmp;

        //print cache info
        CachesInfo *caches_info = NULL;
        XyFlyWeightInfo *xy_fw_info = NULL;
        int tmp_size;
        XyGetBin(DirectVobSubXyOptions::BIN_CACHES_INFO, reinterpret_cast<LPVOID*>(&caches_info), &tmp_size);
        XyGetBin(DirectVobSubXyOptions::BIN_XY_FLY_WEIGHT_INFO, reinterpret_cast<LPVOID*>(&xy_fw_info), &tmp_size);
        ASSERT(caches_info);
        ASSERT(xy_fw_info);
        tmp.Format(
            _T("Cache :stored_num/hit_count/query_count\n")\
            _T("  Parser cache 1:%ld/%ld/%ld\n")\
            _T("  Parser cache 2:%ld/%ld/%ld\n")\
            _T("\n")\
            _T("  LV 4:%ld/%ld/%ld\t\t")\
            _T("  LV 3:%ld/%ld/%ld\n")\
            _T("  LV 2:%ld/%ld/%ld\t\t")\
            _T("  LV 1:%ld/%ld/%ld\n")\
            _T("  LV 0:%ld/%ld/%ld\n")\
            _T("\n")\
            _T("  bitmap   :%ld/%ld/%ld\t\t")\
            _T("  scan line:%ld/%ld/%ld\n")\
            _T("  relay key:%ld/%ld/%ld\t\t")\
            _T("  clipper  :%ld/%ld/%ld\n")\
            _T("\n")\
            _T("  FW string pool    :%ld/%ld/%ld\t\t")\
            _T("  FW bitmap key pool:%ld/%ld/%ld\n")\
            ,
            caches_info->text_info_cache_cur_item_num, caches_info->text_info_cache_hit_count, caches_info->text_info_cache_query_count,
            caches_info->word_info_cache_cur_item_num, caches_info->word_info_cache_hit_count, caches_info->word_info_cache_query_count,
            caches_info->path_cache_cur_item_num,     caches_info->path_cache_hit_count,     caches_info->path_cache_query_count,
            caches_info->scanline_cache2_cur_item_num, caches_info->scanline_cache2_hit_count, caches_info->scanline_cache2_query_count,
            caches_info->non_blur_cache_cur_item_num, caches_info->non_blur_cache_hit_count, caches_info->non_blur_cache_query_count,
            caches_info->overlay_cache_cur_item_num,  caches_info->overlay_cache_hit_count,  caches_info->overlay_cache_query_count,
            caches_info->interpolate_cache_cur_item_num, caches_info->interpolate_cache_hit_count, caches_info->interpolate_cache_query_count,
            caches_info->bitmap_cache_cur_item_num, caches_info->bitmap_cache_hit_count, caches_info->bitmap_cache_query_count,
            caches_info->scanline_cache_cur_item_num, caches_info->scanline_cache_hit_count, caches_info->scanline_cache_query_count,
            caches_info->overlay_key_cache_cur_item_num, caches_info->overlay_key_cache_hit_count, caches_info->overlay_key_cache_query_count,
            caches_info->clipper_cache_cur_item_num, caches_info->clipper_cache_hit_count, caches_info->clipper_cache_query_count,

            xy_fw_info->xy_fw_string_w.cur_item_num, xy_fw_info->xy_fw_string_w.hit_count, xy_fw_info->xy_fw_string_w.query_count,
            xy_fw_info->xy_fw_grouped_draw_items_hash_key.cur_item_num, xy_fw_info->xy_fw_grouped_draw_items_hash_key.hit_count, xy_fw_info->xy_fw_grouped_draw_items_hash_key.query_count
            );
        msg += tmp;
        delete []caches_info;
	}

	if(msg.IsEmpty()) return;

	HANDLE hOldBitmap = SelectObject(m_hdc, m_hbm);
	HANDLE hOldFont = SelectObject(m_hdc, m_hfont);

	SetTextColor(m_hdc, 0xffffff);
	SetBkMode(m_hdc, TRANSPARENT);
	SetMapMode(m_hdc, MM_TEXT);

	BITMAP bm;
	GetObject(m_hbm, sizeof(BITMAP), &bm);

	CRect r(0, 0, bm.bmWidth, bm.bmHeight);
	DrawText(m_hdc, msg, _tcslen(msg), &r, DT_CALCRECT|DT_EXTERNALLEADING|DT_NOPREFIX|DT_WORDBREAK);

	r += CPoint(10, 10);
	r &= CRect(0, 0, bm.bmWidth, bm.bmHeight);

	DrawText(m_hdc, msg, _tcslen(msg), &r, DT_LEFT|DT_TOP|DT_NOPREFIX|DT_WORDBREAK);

	BYTE* pIn = (BYTE*)bm.bmBits;
	int pitchIn = bm.bmWidthBytes;
	int pitchOut = bihOut.biWidth * bihOut.biBitCount >> 3;

	if( subtype == MEDIASUBTYPE_YV12 || subtype == MEDIASUBTYPE_I420 || subtype == MEDIASUBTYPE_IYUV 
        || subtype== MEDIASUBTYPE_NV12 || subtype==MEDIASUBTYPE_NV21 )
		pitchOut = bihOut.biWidth;
    else if (subtype == MEDIASUBTYPE_P010 || subtype == MEDIASUBTYPE_P016)
        pitchOut = bihOut.biWidth * 2;

	pitchIn = (pitchIn+3)&~3;
	pitchOut = (pitchOut+3)&~3;

	if(bihOut.biHeight > 0 && bihOut.biCompression <= 3) // flip if the dst bitmap is flipped rgb (m_hbm is a top-down bitmap, not like the subpictures)
	{
		pOut += pitchOut * (abs(bihOut.biHeight)-1);
		pitchOut = -pitchOut;
	}

	pIn += pitchIn * r.top;
	pOut += pitchOut * r.top;

	for(int w = min(r.right, m_w), h = r.Height(); h--; pIn += pitchIn, pOut += pitchOut)
	{
		BltLineRGB32((DWORD*)pOut, pIn, w, subtype);
		memsetd(pIn, 0xff000000, r.right*4);
	}

	SelectObject(m_hdc, hOldBitmap);
	SelectObject(m_hdc, hOldFont);
}
