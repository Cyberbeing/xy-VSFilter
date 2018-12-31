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
#include <math.h>
#include <time.h>
#include "DirectVobSubFilter.h"
#include "TextInputPin.h"
#include "DirectVobSubPropPage.h"
#include "VSFilter.h"
#include "systray.h"
#include "../../../DSUtil/MediaTypes.h"
#include "../../../SubPic/SimpleSubPicProviderImpl.h"
#include "../../../SubPic/PooledSubPic.h"
#include "../../../subpic/SimpleSubPicWrapper.h"

#include <initguid.h>
#include "..\..\..\..\include\moreuuids.h"

#include "CAutoTiming.h"

#if ENABLE_XY_LOG_RENDERER_REQUEST
#  define TRACE_RENDERER_REQUEST(msg) XY_LOG_TRACE(msg)
#else
#  define TRACE_RENDERER_REQUEST(msg)
#endif

#define MAX_SUBPIC_QUEUE_LENGTH 1

///////////////////////////////////////////////////////////////////////////

/*removeme*/
bool g_RegOK = true;//false; // doesn't work with the dvd graph builder
#include "valami.cpp"

#if ENABLE_XY_LOG
EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#endif

using namespace DirectVobSubXyOptions;

////////////////////////////////////////////////////////////////////////////
//
// Constructor
//

CDirectVobSubFilter::CDirectVobSubFilter(LPUNKNOWN punk, HRESULT* phr, const GUID& clsid)
    : CDirectVobSub(DirectVobFilterOptions, &m_csSubLock)
    , CBaseVideoFilter(NAME("CDirectVobSubFilter"), punk, phr, clsid)
    , m_nSubtitleId(-1)
    , m_fMSMpeg4Fix(false)
    , m_fps(25)
{
    m_xy_str_opt[STRING_NAME] = L"DirectVobSubFilter";

    XY_LOG_INFO("Construct CDirectVobSubFilter");

    // and then, anywhere you need it:

    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    ZeroObj4OSD();

    theApp.WriteProfileString(ResStr(IDS_R_DEFTEXTPATHES), _T("Hint"), _T("The first three are fixed, but you can add more up to ten entries."));

    CString tmp;
    tmp.Format(ResStr(IDS_RP_PATH), 0);
    theApp.WriteProfileString(ResStr(IDS_R_DEFTEXTPATHES), tmp, _T("."));
    tmp.Format(ResStr(IDS_RP_PATH), 1);
    theApp.WriteProfileString(ResStr(IDS_R_DEFTEXTPATHES), tmp, _T(".\\subtitles"));
    tmp.Format(ResStr(IDS_RP_PATH), 2);
    theApp.WriteProfileString(ResStr(IDS_R_DEFTEXTPATHES), tmp, _T(".\\subs"));

    m_fLoading = true;

    m_tbid.WndCreatedEvent.Create(0, FALSE, FALSE, 0);
    m_hSystrayThread = 0;
    m_tbid.hSystrayWnd = NULL;
    m_tbid.graph = NULL;
    m_tbid.fRunOnce = false;
    m_tbid.fShowIcon = (theApp.m_AppName.Find(_T("zplayer"), 0) < 0 || m_xy_bool_opt[BOOL_ENABLE_ZP_ICON]);

    HRESULT hr = S_OK;
    m_pTextInput.Add(new CTextInputPin(this, m_pLock, &m_csSubLock, &hr));
    ASSERT(SUCCEEDED(hr));

    m_frd.ThreadStartedEvent.Create(0, FALSE, FALSE, 0);
    m_frd.EndThreadEvent.Create(0, FALSE, FALSE, 0);
    m_frd.RefreshEvent.Create(0, FALSE, FALSE, 0);
    CAMThread::Create();

    WaitForSingleObject(m_frd.ThreadStartedEvent, INFINITE);

    memset(&m_CurrentVIH2, 0, sizeof(VIDEOINFOHEADER2));

    m_donot_follow_upstream_preferred_order = !m_xy_bool_opt[BOOL_FOLLOW_UPSTREAM_PREFERRED_ORDER];

    m_video_yuv_matrix_decided_by_sub = ColorConvTable::NONE;
    m_video_yuv_range_decided_by_sub = ColorConvTable::RANGE_NONE;
}

CDirectVobSubFilter::~CDirectVobSubFilter()
{
    CAutoLock cAutoLock(&m_csSubLock);
    if(m_simple_provider)
    {
        m_simple_provider->Invalidate();
    }
    m_simple_provider = NULL;

    DeleteObj4OSD();

    for(size_t i = 0; i < m_pTextInput.GetCount(); i++)
        delete m_pTextInput[i];

    XY_LOG_INFO("Closing ThreadProc");
    m_frd.EndThreadEvent.Set();
    XY_LOG_INFO("EndThreadEvent set");
    CAMThread::Close();

    ::DeleteSystray(&m_hSystrayThread, &m_tbid);
    XY_LOG_INFO("Deconstructed");
}

STDMETHODIMP CDirectVobSubFilter::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
    CheckPointer(ppv, E_POINTER);

    return
		QI(IDirectVobSub)
		QI(IDirectVobSub2)
        QI(IXyOptions)
		QI(IFilterVersion)
		QI(ISpecifyPropertyPages)
		QI(IAMStreamSelect)
		__super::NonDelegatingQueryInterface(riid, ppv);
}

// CBaseVideoFilter

void CDirectVobSubFilter::GetOutputSize(int& w, int& h, int& arx, int& ary)
{
    XY_LOG_INFO("");
	CSize s(w, h), os = s;
	AdjustFrameSize(s);
	w = s.cx;
	h = s.cy;

	if(w != os.cx)
	{
		while(arx < 100) arx *= 10, ary *= 10;
		arx = arx * w / os.cx;
	}

	if(h != os.cy)
	{
		while(ary < 100) arx *= 10, ary *= 10;
		ary = ary * h / os.cy;
	}
}

HRESULT CDirectVobSubFilter::TryNotCopy(IMediaSample* pIn, const CMediaType& mt, const BITMAPINFOHEADER& bihIn )
{
    CSize sub(m_w, m_h);
    CSize in(bihIn.biWidth, bihIn.biHeight);
    BYTE* pDataIn = NULL;
    if(FAILED(pIn->GetPointer(&pDataIn)) || !pDataIn)
        return S_FALSE;
    if( sub==in )
    {
        m_spd.bits = pDataIn;
    }
    else
    {
        m_spd.bits = static_cast<BYTE*>(m_pTempPicBuff);
        bool fYV12 = (mt.subtype == MEDIASUBTYPE_YV12 || mt.subtype == MEDIASUBTYPE_I420 || mt.subtype == MEDIASUBTYPE_IYUV);	
        bool fNV12 = (mt.subtype == MEDIASUBTYPE_NV12 || mt.subtype == MEDIASUBTYPE_NV21);        
        bool fP010 = (mt.subtype == MEDIASUBTYPE_P010 || mt.subtype == MEDIASUBTYPE_P016);

        int bpp = fP010 ? 16 : (fYV12||fNV12) ? 8 : bihIn.biBitCount;
        DWORD black = fP010 ? 0x10001000 : (fYV12||fNV12) ? 0x10101010 : (bihIn.biCompression == '2YUY') ? 0x80108010 : 0;


        if(FAILED(Copy((BYTE*)m_pTempPicBuff, pDataIn, sub, in, bpp, mt.subtype, black)))
            return E_FAIL;

        if(fYV12)
        {
            BYTE* pSubV = (BYTE*)m_pTempPicBuff + (sub.cx*bpp>>3)*sub.cy;
            BYTE* pInV = pDataIn + (in.cx*bpp>>3)*in.cy;
            sub.cx >>= 1; sub.cy >>= 1; in.cx >>= 1; in.cy >>= 1;
            BYTE* pSubU = pSubV + (sub.cx*bpp>>3)*sub.cy;
            BYTE* pInU = pInV + (in.cx*bpp>>3)*in.cy;
            if(FAILED(Copy(pSubV, pInV, sub, in, bpp, mt.subtype, 0x80808080)))
                return E_FAIL;
            if(FAILED(Copy(pSubU, pInU, sub, in, bpp, mt.subtype, 0x80808080)))
                return E_FAIL;
        }
        else if (fP010)
        {
            BYTE* pSubUV = (BYTE*)m_pTempPicBuff + (sub.cx*bpp>>3)*sub.cy;
            BYTE* pInUV = pDataIn + (in.cx*bpp>>3)*in.cy;
            sub.cy >>= 1; in.cy >>= 1;
            if(FAILED(Copy(pSubUV, pInUV, sub, in, bpp, mt.subtype, 0x80008000)))
                return E_FAIL;
        }
        else if(fNV12) {
            BYTE* pSubUV = (BYTE*)m_pTempPicBuff + (sub.cx*bpp>>3)*sub.cy;
            BYTE* pInUV = pDataIn + (in.cx*bpp>>3)*in.cy;
            sub.cy >>= 1;
            in.cy >>= 1;
            if(FAILED(Copy(pSubUV, pInUV, sub, in, bpp, mt.subtype, 0x80808080)))
                return E_FAIL;
        }
    }    
    return S_OK;
}

HRESULT CDirectVobSubFilter::Transform(IMediaSample* pIn)
{
	HRESULT hr;

	REFERENCE_TIME rtStart, rtStop;
	if(SUCCEEDED(pIn->GetTime(&rtStart, &rtStop)))
	{
		double dRate = m_pInput->CurrentRate();

		m_tPrev = m_pInput->CurrentStartTime() + dRate*rtStart;

		REFERENCE_TIME rtAvgTimePerFrame = rtStop - rtStart;
		if(CComQIPtr<ISubClock2> pSC2 = m_pSubClock)
		{
			REFERENCE_TIME rt;
			 if(S_OK == pSC2->GetAvgTimePerFrame(&rt))
				 rtAvgTimePerFrame = rt;
		}

		m_fps = 10000000.0/rtAvgTimePerFrame / dRate;
	}

	//

	{
		CAutoLock cAutoLock(&m_csSubLock);

		if(m_simple_provider)
		{
			m_simple_provider->SetTime(CalcCurrentTime());
			m_simple_provider->SetFPS(m_fps);
		}
	}

	//


	const CMediaType& mt = m_pInput->CurrentMediaType();

	BITMAPINFOHEADER bihIn;
	ExtractBIH(&mt, &bihIn);

    hr = TryNotCopy(pIn, mt, bihIn); 
    if( hr!=S_OK )
    {
        //fix me: log error
        return hr;
    }
	//

	SubPicDesc spd = m_spd;

	CComPtr<IMediaSample> pOut;
	BYTE* pDataOut = NULL;
	if(FAILED(hr = GetDeliveryBuffer(spd.w, spd.h, &pOut))
	|| FAILED(hr = pOut->GetPointer(&pDataOut)))
		return hr;
	pOut->SetTime(&rtStart, &rtStop);
	pOut->SetMediaTime(NULL, NULL);
	pOut->SetDiscontinuity(pIn->IsDiscontinuity() == S_OK);
	pOut->SetSyncPoint(pIn->IsSyncPoint() == S_OK);
	pOut->SetPreroll(pIn->IsPreroll() == S_OK);
	//

	BITMAPINFOHEADER bihOut;
	ExtractBIH(&m_pOutput->CurrentMediaType(), &bihOut);

	bool fInputFlipped = bihIn.biHeight >= 0 && bihIn.biCompression <= 3;
	bool fOutputFlipped = bihOut.biHeight >= 0 && bihOut.biCompression <= 3;

	bool fFlip = fInputFlipped != fOutputFlipped;
    if(m_xy_bool_opt[BOOL_FLIP_PICTURE]) fFlip = !fFlip;
	if(m_fMSMpeg4Fix) fFlip = !fFlip;

	bool fFlipSub = fOutputFlipped;
    if(m_xy_bool_opt[BOOL_FLIP_SUBTITLE]) fFlipSub = !fFlipSub;

	//

    {
        CAutoLock cAutoLock(&m_csSubLock);

        if(m_simple_provider)
        {
            CComPtr<ISimpleSubPic> pSubPic;
            REFERENCE_TIME rt = CalcCurrentTime();
            TRACE_RENDERER_REQUEST("LookupSubPic for "<<rt);
            bool lookupResult = m_simple_provider->LookupSubPic(rt, &pSubPic);
            if(lookupResult && pSubPic)
            {
                if(fFlip ^ fFlipSub)
                    spd.h = -spd.h;
                TRACE_RENDERER_REQUEST("AlphaBlt");
                pSubPic->AlphaBlt(&spd);
                TRACE_RENDERER_REQUEST("AlphaBlt finished");
            }
        }
    }
    CopyBuffer(pDataOut, (BYTE*)spd.bits, spd.w, abs(spd.h)*(fFlip?-1:1), spd.pitch, mt.subtype);

    PrintMessages(pDataOut);
    return m_pOutput->Deliver(pOut);
}

