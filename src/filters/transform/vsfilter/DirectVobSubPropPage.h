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

#include <afxcmn.h>
#include "IDirectVobSub.h"
#include "IDirectVobSubXy.h"

class CDVSBasePPage : public CBasePropertyPage
{
public:
	// we have to override these to use external, resource-only dlls
	STDMETHODIMP GetPageInfo(LPPROPPAGEINFO pPageInfo);
	STDMETHODIMP Activate(HWND hwndParent, LPCRECT pRect, BOOL fModal);

protected:
	CComQIPtr<IDirectVobSub2> m_pDirectVobSub;
    CComQIPtr<IXyOptions> m_pDirectVobSubXy;

	virtual bool OnMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) {return(false);}
	virtual void UpdateObjectData(bool fSave) {}
	virtual void UpdateControlData(bool fSave) {}

protected:
    CDVSBasePPage(TCHAR* pName, LPUNKNOWN lpunk, int DialogId, int TitleId);

	bool m_fDisableInstantUpdate;

private:
    BOOL m_bIsInitialized;

    HRESULT OnConnect(IUnknown* pUnknown), OnDisconnect(), OnActivate(), OnDeactivate(), OnApplyChanges();
	INT_PTR OnReceiveMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
	bool m_fAttached;
	void AttachControls(), DetachControls();

	CMap<UINT, UINT&, CWnd*, CWnd*> m_controls;

protected:
	void BindControl(UINT id, CWnd& control);
};

[uuid("60765CF5-01C2-4ee7-A44B-C791CF25FEA0")]
class CDVSMainPPage : public CDVSBasePPage
{
	void FreeLangs(), AllocLangs(int nLangs);

	WCHAR m_fn[MAX_PATH];
	int m_iSelectedLanguage, m_nLangs;
	WCHAR** m_ppLangs;
	bool m_fOverridePlacement;
    bool m_fHideTrayIcon;
	int	m_PlacementXperc, m_PlacementYperc;
	STSStyle m_defStyle;
	bool m_fOnlyShowForcedVobSubs;
	CSimpleTextSubtitle::EPARCompensationType m_ePARCompensationType;

	CEdit m_fnedit;
	CComboBox m_langs;
	CButton m_oplacement;
	CSpinButtonCtrl m_subposx, m_subposy;
	CButton m_styles, m_forcedsubs;
	CButton m_AutoPARCompensation;
	CComboBox m_PARCombo;
    CButton m_hide_tray_icon;
protected:
    virtual bool OnMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
	virtual void UpdateControlData(bool fSave);
	virtual void UpdateObjectData(bool fSave);

public:
    CDVSMainPPage(LPUNKNOWN lpunk, HRESULT* phr);
	virtual ~CDVSMainPPage();
};

[uuid("0180E49C-13BF-46db-9AFD-9F52292E1C22")]
class CDVSGeneralPPage : public CDVSBasePPage
{
	int m_HorExt, m_VerExt, m_ResX2, m_ResX2minw, m_ResX2minh;
	int m_LoadLevel;
	bool m_fExternalLoad, m_fWebLoad, m_fEmbeddedLoad;

	CComboBox m_verext;
	CButton m_mod32fix;
	CComboBox m_resx2;
	CSpinButtonCtrl m_resx2w, m_resx2h;
	CComboBox m_load;
	CButton m_extload, m_webload, m_embload;

protected:
    virtual bool OnMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
	virtual void UpdateControlData(bool fSave);
	virtual void UpdateObjectData(bool fSave);

public:
    CDVSGeneralPPage(LPUNKNOWN lpunk, HRESULT* phr, TCHAR *pName = NAME("DirectVobSub Property Page (general settings)"));
};

[uuid("A8B25C0E-0894-4531-B668-AB1599FAF7F6")]
class CDVSMiscPPage : public CDVSBasePPage
{
	bool m_fFlipPicture, m_fFlipSubtitles, m_fHideSubtitles, m_fOSD, m_fDoPreBuffering, m_fReloaderDisabled, m_fSaveFullPath;

    int m_colorSpace, m_yuvRange;

	CButton m_flippic, m_flipsub, m_hidesub, m_showosd, m_prebuff, m_autoreload, m_savefullpath, m_instupd;
    CComboBox m_colorSpaceDropList, m_yuvRangeDropList;

protected:
    virtual bool OnMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
	virtual void UpdateControlData(bool fSave);
	virtual void UpdateObjectData(bool fSave);

public:
    CDVSMiscPPage(LPUNKNOWN lpunk, HRESULT* phr, TCHAR *pName = NAME("DirectVobSub Property Page (misc settings)"));
};

[uuid("ACE4747B-35BD-4e97-9DD7-1D4245B0695C")]
class CDVSTimingPPage : public CDVSBasePPage
{
	int m_SubtitleSpeedMul, m_SubtitleSpeedDiv, m_SubtitleDelay;
	bool m_fMediaFPSEnabled;
	double m_MediaFPS;

