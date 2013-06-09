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

#include "StdAfx.h"
#include <mmintrin.h>
#include "BaseVideoFilter.h"
#include "..\..\..\DSUtil\DSUtil.h"
#include "..\..\..\DSUtil\MediaTypes.h"

#pragma warning(push) 
#pragma warning(disable:4995) 

#include <initguid.h>
#include "..\..\..\..\include\moreuuids.h"
#include <algorithm>
#include "xy_logger.h"

#pragma warning(pop) 

const GUID* InputFmts[] =
{
    &MEDIASUBTYPE_P010,
    &MEDIASUBTYPE_P016,
    &MEDIASUBTYPE_NV12,
    &MEDIASUBTYPE_NV21,
    &MEDIASUBTYPE_YV12, 
    &MEDIASUBTYPE_I420, 
    &MEDIASUBTYPE_IYUV, 
    &MEDIASUBTYPE_YUY2, 
    &MEDIASUBTYPE_AYUV,
    &MEDIASUBTYPE_ARGB32, 
    &MEDIASUBTYPE_RGB32,
    &MEDIASUBTYPE_RGB24,
    &MEDIASUBTYPE_RGB565, 
    //&MEDIASUBTYPE_RGB555
};

const OutputFormatBase OutputFmts[] =
{
    {&MEDIASUBTYPE_P010, 2, 24, '010P'},
    {&MEDIASUBTYPE_P016, 2, 24, '610P'},
    {&MEDIASUBTYPE_NV12, 2, 12, '21VN'},
    {&MEDIASUBTYPE_NV21, 2, 12, '12VN'},
    {&MEDIASUBTYPE_YV12, 3, 12, '21VY'},
    {&MEDIASUBTYPE_I420, 3, 12, '024I'},
    {&MEDIASUBTYPE_IYUV, 3, 12, 'VUYI'},
    {&MEDIASUBTYPE_YUY2, 1, 16, '2YUY'},
    {&MEDIASUBTYPE_AYUV, 1, 32, 'VUYA'},
    {&MEDIASUBTYPE_ARGB32, 1, 32, BI_RGB},
    {&MEDIASUBTYPE_RGB32, 1, 32, BI_RGB},
    {&MEDIASUBTYPE_RGB24, 1, 24, BI_RGB},
    {&MEDIASUBTYPE_RGB565, 1, 16, BI_RGB},
    //{&MEDIASUBTYPE_RGB555, 1, 16, BI_RGB},
    {&MEDIASUBTYPE_ARGB32, 1, 32, BI_BITFIELDS},
    {&MEDIASUBTYPE_RGB32, 1, 32, BI_BITFIELDS},
    {&MEDIASUBTYPE_RGB24, 1, 24, BI_BITFIELDS},
    {&MEDIASUBTYPE_RGB565, 1, 16, BI_BITFIELDS},
    //{&MEDIASUBTYPE_RGB555, 1, 16, BI_BITFIELDS},
};

//fixme: use the same function for by input and output
CString OutputFmt2String(const OutputFormatBase& fmt)
{
    CString ret = CString(GuidNames[*fmt.subtype]);
    if(!ret.Left(13).CompareNoCase(_T("MEDIASUBTYPE_"))) ret = ret.Mid(13);
    if(fmt.biCompression == 3) ret += _T(" BITF");
    if(*fmt.subtype == MEDIASUBTYPE_I420) ret = _T("I420"); // FIXME
    else if(*fmt.subtype == MEDIASUBTYPE_NV21) ret = _T("NV21"); // FIXME
    return(ret);
}

CString GetColorSpaceName(ColorSpaceId colorSpace, ColorSpaceDir inputOrOutput)
{
    if(inputOrOutput==INPUT_COLOR_SPACE)
        return Subtype2String(*InputFmts[colorSpace]);
    else
        return OutputFmt2String(OutputFmts[colorSpace]);
}

UINT GetOutputColorSpaceNumber()
{
    return countof(OutputFmts);
}

UINT GetInputColorSpaceNumber()
{
    return countof(InputFmts);
}

ColorSpaceId Subtype2OutputColorSpaceId( const GUID& subtype, ColorSpaceId startId /*=0 */ )
{
    ColorSpaceId i=startId;
    int num = GetOutputColorSpaceNumber();
    for(i=startId;i<num;i++)
    {
        if (*(OutputFmts[i].subtype)==subtype)
        {
            break;
        }
    }
    return i<num ? i:-1;
}

//
// CBaseVideoFilter
//