// CBaseFilter

CBasePin* CDirectVobSubFilter::GetPin(int n)
{
	if(n < __super::GetPinCount())
		return __super::GetPin(n);

	n -= __super::GetPinCount();

	if(n >= 0 && n < m_pTextInput.GetCount())
		return m_pTextInput[n];

	n -= m_pTextInput.GetCount();

	return NULL;
}

int CDirectVobSubFilter::GetPinCount()
{
	return __super::GetPinCount() + m_pTextInput.GetCount();
}

HRESULT CDirectVobSubFilter::JoinFilterGraph(IFilterGraph* pGraph, LPCWSTR pName)
{
    XY_LOG_INFO("pGraph:"<<(void*)pGraph<<" pName:"<<(pName?pName:L"NULL"));
	if(pGraph)
	{
		AFX_MANAGE_STATE(AfxGetStaticModuleState());

		if(!theApp.GetProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_SEENDIVXWARNING), 0))
		{
			unsigned __int64 ver = GetFileVersion(_T("divx_c32.ax"));
			if(((ver >> 48)&0xffff) == 4 && ((ver >> 32)&0xffff) == 2)
			{
				DWORD dwVersion = GetVersion();
				DWORD dwWindowsMajorVersion = (DWORD)(LOBYTE(LOWORD(dwVersion)));
				DWORD dwWindowsMinorVersion = (DWORD)(HIBYTE(LOWORD(dwVersion)));

				if(dwVersion < 0x80000000 && dwWindowsMajorVersion >= 5)
				{
					AfxMessageBox(IDS_DIVX_WARNING);
					theApp.WriteProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_SEENDIVXWARNING), 1);
				}
			}
		}

		/*removeme*/
		if(!g_RegOK)
		{
			DllRegisterServer();
			g_RegOK = true;
		}
	}
	else
	{
        ::DeleteSystray(&m_hSystrayThread, &m_tbid);
	}

	return __super::JoinFilterGraph(pGraph, pName);
}

STDMETHODIMP CDirectVobSubFilter::QueryFilterInfo(FILTER_INFO* pInfo)
{
    CheckPointer(pInfo, E_POINTER);
    ValidateReadWritePtr(pInfo, sizeof(FILTER_INFO));

	if(!get_Forced())
		return __super::QueryFilterInfo(pInfo);

	wcscpy(pInfo->achName, L"DirectVobSub (forced auto-loading version)");
	if(pInfo->pGraph = m_pGraph) m_pGraph->AddRef();

	return S_OK;
}

// CTransformFilter

HRESULT CDirectVobSubFilter::SetMediaType(PIN_DIRECTION dir, const CMediaType* pmt)
{
    XY_LOG_INFO(XY_LOG_VAR_2_STR(dir)<<XY_LOG_VAR_2_STR(pmt));
	HRESULT hr = __super::SetMediaType(dir, pmt);
	if(FAILED(hr)) return hr;

	if(dir == PINDIR_INPUT)
	{
		CAutoLock cAutoLock(&m_csReceive);

		REFERENCE_TIME atpf =
			pmt->formattype == FORMAT_VideoInfo ? ((VIDEOINFOHEADER*)pmt->Format())->AvgTimePerFrame :
			pmt->formattype == FORMAT_VideoInfo2 ? ((VIDEOINFOHEADER2*)pmt->Format())->AvgTimePerFrame :
			0;

		m_fps = atpf ? 10000000.0 / atpf : 25;

		if (pmt->formattype == FORMAT_VideoInfo2)
			m_CurrentVIH2 = *(VIDEOINFOHEADER2*)pmt->Format();

		InitSubPicQueue();
	}
	else if(dir == PINDIR_OUTPUT)
	{

	}

	return hr;
}

HRESULT CDirectVobSubFilter::CheckConnect(PIN_DIRECTION dir, IPin* pPin)
{
    XY_LOG_INFO(XY_LOG_VAR_2_STR(dir)<<XY_LOG_VAR_2_STR(pPin));
	if(dir == PINDIR_INPUT)
	{
	}
	else if(dir == PINDIR_OUTPUT)
	{
		/*removeme*/
		if(HmGyanusVagyTeNekem(pPin)) return(E_FAIL);
	}

	return __super::CheckConnect(dir, pPin);
}

HRESULT CDirectVobSubFilter::CompleteConnect(PIN_DIRECTION dir, IPin* pReceivePin)
{
    XY_LOG_INFO(XY_LOG_VAR_2_STR(dir)<<XY_LOG_VAR_2_STR(pReceivePin));
    bool reconnected = false;
    if(dir == PINDIR_INPUT)
    {
        DumpGraph(m_pGraph,0);
        CComPtr<IBaseFilter> pFilter;

        // needed when we have a decoder with a version number of 3.x
        if(SUCCEEDED(m_pGraph->FindFilterByName(L"DivX MPEG-4 DVD Video Decompressor ", &pFilter))
            && (GetFileVersion(_T("divx_c32.ax")) >> 48) <= 4
            || SUCCEEDED(m_pGraph->FindFilterByName(L"Microcrap MPEG-4 Video Decompressor", &pFilter))
            || SUCCEEDED(m_pGraph->FindFilterByName(L"Microsoft MPEG-4 Video Decompressor", &pFilter))
            && (GetFileVersion(_T("mpg4ds32.ax")) >> 48) <= 3)
        {
            m_fMSMpeg4Fix = true;
        }
    }
    else if(dir == PINDIR_OUTPUT)
    {
        DumpGraph(m_pGraph,0);
        const CMediaType* mtIn = &(m_pInput->CurrentMediaType());
        const CMediaType* mtOut = &(m_pOutput->CurrentMediaType());
        CMediaType desiredMt;
        int position = 0;
        HRESULT hr;

        bool can_reconnect = false;
        bool can_transform = (CheckTransform(mtIn, mtOut)==S_OK);
        if( mtIn->subtype!=mtOut->subtype )
        {
            position = GetOutputSubtypePosition(mtOut->subtype);
            if(position>=0)
            {
                hr = GetMediaType(position, &desiredMt);
                if (hr!=S_OK)
                {
                    XY_LOG_ERROR("Unexpected error when GetMediaType."<<XY_LOG_VAR_2_STR(position));
                }
                else
                {
                    hr = CheckTransform(&desiredMt, mtOut);
                    if (hr!=S_OK)
                    {
                        DbgLog((LOG_TRACE, 3, TEXT("Transform not accept:")));
                        DisplayType(0,&desiredMt);
                        DisplayType(0,mtOut);
                    }
                    else
                    {
                        hr = m_pInput->GetConnected()->QueryAccept(&desiredMt);
                        if(hr!=S_OK)
                        {
                            DbgLog((LOG_TRACE, 3, TEXT("Upstream not accept:")));
                            DisplayType(0, &desiredMt);
                        }
                        else
                        {
                            can_reconnect = true;
                            XY_LOG_ERROR("Can use the same subtype!");
                        }
                    }
                }
            }
            else
            {
                XY_LOG_ERROR("Cannot use the same subtype!");
            }
        }
        if ( can_reconnect )
        {
            if (SUCCEEDED(ReconnectPin(m_pInput, &desiredMt)))
            {
                reconnected = true;
                //m_pInput->SetMediaType(&desiredMt);
                XY_LOG_INFO(_T("Reconnect succeeded!")<<GuidNames[desiredMt.subtype]);
            }
            else
            {
                XY_LOG_ERROR("Failed to reconnect input pin!"<<GuidNames[desiredMt.subtype]);
                return E_FAIL;
            }
        }
        else if(!can_transform)
        {
            XY_LOG_ERROR("Can neither transform with the current in/out media type or reconnect with out media type!"
                <<XY_LOG_VAR_2_STR(GuidNames[mtIn->subtype])<<XY_LOG_VAR_2_STR(GuidNames[mtOut->subtype]));
            if(m_pInput->IsConnected())
            {
                m_pInput->GetConnected()->Disconnect();
                m_pInput->Disconnect();
            }
            if(m_pOutput->IsConnected())
            {
                m_pOutput->GetConnected()->Disconnect();
                m_pOutput->Disconnect();
            }
            return VFW_E_TYPE_NOT_ACCEPTED;
        }
    }
    if (!reconnected && m_pOutput->IsConnected())
    {
        if(!m_hSystrayThread && !m_xy_bool_opt[BOOL_HIDE_TRAY_ICON])
        {
            m_tbid.graph = m_pGraph;
            m_tbid.dvs = this;

            m_hSystrayThread = ::CreateSystray(&m_tbid);
            XY_LOG_INFO("Systray thread created "<<m_hSystrayThread);
        }
        m_pInput->SetMediaType( &m_pInput->CurrentMediaType() );
    }

    HRESULT hr = __super::CompleteConnect(dir, pReceivePin);
    XY_LOG_INFO(_T("Connection fininshed!")<<XY_LOG_VAR_2_STR(hr));
    DumpGraph(m_pGraph,0);
    return hr;
}

HRESULT CDirectVobSubFilter::BreakConnect(PIN_DIRECTION dir)
{
    XY_LOG_INFO(dir);
	if(dir == PINDIR_INPUT)
	{
        if (m_pInput->IsConnected()) {
            m_inputFmtCount = -1;
            m_outputFmtCount = -1;
        }
		//if(m_pOutput->IsConnected())
		//{
		//	m_pOutput->GetConnected()->Disconnect();
		//	m_pOutput->Disconnect();
		//}
        m_fMSMpeg4Fix = false;
	}
	else if(dir == PINDIR_OUTPUT)
	{
        if (m_pOutput->IsConnected()) {
            m_outputFmtCount = -1;
        }
		// not really needed, but may free up a little memory
		CAutoLock cAutoLock(&m_csSubLock);
		m_simple_provider = NULL;
	}

	return __super::BreakConnect(dir);
}

HRESULT CDirectVobSubFilter::StartStreaming()
{
    XY_LOG_INFO("");
    /* WARNING: calls to m_pGraph member functions from within this function will generate deadlock with Haali
     * Video Renderer in MPC. Reason is that CAutoLock's variables in IFilterGraph functions are overriden by
     * CFGManager class.
     */

    HRESULT hr = NOERROR;
    m_fLoading = false;

    InitSubPicQueue();

    m_tbid.fRunOnce = true;

    return __super::StartStreaming();
}

HRESULT CDirectVobSubFilter::StopStreaming()
{
    XY_LOG_INFO("");
    InvalidateSubtitle();
    return __super::StopStreaming();
}

HRESULT CDirectVobSubFilter::NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate)
{
    TRACE_RENDERER_REQUEST(XY_LOG_VAR_2_STR(tStart)<<XY_LOG_VAR_2_STR(tStop)<<XY_LOG_VAR_2_STR(dRate));
    m_tPrev = tStart;
    return __super::NewSegment(tStart, tStop, dRate);
}

//

REFERENCE_TIME CDirectVobSubFilter::CalcCurrentTime()
{
	REFERENCE_TIME rt = m_pSubClock ? m_pSubClock->GetTime() : m_tPrev;
	return (rt - 10000i64*m_SubtitleDelay) * m_SubtitleSpeedMul / m_SubtitleSpeedDiv; // no, it won't overflow if we use normal parameters (__int64 is enough for about 2000 hours if we multiply it by the max: 65536 as m_SubtitleSpeedMul)
}

