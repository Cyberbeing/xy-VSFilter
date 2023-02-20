/* 
 *	Copyright (C) 2007 Niels Martin Hansen
 *	http://aegisub.net/
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
#include <afxdlgs.h>
#include <atlpath.h>
#include "resource.h"
#include "..\..\..\subtitles\VobSubFile.h"
#include "..\..\..\subtitles\RTS.h"
#include "..\..\..\subtitles\SSF.h"

#define CSRIAPI extern "C" __declspec(dllexport)
#define CSRI_OWN_HANDLES
typedef const char *csri_rend;
extern "C" struct csri_vsfilter_inst {
	CRenderedTextSubtitle *rts;
	CCritSec *cs;
	CSize script_res;
	CSize screen_res;
	CRect video_rect;
	enum csri_pixfmt pixfmt;
	size_t readorder;
};
typedef struct csri_vsfilter_inst csri_inst;
#include "csri.h"
static csri_rend csri_vsfilter = "vsfilter";


CSRIAPI csri_inst *csri_open_file(csri_rend *renderer, const char *filename, struct csri_openflag *flags)
{
    AMTRACE((TEXT(__FUNCTION__),0));
	int namesize;
	wchar_t *namebuf;

	namesize = MultiByteToWideChar(CP_UTF8, 0, filename, -1, NULL, 0);
	if (!namesize)
		return 0;
	namesize++;
	namebuf = new wchar_t[namesize];
	MultiByteToWideChar(CP_UTF8, 0, filename, -1, namebuf, namesize);

	csri_inst *inst = new csri_inst();
	inst->cs = new CCritSec();
	inst->rts = new CRenderedTextSubtitle(inst->cs);
	if (inst->rts->Open(CString(namebuf), DEFAULT_CHARSET)) {
		delete[] namebuf;
		inst->readorder = 0;
		return inst;
	} else {
		delete[] namebuf;
		delete inst->rts;
		delete inst->cs;
		delete inst;
		return 0;
	}
}


CSRIAPI csri_inst *csri_open_mem(csri_rend *renderer, const void *data, size_t length, struct csri_openflag *flags)
{
    AMTRACE((TEXT(__FUNCTION__),0));
	// This is actually less effecient than opening a file, since this first writes the memory data to a temp file,
	// then opens that file and parses from that.
	csri_inst *inst = new csri_inst();

	inst->cs = new CCritSec();
	inst->rts = new CRenderedTextSubtitle(inst->cs);
	if (inst->rts->Open((BYTE*)data, (int)length, DEFAULT_CHARSET, _T("CSRI memory subtitles"))) {
		inst->readorder = 0;
		return inst;
	} else {
		delete inst->rts;
		delete inst->cs;
		delete inst;
		return 0;
	}
}


CSRIAPI void csri_close(csri_inst *inst)
{
	if (!inst) return;

	delete inst->rts;
	delete inst->cs;
	delete inst;
}


CSRIAPI int csri_request_fmt(csri_inst *inst, const struct csri_fmt *fmt)
{
	if (!inst || !fmt->width || !fmt->height) {
		return -1;
	}

	// Check if pixel format is supported
	switch (fmt->pixfmt) {
		case CSRI_F_BGR_:
		case CSRI_F_BGR:
		case CSRI_F_YUY2:
		case CSRI_F_YV12:
			inst->pixfmt = fmt->pixfmt;
			break;

		default:
			return -1;
	}
	inst->screen_res = CSize(fmt->width, fmt->height);
	inst->video_rect = CRect(0, 0, fmt->width, fmt->height);
	return 0;
}


CSRIAPI void csri_render(csri_inst *inst, struct csri_frame *frame, double time)
{
	const double arbitrary_framerate = 25.0;
	SubPicDesc spd;
	spd.w = inst->screen_res.cx;
	spd.h = inst->screen_res.cy;
	switch (inst->pixfmt) {
		case CSRI_F_BGR_:
			spd.type = MSP_RGBA;
			spd.bpp = 32;
			spd.bits = frame->planes[0];
			spd.pitch = frame->strides[0];
			break;

		default:
            ASSERT(0);
            CString msg;
            msg.Format(_T("Anything other then RGB32 is NOT supported!"));
            MessageBox(NULL, msg, _T("Warning"), MB_OKCANCEL|MB_ICONWARNING);
            int o = 0; o=o/o;
			return;
	}
	spd.vidrect = inst->video_rect;

	inst->rts->Render(spd, (REFERENCE_TIME)(time*10000000), arbitrary_framerate, inst->video_rect);
}


// No extensions supported
CSRIAPI void *csri_query_ext(csri_rend *rend, csri_ext_id extname) { return 0; }

// Get info for renderer
static struct csri_info csri_vsfilter_info = {
#ifdef _DEBUG
	"xy-vsfilter_textsub_debug", // name
#else
	"xy-vsfilter_textsub", // name
#endif
	"3.0", // version
	"xy-VSFilter/TextSub", // longname
	"Gabest", // author
	"Copyright (c) 2003-2014 by Gabest et al." // copyright
};

CSRIAPI struct csri_info *csri_renderer_info(csri_rend *rend) { return &csri_vsfilter_info; }
CSRIAPI csri_rend *csri_renderer_byname(const char *name, const char *specific) { return &csri_vsfilter; }
CSRIAPI csri_rend *csri_renderer_default() { return &csri_vsfilter; }
CSRIAPI csri_rend *csri_renderer_next(csri_rend *prev) { return 0; }
