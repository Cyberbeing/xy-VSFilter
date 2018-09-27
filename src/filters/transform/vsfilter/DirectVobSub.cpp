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
#include "DirectVobSub.h"
#include "VSFilter.h"

using namespace DirectVobSubXyOptions;

//////////////////////////////////////////////////////////////////////////

DirectVobSubImpl::DirectVobSubImpl(const Option *options, CCritSec * pLock)
    : XyOptionsImpl(options)
    , m_propsLock(pLock)
{
}

DirectVobSubImpl::~DirectVobSubImpl()
{
}

STDMETHODIMP DirectVobSubImpl::get_FileName(WCHAR* fn)
{
    WCHAR *tmp = NULL;
    HRESULT hr = XyGetString(STRING_FILE_NAME, &tmp, NULL);
    if (SUCCEEDED(hr))
    {
        wcscpy_s(fn, wcslen(tmp)+1, tmp);
        LocalFree(tmp);
    }
    return hr;
}

STDMETHODIMP DirectVobSubImpl::put_FileName(WCHAR* fn)
{
    return XySetString(STRING_FILE_NAME, fn, wcslen(fn));
}

STDMETHODIMP DirectVobSubImpl::get_LanguageCount(int* nLangs)
{
    return XyGetInt(DirectVobSubXyOptions::INT_LANGUAGE_COUNT, nLangs);
}

STDMETHODIMP DirectVobSubImpl::get_LanguageName(int iLanguage, WCHAR** ppName)
{
    if (!TestOption(DirectVobSubXyOptions::void_LanguageName))
    {
        return E_NOTIMPL;
    }
	return S_OK;
}

STDMETHODIMP DirectVobSubImpl::get_SelectedLanguage(int* iSelected)
{
    return XyGetInt(DirectVobSubXyOptions::INT_SELECTED_LANGUAGE, iSelected);
}

STDMETHODIMP DirectVobSubImpl::put_SelectedLanguage(int iSelected)
{
    return XySetInt(DirectVobSubXyOptions::INT_SELECTED_LANGUAGE, iSelected);
}

STDMETHODIMP DirectVobSubImpl::get_HideSubtitles(bool* fHideSubtitles)
{
    return XyGetBool(DirectVobSubXyOptions::BOOL_HIDE_SUBTITLES, fHideSubtitles);
}

STDMETHODIMP DirectVobSubImpl::put_HideSubtitles(bool fHideSubtitles)
{
    return XySetBool(DirectVobSubXyOptions::BOOL_HIDE_SUBTITLES, fHideSubtitles);
}

STDMETHODIMP DirectVobSubImpl::get_PreBuffering(bool* fDoPreBuffering)
{
    return XyGetBool(DirectVobSubXyOptions::BOOL_PRE_BUFFERING, fDoPreBuffering);
}

STDMETHODIMP DirectVobSubImpl::put_PreBuffering(bool fDoPreBuffering)
{
    return XySetBool(DirectVobSubXyOptions::BOOL_PRE_BUFFERING, fDoPreBuffering);
}


STDMETHODIMP DirectVobSubImpl::get_Placement(bool* fOverridePlacement, int* xperc, int* yperc)
{
    HRESULT hr = XyGetBool(DirectVobSubXyOptions::BOOL_OVERRIDE_PLACEMENT, fOverridePlacement);
    if (FAILED(hr))
    {
        XY_LOG_ERROR("Failed to get option override placement. "<<XY_LOG_VAR_2_STR(fOverridePlacement));
        return hr;
    }
    SIZE cs;
    hr = XyGetSize(DirectVobSubXyOptions::SIZE_PLACEMENT_PERC, &cs);
    if (FAILED(hr))
    {
        XY_LOG_ERROR("Failed to get option placement perc. ");
        return hr;
    }
    if(xperc) *xperc = cs.cx;
    if(yperc) *yperc = cs.cy;

    return hr;
}

STDMETHODIMP DirectVobSubImpl::put_Placement(bool fOverridePlacement, int xperc, int yperc)
{
    if (!TestOption(DirectVobSubXyOptions::BOOL_OVERRIDE_PLACEMENT, OPTION_TYPE_BOOL, OPTION_MODE_WRITE) ||
        !TestOption(DirectVobSubXyOptions::SIZE_PLACEMENT_PERC, OPTION_TYPE_SIZE, OPTION_MODE_WRITE))
    {
        return E_NOTIMPL;
    }
    HRESULT hr = XySetBool(DirectVobSubXyOptions::BOOL_OVERRIDE_PLACEMENT, fOverridePlacement);
    if (FAILED(hr))
    {
        XY_LOG_ERROR("Failed to set option override placement. "<<XY_LOG_VAR_2_STR(fOverridePlacement));
        return hr;
    }
    CSize cs(xperc, yperc);
    hr = XySetSize(DirectVobSubXyOptions::SIZE_PLACEMENT_PERC, cs);
    if (FAILED(hr))
    {
        XY_LOG_ERROR("Failed to set option placement perc. "<<XY_LOG_VAR_2_STR(xperc)<<XY_LOG_VAR_2_STR(yperc));
        return hr;
    }
    return hr;
}

STDMETHODIMP DirectVobSubImpl::get_VobSubSettings(bool* fBuffer, bool* fOnlyShowForcedSubs, bool* fPolygonize)
{
    HRESULT hr = XyGetBool(DirectVobSubXyOptions::BOOL_VOBSUBSETTINGS_BUFFER, fBuffer);
    if (FAILED(hr))
    {
        XY_LOG_ERROR("Failed to get option BOOL_VOBSUBSETTINGS_BUFFER"<<XY_LOG_VAR_2_STR(fBuffer));
        return hr;
    }
    hr = XyGetBool(DirectVobSubXyOptions::BOOL_VOBSUBSETTINGS_ONLY_SHOW_FORCED_SUBS, fOnlyShowForcedSubs);
    if (FAILED(hr))
    {
        XY_LOG_ERROR("Failed to get option BOOL_VOBSUBSETTINGS_ONLY_SHOW_FORCED_SUBS"<<XY_LOG_VAR_2_STR(fOnlyShowForcedSubs));
        return hr;
    }
    hr = XyGetBool(DirectVobSubXyOptions::BOOL_VOBSUBSETTINGS_POLYGONIZE, fPolygonize);
    if (FAILED(hr))
    {
        XY_LOG_ERROR("Failed to get option BOOL_VOBSUBSETTINGS_POLYGONIZE"<<XY_LOG_VAR_2_STR(fPolygonize));
        return hr;
    }
    return S_OK;
}

STDMETHODIMP DirectVobSubImpl::put_VobSubSettings(bool fBuffer, bool fOnlyShowForcedSubs, bool fPolygonize)
{
    if (!TestOption(DirectVobSubXyOptions::BOOL_VOBSUBSETTINGS_BUFFER, OPTION_TYPE_BOOL, OPTION_MODE_WRITE) ||
        !TestOption(DirectVobSubXyOptions::BOOL_VOBSUBSETTINGS_ONLY_SHOW_FORCED_SUBS, OPTION_TYPE_BOOL, OPTION_MODE_WRITE) ||
        !TestOption(DirectVobSubXyOptions::BOOL_VOBSUBSETTINGS_POLYGONIZE, OPTION_TYPE_BOOL, OPTION_MODE_WRITE))
    {
        return E_NOTIMPL;
    }
    HRESULT hr1 = XySetBool(DirectVobSubXyOptions::BOOL_VOBSUBSETTINGS_BUFFER, fBuffer);
    if (FAILED(hr1))
    {
        XY_LOG_ERROR("Failed to set option BOOL_VOBSUBSETTINGS_BUFFER"<<XY_LOG_VAR_2_STR(fBuffer));
        return hr1;
    }
    HRESULT hr2 = XySetBool(DirectVobSubXyOptions::BOOL_VOBSUBSETTINGS_ONLY_SHOW_FORCED_SUBS, fOnlyShowForcedSubs);
    if (FAILED(hr2))
    {
        XY_LOG_ERROR("Failed to set option BOOL_VOBSUBSETTINGS_ONLY_SHOW_FORCED_SUBS"<<XY_LOG_VAR_2_STR(fOnlyShowForcedSubs));
        return hr2;
    }
    HRESULT hr3 = XySetBool(DirectVobSubXyOptions::BOOL_VOBSUBSETTINGS_POLYGONIZE, fPolygonize);
    if (FAILED(hr3))
    {
        XY_LOG_ERROR("Failed to set option BOOL_VOBSUBSETTINGS_POLYGONIZE"<<XY_LOG_VAR_2_STR(fPolygonize));
        return hr3;
    }
    return (hr1==S_FALSE&&hr2==S_FALSE&&hr3==S_FALSE) ? S_FALSE : S_OK;
}

STDMETHODIMP DirectVobSubImpl::get_TextSettings(void* lf, int lflen, COLORREF* color, bool* fShadow, bool* fOutline, bool* fAdvancedRenderer)
{
    if (!TestOption(DirectVobSubXyOptions::BIN2_TEXT_SETTINGS, OPTION_TYPE_BIN2, OPTION_MODE_READ))
    {
        return E_NOTIMPL;
    }
    CAutoLock cAutoLock(m_propsLock);
    if (lf)
    {
        if(lflen == sizeof(LOGFONTA))
            strncpy_s(((LOGFONTA*)lf)->lfFaceName, LF_FACESIZE, CStringA(m_defStyle.fontName), _TRUNCATE);
        else if(lflen == sizeof(LOGFONTW))
            wcsncpy_s(((LOGFONTW*)lf)->lfFaceName, LF_FACESIZE, CStringW(m_defStyle.fontName), _TRUNCATE);
        else
            return E_INVALIDARG;

        ((LOGFONT*)lf)->lfCharSet = m_defStyle.charSet;
        ((LOGFONT*)lf)->lfItalic = m_defStyle.fItalic;
        ((LOGFONT*)lf)->lfHeight = m_defStyle.fontSize;
        ((LOGFONT*)lf)->lfWeight = m_defStyle.fontWeight;
        ((LOGFONT*)lf)->lfStrikeOut = m_defStyle.fStrikeOut;
        ((LOGFONT*)lf)->lfUnderline = m_defStyle.fUnderline;
    }

    if(color) *color = m_defStyle.colors[0];
    if(fShadow) *fShadow = (m_defStyle.shadowDepthX>0) || (m_defStyle.shadowDepthY>0);
    if(fOutline) *fOutline = (m_defStyle.outlineWidthX>0) || (m_defStyle.outlineWidthY>0);
    if(fAdvancedRenderer) *fAdvancedRenderer = m_fAdvancedRenderer;

    return S_OK;
}

STDMETHODIMP DirectVobSubImpl::put_TextSettings(void* lf, int lflen, COLORREF color, bool fShadow, bool fOutline, bool fAdvancedRenderer)
{
    if (!TestOption(DirectVobSubXyOptions::BIN2_TEXT_SETTINGS, OPTION_TYPE_BIN2, OPTION_MODE_WRITE))
    {
        return E_NOTIMPL;
    }
    CAutoLock cAutoLock(m_propsLock);
    STSStyle tmp = m_defStyle;
    if(lf)
    {
        if(lflen == sizeof(LOGFONTA))
            tmp.fontName = ((LOGFONTA*)lf)->lfFaceName;
        else if(lflen == sizeof(LOGFONTW))
            tmp.fontName = ((LOGFONTW*)lf)->lfFaceName;
        else
            return E_INVALIDARG;

        tmp.charSet = ((LOGFONT*)lf)->lfCharSet;
        tmp.fItalic = !!((LOGFONT*)lf)->lfItalic;
        tmp.fontSize = ((LOGFONT*)lf)->lfHeight;
        tmp.fontWeight = ((LOGFONT*)lf)->lfWeight;
        tmp.fStrikeOut = !!((LOGFONT*)lf)->lfStrikeOut;
        tmp.fUnderline = !!((LOGFONT*)lf)->lfUnderline;

        if(tmp.fontSize < 0)
        {
            HDC hdc = ::GetDC(0);
            m_defStyle.fontSize = -m_defStyle.fontSize * 72 / GetDeviceCaps(hdc, LOGPIXELSY);
            ::ReleaseDC(0, hdc);
        }

    }

    tmp.colors[0] = color;
    tmp.shadowDepthX = tmp.shadowDepthY = fShadow?2:0;
    tmp.outlineWidthX = tmp.outlineWidthY = fOutline?2:0;

    if(tmp==m_defStyle) 
    {
        return S_FALSE;//Important! Avoid unnecessary deinit
    }
    else
    {
        m_defStyle = tmp;
        return OnOptionChanged(BIN2_TEXT_SETTINGS);
    }
}

STDMETHODIMP DirectVobSubImpl::get_Flip(bool* fPicture, bool* fSubtitles)
{
    HRESULT hr = XyGetBool(DirectVobSubXyOptions::BOOL_FLIP_PICTURE, fPicture);
    if (FAILED(hr))
    {
        XY_LOG_ERROR("Failed to get flip picture option."<<XY_LOG_VAR_2_STR(fPicture));
        return hr;
    }
    hr = XyGetBool(DirectVobSubXyOptions::BOOL_FLIP_SUBTITLE, fSubtitles);
    if (FAILED(hr))
    {
        XY_LOG_ERROR("Failed to get flip subtitles option."<<XY_LOG_VAR_2_STR(fPicture));
        return hr;
    }

    return S_OK;
}

