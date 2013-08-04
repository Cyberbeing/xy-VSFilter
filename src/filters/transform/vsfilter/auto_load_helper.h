

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

[uuid("0c2c6a4c-f03c-4f89-b9b8-8b5444cef472")]
interface IXySubFilterAutoLoaderGraphMutex : public IUnknown
{
};

[uuid("6b237877-902b-4c6c-92f6-e63169a5166c")]
class XySubFilterAutoLoader : public CBaseFilter, public IXySubFilterAutoLoaderGraphMutex
{
public:
    XySubFilterAutoLoader(LPUNKNOWN punk, HRESULT* phr, const GUID& clsid = __uuidof(XySubFilterAutoLoader));
    virtual ~XySubFilterAutoLoader();

    DECLARE_IUNKNOWN;
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);

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
