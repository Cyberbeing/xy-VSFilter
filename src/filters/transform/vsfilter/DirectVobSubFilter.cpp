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

#include <d3d9.h>
#include <dxva2api.h>

#include "CAutoTiming.h"


#define MAX_SUBPIC_QUEUE_LENGTH 1

///////////////////////////////////////////////////////////////////////////

/*removeme*/
bool g_RegOK = true;//false; // doesn't work with the dvd graph builder
#include "valami.cpp"

#ifdef __DO_LOG
EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#endif

using namespace DirectVobSubXyOptions;

////////////////////////////////////////////////////////////////////////////
//
// Constructor
//

CDirectVobSubFilter::CDirectVobSubFilter(LPUNKNOWN punk, HRESULT* phr, const GUID& clsid)
	: CBaseVideoFilter(NAME("CDirectVobSubFilter"), punk, phr, clsid)
	, m_nSubtitleId(-1)
	, m_fMSMpeg4Fix(false)
	, m_fps(25)
{
	DbgLog((LOG_TRACE, 3, _T("CDirectVobSubFilter::CDirectVobSubFilter")));

    // and then, anywhere you need it:

	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	ZeroObj4OSD();

	theApp.WriteProfileString(ResStr(IDS_R_DEFTEXTPATHES), _T("Hint"), _T("The first three are fixed, but you can add more up to ten entries."));
	theApp.WriteProfileString(ResStr(IDS_R_DEFTEXTPATHES), _T("Path0"), _T("."));
	theApp.WriteProfileString(ResStr(IDS_R_DEFTEXTPATHES), _T("Path1"), _T("c:\\subtitles"));
	theApp.WriteProfileString(ResStr(IDS_R_DEFTEXTPATHES), _T("Path2"), _T(".\\subtitles"));

	m_fLoading = true;

	m_hSystrayThread = 0;
	m_tbid.hSystrayWnd = NULL;
	m_tbid.graph = NULL;
	m_tbid.fRunOnce = false;
	m_tbid.fShowIcon = (theApp.m_AppName.Find(_T("zplayer"), 0) < 0 || !!theApp.GetProfileInt(ResStr(IDS_R_GENERAL), ResStr(IDS_RG_ENABLEZPICON), 0));

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

    m_time_alphablt = m_time_rasterization = 0;

    m_video_yuv_matrix_decided_by_sub = ColorConvTable::NONE;
    m_video_yuv_range_decided_by_sub = ColorConvTable::RANGE_NONE;
}

CDirectVobSubFilter::~CDirectVobSubFilter()
{
	CAutoLock cAutoLock(&m_csQueueLock);
	if(m_simple_provider)
	{
		DbgLog((LOG_TRACE, 3, "~CDirectVobSubFilter::Invalidate"));
		m_simple_provider->Invalidate();
	}
	m_simple_provider = NULL;

    DeleteObj4OSD();

	for(size_t i = 0; i < m_pTextInput.GetCount(); i++)
		delete m_pTextInput[i];

	m_frd.EndThreadEvent.Set();
	CAMThread::Close();

	DbgLog((LOG_TRACE, 3, _T("CDirectVobSubFilter::~CDirectVobSubFilter")));

	//Trace(_T("CDirectVobSubFilter::~CDirectVobSubFilter"));
	//ReleaseTracer;
}

STDMETHODIMP CDirectVobSubFilter::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
    CheckPointer(ppv, E_POINTER);

    return
		QI(IDirectVobSub)
		QI(IDirectVobSub2)
        QI(IDirectVobSubXy)
		QI(IFilterVersion)
		QI(ISpecifyPropertyPages)
		QI(IAMStreamSelect)
		__super::NonDelegatingQueryInterface(riid, ppv);
}

// CBaseVideoFilter