STDMETHODIMP DirectVobSubImpl::put_Flip(bool fPicture, bool fSubtitles)
{
    if (!TestOption(DirectVobSubXyOptions::BOOL_FLIP_PICTURE,OPTION_TYPE_BOOL,OPTION_MODE_WRITE) || 
        !TestOption(DirectVobSubXyOptions::BOOL_FLIP_SUBTITLE,OPTION_TYPE_BOOL,OPTION_MODE_WRITE))
    {
        return E_NOTIMPL;
    }
    HRESULT hr = XySetBool(DirectVobSubXyOptions::BOOL_FLIP_PICTURE, fPicture);
    if (FAILED(hr))
    {
        XY_LOG_ERROR("Failed to set flip picture option."<<XY_LOG_VAR_2_STR(fPicture));
        return hr;
    }
    hr = XySetBool(DirectVobSubXyOptions::BOOL_FLIP_SUBTITLE, fSubtitles);
    if (FAILED(hr))
    {
        XY_LOG_ERROR("Failed to set flip subtitles option."<<XY_LOG_VAR_2_STR(fPicture));
        return hr;
    }
    return S_OK;
}

STDMETHODIMP DirectVobSubImpl::get_OSD(bool* fOSD)
{
    return XyGetBool(DirectVobSubXyOptions::BOOL_OSD, fOSD);
}

STDMETHODIMP DirectVobSubImpl::put_OSD(bool fOSD)
{
    return XySetBool(DirectVobSubXyOptions::BOOL_OSD, fOSD);
}

STDMETHODIMP DirectVobSubImpl::get_SaveFullPath(bool* fSaveFullPath)
{
    return XyGetBool(DirectVobSubXyOptions::BOOL_SAVE_FULL_PATH, fSaveFullPath);
}

STDMETHODIMP DirectVobSubImpl::put_SaveFullPath(bool fSaveFullPath)
{
    return XySetBool(DirectVobSubXyOptions::BOOL_SAVE_FULL_PATH, fSaveFullPath);
}

STDMETHODIMP DirectVobSubImpl::get_SubtitleTiming(int* delay, int* speedmul, int* speeddiv)
{
    if (!TestOption(DirectVobSubXyOptions::BIN2_SUBTITLE_TIMING, OPTION_TYPE_BIN2, OPTION_MODE_READ))
    {
        return E_NOTIMPL;
    }
    CAutoLock cAutoLock(m_propsLock);

    if(delay) *delay = m_SubtitleDelay;
    if(speedmul) *speedmul = m_SubtitleSpeedMul;
    if(speeddiv) *speeddiv = m_SubtitleSpeedDiv;

    return S_OK;
}

STDMETHODIMP DirectVobSubImpl::put_SubtitleTiming(int delay, int speedmul, int speeddiv)
{
    if (!TestOption(DirectVobSubXyOptions::BIN2_SUBTITLE_TIMING, OPTION_TYPE_BIN2, OPTION_MODE_WRITE))
    {
        return E_NOTIMPL;
    }
    CAutoLock cAutoLock(m_propsLock);

    if(m_SubtitleDelay == delay && m_SubtitleSpeedMul == speedmul && m_SubtitleSpeedDiv == speeddiv) return S_FALSE;

    m_SubtitleDelay = delay;
    m_SubtitleSpeedMul = speedmul;
    if(speeddiv > 0) m_SubtitleSpeedDiv = speeddiv;

    return OnOptionChanged(BIN2_SUBTITLE_TIMING);
}

STDMETHODIMP DirectVobSubImpl::get_MediaFPS(bool* fEnabled, double* fps)
{
    return E_NOTIMPL;
}

STDMETHODIMP DirectVobSubImpl::put_MediaFPS(bool fEnabled, double fps)
{
    return E_NOTIMPL;
}

STDMETHODIMP DirectVobSubImpl::get_ZoomRect(NORMALIZEDRECT* rect)
{
    if (!TestOption(DirectVobSubXyOptions::BIN2_ZOOM_RECT,OPTION_TYPE_BIN2, OPTION_MODE_READ))
    {
        return E_NOTIMPL;
    }

    if(!rect) return E_POINTER;

    CAutoLock cAutoLock(m_propsLock);

    *rect = m_ZoomRect;

    return S_OK;
}

STDMETHODIMP DirectVobSubImpl::put_ZoomRect(NORMALIZEDRECT* rect)
{
    if (!TestOption(DirectVobSubXyOptions::BIN2_ZOOM_RECT, OPTION_TYPE_BIN2, OPTION_MODE_WRITE))
    {
        return E_NOTIMPL;
    }

    if(!rect) return E_POINTER;

    CAutoLock cAutoLock(m_propsLock);

    if(!memcmp(&m_ZoomRect, rect, sizeof(m_ZoomRect))) return S_FALSE;

    m_ZoomRect = *rect;

    return OnOptionChanged(BIN2_ZOOM_RECT);
}

STDMETHODIMP DirectVobSubImpl::get_CachesInfo(CachesInfo* caches_info)
{
    return E_NOTIMPL;
}

STDMETHODIMP DirectVobSubImpl::get_XyFlyWeightInfo(XyFlyWeightInfo* xy_fw_info)
{
    return E_NOTIMPL;
}

STDMETHODIMP DirectVobSubImpl::HasConfigDialog(int iSelected)
{
    if (!TestOption(DirectVobSubXyOptions::void_HasConfigDialog))
    {
        return E_NOTIMPL;
    }
	return E_NOTIMPL;
}

STDMETHODIMP DirectVobSubImpl::ShowConfigDialog(int iSelected, HWND hWndParent)
{
    if (!TestOption(DirectVobSubXyOptions::void_ShowConfigDialog))
    {
        return E_NOTIMPL;
    }
	return E_NOTIMPL;
}

STDMETHODIMP DirectVobSubImpl::IsSubtitleReloaderLocked(bool* fLocked)
{
    return XyGetBool(DirectVobSubXyOptions::BOOL_SUBTITLE_RELOADER_LOCK, fLocked);
}

STDMETHODIMP DirectVobSubImpl::LockSubtitleReloader(bool fLock)
{
    if (!TestOption(DirectVobSubXyOptions::BOOL_SUBTITLE_RELOADER_LOCK, OPTION_TYPE_BOOL, OPTION_MODE_WRITE))
    {
        return E_NOTIMPL;
    }
    CAutoLock cAutoLock(m_propsLock);

    if(fLock) m_nReloaderDisableCount++;
    else m_nReloaderDisableCount--;

    ASSERT(m_nReloaderDisableCount >= 0);
    if (m_nReloaderDisableCount < 0) {
        XY_LOG_ERROR("Unexpected state. Lock & unlock calls on Subtitle reloader missed matched or the object is not initiated properly");
        m_nReloaderDisableCount = 0;
    }

    return OnOptionChanged(BOOL_SUBTITLE_RELOADER_LOCK);
}

STDMETHODIMP DirectVobSubImpl::get_SubtitleReloader(bool* fDisabled)
{
    return XyGetBool(DirectVobSubXyOptions::BOOL_SUBTITLE_RELOADER_DISABLED, fDisabled);
}

STDMETHODIMP DirectVobSubImpl::put_SubtitleReloader(bool fDisable)
{
    return XySetBool(DirectVobSubXyOptions::BOOL_SUBTITLE_RELOADER_DISABLED, fDisable);
}

STDMETHODIMP DirectVobSubImpl::get_ExtendPicture(int* horizontal, int* vertical, int* resx2, int* resx2minw, int* resx2minh)
{
    HRESULT hr = XyGetInt(DirectVobSubXyOptions::INT_EXTEND_PICTURE_HORIZONTAL, horizontal);
    if (FAILED(hr))
    {
        XY_LOG_ERROR("Failed to get option INT_EXTEND_PICTURE_HORIZONTAL"<<XY_LOG_VAR_2_STR(horizontal));
        return hr;
    }
    hr = XyGetInt(DirectVobSubXyOptions::INT_EXTEND_PICTURE_VERTICAL, vertical);
    if (FAILED(hr))
    {
        XY_LOG_ERROR("Failed to get option INT_EXTEND_PICTURE_VERTICAL"<<XY_LOG_VAR_2_STR(vertical));
        return hr;
    }
    hr = XyGetInt(DirectVobSubXyOptions::INT_EXTEND_PICTURE_RESX2, resx2);
    if (FAILED(hr))
    {
        XY_LOG_ERROR("Failed to get option INT_EXTEND_PICTURE_RESX2."<<XY_LOG_VAR_2_STR(resx2));
        return hr;
    }
    SIZE cs;
    hr = XyGetSize(DirectVobSubXyOptions::SIZE_EXTEND_PICTURE_RESX2MIN, &cs);
    if (FAILED(hr))
    {
        XY_LOG_ERROR("Failed to get option SIZE_EXTEND_PICTURE_RESX2MIN.");
        return hr;
    }
    if(resx2minw) *resx2minw = cs.cx;
    if(resx2minh) *resx2minh = cs.cy;

    return S_OK;
}

STDMETHODIMP DirectVobSubImpl::put_ExtendPicture(int horizontal, int vertical, int resx2, int resx2minw, int resx2minh)
{
    if (!TestOption(DirectVobSubXyOptions::INT_EXTEND_PICTURE_HORIZONTAL, OPTION_TYPE_INT , OPTION_MODE_WRITE) ||
        !TestOption(DirectVobSubXyOptions::INT_EXTEND_PICTURE_VERTICAL  , OPTION_TYPE_INT , OPTION_MODE_WRITE) ||
        !TestOption(DirectVobSubXyOptions::INT_EXTEND_PICTURE_RESX2     , OPTION_TYPE_INT , OPTION_MODE_WRITE) ||
        !TestOption(DirectVobSubXyOptions::SIZE_EXTEND_PICTURE_RESX2MIN , OPTION_TYPE_SIZE, OPTION_MODE_WRITE))
    {
        return E_NOTIMPL;
    }
    HRESULT hr1 = XySetInt(DirectVobSubXyOptions::INT_EXTEND_PICTURE_HORIZONTAL, horizontal);
    if (FAILED(hr1))
    {
        XY_LOG_ERROR("Failed to set option INT_EXTEND_PICTURE_HORIZONTAL"<<XY_LOG_VAR_2_STR(horizontal));
        return hr1;
    }
    HRESULT hr2 = XySetInt(DirectVobSubXyOptions::INT_EXTEND_PICTURE_VERTICAL, vertical);
    if (FAILED(hr2))
    {
        XY_LOG_ERROR("Failed to set option INT_EXTEND_PICTURE_VERTICAL"<<XY_LOG_VAR_2_STR(vertical));
        return hr2;
    }
    HRESULT hr3 = XySetInt(DirectVobSubXyOptions::INT_EXTEND_PICTURE_RESX2, resx2);
    if (FAILED(hr3))
    {
        XY_LOG_ERROR("Failed to set option INT_EXTEND_PICTURE_RESX2."<<XY_LOG_VAR_2_STR(resx2));
        return hr3;
    }
    CSize cs(resx2minw, resx2minh);
    HRESULT hr4 = XySetSize(DirectVobSubXyOptions::SIZE_EXTEND_PICTURE_RESX2MIN, cs);
    if (FAILED(hr4))
    {
        XY_LOG_ERROR("Failed to set option SIZE_EXTEND_PICTURE_RESX2MIN."<<XY_LOG_VAR_2_STR(resx2minw)<<XY_LOG_VAR_2_STR(resx2minh));
        return hr4;
    }

    return (hr1==hr2&&hr2==hr3&&hr3==hr4&&hr4==S_FALSE)? S_FALSE : S_OK;
}

STDMETHODIMP DirectVobSubImpl::get_LoadSettings(int* level, bool* fExternalLoad, bool* fWebLoad, bool* fEmbeddedLoad)
{
    HRESULT hr = XyGetInt(DirectVobSubXyOptions::INT_LOAD_SETTINGS_LEVEL, level);
    if (FAILED(hr))
    {
        XY_LOG_ERROR("Failed to get option INT_LOAD_SETTINGS_LEVEL."<<XY_LOG_VAR_2_STR(level));
        return hr;
    }
    hr = XyGetBool(DirectVobSubXyOptions::BOOL_LOAD_SETTINGS_EXTENAL, fExternalLoad);
    if (FAILED(hr))
    {
        XY_LOG_ERROR("Failed to get option BOOL_LOAD_SETTINGS_EXTENAL."<<XY_LOG_VAR_2_STR(fExternalLoad));
        return hr;
    }
    hr = XyGetBool(DirectVobSubXyOptions::BOOL_LOAD_SETTINGS_WEB, fWebLoad);
    if (FAILED(hr))
    {
        XY_LOG_ERROR("Failed to get option BOOL_LOAD_SETTINGS_WEB."<<XY_LOG_VAR_2_STR(fWebLoad));
        return hr;
    }
    hr = XyGetBool(DirectVobSubXyOptions::BOOL_LOAD_SETTINGS_EMBEDDED, fEmbeddedLoad);
    if (FAILED(hr))
    {
        XY_LOG_ERROR("Failed to get option BOOL_LOAD_SETTINGS_EMBEDDED."<<XY_LOG_VAR_2_STR(fEmbeddedLoad));
        return hr;
    }
    return hr;
}