CBaseVideoFilter::CBaseVideoFilter(TCHAR* pName, LPUNKNOWN lpunk, HRESULT* phr, REFCLSID clsid, long cBuffers) 
	: CTransformFilter(pName, lpunk, clsid)
	, m_cBuffers(cBuffers)
    , m_inputFmtCount(-1),m_outputFmtCount(-1)
    , m_donot_follow_upstream_preferred_order(false)
{
	if(phr) 
        *phr = S_OK;
    HRESULT hr = S_OK;
    m_pInput = new CBaseVideoInputPin(NAME("CBaseVideoInputPin"), this, &hr, L"Video");
	if(!m_pInput)
    {
        if (phr) *phr = E_OUTOFMEMORY;
        return;
    }
	if(FAILED(hr))
    {
        if (phr) *phr = hr;
        return;
    }
    m_pOutput = new CBaseVideoOutputPin(NAME("CBaseVideoOutputPin"), this, &hr, L"Output");
	if(!m_pOutput)
    {
        delete m_pInput, m_pInput = NULL;
        if (phr) *phr = E_OUTOFMEMORY;
        return;
    }
	if(FAILED(hr))
    {
        delete m_pInput, m_pInput = NULL;
        if (phr) *phr = hr;
        return;
    }

	m_wout = m_win = m_w = 0;
	m_hout = m_hin = m_h = 0;
	m_arxout = m_arxin = m_arx = 0;
	m_aryout = m_aryin = m_ary = 0;
}

CBaseVideoFilter::~CBaseVideoFilter()
{
}

int CBaseVideoFilter::GetPinCount()
{
	return 2;
}

CBasePin* CBaseVideoFilter::GetPin(int n)
{
	switch(n)
	{
	case 0: return m_pInput;
	case 1: return m_pOutput;
	}
	return NULL;
}

HRESULT CBaseVideoFilter::Receive(IMediaSample* pIn)
{
#ifndef _WIN64
    // TODOX64 : fixme!
	_mm_empty(); // just for safety
#endif

	CAutoLock cAutoLock(&m_csReceive);

	HRESULT hr;

    AM_SAMPLE2_PROPERTIES* const pProps = m_pInput->SampleProps();
    if(pProps->dwStreamId != AM_STREAM_MEDIA)
		return m_pOutput->Deliver(pIn);

	AM_MEDIA_TYPE* pmt;
	if(SUCCEEDED(pIn->GetMediaType(&pmt)) && pmt)
	{
		CMediaType mt(*pmt);
		m_pInput->SetMediaType(&mt);
		DeleteMediaType(pmt);
	}

	if(FAILED(hr = Transform(pIn)))
		return hr;

	return S_OK;
}

HRESULT CBaseVideoFilter::GetDeliveryBuffer(int w, int h, IMediaSample** ppOut)
{
	CheckPointer(ppOut, E_POINTER);

	HRESULT hr;

	if(FAILED(hr = ReconnectOutput(w, h)))
		return hr;

	if(FAILED(hr = m_pOutput->GetDeliveryBuffer(ppOut, NULL, NULL, 0)))
		return hr;

	AM_MEDIA_TYPE* pmt;
	if(SUCCEEDED((*ppOut)->GetMediaType(&pmt)) && pmt)
	{
		CMediaType mt = *pmt;
		m_pOutput->SetMediaType(&mt);
		DeleteMediaType(pmt);
	}

	(*ppOut)->SetDiscontinuity(FALSE);
	(*ppOut)->SetSyncPoint(TRUE);

	// FIXME: hell knows why but without this the overlay mixer starts very skippy
	// (don't enable this for other renderers, the old for example will go crazy if you do)
	if(GetCLSID(m_pOutput->GetConnected()) == CLSID_OverlayMixer)
		(*ppOut)->SetDiscontinuity(TRUE);

	return S_OK;
}

HRESULT CBaseVideoFilter::ReconnectOutput(int w, int h)
{
	CMediaType& mt = m_pOutput->CurrentMediaType();

	int w_org = m_w;
	int h_org = m_h;

	bool fForceReconnection = false;
	if(w != m_w || h != m_h)
	{
		fForceReconnection = true;
		m_w = w;
		m_h = h;
	}

	HRESULT hr = S_OK;

	if(fForceReconnection || m_w != m_wout || m_h != m_hout || m_arx != m_arxout || m_ary != m_aryout)
	{
		if(GetCLSID(m_pOutput->GetConnected()) == CLSID_VideoRenderer)
		{
			NotifyEvent(EC_ERRORABORT, 0, 0);
			return E_FAIL;
		}

		BITMAPINFOHEADER* bmi = NULL;

		if(mt.formattype == FORMAT_VideoInfo)
		{
			VIDEOINFOHEADER* vih = (VIDEOINFOHEADER*)mt.Format();
			SetRect(&vih->rcSource, 0, 0, m_w, m_h);
			SetRect(&vih->rcTarget, 0, 0, m_w, m_h);
			bmi = &vih->bmiHeader;
			bmi->biXPelsPerMeter = m_w * m_ary;
			bmi->biYPelsPerMeter = m_h * m_arx;
		}
		else if(mt.formattype == FORMAT_VideoInfo2)
		{
			VIDEOINFOHEADER2* vih = (VIDEOINFOHEADER2*)mt.Format();
			SetRect(&vih->rcSource, 0, 0, m_w, m_h);
			SetRect(&vih->rcTarget, 0, 0, m_w, m_h);
			bmi = &vih->bmiHeader;
			vih->dwPictAspectRatioX = m_arx;
			vih->dwPictAspectRatioY = m_ary;
		}

		bmi->biWidth = m_w;
		bmi->biHeight = m_h;
		bmi->biSizeImage = m_w*m_h*bmi->biBitCount>>3;

		hr = m_pOutput->GetConnected()->QueryAccept(&mt);
		ASSERT(SUCCEEDED(hr)); // should better not fail, after all "mt" is the current media type, just with a different resolution
HRESULT hr1 = 0, hr2 = 0;
		CComPtr<IMediaSample> pOut;
		if(SUCCEEDED(hr1 = m_pOutput->GetConnected()->ReceiveConnection(m_pOutput, &mt))
		&& SUCCEEDED(hr2 = m_pOutput->GetDeliveryBuffer(&pOut, NULL, NULL, 0)))
		{
			AM_MEDIA_TYPE* pmt;
			if(SUCCEEDED(pOut->GetMediaType(&pmt)) && pmt)
			{
				CMediaType mt = *pmt;
				m_pOutput->SetMediaType(&mt);
				DeleteMediaType(pmt);
			}
			else // stupid overlay mixer won't let us know the new pitch...
			{
				long size = pOut->GetSize();
				bmi->biWidth = size / bmi->biHeight * 8 / bmi->biBitCount;
			}
		}
		else
		{
			m_w = w_org;
			m_h = h_org;
			return E_FAIL;
		}

		m_wout = m_w;
		m_hout = m_h;
		m_arxout = m_arx;
		m_aryout = m_ary;

		// some renderers don't send this
		NotifyEvent(EC_VIDEO_SIZE_CHANGED, MAKELPARAM(m_w, m_h), 0);

		return S_OK;
	}

	return S_FALSE;
}

