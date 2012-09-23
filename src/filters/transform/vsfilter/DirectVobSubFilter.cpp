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

#include "VSFilter.h"
#include "systray.h"
#include "../../../DSUtil/MediaTypes.h"
#include "../../../SubPic/SimpleSubPicProviderImpl.h"
#include "../../../SubPic/PooledSubPic.h"
#include "../../../subpic/color_conv_table.h"
#include "../../../subpic/SimpleSubPicWrapper.h"

#include <initguid.h>
#include "..\..\..\..\include\moreuuids.h"

#include "CAutoTiming.h"
#include "xy_logger.h"

#include "xy_sub_filter.h"

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
	, m_fMSMpeg4Fix(false)
	, m_fps(25)
{
	DbgLog((LOG_TRACE, 3, _T("CDirectVobSubFilter::CDirectVobSubFilter")));

    // and then, anywhere you need it:

#ifdef __DO_LOG
    LPTSTR  strDLLPath = new TCHAR[_MAX_PATH];
    ::GetModuleFileName( reinterpret_cast<HINSTANCE>(&__ImageBase), strDLLPath, _MAX_PATH);
    CString dllPath = strDLLPath;
    dllPath += ".properties";
    xy_logger::doConfigure( dllPath.GetString() );
#endif

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
	m_pTextInput.Add(new CTextInputPin(this, m_pLock, &m_xy_sub_filter->m_csSubLock, &hr));
	ASSERT(SUCCEEDED(hr));

    m_xy_sub_filter = new XySubFilter(this, 0);

	memset(&m_CurrentVIH2, 0, sizeof(VIDEOINFOHEADER2));

    m_donot_follow_upstream_preferred_order = !m_xy_sub_filter->m_xy_bool_opt[BOOL_FOLLOW_UPSTREAM_PREFERRED_ORDER];

	m_time_alphablt = m_time_rasterization = 0;
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

	for(int i = 0; i < m_pTextInput.GetCount(); i++)
		delete m_pTextInput[i];

    delete m_xy_sub_filter; m_xy_sub_filter = NULL;

	DbgLog((LOG_TRACE, 3, _T("CDirectVobSubFilter::~CDirectVobSubFilter")));
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
		if(CComQIPtr<ISubClock2> pSC2 = m_xy_sub_filter->m_pSubClock)
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
	if(m_xy_sub_filter->m_fFlipPicture) fFlip = !fFlip;
	if(m_fMSMpeg4Fix) fFlip = !fFlip;

	bool fFlipSub = fOutputFlipped;
	if(m_xy_sub_filter->m_fFlipSubtitles) fFlipSub = !fFlipSub;

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
        bool can_transform = (DoCheckTransform(mtIn, mtOut, true)==S_OK);
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
                    hr = DoCheckTransform(&desiredMt, mtOut, true);
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
        if(!can_reconnect && !can_transform)
        {
            position = 0;
            do
            {                
                hr = GetMediaType(position, &desiredMt);
                ++position;
                //if( FAILED(hr) )
                if( hr!=S_OK )
                    break;

                DbgLog((LOG_TRACE, 3, TEXT("Checking reconnect with media type:")));
                DisplayType(0, &desiredMt);

                if( DoCheckTransform(&desiredMt, mtOut, true)!=S_OK ||
                    m_pInput->GetConnected()->QueryAccept(&desiredMt)!=S_OK )
                {
                    continue;
                }
                else
                {
                    can_reconnect = true;
                    break;
                }
            } while ( true );
        }
        if ( can_reconnect )
        {
            if (SUCCEEDED(ReconnectPin(m_pInput, &desiredMt)))
            {
                reconnected = true;
                DumpGraph(m_pGraph,0);
                //m_pInput->SetMediaType(&desiredMt);
                DbgLog((LOG_TRACE, 3, TEXT("reconnected succeed!")));
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
        if(!m_hSystrayThread && !m_xy_sub_filter->m_xy_bool_opt[BOOL_HIDE_TRAY_ICON])
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
		//if(m_pOutput->IsConnected())
		//{
		//	m_pOutput->GetConnected()->Disconnect();
		//	m_pOutput->Disconnect();
		//}
	}
	else if(dir == PINDIR_OUTPUT)
	{
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

	put_MediaFPS(m_xy_sub_filter->m_fMediaFPSEnabled, m_xy_sub_filter->m_MediaFPS);

	return __super::StartStreaming();
}

HRESULT CDirectVobSubFilter::StopStreaming()
{
	m_xy_sub_filter->InvalidateSubtitle();

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
	REFERENCE_TIME rt = m_xy_sub_filter->m_pSubClock ? m_xy_sub_filter->m_pSubClock->GetTime() : m_tPrev;
	return (rt - 10000i64*m_xy_sub_filter->m_SubtitleDelay) * m_xy_sub_filter->m_SubtitleSpeedMul / m_xy_sub_filter->m_SubtitleSpeedDiv; // no, it won't overflow if we use normal parameters (__int64 is enough for about 2000 hours if we multiply it by the max: 65536 as m_SubtitleSpeedMul)
}

void CDirectVobSubFilter::InitSubPicQueue()
{
	CAutoLock cAutoLock(&m_csQueueLock);

    CacheManager::GetPathDataMruCache()->SetMaxItemNum(m_xy_sub_filter->m_xy_int_opt[INT_PATH_DATA_CACHE_MAX_ITEM_NUM]);
    CacheManager::GetScanLineData2MruCache()->SetMaxItemNum(m_xy_sub_filter->m_xy_int_opt[INT_SCAN_LINE_DATA_CACHE_MAX_ITEM_NUM]);
    CacheManager::GetOverlayNoBlurMruCache()->SetMaxItemNum(m_xy_sub_filter->m_xy_int_opt[INT_OVERLAY_NO_BLUR_CACHE_MAX_ITEM_NUM]);
    CacheManager::GetOverlayMruCache()->SetMaxItemNum(m_xy_sub_filter->m_xy_int_opt[INT_OVERLAY_CACHE_MAX_ITEM_NUM]);

    XyFwGroupedDrawItemsHashKey::GetCacher()->SetMaxItemNum(m_xy_sub_filter->m_xy_int_opt[INT_BITMAP_MRU_CACHE_ITEM_NUM]);
    CacheManager::GetBitmapMruCache()->SetMaxItemNum(m_xy_sub_filter->m_xy_int_opt[INT_BITMAP_MRU_CACHE_ITEM_NUM]);

    CacheManager::GetClipperAlphaMaskMruCache()->SetMaxItemNum(m_xy_sub_filter->m_xy_int_opt[INT_CLIPPER_MRU_CACHE_ITEM_NUM]);
    CacheManager::GetTextInfoCache()->SetMaxItemNum(m_xy_sub_filter->m_xy_int_opt[INT_TEXT_INFO_CACHE_ITEM_NUM]);
    CacheManager::GetAssTagListMruCache()->SetMaxItemNum(m_xy_sub_filter->m_xy_int_opt[INT_ASS_TAG_LIST_CACHE_ITEM_NUM]);

    SubpixelPositionControler::GetGlobalControler().SetSubpixelLevel( static_cast<SubpixelPositionControler::SUBPIXEL_LEVEL>(m_xy_sub_filter->m_xy_int_opt[INT_SUBPIXEL_POS_LEVEL]) );

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
    m_simple_provider = new SimpleSubPicProvider2(m_spd.type, CSize(m_w, m_h), window, video_rect, m_xy_sub_filter, &hr);

	if(FAILED(hr)) m_simple_provider = NULL;

    XySetSize(DirectVobSubXyOptions::SIZE_ORIGINAL_VIDEO, CSize(m_w, m_h));
	m_xy_sub_filter->UpdateSubtitle(false);

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
    return m_xy_sub_filter->Count(pcStreams);
}

STDMETHODIMP CDirectVobSubFilter::Enable(long lIndex, DWORD dwFlags)
{
	return m_xy_sub_filter->Enable(lIndex, dwFlags);
}

STDMETHODIMP CDirectVobSubFilter::Info(long lIndex, AM_MEDIA_TYPE** ppmt, DWORD* pdwFlags, LCID* plcid, DWORD* pdwGroup, WCHAR** ppszName, IUnknown** ppObject, IUnknown** ppUnk)
{
	return m_xy_sub_filter->Info(lIndex, ppmt, pdwFlags, plcid, pdwGroup, ppszName, ppObject, ppUnk);
}

STDMETHODIMP CDirectVobSubFilter::GetClassID(CLSID* pClsid)
{
    if(pClsid == NULL) return E_POINTER;
	*pClsid = m_clsid;
    return NOERROR;
}

STDMETHODIMP CDirectVobSubFilter::GetPages(CAUUID* pPages)
{
    return m_xy_sub_filter->GetPages(pPages);
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

				if(pOutPin && GetFilterName(pBF) == _T("Overlay Mixer"))
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

	if(!ShouldWeAutoload(m_pGraph)) return VFW_E_TYPE_NOT_ACCEPTED;

	GetRidOfInternalScriptRenderer();

	return NOERROR;
}

bool CDirectVobSubFilter2::ShouldWeAutoload(IFilterGraph* pGraph)
{
    XY_AUTO_TIMING(_T("CDirectVobSubFilter2::ShouldWeAutoload"));
	TCHAR blacklistedapps[][32] =
	{
		_T("WM8EUTIL."), // wmp8 encoder's dummy renderer releases the outputted media sample after calling Receive on its input pin (yes, even when dvobsub isn't registered at all)
		_T("explorer."), // as some users reported thumbnail preview loads dvobsub, I've never experienced this yet...
		_T("producer."), // this is real's producer
		_T("GoogleDesktopIndex."), // Google Desktop
		_T("GoogleDesktopDisplay."), // Google Desktop
		_T("GoogleDesktopCrawl."), // Google Desktop
	};

	for(int i = 0; i < countof(blacklistedapps); i++)
	{
		if(theApp.m_AppName.Find(blacklistedapps[i]) >= 0)
			return(false);
	}

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
		bool fTemp = m_xy_sub_filter->m_fHideSubtitles;
		fRet = !fn.IsEmpty() && SUCCEEDED(put_FileName((LPWSTR)(LPCWSTR)fn))
			|| SUCCEEDED(put_FileName(L"c:\\tmp.srt"))
			|| fRet;
		if(fTemp) m_xy_sub_filter->m_fHideSubtitles = true;
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

//////////////////////////////////////////////////////////////////////////////////////////

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

//
// IDirectVobSub
// 
STDMETHODIMP CDirectVobSubFilter::get_FileName(WCHAR* fn)
{
    return m_xy_sub_filter->get_FileName(fn);
}

STDMETHODIMP CDirectVobSubFilter::put_FileName(WCHAR* fn)
{
    return m_xy_sub_filter->put_FileName(fn);
}

STDMETHODIMP CDirectVobSubFilter::get_LanguageCount(int* nLangs)
{
    return m_xy_sub_filter->get_LanguageCount(nLangs);
}

STDMETHODIMP CDirectVobSubFilter::get_LanguageName(int iLanguage, WCHAR** ppName)
{
    return m_xy_sub_filter->get_LanguageName(iLanguage, ppName);
}

STDMETHODIMP CDirectVobSubFilter::get_SelectedLanguage(int* iSelected)
{
    return m_xy_sub_filter->get_SelectedLanguage(iSelected);
}

STDMETHODIMP CDirectVobSubFilter::put_SelectedLanguage(int iSelected)
{
    return m_xy_sub_filter->put_SelectedLanguage(iSelected);
}

STDMETHODIMP CDirectVobSubFilter::get_HideSubtitles(bool* fHideSubtitles)
{
    return m_xy_sub_filter->get_HideSubtitles(fHideSubtitles);
}

STDMETHODIMP CDirectVobSubFilter::put_HideSubtitles(bool fHideSubtitles)
{
    return m_xy_sub_filter->put_HideSubtitles(fHideSubtitles);
}

STDMETHODIMP CDirectVobSubFilter::get_PreBuffering(bool* fDoPreBuffering)
{
    return m_xy_sub_filter->get_PreBuffering(fDoPreBuffering);
}

STDMETHODIMP CDirectVobSubFilter::put_PreBuffering(bool fDoPreBuffering)
{
    return m_xy_sub_filter->put_PreBuffering(fDoPreBuffering);
}

STDMETHODIMP CDirectVobSubFilter::get_SubPictToBuffer( unsigned int* uSubPictToBuffer )
{
    return m_xy_sub_filter->get_SubPictToBuffer(uSubPictToBuffer);
}

STDMETHODIMP CDirectVobSubFilter::put_SubPictToBuffer( unsigned int uSubPictToBuffer )
{
    return m_xy_sub_filter->put_SubPictToBuffer(uSubPictToBuffer);
}

STDMETHODIMP CDirectVobSubFilter::get_AnimWhenBuffering( bool* fAnimWhenBuffering )
{
    return m_xy_sub_filter->get_AnimWhenBuffering(fAnimWhenBuffering);
}

STDMETHODIMP CDirectVobSubFilter::put_AnimWhenBuffering( bool fAnimWhenBuffering )
{
    return m_xy_sub_filter->put_AnimWhenBuffering(fAnimWhenBuffering);
}

STDMETHODIMP CDirectVobSubFilter::get_Placement(bool* fOverridePlacement, int* xperc, int* yperc)
{
    return m_xy_sub_filter->get_Placement(fOverridePlacement, xperc, yperc);
}

STDMETHODIMP CDirectVobSubFilter::put_Placement(bool fOverridePlacement, int xperc, int yperc)
{
    return m_xy_sub_filter->put_Placement(fOverridePlacement, xperc, yperc);
}

STDMETHODIMP CDirectVobSubFilter::get_VobSubSettings(bool* fBuffer, bool* fOnlyShowForcedSubs, bool* fPolygonize)
{
    return m_xy_sub_filter->get_VobSubSettings(fBuffer, fOnlyShowForcedSubs, fPolygonize);
}

STDMETHODIMP CDirectVobSubFilter::put_VobSubSettings(bool fBuffer, bool fOnlyShowForcedSubs, bool fPolygonize)
{
    return m_xy_sub_filter->put_VobSubSettings(fBuffer, fOnlyShowForcedSubs, fPolygonize);
}

STDMETHODIMP CDirectVobSubFilter::get_TextSettings(void* lf, int lflen, COLORREF* color, bool* fShadow, bool* fOutline, bool* fAdvancedRenderer)
{
    return m_xy_sub_filter->get_TextSettings(lf, lflen, color, fShadow, fOutline, fAdvancedRenderer);
}

STDMETHODIMP CDirectVobSubFilter::put_TextSettings(void* lf, int lflen, COLORREF color, bool fShadow, bool fOutline, bool fAdvancedRenderer)
{
    return m_xy_sub_filter->put_TextSettings(lf, lflen, color, fShadow, fOutline, fAdvancedRenderer);
}

STDMETHODIMP CDirectVobSubFilter::get_Flip(bool* fPicture, bool* fSubtitles)
{
    return m_xy_sub_filter->get_Flip(fPicture, fSubtitles);
}

STDMETHODIMP CDirectVobSubFilter::put_Flip(bool fPicture, bool fSubtitles)
{
    return m_xy_sub_filter->put_Flip(fPicture, fSubtitles);
}

STDMETHODIMP CDirectVobSubFilter::get_OSD(bool* fOSD)
{
    return m_xy_sub_filter->get_OSD(fOSD);
}

STDMETHODIMP CDirectVobSubFilter::put_OSD(bool fOSD)
{
    return m_xy_sub_filter->put_OSD(fOSD);
}

STDMETHODIMP CDirectVobSubFilter::get_SaveFullPath(bool* fSaveFullPath)
{
    return m_xy_sub_filter->get_SaveFullPath(fSaveFullPath);
}

STDMETHODIMP CDirectVobSubFilter::put_SaveFullPath(bool fSaveFullPath)
{
    return m_xy_sub_filter->put_SaveFullPath(fSaveFullPath);
}

STDMETHODIMP CDirectVobSubFilter::get_SubtitleTiming(int* delay, int* speedmul, int* speeddiv)
{
    return m_xy_sub_filter->get_SubtitleTiming(delay, speedmul, speeddiv);
}

STDMETHODIMP CDirectVobSubFilter::put_SubtitleTiming(int delay, int speedmul, int speeddiv)
{
    return m_xy_sub_filter->put_SubtitleTiming(delay, speedmul, speeddiv);
}

STDMETHODIMP CDirectVobSubFilter::get_MediaFPS(bool* fEnabled, double* fps)
{
    return m_xy_sub_filter->get_MediaFPS(fEnabled, fps);
}

STDMETHODIMP CDirectVobSubFilter::put_MediaFPS(bool fEnabled, double fps)
{
    return m_xy_sub_filter->put_MediaFPS(fEnabled, fps);
}

STDMETHODIMP CDirectVobSubFilter::get_ZoomRect(NORMALIZEDRECT* rect)
{
    return m_xy_sub_filter->get_ZoomRect(rect);
}

STDMETHODIMP CDirectVobSubFilter::put_ZoomRect(NORMALIZEDRECT* rect)
{
    return m_xy_sub_filter->put_ZoomRect(rect);
}

STDMETHODIMP CDirectVobSubFilter::UpdateRegistry()
{
    return m_xy_sub_filter->UpdateRegistry();
}

STDMETHODIMP CDirectVobSubFilter::HasConfigDialog(int iSelected)
{
    return m_xy_sub_filter->HasConfigDialog(iSelected);
}

STDMETHODIMP CDirectVobSubFilter::get_ColorFormat( int* iPosition )
{
    return m_xy_sub_filter->get_ColorFormat(iPosition);
}

STDMETHODIMP CDirectVobSubFilter::put_ColorFormat( int iPosition )
{
    return m_xy_sub_filter->put_ColorFormat(iPosition);
}

STDMETHODIMP CDirectVobSubFilter::ShowConfigDialog(int iSelected, HWND hWndParent)
{
    return m_xy_sub_filter->ShowConfigDialog(iSelected, hWndParent);
}

STDMETHODIMP CDirectVobSubFilter::IsSubtitleReloaderLocked(bool* fLocked)
{
    return m_xy_sub_filter->IsSubtitleReloaderLocked(fLocked);
}

STDMETHODIMP CDirectVobSubFilter::LockSubtitleReloader(bool fLock)
{
    return m_xy_sub_filter->LockSubtitleReloader(fLock);
}

STDMETHODIMP CDirectVobSubFilter::get_SubtitleReloader(bool* fDisabled)
{
    return m_xy_sub_filter->get_SubtitleReloader(fDisabled);
}

STDMETHODIMP CDirectVobSubFilter::put_SubtitleReloader(bool fDisable)
{
    return m_xy_sub_filter->put_SubtitleReloader(fDisable);
}

STDMETHODIMP CDirectVobSubFilter::get_ExtendPicture(int* horizontal, int* vertical, int* resx2, int* resx2minw, int* resx2minh)
{
    return m_xy_sub_filter->get_ExtendPicture(horizontal, vertical, resx2, resx2minw, resx2minh);
}

STDMETHODIMP CDirectVobSubFilter::put_ExtendPicture(int horizontal, int vertical, int resx2, int resx2minw, int resx2minh)
{
    return m_xy_sub_filter->put_ExtendPicture(horizontal, vertical, resx2, resx2minw, resx2minh);
}

STDMETHODIMP CDirectVobSubFilter::get_LoadSettings(int* level, bool* fExternalLoad, bool* fWebLoad, bool* fEmbeddedLoad)
{
    return m_xy_sub_filter->get_LoadSettings(level, fExternalLoad, fWebLoad, fEmbeddedLoad);
}

STDMETHODIMP CDirectVobSubFilter::put_LoadSettings(int level, bool fExternalLoad, bool fWebLoad, bool fEmbeddedLoad)
{
    return m_xy_sub_filter->put_LoadSettings(level, fExternalLoad, fWebLoad, fEmbeddedLoad);
}

// IDirectVobSub2

STDMETHODIMP CDirectVobSubFilter::AdviseSubClock(ISubClock* pSubClock)
{
    return m_xy_sub_filter->AdviseSubClock(pSubClock);
}

STDMETHODIMP_(bool) CDirectVobSubFilter::get_Forced()
{
    return m_xy_sub_filter->get_Forced();
}

STDMETHODIMP CDirectVobSubFilter::put_Forced(bool fForced)
{
    return m_xy_sub_filter->put_Forced(fForced);
}

STDMETHODIMP CDirectVobSubFilter::get_TextSettings(STSStyle* pDefStyle)
{
    return m_xy_sub_filter->get_TextSettings(pDefStyle);
}

STDMETHODIMP CDirectVobSubFilter::put_TextSettings(STSStyle* pDefStyle)
{
    return m_xy_sub_filter->put_TextSettings(pDefStyle);
}

STDMETHODIMP CDirectVobSubFilter::get_AspectRatioSettings(CSimpleTextSubtitle::EPARCompensationType* ePARCompensationType)
{
    return m_xy_sub_filter->get_AspectRatioSettings(ePARCompensationType);
}

STDMETHODIMP CDirectVobSubFilter::put_AspectRatioSettings(CSimpleTextSubtitle::EPARCompensationType* ePARCompensationType)
{
    return m_xy_sub_filter->put_AspectRatioSettings(ePARCompensationType);
}

// IFilterVersion

STDMETHODIMP_(DWORD) CDirectVobSubFilter::GetFilterVersion()
{
    return m_xy_sub_filter->GetFilterVersion();
}

//
// IDirectVobSubXy
// 
STDMETHODIMP CDirectVobSubFilter::XyGetBool( int field, bool *value )
{
    return m_xy_sub_filter->XyGetBool(field, value);
}

STDMETHODIMP CDirectVobSubFilter::XyGetInt( int field, int *value )
{
    return m_xy_sub_filter->XyGetInt(field, value);
}

STDMETHODIMP CDirectVobSubFilter::XyGetSize( int field, SIZE *value )
{
    return m_xy_sub_filter->XyGetSize(field, value);
}

STDMETHODIMP CDirectVobSubFilter::XyGetRect( int field, RECT *value )
{
    return m_xy_sub_filter->XyGetRect(field, value);
}

STDMETHODIMP CDirectVobSubFilter::XyGetUlonglong( int field, ULONGLONG *value )
{
    return m_xy_sub_filter->XyGetUlonglong(field, value);
}

STDMETHODIMP CDirectVobSubFilter::XyGetDouble( int field, double *value )
{
    return m_xy_sub_filter->XyGetDouble(field, value);
}

STDMETHODIMP CDirectVobSubFilter::XyGetString( int field, LPWSTR *value, int *chars )
{
    return m_xy_sub_filter->XyGetString(field, value, chars);
}

STDMETHODIMP CDirectVobSubFilter::XyGetBin( int field, LPVOID *value, int *size )
{
    return m_xy_sub_filter->XyGetBin(field, value, size);
}

STDMETHODIMP CDirectVobSubFilter::XySetBool( int field, bool value )
{
    return m_xy_sub_filter->XySetBool(field, value);
}

STDMETHODIMP CDirectVobSubFilter::XySetInt( int field, int value )
{
    return m_xy_sub_filter->XySetInt(field, value);
}

STDMETHODIMP CDirectVobSubFilter::XySetSize( int field, SIZE value )
{
    return m_xy_sub_filter->XySetSize(field, value);
}

STDMETHODIMP CDirectVobSubFilter::XySetRect( int field, RECT value )
{
    return m_xy_sub_filter->XySetRect(field, value);
}

STDMETHODIMP CDirectVobSubFilter::XySetUlonglong( int field, ULONGLONG value )
{
    return m_xy_sub_filter->XySetUlonglong(field, value);
}

STDMETHODIMP CDirectVobSubFilter::XySetDouble( int field, double value )
{
    return m_xy_sub_filter->XySetDouble(field, value);
}

STDMETHODIMP CDirectVobSubFilter::XySetString( int field, LPWSTR value, int chars )
{
    return m_xy_sub_filter->XySetString(field, value, chars);
}

STDMETHODIMP CDirectVobSubFilter::XySetBin( int field, LPVOID value, int size )
{
    return m_xy_sub_filter->XySetBin(field, value, size);
}
