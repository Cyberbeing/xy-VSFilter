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

/* This is for graphedit */

[uuid("93A22E7A-5091-45ef-BA61-6DA26156A5D0")]
class CDirectVobSubFilter
	: public CBaseVideoFilter
	, public CDirectVobSub
	, public ISpecifyPropertyPages
	, public IAMStreamSelect
	, public CAMThread
{
    friend class CTextInputPin;

	CCritSec m_csQueueLock;
	CComPtr<ISimpleSubPicProvider> m_simple_provider;

	void InitSubPicQueue();
	SubPicDesc m_spd;

	bool AdjustFrameSize(CSize& s);

    HRESULT TryNotCopy( IMediaSample* pIn, const CMediaType& mt, const BITMAPINFOHEADER& bihIn );

    ColorConvTable::YuvMatrixType m_video_yuv_matrix_decided_by_sub;
    ColorConvTable::YuvRangeType m_video_yuv_range_decided_by_sub;

    void SetYuvMatrix();
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

    // IDirectVobSubXy
    STDMETHODIMP XySetBool     (int field, bool      value);
    STDMETHODIMP XySetInt      (int field, int       value);

    // IDirectVobSub
    STDMETHODIMP put_FileName(WCHAR* fn);
	STDMETHODIMP get_LanguageCount(int* nLangs);
	STDMETHODIMP get_LanguageName(int iLanguage, WCHAR** ppName);
	STDMETHODIMP put_SelectedLanguage(int iSelected);
    STDMETHODIMP put_HideSubtitles(bool fHideSubtitles);
	STDMETHODIMP put_PreBuffering(bool fDoPreBuffering);

    STDMETHODIMP put_Placement(bool fOverridePlacement, int xperc, int yperc);
    STDMETHODIMP put_VobSubSettings(bool fBuffer, bool fOnlyShowForcedSubs, bool fPolygonize);
    STDMETHODIMP put_TextSettings(void* lf, int lflen, COLORREF color, bool fShadow, bool fOutline, bool fAdvancedRenderer);
    STDMETHODIMP put_SubtitleTiming(int delay, int speedmul, int speeddiv);

    STDMETHODIMP get_CachesInfo(CachesInfo* caches_info);
    STDMETHODIMP get_XyFlyWeightInfo(XyFlyWeightInfo* xy_fw_info);

    STDMETHODIMP get_MediaFPS(bool* fEnabled, double* fps);
    STDMETHODIMP put_MediaFPS(bool fEnabled, double fps);
    STDMETHODIMP get_ZoomRect(NORMALIZEDRECT* rect);
    STDMETHODIMP put_ZoomRect(NORMALIZEDRECT* rect);
	STDMETHODIMP HasConfigDialog(int iSelected);
	STDMETHODIMP ShowConfigDialog(int iSelected, HWND hWndParent);

	// IDirectVobSub2
	STDMETHODIMP put_TextSettings(STSStyle* pDefStyle);
	STDMETHODIMP put_AspectRatioSettings(CSimpleTextSubtitle::EPARCompensationType* ePARCompensationType);

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

	bool Open();

	int FindPreferedLanguage(bool fHideToo = true);
	void UpdatePreferedLanguages(CString lang);

	CCritSec m_csSubLock;

	CInterfaceList<ISubStream> m_pSubStreams;
    CAtlList<bool> m_fIsSubStreamEmbeded;

	DWORD_PTR m_nSubtitleId;
	void UpdateSubtitle(bool fApplyDefStyle = true);
	void SetSubtitle(ISubStream* pSubStream, bool fApplyDefStyle = true);
	void InvalidateSubtitle(REFERENCE_TIME rtInvalidate = -1, DWORD_PTR nSubtitleId = -1);

	// the text input pin is using these
	void AddSubStream(ISubStream* pSubStream);
	void RemoveSubStream(ISubStream* pSubStream);
	void Post_EC_OLE_EVENT(CString str, DWORD_PTR nSubtitleId = -1);

private:
	class CFileReloaderData
	{
	public:
		ATL::CEvent ThreadStartedEvent, EndThreadEvent, RefreshEvent;
		CAtlList<CString> files;
		CAtlArray<CTime> mtime;
	} m_frd;

	void SetupFRD(CStringArray& paths, CAtlArray<HANDLE>& handles);
	DWORD ThreadProc();

private:
	HANDLE m_hSystrayThread;
	SystrayIconData m_tbid;

	VIDEOINFOHEADER2 m_CurrentVIH2;

	//xy TIMING
	long m_time_rasterization, m_time_alphablt;
	
	bool m_bExternalSubtitle = false;
	std::vector<ISubStream*> m_ExternalSubstreams;
};

/* The "auto-loading" version */

[uuid("9852A670-F845-491b-9BE6-EBD841B8A613")]
class CDirectVobSubFilter2 : public CDirectVobSubFilter
{
	bool IsAppBlackListed();
	bool ShouldWeAutoload(IFilterGraph* pGraph);
	void GetRidOfInternalScriptRenderer();

public:
    CDirectVobSubFilter2(LPUNKNOWN punk, HRESULT* phr, const GUID& clsid = __uuidof(CDirectVobSubFilter2));

	HRESULT CheckConnect(PIN_DIRECTION dir, IPin* pPin);
	STDMETHODIMP JoinFilterGraph(IFilterGraph* pGraph, LPCWSTR pName);
    HRESULT CheckInputType(const CMediaType* mtIn);
};

