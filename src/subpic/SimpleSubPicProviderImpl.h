#pragma once

#include "ISimpleSubPic.h"
#include "SubPicQueueImpl.h"

//////////////////////////////////////////////////////////////////////////
//
// SimpleSubPicProvider
// 

class SimpleSubPicProvider: public CUnknown, public ISimpleSubPicProvider
{
public:
    SimpleSubPicProvider(ISubPicExAllocator* pAllocator, HRESULT* phr);
    virtual ~SimpleSubPicProvider();

    DECLARE_IUNKNOWN;
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);

    bool LookupSubPicEx(REFERENCE_TIME rtNow, ISubPicEx** ppSubPic);
    HRESULT GetSubPicProviderEx(ISubPicProviderEx2** pSubPicProviderEx);
    HRESULT RenderTo(ISubPicEx* pSubPic, REFERENCE_TIME rtStart, REFERENCE_TIME rtStop, double fps, BOOL bIsAnimated);
public:
    // ISimpleSubPicProvider

    STDMETHODIMP SetSubPicProvider(IUnknown* subpic_provider);
    STDMETHODIMP GetSubPicProvider(IUnknown** subpic_provider);

    STDMETHODIMP SetFPS(double fps);
    STDMETHODIMP SetTime(REFERENCE_TIME rtNow);

    STDMETHODIMP Invalidate(REFERENCE_TIME rtInvalidate = -1);
    STDMETHODIMP_(bool) LookupSubPic(REFERENCE_TIME now /*[in]*/, ISimpleSubPic** output_subpic/*[out]*/);

    STDMETHODIMP GetStats(int& nSubPics, REFERENCE_TIME& rtNow, REFERENCE_TIME& rtStart, REFERENCE_TIME& rtStop);
    STDMETHODIMP GetStats(int nSubPic, REFERENCE_TIME& rtStart, REFERENCE_TIME& rtStop);
private:
    CCritSec m_csSubPicProvider;
    CComPtr<ISubPicProviderEx2> m_pSubPicProviderEx;

    CAtlList<int> m_prefered_colortype;
protected:
    double m_fps;
    REFERENCE_TIME m_rtNow;

    CComPtr<ISubPicExAllocator> m_pAllocator;

    CCritSec m_csLock;
    CComPtr<ISubPicEx> m_pSubPic;
};

//////////////////////////////////////////////////////////////////////////
//
// SimpleSubPicProvider2
// 

class SimpleSubPicProvider2: public CUnknown, public ISimpleSubPicProvider
{
public:
    SimpleSubPicProvider2(ISubPicExAllocator* pAllocator, HRESULT* phr, const int *prefered_colortype=NULL, int prefered_colortype_num=0);
    virtual ~SimpleSubPicProvider2();

    DECLARE_IUNKNOWN;
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);
public:

    // ISimpleSubPicProvider

    STDMETHODIMP SetSubPicProvider(IUnknown* subpic_provider);
    STDMETHODIMP GetSubPicProvider(IUnknown** subpic_provider);

    STDMETHODIMP SetFPS(double fps);
    STDMETHODIMP SetTime(REFERENCE_TIME rtNow);

    STDMETHODIMP Invalidate(REFERENCE_TIME rtInvalidate = -1);
    STDMETHODIMP_(bool) LookupSubPic(REFERENCE_TIME now /*[in]*/, ISimpleSubPic** output_subpic/*[out]*/);

    STDMETHODIMP GetStats(int& nSubPics, REFERENCE_TIME& rtNow, REFERENCE_TIME& rtStart, REFERENCE_TIME& rtStop);
    STDMETHODIMP GetStats(int nSubPic, REFERENCE_TIME& rtStart, REFERENCE_TIME& rtStop);

protected:
    ISimpleSubPicProvider *m_cur_provider;
    SimpleSubPicProvider m_ex_provider;
    CSubPicQueueNoThread m_old_provider;

private:
    SimpleSubPicProvider2(const SimpleSubPicProvider2&);
    void operator=(const SimpleSubPicProvider2&)const;
};