STDMETHODIMP DirectVobSubImpl::put_LoadSettings(int level, bool fExternalLoad, bool fWebLoad, bool fEmbeddedLoad)
{
    if (!TestOption(DirectVobSubXyOptions::INT_LOAD_SETTINGS_LEVEL    , OPTION_TYPE_INT , OPTION_MODE_WRITE) ||
        !TestOption(DirectVobSubXyOptions::BOOL_LOAD_SETTINGS_EXTENAL , OPTION_TYPE_BOOL, OPTION_MODE_WRITE) ||
        !TestOption(DirectVobSubXyOptions::BOOL_LOAD_SETTINGS_WEB     , OPTION_TYPE_BOOL, OPTION_MODE_WRITE) ||
        !TestOption(DirectVobSubXyOptions::BOOL_LOAD_SETTINGS_EMBEDDED, OPTION_TYPE_BOOL, OPTION_MODE_WRITE))
    {
        return E_NOTIMPL;
    }
    HRESULT hr1 = XySetInt(DirectVobSubXyOptions::INT_LOAD_SETTINGS_LEVEL, level);
    if (FAILED(hr1))
    {
        XY_LOG_ERROR("Failed to set option INT_LOAD_SETTINGS_LEVEL."<<XY_LOG_VAR_2_STR(level));
        return hr1;
    }
    HRESULT hr2 = XySetBool(DirectVobSubXyOptions::BOOL_LOAD_SETTINGS_EXTENAL, fExternalLoad);
    if (FAILED(hr2))
    {
        XY_LOG_ERROR("Failed to set option BOOL_LOAD_SETTINGS_EXTENAL."<<XY_LOG_VAR_2_STR(fExternalLoad));
        return hr2;
    }
    HRESULT hr3 = XySetBool(DirectVobSubXyOptions::BOOL_LOAD_SETTINGS_WEB, fWebLoad);
    if (FAILED(hr3))
    {
        XY_LOG_ERROR("Failed to set option BOOL_LOAD_SETTINGS_WEB."<<XY_LOG_VAR_2_STR(fWebLoad));
        return hr3;
    }
    HRESULT hr4 = XySetBool(DirectVobSubXyOptions::BOOL_LOAD_SETTINGS_EMBEDDED, fEmbeddedLoad);
    if (FAILED(hr4))
    {
        XY_LOG_ERROR("Failed to set option BOOL_LOAD_SETTINGS_EMBEDDED."<<XY_LOG_VAR_2_STR(fEmbeddedLoad));
        return hr4;
    }
    return (hr1==hr2&&hr2==hr3&&hr3==hr4&&hr4==S_FALSE)? S_FALSE : S_OK;
}

// IDirectVobSub2

STDMETHODIMP DirectVobSubImpl::AdviseSubClock(ISubClock* pSubClock)
{
    if (!TestOption(DirectVobSubXyOptions::void_AdviseSubClock))
    {
        return E_NOTIMPL;
    }
	m_pSubClock = pSubClock;
	return S_OK;
}

STDMETHODIMP_(bool) DirectVobSubImpl::get_Forced()
{
    bool forced = false;
    if (FAILED(XyGetBool(DirectVobSubXyOptions::BOOL_FORCED_LOAD, &forced)))
    {
        XY_LOG_ERROR("Forced loading is NOT supported!");
        return false;
    }
    return forced;
}

STDMETHODIMP DirectVobSubImpl::put_Forced(bool fForced)
{
    return XySetBool(DirectVobSubXyOptions::BOOL_FORCED_LOAD, fForced);
}

STDMETHODIMP DirectVobSubImpl::get_TextSettings(STSStyle* pDefStyle)
{
    if (!TestOption(DirectVobSubXyOptions::BIN2_TEXT_SETTINGS))
    {
        return E_NOTIMPL;
    }
	CheckPointer(pDefStyle, E_POINTER);

	CAutoLock cAutoLock(m_propsLock);

	*pDefStyle = m_defStyle;

	return S_OK;
}

STDMETHODIMP DirectVobSubImpl::put_TextSettings(STSStyle* pDefStyle)
{
    if (!TestOption(DirectVobSubXyOptions::BIN2_TEXT_SETTINGS))
    {
        return E_NOTIMPL;
    }
    CheckPointer(pDefStyle, E_POINTER);

    CAutoLock cAutoLock(m_propsLock);

    if(m_defStyle==*pDefStyle)
        return S_FALSE;
    if(!memcmp(&m_defStyle, pDefStyle, sizeof(m_defStyle)))
        return S_FALSE;

    m_defStyle = *pDefStyle;

    return OnOptionChanged(BIN2_TEXT_SETTINGS);
}

STDMETHODIMP DirectVobSubImpl::get_AspectRatioSettings(CSimpleTextSubtitle::EPARCompensationType* ePARCompensationType)
{
    return XyGetInt(DirectVobSubXyOptions::INT_ASPECT_RATIO_SETTINGS, (int*)ePARCompensationType);
}

STDMETHODIMP DirectVobSubImpl::put_AspectRatioSettings(CSimpleTextSubtitle::EPARCompensationType* ePARCompensationType)
{
    CheckPointer(ePARCompensationType, E_POINTER);
    return XySetInt(DirectVobSubXyOptions::INT_ASPECT_RATIO_SETTINGS, *ePARCompensationType);
}

// IFilterVersion

STDMETHODIMP_(DWORD) DirectVobSubImpl::GetFilterVersion()
{
	return 0x0234;
}

bool DirectVobSubImpl::is_compatible()
{
    bool compatible = false;
    if ( m_config_info_version>XY_VSFILTER_VERSION_COMMIT )
    {
        compatible = m_supported_filter_verion<=XY_VSFILTER_VERSION_COMMIT;
    }
    else
    {
        compatible = REQUIRED_CONFIG_VERSION<=m_config_info_version;
    }
    return compatible;
}

UINT DirectVobSubImpl::GetCompatibleProfileInt( LPCTSTR lpszSection, LPCTSTR lpszEntry, int nDefault )
{
    UINT result = nDefault;
    if (is_compatible())
    {
        result = theApp.GetProfileInt(lpszSection, lpszEntry, nDefault);
    }
    return result;
}

CString DirectVobSubImpl::GetCompatibleProfileString( LPCTSTR lpszSection, LPCTSTR lpszEntry, LPCTSTR lpszDefault )
{
    CString result = lpszDefault;
    if (is_compatible())
    {
    	result = theApp.GetProfileString(lpszSection, lpszEntry, lpszDefault);
    }
    return result;
}

void DirectVobSubImpl::LoadKnownSourceFilters( CAtlArray<CStringW> *filter_guid,
    CAtlArray<CStringW> *filter_name )
{
    //fixme: CString VS CStringW
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    int number = theApp.GetProfileInt(ResStr(IDS_R_KNOWN_SOURCE_FILTER),
                                      ResStr(IDS_RK_TOTAL_NUMBER), -248336);
    if (number==-248336)
    {
        struct  
        {
            CStringW guid, friendly_name;
        } DefaultKnownSourceFilters[] = {
            {L"{f07e245f-5a1f-4d1e-8bff-dc31d84a55ab}", L"Ogg Splitter"                 },
            {L"{1b544c20-fd0b-11ce-8c63-00aa0044b51e}", L"Avi Stream Splitter"          },
            {L"{34293064-02F2-41D5-9D75-CC5967ACA1AB}", L"Matroska Demux"               },
            {L"{0A68C3B5-9164-4a54-AFAF-995B2FF0E0D4}", L"Matroska Source"              },
            {L"{149D2E01-C32E-4939-80F6-C07B81015A7A}", L"Matroska Splitter"            },
            {L"{55DA30FC-F16B-49fc-BAA5-AE59FC65F82D}", L"Haali Media Splitter"         },
            {L"{564FD788-86C9-4444-971E-CC4A243DA150}", L"Haali Media Splitter (AR)"    },
            {L"{8F43B7D9-9D6B-4F48-BE18-4D787C795EEA}", L"Haali Simple Media Splitter"  },
            {L"{52B63861-DC93-11CE-A099-00AA00479A58}", L"3ivx splitter"                },
            {L"{6D3688CE-3E9D-42F4-92CA-8A11119D25CD}", L"Our Ogg Source"               },
            {L"{9FF48807-E133-40AA-826F-9B2959E5232D}", L"Our Ogg Splitter"             },
            {L"{803E8280-F3CE-4201-982C-8CD8FB512004}", L"Dsm Source"                   },
            {L"{0912B4DD-A30A-4568-B590-7179EBB420EC}", L"Dsm Splitter"                 },
            {L"{3CCC052E-BDEE-408a-BEA7-90914EF2964B}", L"MP4 Source"                   },
            {L"{61F47056-E400-43d3-AF1E-AB7DFFD4C4AD}", L"MP4 Splitter"                 },
            {L"{171252A0-8820-4AFE-9DF8-5C92B2D66B04}", L"LAV Splitter"                 },
            {L"{B98D13E7-55DB-4385-A33D-09FD1BA26338}", L"LAV Splitter Source"          },
            {L"{E436EBB5-524F-11CE-9F53-0020AF0BA770}", L"Solveig Matroska Splitter"    },
            {L"{1365BE7A-C86A-473C-9A41-C0A6E82C9FA3}", L"MPC-HC MPEG PS/TS/PVA Source" },
            {L"{DC257063-045F-4BE2-BD5B-E12279C464F0}", L"MPC-HC MPEG Splitter"         },
            {L"{529A00DB-0C43-4f5b-8EF2-05004CBE0C6F}", L"AV Splitter"                  },
            {L"{D8980E15-E1F6-4916-A10F-D7EB4E9E10B8}", L"AV Source"                    },
            {L"{A2E7EDBB-DCDD-4C32-A2A9-0CFBBE6154B4}", L"PotPlayer Built-in MKV Source"},
        };

        number = countof(DefaultKnownSourceFilters);
        if (filter_guid) filter_guid->SetCount(number);
        if (filter_name) filter_name->SetCount(number);
        for (int i=0;i<number;i++)
        {
            if (filter_guid) filter_guid->GetAt(i) = DefaultKnownSourceFilters[i].guid;
            if (filter_name) filter_name->GetAt(i) = DefaultKnownSourceFilters[i].friendly_name;
        }
    }
    else
    {
        if (filter_guid) filter_guid->SetCount(number);
        if (filter_name) filter_name->SetCount(number);
        CString tmp;
        for (int i=0;i<number;i++)
        {
            if (filter_guid) {
                tmp.Format(ResStr(IDS_RK_GUID), i+1);
                filter_guid->GetAt(i) = theApp.GetProfileString(ResStr(IDS_R_KNOWN_SOURCE_FILTER), tmp, _T(""));
            }
            if (filter_name) {
                tmp.Format(ResStr(IDS_RK_FRIENDLY_NAME), i+1);
                filter_name->GetAt(i) = theApp.GetProfileString(ResStr(IDS_R_KNOWN_SOURCE_FILTER), tmp, _T(""));
            }
        }
    }
}

void DirectVobSubImpl::SaveKnownSourceFilters( const CAtlArray<CStringW>& filter_guid, 
                                               const CAtlArray<CStringW>& filter_name )
{
    //fixme: CString VS CStringW
    int number = filter_guid.GetCount();
    CString tmp;
    theApp.WriteProfileInt(ResStr(IDS_R_KNOWN_SOURCE_FILTER), ResStr(IDS_RK_TOTAL_NUMBER), number);
    for (int i=0;i<number;i++)
    {
        tmp.Format(ResStr(IDS_RK_GUID), i+1);
        theApp.WriteProfileString(ResStr(IDS_R_KNOWN_SOURCE_FILTER), tmp, filter_guid[i]);
        tmp.Format(ResStr(IDS_RK_FRIENDLY_NAME), i+1);
        theApp.WriteProfileString(ResStr(IDS_R_KNOWN_SOURCE_FILTER), tmp, filter_name[i]);
    }
}

// XyOptionsImpl

HRESULT DirectVobSubImpl::DoGetField( unsigned field, void *value )
{
    CAutoLock cAutoLock(m_propsLock);
    switch(field) {
    case BOOL_SUBTITLE_RELOADER_LOCK: 
        *(bool*)value = m_xy_bool_opt[BOOL_SUBTITLE_RELOADER_DISABLED] || m_nReloaderDisableCount > 0;
        break;
    case SIZE_LAYOUT_WITH:
        {
            SIZE *size_value = (SIZE*)value;
            switch(m_xy_int_opt[INT_LAYOUT_SIZE_OPT])
            {
            case LAYOUT_SIZE_OPT_ORIGINAL_VIDEO_SIZE:
                *size_value = m_xy_size_opt[SIZE_ORIGINAL_VIDEO];
                break;
            case LAYOUT_SIZE_OPT_AR_ADJUSTED_VIDEO_SIZE:
                *size_value = m_xy_size_opt[SIZE_AR_ADJUSTED_VIDEO];
                break;
            case LAYOUT_SIZE_OPT_USER_SPECIFIED:
                *size_value = m_xy_size_opt[SIZE_USER_SPECIFIED_LAYOUT_SIZE];
                break;
            default:
                *size_value = m_xy_size_opt[SIZE_ASS_PLAY_RESOLUTION];
                break;
            }
            if (size_value->cx * size_value->cy == 0)
            {
                *size_value = m_xy_size_opt[SIZE_ORIGINAL_VIDEO];
            }
        }
        break;
    default:
        return XyOptionsImpl::DoGetField(field, value);
        break;
    }
    return S_OK;
}

HRESULT DirectVobSubImpl::DoSetField( unsigned field, void const *value )
{
    switch(field)
    {
    case BOOL_SUBTITLE_RELOADER_LOCK:
        return LockSubtitleReloader(*(bool*)value);
    case INT_LAYOUT_SIZE_OPT:
        if (*(int*)value<0 || *(int*)value>=LAYOUT_SIZE_OPT_COUNT)
        {
            return E_INVALIDARG;
        }
        break;
    case INT_VSFILTER_COMPACT_RGB_CORRECTION:
        if (*(int*)value<0 || *(int*)value>=RGB_CORRECTION_COUNT)
        {
            return E_INVALIDARG;
        }
        break;
    case INT_SELECTED_LANGUAGE:
        int nCount;
        if(FAILED(get_LanguageCount(&nCount))
            || *(int*)value < 0 
            || *(int*)value >= nCount) 
            return E_FAIL;
        break;
    case STRING_FILE_NAME:
        {
            CAutoLock cAutoLock(m_propsLock);
            CStringW tmp = *(CStringW*)value;
            tmp = tmp.Left(tmp.ReverseFind(L'.')+1);
            CStringW file_name = m_xy_str_opt[STRING_FILE_NAME].Left(m_xy_str_opt[STRING_FILE_NAME].ReverseFind(L'.')+1);
            if(!file_name.CompareNoCase(tmp))
                return S_FALSE;
            return XyOptionsImpl::DoSetField(field, value);
        }
        break;
    }
    CAutoLock cAutoLock(m_propsLock);
    return XyOptionsImpl::DoSetField(field, value);
}

