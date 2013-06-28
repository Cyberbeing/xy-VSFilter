#include "stdafx.h"
#include "xy_sub_filter.h"
#include "SubtitleInputPin2.h"
#include "../../../subtitles/hdmv_subtitle_provider.h"
#include "moreuuids.h"
//
// CHdmvInputPinHepler
//
class CHdmvInputPinHepler2: public CSubtitleInputPinHelperImpl
{
public:
    CHdmvInputPinHepler2(HdmvSubtitleProvider* pHdmvSubtitle, const CMediaType& mt);

    STDMETHODIMP NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate);
    STDMETHODIMP Receive(IMediaSample* pSample);
    STDMETHODIMP EndOfStream(void);
private:
    HdmvSubtitleProvider* m_pHdmvSubtitle;
    const CMediaType& m_mt;
};

CHdmvInputPinHepler2::CHdmvInputPinHepler2( HdmvSubtitleProvider* pHdmvSubtitle, const CMediaType& mt )
    : CSubtitleInputPinHelperImpl(pHdmvSubtitle)
    , m_pHdmvSubtitle(pHdmvSubtitle), m_mt(mt)
{

}

STDMETHODIMP CHdmvInputPinHepler2::NewSegment( REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate )
{
    return m_pHdmvSubtitle->NewSegment (tStart, tStop, dRate);
}

STDMETHODIMP CHdmvInputPinHepler2::Receive( IMediaSample* pSample )
{
    return m_pHdmvSubtitle->ParseSample (pSample);
}

STDMETHODIMP CHdmvInputPinHepler2::EndOfStream( void )
{
    return m_pHdmvSubtitle->EndOfStream();
}

//
// SubtitleInputPin2
//
SubtitleInputPin2::SubtitleInputPin2( XySubFilter* pFilter, CCritSec* pLock
                                     , CCritSec* pSubLock, HRESULT* phr )
                                     : CSubtitleInputPin(pFilter,pLock,pSubLock,phr)
                                     , xy_sub_filter(pFilter)
{
    ASSERT(pFilter);
}

void SubtitleInputPin2::AddSubStream( ISubStream* pSubStream )
{
    xy_sub_filter->AddSubStream(pSubStream);
}

void SubtitleInputPin2::RemoveSubStream( ISubStream* pSubStream )
{
    xy_sub_filter->RemoveSubStream(pSubStream);
}

void SubtitleInputPin2::InvalidateSubtitle( REFERENCE_TIME rtStart, ISubStream* pSubStream )
{
    xy_sub_filter->InvalidateSubtitle(rtStart, (DWORD_PTR)(ISubStream*)pSubStream);
}

STDMETHODIMP_(CSubtitleInputPinHelper*) SubtitleInputPin2::CreateHelper( const CMediaType& mt, IPin* pReceivePin )
{
    if (IsHdmvSub(&mt)) 
    {
        XY_LOG_INFO("Create CHdmvInputPinHepler2");
        SUBTITLEINFO* psi = (SUBTITLEINFO*)mt.pbFormat;
        DWORD			dwOffset	= 0;
        CString			name;
        LCID			lcid = 0;

        if (psi != NULL) {
            dwOffset = psi->dwOffset;

            name = ISO6392ToLanguage(psi->IsoLang);
            lcid = ISO6392ToLcid(psi->IsoLang);

            if(wcslen(psi->TrackName) > 0) {
                name += (!name.IsEmpty() ? _T(", ") : _T("")) + CString(psi->TrackName);
            }
            if(name.IsEmpty()) {
                name = _T("Unknown");
            }
        }

        HdmvSubtitleProvider *hdmv_sub = DEBUG_NEW HdmvSubtitleProvider(m_pSubLock,
            (mt.subtype == MEDIASUBTYPE_DVB_SUBTITLES) ? ST_DVB : ST_HDMV, name, lcid);
        return DEBUG_NEW CHdmvInputPinHepler2(hdmv_sub, m_mt);
    }
    else
    {
        return __super::CreateHelper(mt, pReceivePin);
    }
    return NULL;
}

HRESULT SubtitleInputPin2::CheckMediaType(const CMediaType* pmt)
{
    HRESULT hr = xy_sub_filter->CheckInputType(pmt);
    if (FAILED(hr))
    {
        return hr;
    }
    return __super::CheckMediaType(pmt);
}
