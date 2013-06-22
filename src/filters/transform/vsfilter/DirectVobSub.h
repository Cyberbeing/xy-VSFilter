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

#include "IDirectVobSub.h"
#include "IDirectVobSubXy.h"
#include "..\..\..\..\include\IFilterVersion.h"
#include "version.h"
#include "XyOptionsImpl.h"

class CDirectVobSub : public IDirectVobSub2, public XyOptionsImpl, public IFilterVersion
{
public:
    typedef XyOptionsImpl::Option Option;

    enum ColorSpaceOption
    {
        YuvMatrix_AUTO = 0
        ,BT_601
        ,BT_709
        ,GUESS
    };
    enum YuvRange
    {
        YuvRange_Auto = 0
        ,YuvRange_TV
        ,YuvRange_PC
    };
    enum LoadLevel
    {
        LOADLEVEL_WHEN_NEEDED = 0,
        LOADLEVEL_ALWAYS = 1,
        LOADLEVEL_DISABLED = 2,
        LOADLEVEL_COUNT
    };

    static const int REQUIRED_CONFIG_VERSION = 209;
    static const int CUR_SUPPORTED_FILTER_VERSION = 39;

    typedef DirectVobSubXyOptions::CachesInfo CachesInfo;
    typedef DirectVobSubXyOptions::XyFlyWeightInfo XyFlyWeightInfo;
    typedef DirectVobSubXyOptions::ColorSpaceOpt ColorSpaceOpt;
protected:
    CDirectVobSub(const Option *options, CCritSec * pLock);
    virtual ~CDirectVobSub();

    bool is_compatible();
    UINT GetCompatibleProfileInt(LPCTSTR lpszSection, LPCTSTR lpszEntry, int nDefault);
protected:
    CCritSec *m_propsLock;

    int m_bt601Width, m_bt601Height;//for AUTO_GUESS

    static int const MAX_COLOR_SPACE = 256;
    ColorSpaceOpt m_outputColorSpace[MAX_COLOR_SPACE];
    ColorSpaceOpt m_inputColorSpace[MAX_COLOR_SPACE];

	STSStyle m_defStyle;

	bool m_fAdvancedRenderer;
	int m_nReloaderDisableCount;
	int m_SubtitleDelay, m_SubtitleSpeedMul, m_SubtitleSpeedDiv;
	NORMALIZEDRECT m_ZoomRect;

    int m_supported_filter_verion;
    int m_config_info_version;

	CComPtr<ISubClock> m_pSubClock;

public:
    // XyOptionsImpl
    HRESULT OnOptionReading(unsigned field);

    // IXyOptions

    STDMETHODIMP XyGetBool     (unsigned field, bool      *value);
    STDMETHODIMP XyGetInt      (unsigned field, int       *value);
    STDMETHODIMP XyGetSize     (unsigned field, SIZE      *value);
    STDMETHODIMP XyGetRect     (unsigned field, RECT      *value);
    STDMETHODIMP XyGetUlonglong(unsigned field, ULONGLONG *value);
    STDMETHODIMP XyGetDouble   (unsigned field, double    *value);
    STDMETHODIMP XyGetString   (unsigned field, LPWSTR    *value, int *chars);
    STDMETHODIMP XyGetBin      (unsigned field, LPVOID    *value, int *size );
    STDMETHODIMP XyGetBin2     (unsigned field, void      *value, int size );

    STDMETHODIMP XySetBool     (unsigned field, bool      value);
    STDMETHODIMP XySetInt      (unsigned field, int       value);
    STDMETHODIMP XySetSize     (unsigned field, SIZE      value);
    STDMETHODIMP XySetRect     (unsigned field, RECT      value);
    STDMETHODIMP XySetUlonglong(unsigned field, ULONGLONG value);
    STDMETHODIMP XySetDouble   (unsigned field, double    value);
    STDMETHODIMP XySetString   (unsigned field, LPWSTR    value, int chars);
    STDMETHODIMP XySetBin      (unsigned field, LPVOID    value, int size );

	// IDirectVobSub

