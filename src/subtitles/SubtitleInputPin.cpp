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

#include "stdafx.h"
#include "SSF.h"
#include "VobSubFile.h"
#include "RTS.h"
#include "RenderedHdmvSubtitle.h"
#include "SubtitleInputPin.h"

#include <initguid.h>
#include "..\..\include\moreuuids.h"

// our first format id
#define __GAB1__ "GAB1"

// our tags for __GAB1__ (ushort) + size (ushort)

// "lang" + '0'
#define __GAB1_LANGUAGE__ 0
// (int)start+(int)stop+(char*)line+'0'
#define __GAB1_ENTRY__ 1
// L"lang" + '0'
#define __GAB1_LANGUAGE_UNICODE__ 2
// (int)start+(int)stop+(WCHAR*)line+'0'
#define __GAB1_ENTRY_UNICODE__ 3

// same as __GAB1__, but the size is (uint) and only __GAB1_LANGUAGE_UNICODE__ is valid
#define __GAB2__ "GAB2"

// (BYTE*)
#define __GAB1_RAWTEXTSUBTITLE__ 4

#if ENABLE_XY_LOG_EMBEDDED_SAMPLE
# define TRACE_SAMPLE(msg) XY_LOG_TRACE(msg)
# define TRACE_SAMPLE_TIMING(msg) XY_AUTO_TIMING(msg)
#else
# define TRACE_SAMPLE(msg)
# define TRACE_SAMPLE_TIMING(msg)
#endif

//
// CSubtitleInputPinHelperImpl
//
STDMETHODIMP CSubtitleInputPinHelperImpl::NewSegment( REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate )
{
    m_tStart = tStart;
    m_tStop = tStop;
    m_dRate = dRate;
    return S_OK;
}

//
// CTextSubtitleInputPinImpl
//
class CTextSubtitleInputPinHepler: public CSubtitleInputPinHelperImpl
{
public:
    CTextSubtitleInputPinHepler(CRenderedTextSubtitle *pRTS, const CMediaType& mt);

    STDMETHODIMP NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate);
    STDMETHODIMP Receive(IMediaSample* pSample);
private:
    CRenderedTextSubtitle * m_pRTS;
    const CMediaType& m_mt;
};

//
// CSSFInputPinHepler
//
class CSSFInputPinHepler: public CSubtitleInputPinHelperImpl
{
public:
    CSSFInputPinHepler(ssf::CRenderer* pSSF, const CMediaType& mt);

    STDMETHODIMP NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate);
    STDMETHODIMP Receive(IMediaSample* pSample);
private:
    ssf::CRenderer* m_pSSF;
    const CMediaType& m_mt;
};

//
// CVobsubInputPinHepler
//
class CVobsubInputPinHepler: public CSubtitleInputPinHelperImpl
{
public:
    CVobsubInputPinHepler(CVobSubStream* pVSS, const CMediaType& mt);

    STDMETHODIMP NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate);
    STDMETHODIMP Receive(IMediaSample* pSample);
private:
    CVobSubStream* m_pVSS;
    const CMediaType& m_mt;
};

//
// CHdmvInputPinHepler
//
class CHdmvInputPinHepler: public CSubtitleInputPinHelperImpl
{
public:
    CHdmvInputPinHepler(CRenderedHdmvSubtitle* pHdmvSubtitle, const CMediaType& mt);

    STDMETHODIMP NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate);
    STDMETHODIMP Receive(IMediaSample* pSample);
    STDMETHODIMP EndOfStream(void);
private:
    CRenderedHdmvSubtitle* m_pHdmvSubtitle;
    const CMediaType& m_mt;
};


//
// CTextSubtitleInputPinHepler
//

CTextSubtitleInputPinHepler::CTextSubtitleInputPinHepler( CRenderedTextSubtitle *pRTS
                                                         , const CMediaType& mt )
                                                         : CSubtitleInputPinHelperImpl(pRTS)
                                                         , m_pRTS(pRTS), m_mt(mt) 
{

}