HRESULT CBaseVideoFilter::CopyBuffer(BYTE* pOut, BYTE* pIn, int w, int h, int pitchIn, const GUID& subtype, bool fInterlaced)
{
	int abs_h = abs(h);
	BYTE* pInYUV[3] = {pIn, pIn + pitchIn*abs_h, pIn + pitchIn*abs_h + (pitchIn>>1)*(abs_h>>1)};
	return CopyBuffer(pOut, pInYUV, w, h, pitchIn, subtype, fInterlaced);
}

HRESULT CBaseVideoFilter::CopyBuffer(BYTE* pOut, BYTE** ppIn, int w, int h, int pitchIn, const GUID& subtype, bool fInterlaced)
{
	BITMAPINFOHEADER bihOut;
	ExtractBIH(&m_pOutput->CurrentMediaType(), &bihOut);

	int pitchOut = 0;

	if (bihOut.biCompression == BI_RGB || bihOut.biCompression == BI_BITFIELDS)
	{
		pitchOut = bihOut.biWidth*bihOut.biBitCount>>3;

		if(bihOut.biHeight > 0)
		{
			pOut += pitchOut*(h-1);
			pitchOut = -pitchOut;
			if(h < 0) h = -h;
		}
	}
    if (bihOut.biCompression == 'VUYA')
    {
        pitchOut = bihOut.biWidth*bihOut.biBitCount>>3;
    }
	if(h < 0)
	{
		h = -h;
		ppIn[0] += pitchIn*(h-1);

        if(subtype == MEDIASUBTYPE_I420 || subtype == MEDIASUBTYPE_IYUV || subtype == MEDIASUBTYPE_YV12)
        {
            ppIn[1] += (pitchIn>>1)*((h>>1)-1);
		    ppIn[2] += (pitchIn>>1)*((h>>1)-1);
        }
        else if(subtype == MEDIASUBTYPE_P010 || subtype == MEDIASUBTYPE_P016 
            || subtype == MEDIASUBTYPE_NV12 || subtype == MEDIASUBTYPE_NV21)
        {
            ppIn[1] += pitchIn*((h>>1)-1);
        }
		pitchIn = -pitchIn;
	}

	if(subtype == MEDIASUBTYPE_I420 || subtype == MEDIASUBTYPE_IYUV || subtype == MEDIASUBTYPE_YV12)
	{
		BYTE* pIn = ppIn[0];
		BYTE* pInU = ppIn[1];
		BYTE* pInV = ppIn[2];

		if(subtype == MEDIASUBTYPE_YV12) {BYTE* tmp = pInU; pInU = pInV; pInV = tmp;}

		BYTE* pOutU = pOut + bihOut.biWidth*h;
		BYTE* pOutV = pOut + bihOut.biWidth*h*5/4;

		if(bihOut.biCompression == '21VY') {BYTE* tmp = pOutU; pOutU = pOutV; pOutV = tmp;}

		ASSERT(w <= abs(pitchIn));

		if(bihOut.biCompression == '2YUY')
		{
            if (!fInterlaced) {
                BitBltFromI420ToYUY2(w, h, pOut, bihOut.biWidth*2, pIn, pInU, pInV, pitchIn);
            } else {
                BitBltFromI420ToYUY2Interlaced(w, h, pOut, bihOut.biWidth*2, pIn, pInU, pInV, pitchIn);
            }
		}
		else if(bihOut.biCompression == '024I' || bihOut.biCompression == 'VUYI' || bihOut.biCompression == '21VY')
		{
			BitBltFromI420ToI420(w, h, pOut, pOutU, pOutV, bihOut.biWidth, pIn, pInU, pInV, pitchIn);
		}
		else if(bihOut.biCompression == BI_RGB || bihOut.biCompression == BI_BITFIELDS)
		{
			if(!BitBltFromI420ToRGB(w, h, pOut, pitchOut, bihOut.biBitCount, pIn, pInU, pInV, pitchIn))
			{
				for(int y = 0; y < h; y++, pOut += pitchOut)
					memset(pOut, 0, pitchOut);
			}
		}
	}
    else if(subtype == MEDIASUBTYPE_P010 || subtype == MEDIASUBTYPE_P016)
    {
        if (bihOut.biCompression != subtype.Data1)
            return VFW_E_TYPE_NOT_ACCEPTED;
        BitBltFromP010ToP010(w, h, pOut, bihOut.biWidth*2, ppIn[0], pitchIn);
    }
    else if(subtype == MEDIASUBTYPE_NV12 || subtype == MEDIASUBTYPE_NV21) 
    {
        if (bihOut.biCompression != subtype.Data1)
            return VFW_E_TYPE_NOT_ACCEPTED;
        BitBltFromNV12ToNV12(w, h, pOut, bihOut.biWidth, ppIn[0], pitchIn);
    }
    else if(subtype == MEDIASUBTYPE_YUY2)
    {
        if(bihOut.biCompression == '2YUY')
        {
            BitBltFromYUY2ToYUY2(w, h, pOut, bihOut.biWidth*2, ppIn[0], pitchIn);
        }
        else if(bihOut.biCompression == BI_RGB || bihOut.biCompression == BI_BITFIELDS)
        {
            if(!BitBltFromYUY2ToRGB(w, h, pOut, pitchOut, bihOut.biBitCount, ppIn[0], pitchIn))
            {
                for(int y = 0; y < h; y++, pOut += pitchOut)
                    memset(pOut, 0, pitchOut);
            }
        }
    }
	else if(subtype == MEDIASUBTYPE_ARGB32 || subtype == MEDIASUBTYPE_RGB32 || subtype == MEDIASUBTYPE_RGB24 || subtype == MEDIASUBTYPE_RGB565
        || subtype == MEDIASUBTYPE_AYUV)
	{
		int sbpp = 
			subtype == MEDIASUBTYPE_ARGB32 || subtype == MEDIASUBTYPE_RGB32 || subtype == MEDIASUBTYPE_AYUV ? 32 :
			subtype == MEDIASUBTYPE_RGB24 ? 24 :
			subtype == MEDIASUBTYPE_RGB565 ? 16 : 0;

		if(bihOut.biCompression == '2YUY')
		{
			// TODO
			// BitBltFromRGBToYUY2();
		}
		else if(bihOut.biCompression == BI_RGB || bihOut.biCompression == BI_BITFIELDS ||
            bihOut.biCompression == 'VUYA' )
		{
			if(!BitBltFromRGBToRGB(w, h, pOut, pitchOut, bihOut.biBitCount, ppIn[0], pitchIn, sbpp))
			{
				for(int y = 0; y < h; y++, pOut += pitchOut)
					memset(pOut, 0, pitchOut);
			}
		}
	}
	else
	{
		return VFW_E_TYPE_NOT_ACCEPTED;
	}

	return S_OK;
}