void CDirectVobSubFilter::InitSubPicQueue()
{
    XY_LOG_INFO("");
    CAutoLock cAutoLock(&m_csSubLock);

    CacheManager::GetPathDataMruCache()->SetMaxItemNum(m_xy_int_opt[INT_PATH_DATA_CACHE_MAX_ITEM_NUM]);
    CacheManager::GetScanLineData2MruCache()->SetMaxItemNum(m_xy_int_opt[INT_SCAN_LINE_DATA_CACHE_MAX_ITEM_NUM]);
    CacheManager::GetOverlayNoBlurMruCache()->SetMaxItemNum(m_xy_int_opt[INT_OVERLAY_NO_BLUR_CACHE_MAX_ITEM_NUM]);
    CacheManager::GetOverlayMruCache()->SetMaxItemNum(m_xy_int_opt[INT_OVERLAY_CACHE_MAX_ITEM_NUM]);

    XyFwGroupedDrawItemsHashKey::GetCacher()->SetMaxItemNum(m_xy_int_opt[INT_BITMAP_MRU_CACHE_ITEM_NUM]);
    CacheManager::GetBitmapMruCache()->SetMaxItemNum(m_xy_int_opt[INT_BITMAP_MRU_CACHE_ITEM_NUM]);

    CacheManager::GetClipperAlphaMaskMruCache()->SetMaxItemNum(m_xy_int_opt[INT_CLIPPER_MRU_CACHE_ITEM_NUM]);
    CacheManager::GetTextInfoCache()->SetMaxItemNum(m_xy_int_opt[INT_TEXT_INFO_CACHE_ITEM_NUM]);
    CacheManager::GetAssTagListMruCache()->SetMaxItemNum(m_xy_int_opt[INT_ASS_TAG_LIST_CACHE_ITEM_NUM]);

    SubpixelPositionControler::GetGlobalControler().SetSubpixelLevel( static_cast<SubpixelPositionControler::SUBPIXEL_LEVEL>(m_xy_int_opt[INT_SUBPIXEL_POS_LEVEL]) );

	m_simple_provider = NULL;

	const GUID& subtype = m_pInput->CurrentMediaType().subtype;

	BITMAPINFOHEADER bihIn;
	ExtractBIH(&m_pInput->CurrentMediaType(), &bihIn);

	m_spd.type = -1;

	if(subtype == MEDIASUBTYPE_YV12) m_spd.type = MSP_YV12;
    else if(subtype == MEDIASUBTYPE_P010) m_spd.type = MSP_P010;
    else if(subtype == MEDIASUBTYPE_P016) m_spd.type = MSP_P016;
	else if(subtype == MEDIASUBTYPE_I420 || subtype == MEDIASUBTYPE_IYUV) m_spd.type = MSP_IYUV;
	else if(subtype == MEDIASUBTYPE_YUY2) m_spd.type = MSP_YUY2;
	else if(subtype == MEDIASUBTYPE_RGB32) m_spd.type = MSP_RGB32;
	else if(subtype == MEDIASUBTYPE_RGB24) m_spd.type = MSP_RGB24;
	else if(subtype == MEDIASUBTYPE_RGB565) m_spd.type = MSP_RGB16;
	else if(subtype == MEDIASUBTYPE_RGB555) m_spd.type = MSP_RGB15;
    else if(subtype == MEDIASUBTYPE_NV12) m_spd.type = MSP_NV12;
    else if(subtype == MEDIASUBTYPE_NV21) m_spd.type = MSP_NV21;
    else if(subtype == MEDIASUBTYPE_AYUV) m_spd.type = MSP_AYUV;

	m_spd.w = m_w;
	m_spd.h = m_h;
	m_spd.bpp = (m_spd.type == MSP_P010 || m_spd.type == MSP_P016) ? 16 :
        (m_spd.type == MSP_YV12 || m_spd.type == MSP_IYUV || m_spd.type == MSP_NV12 || m_spd.type == MSP_NV21) ? 8 : bihIn.biBitCount;
	m_spd.pitch = m_spd.w*m_spd.bpp>>3;

    m_pTempPicBuff.Free();
    if(m_spd.type == MSP_YV12 || m_spd.type == MSP_IYUV || m_spd.type == MSP_NV12 || m_spd.type == MSP_NV21)
        m_pTempPicBuff.Allocate(4*m_spd.pitch*m_spd.h);//fix me: can be smaller
    else if(m_spd.type == MSP_P010 || m_spd.type == MSP_P016)
        m_pTempPicBuff.Allocate(m_spd.pitch*m_spd.h+m_spd.pitch*m_spd.h/2);
    else
        m_pTempPicBuff.Allocate(m_spd.pitch*m_spd.h);
	m_spd.bits = (BYTE*)m_pTempPicBuff;

	CSize video(bihIn.biWidth, bihIn.biHeight), window = video;
	if(AdjustFrameSize(window)) video += video;
	ASSERT(window == CSize(m_w, m_h));
    CRect video_rect(CPoint((window.cx - video.cx)/2, (window.cy - video.cy)/2), video);

	HRESULT hr = S_OK;
	//m_pSubPicQueue = m_fDoPreBuffering
	//	? (ISubPicQueue*)new CSubPicQueue(MAX_SUBPIC_QUEUE_LENGTH, pSubPicAllocator, &hr)
	//	: (ISubPicQueue*)new CSubPicQueueNoThread(pSubPicAllocator, &hr);
    m_simple_provider = new SimpleSubPicProvider2(m_spd.type, CSize(m_w, m_h), window, video_rect, this, &hr);

	if(FAILED(hr)) m_simple_provider = NULL;

    m_xy_size_opt[SIZE_ORIGINAL_VIDEO] = CSize(m_w, m_h);
	UpdateSubtitle(false);

	InitObj4OSD();
}

bool CDirectVobSubFilter::AdjustFrameSize(CSize& s)
{
	int horizontal, vertical, resx2, resx2minw, resx2minh;
	get_ExtendPicture(&horizontal, &vertical, &resx2, &resx2minw, &resx2minh);

	bool fRet = (resx2 == 1) || (resx2 == 2 && s.cx*s.cy <= resx2minw*resx2minh);

	if(fRet)
	{
		s.cx <<= 1;
		s.cy <<= 1;
	}

	int h;
	switch(vertical&0x7f)
	{
	case 1:
		h = s.cx * 9 / 16;
		if(s.cy < h || !!(vertical&0x80)) s.cy = (h + 3) & ~3;
		break;
	case 2:
		h = s.cx * 3 / 4;
		if(s.cy < h || !!(vertical&0x80)) s.cy = (h + 3) & ~3;
		break;
	case 3:
		h = 480;
		if(s.cy < h || !!(vertical&0x80)) s.cy = (h + 3) & ~3;
		break;
	case 4:
		h = 576;
		if(s.cy < h || !!(vertical&0x80)) s.cy = (h + 3) & ~3;
		break;
	}

	if(horizontal == 1)
	{
		s.cx = (s.cx + 31) & ~31;
		s.cy = (s.cy + 1) & ~1;
	}

	return(fRet);
}

STDMETHODIMP CDirectVobSubFilter::Count(DWORD* pcStreams)
{
	if(!pcStreams) return E_POINTER;

	*pcStreams = 0;

	int nLangs = 0;
	if(SUCCEEDED(get_LanguageCount(&nLangs)))
		(*pcStreams) += nLangs;

	(*pcStreams) += 2; // enable ... disable

	(*pcStreams) += 2; // normal flipped

	return S_OK;
}

#define MAXPREFLANGS 5

int CDirectVobSubFilter::FindPreferedLanguage(bool fHideToo)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    HRESULT hr = NOERROR;

    int nLangs = 0;
    hr = get_LanguageCount(&nLangs);
    CHECK_N_LOG(hr, "Failed to get option");

    if(nLangs <= 0) {
        XY_LOG_WARN("There is NO languages yet");
        return(0);
    }

	for(int i = 0; i < MAXPREFLANGS; i++)
	{
		CString tmp;
		tmp.Format(IDS_RL_LANG, i);

		CString lang = theApp.GetProfileString(ResStr(IDS_R_PREFLANGS), tmp);

		if(!lang.IsEmpty())
		{
			for(int ret = 0; ret < nLangs; ret++)
			{
				CString l;
				WCHAR* pName = NULL;
				get_LanguageName(ret, &pName);
				l = pName;
				CoTaskMemFree(pName);

				if(!l.CompareNoCase(lang)) return(ret);
			}
		}
	}

	return(0);
}

void CDirectVobSubFilter::UpdatePreferedLanguages(CString l)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	CString langs[MAXPREFLANGS+1];

	int i = 0, j = 0, k = -1;
	for(; i < MAXPREFLANGS; i++)
	{
		CString tmp;
		tmp.Format(IDS_RL_LANG, i);

		langs[j] = theApp.GetProfileString(ResStr(IDS_R_PREFLANGS), tmp);

		if(!langs[j].IsEmpty())
		{
			if(!langs[j].CompareNoCase(l)) k = j;
			j++;
		}
	}

	if(k == -1)
	{
		langs[k = j] = l;
		j++;
	}

	// move the selected to the top of the list

	while(k > 0)
	{
		CString tmp = langs[k]; langs[k] = langs[k-1]; langs[k-1] = tmp;
		k--;
	}

	// move "Hide subtitles" to the last position if it wasn't our selection

	CString hidesubs;
	hidesubs.LoadString(IDS_M_HIDESUBTITLES);

	for(k = 1; k < j; k++)
	{
		if(!langs[k].CompareNoCase(hidesubs)) break;
	}

	while(k < j-1)
	{
		CString tmp = langs[k]; langs[k] = langs[k+1]; langs[k+1] = tmp;
		k++;
	}

	for(i = 0; i < j; i++)
	{
		CString tmp;
		tmp.Format(IDS_RL_LANG, i);

		theApp.WriteProfileString(ResStr(IDS_R_PREFLANGS), tmp, langs[i]);
	}
}

STDMETHODIMP CDirectVobSubFilter::Enable(long lIndex, DWORD dwFlags)
{
    HRESULT hr = NOERROR;
    if(!(dwFlags & AMSTREAMSELECTENABLE_ENABLE))
        return E_NOTIMPL;

    int nLangs = 0;
    hr = get_LanguageCount(&nLangs);
    CHECK_N_LOG(hr, "Failed to get option");

    if(!(lIndex >= 0 && lIndex < nLangs+2+2))
        return E_INVALIDARG;

    int i = lIndex-1;

    if(i == -1 && !m_fLoading) // we need this because when loading something stupid media player pushes the first stream it founds, which is "enable" in our case
    {
        hr = put_HideSubtitles(false);
    }
    else if(i >= 0 && i < nLangs)
    {
        hr = put_HideSubtitles(false);
        CHECK_N_LOG(hr, "Failed to get option");
        hr = put_SelectedLanguage(i);
        CHECK_N_LOG(hr, "Failed to get option");

        WCHAR* pName = NULL;
        hr = get_LanguageName(i, &pName);
        if(SUCCEEDED(hr))
        {
            UpdatePreferedLanguages(CString(pName));
            if(pName) CoTaskMemFree(pName);
        }
    }
    else if(i == nLangs && !m_fLoading)
    {
        hr = put_HideSubtitles(true);
    }
    else if((i == nLangs+1 || i == nLangs+2) && !m_fLoading)
    {
        hr = put_Flip(i == nLangs+2, m_xy_bool_opt[BOOL_FLIP_SUBTITLE]);
    }
    CHECK_N_LOG(hr, "Failed "<<XY_LOG_VAR_2_STR(lIndex)<<XY_LOG_VAR_2_STR(dwFlags));
    return S_OK;
}