STDMETHODIMP CTextSubtitleInputPinHepler::NewSegment( REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate )
{
    m_pRTS->RemoveAllEntries();
    m_pRTS->CreateSegments();
    return __super::NewSegment(tStart,tStop,dRate);
}

STDMETHODIMP CTextSubtitleInputPinHepler::Receive( IMediaSample* pSample )
{
    REFERENCE_TIME tStart, tStop;
    pSample->GetTime(&tStart, &tStop);
    tStart += m_tStart; 
    tStop += m_tStart;

    BYTE* pData = NULL;
    HRESULT hr = pSample->GetPointer(&pData);
    if(FAILED(hr) || pData == NULL) return hr;

    int len = pSample->GetActualDataLength();

    if(m_mt.majortype == MEDIATYPE_Text)
    {
        if(!strncmp((char*)pData, __GAB1__, strlen(__GAB1__)))
        {
            char* ptr = (char*)&pData[strlen(__GAB1__)+1];
            char* end = (char*)&pData[len];

            while(ptr < end)
            {
                WORD tag = *((WORD*)(ptr)); ptr += 2;
                WORD size = *((WORD*)(ptr)); ptr += 2;

                if(tag == __GAB1_LANGUAGE__)
                {
                    m_pRTS->m_name = CString(ptr);
                }
                else if(tag == __GAB1_ENTRY__)
                {
                    m_pRTS->Add((LPWSTR)CA2WEX<>(ptr), false, *(int*)ptr, *(int*)(ptr+4));
                }
                else if(tag == __GAB1_LANGUAGE_UNICODE__)
                {
                    m_pRTS->m_name = (WCHAR*)ptr;
                }
                else if(tag == __GAB1_ENTRY_UNICODE__)
                {
                    m_pRTS->Add((WCHAR*)(ptr+8), true, *(int*)ptr, *(int*)(ptr+4));
                }

                ptr += size;
            }
        }
        else if(!strncmp((char*)pData, __GAB2__, strlen(__GAB2__)))
        {
            char* ptr = (char*)&pData[strlen(__GAB2__)+1];
            char* end = (char*)&pData[len];

            while(ptr < end)
            {
                WORD tag = *((WORD*)(ptr)); ptr += 2;
                DWORD size = *((DWORD*)(ptr)); ptr += 4;

                if(tag == __GAB1_LANGUAGE_UNICODE__)
                {
                    m_pRTS->m_name = (WCHAR*)ptr;
                }
                else if(tag == __GAB1_RAWTEXTSUBTITLE__)
                {
                    m_pRTS->Open((BYTE*)ptr, size, DEFAULT_CHARSET, m_pRTS->m_name);
                }

                ptr += size;
            }
        }
        else if(pData != 0 && len > 1 && *pData != 0)
        {
            CStringA str((char*)pData, len);

            str.Replace("\r\n", "\n");
            str.Trim();

            if(!str.IsEmpty())
            {
                m_pRTS->Add((LPWSTR)CA2WEX<>(str), false, (int)(tStart / 10000), (int)(tStop / 10000));
            }
        }
        else
        {
            XY_LOG_WARN("Unexpected data");
        }
    }
    else if(m_mt.majortype == MEDIATYPE_Subtitle)
    {
        if(m_mt.subtype == MEDIASUBTYPE_UTF8)
        {
            CStringW str = UTF8To16(CStringA((LPCSTR)pData, len)).Trim();
            if(!str.IsEmpty())
            {
                m_pRTS->Add(str, true, (int)(tStart / 10000), (int)(tStop / 10000));
            }
            else
            {
                XY_LOG_WARN("Empty data");
            }
        }
        else if(m_mt.subtype == MEDIASUBTYPE_SSA || m_mt.subtype == MEDIASUBTYPE_ASS || m_mt.subtype == MEDIASUBTYPE_ASS2)
        {
            CStringW str = UTF8To16(CStringA((LPCSTR)pData, len)).Trim();
            if(!str.IsEmpty())
            {
                STSEntry stse;

                int fields = m_mt.subtype == MEDIASUBTYPE_ASS2 ? 10 : 9;

                CAtlList<CStringW> sl;
                Explode(str, sl, ',', fields);
                if(sl.GetCount() == fields)
                {
                    stse.readorder = wcstol(sl.RemoveHead(), NULL, 10);
                    stse.layer = wcstol(sl.RemoveHead(), NULL, 10);
                    stse.style = sl.RemoveHead();
                    stse.actor = sl.RemoveHead();
                    stse.marginRect.left = wcstol(sl.RemoveHead(), NULL, 10);
                    stse.marginRect.right = wcstol(sl.RemoveHead(), NULL, 10);
                    stse.marginRect.top = stse.marginRect.bottom = wcstol(sl.RemoveHead(), NULL, 10);
                    if(fields == 10) stse.marginRect.bottom = wcstol(sl.RemoveHead(), NULL, 10);
                    stse.effect = sl.RemoveHead();
                    stse.str = sl.RemoveHead();
                }

                if(!stse.str.IsEmpty())
                {
                    m_pRTS->Add(stse.str, true, (int)(tStart / 10000), (int)(tStop / 10000), 
                        stse.style, stse.actor, stse.effect, stse.marginRect, stse.layer, stse.readorder);
                }
            }
            else
            {
                XY_LOG_WARN("Empty data");
            }
        }
        else
        {
            XY_LOG_WARN("Unsupported media type "<<XyUuidToString(m_mt.subtype));
        }
    }
    else
    {
        XY_LOG_WARN("Unsupported media type "<<XyUuidToString(m_mt.majortype));
    }
    return S_OK;
}