void CDirectVobSubFilter::GetOutputSize(int& w, int& h, int& arx, int& ary)
{
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
	XY_LOG_ONCE(0, _T("CDirectVobSubFilter::Transform"));
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
		CAutoLock cAutoLock(&m_csQueueLock);

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

	CComPtr<IMediaSample2> pOut;
	BYTE* pDataOut = NULL;
	if (FAILED(hr = GetDeliveryBuffer(spd.w, spd.h, (IMediaSample**)&pOut))
	|| FAILED(hr = pOut->GetPointer(&pDataOut)))
		return hr;
	pOut->SetTime(&rtStart, &rtStop);
	pOut->SetMediaTime(NULL, NULL);
	pOut->SetDiscontinuity(pIn->IsDiscontinuity() == S_OK);
	pOut->SetSyncPoint(pIn->IsSyncPoint() == S_OK);
	pOut->SetPreroll(pIn->IsPreroll() == S_OK);

    AM_SAMPLE2_PROPERTIES inputProps;

    if (SUCCEEDED(((IMediaSample2*)pIn)->GetProperties(sizeof(inputProps), (BYTE*)&inputProps))) {

        AM_SAMPLE2_PROPERTIES outProps;

        if (SUCCEEDED(pOut->GetProperties(sizeof(outProps), (BYTE*)&outProps))) {

            outProps.dwTypeSpecificFlags = inputProps.dwTypeSpecificFlags;

            pOut->SetProperties(sizeof(outProps), (BYTE*)&outProps);

        }

    }

	//

	BITMAPINFOHEADER bihOut;
	ExtractBIH(&m_pOutput->CurrentMediaType(), &bihOut);

	bool fInputFlipped = bihIn.biHeight >= 0 && bihIn.biCompression <= 3;
	bool fOutputFlipped = bihOut.biHeight >= 0 && bihOut.biCompression <= 3;

	bool fFlip = fInputFlipped != fOutputFlipped;
	if(m_fFlipPicture) fFlip = !fFlip;
	if(m_fMSMpeg4Fix) fFlip = !fFlip;

	bool fFlipSub = fOutputFlipped;
	if(m_fFlipSubtitles) fFlipSub = !fFlipSub;

	//

	{
		CAutoLock cAutoLock(&m_csQueueLock);

		if(m_simple_provider)
		{
            CComPtr<ISimpleSubPic> pSubPic;
			//int timeStamp1 = GetTickCount();
			bool lookupResult = m_simple_provider->LookupSubPic(CalcCurrentTime(), &pSubPic);
			//int timeStamp2 = GetTickCount();
			//m_time_rasterization += timeStamp2-timeStamp1;

			if(lookupResult && pSubPic)
			{
                if(fFlip ^ fFlipSub)
                    spd.h = -spd.h;
                pSubPic->AlphaBlt(&spd);
				DbgLog((LOG_TRACE,3,"AlphaBlt time:%lu", (ULONG)(CalcCurrentTime()/10000)));
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

	if(n >= 0 && (unsigned int)n < m_pTextInput.GetCount())
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
		if(m_hSystrayThread)
		{
			SendMessage(m_tbid.hSystrayWnd, WM_CLOSE, 0, 0);

			if(WaitForSingleObject(m_hSystrayThread, 10000) != WAIT_OBJECT_0)
			{
				DbgLog((LOG_TRACE, 0, _T("CALL THE AMBULANCE!!!")));
				TerminateThread(m_hSystrayThread, (DWORD)-1);
			}

			m_hSystrayThread = 0;
		}
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

		DbgLog((LOG_TRACE, 3, "SetMediaType => InitSubPicQueue"));
		InitSubPicQueue();
	}
	else if(dir == PINDIR_OUTPUT)
	{

	}

	return hr;
}

HRESULT CDirectVobSubFilter::CheckConnect(PIN_DIRECTION dir, IPin* pPin)
{
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
    bool reconnected = false;
	if(dir == PINDIR_INPUT)
	{
        DbgLog((LOG_TRACE, 3, TEXT("connect input")));
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
        DbgLog((LOG_TRACE, 3, TEXT("connect output")));
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
                    DbgLog((LOG_ERROR, 3, TEXT("Unexpected error when GetMediaType, position:%d"), position));
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
                            DbgLog((LOG_ERROR, 3, TEXT("Can use the same subtype!")));
                        }
                    }
                }
            }
            else
            {
                DbgLog((LOG_ERROR, 3, TEXT("Cannot use the same subtype!")));
            }
        }
        if ( can_reconnect )
        {
            if (SUCCEEDED(ReconnectPin(m_pInput, &desiredMt)))
            {
                reconnected = true;
                //m_pInput->SetMediaType(&desiredMt);
                DbgLog((LOG_TRACE, 3, TEXT("reconnected succeed!")));
            }
            else
            {
                //log
                return E_FAIL;
            }
        }
        else if(!can_transform)
        {
            DbgLog((LOG_TRACE, 3, TEXT("Failed to agree reconnect type!")));
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
            m_tbid.dvs = static_cast<IDirectVobSub*>(this);

            DWORD tid;
            m_hSystrayThread = CreateThread(0, 0, SystrayThreadProc, &m_tbid, 0, &tid);
        }
        m_pInput->SetMediaType( &m_pInput->CurrentMediaType() );
    }

    HRESULT hr = __super::CompleteConnect(dir, pReceivePin);
    DbgLog((LOG_TRACE, 3, TEXT("connect fininshed!")));
    DumpGraph(m_pGraph,0);
	return hr;    
}