HRESULT CBaseVideoFilter::CheckInputType(const CMediaType* mtIn)
{
	BITMAPINFOHEADER bih;
	ExtractBIH(mtIn, &bih);
    
	return mtIn->majortype == MEDIATYPE_Video 
		&& GetInputSubtypePosition(mtIn->subtype)!=-1        
		&& (mtIn->formattype == FORMAT_VideoInfo || mtIn->formattype == FORMAT_VideoInfo2)
		&& bih.biHeight > 0
		? S_OK
		: VFW_E_TYPE_NOT_ACCEPTED;
}

HRESULT CBaseVideoFilter::CheckTransform(const CMediaType* mtIn, const CMediaType* mtOut)
{
    if (GetInputSubtypePosition(mtIn->subtype)<0 || GetOutputSubtypePosition(mtOut->subtype)<0)
        return VFW_E_TYPE_NOT_ACCEPTED;
    bool can_transform = true;
    if( mtIn->subtype == MEDIASUBTYPE_YV12 
        || mtIn->subtype == MEDIASUBTYPE_I420 
        || mtIn->subtype == MEDIASUBTYPE_IYUV )
    {
        if( mtOut->subtype != MEDIASUBTYPE_YV12
            && mtOut->subtype != MEDIASUBTYPE_I420
            && mtOut->subtype != MEDIASUBTYPE_IYUV
            && mtOut->subtype != MEDIASUBTYPE_YUY2
            && mtOut->subtype != MEDIASUBTYPE_ARGB32
            && mtOut->subtype != MEDIASUBTYPE_RGB32
            && mtOut->subtype != MEDIASUBTYPE_RGB24
            && mtOut->subtype != MEDIASUBTYPE_RGB565)
            can_transform = false;
    }
    else if( mtIn->subtype == MEDIASUBTYPE_YUY2 )
    {
        if( mtOut->subtype != MEDIASUBTYPE_YUY2
            && mtOut->subtype != MEDIASUBTYPE_ARGB32
            && mtOut->subtype != MEDIASUBTYPE_RGB32
            && mtOut->subtype != MEDIASUBTYPE_RGB24
            && mtOut->subtype != MEDIASUBTYPE_RGB565)
            can_transform = false;
    }
    else if( mtIn->subtype == MEDIASUBTYPE_ARGB32
        || mtIn->subtype == MEDIASUBTYPE_RGB32
        || mtIn->subtype == MEDIASUBTYPE_RGB24
        || mtIn->subtype == MEDIASUBTYPE_RGB565 )
    {
        if(mtOut->subtype != MEDIASUBTYPE_ARGB32
            && mtOut->subtype != MEDIASUBTYPE_RGB32
            && mtOut->subtype != MEDIASUBTYPE_RGB24
            && mtOut->subtype != MEDIASUBTYPE_RGB565)
            can_transform = false;
    }
    else
    {
        can_transform = ((mtOut->subtype==mtIn->subtype)==TRUE);
    }
    if (can_transform)
    {
        return S_OK;
    }
    return VFW_E_TYPE_NOT_ACCEPTED;
}