//
// CSSFInputPinHepler
//

CSSFInputPinHepler::CSSFInputPinHepler( ssf::CRenderer* pSSF, const CMediaType& mt )
    : CSubtitleInputPinHelperImpl(pSSF)
    , m_pSSF(pSSF), m_mt(mt)
{

}

HRESULT CSSFInputPinHepler::NewSegment( REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate )
{
    // LAME, implement RemoveSubtitles
    DWORD dwOffset = ((SUBTITLEINFO*)m_mt.pbFormat)->dwOffset;
    m_pSSF->Open(ssf::MemoryInputStream(m_mt.pbFormat + dwOffset, m_mt.cbFormat - dwOffset, false, false), _T(""));
    // pSSF->RemoveSubtitles();
    return __super::NewSegment(tStart,tStop,dRate);
}

STDMETHODIMP CSSFInputPinHepler::Receive( IMediaSample* pSample )
{
    REFERENCE_TIME tStart, tStop;
    pSample->GetTime(&tStart, &tStop);
    tStart += m_tStart; 
    tStop += m_tStart;

    BYTE* pData = NULL;
    HRESULT hr = pSample->GetPointer(&pData);
    if(FAILED(hr) || pData == NULL) return hr;

    int len = pSample->GetActualDataLength();

    bool fInvalidate = false;
    if(m_mt.majortype == MEDIATYPE_Subtitle)
    {
        if(m_mt.subtype == MEDIASUBTYPE_SSF)
        {
            CStringW str = UTF8To16(CStringA((LPCSTR)pData, len)).Trim();
            if(!str.IsEmpty())
            {
                m_pSSF->Append(tStart, tStop, str);
            }
            else
            {
                XY_LOG_WARN("Empty data");
            }
        }
    }
    return S_OK;
}

//
// CVobsubInputPinHepler
//

CVobsubInputPinHepler::CVobsubInputPinHepler( CVobSubStream* pVSS, const CMediaType& mt )
    : CSubtitleInputPinHelperImpl(pVSS)
    , m_pVSS(pVSS), m_mt(mt)
{

}

STDMETHODIMP CVobsubInputPinHepler::NewSegment( REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate )
{
    m_pVSS->RemoveAll();
    return __super::NewSegment(tStart,tStop,dRate);
}

STDMETHODIMP CVobsubInputPinHepler::Receive( IMediaSample* pSample )
{
    REFERENCE_TIME tStart, tStop;
    pSample->GetTime(&tStart, &tStop);
    tStart += m_tStart; 
    tStop += m_tStart;

    BYTE* pData = NULL;
    HRESULT hr = pSample->GetPointer(&pData);
    if(FAILED(hr) || pData == NULL) return hr;

    int len = pSample->GetActualDataLength();

    if(m_mt.majortype == MEDIATYPE_Subtitle)
    {
        if(m_mt.subtype == MEDIASUBTYPE_VOBSUB)
        {
            m_pVSS->Add(tStart, tStop, pData, len);
        }
    }
    return S_OK;
}

