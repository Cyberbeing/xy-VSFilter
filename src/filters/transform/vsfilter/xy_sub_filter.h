#pragma once

#include <atlsync.h>

class CDirectVobSubFilter;

[uuid("2dfcb782-ec20-4a7c-b530-4577adb33f21")]
class XySubFilter
    : public CUnknown
    , public ISpecifyPropertyPages
    , public IAMStreamSelect
    , public CAMThread
{
public:
    friend class CDirectVobSubFilter;

    XySubFilter(CDirectVobSubFilter *p_dvs, LPUNKNOWN punk);
	virtual ~XySubFilter();

    DECLARE_IUNKNOWN;
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);

    // ISpecifyPropertyPages
    STDMETHODIMP GetPages(CAUUID* pPages);

	// IAMStreamSelect
	STDMETHODIMP Count(DWORD* pcStreams); 
	STDMETHODIMP Enable(long lIndex, DWORD dwFlags); 
	STDMETHODIMP Info(long lIndex, AM_MEDIA_TYPE** ppmt, DWORD* pdwFlags, LCID* plcid, DWORD* pdwGroup, WCHAR** ppszName, IUnknown** ppObject, IUnknown** ppUnk);

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
    CDirectVobSubFilter *m_dvs;
};