STDMETHODIMP CDirectVobSubFilter::Info(long lIndex, AM_MEDIA_TYPE** ppmt, DWORD* pdwFlags, LCID* plcid, DWORD* pdwGroup, WCHAR** ppszName, IUnknown** ppObject, IUnknown** ppUnk)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
    const int FLAG_CMD = 1;
    const int FLAG_EXTERNAL_SUB = 2;
    const int FLAG_PICTURE_CMD = 4;
    const int FLAG_VISIBILITY_CMD = 8;
    
    const int GROUP_NUM_BASE = 0x648E40;//random number

	int nLangs = 0;
	get_LanguageCount(&nLangs);

	if(!(lIndex >= 0 && lIndex < nLangs+2+2))
		return E_INVALIDARG;

	int i = lIndex-1;

	if(ppmt) *ppmt = CreateMediaType(&m_pInput->CurrentMediaType());

	if(pdwFlags)
	{
		*pdwFlags = 0;

		if(i == -1 && !m_xy_bool_opt[BOOL_HIDE_SUBTITLES]
		|| i >= 0 && i < nLangs && i == m_xy_int_opt[INT_SELECTED_LANGUAGE]
		|| i == nLangs && m_xy_bool_opt[BOOL_HIDE_SUBTITLES]
		|| i == nLangs+1 && !m_xy_bool_opt[BOOL_FLIP_PICTURE]
		|| i == nLangs+2 && m_xy_bool_opt[BOOL_FLIP_PICTURE])
		{
			*pdwFlags |= AMSTREAMSELECTINFO_ENABLED;
		}
	}

	if(plcid) *plcid = 0;

	if(pdwGroup)
    {
        *pdwGroup = GROUP_NUM_BASE;
        if(i == -1)
        {
            *pdwGroup = GROUP_NUM_BASE | FLAG_CMD | FLAG_VISIBILITY_CMD;
        }
        else if(i >= 0 && i < nLangs)
        {
            bool isEmbedded = false;
            if( SUCCEEDED(GetIsEmbeddedSubStream(i, &isEmbedded)) )
            {
                if(isEmbedded)
                {
                    *pdwGroup = GROUP_NUM_BASE & ~(FLAG_CMD | FLAG_EXTERNAL_SUB);
                }
                else
                {
                    *pdwGroup = (GROUP_NUM_BASE & ~FLAG_CMD) | FLAG_EXTERNAL_SUB;
                }
            }            
        }
        else if(i == nLangs)
        {
            *pdwGroup = GROUP_NUM_BASE | FLAG_CMD | FLAG_VISIBILITY_CMD;
        }
        else if(i == nLangs+1)
        {
            *pdwGroup = GROUP_NUM_BASE | FLAG_CMD | FLAG_PICTURE_CMD;
        }
        else if(i == nLangs+2)
        {
            *pdwGroup = GROUP_NUM_BASE | FLAG_CMD | FLAG_PICTURE_CMD;
        }
    }

	if(ppszName)
	{
		*ppszName = NULL;

		CStringW str;
		if(i == -1) str = ResStr(IDS_M_SHOWSUBTITLES);
		else if(i >= 0 && i < nLangs)
        {
            get_LanguageName(i, ppszName);
        }
		else if(i == nLangs)
        {
            str = ResStr(IDS_M_HIDESUBTITLES);
        }
		else if(i == nLangs+1)
        {
            str = ResStr(IDS_M_ORIGINALPICTURE);
        }
		else if(i == nLangs+2)
        {
            str = ResStr(IDS_M_FLIPPEDPICTURE);
        }

		if(!str.IsEmpty())
		{
			*ppszName = (WCHAR*)CoTaskMemAlloc((str.GetLength()+1)*sizeof(WCHAR));
			if(*ppszName == NULL) return S_FALSE;
			wcscpy(*ppszName, str);
		}
	}

	if(ppObject) *ppObject = NULL;

	if(ppUnk) *ppUnk = NULL;

	return S_OK;
}

STDMETHODIMP CDirectVobSubFilter::GetClassID(CLSID* pClsid)
{
    if(pClsid == NULL) return E_POINTER;
	*pClsid = m_clsid;
    return NOERROR;
}

STDMETHODIMP CDirectVobSubFilter::GetPages(CAUUID* pPages)
{
    CheckPointer(pPages, E_POINTER);

	pPages->cElems = 8;
    pPages->pElems = (GUID*)CoTaskMemAlloc(sizeof(GUID)*pPages->cElems);

	if(pPages->pElems == NULL) return E_OUTOFMEMORY;

	int i = 0;
    pPages->pElems[i++] = __uuidof(CDVSMainPPage);
    pPages->pElems[i++] = __uuidof(CDVSGeneralPPage);
    pPages->pElems[i++] = __uuidof(CDVSMiscPPage);
    pPages->pElems[i++] = __uuidof(CDVSMorePPage);
    pPages->pElems[i++] = __uuidof(CDVSTimingPPage);
    pPages->pElems[i++] = __uuidof(CDVSColorPPage);
    pPages->pElems[i++] = __uuidof(CDVSPathsPPage);
    pPages->pElems[i++] = __uuidof(CDVSAboutPPage);

    return NOERROR;
}

// IDirectVobSub
STDMETHODIMP CDirectVobSubFilter::get_LanguageName(int iLanguage, WCHAR** ppName)
{
    XY_LOG_INFO(iLanguage);
	HRESULT hr = CDirectVobSub::get_LanguageName(iLanguage, ppName);

	if(!ppName) return E_POINTER;

	if(hr == NOERROR)
	{
        CAutoLock cAutolock(&m_csSubLock);

		hr = E_INVALIDARG;

		int i = iLanguage;

		POSITION pos = m_pSubStreams.GetHeadPosition();
		while(i >= 0 && pos)
		{
			CComPtr<ISubStream> pSubStream = m_pSubStreams.GetNext(pos);

			if(i < pSubStream->GetStreamCount())
			{
				pSubStream->GetStreamInfo(i, ppName, NULL);
				hr = NOERROR;
				break;
			}

			i -= pSubStream->GetStreamCount();
		}
	}

	return hr;
}

STDMETHODIMP CDirectVobSubFilter::put_PreBuffering(bool fDoPreBuffering)
{
    XY_LOG_INFO(fDoPreBuffering);
	HRESULT hr = CDirectVobSub::put_PreBuffering(fDoPreBuffering);

	if(hr == NOERROR)
	{
		InitSubPicQueue();
	}

	return hr;
}

STDMETHODIMP CDirectVobSubFilter::get_CachesInfo(CachesInfo* caches_info)
{
    XY_LOG_INFO(caches_info);
    CheckPointer(caches_info, S_FALSE);
    CAutoLock cAutoLock(&m_csSubLock);

    caches_info->path_cache_cur_item_num    = CacheManager::GetPathDataMruCache()->GetCurItemNum();
    caches_info->path_cache_hit_count       = CacheManager::GetPathDataMruCache()->GetCacheHitCount();
    caches_info->path_cache_query_count     = CacheManager::GetPathDataMruCache()->GetQueryCount();
    caches_info->scanline_cache2_cur_item_num= CacheManager::GetScanLineData2MruCache()->GetCurItemNum();
    caches_info->scanline_cache2_hit_count   = CacheManager::GetScanLineData2MruCache()->GetCacheHitCount();
    caches_info->scanline_cache2_query_count = CacheManager::GetScanLineData2MruCache()->GetQueryCount();
    caches_info->non_blur_cache_cur_item_num= CacheManager::GetOverlayNoBlurMruCache()->GetCurItemNum();
    caches_info->non_blur_cache_hit_count   = CacheManager::GetOverlayNoBlurMruCache()->GetCacheHitCount();
    caches_info->non_blur_cache_query_count = CacheManager::GetOverlayNoBlurMruCache()->GetQueryCount();
    caches_info->overlay_cache_cur_item_num = CacheManager::GetOverlayMruCache()->GetCurItemNum();
    caches_info->overlay_cache_hit_count    = CacheManager::GetOverlayMruCache()->GetCacheHitCount();
    caches_info->overlay_cache_query_count  = CacheManager::GetOverlayMruCache()->GetQueryCount();

    caches_info->bitmap_cache_cur_item_num  = CacheManager::GetBitmapMruCache()->GetCurItemNum();
    caches_info->bitmap_cache_hit_count     = CacheManager::GetBitmapMruCache()->GetCacheHitCount();
    caches_info->bitmap_cache_query_count   = CacheManager::GetBitmapMruCache()->GetQueryCount();

    caches_info->interpolate_cache_cur_item_num = CacheManager::GetSubpixelVarianceCache()->GetCurItemNum();
    caches_info->interpolate_cache_hit_count    = CacheManager::GetSubpixelVarianceCache()->GetCacheHitCount();
    caches_info->interpolate_cache_query_count  = CacheManager::GetSubpixelVarianceCache()->GetQueryCount();    
    caches_info->text_info_cache_cur_item_num   = CacheManager::GetTextInfoCache()->GetCurItemNum();
    caches_info->text_info_cache_hit_count      = CacheManager::GetTextInfoCache()->GetCacheHitCount();
    caches_info->text_info_cache_query_count    = CacheManager::GetTextInfoCache()->GetQueryCount();
    
    caches_info->word_info_cache_cur_item_num   = CacheManager::GetAssTagListMruCache()->GetCurItemNum();
    caches_info->word_info_cache_hit_count      = CacheManager::GetAssTagListMruCache()->GetCacheHitCount();
    caches_info->word_info_cache_query_count    = CacheManager::GetAssTagListMruCache()->GetQueryCount();    

    caches_info->scanline_cache_cur_item_num = CacheManager::GetScanLineDataMruCache()->GetCurItemNum();
    caches_info->scanline_cache_hit_count    = CacheManager::GetScanLineDataMruCache()->GetCacheHitCount();
    caches_info->scanline_cache_query_count  = CacheManager::GetScanLineDataMruCache()->GetQueryCount();
    
    caches_info->overlay_key_cache_cur_item_num = CacheManager::GetOverlayNoOffsetMruCache()->GetCurItemNum();
    caches_info->overlay_key_cache_hit_count = CacheManager::GetOverlayNoOffsetMruCache()->GetCacheHitCount();
    caches_info->overlay_key_cache_query_count = CacheManager::GetOverlayNoOffsetMruCache()->GetQueryCount();
    
    caches_info->clipper_cache_cur_item_num = CacheManager::GetClipperAlphaMaskMruCache()->GetCurItemNum();
    caches_info->clipper_cache_hit_count    = CacheManager::GetClipperAlphaMaskMruCache()->GetCacheHitCount();
    caches_info->clipper_cache_query_count  = CacheManager::GetClipperAlphaMaskMruCache()->GetQueryCount();

    return S_OK;
}

STDMETHODIMP CDirectVobSubFilter::get_XyFlyWeightInfo( XyFlyWeightInfo* xy_fw_info )
{
    XY_LOG_INFO(xy_fw_info);
    CheckPointer(xy_fw_info, S_FALSE);
    CAutoLock cAutoLock(&m_csSubLock);
    
    xy_fw_info->xy_fw_string_w.cur_item_num = XyFwStringW::GetCacher()->GetCurItemNum();
    xy_fw_info->xy_fw_string_w.hit_count = XyFwStringW::GetCacher()->GetCacheHitCount();
    xy_fw_info->xy_fw_string_w.query_count = XyFwStringW::GetCacher()->GetQueryCount();

    xy_fw_info->xy_fw_grouped_draw_items_hash_key.cur_item_num = XyFwGroupedDrawItemsHashKey::GetCacher()->GetCurItemNum();
    xy_fw_info->xy_fw_grouped_draw_items_hash_key.hit_count = XyFwGroupedDrawItemsHashKey::GetCacher()->GetCacheHitCount();
    xy_fw_info->xy_fw_grouped_draw_items_hash_key.query_count = XyFwGroupedDrawItemsHashKey::GetCacher()->GetQueryCount();

    return S_OK;
}

// IDirectVobSub2

STDMETHODIMP CDirectVobSubFilter::put_TextSettings(STSStyle* pDefStyle)
{
	HRESULT hr = CDirectVobSub::put_TextSettings(pDefStyle);

	if(hr == NOERROR)
	{
		//DbgLog((LOG_TRACE, 3, "%d %s:UpdateSubtitle(true)", __LINE__, __FUNCTION__));
		//UpdateSubtitle(true);
		UpdateSubtitle(false);
	}

	return hr;
}

// IDirectVobSubFilterColor

STDMETHODIMP CDirectVobSubFilter::HasConfigDialog(int iSelected)
{
    XY_LOG_INFO(iSelected);
	int nLangs;
	if(FAILED(get_LanguageCount(&nLangs))) return E_FAIL;
	return E_FAIL;
	// TODO: temporally disabled since we don't have a new textsub/vobsub editor dlg for dvs yet
//	return(nLangs >= 0 && iSelected < nLangs ? S_OK : E_FAIL);
}