// IXyOptions

STDMETHODIMP DirectVobSubImpl::XyGetBin      (unsigned field, LPVOID    *value, int *size )
{
    if (!TestOption(field, XyOptionsImpl::OPTION_TYPE_BIN, XyOptionsImpl::OPTION_MODE_READ))
    {
        return E_NOTIMPL;
    }
    if (!size)
    {
        return E_INVALIDARG;
    }
    *size = 0;
    if (value)
    {
        *value = NULL;
    }
    CAutoLock cAutoLock(m_propsLock);
    switch(field)
    {
    case BIN_OUTPUT_COLOR_FORMAT:
        if(size)
        {
            *size = GetOutputColorSpaceNumber();
        }
        if(value && size)
        {
            *value = DEBUG_NEW ColorSpaceOpt[*size];
            ASSERT(*value);
            memcpy(*value, m_outputColorSpace, GetOutputColorSpaceNumber()*sizeof(m_outputColorSpace[0]));
        }
        return S_OK;
    case BIN_INPUT_COLOR_FORMAT:
        if(size)
        {
            *size = GetInputColorSpaceNumber();
        }
        if(value && size)
        {
            *value = DEBUG_NEW ColorSpaceOpt[*size];
            ASSERT(*value);
            memcpy(*value, m_inputColorSpace, GetInputColorSpaceNumber()*sizeof(m_inputColorSpace[0]));
        }
        return S_OK;
    }
    return E_NOTIMPL;
}

STDMETHODIMP DirectVobSubImpl::XyGetBin2( unsigned field, void *value, int size )
{
    if (!TestOption(field, XyOptionsImpl::OPTION_TYPE_BIN2, XyOptionsImpl::OPTION_MODE_READ))
    {
        return E_NOTIMPL;
    }
    CheckPointer(value, E_POINTER);
    CAutoLock cAutoLock(m_propsLock);
    switch(field)
    {
    case BIN2_CACHES_INFO:
        if (size!=sizeof(CachesInfo))
        {
            return E_INVALIDARG;
        }
        return get_CachesInfo(reinterpret_cast<CachesInfo*>(value));
    case BIN2_XY_FLY_WEIGHT_INFO:
        if (size!=sizeof(XyFlyWeightInfo))
        {
            return E_INVALIDARG;
        }
        return get_XyFlyWeightInfo(reinterpret_cast<XyFlyWeightInfo*>(value));
    case BIN2_TEXT_SETTINGS:
        if (size!=sizeof(DirectVobSubXyOptions::TextSettings))
        {
            return E_INVALIDARG;
        }
        {
            TextSettings* tmp = reinterpret_cast<TextSettings*>(value);
            return get_TextSettings(tmp->lf, tmp->lflen, &tmp->color, &tmp->shadow, &tmp->outline, &tmp->advanced_renderer);
        }
    case BIN2_SUBTITLE_TIMING:
        if (size != sizeof(DirectVobSubXyOptions::SubtitleTiming))
        {
            return E_INVALIDARG;
        }
        {
            SubtitleTiming* tmp = reinterpret_cast<SubtitleTiming*>(value);
            return get_SubtitleTiming(&tmp->delay, &tmp->speedmul, &tmp->speeddiv);
        }
    case BIN2_ZOOM_RECT:
        if (size != sizeof(NORMALIZEDRECT))
        {
            return E_INVALIDARG;
        }
        {
            NORMALIZEDRECT* tmp = reinterpret_cast<NORMALIZEDRECT*>(value);
            return get_ZoomRect(tmp);
        }
    case BIN2_CUR_STYLES:
        return GetCurStyles( reinterpret_cast<SubStyle*>(value), size );
    }
    return E_NOTIMPL;
}

STDMETHODIMP DirectVobSubImpl::XySetBin( unsigned field, LPVOID value, int size )
{
    if (!TestOption(field, XyOptionsImpl::OPTION_TYPE_BIN, XyOptionsImpl::OPTION_MODE_WRITE) &&
        !TestOption(field, XyOptionsImpl::OPTION_TYPE_BIN2, XyOptionsImpl::OPTION_MODE_WRITE))
    {
        return E_NOTIMPL;
    }
    CAutoLock cAutoLock(m_propsLock);
    switch(field)
    {
    case BIN_OUTPUT_COLOR_FORMAT:
        if( size!=GetOutputColorSpaceNumber() )
            return E_INVALIDARG;
        if(value && memcmp(m_outputColorSpace, value, size*sizeof(m_outputColorSpace[0])))
        {
            memcpy(m_outputColorSpace, value, size*sizeof(m_outputColorSpace[0]));
            return S_OK;
        }
        return S_FALSE;
    case BIN_INPUT_COLOR_FORMAT:
        if( size!=GetInputColorSpaceNumber() )
            return S_FALSE;
        if( value && memcmp(m_inputColorSpace, value, size*sizeof(m_inputColorSpace[0])))
        {
            memcpy(m_inputColorSpace, value, size*sizeof(m_inputColorSpace[0]));
            return S_OK;
        }
        return S_FALSE;
    case BIN2_TEXT_SETTINGS:
        if (size!=sizeof(DirectVobSubXyOptions::TextSettings))
        {
            return E_INVALIDARG;
        }
        {
            TextSettings* tmp = reinterpret_cast<TextSettings*>(value);
            return put_TextSettings(tmp->lf,tmp->lflen, tmp->color, tmp->shadow, tmp->outline, tmp->advanced_renderer);
        }
    case BIN2_SUBTITLE_TIMING:
        if (size != sizeof(DirectVobSubXyOptions::SubtitleTiming))
        {
            return E_INVALIDARG;
        }
        {
            SubtitleTiming* tmp = reinterpret_cast<SubtitleTiming*>(value);
            return put_SubtitleTiming(tmp->delay, tmp->speedmul, tmp->speeddiv);
        }
    case BIN2_ZOOM_RECT:
        if (size != sizeof(NORMALIZEDRECT))
        {
            return E_INVALIDARG;
        }
        return put_ZoomRect(reinterpret_cast<NORMALIZEDRECT*>(value));
    case BIN2_CUR_STYLES:
        return SetCurStyles(reinterpret_cast<const SubStyle*>(value), size);
    }
    return E_NOTIMPL;
}

