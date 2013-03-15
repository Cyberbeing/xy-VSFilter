#pragma once

#include <atlsync.h>
#include "DirectVobSub.h"
#include "SubRenderIntf.h"
#include "SubRenderOptionsImpl.h"

class CDirectVobSubFilter;

[uuid("2dfcb782-ec20-4a7c-b530-4577adb33f21")]
class XySubFilter
    : public CBaseFilter
    , public CDirectVobSub
    , public ISpecifyPropertyPages
    , public IAMStreamSelect
    , public CAMThread
    , public ISubRenderProvider
{
public:
    friend class SubtitleInputPin2;

    XySubFilter(LPUNKNOWN punk, HRESULT* phr, const GUID& clsid = __uuidof(XySubFilter));
    virtual ~XySubFilter();

    DECLARE_IUNKNOWN;
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);

    //DECLARE_ISUBRENDEROPTIONS;
    STDMETHODIMP GetBool     (LPCSTR field, bool      *value);
    STDMETHODIMP GetInt      (LPCSTR field, int *value)                 { return E_FAIL; }
    STDMETHODIMP GetSize     (LPCSTR field, SIZE *value)                { return E_FAIL; }
    STDMETHODIMP GetRect     (LPCSTR field, RECT *value)                { return E_FAIL; }
    STDMETHODIMP GetUlonglong(LPCSTR field, ULONGLONG *value)           { return E_FAIL; }
    STDMETHODIMP GetDouble   (LPCSTR field, double *value)              { return E_FAIL; }
    STDMETHODIMP GetString   (LPCSTR field, LPWSTR *value, int *chars);
    STDMETHODIMP GetBin      (LPCSTR field, LPVOID *value, int *size )  { return E_FAIL; }

    STDMETHODIMP SetBool     (LPCSTR field, bool value)                 { return E_FAIL; }
    STDMETHODIMP SetInt      (LPCSTR field, int value)                  { return E_FAIL; }
    STDMETHODIMP SetSize     (LPCSTR field, SIZE value)                 { return E_FAIL; }
    STDMETHODIMP SetRect     (LPCSTR field, RECT value)                 { return E_FAIL; }
    STDMETHODIMP SetUlonglong(LPCSTR field, ULONGLONG value)            { return E_FAIL; }
    STDMETHODIMP SetDouble   (LPCSTR field, double value)               { return E_FAIL; }
    STDMETHODIMP SetString   (LPCSTR field, LPWSTR    value, int chars) { return E_FAIL; }
    STDMETHODIMP SetBin      (LPCSTR field, LPVOID    value, int size ) { return E_FAIL; }

    //CBaseFilter
    CBasePin* GetPin(int n);
    int GetPinCount();
    STDMETHODIMP JoinFilterGraph(IFilterGraph* pGraph, LPCWSTR pName);

    STDMETHODIMP QueryFilterInfo(FILTER_INFO* pInfo);

    STDMETHODIMP Pause();

    // XyOptionsImpl
    virtual HRESULT OnOptionChanged(unsigned field);

    // IXyOptions
    STDMETHODIMP XyGetInt      (unsigned field, int      *value);
    STDMETHODIMP XyGetString   (unsigned field, LPWSTR *value, int *chars);
    STDMETHODIMP XySetInt      (unsigned field, int       value);

    // IDirectVobSub
    STDMETHODIMP put_FileName(WCHAR* fn);
    STDMETHODIMP get_LanguageCount(int* nLangs);
    STDMETHODIMP get_LanguageName(int iLanguage, WCHAR** ppName);
    STDMETHODIMP put_SelectedLanguage(int iSelected);
    STDMETHODIMP put_HideSubtitles(bool fHideSubtitles);

    STDMETHODIMP put_Placement(bool fOverridePlacement, int xperc, int yperc);
    STDMETHODIMP put_VobSubSettings(bool fBuffer, bool fOnlyShowForcedSubs, bool fPolygonize);
    STDMETHODIMP put_TextSettings(void* lf, int lflen, COLORREF color, bool fShadow, bool fOutline, bool fAdvancedRenderer);
    STDMETHODIMP put_SubtitleTiming(int delay, int speedmul, int speeddiv);

    STDMETHODIMP get_CachesInfo(CachesInfo* caches_info);
    STDMETHODIMP get_XyFlyWeightInfo(XyFlyWeightInfo* xy_fw_info);

    // IDirectVobSub2
    STDMETHODIMP put_TextSettings(STSStyle* pDefStyle);
    STDMETHODIMP put_AspectRatioSettings(CSimpleTextSubtitle::EPARCompensationType* ePARCompensationType);

    // ISpecifyPropertyPages
    STDMETHODIMP GetPages(CAUUID* pPages);

    // IAMStreamSelect
    STDMETHODIMP Count(DWORD* pcStreams); 
    STDMETHODIMP Enable(long lIndex, DWORD dwFlags); 
    STDMETHODIMP Info(long lIndex, AM_MEDIA_TYPE** ppmt, DWORD* pdwFlags, LCID* plcid, DWORD* pdwGroup, WCHAR** ppszName, IUnknown** ppObject, IUnknown** ppUnk);

    //ISubRenderProvider
    STDMETHODIMP RequestFrame(REFERENCE_TIME start, REFERENCE_TIME stop, LPVOID context);
    STDMETHODIMP Disconnect(void);
private:
    void SetYuvMatrix();
    bool Open();

    void UpdateSubtitle(bool fApplyDefStyle = true);
    void SetSubtitle(ISubStream* pSubStream, bool fApplyDefStyle = true);
    void InvalidateSubtitle(REFERENCE_TIME rtInvalidate = -1, DWORD_PTR nSubtitleId = -1);

    void InitSubPicQueue();

    HRESULT UpdateParamFromConsumer();

    int FindPreferedLanguage(bool fHideToo = true);
    void UpdatePreferedLanguages(CString l);

    // the text input pin is using these
    void AddSubStream(ISubStream* pSubStream);
    void RemoveSubStream(ISubStream* pSubStream);

    HRESULT GetIsEmbeddedSubStream(int iSelected, bool *fIsEmbedded);

    bool ShouldWeAutoload(IFilterGraph* pGraph);

    HRESULT StartStreaming();

    HRESULT FindAndConnectConsumer(IFilterGraph* pGraph);
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
    ColorConvTable::YuvMatrixType m_video_yuv_matrix_decided_by_sub;
    ColorConvTable::YuvRangeType m_video_yuv_range_decided_by_sub;

    DWORD_PTR m_nSubtitleId;
    CInterfaceList<ISubStream> m_pSubStreams;
    CAtlList<bool> m_fIsSubStreamEmbeded;

    bool m_consumer_options_read;

    // critical section protecting filter state.
    CCritSec m_csFilter;

    // critical section stopping state changes (ie Stop) while we're
    // processing a sample.
    //
    // This critical section is held when processing
    // events that occur on the receive thread - Receive() and EndOfStream().
    //
    // If you want to hold both m_csReceive and m_csFilter then grab
    // m_csFilter FIRST - like CTransformFilter::Stop() does.

    CCritSec m_csReceive;

    CCritSec m_csSubLock;

    CComPtr<IXySubRenderProvider> m_sub_provider;

    CAtlArray<SubtitleInputPin2*> m_pSubtitleInputPin;

    // don't set the "hide subtitles" stream until we are finished with loading
    bool m_fLoading;

    bool m_not_first_pause;

    SystrayIconData m_tbid;
    HANDLE m_hSystrayThread;

    ISubRenderConsumer *m_consumer;
    ULONGLONG m_context_id;
};