STDMETHODIMP CDirectVobSubFilter::ShowConfigDialog(int iSelected, HWND hWndParent)
{
    XY_LOG_INFO(XY_LOG_VAR_2_STR(iSelected)<<XY_LOG_VAR_2_STR(hWndParent));
	// TODO: temporally disabled since we don't have a new textsub/vobsub editor dlg for dvs yet
	return(E_FAIL);
}

///////////////////////////////////////////////////////////////////////////

CDirectVobSubFilter2::CDirectVobSubFilter2(LPUNKNOWN punk, HRESULT* phr, const GUID& clsid)
    : CDirectVobSubFilter(punk, phr, clsid)
{
    XY_LOG_INFO("Constructing");
    m_xy_str_opt[STRING_NAME] = L"DirectVobSubFilter2";
}

HRESULT CDirectVobSubFilter2::CheckConnect(PIN_DIRECTION dir, IPin* pPin)
{
    XY_LOG_INFO(XY_LOG_VAR_2_STR(dir)<<XY_LOG_VAR_2_STR(pPin));
	CPinInfo pi;
	if(FAILED(pPin->QueryPinInfo(&pi))) return E_FAIL;

	if(CComQIPtr<IDirectVobSub>(pi.pFilter)) return E_FAIL;

	if(dir == PINDIR_INPUT)
	{
		CFilterInfo fi;
		if(SUCCEEDED(pi.pFilter->QueryFilterInfo(&fi))
		&& !_wcsnicmp(fi.achName, L"Overlay Mixer", 13))
			return(E_FAIL);
	}
	else
	{
	}

	return __super::CheckConnect(dir, pPin);
}

HRESULT CDirectVobSubFilter2::JoinFilterGraph(IFilterGraph* pGraph, LPCWSTR pName)
{
    XY_LOG_INFO("JoinFilterGraph. pGraph:"<<(void*)pGraph);
    HRESULT hr = NOERROR;
	if(pGraph)
	{
        if (IsAppBlackListed()) {
            return E_FAIL;
        }

		BeginEnumFilters(pGraph, pEF, pBF)
		{
			if(pBF != (IBaseFilter*)this && CComQIPtr<IDirectVobSub>(pBF))
				return E_FAIL;
		}
		EndEnumFilters

		// don't look... we will do some serious graph hacking again...
		//
		// we will add dvs2 to the filter graph cache
		// - if the main app has already added some kind of renderer or overlay mixer (anything which accepts video on its input)
		// and
		// - if we have a reason to auto-load (we don't want to make any trouble when there is no need :)
		//
		// This whole workaround is needed because the video stream will always be connected
		// to the pre-added filters first, no matter how high merit we have.

		if(!get_Forced())
		{
			BeginEnumFilters(pGraph, pEF, pBF)
			{
				if(CComQIPtr<IDirectVobSub>(pBF))
					continue;

				CComPtr<IPin> pInPin = GetFirstPin(pBF, PINDIR_INPUT);
				CComPtr<IPin> pOutPin = GetFirstPin(pBF, PINDIR_OUTPUT);

				if(!pInPin)
					continue;

				CComPtr<IPin> pPin;
				if(pInPin && SUCCEEDED(pInPin->ConnectedTo(&pPin))
				|| pOutPin && SUCCEEDED(pOutPin->ConnectedTo(&pPin)))
					continue;

				if(pOutPin && GetFilterName(pBF) != _T("Overlay Mixer"))
					continue;

				bool fVideoInputPin = false;

				do
				{
					BITMAPINFOHEADER bih = {sizeof(BITMAPINFOHEADER), 384, 288, 1, 16, '2YUY', 384*288*2, 0, 0, 0, 0};

					CMediaType cmt;
					cmt.majortype = MEDIATYPE_Video;
					cmt.subtype = MEDIASUBTYPE_YUY2;
					cmt.formattype = FORMAT_VideoInfo;
					cmt.pUnk = NULL;
					cmt.bFixedSizeSamples = TRUE;
					cmt.bTemporalCompression = TRUE;
					cmt.lSampleSize = 384*288*2;
					VIDEOINFOHEADER* vih = (VIDEOINFOHEADER*)cmt.AllocFormatBuffer(sizeof(VIDEOINFOHEADER));
					memset(vih, 0, sizeof(VIDEOINFOHEADER));
					memcpy(&vih->bmiHeader, &bih, sizeof(bih));
					vih->AvgTimePerFrame = 400000;

					if(SUCCEEDED(pInPin->QueryAccept(&cmt)))
					{
						fVideoInputPin = true;
						break;
					}

					VIDEOINFOHEADER2* vih2 = (VIDEOINFOHEADER2*)cmt.AllocFormatBuffer(sizeof(VIDEOINFOHEADER2));
					memset(vih2, 0, sizeof(VIDEOINFOHEADER2));
					memcpy(&vih2->bmiHeader, &bih, sizeof(bih));
					vih2->AvgTimePerFrame = 400000;
					vih2->dwPictAspectRatioX = 384;
					vih2->dwPictAspectRatioY = 288;

					if(SUCCEEDED(pInPin->QueryAccept(&cmt)))
					{
						fVideoInputPin = true;
						break;
					}
				}
				while(false);

				if(fVideoInputPin)
				{
					CComPtr<IBaseFilter> pDVS;
					if(ShouldWeAutoload(pGraph) && SUCCEEDED(pDVS.CoCreateInstance(__uuidof(CDirectVobSubFilter2))))
					{
                        hr = CComQIPtr<IDirectVobSub2>(pDVS)->put_Forced(true);
                        CHECK_N_LOG(hr, "Failed to set option");
                        hr = CComQIPtr<IGraphConfig>(pGraph)->AddFilterToCache(pDVS);
                        CHECK_N_LOG(hr, "Failed to AddFilterToCache. "<<XY_LOG_VAR_2_STR(pGraph)<<XY_LOG_VAR_2_STR(pDVS));
					}

					break;
				}
			}
			EndEnumFilters
		}
	}
	else
	{
	}

	return __super::JoinFilterGraph(pGraph, pName);
}

HRESULT CDirectVobSubFilter2::CheckInputType(const CMediaType* mtIn)
{
    XY_LOG_INFO((mtIn?GuidNames[mtIn->subtype]:"NULL"));
    HRESULT hr = __super::CheckInputType(mtIn);

	if(FAILED(hr) || m_pInput->IsConnected()) return hr;

    if (IsAppBlackListed() || !ShouldWeAutoload(m_pGraph)) {
        return VFW_E_TYPE_NOT_ACCEPTED;
    }

	GetRidOfInternalScriptRenderer();

	return NOERROR;
}

bool CDirectVobSubFilter2::IsAppBlackListed()
{
    // all entries must be lowercase!
    TCHAR* blacklistedapps[] = {
        _T("wm8eutil."), // wmp8 encoder's dummy renderer releases the outputted media sample after calling Receive on its input pin (yes, even when dvobsub isn't registered at all)
        _T("explorer."), // as some users reported thumbnail preview loads dvobsub, I've never experienced this yet...
        _T("producer."), // this is real's producer
        _T("googledesktopindex."), // Google Desktop
        _T("googledesktopdisplay."), // Google Desktop
        _T("googledesktopcrawl."), // Google Desktop
        _T("subtitleworkshop."), // Subtitle Workshop
        _T("subtitleworkshop4."),
        _T("darksouls."), // Dark Souls (Game)
        _T("rometw."), // Rome Total War (Game)
        _T("everquest2."), // EverQuest II (Game)
        _T("yso_win."), // Ys Origin (Game)
        _T("launcher_main."), // Logitech WebCam Software
        _T("webcamdell2."), // Dell WebCam Software
    };

    for (size_t i = 0; i < _countof(blacklistedapps); i++) {
        if (theApp.m_AppName.Find(blacklistedapps[i]) >= 0) {
            return true;
        }
    }

    return false;
}

bool CDirectVobSubFilter2::ShouldWeAutoload(IFilterGraph* pGraph)
{
    XY_LOG_INFO(pGraph);
	int level;
	bool m_fExternalLoad, m_fWebLoad, m_fEmbeddedLoad;
	get_LoadSettings(&level, &m_fExternalLoad, &m_fWebLoad, &m_fEmbeddedLoad);

    if(level < 0 || level >= LOADLEVEL_COUNT || level==LOADLEVEL_DISABLED) return(false);

	bool fRet = false;

    if(level == LOADLEVEL_ALWAYS)
		fRet = m_fExternalLoad = m_fWebLoad = m_fEmbeddedLoad = true;

	// find text stream on known splitters

	if(!fRet && m_fEmbeddedLoad)
	{
		CComPtr<IBaseFilter> pBF;
        for (int i=0, n=m_known_source_filters_guid.GetCount();i<n;i++)
        {
            if (pBF = FindFilter(m_known_source_filters_guid[i], pGraph))
            {
                break;
            }
        }
        BeginEnumPins(pBF, pEP, pPin)
        {
            BeginEnumMediaTypes(pPin, pEM, pmt)
            {
                if(pmt->majortype == MEDIATYPE_Text || pmt->majortype == MEDIATYPE_Subtitle)
                {
                    fRet = true;
                    break;
                }
            }
            EndEnumMediaTypes(pmt)
                if(fRet) break;
        }
        EndEnumPins
	}

	// find file name

	CStringW fn;

	BeginEnumFilters(pGraph, pEF, pBF)
	{
		if(CComQIPtr<IFileSourceFilter> pFSF = pBF)
		{
			LPOLESTR fnw = NULL;
			if(!pFSF || FAILED(pFSF->GetCurFile(&fnw, NULL)) || !fnw)
				continue;
			fn = CString(fnw);
			CoTaskMemFree(fnw);
			break;
		}
	}
	EndEnumFilters

	if((m_fExternalLoad || m_fWebLoad) && (m_fWebLoad || !(wcsstr(fn, L"http://") || wcsstr(fn, L"mms://"))))
	{
		bool fTemp = m_xy_bool_opt[BOOL_HIDE_SUBTITLES];
		fRet = !fn.IsEmpty() && SUCCEEDED(put_FileName((LPWSTR)(LPCWSTR)fn))
			|| SUCCEEDED(put_FileName(L"c:\\tmp.srt"))
			|| fRet;
		if(fTemp) m_xy_bool_opt[BOOL_HIDE_SUBTITLES] = true;
	}

	return(fRet);
}

void CDirectVobSubFilter2::GetRidOfInternalScriptRenderer()
{
	while(CComPtr<IBaseFilter> pBF = FindFilter(L"{48025243-2D39-11CE-875D-00608CB78066}", m_pGraph))
	{
		BeginEnumPins(pBF, pEP, pPin)
		{
			PIN_DIRECTION dir;
			CComPtr<IPin> pPinTo;

			if(SUCCEEDED(pPin->QueryDirection(&dir)) && dir == PINDIR_INPUT
			&& SUCCEEDED(pPin->ConnectedTo(&pPinTo)))
			{
				m_pGraph->Disconnect(pPinTo);
				m_pGraph->Disconnect(pPin);
				m_pGraph->ConnectDirect(pPinTo, GetPin(2 + m_pTextInput.GetCount()-1), NULL);
			}
		}
		EndEnumPins

		if(FAILED(m_pGraph->RemoveFilter(pBF)))
			break;
	}
}

///////////////////////////////////////////////////////////////////////////////