HRESULT CBaseVideoFilter::CheckReconnect( const CMediaType* mtIn, const CMediaType* mtOut )
{
    bool can_reconnect = false;
    CMediaType desiredMt;
    int position = 0;
    HRESULT hr;

    position = GetOutputSubtypePosition(mtOut->subtype);
    if (position>=0)
    {
        hr = GetMediaType(position, &desiredMt);
        if (hr!=S_OK)
        {
            XY_LOG_ERROR(_T("Unexpected error when GetMediaType.")<<XY_LOG_VAR_2_STR(position));
        }
        else
        {
            hr = CheckTransform(&desiredMt, mtOut);
            if (hr!=S_OK)
            {
                XY_LOG_DEBUG(_T("Transform not accept.")<<XY_LOG_VAR_2_STR(GuidNames[desiredMt.subtype])
                    <<XY_LOG_VAR_2_STR(GuidNames[mtOut->subtype]));
            }
            else
            {
                hr = m_pInput->GetConnected()->QueryAccept(&desiredMt);
                if(hr!=S_OK)
                {
                    XY_LOG_DEBUG(_T("Upstream not accept.")<<XY_LOG_VAR_2_STR(GuidNames[desiredMt.subtype]));
                }
                else
                {
                    can_reconnect = true;
                }
            }
        }
    }

    if ( can_reconnect )
    {
        return S_OK;
    }
    return VFW_E_TYPE_NOT_ACCEPTED;
}

HRESULT CBaseVideoFilter::CheckOutputType(const CMediaType& mtOut)
{
	int wout = 0, hout = 0, arxout = 0, aryout = 0;
    CMediaType &mt = m_pOutput->CurrentMediaType();
	return ExtractDim(&mtOut, wout, hout, arxout, aryout)
		&& m_h == abs((int)hout)
		&& mtOut.subtype != MEDIASUBTYPE_ARGB32
        && mtOut.subtype == mt.subtype
        && GetOutputSubtypePosition(mtOut.subtype)!=-1
		? S_OK
		: VFW_E_TYPE_NOT_ACCEPTED;
}

HRESULT CBaseVideoFilter::DecideBufferSize(IMemAllocator* pAllocator, ALLOCATOR_PROPERTIES* pProperties)
{
	if(m_pInput->IsConnected() == FALSE) return E_UNEXPECTED;

	BITMAPINFOHEADER bih;
	ExtractBIH(&m_pOutput->CurrentMediaType(), &bih);

	long cBuffers = m_pOutput->CurrentMediaType().formattype == FORMAT_VideoInfo ? 1 : m_cBuffers;

	pProperties->cBuffers = m_cBuffers;
	pProperties->cbBuffer = bih.biSizeImage;
	pProperties->cbAlign = 1;
	pProperties->cbPrefix = 0;

	HRESULT hr;
	ALLOCATOR_PROPERTIES Actual;
    if(FAILED(hr = pAllocator->SetProperties(pProperties, &Actual))) 
		return hr;

    return pProperties->cBuffers > Actual.cBuffers || pProperties->cbBuffer > Actual.cbBuffer
		? E_FAIL
		: NOERROR;
}