//
// CDirectVobSub
//
CDirectVobSub::CDirectVobSub( const Option *options, CCritSec * pLock )
    : DirectVobSubImpl(options, pLock)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    BYTE* pData = NULL;
    UINT nSize = 0;

    m_supported_filter_verion = theApp.GetProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_SUPPORTED_VERSION), 0);
    m_config_info_version = theApp.GetProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_VERSION), 0);

    m_xy_int_opt[INT_SELECTED_LANGUAGE] = 0;
    m_xy_bool_opt[BOOL_HIDE_SUBTITLES] = !!theApp.GetProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_HIDE), 0);
    m_xy_bool_opt[BOOL_ALLOW_MOVING] = !!theApp.GetProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_ALLOWMOVING), 0);
    m_xy_bool_opt[BOOL_PRE_BUFFERING] = !!theApp.GetProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_DOPREBUFFERING), 1);

    m_xy_int_opt[INT_COLOR_SPACE] = GetCompatibleProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_COLOR_SPACE), YuvMatrix_AUTO);
    if( m_xy_int_opt[INT_COLOR_SPACE]!=YuvMatrix_AUTO && 
        m_xy_int_opt[INT_COLOR_SPACE]!=BT_601 && 
        m_xy_int_opt[INT_COLOR_SPACE]!=BT_709 &&
        m_xy_int_opt[INT_COLOR_SPACE]!=BT_2020 &&
        m_xy_int_opt[INT_COLOR_SPACE]!=GUESS)
    {
        m_xy_int_opt[INT_COLOR_SPACE] = YuvMatrix_AUTO;
    }
    m_xy_int_opt[INT_YUV_RANGE] = GetCompatibleProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_YUV_RANGE), DirectVobSubImpl::YuvRange_Auto);
    if( m_xy_int_opt[INT_YUV_RANGE]!=DirectVobSubImpl::YuvRange_Auto && 
        m_xy_int_opt[INT_YUV_RANGE]!=DirectVobSubImpl::YuvRange_PC && 
        m_xy_int_opt[INT_YUV_RANGE]!=DirectVobSubImpl::YuvRange_TV )
    {
        m_xy_int_opt[INT_YUV_RANGE] = DirectVobSubImpl::YuvRange_Auto;
    }

    m_bt601Width = theApp.GetProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_BT601_WIDTH), 1024);
    m_bt601Height = theApp.GetProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_BT601_HEIGHT), 600);

    m_xy_bool_opt[BOOL_OVERRIDE_PLACEMENT] = !!theApp.GetProfileInt(ResStr(IDS_R_TEXT), ResStr(IDS_RT_OVERRIDEPLACEMENT), 0);
    m_xy_size_opt[SIZE_PLACEMENT_PERC].cx = theApp.GetProfileInt(ResStr(IDS_R_TEXT), ResStr(IDS_RT_XPERC), 50);
    m_xy_size_opt[SIZE_PLACEMENT_PERC].cy = theApp.GetProfileInt(ResStr(IDS_R_TEXT), ResStr(IDS_RT_YPERC), 90);
    m_xy_bool_opt[BOOL_VOBSUBSETTINGS_BUFFER] = !!theApp.GetProfileInt(ResStr(IDS_R_VOBSUB), ResStr(IDS_RV_BUFFER), 1);
    m_xy_bool_opt[BOOL_VOBSUBSETTINGS_ONLY_SHOW_FORCED_SUBS] = !!theApp.GetProfileInt(ResStr(IDS_R_VOBSUB), ResStr(IDS_RV_ONLYSHOWFORCEDSUBS), 0);
    m_xy_bool_opt[BOOL_VOBSUBSETTINGS_POLYGONIZE] = !!theApp.GetProfileInt(ResStr(IDS_R_VOBSUB), ResStr(IDS_RV_POLYGONIZE), 0);
    m_defStyle <<= theApp.GetProfileString(ResStr(IDS_R_TEXT), ResStr(IDS_RT_STYLE), _T(""));
    m_xy_bool_opt[BOOL_FLIP_PICTURE]  = !!theApp.GetProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_FLIPPICTURE), 0);
    m_xy_bool_opt[BOOL_FLIP_SUBTITLE] = !!theApp.GetProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_FLIPSUBTITLES), 0);
    m_xy_bool_opt[BOOL_OSD] = !!theApp.GetProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_SHOWOSDSTATS), 0);
    m_xy_bool_opt[BOOL_SAVE_FULL_PATH] = !!theApp.GetProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_SAVEFULLPATH), 0);
    m_nReloaderDisableCount = !!theApp.GetProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_DISABLERELOADER), 0) ? 1 : 0;
    m_SubtitleDelay = theApp.GetProfileInt(ResStr(IDS_R_TIMING), ResStr(IDS_RTM_SUBTITLEDELAY), 0);
    m_SubtitleSpeedMul = theApp.GetProfileInt(ResStr(IDS_R_TIMING), ResStr(IDS_RTM_SUBTITLESPEEDMUL), 1000);
    m_SubtitleSpeedDiv = theApp.GetProfileInt(ResStr(IDS_R_TIMING), ResStr(IDS_RTM_SUBTITLESPEEDDIV), 1000);
    m_xy_int_opt[INT_ASPECT_RATIO_SETTINGS] = static_cast<CSimpleTextSubtitle::EPARCompensationType>(theApp.GetProfileInt(ResStr(IDS_R_TEXT), ResStr(IDS_RT_AUTOPARCOMPENSATION), 0));
    m_xy_bool_opt[BOOL_HIDE_TRAY_ICON] =  !!theApp.GetProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_HIDE_TRAY_ICON), 0);

    m_xy_int_opt[INT_OVERLAY_CACHE_MAX_ITEM_NUM] = GetCompatibleProfileInt(ResStr(IDS_R_PERFORMANCE), ResStr(IDS_RP_OVERLAY_CACHE_MAX_ITEM_NUM)
        , CacheManager::OVERLAY_CACHE_ITEM_NUM);
    if(m_xy_int_opt[INT_OVERLAY_CACHE_MAX_ITEM_NUM]<0) m_xy_int_opt[INT_OVERLAY_CACHE_MAX_ITEM_NUM] = 0;

    m_xy_int_opt[INT_OVERLAY_NO_BLUR_CACHE_MAX_ITEM_NUM] = GetCompatibleProfileInt(ResStr(IDS_R_PERFORMANCE), ResStr(IDS_RP_OVERLAY_NO_BLUR_CACHE_MAX_ITEM_NUM)
        , CacheManager::OVERLAY_NO_BLUR_CACHE_ITEM_NUM);
    if(m_xy_int_opt[INT_OVERLAY_NO_BLUR_CACHE_MAX_ITEM_NUM]<0) m_xy_int_opt[INT_OVERLAY_NO_BLUR_CACHE_MAX_ITEM_NUM] = 0;

    m_xy_int_opt[INT_SCAN_LINE_DATA_CACHE_MAX_ITEM_NUM] = GetCompatibleProfileInt(ResStr(IDS_R_PERFORMANCE), ResStr(IDS_RP_SCAN_LINE_DATA_CACHE_MAX_ITEM_NUM)
        , CacheManager::SCAN_LINE_DATA_CACHE_ITEM_NUM);
    if(m_xy_int_opt[INT_SCAN_LINE_DATA_CACHE_MAX_ITEM_NUM]<0) m_xy_int_opt[INT_SCAN_LINE_DATA_CACHE_MAX_ITEM_NUM] = 0;

    m_xy_int_opt[INT_PATH_DATA_CACHE_MAX_ITEM_NUM] = GetCompatibleProfileInt(ResStr(IDS_R_PERFORMANCE), ResStr(IDS_RP_PATH_DATA_CACHE_MAX_ITEM_NUM)
        , CacheManager::PATH_CACHE_ITEM_NUM);
    if(m_xy_int_opt[INT_PATH_DATA_CACHE_MAX_ITEM_NUM]<0) m_xy_int_opt[INT_PATH_DATA_CACHE_MAX_ITEM_NUM] = 0;

    m_xy_int_opt[INT_BITMAP_MRU_CACHE_ITEM_NUM] = GetCompatibleProfileInt(ResStr(IDS_R_PERFORMANCE), ResStr(IDS_RP_BITMAP_MRU_CACHE_ITEM_NUM)
        , CacheManager::BITMAP_MRU_CACHE_ITEM_NUM);
    if(m_xy_int_opt[INT_BITMAP_MRU_CACHE_ITEM_NUM]<0) m_xy_int_opt[INT_BITMAP_MRU_CACHE_ITEM_NUM] = 0;

    m_xy_int_opt[INT_CLIPPER_MRU_CACHE_ITEM_NUM] = GetCompatibleProfileInt(ResStr(IDS_R_PERFORMANCE), ResStr(IDS_RP_CLIPPER_MRU_CACHE_ITEM_NUM)
        , CacheManager::CLIPPER_MRU_CACHE_ITEM_NUM);
    if(m_xy_int_opt[INT_CLIPPER_MRU_CACHE_ITEM_NUM]<0) m_xy_int_opt[INT_CLIPPER_MRU_CACHE_ITEM_NUM] = 0;

    m_xy_int_opt[INT_TEXT_INFO_CACHE_ITEM_NUM] = GetCompatibleProfileInt(ResStr(IDS_R_PERFORMANCE), ResStr(IDS_RP_TEXT_INFO_CACHE_ITEM_NUM)
        , CacheManager::TEXT_INFO_CACHE_ITEM_NUM);
    if(m_xy_int_opt[INT_TEXT_INFO_CACHE_ITEM_NUM]<0) m_xy_int_opt[INT_TEXT_INFO_CACHE_ITEM_NUM] = 0;

    //m_xy_int_opt[INT_ASS_TAG_LIST_CACHE_ITEM_NUM] = GetCompatibleProfileInt(ResStr(IDS_R_PERFORMANCE), ResStr(IDS_RP_ASS_TAG_LIST_CACHE_ITEM_NUM)
    //    , CacheManager::ASS_TAG_LIST_CACHE_ITEM_NUM);
    //if(m_xy_int_opt[INT_ASS_TAG_LIST_CACHE_ITEM_NUM]<0) m_xy_int_opt[INT_ASS_TAG_LIST_CACHE_ITEM_NUM] = 0;

    m_xy_int_opt[INT_SUBPIXEL_POS_LEVEL] = theApp.GetProfileInt(ResStr(IDS_R_PERFORMANCE), ResStr(IDS_RP_SUBPIXEL_POS_LEVEL), SubpixelPositionControler::EIGHT_X_EIGHT);
    if(m_xy_int_opt[INT_SUBPIXEL_POS_LEVEL]<0) m_xy_int_opt[INT_SUBPIXEL_POS_LEVEL]=0;
    else if(m_xy_int_opt[INT_SUBPIXEL_POS_LEVEL]>=SubpixelPositionControler::MAX_COUNT) m_xy_int_opt[INT_SUBPIXEL_POS_LEVEL]=SubpixelPositionControler::EIGHT_X_EIGHT;

    CString str_layout_size_opt = theApp.GetProfileString(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_LAYOUT_SIZE_OPT), _T("Use Original Video Size"));
    str_layout_size_opt.MakeLower();
    if (str_layout_size_opt.Left(14)==_T("user specified"))
    {
        m_xy_int_opt[INT_LAYOUT_SIZE_OPT] = LAYOUT_SIZE_OPT_USER_SPECIFIED;
    }
    else if (str_layout_size_opt.Left(26)==_T("use ar adjusted video size"))
    {
        m_xy_int_opt[INT_LAYOUT_SIZE_OPT] = LAYOUT_SIZE_OPT_AR_ADJUSTED_VIDEO_SIZE;
    }
    else
    {
        m_xy_int_opt[INT_LAYOUT_SIZE_OPT] = LAYOUT_SIZE_OPT_ORIGINAL_VIDEO_SIZE;
    }

    m_xy_int_opt[INT_VSFILTER_COMPACT_RGB_CORRECTION] = DirectVobSubXyOptions::RGB_CORRECTION_AUTO;
    CString str_rgb_correction_setting = theApp.GetProfileString(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_RGB_CORRECTION), _T("auto"));
    str_rgb_correction_setting.MakeLower();
    if (str_rgb_correction_setting.Compare(_T("never"))==0)
    {
        m_xy_int_opt[INT_VSFILTER_COMPACT_RGB_CORRECTION] = DirectVobSubXyOptions::RGB_CORRECTION_NEVER;
    }
    else if (str_rgb_correction_setting.Compare(_T("always"))==0)
    {
        m_xy_int_opt[INT_VSFILTER_COMPACT_RGB_CORRECTION] = DirectVobSubXyOptions::RGB_CORRECTION_ALWAYS;
    }

    m_xy_size_opt[SIZE_USER_SPECIFIED_LAYOUT_SIZE].cx = theApp.GetProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_USER_SPECIFIED_LAYOUT_SIZE_X), 0);
    m_xy_size_opt[SIZE_USER_SPECIFIED_LAYOUT_SIZE].cy = theApp.GetProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_USER_SPECIFIED_LAYOUT_SIZE_Y), 0);
    if (m_xy_size_opt[SIZE_USER_SPECIFIED_LAYOUT_SIZE].cx<=0 || m_xy_size_opt[SIZE_USER_SPECIFIED_LAYOUT_SIZE].cy<=0)
    {
        m_xy_size_opt[SIZE_USER_SPECIFIED_LAYOUT_SIZE].cx = 1280;
        m_xy_size_opt[SIZE_USER_SPECIFIED_LAYOUT_SIZE].cy = 720;
    }

    m_xy_size_opt[SIZE_ASS_PLAY_RESOLUTION] = CSize(0,0);
    m_xy_size_opt[SIZE_ORIGINAL_VIDEO] = CSize(0,0);

    m_xy_bool_opt[BOOL_FOLLOW_UPSTREAM_PREFERRED_ORDER] = !!theApp.GetProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_USE_UPSTREAM_PREFERRED_ORDER), true);
    // get output colorspace config
    if(pData)
    {
        delete [] pData;
        pData = NULL;
    }
    if(theApp.GetProfileBinary(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_OUTPUT_COLORFORMATS), &pData, &nSize)
        && pData && nSize == 2*GetOutputColorSpaceNumber())
    {
        for (unsigned i=0;i<nSize/2;i++)
        {
            m_outputColorSpace[i].color_space = static_cast<ColorSpaceId>(pData[2*i]);
            m_outputColorSpace[i].selected = !!pData[2*i+1];
        }
    }
    else
    {
        for (unsigned i=0;i<GetOutputColorSpaceNumber();i++)
        {
            m_outputColorSpace[i].color_space = static_cast<ColorSpaceId>(i);
            m_outputColorSpace[i].selected = true;
        }
    }

    // get input colorspace config
    if(pData)
    {
        delete [] pData;
        pData = NULL;
    }
    if(theApp.GetProfileBinary(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_INPUT_COLORFORMATS), &pData, &nSize)
        && pData && nSize == 2*GetInputColorSpaceNumber())
    {
        for (unsigned i=0;i<nSize/2;i++)
        {
            m_inputColorSpace[i].color_space = static_cast<ColorSpaceId>(pData[2*i]);
            m_inputColorSpace[i].selected = !!pData[2*i+1];
        }        
    }
    else
    {
        for (unsigned i=0;i<GetOutputColorSpaceNumber();i++)
        {
            m_inputColorSpace[i].color_space = static_cast<ColorSpaceId>(i);
            m_inputColorSpace[i].selected = true;
        }
    }

    //
    m_ZoomRect.left = m_ZoomRect.top = 0;
    m_ZoomRect.right = m_ZoomRect.bottom = 1;

    //fix me: CStringw = CString
    m_xy_str_opt[STRING_LOAD_EXT_LIST] = 
        GetCompatibleProfileString(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_LOAD_EXT_LIST), _T("ass;ssa;srt;idx;sup;txt;usf;xss;ssf;smi;psb;rt;sub"));

    CString str_pgs_yuv_setting = theApp.GetProfileString(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_PGS_COLOR_TYPE), _T("GUESS.GUESS"));
    if (str_pgs_yuv_setting.Left(2).CompareNoCase(_T("TV"))==0)
    {
        m_xy_str_opt[STRING_PGS_YUV_RANGE] = _T("TV");
    }
    else if (str_pgs_yuv_setting.Left(2).CompareNoCase(_T("PC"))==0)
    {
        m_xy_str_opt[STRING_PGS_YUV_RANGE] = _T("PC");
    }
    else
    {
        m_xy_str_opt[STRING_PGS_YUV_RANGE] = _T("GUESS");
    }

    if (str_pgs_yuv_setting.Right(3).CompareNoCase(_T("601"))==0)
    {
        m_xy_str_opt[STRING_PGS_YUV_MATRIX] = _T("BT601");
    }
    else if (str_pgs_yuv_setting.Right(3).CompareNoCase(_T("709"))==0)
    {
        m_xy_str_opt[STRING_PGS_YUV_MATRIX] = _T("BT709");
    }
    else if (str_pgs_yuv_setting.Right(4).CompareNoCase(_T("2020"))==0)
    {
        m_xy_str_opt[STRING_PGS_YUV_MATRIX] = _T("BT2020");
    }
    else
    {
        m_xy_str_opt[STRING_PGS_YUV_MATRIX] = _T("GUESS");
    }

    CStringA version = XY_ABOUT_VERSION_STR;
    m_xy_str_opt[STRING_VERSION] = "";
    for (int i=0;i<version.GetLength();i++)
    {
        m_xy_str_opt[STRING_VERSION] += version[i];
    }
    m_xy_str_opt[STRING_YUV_MATRIX] = "None";

    m_xy_int_opt[INT_MAX_BITMAP_COUNT] = theApp.GetProfileInt(ResStr(IDS_R_PERFORMANCE), ResStr(IDS_RP_MAX_BITMAP_COUNT)
        , 8);
    if (m_xy_int_opt[INT_MAX_BITMAP_COUNT]<= 1)
    {
        m_xy_int_opt[INT_MAX_BITMAP_COUNT] = 1;
    }

    m_xy_int_opt[INT_EXTEND_PICTURE_HORIZONTAL]    = theApp.GetProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_MOD32FIX ), 0) & 1;
    m_xy_int_opt[INT_EXTEND_PICTURE_VERTICAL  ]    = theApp.GetProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_EXTPIC   ), 0);
    m_xy_int_opt[INT_EXTEND_PICTURE_RESX2     ]    = 0;// theApp.GetProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_RESX2), 2) & 3;
    m_xy_size_opt[SIZE_EXTEND_PICTURE_RESX2MIN].cx = theApp.GetProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_RESX2MINW), 384);
    m_xy_size_opt[SIZE_EXTEND_PICTURE_RESX2MIN].cy = theApp.GetProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_RESX2MINH), 288);

    m_xy_int_opt[INT_LOAD_SETTINGS_LEVEL     ] =   theApp.GetProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_LOADLEVEL), 0) & 3;
    m_xy_bool_opt[BOOL_LOAD_SETTINGS_EXTENAL ] = !!theApp.GetProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_EXTERNALLOAD), 1);
    m_xy_bool_opt[BOOL_LOAD_SETTINGS_WEB     ] = !!theApp.GetProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_WEBLOAD), 0);
    m_xy_bool_opt[BOOL_LOAD_SETTINGS_EMBEDDED] = !!theApp.GetProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_EMBEDDEDLOAD), 1);

    m_xy_bool_opt[BOOL_SUBTITLE_RELOADER_DISABLED] = !!theApp.GetProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_DISABLERELOADER), 0);

    m_xy_bool_opt[BOOL_ENABLE_ZP_ICON] = !!theApp.GetProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_ENABLEZPICON), 0);

    LoadKnownSourceFilters(&m_known_source_filters_guid, &m_known_source_filters_name);

    if(pData)
    {
        delete [] pData;
        pData = NULL;
    }

}

