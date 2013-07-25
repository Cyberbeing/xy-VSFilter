#pragma once

#include <atlsync.h>
#include "DirectVobSub.h"
#include "SubRenderIntf.h"
#include "SubRenderOptionsImpl.h"

class CDirectVobSubFilter;

// Expose this interface if you do NOT want to work with XySubFilter in a graph
[uuid("8871ac83-89c8-44e9-913f-a9a2a322440a")]
interface IXySubFilterGraphMutex : public IUnknown
{

};

[uuid("2dfcb782-ec20-4a7c-b530-4577adb33f21")]
class XySubFilter
    : public CBaseFilter
    , public CDVS4XySubFilter
    , public ISpecifyPropertyPages
    , public IAMStreamSelect
    , public CAMThread
    , public SubRenderOptionsImpl
    , public ISubRenderProvider
    , public IXySubFilterGraphMutex
{
public:
    friend class SubtitleInputPin2;

    XySubFilter(LPUNKNOWN punk, HRESULT* phr, const GUID& clsid = __uuidof(XySubFilter));
    virtual ~XySubFilter();

    DECLARE_IUNKNOWN;
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);

    DECLARE_ISUBRENDEROPTIONS;

    //CBaseFilter
    CBasePin* GetPin(int n);
    int GetPinCount();
    STDMETHODIMP JoinFilterGraph(IFilterGraph* pGraph, LPCWSTR pName);

    STDMETHODIMP QueryFilterInfo(FILTER_INFO* pInfo);

    STDMETHODIMP Pause();

    // XyOptionsImpl
    virtual HRESULT OnOptionChanged(unsigned field);
    virtual HRESULT DoGetField(unsigned field, void *value);

    // IXyOptions
    STDMETHODIMP XyGetString   (unsigned field, LPWSTR   *value, int *chars);
    STDMETHODIMP XySetBool     (unsigned field, bool      value);

    // DirectVobSubImpl
    HRESULT GetCurStyles(SubStyle sub_style[], int count);
    HRESULT SetCurStyles(const SubStyle sub_style[], int count);

    // IDirectVobSub
    STDMETHODIMP get_LanguageName(int iLanguage, WCHAR** ppName);

    STDMETHODIMP get_CachesInfo(CachesInfo* caches_info);
    STDMETHODIMP get_XyFlyWeightInfo(XyFlyWeightInfo* xy_fw_info);

    STDMETHODIMP get_MediaFPS(bool* fEnabled, double* fps);
    STDMETHODIMP put_MediaFPS(bool fEnabled, double fps);

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
    void SetRgbOutputLevel();
    bool Open();

    void UpdateSubtitle(bool fApplyDefStyle = true);
    void SetSubtitle(ISubStream* pSubStream, bool fApplyDefStyle = true);
    void InvalidateSubtitle(REFERENCE_TIME rtInvalidate = -1, DWORD_PTR nSubtitleId = -1);

    HRESULT UpdateParamFromConsumer(bool getNameAndVersion=false);

    int FindPreferedLanguage(bool fHideToo = true);
    void UpdatePreferedLanguages(CString l);

    // the text input pin is using these
    void AddSubStream(ISubStream* pSubStream);
    void RemoveSubStream(ISubStream* pSubStream);
    HRESULT CheckInputType(const CMediaType* pmt);

    HRESULT GetIsEmbeddedSubStream(int iSelected, bool *fIsEmbedded);

    bool LoadExternalSubtitle(IFilterGraph* pGraph);
    bool ShouldWeAutoLoad(IFilterGraph* pGraph);

    HRESULT StartStreaming();

    HRESULT FindAndConnectConsumer(IFilterGraph* pGraph);

    void UpdateLanguageCount();

    CStringW DumpProviderInfo();
    CStringW DumpConsumerInfo();
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
    ColorConvTable::YuvMatrixType m_video_yuv_matrix_decided_by_sub;
    ColorConvTable::YuvRangeType m_video_yuv_range_decided_by_sub;

    ISubStream *m_curSubStream;
    CInterfaceList<ISubStream> m_pSubStreams;
    CAtlList<bool> m_fIsSubStreamEmbeded;

    // critical section protecting all
    CCritSec m_csFilter;

    CCritSec m_csProviderFields;// critical section protecting fields to be read by consumer
                                // it is needed because otherwise consumer has to hold m_csSubLock which may be holded 
                                // by renderring thread for a long time

    CCritSec m_csConsumer;      // critical section protecting m_consumer

    CComPtr<IXySubRenderProvider> m_sub_provider;

    CAtlArray<SubtitleInputPin2*> m_pSubtitleInputPin;

    // don't set the "hide subtitles" stream until we are finished with loading
    bool m_fLoading;

    bool m_not_first_pause;

    SystrayIconData m_tbid;
    HANDLE m_hSystrayThread;

    CComPtr<ISubRenderConsumer> m_consumer;
    ULONGLONG m_context_id;

    REFERENCE_TIME m_last_requested;

    bool m_workaround_mpc_hc;//enable workaround for MPC-HC to prevent be removed from the graph

    bool m_disconnect_entered;

    bool m_be_auto_loaded;

    CStringW m_filter_info_string;

    friend class XySubFilterAutoLoader;
};

class XyAutoLoaderDummyInputPin: public CBaseInputPin
{
public:
    XyAutoLoaderDummyInputPin(
        CBaseFilter *pFilter,
        CCritSec *pLock,
        HRESULT *phr,
        LPCWSTR pName);

    HRESULT CheckMediaType(const CMediaType *);
};

[uuid("6b237877-902b-4c6c-92f6-e63169a5166c")]
class XySubFilterAutoLoader : public CBaseFilter
{
public:
    XySubFilterAutoLoader(LPUNKNOWN punk, HRESULT* phr, const GUID& clsid = __uuidof(XySubFilterAutoLoader));
    virtual ~XySubFilterAutoLoader();

    DECLARE_IUNKNOWN;

    //CBaseFilter
    CBasePin* GetPin(int n);
    int GetPinCount();
    STDMETHODIMP JoinFilterGraph(IFilterGraph* pGraph, LPCWSTR pName);

    STDMETHODIMP QueryFilterInfo(FILTER_INFO* pInfo);
public:
    static HRESULT GetMerit( const GUID& clsid, DWORD *merit );
protected:
    CCritSec m_pLock;
    XyAutoLoaderDummyInputPin * m_pin;
};
