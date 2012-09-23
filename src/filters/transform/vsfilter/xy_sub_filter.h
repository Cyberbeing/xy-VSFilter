#pragma once

#include <atlsync.h>
#include "DirectVobSub.h"

class CDirectVobSubFilter;

[uuid("2dfcb782-ec20-4a7c-b530-4577adb33f21")]
class XySubFilter
    : public CBaseFilter
    , public CDirectVobSub
    , public ISpecifyPropertyPages
    , public IAMStreamSelect
    , public CAMThread
{
public:
    friend class CDirectVobSubFilter;
    friend class CDirectVobSubFilter2;
    friend class CTextInputPin;

    XySubFilter(CDirectVobSubFilter *p_dvs, LPUNKNOWN punk, HRESULT* phr, const GUID& clsid = __uuidof(XySubFilter));
	virtual ~XySubFilter();

    DECLARE_IUNKNOWN;
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);

    //CBaseFilter
    CBasePin* GetPin(int n);
    int GetPinCount();
    STDMETHODIMP JoinFilterGraph(IFilterGraph* pGraph, LPCWSTR pName);

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

private:
    void SetYuvMatrix();
    bool Open();

    void UpdateSubtitle(bool fApplyDefStyle = true);
    void SetSubtitle(ISubStream* pSubStream, bool fApplyDefStyle = true);
    void InvalidateSubtitle(REFERENCE_TIME rtInvalidate = -1, DWORD_PTR nSubtitleId = -1);

    int FindPreferedLanguage(bool fHideToo = true);
    void UpdatePreferedLanguages(CString l);

    // the text input pin is using these
    void AddSubStream(ISubStream* pSubStream);
    void RemoveSubStream(ISubStream* pSubStream);

    HRESULT GetIsEmbeddedSubStream(int iSelected, bool *fIsEmbedded);

    bool ShouldWeAutoload(IFilterGraph* pGraph);
private:
    class CFileReloaderData
    {
    public:
        ATL::CEvent EndThreadEvent, RefreshEvent;
        CAtlList<CString> files;
        CAtlArray<CTime> mtime;
    } m_frd;

    void SetupFRD(CStringArray& paths, CAtlArray<HANDLE>& handles);
    DWORD ThreadProc();
private:
    CSimpleTextSubtitle::YCbCrMatrix m_script_selected_yuv;
    CSimpleTextSubtitle::YCbCrRange m_script_selected_range;

    DWORD_PTR m_nSubtitleId;
    CInterfaceList<ISubStream> m_pSubStreams;
    CAtlList<bool> m_fIsSubStreamEmbeded;

    CCritSec m_csFilter;
    CCritSec m_csSubLock;

    CComPtr<ISimpleSubPicProvider> m_simple_provider;

    CAtlArray<CTextInputPin*> m_pTextInput;

    CDirectVobSubFilter *m_dvs;
};