//
// CHdmvInputPinHepler
//

CHdmvInputPinHepler::CHdmvInputPinHepler( CRenderedHdmvSubtitle* pHdmvSubtitle, const CMediaType& mt )
    : CSubtitleInputPinHelperImpl(pHdmvSubtitle)
    , m_pHdmvSubtitle(pHdmvSubtitle), m_mt(mt)
{

}

STDMETHODIMP CHdmvInputPinHepler::NewSegment( REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate )
{
    return m_pHdmvSubtitle->NewSegment (tStart, tStop, dRate);
}

STDMETHODIMP CHdmvInputPinHepler::Receive( IMediaSample* pSample )
{
    return m_pHdmvSubtitle->ParseSample (pSample);
}

STDMETHODIMP CHdmvInputPinHepler::EndOfStream( void )
{
    m_pHdmvSubtitle->EndOfStream();
    return S_OK;
}

CSubtitleInputPin::CSubtitleInputPin(CBaseFilter* pFilter, CCritSec* pLock, CCritSec* pSubLock, HRESULT* phr)
    : CBaseInputPin(NAME("CSubtitleInputPin"), pFilter, pLock, phr, L"Input")
    , m_pSubLock(pSubLock)
    , m_helper(NULL)
{
    m_bCanReconnectWhenActive = TRUE;
}

HRESULT CSubtitleInputPin::CheckMediaType(const CMediaType* pmt)
{
    XY_LOG_DEBUG(XY_LOG_VAR_2_STR(pmt));
    return pmt->majortype == MEDIATYPE_Text && (pmt->subtype == MEDIASUBTYPE_NULL || pmt->subtype == FOURCCMap((DWORD)0))
        || pmt->majortype == MEDIATYPE_Subtitle && pmt->subtype == MEDIASUBTYPE_UTF8
        || pmt->majortype == MEDIATYPE_Subtitle && (pmt->subtype == MEDIASUBTYPE_SSA || pmt->subtype == MEDIASUBTYPE_ASS || pmt->subtype == MEDIASUBTYPE_ASS2)
        || pmt->majortype == MEDIATYPE_Subtitle && pmt->subtype == MEDIASUBTYPE_SSF
        || pmt->majortype == MEDIATYPE_Subtitle && (pmt->subtype == MEDIASUBTYPE_VOBSUB)
        || IsHdmvSub(pmt)
        ? S_OK 
        : E_FAIL;
}

HRESULT CSubtitleInputPin::CompleteConnect(IPin* pReceivePin)
{
    CAutoLock cAutoLock(m_pSubLock);
    XY_LOG_DEBUG(XY_LOG_VAR_2_STR(pReceivePin));
    delete m_helper; m_helper = NULL;
    m_helper = CreateHelper(m_mt, pReceivePin);
    if (!m_helper)
    {
        XY_LOG_ERROR("Failed to Create helper. ");
        return E_FAIL;
    }
    AddSubStream(m_helper->GetSubStream());

    return __super::CompleteConnect(pReceivePin);
}