HRESULT CBaseVideoFilter::GetMediaType(int iPosition, CMediaType* pmt)
{
    if(m_outputFmtCount <0)
    {
        InitOutputColorSpaces();
    }
    if(m_pInput->IsConnected() == FALSE) return E_UNEXPECTED;
    
	// this will make sure we won't connect to the old renderer in dvd mode
	// that renderer can't switch the format dynamically

	bool fFoundDVDNavigator = false;
	CComPtr<IBaseFilter> pBF = this;
	CComPtr<IPin> pPin = m_pInput;
	for(; !fFoundDVDNavigator && (pBF = GetUpStreamFilter(pBF, pPin)); pPin = GetFirstPin(pBF))
        fFoundDVDNavigator = ((GetCLSID(pBF) == CLSID_DVDNavigator)==TRUE);

	if(fFoundDVDNavigator || m_pInput->CurrentMediaType().formattype == FORMAT_VideoInfo2)
		iPosition = iPosition*2;

	//

	if(iPosition < 0) return E_INVALIDARG;
	if(iPosition >= 2*m_outputFmtCount) return VFW_S_NO_MORE_ITEMS;

	pmt->majortype = MEDIATYPE_Video;
	pmt->subtype = *m_outputFmt[iPosition/2]->subtype;

	int w = m_win, h = m_hin, arx = m_arxin, ary = m_aryin;
	GetOutputSize(w, h, arx, ary);

	BITMAPINFOHEADER bihOut;
	memset(&bihOut, 0, sizeof(bihOut));
	bihOut.biSize = sizeof(bihOut);
	bihOut.biWidth = w;
	bihOut.biHeight = h;
	bihOut.biPlanes = m_outputFmt[iPosition/2]->biPlanes;
	bihOut.biBitCount = m_outputFmt[iPosition/2]->biBitCount;
	bihOut.biCompression = m_outputFmt[iPosition/2]->biCompression;
	bihOut.biSizeImage = w*h*bihOut.biBitCount>>3;

	if(iPosition&1)
	{
		pmt->formattype = FORMAT_VideoInfo;
		VIDEOINFOHEADER* vih = (VIDEOINFOHEADER*)pmt->AllocFormatBuffer(sizeof(VIDEOINFOHEADER));
		memset(vih, 0, sizeof(VIDEOINFOHEADER));
		vih->bmiHeader = bihOut;
		vih->bmiHeader.biXPelsPerMeter = vih->bmiHeader.biWidth * ary;
		vih->bmiHeader.biYPelsPerMeter = vih->bmiHeader.biHeight * arx;
	}
	else
	{
		pmt->formattype = FORMAT_VideoInfo2;
		VIDEOINFOHEADER2* vih = (VIDEOINFOHEADER2*)pmt->AllocFormatBuffer(sizeof(VIDEOINFOHEADER2));
		memset(vih, 0, sizeof(VIDEOINFOHEADER2));
		vih->bmiHeader = bihOut;
		vih->dwPictAspectRatioX = arx;
		vih->dwPictAspectRatioY = ary;
		if(IsVideoInterlaced()) vih->dwInterlaceFlags = AMINTERLACE_IsInterlaced;
	}

	CMediaType& mt = m_pInput->CurrentMediaType();

	// these fields have the same field offset in all four structs
	((VIDEOINFOHEADER*)pmt->Format())->AvgTimePerFrame = ((VIDEOINFOHEADER*)mt.Format())->AvgTimePerFrame;
	((VIDEOINFOHEADER*)pmt->Format())->dwBitRate = ((VIDEOINFOHEADER*)mt.Format())->dwBitRate;
	((VIDEOINFOHEADER*)pmt->Format())->dwBitErrorRate = ((VIDEOINFOHEADER*)mt.Format())->dwBitErrorRate;

	CorrectMediaType(pmt);

	return S_OK;
}

int CBaseVideoFilter::GetInputSubtypePosition( const GUID& subtype )
{
    if(m_inputFmtCount<0)
    {
        InitInputColorSpaces();
    }

    int i=0;
    for (i=0;i<m_inputFmtCount;i++)
    {
        if (*m_inputFmt[i]==subtype)
        {
            break;
        }
    }
    return i<m_inputFmtCount ? i : -1;
}

int CBaseVideoFilter::GetOutputSubtypePosition( const GUID& subtype, int startPos /*=0*/ )
{
    if(m_outputFmtCount <0)
    {
        InitOutputColorSpaces();
    }
    int i=startPos;
    for(i=startPos;i<m_outputFmtCount;i++)
    {
        if (*(m_outputFmt[i]->subtype)==subtype)
        {
            break;
        }
    }
    return i<m_outputFmtCount ? i:-1;
}

HRESULT CBaseVideoFilter::SetMediaType(PIN_DIRECTION dir, const CMediaType* pmt)
{
	if(dir == PINDIR_INPUT)
	{
		m_w = m_h = m_arx = m_ary = 0;
		ExtractDim(pmt, m_w, m_h, m_arx, m_ary);
		m_win = m_w;
		m_hin = m_h;
		m_arxin = m_arx;
		m_aryin = m_ary;
		GetOutputSize(m_w, m_h, m_arx, m_ary);

		DWORD a = m_arx, b = m_ary;
		while(a) {int tmp = a; a = b % tmp; b = tmp;}
		if(b) m_arx /= b, m_ary /= b;
	}
	else if(dir == PINDIR_OUTPUT)
	{
		int wout = 0, hout = 0, arxout = 0, aryout = 0;
		ExtractDim(pmt, wout, hout, arxout, aryout);
		if(m_w == wout && m_h == hout && m_arx == arxout && m_ary == aryout)
		{
			m_wout = wout;
			m_hout = hout;
			m_arxout = arxout;
			m_aryout = aryout;
		}
	}

	return __super::SetMediaType(dir, pmt);
}

void CBaseVideoFilter::InitInputColorSpaces()
{
    ColorSpaceId preferredOrder[MAX_COLOR_SPACE_NUM];
    UINT count = 0;
    GetInputColorspaces(preferredOrder, &count);
    m_inputFmtCount = count;
    for (UINT i=0;i<count;i++)
    {
        m_inputFmt[i] = InputFmts[preferredOrder[i]];
    }
}

