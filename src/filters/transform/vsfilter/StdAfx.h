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

#pragma once

#define _ATL_APARTMENT_THREADED

#include "vsfilter_config.h"

#include "../../../DSUtil/SharedInclude.h"
#include "../../../../include/stdafx_common.h"
#include "../../../../include/stdafx_common_afx2.h"
#include "../../../../include/stdafx_common_dshow.h"
#include "../../../DSUtil/DSUtil.h"
#include "../../../subtitles/flyweight_base_types.h"
#include "../../../subtitles/RTS.h"
#include "../../../subtitles/cache_manager.h"
#include "../../../subtitles/subpixel_position_controler.h"
#include "../../../subtitles/RenderedHdmvSubtitle.h"
#include "../../../subpic/color_conv_table.h"
#include "XyOptionsImpl.h"
#include "SubRenderOptionsImpl.h"
#include "XySubRenderIntf.h"
#include "../../../subpic/XySubRenderFrameWrapper.h"
#include "../../../subpic/XySubRenderProviderWrapper.h"
#include "../../../subpic/MemSubPic.h"
#include "DirectVobSub.h"
#include "Systray.h"
#include "xy_logger.h"