STDMETHODIMP CDirectVobSub::UpdateRegistry()
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    CAutoLock cAutoLock(m_propsLock);

    theApp.WriteProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_HIDE), m_xy_bool_opt[BOOL_HIDE_SUBTITLES]);
    theApp.WriteProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_ALLOWMOVING), m_xy_bool_opt[BOOL_ALLOW_MOVING]);
    theApp.WriteProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_DOPREBUFFERING), m_xy_bool_opt[BOOL_PRE_BUFFERING]);

    theApp.WriteProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_YUV_RANGE), m_xy_int_opt[INT_YUV_RANGE]);

    theApp.WriteProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_COLOR_SPACE), m_xy_int_opt[INT_COLOR_SPACE]);
    theApp.WriteProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_BT601_WIDTH), 1024);
    theApp.WriteProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_BT601_HEIGHT), 600);

    theApp.WriteProfileInt(ResStr(IDS_R_TEXT), ResStr(IDS_RT_OVERRIDEPLACEMENT), m_xy_bool_opt[BOOL_OVERRIDE_PLACEMENT]);
    theApp.WriteProfileInt(ResStr(IDS_R_TEXT), ResStr(IDS_RT_XPERC), m_xy_size_opt[SIZE_PLACEMENT_PERC].cx);
    theApp.WriteProfileInt(ResStr(IDS_R_TEXT), ResStr(IDS_RT_YPERC), m_xy_size_opt[SIZE_PLACEMENT_PERC].cy);
    theApp.WriteProfileInt(ResStr(IDS_R_VOBSUB), ResStr(IDS_RV_BUFFER), m_xy_bool_opt[BOOL_VOBSUBSETTINGS_BUFFER]);
    theApp.WriteProfileInt(ResStr(IDS_R_VOBSUB), ResStr(IDS_RV_ONLYSHOWFORCEDSUBS), m_xy_bool_opt[BOOL_VOBSUBSETTINGS_ONLY_SHOW_FORCED_SUBS]);
    theApp.WriteProfileInt(ResStr(IDS_R_VOBSUB), ResStr(IDS_RV_POLYGONIZE), m_xy_bool_opt[BOOL_VOBSUBSETTINGS_POLYGONIZE]);
    CString style;
    theApp.WriteProfileString(ResStr(IDS_R_TEXT), ResStr(IDS_RT_STYLE), style <<= m_defStyle);
    theApp.WriteProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_FLIPPICTURE), m_xy_bool_opt[BOOL_FLIP_PICTURE]);
    theApp.WriteProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_FLIPSUBTITLES), m_xy_bool_opt[BOOL_FLIP_SUBTITLE]);
    theApp.WriteProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_SHOWOSDSTATS), m_xy_bool_opt[BOOL_OSD]);
    theApp.WriteProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_SAVEFULLPATH), m_xy_bool_opt[BOOL_SAVE_FULL_PATH]);
    theApp.WriteProfileInt(ResStr(IDS_R_TIMING), ResStr(IDS_RTM_SUBTITLEDELAY), m_SubtitleDelay);
    theApp.WriteProfileInt(ResStr(IDS_R_TIMING), ResStr(IDS_RTM_SUBTITLESPEEDMUL), m_SubtitleSpeedMul);
    theApp.WriteProfileInt(ResStr(IDS_R_TIMING), ResStr(IDS_RTM_SUBTITLESPEEDDIV), m_SubtitleSpeedDiv);

    theApp.WriteProfileInt(ResStr(IDS_R_TEXT), ResStr(IDS_RT_AUTOPARCOMPENSATION), m_xy_int_opt[INT_ASPECT_RATIO_SETTINGS]);

    theApp.WriteProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_HIDE_TRAY_ICON), m_xy_bool_opt[BOOL_HIDE_TRAY_ICON]);

    theApp.WriteProfileInt(ResStr(IDS_R_PERFORMANCE), ResStr(IDS_RP_BITMAP_MRU_CACHE_ITEM_NUM), m_xy_int_opt[INT_BITMAP_MRU_CACHE_ITEM_NUM]);
    theApp.WriteProfileInt(ResStr(IDS_R_PERFORMANCE), ResStr(IDS_RP_CLIPPER_MRU_CACHE_ITEM_NUM), m_xy_int_opt[INT_CLIPPER_MRU_CACHE_ITEM_NUM]);
    theApp.WriteProfileInt(ResStr(IDS_R_PERFORMANCE), ResStr(IDS_RP_TEXT_INFO_CACHE_ITEM_NUM), m_xy_int_opt[INT_TEXT_INFO_CACHE_ITEM_NUM]);
    //theApp.WriteProfileInt(ResStr(IDS_R_PERFORMANCE), ResStr(IDS_RP_ASS_TAG_LIST_CACHE_ITEM_NUM), m_xy_int_opt[INT_ASS_TAG_LIST_CACHE_ITEM_NUM]);

    theApp.WriteProfileInt(ResStr(IDS_R_PERFORMANCE), ResStr(IDS_RP_OVERLAY_CACHE_MAX_ITEM_NUM), m_xy_int_opt[INT_OVERLAY_CACHE_MAX_ITEM_NUM]);
    theApp.WriteProfileInt(ResStr(IDS_R_PERFORMANCE), ResStr(IDS_RP_OVERLAY_NO_BLUR_CACHE_MAX_ITEM_NUM), m_xy_int_opt[INT_OVERLAY_NO_BLUR_CACHE_MAX_ITEM_NUM]);
    theApp.WriteProfileInt(ResStr(IDS_R_PERFORMANCE), ResStr(IDS_RP_SCAN_LINE_DATA_CACHE_MAX_ITEM_NUM), m_xy_int_opt[INT_SCAN_LINE_DATA_CACHE_MAX_ITEM_NUM]);
    theApp.WriteProfileInt(ResStr(IDS_R_PERFORMANCE), ResStr(IDS_RP_PATH_DATA_CACHE_MAX_ITEM_NUM), m_xy_int_opt[INT_PATH_DATA_CACHE_MAX_ITEM_NUM]);
    theApp.WriteProfileInt(ResStr(IDS_R_PERFORMANCE), ResStr(IDS_RP_SUBPIXEL_POS_LEVEL), m_xy_int_opt[INT_SUBPIXEL_POS_LEVEL]);
    theApp.WriteProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_USE_UPSTREAM_PREFERRED_ORDER), m_xy_bool_opt[BOOL_FOLLOW_UPSTREAM_PREFERRED_ORDER]);

    CString str_layout_size_opt = _T("Use Original Video Size");
    switch(m_xy_int_opt[INT_LAYOUT_SIZE_OPT])
    {
    case LAYOUT_SIZE_OPT_USER_SPECIFIED:
        str_layout_size_opt = _T("User Specified");
        break;
    case LAYOUT_SIZE_OPT_AR_ADJUSTED_VIDEO_SIZE:
        str_layout_size_opt = _T("Use AR Adjusted Video Size");
        break;
    default:
        str_layout_size_opt = _T("Use Original Video Size");
        break;
    }
    theApp.WriteProfileString(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_LAYOUT_SIZE_OPT), str_layout_size_opt);
    theApp.WriteProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_USER_SPECIFIED_LAYOUT_SIZE_X), m_xy_size_opt[SIZE_USER_SPECIFIED_LAYOUT_SIZE].cx);
    theApp.WriteProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_USER_SPECIFIED_LAYOUT_SIZE_Y), m_xy_size_opt[SIZE_USER_SPECIFIED_LAYOUT_SIZE].cy);

    CString str_rgb_correction_setting;
    switch(m_xy_int_opt[INT_VSFILTER_COMPACT_RGB_CORRECTION])
    {
    case DirectVobSubXyOptions::RGB_CORRECTION_AUTO:
        str_rgb_correction_setting = _T("auto");
        break;
    case DirectVobSubXyOptions::RGB_CORRECTION_NEVER:
        str_rgb_correction_setting = _T("never");
        break;
    case DirectVobSubXyOptions::RGB_CORRECTION_ALWAYS:
        str_rgb_correction_setting = _T("always");
        break;
    }
    theApp.WriteProfileString(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_RGB_CORRECTION), str_rgb_correction_setting);

    theApp.WriteProfileString(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_LOAD_EXT_LIST), m_xy_str_opt[STRING_LOAD_EXT_LIST]);//fix me:m_xy_str_opt[] is wide char string

    CString str_pgs_yuv_type = m_xy_str_opt[STRING_PGS_YUV_RANGE] + _T(".") + m_xy_str_opt[STRING_PGS_YUV_MATRIX];
    theApp.WriteProfileString(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_PGS_COLOR_TYPE), str_pgs_yuv_type);

    //save output color config
    {
        int count = GetOutputColorSpaceNumber();
        BYTE* pData = DEBUG_NEW BYTE[2*count];
        for(int i = 0; i < count; i++)
        {
            pData[2*i] = static_cast<BYTE>(m_outputColorSpace[i].color_space);
            pData[2*i+1] = static_cast<BYTE>(m_outputColorSpace[i].selected);
        }
        theApp.WriteProfileBinary(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_OUTPUT_COLORFORMATS), pData, 2*count);

        delete [] pData;
    }

    //save input color config
    {
        int count = GetInputColorSpaceNumber();
        BYTE* pData = DEBUG_NEW BYTE[2*count];
        for(int i = 0; i < count; i++)
        {
            pData[2*i] = static_cast<BYTE>(m_inputColorSpace[i].color_space);
            pData[2*i+1] = static_cast<BYTE>(m_inputColorSpace[i].selected);
        }
        theApp.WriteProfileBinary(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_INPUT_COLORFORMATS), pData, 2*count);

        delete [] pData;
    }

    theApp.WriteProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_MOD32FIX ), m_xy_int_opt[INT_EXTEND_PICTURE_HORIZONTAL] & 1);
    theApp.WriteProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_EXTPIC   ), m_xy_int_opt[INT_EXTEND_PICTURE_VERTICAL]);
    theApp.WriteProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_RESX2    ), m_xy_int_opt[INT_EXTEND_PICTURE_RESX2] & 3);
    theApp.WriteProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_RESX2MINW), m_xy_size_opt[SIZE_EXTEND_PICTURE_RESX2MIN].cx);
    theApp.WriteProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_RESX2MINH), m_xy_size_opt[SIZE_EXTEND_PICTURE_RESX2MIN].cy);

    theApp.WriteProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_LOADLEVEL   ), m_xy_int_opt[INT_LOAD_SETTINGS_LEVEL     ] & 3);
    theApp.WriteProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_EXTERNALLOAD), m_xy_bool_opt[BOOL_LOAD_SETTINGS_EXTENAL ] );
    theApp.WriteProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_WEBLOAD     ), m_xy_bool_opt[BOOL_LOAD_SETTINGS_WEB     ] );
    theApp.WriteProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_EMBEDDEDLOAD), m_xy_bool_opt[BOOL_LOAD_SETTINGS_EMBEDDED] );

    theApp.WriteProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_DISABLERELOADER), m_xy_bool_opt[BOOL_SUBTITLE_RELOADER_DISABLED]);

    theApp.WriteProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_ENABLEZPICON), m_xy_bool_opt[BOOL_ENABLE_ZP_ICON]);

    SaveKnownSourceFilters(m_known_source_filters_guid, m_known_source_filters_name);

    theApp.WriteProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_SUPPORTED_VERSION), CUR_SUPPORTED_FILTER_VERSION);
    theApp.WriteProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_VERSION), XY_VSFILTER_VERSION_COMMIT);
    //
    return S_OK;
}