HRESULT CDirectVobSubFilter::BreakConnect(PIN_DIRECTION dir)
{
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
	}
	else if(dir == PINDIR_OUTPUT)
	{
        if (m_pOutput->IsConnected()) {
            m_outputFmtCount = -1;
        }
		// not really needed, but may free up a little memory
		CAutoLock cAutoLock(&m_csQueueLock);
		m_simple_provider = NULL;
	}

	return __super::BreakConnect(dir);
}

HRESULT CDirectVobSubFilter::StartStreaming()
{
	/* WARNING: calls to m_pGraph member functions from within this function will generate deadlock with Haali
	 * Video Renderer in MPC. Reason is that CAutoLock's variables in IFilterGraph functions are overriden by
	 * CFGManager class.
	 */

	m_fLoading = false;

	DbgLog((LOG_TRACE, 3, "StartStreaming => InitSubPicQueue"));
	InitSubPicQueue();

	m_tbid.fRunOnce = true;

	put_MediaFPS(m_fMediaFPSEnabled, m_MediaFPS);

	return __super::StartStreaming();
}

HRESULT CDirectVobSubFilter::StopStreaming()
{
	InvalidateSubtitle();

	//xy Timing
	//FILE * timingFile = fopen("C:\\vsfilter_timing.txt", "at");
	//fprintf(timingFile, "%s:%ld %s:%ld\n", "m_time_alphablt", m_time_alphablt, "m_time_rasterization", m_time_rasterization);
	//fclose(timingFile);

	return __super::StopStreaming();
}