	CSpinButtonCtrl m_subdelay, m_subspeedmul, m_subspeeddiv;    

protected:
	virtual void UpdateControlData(bool fSave);
	virtual void UpdateObjectData(bool fSave);

public:
    CDVSTimingPPage(LPUNKNOWN lpunk, HRESULT* phr, TCHAR *pName = NAME("DirectVobSub Timing Property Page"));
};

[uuid("69CE757B-E8C0-4B0A-9EA0-CEA284096F98")]
class CDVSMorePPage : public CDVSBasePPage
{
    int m_overlay_cache_max_item_num, m_overlay_no_blur_cache_max_item_num, m_path_cache_max_item_num, 
        m_scan_line_data_cache_max_item_num, m_subpixel_pos_level;
    int m_layout_size_opt;
    SIZE m_layout_size;

    CSpinButtonCtrl m_path_cache, m_scanline_cache, m_overlay_no_blur_cache, m_overlay_cache;

    CSpinButtonCtrl m_layout_size_x, m_layout_size_y;
    CComboBox m_combo_subpixel_pos, m_combo_layout_size_opt;
protected:
    virtual bool OnMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
    virtual void UpdateControlData(bool fSave);
    virtual void UpdateObjectData(bool fSave);

public:
    CDVSMorePPage(LPUNKNOWN lpunk, HRESULT* phr);
};

[uuid("F544E0F5-CA3C-47ea-A64D-35FCF1602396")]
class CDVSAboutPPage : public CDVSBasePPage
{
public:
    CDVSAboutPPage(LPUNKNOWN lpunk, HRESULT* phr, TCHAR* pName=NAME("About Property Page"));
    
    bool OnMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

[uuid("525F116F-04AD-40a2-AE2F-A0C4E1AFEF98")]
class CDVSZoomPPage : public CDVSBasePPage
{
	NORMALIZEDRECT m_rect;

	CSpinButtonCtrl m_posx, m_posy, m_scalex, m_scaley;

protected:
    virtual bool OnMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
	virtual void UpdateControlData(bool fSave);
	virtual void UpdateObjectData(bool fSave);

public:
    CDVSZoomPPage(LPUNKNOWN lpunk, HRESULT* phr);
};

[uuid("C2D6D98F-09CA-4524-AF64-1049B5665C9C")]
class CDVSColorPPage : public CDVSBasePPage
{
    CListCtrl m_outputFmtList, m_inputFmtList;
    CButton m_followUpstreamPreferredOrder, m_btnColorUp, m_btnColorDown;
    
    static const int MAX_COLOR_SPACE = 256;

    typedef DirectVobSubXyOptions::ColorSpaceOpt ColorSpaceOpt;
    ColorSpaceOpt *m_outputColorSpace;
    int m_outputColorSpaceCount;

    ColorSpaceOpt *m_inputColorSpace;
    int m_inputColorSpaceCount;

    bool m_fFollowUpstream;
protected:
    virtual bool OnMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
	virtual void UpdateControlData(bool fSave);
	virtual void UpdateObjectData(bool fSave);

public:
    CDVSColorPPage(LPUNKNOWN lpunk, HRESULT* phr, TCHAR *pName = NAME("DirectVobSub Property Page (color settings)"));
    ~CDVSColorPPage();
};

[uuid("CE77C59C-CFD2-429f-868C-8B04D23F94CA")]
class CDVSPathsPPage : public CDVSBasePPage
{
	CStringArray m_paths;

	CListBox m_pathlist;
	CEdit m_path;
	CButton m_browse, m_remove, m_add;

protected:
    virtual bool OnMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
	virtual void UpdateControlData(bool fSave);
	virtual void UpdateObjectData(bool fSave);

public:
    CDVSPathsPPage(LPUNKNOWN lpunk, HRESULT* phr, TCHAR* pName = NAME("DirectVobSub Paths Property Page"));
};

//
// XySubFilter
//
[uuid("4514EFD4-E09B-4995-9668-143F12994FE7")]
class CXySubFilterMainPPage : public CDVSBasePPage
{
    void FreeLangs(), AllocLangs(int nLangs);

    WCHAR m_fn[MAX_PATH];
    int m_iSelectedLanguage, m_nLangs;
    WCHAR** m_ppLangs;
    bool m_fOverridePlacement;
    bool m_fHideTrayIcon;
    int m_PlacementXperc, m_PlacementYperc;
    STSStyle m_defStyle;
    bool m_fForceDefaultStyle;
    bool m_fOnlyShowForcedVobSubs;
    CSimpleTextSubtitle::EPARCompensationType m_ePARCompensationType;
    int m_LoadLevel;
    bool m_fExternalLoad, m_fWebLoad, m_fEmbeddedLoad;