void CBaseVideoFilter::InitOutputColorSpaces()
{
    ColorSpaceId preferredOrder[MAX_COLOR_SPACE_NUM];
    UINT count = 0;
    GetOutputColorspaces(preferredOrder, &count);
    if( CombineOutputPriority(preferredOrder, &count)==S_OK )
    {
        //log
    }
    m_outputFmtCount = count;
    XY_LOG_DEBUG(XY_LOG_VAR_2_STR(m_outputFmtCount));
    for (UINT i=0;i<count;i++)
    {
        m_outputFmt[i] = OutputFmts + preferredOrder[i];
        XY_LOG_DEBUG(i<<" "<<CStringA(OutputFmt2String(*m_outputFmt[i])));
    }
}

void CBaseVideoFilter::GetInputColorspaces( ColorSpaceId *preferredOrder, UINT *count )
{
    *count = GetInputColorSpaceNumber();
    for (UINT i=0;i<*count;i++)
    {
        preferredOrder[i] = i;
    }    
}

void CBaseVideoFilter::GetOutputColorspaces( ColorSpaceId *preferredOrder, UINT *count )
{
    *count = GetOutputColorSpaceNumber();
    for (UINT i=0;i<*count;i++)
    {
        preferredOrder[i] = i;
    }
}

HRESULT CBaseVideoFilter::GetUpstreamOutputPriority( int *priorities, UINT count )
{    
    memset(priorities, 0, count*sizeof(priorities[0]));

    if( ! m_pInput->IsConnected() )
        return VFW_E_ALREADY_CONNECTED;

    int priority = count;
    CComPtr<IEnumMediaTypes> pEnumMediaTypes = NULL;
    HRESULT hr;
    hr = m_pInput->GetConnected()->EnumMediaTypes( &pEnumMediaTypes );
    if (FAILED(hr)) {
        return hr;
    }
    ASSERT(pEnumMediaTypes);     
    
    hr = pEnumMediaTypes->Reset();
    if (FAILED(hr)) {
        return hr;
    }
    CMediaType *pMediaType = NULL;
    ULONG ulMediaCount = 0;

    // attempt to remember a specific error code if there is one
    HRESULT hrFailure = S_OK;
    
    for (;;) {

        /* Retrieve the next media type NOTE each time round the loop the
           enumerator interface will allocate another AM_MEDIA_TYPE structure
           If we are successful then we copy it into our output object, if
           not then we must delete the memory allocated before returning */

        hr = pEnumMediaTypes->Next(1, (AM_MEDIA_TYPE**)&pMediaType,&ulMediaCount);
        if (hr != S_OK) {
            if (S_OK == hrFailure) {
                hrFailure = VFW_E_NO_ACCEPTABLE_TYPES;
            }
            return hrFailure;
        }
        ASSERT(ulMediaCount == 1);
        ASSERT(pMediaType);

        ColorSpaceId pos = Subtype2OutputColorSpaceId(pMediaType->subtype, 0);
        while(pos>=0 && pos<count)
        {
            priorities[pos] = priority;
            priority--;
            pos = Subtype2OutputColorSpaceId(pMediaType->subtype, pos+1);
        }

        DeleteMediaType(pMediaType);
    }
    return S_OK;
}

HRESULT CBaseVideoFilter::CombineOutputPriority( ColorSpaceId *preferredOrder, UINT *count )
{
    int priorities1[MAX_COLOR_SPACE_NUM];
    UINT total_count = GetOutputColorSpaceNumber();

    if( GetUpstreamOutputPriority(priorities1, total_count) == VFW_E_ALREADY_CONNECTED )
    {
        return VFW_E_ALREADY_CONNECTED;
    }
    if (!m_donot_follow_upstream_preferred_order)
    {
        for (UINT i=0;i<*count;i++)
        {
            int c = preferredOrder[i];
            priorities1[c] = (priorities1[c]<<16) + c;
        }
        std::sort(priorities1, priorities1+total_count);
        int i=total_count-1;
        for (;i>=0;i--)
        {
            if( priorities1[i]>=(1<<16) )//enabled 
            {
                preferredOrder[total_count-1-i] = (priorities1[i] & 0xffff);
            }
            else
            {
                break;
            }
        }
        *count = total_count-1 - i;
    }
    else
    {
        int j = 0;
        for (UINT i=0;i<*count;i++)
        {
            int c = preferredOrder[i];
            if (priorities1[c] > 0) {
                preferredOrder[j++] = c;
            }
        }
        *count = j;
    }
    return S_OK;
}

//
// CBaseVideoInputAllocator
//
	
CBaseVideoInputAllocator::CBaseVideoInputAllocator(HRESULT* phr)
	: CMemAllocator(NAME("CBaseVideoInputAllocator"), NULL, phr)
{
	if(phr) *phr = S_OK;
}

void CBaseVideoInputAllocator::SetMediaType(const CMediaType& mt)
{
	m_mt = mt;
}

STDMETHODIMP CBaseVideoInputAllocator::GetBuffer(IMediaSample** ppBuffer, REFERENCE_TIME* pStartTime, REFERENCE_TIME* pEndTime, DWORD dwFlags)
{
	if(!m_bCommitted)
        return VFW_E_NOT_COMMITTED;

	HRESULT hr = __super::GetBuffer(ppBuffer, pStartTime, pEndTime, dwFlags);

	if(SUCCEEDED(hr) && m_mt.majortype != GUID_NULL)
	{
		(*ppBuffer)->SetMediaType(&m_mt);
		m_mt.majortype = GUID_NULL;
	}

	return hr;
}