HRESULT CDirectVobSubFilter::NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate)
{
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
	CAutoLock cAutoLock(&m_csQueueLock);

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
	m_spd.bits = (void*)m_pTempPicBuff;

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

    XySetSize(DirectVobSubXyOptions::SIZE_ORIGINAL_VIDEO, CSize(m_w, m_h));
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

	int nLangs;
	get_LanguageCount(&nLangs);

	if(nLangs <= 0) return(0);

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
	if(!(dwFlags & AMSTREAMSELECTENABLE_ENABLE))
		return E_NOTIMPL;

	int nLangs = 0;
	get_LanguageCount(&nLangs);

	if(!(lIndex >= 0 && lIndex < nLangs+2+2))
		return E_INVALIDARG;

	int i = lIndex-1;

	if(i == -1 && !m_fLoading) // we need this because when loading something stupid media player pushes the first stream it founds, which is "enable" in our case
	{
		put_HideSubtitles(false);
	}
	else if(i >= 0 && i < nLangs)
	{
		put_HideSubtitles(false);
		put_SelectedLanguage(i);

		WCHAR* pName = NULL;
		if(SUCCEEDED(get_LanguageName(i, &pName)))
		{
			UpdatePreferedLanguages(CString(pName));
			if(pName) CoTaskMemFree(pName);
		}
	}
	else if(i == nLangs && !m_fLoading)
	{
		put_HideSubtitles(true);
	}
	else if((i == nLangs+1 || i == nLangs+2) && !m_fLoading)
	{
		put_Flip(i == nLangs+2, m_fFlipSubtitles);
	}

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

		if(i == -1 && !m_fHideSubtitles
		|| i >= 0 && i < nLangs && i == m_iSelectedLanguage
		|| i == nLangs && m_fHideSubtitles
		|| i == nLangs+1 && !m_fFlipPicture
		|| i == nLangs+2 && m_fFlipPicture)
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

STDMETHODIMP CDirectVobSubFilter::put_FileName(WCHAR* fn)
{
    AMTRACE((TEXT(__FUNCTION__),0));
	HRESULT hr = CDirectVobSub::put_FileName(fn);

	if(hr == S_OK && !Open())
	{
		m_FileName.Empty();
		hr = E_FAIL;
	}

	return hr;
}

STDMETHODIMP CDirectVobSubFilter::get_LanguageCount(int* nLangs)
{
	HRESULT hr = CDirectVobSub::get_LanguageCount(nLangs);

	if(hr == NOERROR && nLangs)
	{
        CAutoLock cAutolock(&m_csQueueLock);

		*nLangs = 0;
		POSITION pos = m_pSubStreams.GetHeadPosition();
		while(pos) (*nLangs) += m_pSubStreams.GetNext(pos)->GetStreamCount();
	}

	return hr;
}

STDMETHODIMP CDirectVobSubFilter::get_LanguageName(int iLanguage, WCHAR** ppName)
{
	HRESULT hr = CDirectVobSub::get_LanguageName(iLanguage, ppName);

	if(!ppName) return E_POINTER;

	if(hr == NOERROR)
	{
        CAutoLock cAutolock(&m_csQueueLock);

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

STDMETHODIMP CDirectVobSubFilter::put_SelectedLanguage(int iSelected)
{
	HRESULT hr = CDirectVobSub::put_SelectedLanguage(iSelected);

	if(hr == NOERROR)
	{
		UpdateSubtitle(false);
	}

	return hr;
}

STDMETHODIMP CDirectVobSubFilter::put_HideSubtitles(bool fHideSubtitles)
{
	HRESULT hr = CDirectVobSub::put_HideSubtitles(fHideSubtitles);

	if(hr == NOERROR)
	{
		UpdateSubtitle(false);
	}

	return hr;
}

STDMETHODIMP CDirectVobSubFilter::put_PreBuffering(bool fDoPreBuffering)
{
	HRESULT hr = CDirectVobSub::put_PreBuffering(fDoPreBuffering);

	if(hr == NOERROR)
	{
		DbgLog((LOG_TRACE, 3, "put_PreBuffering => InitSubPicQueue"));
		InitSubPicQueue();
	}

	return hr;
}

STDMETHODIMP CDirectVobSubFilter::put_Placement(bool fOverridePlacement, int xperc, int yperc)
{
	DbgLog((LOG_TRACE, 3, "%s(%d) %s", __FILE__, __LINE__, __FUNCTION__));
	HRESULT hr = CDirectVobSub::put_Placement(fOverridePlacement, xperc, yperc);

	if(hr == NOERROR)
	{
		//DbgLog((LOG_TRACE, 3, "%d %s:UpdateSubtitle(true)", __LINE__, __FUNCTION__));
		//UpdateSubtitle(true);
		UpdateSubtitle(false);
	}

	return hr;
}

STDMETHODIMP CDirectVobSubFilter::put_VobSubSettings(bool fBuffer, bool fOnlyShowForcedSubs, bool fReserved)
{
	HRESULT hr = CDirectVobSub::put_VobSubSettings(fBuffer, fOnlyShowForcedSubs, fReserved);

	if(hr == NOERROR)
	{
//		UpdateSubtitle(false);
		InvalidateSubtitle();
	}

	return hr;
}

STDMETHODIMP CDirectVobSubFilter::put_TextSettings(void* lf, int lflen, COLORREF color, bool fShadow, bool fOutline, bool fAdvancedRenderer)
{
	HRESULT hr = CDirectVobSub::put_TextSettings(lf, lflen, color, fShadow, fOutline, fAdvancedRenderer);

	if(hr == NOERROR)
	{
//		UpdateSubtitle(true);
		InvalidateSubtitle();
	}

	return hr;
}

STDMETHODIMP CDirectVobSubFilter::put_SubtitleTiming(int delay, int speedmul, int speeddiv)
{
	HRESULT hr = CDirectVobSub::put_SubtitleTiming(delay, speedmul, speeddiv);

	if(hr == NOERROR)
	{
		InvalidateSubtitle();
	}

	return hr;
}

STDMETHODIMP CDirectVobSubFilter::get_CachesInfo(CachesInfo* caches_info)
{
    CAutoLock cAutoLock(&m_csQueueLock);
    HRESULT hr = CDirectVobSub::get_CachesInfo(caches_info);

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

    return hr;
}

STDMETHODIMP CDirectVobSubFilter::get_XyFlyWeightInfo( XyFlyWeightInfo* xy_fw_info )
{
    CAutoLock cAutoLock(&m_csQueueLock);
    HRESULT hr = CDirectVobSub::get_XyFlyWeightInfo(xy_fw_info);
    
    xy_fw_info->xy_fw_string_w.cur_item_num = XyFwStringW::GetCacher()->GetCurItemNum();
    xy_fw_info->xy_fw_string_w.hit_count = XyFwStringW::GetCacher()->GetCacheHitCount();
    xy_fw_info->xy_fw_string_w.query_count = XyFwStringW::GetCacher()->GetQueryCount();

    xy_fw_info->xy_fw_grouped_draw_items_hash_key.cur_item_num = XyFwGroupedDrawItemsHashKey::GetCacher()->GetCurItemNum();
    xy_fw_info->xy_fw_grouped_draw_items_hash_key.hit_count = XyFwGroupedDrawItemsHashKey::GetCacher()->GetCacheHitCount();
    xy_fw_info->xy_fw_grouped_draw_items_hash_key.query_count = XyFwGroupedDrawItemsHashKey::GetCacher()->GetQueryCount();

    return hr;
}

STDMETHODIMP CDirectVobSubFilter::get_MediaFPS(bool* fEnabled, double* fps)
{
	HRESULT hr = CDirectVobSub::get_MediaFPS(fEnabled, fps);

	CComQIPtr<IMediaSeeking> pMS = m_pGraph;
	double rate;
	if(pMS && SUCCEEDED(pMS->GetRate(&rate)))
	{
		m_MediaFPS = rate * m_fps;
		if(fps) *fps = m_MediaFPS;
	}

	return hr;
}

STDMETHODIMP CDirectVobSubFilter::put_MediaFPS(bool fEnabled, double fps)
{
	HRESULT hr = CDirectVobSub::put_MediaFPS(fEnabled, fps);

	CComQIPtr<IMediaSeeking> pMS = m_pGraph;
	if(pMS)
	{
		if(hr == NOERROR)
		{
			hr = pMS->SetRate(m_fMediaFPSEnabled ? m_MediaFPS / m_fps : 1.0);
		}

		double dRate;
		if(SUCCEEDED(pMS->GetRate(&dRate)))
			m_MediaFPS = dRate * m_fps;
	}

	return hr;
}

STDMETHODIMP CDirectVobSubFilter::get_ZoomRect(NORMALIZEDRECT* rect)
{
	return E_NOTIMPL;
}

STDMETHODIMP CDirectVobSubFilter::put_ZoomRect(NORMALIZEDRECT* rect)
{
	return E_NOTIMPL;
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

STDMETHODIMP CDirectVobSubFilter::put_AspectRatioSettings(CSimpleTextSubtitle::EPARCompensationType* ePARCompensationType)
{
	HRESULT hr = CDirectVobSub::put_AspectRatioSettings(ePARCompensationType);

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
	int nLangs;
	if(FAILED(get_LanguageCount(&nLangs))) return E_FAIL;
	return E_FAIL;
	// TODO: temporally disabled since we don't have a new textsub/vobsub editor dlg for dvs yet
//	return(nLangs >= 0 && iSelected < nLangs ? S_OK : E_FAIL);
}

STDMETHODIMP CDirectVobSubFilter::ShowConfigDialog(int iSelected, HWND hWndParent)
{
	// TODO: temporally disabled since we don't have a new textsub/vobsub editor dlg for dvs yet
	return(E_FAIL);
}

///////////////////////////////////////////////////////////////////////////

CDirectVobSubFilter2::CDirectVobSubFilter2(LPUNKNOWN punk, HRESULT* phr, const GUID& clsid) :
	CDirectVobSubFilter(punk, phr, clsid)
{
}

HRESULT CDirectVobSubFilter2::CheckConnect(PIN_DIRECTION dir, IPin* pPin)
{
	CPinInfo pi;
	if(FAILED(pPin->QueryPinInfo(&pi))) return E_FAIL;

	if(CComQIPtr<IDirectVobSub>(pi.pFilter)) return E_FAIL;

	if(dir == PINDIR_INPUT)
	{
		CFilterInfo fi;
		if(SUCCEEDED(pi.pFilter->QueryFilterInfo(&fi))
		&& !wcsnicmp(fi.achName, L"Overlay Mixer", 13))
			return(E_FAIL);
	}
	else
	{
	}

	return __super::CheckConnect(dir, pPin);
}

HRESULT CDirectVobSubFilter2::JoinFilterGraph(IFilterGraph* pGraph, LPCWSTR pName)
{
    XY_AUTO_TIMING(_T("CDirectVobSubFilter2::JoinFilterGraph"));
	if(pGraph)
	{
		if(IsAppBlackListed()) return E_FAIL;

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
						CComQIPtr<IDirectVobSub2>(pDVS)->put_Forced(true);
						CComQIPtr<IGraphConfig>(pGraph)->AddFilterToCache(pDVS);
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
    XY_AUTO_TIMING(_T("CDirectVobSubFilter2::CheckInputType"));
    HRESULT hr = __super::CheckInputType(mtIn);

	if(FAILED(hr) || m_pInput->IsConnected()) return hr;

	if(IsAppBlackListed() || !ShouldWeAutoload(m_pGraph))
		return VFW_E_TYPE_NOT_ACCEPTED;

	GetRidOfInternalScriptRenderer();

	return NOERROR;
}

bool CDirectVobSubFilter2::IsAppBlackListed()
{
	XY_AUTO_TIMING(_T("CDirectVobSubFilter2::IsAppBlackListed"));
	// all entries must be lowercase!
	TCHAR* blacklistedapps[] =
	{
		_T("wm8eutil."), // wmp8 encoder's dummy renderer releases the outputted media sample after calling Receive on its input pin (yes, even when dvobsub isn't registered at all)
		_T("explorer."), // as some users reported thumbnail preview loads dvobsub, I've never experienced this yet...
		_T("producer."), // this is real's producer
		_T("googledesktopindex."), // Google Desktop
		_T("googledesktopdisplay."), // Google Desktop
		_T("googledesktopcrawl."), // Google Desktop
		_T("subtitleworkshop."), // Subtitle Workshop
		_T("subtitleworkshop4."),
		_T("darksouls."), // Dark Souls (Game)
		_T("data."), // Dark Souls (Game - Steam Release)
		_T("rometw."), // Rome Total War (Game)
		_T("everquest2."), // EverQuest II (Game)
		_T("yso_win."), // Ys Origin (Game)
		_T("launcher_main."), // Logitech WebCam Software
		_T("webcamdell2."), // Dell WebCam Software
	};

	for(size_t i = 0; i < _countof(blacklistedapps); i++)
	{
		if(theApp.m_AppName.Find(blacklistedapps[i]) >= 0)
			return(true);
	}

	return(false);
}

bool CDirectVobSubFilter2::ShouldWeAutoload(IFilterGraph* pGraph)
{
	XY_AUTO_TIMING(_T("CDirectVobSubFilter2::ShouldWeAutoload"));
	int level;
	bool m_fExternalLoad, m_fWebLoad, m_fEmbeddedLoad;
	get_LoadSettings(&level, &m_fExternalLoad, &m_fWebLoad, &m_fEmbeddedLoad);

	if(level < 0 || level >= 2) return(false);

	bool fRet = false;

	if(level == 1)
		fRet = m_fExternalLoad = m_fWebLoad = m_fEmbeddedLoad = true;

	// find text stream on known splitters

	if(!fRet && m_fEmbeddedLoad)
	{
		CComPtr<IBaseFilter> pBF;
		if((pBF = FindFilter(CLSID_OggSplitter, pGraph)) || (pBF = FindFilter(CLSID_AviSplitter, pGraph))
		|| (pBF = FindFilter(L"{34293064-02F2-41D5-9D75-CC5967ACA1AB}", pGraph)) // matroska demux
		|| (pBF = FindFilter(L"{0A68C3B5-9164-4a54-AFAF-995B2FF0E0D4}", pGraph)) // matroska source
		|| (pBF = FindFilter(L"{149D2E01-C32E-4939-80F6-C07B81015A7A}", pGraph)) // matroska splitter
		|| (pBF = FindFilter(L"{55DA30FC-F16B-49fc-BAA5-AE59FC65F82D}", pGraph)) // Haali Media Splitter
		|| (pBF = FindFilter(L"{564FD788-86C9-4444-971E-CC4A243DA150}", pGraph)) // Haali Media Splitter (AR)
		|| (pBF = FindFilter(L"{8F43B7D9-9D6B-4F48-BE18-4D787C795EEA}", pGraph)) // Haali Simple Media Splitter
		|| (pBF = FindFilter(L"{52B63861-DC93-11CE-A099-00AA00479A58}", pGraph)) // 3ivx splitter
		|| (pBF = FindFilter(L"{6D3688CE-3E9D-42F4-92CA-8A11119D25CD}", pGraph)) // our ogg source
		|| (pBF = FindFilter(L"{9FF48807-E133-40AA-826F-9B2959E5232D}", pGraph)) // our ogg splitter
		|| (pBF = FindFilter(L"{803E8280-F3CE-4201-982C-8CD8FB512004}", pGraph)) // dsm source
		|| (pBF = FindFilter(L"{0912B4DD-A30A-4568-B590-7179EBB420EC}", pGraph)) // dsm splitter
		|| (pBF = FindFilter(L"{3CCC052E-BDEE-408a-BEA7-90914EF2964B}", pGraph)) // mp4 source
		|| (pBF = FindFilter(L"{61F47056-E400-43d3-AF1E-AB7DFFD4C4AD}", pGraph)) // mp4 splitter
        || (pBF = FindFilter(L"{171252A0-8820-4AFE-9DF8-5C92B2D66B04}", pGraph)) // LAV splitter
        || (pBF = FindFilter(L"{B98D13E7-55DB-4385-A33D-09FD1BA26338}", pGraph)) // LAV Splitter Source
        || (pBF = FindFilter(L"{E436EBB5-524F-11CE-9F53-0020AF0BA770}", pGraph)) // Solveig matroska splitter
        || (pBF = FindFilter(L"{1365BE7A-C86A-473C-9A41-C0A6E82C9FA3}", pGraph)) // MPC-HC MPEG PS/TS/PVA source
        || (pBF = FindFilter(L"{DC257063-045F-4BE2-BD5B-E12279C464F0}", pGraph)) // MPC-HC MPEG splitter
        || (pBF = FindFilter(L"{529A00DB-0C43-4f5b-8EF2-05004CBE0C6F}", pGraph)) // AV splitter
        || (pBF = FindFilter(L"{D8980E15-E1F6-4916-A10F-D7EB4E9E10B8}", pGraph)) // AV source
        ) 
		{
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
			EndEnumFilters
		}
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
		bool fTemp = m_fHideSubtitles;
		fRet = !fn.IsEmpty() && SUCCEEDED(put_FileName((LPWSTR)(LPCWSTR)fn))
			|| SUCCEEDED(put_FileName(L"c:\\tmp.srt"))
			|| fRet;
		if(fTemp) m_fHideSubtitles = true;
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
    AMTRACE((TEXT(__FUNCTION__),0));
    XY_AUTO_TIMING(TEXT("CDirectVobSubFilter::Open"));

	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	CAutoLock cAutolock(&m_csQueueLock);

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
	GetSubFileNames(m_FileName, paths, m_xy_str_opt[DirectVobSubXyOptions::STRING_LOAD_EXT_LIST], ret);

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

	if(S_FALSE == put_SelectedLanguage(FindPreferedLanguage()))
        UpdateSubtitle(false); // make sure pSubPicProvider of our queue gets updated even if the stream number hasn't changed

	m_frd.RefreshEvent.Set();

	return(m_pSubStreams.GetCount() > 0);
}

void CDirectVobSubFilter::UpdateSubtitle(bool fApplyDefStyle)
{
	CAutoLock cAutolock(&m_csQueueLock);

	if(!m_simple_provider) return;

	InvalidateSubtitle();

	CComPtr<ISubStream> pSubStream;

	if(!m_fHideSubtitles)
	{
		int i = m_iSelectedLanguage;

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
	DbgLog((LOG_TRACE, 3, "%s(%d): %s", __FILE__, __LINE__, __FUNCTION__));
	DbgLog((LOG_TRACE, 3, "\tpSubStream:%x fApplyDefStyle:%d", pSubStream, (int)fApplyDefStyle));
    CAutoLock cAutolock(&m_csQueueLock);

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
				pVSS->SetAlignment(m_fOverridePlacement, m_PlacementXperc, m_PlacementYperc, 1, 1);
				pVSS->m_fOnlyShowForcedSubs = m_fOnlyShowForcedVobSubs;
			}
		}
		else if(clsid == __uuidof(CVobSubStream))
		{
			CVobSubSettings* pVSS = dynamic_cast<CVobSubStream*>(pSubStream);

			if(fApplyDefStyle)
			{
				pVSS->SetAlignment(m_fOverridePlacement, m_PlacementXperc, m_PlacementYperc, 1, 1);
				pVSS->m_fOnlyShowForcedSubs = m_fOnlyShowForcedVobSubs;
			}
		}
		else if(clsid == __uuidof(CRenderedTextSubtitle))
		{
			CRenderedTextSubtitle* pRTS = dynamic_cast<CRenderedTextSubtitle*>(pSubStream);

			if(fApplyDefStyle || pRTS->m_fUsingAutoGeneratedDefaultStyle)
			{
				STSStyle s = m_defStyle;

				if(m_fOverridePlacement)
				{
					s.scrAlignment = 2;
					int w = pRTS->m_dstScreenSize.cx;
					int h = pRTS->m_dstScreenSize.cy;
                    CRect tmp_rect = s.marginRect.get();
					int mw = w - tmp_rect.left - tmp_rect.right;
					tmp_rect.bottom = h - MulDiv(h, m_PlacementYperc, 100);
					tmp_rect.left = MulDiv(w, m_PlacementXperc, 100) - mw/2;
					tmp_rect.right = w - (tmp_rect.left + mw);
                    s.marginRect = tmp_rect;
				}

				pRTS->SetDefaultStyle(s);
			}

			pRTS->m_ePARCompensationType = m_ePARCompensationType;
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
				m_iSelectedLanguage = i + pSubStream2->GetStream();
				break;
			}

			i += pSubStream2->GetStreamCount();
		}
	}

	m_nSubtitleId = reinterpret_cast<DWORD_PTR>(pSubStream);

    SetYuvMatrix();

    XySetSize(SIZE_ASS_PLAY_RESOLUTION, playres);
    if(m_simple_provider)
        m_simple_provider->SetSubPicProvider(CComQIPtr<ISubPicProviderEx>(pSubStream));
}

void CDirectVobSubFilter::InvalidateSubtitle(REFERENCE_TIME rtInvalidate, DWORD_PTR nSubtitleId)
{
    CAutoLock cAutolock(&m_csQueueLock);

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
	CAutoLock cAutoLock(&m_csQueueLock);

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
	CAutoLock cAutoLock(&m_csQueueLock);

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
	SetThreadPriority(m_hThread, THREAD_PRIORITY_LOWEST/*THREAD_PRIORITY_BELOW_NORMAL*/);

	CStringArray paths;
	CAtlArray<HANDLE> handles;

	SetupFRD(paths, handles);
    m_frd.ThreadStartedEvent.Set();

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

	for(size_t i = 2; i < handles.GetCount(); i++)
	{
		FindCloseChangeNotification(handles[i]);
	}

	return 0;
}

void CDirectVobSubFilter::GetInputColorspaces( ColorSpaceId *preferredOrder, UINT *count )
{
    ColorSpaceOpt *color_space=NULL;
    int tempCount = 0;
    if( XyGetBin(BIN_INPUT_COLOR_FORMAT, reinterpret_cast<LPVOID*>(&color_space), &tempCount)==S_OK )
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
    ColorSpaceOpt *color_space=NULL;
    int tempCount = 0;
    if( XyGetBin(BIN_OUTPUT_COLOR_FORMAT, reinterpret_cast<LPVOID*>(&color_space), &tempCount)==S_OK )
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
    CAutoLock cAutolock(&m_csQueueLock);

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

void CDirectVobSubFilter::SetYuvMatrix()
{
    ColorConvTable::YuvMatrixType yuv_matrix = ColorConvTable::BT601;
    ColorConvTable::YuvRangeType yuv_range = ColorConvTable::RANGE_TV;

    DXVA2_ExtendedFormat extformat;
    extformat.value = m_cf;

    if ( m_xy_int_opt[INT_COLOR_SPACE]==CDirectVobSub::YuvMatrix_AUTO )
    {
        if (m_video_yuv_matrix_decided_by_sub!=ColorConvTable::NONE)
        {
            yuv_matrix = m_video_yuv_matrix_decided_by_sub;
        }
        else
        {
            yuv_matrix = (extformat.VideoTransferMatrix == DXVA2_VideoTransferMatrix_BT709 ||
                          extformat.VideoTransferMatrix == DXVA2_VideoTransferMatrix_SMPTE240M ||
                          m_xy_size_opt[SIZE_ORIGINAL_VIDEO].cx > m_bt601Width ||
                          m_xy_size_opt[SIZE_ORIGINAL_VIDEO].cy > m_bt601Height) ? ColorConvTable::BT709 : ColorConvTable::BT601;
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
            yuv_range = (extformat.NominalRange == DXVA2_NominalRange_0_255) ? ColorConvTable::RANGE_PC : ColorConvTable::RANGE_TV;
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
        }
    }

    ColorConvTable::SetDefaultConvType(yuv_matrix, yuv_range);
}

// IDirectVobSubXy

STDMETHODIMP CDirectVobSubFilter::XySetBool( int field, bool value )
{
    CAutoLock cAutolock(&m_csQueueLock);
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
    default:
        hr = E_NOTIMPL;
        break;
    }
    return hr;
}

STDMETHODIMP CDirectVobSubFilter::XySetInt( int field, int value )
{
    CAutoLock cAutolock(&m_csQueueLock);
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
    case DirectVobSubXyOptions::INT_SUBPIXEL_POS_LEVEL:
        SubpixelPositionControler::GetGlobalControler().SetSubpixelLevel( static_cast<SubpixelPositionControler::SUBPIXEL_LEVEL>(m_xy_int_opt[field]) );
        break;
    default:
        hr = E_NOTIMPL;
        break;
    }
    
    return hr;
}

//
// OSD
//
void CDirectVobSubFilter::ZeroObj4OSD()
{
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
        lf.lfHeight = 28;
        //MulDiv(20, GetDeviceCaps(hdc, LOGPIXELSY), 54);
        ReleaseDC(NULL, hdc);
        lf.lfWeight = FW_BOLD;
        _tcscpy(lf.lfFaceName, _T("Arial"));
        m_hfont = CreateFontIndirect(&lf);
    }
}

void CDirectVobSubFilter::DeleteObj4OSD()
{
    if(m_hfont) {DeleteObject(m_hfont); m_hfont = 0;}
    if(m_hbm) {DeleteObject(m_hbm); m_hbm = 0;}
    if(m_hdc) {DeleteObject(m_hdc); m_hdc = 0;}
}

void CDirectVobSubFilter::InitObj4OSD()
{
    if(m_hbm) {DeleteObject(m_hbm); m_hbm = NULL;}
    if(m_hdc) {DeleteDC(m_hdc); m_hdc = NULL;}

    struct {BITMAPINFOHEADER bih; DWORD mask[3];} b = {{sizeof(BITMAPINFOHEADER), m_w, -(int)m_h, 1, 32, BI_BITFIELDS, 0, 0, 0, 0, 0}, 0xFF0000, 0x00FF00, 0x0000FF};
    m_hdc = CreateCompatibleDC(NULL);
    m_hbm = CreateDIBSection(m_hdc, (BITMAPINFO*)&b, DIB_RGB_COLORS, NULL, NULL, 0);

    BITMAP bm;
    GetObject(m_hbm, sizeof(bm), &bm);
    memsetd(bm.bmBits, 0xFF000000, bm.bmHeight*bm.bmWidthBytes);
}