STDMETHODIMP_(CSubtitleInputPinHelper*) CSubtitleInputPin::CreateHelper( const CMediaType& mt, IPin* pReceivePin )
{
    XY_LOG_INFO(XY_LOG_VAR_2_STR(pReceivePin));
    CSubtitleInputPinHelper *ret = NULL;
    if(mt.majortype == MEDIATYPE_Text)
    {
        XY_LOG_INFO("Create CTextSubtitleInputPinHepler");
        CRenderedTextSubtitle* pRTS = DEBUG_NEW CRenderedTextSubtitle(m_pSubLock);
        pRTS->m_name = CString(GetPinName(pReceivePin)) + _T(" (embeded)");
        pRTS->m_dstScreenSize = CSize(384, 288);
        ret = DEBUG_NEW CTextSubtitleInputPinHepler(pRTS, m_mt);
    }
    else if(mt.majortype == MEDIATYPE_Subtitle)
    {
        SUBTITLEINFO* psi      = (SUBTITLEINFO*)mt.pbFormat;
        DWORD         dwOffset = 0;
        CString       name;
        LCID          lcid     = 0;

        if (psi != NULL) {
            dwOffset = psi->dwOffset;

            name = ISO6392ToLanguage(psi->IsoLang);
            lcid = ISO6392ToLcid(psi->IsoLang);

            CString trackName(psi->TrackName);
            trackName.Trim();
            if (!trackName.IsEmpty()) {
                if (!name.IsEmpty()) {
                    if (trackName[0] != _T('(') && trackName[0] != _T('[')) {
                        name += _T(",");
                    }
                    name += _T(" ");
                }
                name += trackName;
            }
            if (name.IsEmpty()) {
                name = _T("Unknown");
            }
        }

        name.Replace(_T(""), _T(""));//CAUTION: VS may show name.Replace(_T(""),_T("")), however there is a character in the first _T("")
        name.Replace(_T(""), _T(""));//CAUTION: VS may show name.Replace(_T(""),_T("")), however there is a character in the first _T("")

        if(mt.subtype == MEDIASUBTYPE_UTF8 
            /*|| m_mt.subtype == MEDIASUBTYPE_USF*/
            || mt.subtype == MEDIASUBTYPE_SSA 
            || mt.subtype == MEDIASUBTYPE_ASS 
            || mt.subtype == MEDIASUBTYPE_ASS2)
        {
            XY_LOG_INFO("Create CTextSubtitleInputPinHepler");
            CRenderedTextSubtitle* pRTS = DEBUG_NEW CRenderedTextSubtitle(m_pSubLock);
            pRTS->m_name = name;
            pRTS->m_lcid = lcid;
            pRTS->m_dstScreenSize = CSize(384, 288);

            if(dwOffset > 0 && mt.cbFormat - dwOffset > 0)
            {
                CMediaType mt1 = mt;
                if(mt1.pbFormat[dwOffset+0] != 0xef
                    && mt1.pbFormat[dwOffset+1] != 0xbb
                    && mt1.pbFormat[dwOffset+2] != 0xfb)
                {
                    dwOffset -= 3;
                    mt1.pbFormat[dwOffset+0] = 0xef;
                    mt1.pbFormat[dwOffset+1] = 0xbb;
                    mt1.pbFormat[dwOffset+2] = 0xbf;
                }

                pRTS->Open(mt1.pbFormat + dwOffset, mt1.cbFormat - dwOffset, DEFAULT_CHARSET, pRTS->m_name);
            }
            ret = DEBUG_NEW CTextSubtitleInputPinHepler(pRTS, m_mt);
        }
        else if(mt.subtype == MEDIASUBTYPE_SSF)
        {
            XY_LOG_INFO("Create CSSFInputPinHepler");
            ssf::CRenderer* pSSF = DEBUG_NEW ssf::CRenderer(m_pSubLock);
            pSSF->Open(ssf::MemoryInputStream(mt.pbFormat + dwOffset, mt.cbFormat - dwOffset, false, false), name);
            ret = DEBUG_NEW CSSFInputPinHepler(pSSF, m_mt);
        }
        else if(mt.subtype == MEDIASUBTYPE_VOBSUB)
        {
            XY_LOG_INFO("Create CVobsubInputPinHepler");
            CVobSubStream* pVSS = DEBUG_NEW CVobSubStream(m_pSubLock);
            pVSS->Open(name, mt.pbFormat + dwOffset, mt.cbFormat - dwOffset);
            ret = DEBUG_NEW CVobsubInputPinHepler(pVSS, m_mt);
        }
        else if (IsHdmvSub(&mt)) 
        {
            XY_LOG_INFO("Create CHdmvInputPinHepler");
            CRenderedHdmvSubtitle *hdmv_sub = DEBUG_NEW CRenderedHdmvSubtitle(m_pSubLock,
                (mt.subtype == MEDIASUBTYPE_DVB_SUBTITLES) ? ST_DVB : ST_HDMV, name, lcid);
            ret = DEBUG_NEW CHdmvInputPinHepler(hdmv_sub, m_mt);
        }
    }
    return ret;
}