//
// CBaseVideoInputPin
//

CBaseVideoInputPin::CBaseVideoInputPin(TCHAR* pObjectName, CBaseVideoFilter* pFilter, HRESULT* phr, LPCWSTR pName) 
	: CTransformInputPin(pObjectName, pFilter, phr, pName)
	, m_pAllocator(NULL)
{
}

CBaseVideoInputPin::~CBaseVideoInputPin()
{
	delete m_pAllocator;
}

STDMETHODIMP CBaseVideoInputPin::GetAllocator(IMemAllocator** ppAllocator)
{
    CheckPointer(ppAllocator, E_POINTER);

    if(m_pAllocator == NULL)
	{
		HRESULT hr = S_OK;
        m_pAllocator = new CBaseVideoInputAllocator(&hr);
        m_pAllocator->AddRef();
    }

    (*ppAllocator = m_pAllocator)->AddRef();

    return S_OK;
} 

STDMETHODIMP CBaseVideoInputPin::ReceiveConnection(IPin* pConnector, const AM_MEDIA_TYPE* pmt)
{
	CAutoLock cObjectLock(m_pLock);

	if(m_Connected)
	{
		CMediaType mt(*pmt);

		if(FAILED(CheckMediaType(&mt)))
			return VFW_E_TYPE_NOT_ACCEPTED;

		ALLOCATOR_PROPERTIES props, actual;

		CComPtr<IMemAllocator> pMemAllocator;
		if(FAILED(GetAllocator(&pMemAllocator))
		|| FAILED(pMemAllocator->Decommit())
		|| FAILED(pMemAllocator->GetProperties(&props)))
			return E_FAIL;

		BITMAPINFOHEADER bih;
		if(ExtractBIH(pmt, &bih) && bih.biSizeImage)
			props.cbBuffer = bih.biSizeImage;

		if(FAILED(pMemAllocator->SetProperties(&props, &actual))
		|| FAILED(pMemAllocator->Commit())
		|| props.cbBuffer != actual.cbBuffer)
			return E_FAIL;

		if(m_pAllocator) 
			m_pAllocator->SetMediaType(mt);

		return SetMediaType(&mt) == S_OK
			? S_OK
			: VFW_E_TYPE_NOT_ACCEPTED;
	}

	return __super::ReceiveConnection(pConnector, pmt);
}

//
// CBaseVideoOutputPin
//

CBaseVideoOutputPin::CBaseVideoOutputPin(TCHAR* pObjectName, CBaseVideoFilter* pFilter, HRESULT* phr, LPCWSTR pName)
	: CTransformOutputPin(pObjectName, pFilter, phr, pName)
    , m_pFilter(pFilter)
{
}

HRESULT CBaseVideoOutputPin::CheckMediaType(const CMediaType* mtOut)
{
    HRESULT hr = S_OK;
    if (IsConnected())
    {
        hr = ((CBaseVideoFilter*)m_pFilter)->CheckOutputType(*mtOut);
        if(FAILED(hr)) return hr;
    }

    ASSERT(m_pFilter->m_pInput != NULL);
    if ((m_pFilter->m_pInput->IsConnected() == FALSE)) {
        return E_INVALIDARG;
    }

    hr = m_pFilter->CheckTransform(
        &m_pFilter->m_pInput->CurrentMediaType(),
        mtOut);
    if (FAILED(hr))
    {
        hr = m_pFilter->CheckReconnect(&m_pFilter->m_pInput->CurrentMediaType(), mtOut);
    }
    return hr;
}

// CBaseVideoOutputPin::CompleteConnect() calls CBaseOutputPin::CompleteConnect()
// and then calls CTransInPlaceFilter::CompleteConnect().  It does this because 
// CBaseVideoOutputPin::CompleteConnect() can reconnect a pin and we do not want to 
// reconnect a pin if CBaseOutputPin::CompleteConnect() fails.  
// CBaseOutputPin::CompleteConnect() often fails when our output pin is being connected 
// to the Video Mixing Renderer.
HRESULT CBaseVideoOutputPin::CompleteConnect( IPin *pReceivePin )
{
    HRESULT hr = CBaseOutputPin::CompleteConnect(pReceivePin);
    if (FAILED(hr)) {
        return hr;
    }

    return m_pFilter->CompleteConnect(PINDIR_OUTPUT,pReceivePin);
}

//// EnumMediaTypes
//// - pass through to our upstream filter
//STDMETHODIMP CBaseVideoOutputPin::EnumMediaTypes( IEnumMediaTypes **ppEnum )
//{
//    // Can only pass through if connected.
//    if( ! reinterpret_cast<CBaseVideoFilter*>(m_pFilter)->m_pInput->IsConnected() )
//        return VFW_E_NOT_CONNECTED;
//
//    return reinterpret_cast<CBaseVideoFilter*>(m_pFilter)->m_pInput->GetConnected()->EnumMediaTypes( ppEnum );
//
//} // EnumMediaTypes
