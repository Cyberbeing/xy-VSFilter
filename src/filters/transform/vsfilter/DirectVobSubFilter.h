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

#include <atlsync.h>
#include "DirectVobSub.h"
#include "../BaseVideoFilter/BaseVideoFilter.h"
#include "../../../subtitles/VobSubFile.h"
#include "../../../subtitles/RTS.h"
#include "../../../subtitles/SSF.h"
#include "../../../subpic/ISimpleSubPic.h"

typedef struct
{
	HWND hSystrayWnd;
	IFilterGraph* graph;
	IDirectVobSub* dvs;
	bool fRunOnce, fShowIcon;
} SystrayIconData;

class XySubFilter;

/* This is for graphedit */

[uuid("93A22E7A-5091-45ef-BA61-6DA26156A5D0")]
class CDirectVobSubFilter
    : public CBaseVideoFilter
    , public IDirectVobSub2 
    , public IDirectVobSubXy
    , public IFilterVersion
	, public ISpecifyPropertyPages
	, public IAMStreamSelect
{
    friend class CTextInputPin;
    friend class XySubFilter;

	CCritSec m_csQueueLock;
	CComPtr<ISimpleSubPicProvider> m_simple_provider;

	void InitSubPicQueue();
	SubPicDesc m_spd;

	bool AdjustFrameSize(CSize& s);

    HRESULT TryNotCopy( IMediaSample* pIn, const CMediaType& mt, const BITMAPINFOHEADER& bihIn );
protected:
	void GetOutputSize(int& w, int& h, int& arx, int& ary);
	HRESULT Transform(IMediaSample* pIn);    
    HRESULT GetIsEmbeddedSubStream(int iSelected, bool *fIsEmbedded);
public:
    CDirectVobSubFilter(LPUNKNOWN punk, HRESULT* phr, const GUID& clsid = __uuidof(CDirectVobSubFilter));
	virtual ~CDirectVobSubFilter();

    DECLARE_IUNKNOWN;
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);

    // CBaseFilter

	CBasePin* GetPin(int n);
	int GetPinCount();

	STDMETHODIMP JoinFilterGraph(IFilterGraph* pGraph, LPCWSTR pName);
	STDMETHODIMP QueryFilterInfo(FILTER_INFO* pInfo);

    // CTransformFilter
	HRESULT SetMediaType(PIN_DIRECTION dir, const CMediaType* pMediaType),
			CheckConnect(PIN_DIRECTION dir, IPin* pPin),
			CompleteConnect(PIN_DIRECTION dir, IPin* pReceivePin),
			BreakConnect(PIN_DIRECTION dir),
			StartStreaming(), 
			StopStreaming(),
			NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate);

    CAtlArray<CTextInputPin*> m_pTextInput;

    //CBaseVideoFilter
    void GetInputColorspaces(ColorSpaceId *preferredOrder, UINT *count);
    void GetOutputColorspaces(ColorSpaceId *preferredOrder, UINT *count);
    
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
    STDMETHODIMP get_SubPictToBuffer(unsigned int* uSubPictToBuffer);
    STDMETHODIMP put_SubPictToBuffer(unsigned int uSubPictToBuffer);
    STDMETHODIMP get_AnimWhenBuffering(bool* fAnimWhenBuffering);
    STDMETHODIMP put_AnimWhenBuffering(bool fAnimWhenBuffering);
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

    STDMETHODIMP UpdateRegistry();

    STDMETHODIMP get_ColorFormat(int* iPosition);
    STDMETHODIMP put_ColorFormat(int iPosition);
    
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
    
    // IDirectVobSubXy
    STDMETHODIMP XyGetBool     (int field, bool      *value);
    STDMETHODIMP XyGetInt      (int field, int       *value);
    STDMETHODIMP XyGetSize     (int field, SIZE      *value);
    STDMETHODIMP XyGetRect     (int field, RECT      *value);
    STDMETHODIMP XyGetUlonglong(int field, ULONGLONG *value);
    STDMETHODIMP XyGetDouble   (int field, double    *value);
    STDMETHODIMP XyGetString   (int field, LPWSTR    *value, int *chars);
    STDMETHODIMP XyGetBin      (int field, LPVOID    *value, int *size );

    STDMETHODIMP XySetBool     (int field, bool      value);
    STDMETHODIMP XySetInt      (int field, int       value);
    STDMETHODIMP XySetSize     (int field, SIZE      value);
    STDMETHODIMP XySetRect     (int field, RECT      value);
    STDMETHODIMP XySetUlonglong(int field, ULONGLONG value);
    STDMETHODIMP XySetDouble   (int field, double    value);
    STDMETHODIMP XySetString   (int field, LPWSTR    value, int chars);
    STDMETHODIMP XySetBin      (int field, LPVOID    value, int size );

    // IFilterVersion
    STDMETHODIMP_(DWORD) GetFilterVersion();

    // ISpecifyPropertyPages
    STDMETHODIMP GetPages(CAUUID* pPages);

	// IAMStreamSelect
	STDMETHODIMP Count(DWORD* pcStreams); 
	STDMETHODIMP Enable(long lIndex, DWORD dwFlags); 
	STDMETHODIMP Info(long lIndex, AM_MEDIA_TYPE** ppmt, DWORD* pdwFlags, LCID* plcid, DWORD* pdwGroup, WCHAR** ppszName, IUnknown** ppObject, IUnknown** ppUnk);  

    // CPersistStream
	STDMETHODIMP GetClassID(CLSID* pClsid);

protected:
    //OSD
    void ZeroObj4OSD();
    void DeleteObj4OSD();
    void InitObj4OSD();
    void PrintMessages(BYTE* pOut);

    HDC m_hdc;
    HBITMAP m_hbm;
    HFONT m_hfont;
    
protected:
	HRESULT ChangeMediaType(int iPosition);

    /* ResX2 */
	CAutoVectorPtr<BYTE> m_pTempPicBuff;
	HRESULT Copy(BYTE* pSub, BYTE* pIn, CSize sub, CSize in, int bpp, const GUID& subtype, DWORD black);
	// segment start time, absolute time
	CRefTime m_tPrev;
	REFERENCE_TIME CalcCurrentTime();

	double m_fps;

	// 3.x- versions of microsoft's mpeg4 codec output flipped image
	bool m_fMSMpeg4Fix;

	// don't set the "hide subtitles" stream until we are finished with loading
	bool m_fLoading;

	CCritSec m_csSubLock;

	CInterfaceList<ISubStream> m_pSubStreams;
    CAtlList<bool> m_fIsSubStreamEmbeded;

private:
	HANDLE m_hSystrayThread;
	SystrayIconData m_tbid;

	VIDEOINFOHEADER2 m_CurrentVIH2;

	//xy TIMING
	long m_time_rasterization, m_time_alphablt;

protected:
    XySubFilter* m_xy_sub_filter;
};

/* The "auto-loading" version */

[uuid("9852A670-F845-491b-9BE6-EBD841B8A613")]
class CDirectVobSubFilter2 : public CDirectVobSubFilter
{
	bool ShouldWeAutoload(IFilterGraph* pGraph);
	void GetRidOfInternalScriptRenderer();

public:
    CDirectVobSubFilter2(LPUNKNOWN punk, HRESULT* phr, const GUID& clsid = __uuidof(CDirectVobSubFilter2));

	HRESULT CheckConnect(PIN_DIRECTION dir, IPin* pPin);
	STDMETHODIMP JoinFilterGraph(IFilterGraph* pGraph, LPCWSTR pName);
    HRESULT CheckInputType(const CMediaType* mtIn);
};