bool CDirectVobSubFilter::Open()
{
    XY_LOG_INFO("");
    HRESULT hr = NOERROR;

	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	CAutoLock cAutolock(&m_csSubLock);

	m_pSubStreams.RemoveAll();
    m_fIsSubStreamEmbeded.RemoveAll();

	m_frd.files.RemoveAll();

	CAtlArray<CString> paths;

	for(int i = 0; i < 10; i++)
	{
		CString tmp;
		tmp.Format(IDS_RP_PATH, i);
		CString path = theApp.GetProfileString(ResStr(IDS_R_DEFTEXTPATHES), tmp);
		if(!path.IsEmpty()) paths.Add(path);
	}

	CAtlArray<SubFile> ret;
	GetSubFileNames(m_xy_str_opt[STRING_FILE_NAME], paths, m_xy_str_opt[DirectVobSubXyOptions::STRING_LOAD_EXT_LIST], ret);

	for(size_t i = 0; i < ret.GetCount(); i++)
	{
		if(m_frd.files.Find(ret[i].full_file_name))
			continue;

		CComPtr<ISubStream> pSubStream;

        if(!pSubStream)
        {
//            CAutoTiming t(TEXT("CRenderedTextSubtitle::Open"), 0);
            XY_AUTO_TIMING(TEXT("CRenderedTextSubtitle::Open"));
            CAutoPtr<CRenderedTextSubtitle> pRTS(new CRenderedTextSubtitle(&m_csSubLock));
            if(pRTS && pRTS->Open(ret[i].full_file_name, DEFAULT_CHARSET) && pRTS->GetStreamCount() > 0)
            {
                pSubStream = pRTS.Detach();
                m_frd.files.AddTail(ret[i].full_file_name + _T(".style"));
            }
        }

		if(!pSubStream)
		{
            CAutoTiming t(TEXT("CVobSubFile::Open"), 0);
			CAutoPtr<CVobSubFile> pVSF(new CVobSubFile(&m_csSubLock));
			if(pVSF && pVSF->Open(ret[i].full_file_name) && pVSF->GetStreamCount() > 0)
			{
				pSubStream = pVSF.Detach();
				m_frd.files.AddTail(ret[i].full_file_name.Left(ret[i].full_file_name.GetLength()-4) + _T(".sub"));
			}
		}

		if(!pSubStream)
		{
            CAutoTiming t(TEXT("ssf::CRenderer::Open"), 0);
			CAutoPtr<ssf::CRenderer> pSSF(new ssf::CRenderer(&m_csSubLock));
			if(pSSF && pSSF->Open(ret[i].full_file_name) && pSSF->GetStreamCount() > 0)
			{
				pSubStream = pSSF.Detach();
			}
		}

		if(pSubStream)
		{
			m_pSubStreams.AddTail(pSubStream);
            m_fIsSubStreamEmbeded.AddTail(false);
			m_frd.files.AddTail(ret[i].full_file_name);
		}
	}

	for(size_t i = 0; i < m_pTextInput.GetCount(); i++)
	{
		if(m_pTextInput[i]->IsConnected())
        {
			m_pSubStreams.AddTail(m_pTextInput[i]->GetSubStream());
            m_fIsSubStreamEmbeded.AddTail(true);
        }
	}

    hr = put_SelectedLanguage(FindPreferedLanguage());
    CHECK_N_LOG(hr, "Failed to set option");
    if(S_FALSE == hr)
        UpdateSubtitle(false); // make sure pSubPicProvider of our queue gets updated even if the stream number hasn't changed

	m_frd.RefreshEvent.Set();

	return(m_pSubStreams.GetCount() > 0);
}

void CDirectVobSubFilter::UpdateSubtitle(bool fApplyDefStyle)
{
    XY_LOG_INFO(fApplyDefStyle);
	CAutoLock cAutolock(&m_csSubLock);

	if(!m_simple_provider) return;

	InvalidateSubtitle();

	CComPtr<ISubStream> pSubStream;

	if(!m_xy_bool_opt[BOOL_HIDE_SUBTITLES])
	{
		int i = m_xy_int_opt[INT_SELECTED_LANGUAGE];

		for(POSITION pos = m_pSubStreams.GetHeadPosition(); i >= 0 && pos; pSubStream = NULL)
		{
			pSubStream = m_pSubStreams.GetNext(pos);

			if(i < pSubStream->GetStreamCount())
			{
				CAutoLock cAutoLock(&m_csSubLock);
				pSubStream->SetStream(i);
				break;
			}

			i -= pSubStream->GetStreamCount();
		}
	}

	SetSubtitle(pSubStream, fApplyDefStyle);
}

void CDirectVobSubFilter::SetSubtitle(ISubStream* pSubStream, bool fApplyDefStyle)
{
    XY_LOG_INFO(XY_LOG_VAR_2_STR(pSubStream)<<XY_LOG_VAR_2_STR(fApplyDefStyle));
    HRESULT hr = NOERROR;
    CAutoLock cAutolock(&m_csSubLock);

    CSize playres(0,0);
    m_video_yuv_matrix_decided_by_sub = ColorConvTable::NONE;
    m_video_yuv_range_decided_by_sub = ColorConvTable::RANGE_NONE;
	if(pSubStream)
	{
		CAutoLock cAutolock(&m_csSubLock);

		CLSID clsid;
		pSubStream->GetClassID(&clsid);

		if(clsid == __uuidof(CVobSubFile))
		{
			CVobSubSettings* pVSS = dynamic_cast<CVobSubFile*>(pSubStream);

			if(fApplyDefStyle)
			{
				pVSS->SetAlignment(m_xy_bool_opt[BOOL_OVERRIDE_PLACEMENT], m_xy_size_opt[SIZE_PLACEMENT_PERC].cx, m_xy_size_opt[SIZE_PLACEMENT_PERC].cy, 1, 1);
				pVSS->m_fOnlyShowForcedSubs = m_xy_bool_opt[BOOL_VOBSUBSETTINGS_ONLY_SHOW_FORCED_SUBS];
			}
		}
		else if(clsid == __uuidof(CVobSubStream))
		{
			CVobSubSettings* pVSS = dynamic_cast<CVobSubStream*>(pSubStream);

			if(fApplyDefStyle)
			{
				pVSS->SetAlignment(m_xy_bool_opt[BOOL_OVERRIDE_PLACEMENT], m_xy_size_opt[SIZE_PLACEMENT_PERC].cx, m_xy_size_opt[SIZE_PLACEMENT_PERC].cy, 1, 1);
				pVSS->m_fOnlyShowForcedSubs = m_xy_bool_opt[BOOL_VOBSUBSETTINGS_ONLY_SHOW_FORCED_SUBS];
			}
		}
		else if(clsid == __uuidof(CRenderedTextSubtitle))
		{
			CRenderedTextSubtitle* pRTS = dynamic_cast<CRenderedTextSubtitle*>(pSubStream);
            pRTS->Deinit();//clear caches

            STSStyle s = m_defStyle;
            if(m_xy_bool_opt[BOOL_OVERRIDE_PLACEMENT])
            {
                s.scrAlignment = 2;
                int w = pRTS->m_dstScreenSize.cx;
                int h = pRTS->m_dstScreenSize.cy;
                CRect tmp_rect = s.marginRect.get();
                int mw = w - tmp_rect.left - tmp_rect.right;
                tmp_rect.bottom = h - MulDiv(h, m_xy_size_opt[SIZE_PLACEMENT_PERC].cy, 100);
                tmp_rect.left = MulDiv(w, m_xy_size_opt[SIZE_PLACEMENT_PERC].cx, 100) - mw/2;
                tmp_rect.right = w - (tmp_rect.left + mw);
                s.marginRect = tmp_rect;
            }

            bool succeeded = pRTS->SetDefaultStyle(s);
            if (!succeeded)
            {
                XY_LOG_ERROR("Failed to set default style");
            }

			pRTS->m_ePARCompensationType = static_cast<CSimpleTextSubtitle::EPARCompensationType>(m_xy_int_opt[INT_ASPECT_RATIO_SETTINGS]);
			if (m_CurrentVIH2.dwPictAspectRatioX != 0 && m_CurrentVIH2.dwPictAspectRatioY != 0&& m_CurrentVIH2.bmiHeader.biWidth != 0 && m_CurrentVIH2.bmiHeader.biHeight != 0)
			{
				pRTS->m_dPARCompensation = ((double)abs(m_CurrentVIH2.bmiHeader.biWidth) / (double)abs(m_CurrentVIH2.bmiHeader.biHeight)) /
					((double)abs((long)m_CurrentVIH2.dwPictAspectRatioX) / (double)abs((long)m_CurrentVIH2.dwPictAspectRatioY));

			}
			else
			{
				pRTS->m_dPARCompensation = 1.00;
			}

            switch(pRTS->m_eYCbCrMatrix)
            {
            case CSimpleTextSubtitle::YCbCrMatrix_BT601:
                m_video_yuv_matrix_decided_by_sub = ColorConvTable::BT601;
                break;
            case CSimpleTextSubtitle::YCbCrMatrix_BT709:
                m_video_yuv_matrix_decided_by_sub = ColorConvTable::BT709;
                break;
            case CSimpleTextSubtitle::YCbCrMatrix_BT2020:
                m_video_yuv_matrix_decided_by_sub = ColorConvTable::BT2020;
                break;
            default:
                m_video_yuv_matrix_decided_by_sub = ColorConvTable::NONE;
                break;
            }
            switch(pRTS->m_eYCbCrRange)
            {
            case CSimpleTextSubtitle::YCbCrRange_PC:
                m_video_yuv_range_decided_by_sub = ColorConvTable::RANGE_PC;
                break;
            case CSimpleTextSubtitle::YCbCrRange_TV:
                m_video_yuv_range_decided_by_sub = ColorConvTable::RANGE_TV;
                break;
            default:
                m_video_yuv_range_decided_by_sub = ColorConvTable::RANGE_NONE;
                break;
            }
            pRTS->Deinit();
            playres = pRTS->m_dstScreenSize;
        }
        else if(clsid == __uuidof(CRenderedHdmvSubtitle))
        {
            CRenderedHdmvSubtitle *sub = dynamic_cast<CRenderedHdmvSubtitle*>(pSubStream);
            CompositionObject::ColorType color_type = CompositionObject::NONE;
            CompositionObject::YuvRangeType range_type = CompositionObject::RANGE_NONE;
            if ( m_xy_str_opt[STRING_PGS_YUV_RANGE].CompareNoCase(_T("PC"))==0 )
            {
                range_type = CompositionObject::RANGE_PC;
            }
            else if ( m_xy_str_opt[STRING_PGS_YUV_RANGE].CompareNoCase(_T("TV"))==0 )
            {
                range_type = CompositionObject::RANGE_TV;
            }
            if ( m_xy_str_opt[STRING_PGS_YUV_MATRIX].CompareNoCase(_T("BT601"))==0 )
            {
                color_type = CompositionObject::YUV_Rec601;
            }
            else if ( m_xy_str_opt[STRING_PGS_YUV_MATRIX].CompareNoCase(_T("BT709"))==0 )
            {
                color_type = CompositionObject::YUV_Rec709;
            }
            else if ( m_xy_str_opt[STRING_PGS_YUV_MATRIX].CompareNoCase(_T("BT2020"))==0 )
            {
                color_type = CompositionObject::YUV_Rec2020;
            }

            m_video_yuv_matrix_decided_by_sub = (m_w > m_bt601Width || m_h > m_bt601Height) ? ColorConvTable::BT709 : 
                ColorConvTable::BT601;
            sub->SetYuvType(color_type, range_type);
        }
    }

	if(!fApplyDefStyle)
	{
		int i = 0;

		POSITION pos = m_pSubStreams.GetHeadPosition();
		while(pos)
		{
			CComPtr<ISubStream> pSubStream2 = m_pSubStreams.GetNext(pos);

			if(pSubStream == pSubStream2)
			{
				m_xy_int_opt[INT_SELECTED_LANGUAGE] = i + pSubStream2->GetStream();
				break;
			}

			i += pSubStream2->GetStreamCount();
		}
	}

	m_nSubtitleId = reinterpret_cast<DWORD_PTR>(pSubStream);

    SetYuvMatrix();

    m_xy_size_opt[SIZE_ASS_PLAY_RESOLUTION] = playres;
    if(m_simple_provider)
        m_simple_provider->SetSubPicProvider(CComQIPtr<ISubPicProviderEx>(pSubStream));
}