//
// CDVS4XySubFilter
//
CDVS4XySubFilter::CDVS4XySubFilter( const Option *options, CCritSec * pLock )
    : DirectVobSubImpl(options, pLock)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    m_supported_filter_verion = theApp.GetProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_SUPPORTED_VERSION), 0);
    m_config_info_version     = theApp.GetProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_VERSION), 0);

    m_xy_int_opt [INT_SELECTED_LANGUAGE] = 0;
    m_xy_bool_opt[BOOL_HIDE_SUBTITLES  ] = !!theApp.GetProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_HIDE), 0);
    m_xy_bool_opt[BOOL_ALLOW_MOVING    ] = !!theApp.GetProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_ALLOWMOVING), 0);


    CString str_color_space = theApp.GetProfileString(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_COLOR_SPACE), _T("AUTO"));
    if (str_color_space.Left(2).CompareNoCase(_T("TV"))==0)
    {
        m_xy_int_opt[INT_YUV_RANGE] = DirectVobSubImpl::YuvRange_TV;
    }
    else if (str_color_space.Left(2).CompareNoCase(_T("PC"))==0)
    {
        m_xy_int_opt[INT_YUV_RANGE] = DirectVobSubImpl::YuvRange_PC;
    }
    else
    {
        m_xy_int_opt[INT_YUV_RANGE] = DirectVobSubImpl::YuvRange_Auto;
    }

    if (str_color_space.Right(3).CompareNoCase(_T("601"))==0)
    {
        m_xy_int_opt[INT_COLOR_SPACE] = DirectVobSubImpl::BT_601;
    }
    else if (str_color_space.Right(3).CompareNoCase(_T("709"))==0)
    {
        m_xy_int_opt[INT_COLOR_SPACE] = DirectVobSubImpl::BT_709;
    }
    else if (str_color_space.Right(4).CompareNoCase(_T("2020"))==0)
    {
        m_xy_int_opt[INT_COLOR_SPACE] = DirectVobSubImpl::BT_2020;
    }
    else if (str_color_space.Right(5).CompareNoCase(_T("GUESS"))==0)
    {
        m_xy_int_opt[INT_COLOR_SPACE] = DirectVobSubImpl::GUESS;
    }
    else
    {
        m_xy_int_opt[INT_COLOR_SPACE] = DirectVobSubImpl::YuvMatrix_AUTO;
    }

    m_bt601Width  = theApp.GetProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_BT601_WIDTH), 1024);
    m_bt601Height = theApp.GetProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_BT601_HEIGHT), 600);

    m_xy_bool_opt[BOOL_OVERRIDE_PLACEMENT]                   = !!theApp.GetProfileInt(ResStr(IDS_R_TEXT), ResStr(IDS_RT_OVERRIDEPLACEMENT), 0);
    m_xy_size_opt[SIZE_PLACEMENT_PERC].cx                    =   theApp.GetProfileInt(ResStr(IDS_R_TEXT), ResStr(IDS_RT_XPERC), 50);
    m_xy_size_opt[SIZE_PLACEMENT_PERC].cy                    =   theApp.GetProfileInt(ResStr(IDS_R_TEXT), ResStr(IDS_RT_YPERC), 90);
    m_xy_bool_opt[BOOL_VOBSUBSETTINGS_BUFFER]                = !!theApp.GetProfileInt(ResStr(IDS_R_VOBSUB), ResStr(IDS_RV_BUFFER), 1);
    m_xy_bool_opt[BOOL_VOBSUBSETTINGS_ONLY_SHOW_FORCED_SUBS] = !!theApp.GetProfileInt(ResStr(IDS_R_VOBSUB), ResStr(IDS_RV_ONLYSHOWFORCEDSUBS), 0);
    m_xy_bool_opt[BOOL_VOBSUBSETTINGS_POLYGONIZE]            = !!theApp.GetProfileInt(ResStr(IDS_R_VOBSUB), ResStr(IDS_RV_POLYGONIZE), 0);
    m_defStyle <<= theApp.GetProfileString(ResStr(IDS_R_TEXT), ResStr(IDS_RT_STYLE), _T(""));

    m_nReloaderDisableCount                 = !!theApp.GetProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_DISABLERELOADER), 0) ? 1 : 0;
    m_SubtitleDelay                         =   theApp.GetProfileInt(ResStr(IDS_R_TIMING), ResStr(IDS_RTM_SUBTITLEDELAY), 0);
    m_SubtitleSpeedMul                      =   theApp.GetProfileInt(ResStr(IDS_R_TIMING), ResStr(IDS_RTM_SUBTITLESPEEDMUL), 1000);
    m_SubtitleSpeedDiv                      =   theApp.GetProfileInt(ResStr(IDS_R_TIMING), ResStr(IDS_RTM_SUBTITLESPEEDDIV), 1000);

    m_xy_bool_opt[BOOL_HIDE_TRAY_ICON]      = !!theApp.GetProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_HIDE_TRAY_ICON), 0);

    m_xy_int_opt[INT_OVERLAY_CACHE_MAX_ITEM_NUM] = GetCompatibleProfileInt(ResStr(IDS_R_PERFORMANCE), ResStr(IDS_RP_OVERLAY_CACHE_MAX_ITEM_NUM)
        , CacheManager::OVERLAY_CACHE_ITEM_NUM);
    if(m_xy_int_opt[INT_OVERLAY_CACHE_MAX_ITEM_NUM]<0) m_xy_int_opt[INT_OVERLAY_CACHE_MAX_ITEM_NUM] = 0;

    m_xy_int_opt[INT_OVERLAY_NO_BLUR_CACHE_MAX_ITEM_NUM] = GetCompatibleProfileInt(ResStr(IDS_R_PERFORMANCE), ResStr(IDS_RP_OVERLAY_NO_BLUR_CACHE_MAX_ITEM_NUM)
        , CacheManager::OVERLAY_NO_BLUR_CACHE_ITEM_NUM);
    if(m_xy_int_opt[INT_OVERLAY_NO_BLUR_CACHE_MAX_ITEM_NUM]<0) m_xy_int_opt[INT_OVERLAY_NO_BLUR_CACHE_MAX_ITEM_NUM] = 0;

    m_xy_int_opt[INT_SCAN_LINE_DATA_CACHE_MAX_ITEM_NUM] = GetCompatibleProfileInt(ResStr(IDS_R_PERFORMANCE), ResStr(IDS_RP_SCAN_LINE_DATA_CACHE_MAX_ITEM_NUM)
        , CacheManager::SCAN_LINE_DATA_CACHE_ITEM_NUM);
    if(m_xy_int_opt[INT_SCAN_LINE_DATA_CACHE_MAX_ITEM_NUM]<0) m_xy_int_opt[INT_SCAN_LINE_DATA_CACHE_MAX_ITEM_NUM] = 0;

    m_xy_int_opt[INT_PATH_DATA_CACHE_MAX_ITEM_NUM] = GetCompatibleProfileInt(ResStr(IDS_R_PERFORMANCE), ResStr(IDS_RP_PATH_DATA_CACHE_MAX_ITEM_NUM)
        , CacheManager::PATH_CACHE_ITEM_NUM);
    if(m_xy_int_opt[INT_PATH_DATA_CACHE_MAX_ITEM_NUM]<0) m_xy_int_opt[INT_PATH_DATA_CACHE_MAX_ITEM_NUM] = 0;

    m_xy_int_opt[INT_BITMAP_MRU_CACHE_ITEM_NUM] = GetCompatibleProfileInt(ResStr(IDS_R_PERFORMANCE), ResStr(IDS_RP_BITMAP_MRU_CACHE_ITEM_NUM)
        , CacheManager::BITMAP_MRU_CACHE_ITEM_NUM);
    if(m_xy_int_opt[INT_BITMAP_MRU_CACHE_ITEM_NUM]<0) m_xy_int_opt[INT_BITMAP_MRU_CACHE_ITEM_NUM] = 0;

    m_xy_int_opt[INT_CLIPPER_MRU_CACHE_ITEM_NUM] = GetCompatibleProfileInt(ResStr(IDS_R_PERFORMANCE), ResStr(IDS_RP_CLIPPER_MRU_CACHE_ITEM_NUM)
        , CacheManager::CLIPPER_MRU_CACHE_ITEM_NUM);
    if(m_xy_int_opt[INT_CLIPPER_MRU_CACHE_ITEM_NUM]<0) m_xy_int_opt[INT_CLIPPER_MRU_CACHE_ITEM_NUM] = 0;

    m_xy_int_opt[INT_TEXT_INFO_CACHE_ITEM_NUM] = GetCompatibleProfileInt(ResStr(IDS_R_PERFORMANCE), ResStr(IDS_RP_TEXT_INFO_CACHE_ITEM_NUM)
        , CacheManager::TEXT_INFO_CACHE_ITEM_NUM);
    if(m_xy_int_opt[INT_TEXT_INFO_CACHE_ITEM_NUM]<0) m_xy_int_opt[INT_TEXT_INFO_CACHE_ITEM_NUM] = 0;

    //m_xy_int_opt[INT_ASS_TAG_LIST_CACHE_ITEM_NUM] = GetCompatibleProfileInt(ResStr(IDS_R_PERFORMANCE), ResStr(IDS_RP_ASS_TAG_LIST_CACHE_ITEM_NUM)
    //    , CacheManager::ASS_TAG_LIST_CACHE_ITEM_NUM);
    //if(m_xy_int_opt[INT_ASS_TAG_LIST_CACHE_ITEM_NUM]<0) m_xy_int_opt[INT_ASS_TAG_LIST_CACHE_ITEM_NUM] = 0;

    m_xy_int_opt[INT_MAX_CACHE_SIZE_MB] = theApp.GetProfileInt(ResStr(IDS_R_PERFORMANCE), ResStr(IDS_RP_MAX_CACHE_SIZE_MB), -1);

    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof (statex);
    GlobalMemoryStatusEx (&statex);
    m_xy_int_opt[INT_AUTO_MAX_CACHE_SIZE_MB] = (statex.ullTotalPhys < statex.ullTotalVirtual ?
                                                statex.ullTotalPhys : statex.ullTotalVirtual)>>22;
    m_xy_int_opt[INT_AUTO_MAX_CACHE_SIZE_MB] = m_xy_int_opt[INT_AUTO_MAX_CACHE_SIZE_MB] < (SIZE_MAX>>23) ? 
                                               m_xy_int_opt[INT_AUTO_MAX_CACHE_SIZE_MB] : (SIZE_MAX>>23);

    m_xy_int_opt[INT_SUBPIXEL_POS_LEVEL] = theApp.GetProfileInt(ResStr(IDS_R_PERFORMANCE), ResStr(IDS_RP_SUBPIXEL_POS_LEVEL), SubpixelPositionControler::EIGHT_X_EIGHT);
    if(m_xy_int_opt[INT_SUBPIXEL_POS_LEVEL]<0) m_xy_int_opt[INT_SUBPIXEL_POS_LEVEL]=0;
    else if(m_xy_int_opt[INT_SUBPIXEL_POS_LEVEL]>=SubpixelPositionControler::MAX_COUNT) m_xy_int_opt[INT_SUBPIXEL_POS_LEVEL]=SubpixelPositionControler::EIGHT_X_EIGHT;

    CString str_layout_size_opt = theApp.GetProfileString(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_LAYOUT_SIZE_OPT), _T("Use Original Video Size"));
    str_layout_size_opt.MakeLower();
    if (str_layout_size_opt.Left(14)==_T("user specified"))
    {
        m_xy_int_opt[INT_LAYOUT_SIZE_OPT] = LAYOUT_SIZE_OPT_USER_SPECIFIED;
    }
    else if (str_layout_size_opt.Left(26)==_T("use ar adjusted video size"))
    {
        m_xy_int_opt[INT_LAYOUT_SIZE_OPT] = LAYOUT_SIZE_OPT_AR_ADJUSTED_VIDEO_SIZE;
    }
    else
    {
        m_xy_int_opt[INT_LAYOUT_SIZE_OPT] = LAYOUT_SIZE_OPT_ORIGINAL_VIDEO_SIZE;
    }

    m_xy_int_opt[INT_VSFILTER_COMPACT_RGB_CORRECTION] = DirectVobSubXyOptions::RGB_CORRECTION_AUTO;
    CString str_rgb_correction_setting = theApp.GetProfileString(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_RGB_CORRECTION), _T("auto"));
    str_rgb_correction_setting.MakeLower();
    if (str_rgb_correction_setting.Compare(_T("never"))==0)
    {
        m_xy_int_opt[INT_VSFILTER_COMPACT_RGB_CORRECTION] = DirectVobSubXyOptions::RGB_CORRECTION_NEVER;
    }
    else if (str_rgb_correction_setting.Compare(_T("always"))==0)
    {
        m_xy_int_opt[INT_VSFILTER_COMPACT_RGB_CORRECTION] = DirectVobSubXyOptions::RGB_CORRECTION_ALWAYS;
    }

    m_xy_size_opt[SIZE_USER_SPECIFIED_LAYOUT_SIZE].cx = theApp.GetProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_USER_SPECIFIED_LAYOUT_SIZE_X), 0);
    m_xy_size_opt[SIZE_USER_SPECIFIED_LAYOUT_SIZE].cy = theApp.GetProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_USER_SPECIFIED_LAYOUT_SIZE_Y), 0);
    if (m_xy_size_opt[SIZE_USER_SPECIFIED_LAYOUT_SIZE].cx<=0 || m_xy_size_opt[SIZE_USER_SPECIFIED_LAYOUT_SIZE].cy<=0)
    {
        m_xy_size_opt[SIZE_USER_SPECIFIED_LAYOUT_SIZE].cx = 1280;
        m_xy_size_opt[SIZE_USER_SPECIFIED_LAYOUT_SIZE].cy = 720;
    }

    m_ZoomRect.left = m_ZoomRect.top = 0;
    m_ZoomRect.right = m_ZoomRect.bottom = 1;

    //fix me: CStringw = CString
    m_xy_str_opt[STRING_LOAD_EXT_LIST] = 
        GetCompatibleProfileString(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_LOAD_EXT_LIST), _T("ass;ssa;srt;idx;sup;txt;usf;xss;ssf;smi;psb;rt;sub"));

    CString str_pgs_yuv_setting = theApp.GetProfileString(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_PGS_COLOR_TYPE), _T("GUESS.GUESS"));
    if (str_pgs_yuv_setting.Left(2).CompareNoCase(_T("TV"))==0)
    {
        m_xy_str_opt[STRING_PGS_YUV_RANGE] = _T("TV");
    }
    else if (str_pgs_yuv_setting.Left(2).CompareNoCase(_T("PC"))==0)
    {
        m_xy_str_opt[STRING_PGS_YUV_RANGE] = _T("PC");
    }
    else
    {
        m_xy_str_opt[STRING_PGS_YUV_RANGE] = _T("GUESS");
    }

    if (str_pgs_yuv_setting.Right(3).CompareNoCase(_T("601"))==0)
    {
        m_xy_str_opt[STRING_PGS_YUV_MATRIX] = _T("BT601");
    }
    else if (str_pgs_yuv_setting.Right(3).CompareNoCase(_T("709"))==0)
    {
        m_xy_str_opt[STRING_PGS_YUV_MATRIX] = _T("BT709");
    }
    else if (str_pgs_yuv_setting.Right(4).CompareNoCase(_T("2020"))==0)
    {
        m_xy_str_opt[STRING_PGS_YUV_MATRIX] = _T("BT2020");
    }
    else
    {
        m_xy_str_opt[STRING_PGS_YUV_MATRIX] = _T("GUESS");
    }

    CStringA version = XY_ABOUT_VERSION_STR;
    m_xy_str_opt[STRING_VERSION] = "";
    for (int i=0;i<version.GetLength();i++)
    {
        m_xy_str_opt[STRING_VERSION] += version[i];
    }
    m_xy_str_opt[STRING_YUV_MATRIX] = "None";

    m_xy_int_opt[INT_MAX_BITMAP_COUNT] = theApp.GetProfileInt(ResStr(IDS_R_PERFORMANCE), ResStr(IDS_RP_MAX_BITMAP_COUNT)
        , 8);
    if (m_xy_int_opt[INT_MAX_BITMAP_COUNT]<= 1)
    {
        m_xy_int_opt[INT_MAX_BITMAP_COUNT] = 1;
    }

    m_xy_int_opt[INT_LOAD_SETTINGS_LEVEL     ] = LOADLEVEL_WHEN_NEEDED;
    CString str_load_level = theApp.GetProfileString(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_LOADLEVEL), _T("when_needed"));
    str_load_level.MakeLower();
    if (str_load_level.Compare(_T("always"))==0)
    {
        m_xy_int_opt[INT_LOAD_SETTINGS_LEVEL] = LOADLEVEL_ALWAYS;
    }
    else if (str_load_level.Compare(_T("disabled"))==0)
    {
        m_xy_int_opt[INT_LOAD_SETTINGS_LEVEL] = LOADLEVEL_DISABLED;
    }

    m_xy_bool_opt[BOOL_LOAD_SETTINGS_EXTENAL ] = !!theApp.GetProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_EXTERNALLOAD), 1);
    m_xy_bool_opt[BOOL_LOAD_SETTINGS_WEB     ] = !!theApp.GetProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_WEBLOAD), 0);
    m_xy_bool_opt[BOOL_LOAD_SETTINGS_EMBEDDED] = !!theApp.GetProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_EMBEDDEDLOAD), 1);

    m_xy_bool_opt[BOOL_SUBTITLE_RELOADER_DISABLED] = !!theApp.GetProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_DISABLERELOADER), 0);

    CString str_rgb_output_level = theApp.GetProfileString(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_RGB_OUTPUT_LEVEL), _T("AUTO"));
    str_rgb_output_level.MakeLower();
    if (str_rgb_output_level.Left(2)==_T("pc"))
    {
        m_xy_int_opt[INT_RGB_OUTPUT_TV_LEVEL] = RGB_OUTPUT_LEVEL_PC;
    }
    else if (str_rgb_output_level.Left(8)==_T("force_tv"))
    {
        m_xy_int_opt[INT_RGB_OUTPUT_TV_LEVEL] = RGB_OUTPUT_LEVEL_FORCE_TV;
    }
    else
    {
        m_xy_int_opt[INT_RGB_OUTPUT_TV_LEVEL] = RGB_OUTPUT_LEVEL_AUTO;
    }

    m_xy_bool_opt[BOOL_RENDER_TO_ORIGINAL_VIDEO_SIZE] = !!theApp.GetProfileInt(ResStr(IDS_R_GENERAL)
        , ResStr(IDS_RG_RENDER_TO_ORIGINAL_VIDEO_SIZE), 0);

    m_xy_bool_opt[BOOL_FORCE_DEFAULT_STYLE] = !!theApp.GetProfileInt(ResStr(IDS_R_GENERAL)
        , ResStr(IDS_RG_FORCE_DEFAULT_STYLE), 0);

    LoadKnownSourceFilters(&m_known_source_filters_guid, &m_known_source_filters_name);
}