HRESULT CSubtitleInputPin::BreakConnect()
{
    CAutoLock cAutoLock(m_pSubLock);
    XY_LOG_DEBUG("");
    if (m_helper)
    {
        RemoveSubStream(m_helper->GetSubStream());
        delete m_helper; m_helper = NULL;
    }
    ASSERT(IsStopped());

    return __super::BreakConnect();
}

STDMETHODIMP CSubtitleInputPin::ReceiveConnection(IPin* pConnector, const AM_MEDIA_TYPE* pmt)
{
    CAutoLock cAutoLock(m_pSubLock);
    XY_LOG_DEBUG(XY_LOG_VAR_2_STR(pConnector)<<XY_LOG_VAR_2_STR(pmt));
	if(m_Connected)
	{
        RemoveSubStream(m_helper->GetSubStream());
        delete m_helper; m_helper = NULL;

        m_Connected->Release();
        m_Connected = NULL;
	}

	return __super::ReceiveConnection(pConnector, pmt);
}

STDMETHODIMP CSubtitleInputPin::NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate)
{
    TRACE_SAMPLE_TIMING(XY_LOG_VAR_2_STR(tStart)<<XY_LOG_VAR_2_STR(tStop)<<XY_LOG_VAR_2_STR(dRate));
    CAutoLock cAutoLock(m_pSubLock);
    if(m_helper)
    {
        m_helper->NewSegment(tStart, tStop, dRate);
    }
    else
    {
        XY_LOG_WARN("Helper was NOT created yet");
    }
    return __super::NewSegment(tStart, tStop, dRate);
}

interface __declspec(uuid("D3D92BC3-713B-451B-9122-320095D51EA5"))
IMpeg2DemultiplexerTesting :
public IUnknown {
	STDMETHOD(GetMpeg2StreamType)(ULONG* plType) = NULL;
	STDMETHOD(toto)() = NULL;
};

STDMETHODIMP CSubtitleInputPin::Receive(IMediaSample* pSample)
{
    TRACE_SAMPLE_TIMING(__FUNCTIONW__);
    HRESULT hr;
    REFERENCE_TIME tStart, tStop;
    hr = pSample->GetTime(&tStart, &tStop);
    ASSERT(SUCCEEDED(hr));
    tStart += m_tStart; 

    hr = __super::Receive(pSample);

    CAutoLock cAutoLock(m_pSubLock);
    if (m_helper)
    {
        hr = m_helper->Receive(pSample);
        TRACE_SAMPLE("InvalidateSubtitle :"<<ReftimeToCString(tStart));
        InvalidateSubtitle(tStart, m_helper->GetSubStream());
    }

    hr = S_OK;
    return hr;
}

STDMETHODIMP CSubtitleInputPin::EndOfStream(void)
{
    HRESULT hr = __super::EndOfStream();

    if (SUCCEEDED(hr)) {
        CAutoLock cAutoLock(m_pSubLock);
        if (m_helper)
        {
            m_helper->EndOfStream();
        }
        else
        {
            ASSERT(0);
        }
    }

    return hr;
}

bool CSubtitleInputPin::IsHdmvSub(const CMediaType* pmt)
{
	return pmt->majortype == MEDIATYPE_Subtitle && (pmt->subtype == MEDIASUBTYPE_HDMVSUB ||			// Blu ray presentation graphics
			pmt->subtype == MEDIASUBTYPE_DVB_SUBTITLES ||	// DVB subtitles
			(pmt->subtype == MEDIASUBTYPE_NULL && pmt->formattype == FORMAT_SubtitleInfo)) // Workaround : support for Haali PGS
		   ? true
		   : false;
}

ISubStream* CSubtitleInputPin::GetSubStream()
{
    CAutoLock cAutoLock(m_pSubLock);
    return m_helper ? m_helper->GetSubStream() : NULL;
}