    STDMETHODIMP get_FileName(WCHAR* fn);
    STDMETHODIMP put_FileName(WCHAR* fn);
	STDMETHODIMP get_LanguageCount(int* nLangs);
	STDMETHODIMP get_LanguageName(int iLanguage, WCHAR** ppName);
	STDMETHODIMP get_SelectedLanguage(int* iSelected);
	STDMETHODIMP put_SelectedLanguage(int iSelected);
	STDMETHODIMP get_HideSubtitles(bool* fHideSubtitles);
    STDMETHODIMP put_HideSubtitles(bool fHideSubtitles);
    STDMETHODIMP get_PreBuffering(bool* fDoPreBuffering);
    STDMETHODIMP put_PreBuffering(bool fDoPreBuffering);

    
    STDMETHODIMP get_SubPictToBuffer(unsigned int* uSubPictToBuffer)
    {
        return E_NOTIMPL;
    }
    STDMETHODIMP put_SubPictToBuffer(unsigned int uSubPictToBuffer)
    {
        return E_NOTIMPL;
    }
    STDMETHODIMP get_AnimWhenBuffering(bool* fAnimWhenBuffering)
    {
        return E_NOTIMPL;
    }
    STDMETHODIMP put_AnimWhenBuffering(bool fAnimWhenBuffering)
    {
        return E_NOTIMPL;
    }
    STDMETHODIMP get_Placement(bool* fOverridePlacement, int* xperc, int* yperc);
    STDMETHODIMP put_Placement(bool fOverridePlacement, int xperc, int yperc);
    STDMETHODIMP get_VobSubSettings(bool* fBuffer, bool* fOnlyShowForcedSubs, bool* fPolygonize);
    STDMETHODIMP put_VobSubSettings(bool fBuffer, bool fOnlyShowForcedSubs, bool fPolygonize);
    STDMETHODIMP get_TextSettings(void* lf, int lflen, COLORREF* color, bool* fShadow, bool* fOutline, bool* fAdvancedRenderer);
    STDMETHODIMP put_TextSettings(void* lf, int lflen, COLORREF color, bool fShadow, bool fOutline, bool fAdvancedRenderer);
    STDMETHODIMP get_Flip(bool* fPicture, bool* fSubtitles);
    STDMETHODIMP put_Flip(bool fPicture, bool fSubtitles);
    STDMETHODIMP get_OSD(bool* fShowOSD);
    STDMETHODIMP put_OSD(bool fShowOSD);
	STDMETHODIMP get_SaveFullPath(bool* fSaveFullPath);
	STDMETHODIMP put_SaveFullPath(bool fSaveFullPath);
    STDMETHODIMP get_SubtitleTiming(int* delay, int* speedmul, int* speeddiv);
    STDMETHODIMP put_SubtitleTiming(int delay, int speedmul, int speeddiv);
    STDMETHODIMP get_MediaFPS(bool* fEnabled, double* fps);
    STDMETHODIMP put_MediaFPS(bool fEnabled, double fps);
	STDMETHODIMP get_ZoomRect(NORMALIZEDRECT* rect);
    STDMETHODIMP put_ZoomRect(NORMALIZEDRECT* rect);
	STDMETHODIMP get_ColorFormat(int* iPosition) {return E_NOTIMPL;}
    STDMETHODIMP put_ColorFormat(int iPosition) {return E_NOTIMPL;}

    STDMETHOD (get_CachesInfo)(CachesInfo* caches_info);
    STDMETHOD (get_XyFlyWeightInfo)(XyFlyWeightInfo* xy_fw_info);
    
	STDMETHODIMP UpdateRegistry();

	STDMETHODIMP HasConfigDialog(int iSelected);
	STDMETHODIMP ShowConfigDialog(int iSelected, HWND hWndParent);

	// settings for the rest are stored in the registry

	STDMETHODIMP IsSubtitleReloaderLocked(bool* fLocked);
    STDMETHODIMP LockSubtitleReloader(bool fLock);
	STDMETHODIMP get_SubtitleReloader(bool* fDisabled);
    STDMETHODIMP put_SubtitleReloader(bool fDisable);

	// the followings need a partial or full reloading of the filter

	STDMETHODIMP get_ExtendPicture(int* horizontal, int* vertical, int* resx2, int* resx2minw, int* resx2minh);
	STDMETHODIMP put_ExtendPicture(int horizontal, int vertical, int resx2, int resx2minw, int resx2minh);
	STDMETHODIMP get_LoadSettings(int* level, bool* fExternalLoad, bool* fWebLoad, bool* fEmbeddedLoad);
	STDMETHODIMP put_LoadSettings(int level, bool fExternalLoad, bool fWebLoad, bool fEmbeddedLoad);
        
	// IDirectVobSub2

	STDMETHODIMP AdviseSubClock(ISubClock* pSubClock);
	STDMETHODIMP_(bool) get_Forced();
	STDMETHODIMP put_Forced(bool fForced);
    STDMETHODIMP get_TextSettings(STSStyle* pDefStyle);
    STDMETHODIMP put_TextSettings(STSStyle* pDefStyle);
	STDMETHODIMP get_AspectRatioSettings(CSimpleTextSubtitle::EPARCompensationType* ePARCompensationType);
	STDMETHODIMP put_AspectRatioSettings(CSimpleTextSubtitle::EPARCompensationType* ePARCompensationType);

	// IFilterVersion
	
	STDMETHODIMP_(DWORD) GetFilterVersion();
};