STDMETHODIMP CDVS4XySubFilter::UpdateRegistry()
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    CAutoLock cAutoLock(m_propsLock);

    theApp.WriteProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_HIDE), m_xy_bool_opt[BOOL_HIDE_SUBTITLES]);
    theApp.WriteProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_ALLOWMOVING), m_xy_bool_opt[BOOL_ALLOW_MOVING]);

    CString str_color_space;
    switch(m_xy_int_opt[INT_YUV_RANGE])
    {
    case DirectVobSubImpl::YuvRange_TV:
        str_color_space = _T("TV");
        break;
    case DirectVobSubImpl::YuvRange_PC:
        str_color_space = _T("PC");
        break;
    default:
        str_color_space = _T("AUTO");
        break;
    }
    switch(m_xy_int_opt[INT_COLOR_SPACE])
    {
    case DirectVobSubImpl::BT_601:
        str_color_space += _T(".BT601");
        break;
    case DirectVobSubImpl::BT_709:
        str_color_space += _T(".BT709");
        break;
    case DirectVobSubImpl::BT_2020:
        str_color_space += _T(".BT2020");
        break;
    case DirectVobSubImpl::GUESS:
        str_color_space += _T(".GUESS");
        break;
    default:
        str_color_space += _T(".AUTO");
        break;
    }
    theApp.WriteProfileString(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_COLOR_SPACE), str_color_space);

    theApp.WriteProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_BT601_WIDTH), 1024);
    theApp.WriteProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_BT601_HEIGHT), 600);

    theApp.WriteProfileInt(ResStr(IDS_R_TEXT), ResStr(IDS_RT_OVERRIDEPLACEMENT), m_xy_bool_opt[BOOL_OVERRIDE_PLACEMENT]);
    theApp.WriteProfileInt(ResStr(IDS_R_TEXT), ResStr(IDS_RT_XPERC), m_xy_size_opt[SIZE_PLACEMENT_PERC].cx);
    theApp.WriteProfileInt(ResStr(IDS_R_TEXT), ResStr(IDS_RT_YPERC), m_xy_size_opt[SIZE_PLACEMENT_PERC].cy);
    theApp.WriteProfileInt(ResStr(IDS_R_VOBSUB), ResStr(IDS_RV_BUFFER), m_xy_bool_opt[BOOL_VOBSUBSETTINGS_BUFFER]);
    theApp.WriteProfileInt(ResStr(IDS_R_VOBSUB), ResStr(IDS_RV_ONLYSHOWFORCEDSUBS), m_xy_bool_opt[BOOL_VOBSUBSETTINGS_ONLY_SHOW_FORCED_SUBS]);
    theApp.WriteProfileInt(ResStr(IDS_R_VOBSUB), ResStr(IDS_RV_POLYGONIZE), m_xy_bool_opt[BOOL_VOBSUBSETTINGS_POLYGONIZE]);
    CString style;
    theApp.WriteProfileString(ResStr(IDS_R_TEXT), ResStr(IDS_RT_STYLE), style <<= m_defStyle);
    theApp.WriteProfileInt(ResStr(IDS_R_TIMING), ResStr(IDS_RTM_SUBTITLEDELAY), m_SubtitleDelay);
    theApp.WriteProfileInt(ResStr(IDS_R_TIMING), ResStr(IDS_RTM_SUBTITLESPEEDMUL), m_SubtitleSpeedMul);
    theApp.WriteProfileInt(ResStr(IDS_R_TIMING), ResStr(IDS_RTM_SUBTITLESPEEDDIV), m_SubtitleSpeedDiv);

    theApp.WriteProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_HIDE_TRAY_ICON), m_xy_bool_opt[BOOL_HIDE_TRAY_ICON]);

    theApp.WriteProfileInt(ResStr(IDS_R_PERFORMANCE), ResStr(IDS_RP_BITMAP_MRU_CACHE_ITEM_NUM), m_xy_int_opt[INT_BITMAP_MRU_CACHE_ITEM_NUM]);
    theApp.WriteProfileInt(ResStr(IDS_R_PERFORMANCE), ResStr(IDS_RP_CLIPPER_MRU_CACHE_ITEM_NUM), m_xy_int_opt[INT_CLIPPER_MRU_CACHE_ITEM_NUM]);
    theApp.WriteProfileInt(ResStr(IDS_R_PERFORMANCE), ResStr(IDS_RP_TEXT_INFO_CACHE_ITEM_NUM), m_xy_int_opt[INT_TEXT_INFO_CACHE_ITEM_NUM]);
    //theApp.WriteProfileInt(ResStr(IDS_R_PERFORMANCE), ResStr(IDS_RP_ASS_TAG_LIST_CACHE_ITEM_NUM), m_xy_int_opt[INT_ASS_TAG_LIST_CACHE_ITEM_NUM]);

    theApp.WriteProfileInt(ResStr(IDS_R_PERFORMANCE), ResStr(IDS_RP_OVERLAY_CACHE_MAX_ITEM_NUM), m_xy_int_opt[INT_OVERLAY_CACHE_MAX_ITEM_NUM]);
    theApp.WriteProfileInt(ResStr(IDS_R_PERFORMANCE), ResStr(IDS_RP_OVERLAY_NO_BLUR_CACHE_MAX_ITEM_NUM), m_xy_int_opt[INT_OVERLAY_NO_BLUR_CACHE_MAX_ITEM_NUM]);
    theApp.WriteProfileInt(ResStr(IDS_R_PERFORMANCE), ResStr(IDS_RP_SCAN_LINE_DATA_CACHE_MAX_ITEM_NUM), m_xy_int_opt[INT_SCAN_LINE_DATA_CACHE_MAX_ITEM_NUM]);
    theApp.WriteProfileInt(ResStr(IDS_R_PERFORMANCE), ResStr(IDS_RP_PATH_DATA_CACHE_MAX_ITEM_NUM), m_xy_int_opt[INT_PATH_DATA_CACHE_MAX_ITEM_NUM]);
    theApp.WriteProfileInt(ResStr(IDS_R_PERFORMANCE), ResStr(IDS_RP_MAX_CACHE_SIZE_MB), m_xy_int_opt[INT_MAX_CACHE_SIZE_MB]);
    theApp.WriteProfileInt(ResStr(IDS_R_PERFORMANCE), ResStr(IDS_RP_SUBPIXEL_POS_LEVEL), m_xy_int_opt[INT_SUBPIXEL_POS_LEVEL]);

    CString str_layout_size_opt = _T("Use Original Video Size");
    switch(m_xy_int_opt[INT_LAYOUT_SIZE_OPT])
    {
    case LAYOUT_SIZE_OPT_USER_SPECIFIED:
        str_layout_size_opt = _T("User Specified");
        break;
    case LAYOUT_SIZE_OPT_AR_ADJUSTED_VIDEO_SIZE:
        str_layout_size_opt = _T("Use AR Adjusted Video Size");
        break;
    default:
        str_layout_size_opt = _T("Use Original Video Size");
        break;
    }
    theApp.WriteProfileString(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_LAYOUT_SIZE_OPT), str_layout_size_opt);
    theApp.WriteProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_USER_SPECIFIED_LAYOUT_SIZE_X), m_xy_size_opt[SIZE_USER_SPECIFIED_LAYOUT_SIZE].cx);
    theApp.WriteProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_USER_SPECIFIED_LAYOUT_SIZE_Y), m_xy_size_opt[SIZE_USER_SPECIFIED_LAYOUT_SIZE].cy);

    CString str_rgb_correction_setting;
    switch(m_xy_int_opt[INT_VSFILTER_COMPACT_RGB_CORRECTION])
    {
    case DirectVobSubXyOptions::RGB_CORRECTION_AUTO:
        str_rgb_correction_setting = _T("auto");
        break;
    case DirectVobSubXyOptions::RGB_CORRECTION_NEVER:
        str_rgb_correction_setting = _T("never");
        break;
    case DirectVobSubXyOptions::RGB_CORRECTION_ALWAYS:
        str_rgb_correction_setting = _T("always");
        break;
    }
    theApp.WriteProfileString(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_RGB_CORRECTION), str_rgb_correction_setting);

    theApp.WriteProfileString(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_LOAD_EXT_LIST), m_xy_str_opt[STRING_LOAD_EXT_LIST]);//fix me:m_xy_str_opt[] is wide char string

    CString str_pgs_yuv_type = m_xy_str_opt[STRING_PGS_YUV_RANGE] + _T(".") + m_xy_str_opt[STRING_PGS_YUV_MATRIX];
    theApp.WriteProfileString(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_PGS_COLOR_TYPE), str_pgs_yuv_type);

    CString str_load_level;
    switch(m_xy_int_opt[INT_LOAD_SETTINGS_LEVEL])
    {
    case LOADLEVEL_ALWAYS:
        str_load_level = _T("always");
        break;
    case LOADLEVEL_DISABLED:
        str_load_level = _T("disabled");
        break;
    default:
        str_load_level = _T("when_needed");
        break;
    }
    theApp.WriteProfileString(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_LOADLEVEL), str_load_level);

    theApp.WriteProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_EXTERNALLOAD), m_xy_bool_opt[BOOL_LOAD_SETTINGS_EXTENAL ] );
    theApp.WriteProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_WEBLOAD     ), m_xy_bool_opt[BOOL_LOAD_SETTINGS_WEB     ] );
    theApp.WriteProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_EMBEDDEDLOAD), m_xy_bool_opt[BOOL_LOAD_SETTINGS_EMBEDDED] );

    theApp.WriteProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_DISABLERELOADER), m_xy_bool_opt[BOOL_SUBTITLE_RELOADER_DISABLED]);

    CString str_rgb_output_level;
    switch (m_xy_int_opt[INT_RGB_OUTPUT_TV_LEVEL])
    {
    case RGB_OUTPUT_LEVEL_PC:
        str_rgb_output_level = _T("PC");
        break;
    case RGB_OUTPUT_LEVEL_FORCE_TV:
        str_rgb_output_level = _T("FORCE_TV");
        break;
    default:
        str_rgb_output_level = _T("AUTO");
        break;
    }
    theApp.WriteProfileString(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_RGB_OUTPUT_LEVEL), str_rgb_output_level);

    theApp.WriteProfileInt(ResStr(IDS_R_GENERAL)
        , ResStr(IDS_RG_RENDER_TO_ORIGINAL_VIDEO_SIZE), m_xy_bool_opt[BOOL_RENDER_TO_ORIGINAL_VIDEO_SIZE]);

    theApp.WriteProfileInt(ResStr(IDS_R_GENERAL)
        , ResStr(IDS_RG_FORCE_DEFAULT_STYLE), m_xy_bool_opt[BOOL_FORCE_DEFAULT_STYLE]);

    SaveKnownSourceFilters(m_known_source_filters_guid, m_known_source_filters_name);

    theApp.WriteProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_SUPPORTED_VERSION), CUR_SUPPORTED_FILTER_VERSION);
    theApp.WriteProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_VERSION), XY_VSFILTER_VERSION_COMMIT);
    //
    return S_OK;
}