void CDirectVobSubFilter::InvalidateSubtitle(REFERENCE_TIME rtInvalidate, DWORD_PTR nSubtitleId)
{
    TRACE_RENDERER_REQUEST(XY_LOG_VAR_2_STR(rtInvalidate)<<XY_LOG_VAR_2_STR(nSubtitleId));
    CAutoLock cAutolock(&m_csSubLock);

	if(m_simple_provider)
	{
		if(nSubtitleId == -1 || nSubtitleId == m_nSubtitleId)
		{
			DbgLog((LOG_TRACE, 3, "InvalidateSubtitle::Invalidate"));
			m_simple_provider->Invalidate(rtInvalidate);
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////

void CDirectVobSubFilter::AddSubStream(ISubStream* pSubStream)
{
    XY_LOG_INFO(pSubStream);
	CAutoLock cAutoLock(&m_csSubLock);

	POSITION pos = m_pSubStreams.Find(pSubStream);
	if(!pos)
    {
        m_pSubStreams.AddTail(pSubStream);
        m_fIsSubStreamEmbeded.AddTail(true);//todo: fix me
    }

	int len = m_pTextInput.GetCount();
	for(size_t i = 0; i < m_pTextInput.GetCount(); i++)
		if(m_pTextInput[i]->IsConnected()) len--;

	if(len == 0)
	{
		HRESULT hr = S_OK;
		m_pTextInput.Add(new CTextInputPin(this, m_pLock, &m_csSubLock, &hr));
	}
}

void CDirectVobSubFilter::RemoveSubStream(ISubStream* pSubStream)
{
    XY_LOG_INFO(pSubStream);
	CAutoLock cAutoLock(&m_csSubLock);

    POSITION pos = m_pSubStreams.GetHeadPosition();
    POSITION pos2 = m_fIsSubStreamEmbeded.GetHeadPosition();
    while(pos!=NULL)
    {
        if( m_pSubStreams.GetAt(pos)==pSubStream )
        {
            m_pSubStreams.RemoveAt(pos);
            m_fIsSubStreamEmbeded.RemoveAt(pos2);
            break;
        }
        else
        {
            m_pSubStreams.GetNext(pos);
            m_fIsSubStreamEmbeded.GetNext(pos2);
        }
    }
}

void CDirectVobSubFilter::Post_EC_OLE_EVENT(CString str, DWORD_PTR nSubtitleId)
{
    XY_LOG_INFO(XY_LOG_VAR_2_STR(str)<<XY_LOG_VAR_2_STR(nSubtitleId));
	if(nSubtitleId != -1 && nSubtitleId != m_nSubtitleId)
		return;

	CComQIPtr<IMediaEventSink> pMES = m_pGraph;
	if(!pMES) return;

	CComBSTR bstr1("Text"), bstr2(" ");

	str.Trim();
	if(!str.IsEmpty()) bstr2 = CStringA(str);

	pMES->Notify(EC_OLE_EVENT, (LONG_PTR)bstr1.Detach(), (LONG_PTR)bstr2.Detach());
}

////////////////////////////////////////////////////////////////

void CDirectVobSubFilter::SetupFRD(CStringArray& paths, CAtlArray<HANDLE>& handles)
{
    XY_LOG_INFO(XY_LOG_VAR_2_STR(paths.GetCount())<<XY_LOG_VAR_2_STR(handles.GetCount()));
    CAutoLock cAutolock(&m_csSubLock);

	for(size_t i = 2; i < handles.GetCount(); i++)
	{
		FindCloseChangeNotification(handles[i]);
	}

	paths.RemoveAll();
	handles.RemoveAll();

	handles.Add(m_frd.EndThreadEvent);
	handles.Add(m_frd.RefreshEvent);

	m_frd.mtime.SetCount(m_frd.files.GetCount());

	POSITION pos = m_frd.files.GetHeadPosition();
	for(int i = 0; pos; i++)
	{
		CString fn = m_frd.files.GetNext(pos);

		CFileStatus status;
		if(CFileGetStatus(fn, status))
			m_frd.mtime[i] = status.m_mtime;

		fn.Replace('\\', '/');
		fn = fn.Left(fn.ReverseFind('/')+1);

		bool fFound = false;

		for(int j = 0; !fFound && j < paths.GetCount(); j++)
		{
			if(paths[j] == fn) fFound = true;
		}

		if(!fFound)
		{
			paths.Add(fn);

			HANDLE h = FindFirstChangeNotification(fn, FALSE, FILE_NOTIFY_CHANGE_LAST_WRITE);
			if(h != INVALID_HANDLE_VALUE) handles.Add(h);
		}
	}
}

DWORD CDirectVobSubFilter::ThreadProc()
{
    XY_LOG_INFO("ThreadProc start");
	SetThreadPriority(m_hThread, THREAD_PRIORITY_LOWEST/*THREAD_PRIORITY_BELOW_NORMAL*/);

	CStringArray paths;
	CAtlArray<HANDLE> handles;

	SetupFRD(paths, handles);
    m_frd.ThreadStartedEvent.Set();
    XY_LOG_INFO("ThreadProc Loop start");
	while(1)
	{
		DWORD idx = WaitForMultipleObjects(handles.GetCount(), handles.GetData(), FALSE, INFINITE);

		if(idx == (WAIT_OBJECT_0 + 0)) // m_frd.hEndThreadEvent
		{
			break;
		}
		if(idx == (WAIT_OBJECT_0 + 1)) // m_frd.hRefreshEvent
		{
			SetupFRD(paths, handles);
		}
		else if(idx >= (WAIT_OBJECT_0 + 2) && idx < (WAIT_OBJECT_0 + handles.GetCount()))
		{
			bool fLocked = true;
			IsSubtitleReloaderLocked(&fLocked);
			if(fLocked) continue;

			if(FindNextChangeNotification(handles[idx - WAIT_OBJECT_0]) == FALSE)
				break;

			int j = 0;

			POSITION pos = m_frd.files.GetHeadPosition();
			for(int i = 0; pos && j == 0; i++)
			{
				CString fn = m_frd.files.GetNext(pos);

				CFileStatus status;
				if(CFileGetStatus(fn, status) && m_frd.mtime[i] != status.m_mtime)
				{
					for(j = 0; j < 10; j++)
					{
						if(FILE* f = _tfopen(fn, _T("rb+")))
						{
							fclose(f);
							j = 0;
							break;
						}
						else
						{
							Sleep(100);
							j++;
						}
					}
				}
			}

			if(j > 0)
			{
				SetupFRD(paths, handles);
			}
			else
			{
				Sleep(500);

				POSITION pos = m_frd.files.GetHeadPosition();
				for(int i = 0; pos; i++)
				{
					CFileStatus status;
					if(CFileGetStatus(m_frd.files.GetNext(pos), status)
						&& m_frd.mtime[i] != status.m_mtime)
					{
						Open();
						SetupFRD(paths, handles);
						break;
					}
				}
			}
		}
		else
		{
			break;
		}
	}
    XY_LOG_INFO("ThreadProc Loop exit");
	for(size_t i = 2; i < handles.GetCount(); i++)
	{
		FindCloseChangeNotification(handles[i]);
	}
    XY_LOG_INFO("ThreadProc exit");
	return 0;
}

void CDirectVobSubFilter::GetInputColorspaces( ColorSpaceId *preferredOrder, UINT *count )
{
    XY_LOG_INFO("");
    HRESULT hr = NOERROR;
    ColorSpaceOpt *color_space=NULL;
    int tempCount = 0;
    hr = XyGetBin(BIN_INPUT_COLOR_FORMAT, reinterpret_cast<LPVOID*>(&color_space), &tempCount);
    CHECK_N_LOG(hr, "Failed to get option");
    if( hr==S_OK )
    {
        *count = 0;
        for (int i=0;i<tempCount;i++)
        {
            if(color_space[i].selected)
            {
                preferredOrder[*count] = color_space[i].color_space;
                (*count)++;
            }
        }
    }
    else
    {
        CBaseVideoFilter::GetInputColorspaces(preferredOrder, count);
    }
    delete[]color_space;
}

void CDirectVobSubFilter::GetOutputColorspaces( ColorSpaceId *preferredOrder, UINT *count )
{
    XY_LOG_INFO("");
    HRESULT hr = NOERROR;
    ColorSpaceOpt *color_space=NULL;
    int tempCount = 0;
    hr = XyGetBin(BIN_OUTPUT_COLOR_FORMAT, reinterpret_cast<LPVOID*>(&color_space), &tempCount);
    CHECK_N_LOG(hr, "Failed to get option");
    if( hr==S_OK )
    {
        *count = 0;
        for (int i=0;i<tempCount;i++)
        {
            if(color_space[i].selected)
            {
                preferredOrder[*count] = color_space[i].color_space;
                (*count)++;
            }
        }
    }
    else
    {
        CBaseVideoFilter::GetInputColorspaces(preferredOrder, count);
    }
    delete []color_space;
}

HRESULT CDirectVobSubFilter::GetIsEmbeddedSubStream( int iSelected, bool *fIsEmbedded )
{
    XY_LOG_INFO(iSelected);
    CAutoLock cAutolock(&m_csSubLock);

    HRESULT hr = E_INVALIDARG;
    if (!fIsEmbedded)
    {
        return S_FALSE;
    }

    int i = iSelected;
    *fIsEmbedded = false;

    POSITION pos = m_pSubStreams.GetHeadPosition();
    POSITION pos2 = m_fIsSubStreamEmbeded.GetHeadPosition();
    while(i >= 0 && pos)
    {
        CComPtr<ISubStream> pSubStream = m_pSubStreams.GetNext(pos);
        bool isEmbedded = m_fIsSubStreamEmbeded.GetNext(pos2);
        if(i < pSubStream->GetStreamCount())
        {
            hr = NOERROR;
            *fIsEmbedded = isEmbedded;
            break;
        }

        i -= pSubStream->GetStreamCount();
    }
    return hr;
}

void CDirectVobSubFilter::UpdateLanguageCount()
{
    CAutoLock cAutolock(&m_csSubLock);

    int tmp = 0;
    POSITION pos = m_pSubStreams.GetHeadPosition();
    while(pos) {
        tmp += m_pSubStreams.GetNext(pos)->GetStreamCount();
    }
    m_xy_int_opt[INT_LANGUAGE_COUNT] = tmp;
}

void CDirectVobSubFilter::SetYuvMatrix()
{
    XY_LOG_INFO("");
    ColorConvTable::YuvMatrixType yuv_matrix = ColorConvTable::BT601;
    ColorConvTable::YuvRangeType yuv_range = ColorConvTable::RANGE_TV;

    if ( m_xy_int_opt[INT_COLOR_SPACE]==CDirectVobSub::YuvMatrix_AUTO )
    {
        if (m_video_yuv_matrix_decided_by_sub!=ColorConvTable::NONE)
        {
            yuv_matrix = m_video_yuv_matrix_decided_by_sub;
        }
        else
        {
            if (m_xy_str_opt[STRING_CONSUMER_YUV_MATRIX].Right(3).CompareNoCase(L"601")==0)
            {
                yuv_matrix = ColorConvTable::BT601;
            }
            else if (m_xy_str_opt[STRING_CONSUMER_YUV_MATRIX].Right(3).CompareNoCase(L"709")==0)
            {
                yuv_matrix = ColorConvTable::BT709;
            }
            else if (m_xy_str_opt[STRING_CONSUMER_YUV_MATRIX].Right(4).CompareNoCase(L"2020")==0)
            {
                yuv_matrix = ColorConvTable::BT2020;
            }
            else
            {
                XY_LOG_WARN(L"Can NOT get useful YUV range from consumer:"<<m_xy_str_opt[STRING_CONSUMER_YUV_MATRIX].GetString());
                yuv_matrix = (m_xy_size_opt[SIZE_ORIGINAL_VIDEO].cx > m_bt601Width || 
                    m_xy_size_opt[SIZE_ORIGINAL_VIDEO].cy > m_bt601Height) ? ColorConvTable::BT709 : ColorConvTable::BT601;
            }
        }
    }
    else
    {
        switch(m_xy_int_opt[INT_COLOR_SPACE])
        {
        case CDirectVobSub::BT_601:
            yuv_matrix = ColorConvTable::BT601;
            break;
        case CDirectVobSub::BT_709:
            yuv_matrix = ColorConvTable::BT709;
            break;
        case CDirectVobSub::BT_2020:
            yuv_matrix = ColorConvTable::BT2020;
            break;
        case CDirectVobSub::GUESS:
        default:        
            yuv_matrix = (m_w > m_bt601Width || m_h > m_bt601Height) ? ColorConvTable::BT709 : ColorConvTable::BT601;
            break;
        }
    }

    if( m_xy_int_opt[INT_YUV_RANGE]==CDirectVobSub::YuvRange_Auto )
    {
        if (m_video_yuv_range_decided_by_sub!=ColorConvTable::RANGE_NONE)
            yuv_range = m_video_yuv_range_decided_by_sub;
        else
            yuv_range = ColorConvTable::RANGE_TV;
    }
    else
    {
        switch(m_xy_int_opt[INT_YUV_RANGE])
        {
        case CDirectVobSub::YuvRange_TV:
            yuv_range = ColorConvTable::RANGE_TV;
            break;
        case CDirectVobSub::YuvRange_PC:
            yuv_range = ColorConvTable::RANGE_PC;
            break;
        case CDirectVobSub::YuvRange_Auto:
            yuv_range = ColorConvTable::RANGE_TV;
            break;
        }
    }

    ColorConvTable::SetDefaultConvType(yuv_matrix, yuv_range);
}

//
// XyOptionsImpl
//

HRESULT CDirectVobSubFilter::OnOptionChanged( unsigned field )
{
    HRESULT hr = CDirectVobSub::OnOptionChanged(field);
    if (FAILED(hr))
    {
        return hr;
    }
    switch(field)
    {
    case STRING_FILE_NAME:
        if (!Open())
        {
            m_xy_str_opt[STRING_FILE_NAME].Empty();
            hr = E_FAIL;
            break;
        }
        break;
    case BOOL_OVERRIDE_PLACEMENT:
    case SIZE_PLACEMENT_PERC:
    case INT_ASPECT_RATIO_SETTINGS:
        UpdateSubtitle(false);
        break;
    case BOOL_VOBSUBSETTINGS_BUFFER:
    case BOOL_VOBSUBSETTINGS_ONLY_SHOW_FORCED_SUBS:
    case BOOL_VOBSUBSETTINGS_POLYGONIZE:
    case BIN2_TEXT_SETTINGS:
    case BIN2_SUBTITLE_TIMING:
        InvalidateSubtitle();
        break;
    }

    return hr;
}

HRESULT CDirectVobSubFilter::DoGetField(unsigned field, void *value)
{
    HRESULT hr = NOERROR;
    switch(field)
    {
    case INT_LANGUAGE_COUNT:
        UpdateLanguageCount();
        hr = DirectVobSubImpl::DoGetField(field, value);
        break;
    case INT_CUR_STYLES_COUNT:
        {
            CAutoLock cAutoLock(&m_csFilter);
            ISubStream *pSubStream = NULL;
            if (m_nSubtitleId!=-1)
            {
                pSubStream = reinterpret_cast<ISubStream *>(m_nSubtitleId);
            }
            if (dynamic_cast<CRenderedTextSubtitle*>(pSubStream)!=NULL)
            {
                hr = S_OK;
                CRenderedTextSubtitle * rts = dynamic_cast<CRenderedTextSubtitle*>(pSubStream);
                *(int*)value = rts->m_styles.GetCount();
            }
            else
            {
                hr = S_FALSE;
                *(int*)value = 0;
            }
        }
        break;
    default:
        hr = DirectVobSubImpl::DoGetField(field, value);
        break;
    }
    return hr;
}

//
// DirectVobSubImpl
//
HRESULT CDirectVobSubFilter::GetCurStyles( SubStyle sub_style[], int count )
{
    CAutoLock cAutoLock(&m_csFilter);
    ISubStream *curSubStream = NULL;
    if (m_nSubtitleId!=-1)
    {
        curSubStream = reinterpret_cast<ISubStream *>(m_nSubtitleId);
    }
    if (dynamic_cast<CRenderedTextSubtitle*>(curSubStream)!=NULL)
    {
        CRenderedTextSubtitle * rts = dynamic_cast<CRenderedTextSubtitle*>(curSubStream);
        if (count != rts->m_styles.GetCount())
        {
            return E_INVALIDARG;
        }
        POSITION pos = rts->m_styles.GetStartPosition();
        int i = 0;
        while(pos)
        {
            CSTSStyleMap::CPair *pair = rts->m_styles.GetNext(pos);
            if (!pair)
            {
                return E_FAIL;
            }
            if (pair->m_key.GetLength()>=countof(sub_style[0].name))
            {
                return ERROR_BUFFER_OVERFLOW;
            }
            wcscpy_s(sub_style[i].name, pair->m_key.GetString());
            if (sub_style[i].style)
            {
                *(STSStyle*)(sub_style[i].style) = *pair->m_value;
            }
            i++;
        }
    }
    else
    {
        return S_FALSE;
    }
    return S_OK;
}

HRESULT CDirectVobSubFilter::SetCurStyles( const SubStyle sub_style[], int count )
{
    HRESULT hr = NOERROR;
    CAutoLock cAutoLock(&m_csFilter);
    ISubStream *curSubStream = NULL;
    if (m_nSubtitleId!=-1)
    {
        curSubStream = reinterpret_cast<ISubStream *>(m_nSubtitleId);
    }
    if (dynamic_cast<CRenderedTextSubtitle*>(curSubStream)!=NULL)
    {
        CRenderedTextSubtitle * rts = dynamic_cast<CRenderedTextSubtitle*>(curSubStream);
        if (count != rts->m_styles.GetCount())
        {
            return E_INVALIDARG;
        }
        for (int i=0;i<count;i++)
        {
            if (!rts->m_styles.Lookup(sub_style[i].name))
            {
                return E_FAIL;
            }
        }
        bool changed = false;
        for (int i=0;i<count;i++)
        {
            STSStyle * style = NULL;
            rts->m_styles.Lookup(sub_style[i].name, style);
            ASSERT(style);
            if (sub_style[i].style)
            {
                *style = *static_cast<STSStyle*>(sub_style[i].style);
                changed = true;
            }
        }
        if (changed) {
            hr = OnOptionChanged(BIN2_CUR_STYLES);
        }
    }
    else
    {
        return E_FAIL;
    }
    return hr;
}

//
// IXyOptions
//

STDMETHODIMP CDirectVobSubFilter::XySetBool( unsigned field, bool value )
{
    CAutoLock cAutolock(&m_csSubLock);
    HRESULT hr = CDirectVobSub::XySetBool(field, value);
    if(hr != NOERROR)
    {
        return hr;
    }
    switch(field)
    {
    case DirectVobSubXyOptions::BOOL_FOLLOW_UPSTREAM_PREFERRED_ORDER:
        m_donot_follow_upstream_preferred_order = !m_xy_bool_opt[BOOL_FOLLOW_UPSTREAM_PREFERRED_ORDER];
        break;
    case DirectVobSubXyOptions::BOOL_COMBINE_BITMAPS:
    case DirectVobSubXyOptions::BOOL_IS_MOVABLE:
    case DirectVobSubXyOptions::BOOL_IS_BITMAP:
        hr = E_NOTIMPL;
        break;
    case DirectVobSubXyOptions::BOOL_HIDE_SUBTITLES:
        UpdateSubtitle(false);
        break;
    default:
        break;
    }
    return hr;
}

STDMETHODIMP CDirectVobSubFilter::XySetInt( unsigned field, int value )
{
    CAutoLock cAutolock(&m_csSubLock);
    HRESULT hr = CDirectVobSub::XySetInt(field, value);
    if(hr != NOERROR)
    {
        return hr;
    }
    switch(field)
    {
    case DirectVobSubXyOptions::INT_COLOR_SPACE:
    case DirectVobSubXyOptions::INT_YUV_RANGE:
        SetYuvMatrix();
        break;
    case DirectVobSubXyOptions::INT_OVERLAY_CACHE_MAX_ITEM_NUM:
        CacheManager::GetOverlayMruCache()->SetMaxItemNum(m_xy_int_opt[field]);
        break;
    case DirectVobSubXyOptions::INT_SCAN_LINE_DATA_CACHE_MAX_ITEM_NUM:
        CacheManager::GetScanLineData2MruCache()->SetMaxItemNum(m_xy_int_opt[field]);
        break;
    case DirectVobSubXyOptions::INT_PATH_DATA_CACHE_MAX_ITEM_NUM:
        CacheManager::GetPathDataMruCache()->SetMaxItemNum(m_xy_int_opt[field]);
        break;
    case DirectVobSubXyOptions::INT_OVERLAY_NO_BLUR_CACHE_MAX_ITEM_NUM:
        CacheManager::GetOverlayNoBlurMruCache()->SetMaxItemNum(m_xy_int_opt[field]);
        break;
    case DirectVobSubXyOptions::INT_BITMAP_MRU_CACHE_ITEM_NUM:
        CacheManager::GetBitmapMruCache()->SetMaxItemNum(m_xy_int_opt[field]);
        break;
    case DirectVobSubXyOptions::INT_CLIPPER_MRU_CACHE_ITEM_NUM:
        CacheManager::GetClipperAlphaMaskMruCache()->SetMaxItemNum(m_xy_int_opt[field]);
        break;
    case DirectVobSubXyOptions::INT_TEXT_INFO_CACHE_ITEM_NUM:
        CacheManager::GetTextInfoCache()->SetMaxItemNum(m_xy_int_opt[field]);
        break;
    case DirectVobSubXyOptions::INT_ASS_TAG_LIST_CACHE_ITEM_NUM:
        CacheManager::GetAssTagListMruCache()->SetMaxItemNum(m_xy_int_opt[field]);
        break;
    case DirectVobSubXyOptions::INT_SUBPIXEL_VARIANCE_CACHE_ITEM_NUM:
        CacheManager::GetSubpixelVarianceCache()->SetMaxItemNum(m_xy_int_opt[field]);
        break;
    case DirectVobSubXyOptions::INT_SUBPIXEL_POS_LEVEL:
        SubpixelPositionControler::GetGlobalControler().SetSubpixelLevel( static_cast<SubpixelPositionControler::SUBPIXEL_LEVEL>(m_xy_int_opt[field]) );
        break;
    case DirectVobSubXyOptions::INT_SELECTED_LANGUAGE:
        UpdateSubtitle(false);
        break;
    default:
        break;
    }

    return hr;
}

//
// OSD
//
void CDirectVobSubFilter::ZeroObj4OSD()
{
    XY_LOG_INFO("");
    m_hdc = 0;
    m_hbm = 0;
    m_hfont = 0;

    {
        LOGFONT lf;
        memset(&lf, 0, sizeof(lf));
        lf.lfCharSet = DEFAULT_CHARSET;
        lf.lfOutPrecision = OUT_CHARACTER_PRECIS;
        lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
        lf.lfQuality = ANTIALIASED_QUALITY;
        HDC hdc = GetDC(NULL);
        lf.lfHeight = 16;
        //MulDiv(20, GetDeviceCaps(hdc, LOGPIXELSY), 54);
        ReleaseDC(NULL, hdc);
        lf.lfWeight = FW_BOLD;
        _tcscpy(lf.lfFaceName, _T("Arial"));
        m_hfont = CreateFontIndirect(&lf);
    }
}

void CDirectVobSubFilter::DeleteObj4OSD()
{
    XY_LOG_INFO("");
    if(m_hfont) {DeleteObject(m_hfont); m_hfont = 0;}
    if(m_hbm) {DeleteObject(m_hbm); m_hbm = 0;}
    if(m_hdc) {DeleteObject(m_hdc); m_hdc = 0;}
}

void CDirectVobSubFilter::InitObj4OSD()
{
    XY_LOG_INFO("");
    if(m_hbm) {DeleteObject(m_hbm); m_hbm = NULL;}
    if(m_hdc) {DeleteDC(m_hdc); m_hdc = NULL;}

    struct {
        BITMAPINFOHEADER bih;
        RGBQUAD biColors[256];
    } b = {
        {sizeof(BITMAPINFOHEADER), m_w, -(int)m_h, 1, 8, BI_RGB, 0, 0, 0, 0, 0},
        {0}
    };
    for (int i=0;i<256;i++)
    {
        b.biColors[i].rgbBlue = b.biColors[i].rgbGreen = b.biColors[i].rgbRed = i;
    }
    m_hdc = CreateCompatibleDC(NULL);
    m_hbm = CreateDIBSection(m_hdc, (BITMAPINFO*)&b, DIB_RGB_COLORS, NULL, NULL, 0);

    BITMAP bm;
    GetObject(m_hbm, sizeof(bm), &bm);
    memset(bm.bmBits, 0, bm.bmHeight*bm.bmWidthBytes);
}