    CEdit m_fnedit;
    CComboBox m_langs;
    CButton m_oplacement;
    CSpinButtonCtrl m_subposx, m_subposy;
    CButton m_styles, m_force_default_style, m_forcedsubs;
    CButton m_hide_tray_icon;
    CComboBox m_load;
    CButton m_extload, m_webload, m_embload;
protected:
    virtual bool OnMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
    virtual void UpdateControlData(bool fSave);
    virtual void UpdateObjectData(bool fSave);

public:
    CXySubFilterMainPPage(LPUNKNOWN lpunk, HRESULT* phr);
    virtual ~CXySubFilterMainPPage();
};

[uuid("E7946C91-1083-4F0E-AC45-5CF6BE7DB4C7")]
class CXySubFilterMorePPage : public CDVSBasePPage
{
    int m_overlay_cache_max_item_num, m_overlay_no_blur_cache_max_item_num, m_path_cache_max_item_num, 
        m_scan_line_data_cache_max_item_num, m_subpixel_pos_level;
    int m_layout_size_opt;
    int m_yuv_matrix, m_yuv_range, m_rgb_level;
    SIZE m_layout_size;

    bool m_fHideSubtitles, m_fAllowMoving, m_fReloaderDisabled;
    bool m_render_to_original_video_size;

    int  m_cache_size, m_auto_cache_size;

    CButton m_hidesub, m_allowmoving, m_autoreload, m_instupd;

    CSpinButtonCtrl m_path_cache, m_scanline_cache, m_overlay_no_blur_cache, m_overlay_cache;

    CSpinButtonCtrl m_layout_size_x, m_layout_size_y;
    CComboBox m_combo_subpixel_pos, m_combo_layout_size_opt;
    CButton m_checkbox_render_to_original_video_size;

    CComboBox m_combo_yuv_matrix, m_combo_yuv_range, m_combo_rgb_level;
    CEdit m_edit_cache_size;
protected:
    virtual bool OnMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
    virtual void UpdateControlData(bool fSave);
    virtual void UpdateObjectData(bool fSave);

public:
    CXySubFilterMorePPage(LPUNKNOWN lpunk, HRESULT* phr);
};

[uuid("1438A8B4-E8AF-4A81-BCA8-970751A9EE82")]
class CXySubFilterTimingPPage : public CDVSTimingPPage
{
public:
    CXySubFilterTimingPPage(LPUNKNOWN lpunk, HRESULT* phr
        , TCHAR *pName = NAME("XySubFilter Property Page (timing)"))
        : CDVSTimingPPage(lpunk, phr, pName) {}
};

[uuid("26DD9A01-7F95-40DE-B53D-4B4CE4023280")]
class CXySubFilterPathsPPage : public CDVSPathsPPage
{
public:
    CXySubFilterPathsPPage(LPUNKNOWN lpunk, HRESULT* phr
        , TCHAR* pName = NAME("XySubFilter Property Page (path)"))
        : CDVSPathsPPage(lpunk, phr, pName) {}
};

[uuid("D0DE7ADC-7DC6-43EC-912D-ABD44D7453FB")]
class CXySubFilterAboutPPage : public CDVSAboutPPage
{
public:
    CXySubFilterAboutPPage(LPUNKNOWN lpunk, HRESULT* phr, TCHAR* pName=NAME("XySubFilter Property Page (about)"))
        : CDVSAboutPPage(lpunk, phr, pName) {}
};

//
// XySubFilterConsumer
//
[uuid("b7f66b77-10b9-48e4-be40-2e6e4d00292e")]
class CXySubFilterConsumerGeneralPPage : public CDVSGeneralPPage
{
public:
    CXySubFilterConsumerGeneralPPage(LPUNKNOWN lpunk, HRESULT* phr
        , TCHAR *pName = NAME("XySubFilterConsumer Property Page (General)"))
        : CDVSGeneralPPage(lpunk, phr, pName) {}
};

[uuid("f2609280-2d03-4251-a1e4-65b1aff9dafe")]
class CXySubFilterConsumerMiscPPage : public CDVSMiscPPage
{
public:
    CXySubFilterConsumerMiscPPage(LPUNKNOWN lpunk, HRESULT* phr
        , TCHAR *pName = NAME("XySubFilterConsumer Property Page (Misc)"))
        : CDVSMiscPPage(lpunk, phr, pName) {}
};

[uuid("2e714223-34bc-430d-b52a-5bd23ddbf049")]
class CXySubFilterConsumerAboutPPage : public CDVSAboutPPage
{
public:
    CXySubFilterConsumerAboutPPage(LPUNKNOWN lpunk, HRESULT* phr, TCHAR* pName=NAME("XySubFilterConsumer Property Page (about)"))
        : CDVSAboutPPage(lpunk, phr, pName) {}
};

[uuid("c371c901-97eb-4bb4-b742-d53a0eaa7b3c")]
class CXySubFilterConsumerColorPPage : public CDVSColorPPage
{
public:
    CXySubFilterConsumerColorPPage(LPUNKNOWN lpunk, HRESULT* phr, TCHAR* pName=NAME("XySubFilterConsumer Property Page (color)"))
        : CDVSColorPPage(lpunk, phr, pName) {}
};
