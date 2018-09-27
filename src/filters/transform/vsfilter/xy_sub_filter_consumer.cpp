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
#include "xy_sub_filter_consumer.h"
#include "DirectVobSubPropPage.h"
#include "VSFilter.h"
#include "../../../DSUtil/MediaTypes.h"
#include "../../../SubPic/SimpleSubPicProviderImpl.h"
#include "../../../SubPic/PooledSubPic.h"
#include "../../../subpic/SimpleSubPicWrapper.h"

#include <initguid.h>
#include "..\..\..\..\include\moreuuids.h"
#include "../../../subtitles/xy_bitmap.h"
#include "../../../subpic/alpha_blender.h"

#if ENABLE_XY_LOG_RENDERER_REQUEST
#  define TRACE_RENDERER_REQUEST(msg) XY_LOG_TRACE(msg)
#else
#  define TRACE_RENDERER_REQUEST(msg)
#endif

///////////////////////////////////////////////////////////////////////////

/*removeme*/
bool g_RegOK = true;//false; // doesn't work with the dvd graph builder
#include "valami.cpp"

#if ENABLE_XY_LOG
EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#endif

using namespace DirectVobSubXyOptions;

const SubRenderOptionsImpl::OptionMap options[] = 
{
    {"name"               , SubRenderOptionsImpl::OPTION_TYPE_STRING   , STRING_NAME           },
    {"version"            , SubRenderOptionsImpl::OPTION_TYPE_STRING   , STRING_VERSION        },
    {"originalVideoSize"  , SubRenderOptionsImpl::OPTION_TYPE_SIZE     , SIZE_ORIGINAL_VIDEO   },
    {"arAdjustedVideoSize", SubRenderOptionsImpl::OPTION_TYPE_SIZE     , SIZE_AR_ADJUSTED_VIDEO},
    {"videoOutputRect"    , SubRenderOptionsImpl::OPTION_TYPE_RECT     , RECT_VIDEO_OUTPUT     },
    {"subtitleTargetRect" , SubRenderOptionsImpl::OPTION_TYPE_RECT     , RECT_SUBTITLE_TARGET  },
    {"frameRate"          , SubRenderOptionsImpl::OPTION_TYPE_ULONGLONG, ULONGLOG_FRAME_RATE   },
    {"refreshRate"        , SubRenderOptionsImpl::OPTION_TYPE_DOUBLE   , DOUBLE_REFRESH_RATE   },
    {"yuvMatrix"          , SubRenderOptionsImpl::OPTION_TYPE_STRING   , STRING_YUV_MATRIX     },
    {0}
};

const REFERENCE_TIME DEFAULT_FRAME_RATE = 10000000.0/25;

#ifndef CONSUMRE_MERIT
#  define CONSUMRE_MERIT 0x00020000
#endif

////////////////////////////////////////////////////////////////////////////
//
// Constructor
//

XySubFilterConsumer::XySubFilterConsumer(LPUNKNOWN punk, HRESULT* phr, const GUID& clsid)
    : CDirectVobSub(XyConsumerVobFilterOptions, &m_csSubLock)
    , CBaseVideoFilter(NAME("XySubFilterConsumer"), punk, phr, clsid)
    , m_fMSMpeg4Fix(false)
    , SubRenderOptionsImpl(::options, this)
{
    XY_LOG_INFO("Construct XySubFilterConsumer");
    m_xy_str_opt[STRING_NAME] = L"XySubFilterConsumer";
    m_xy_ulonglong_opt[ULONGLOG_FRAME_RATE] = DEFAULT_FRAME_RATE;

    // and then, anywhere you need it:

    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    ZeroObj4OSD();

    theApp.WriteProfileString(ResStr(IDS_R_DEFTEXTPATHES), _T("Hint"), _T("The first three are fixed, but you can add more up to ten entries."));

    CString tmp;
    tmp.Format(ResStr(IDS_RP_PATH), 0);
    theApp.WriteProfileString(ResStr(IDS_R_DEFTEXTPATHES), tmp, _T("."));
    tmp.Format(ResStr(IDS_RP_PATH), 1);
    theApp.WriteProfileString(ResStr(IDS_R_DEFTEXTPATHES), tmp, _T("c:\\subtitles"));
    tmp.Format(ResStr(IDS_RP_PATH), 2);
    theApp.WriteProfileString(ResStr(IDS_R_DEFTEXTPATHES), tmp, _T(".\\subtitles"));

    memset(&m_CurrentVIH2, 0, sizeof(VIDEOINFOHEADER2));

    m_donot_follow_upstream_preferred_order = !m_xy_bool_opt[BOOL_FOLLOW_UPSTREAM_PREFERRED_ORDER];

    m_video_yuv_matrix_decided_by_sub = ColorConvTable::NONE;
    m_video_yuv_range_decided_by_sub = ColorConvTable::RANGE_NONE;
}

XySubFilterConsumer::~XySubFilterConsumer()
{
    CAutoLock cAutoLock(&m_csSubLock);

    DeleteObj4OSD();

    XY_LOG_INFO("Deconstructed");
}

STDMETHODIMP XySubFilterConsumer::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
    CheckPointer(ppv, E_POINTER);

    return
        QI(IDirectVobSub)
        QI(IDirectVobSub2)
        QI(IXyOptions)
        QI(IFilterVersion)
        QI(ISpecifyPropertyPages)
        QI(ISubRenderConsumer)
        __super::NonDelegatingQueryInterface(riid, ppv);
}

// CBaseVideoFilter

void XySubFilterConsumer::GetOutputSize(int& w, int& h, int& arx, int& ary)
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

HRESULT XySubFilterConsumer::TryNotCopy(IMediaSample* pIn, const CMediaType& mt, const BITMAPINFOHEADER& bihIn )
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

HRESULT XySubFilterConsumer::Transform(IMediaSample* pIn)
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

        m_xy_ulonglong_opt[ULONGLOG_FRAME_RATE] = rtAvgTimePerFrame * dRate;
    }

    //

    {
        CAutoLock cAutoLock(&m_csSubLock);
        m_sub_render_frame = NULL;
        if(m_provider)
        {
            m_provider->RequestFrame(rtStart, rtStop, 0);//fix me: important! we've assumed provider would call DeliverFrame 
                                                         // in the same thread immediately, so that contextid is ignored.
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

        if(m_sub_render_frame)
        {
            if(fFlip ^ fFlipSub)
                spd.h = -spd.h;

            TRACE_RENDERER_REQUEST("AlphaBlt");
            hr = AlphaBlt(spd);
            if (FAILED(hr))
            {
                XY_LOG_ERROR("Failed to alphablt."<<XY_LOG_VAR_2_STR(hr));
            }
            TRACE_RENDERER_REQUEST("AlphaBlt finished");
        }
    }
    CopyBuffer(pDataOut, (BYTE*)spd.bits, spd.w, abs(spd.h)*(fFlip?-1:1), spd.pitch, mt.subtype);

    PrintMessages(pDataOut);
    return m_pOutput->Deliver(pOut);
}

HRESULT XySubFilterConsumer::AlphaBlt(SubPicDesc& spd)
{
    HRESULT hr = NOERROR;
    XySubPicPlan dst = { spd.w, spd.h, spd.pitch, spd.bits };
    ASSERT(m_sub_render_frame);

    int count = 0;
    hr = m_sub_render_frame->GetBitmapCount(&count);
    ASSERT(SUCCEEDED(hr));
    for (int i=0;i<count;i++)
    {
        XySubPicPlan sub;
        CPoint pos;
        SIZE size;
        hr = m_sub_render_frame->GetBitmap(i, NULL, &pos, &size, (LPCVOID*)&sub.bits, &sub.pitch );
        if (FAILED(hr))
        {
            XY_LOG_ERROR("Failed to get bitmap. "<<XY_LOG_VAR_2_STR(hr));
            return hr;
        }
        sub.w = size.cx;
        sub.h = size.cy;
        BaseAlphaBlender::DefaultAlphaBlend(&sub, &dst, pos, size);
    }
    if (FAILED(hr))
    {
        XY_LOG_ERROR("Failed to unlock. "<<XY_LOG_VAR_2_STR(hr));
        return hr;
    }
    return hr;
}

// CBaseFilter

HRESULT XySubFilterConsumer::JoinFilterGraph(IFilterGraph* pGraph, LPCWSTR pName)
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

    return __super::JoinFilterGraph(pGraph, pName);
}

STDMETHODIMP XySubFilterConsumer::QueryFilterInfo(FILTER_INFO* pInfo)
{
    CheckPointer(pInfo, E_POINTER);
    ValidateReadWritePtr(pInfo, sizeof(FILTER_INFO));

    if(!get_Forced()) {
        swprintf_s(pInfo->achName, L"XySubFilterConsumer (merit:0x%x)", CONSUMRE_MERIT);
    }
    else {
        swprintf_s(pInfo->achName, L"XySubFilterConsumer (merit:0x%x|forced loading)", CONSUMRE_MERIT);
    }
    if(pInfo->pGraph = m_pGraph) m_pGraph->AddRef();

    return S_OK;
}

// CTransformFilter

HRESULT XySubFilterConsumer::SetMediaType(PIN_DIRECTION dir, const CMediaType* pmt)
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

        m_xy_ulonglong_opt[ULONGLOG_FRAME_RATE] = atpf ? atpf : DEFAULT_FRAME_RATE;

        if (pmt->formattype == FORMAT_VideoInfo2)
            m_CurrentVIH2 = *(VIDEOINFOHEADER2*)pmt->Format();

        InitConsumerFields();
    }
    else if(dir == PINDIR_OUTPUT)
    {

    }

    return hr;
}

HRESULT XySubFilterConsumer::CheckConnect(PIN_DIRECTION dir, IPin* pPin)
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

HRESULT XySubFilterConsumer::CompleteConnect(PIN_DIRECTION dir, IPin* pReceivePin)
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
        m_pInput->SetMediaType( &m_pInput->CurrentMediaType() );
    }

    HRESULT hr = __super::CompleteConnect(dir, pReceivePin);
    XY_LOG_INFO(_T("Connection fininshed!")<<XY_LOG_VAR_2_STR(hr));
    DumpGraph(m_pGraph,0);
    return hr;
}

HRESULT XySubFilterConsumer::BreakConnect(PIN_DIRECTION dir)
{
    XY_LOG_INFO(dir);
    if(dir == PINDIR_INPUT)
    {
        //if(m_pOutput->IsConnected())
        //{
        //	m_pOutput->GetConnected()->Disconnect();
        //	m_pOutput->Disconnect();
        //}
        m_fMSMpeg4Fix = false;
    }
    else if(dir == PINDIR_OUTPUT)
    {
    }

    return __super::BreakConnect(dir);
}

HRESULT XySubFilterConsumer::StartStreaming()
{
    XY_LOG_INFO("");
    /* WARNING: calls to m_pGraph member functions from within this function will generate deadlock with Haali
     * Video Renderer in MPC. Reason is that CAutoLock's variables in IFilterGraph functions are overriden by
     * CFGManager class.
     */

    HRESULT hr = NOERROR;

    InitConsumerFields();

    return __super::StartStreaming();
}

HRESULT XySubFilterConsumer::StopStreaming()
{
    XY_LOG_INFO("");
    return __super::StopStreaming();
}

HRESULT XySubFilterConsumer::NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate)
{
    TRACE_RENDERER_REQUEST(XY_LOG_VAR_2_STR(tStart)<<XY_LOG_VAR_2_STR(tStop)<<XY_LOG_VAR_2_STR(dRate));
    m_tPrev = tStart;
    return __super::NewSegment(tStart, tStop, dRate);
}

//

REFERENCE_TIME XySubFilterConsumer::CalcCurrentTime()
{
    REFERENCE_TIME rt = m_pSubClock ? m_pSubClock->GetTime() : m_tPrev;
    return (rt - 10000i64*m_SubtitleDelay) * m_SubtitleSpeedMul / m_SubtitleSpeedDiv; // no, it won't overflow if we use normal parameters (__int64 is enough for about 2000 hours if we multiply it by the max: 65536 as m_SubtitleSpeedMul)
}

void XySubFilterConsumer::InitConsumerFields()
{
    XY_LOG_INFO("");
    CAutoLock cAutoLock(&m_csSubLock);

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

    CSize video(bihIn.biWidth, bihIn.biHeight);

    HRESULT hr = S_OK;

    m_xy_size_opt[SIZE_ORIGINAL_VIDEO   ].SetSize(m_w, m_h);
    m_xy_size_opt[SIZE_AR_ADJUSTED_VIDEO].SetSize(m_w, m_h);//fix me: important! AR adjusted video size not filled properly
    m_xy_rect_opt[RECT_VIDEO_OUTPUT     ].SetRect(0, 0, m_w, m_h);
    m_xy_rect_opt[RECT_SUBTITLE_TARGET  ].SetRect(0, 0, m_w, m_h);

    InitObj4OSD();
}

bool XySubFilterConsumer::AdjustFrameSize(CSize& s)
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

STDMETHODIMP XySubFilterConsumer::GetClassID(CLSID* pClsid)
{
    if(pClsid == NULL) return E_POINTER;
    *pClsid = m_clsid;
    return NOERROR;
}

STDMETHODIMP XySubFilterConsumer::GetPages(CAUUID* pPages)
{
    CheckPointer(pPages, E_POINTER);

    pPages->cElems = 4;
    pPages->pElems = (GUID*)CoTaskMemAlloc(sizeof(GUID)*pPages->cElems);

    if(pPages->pElems == NULL) return E_OUTOFMEMORY;

    int i = 0;
    pPages->pElems[i++] = __uuidof(CXySubFilterConsumerGeneralPPage);
    pPages->pElems[i++] = __uuidof(CXySubFilterConsumerMiscPPage);
    pPages->pElems[i++] = __uuidof(CXySubFilterConsumerColorPPage);
    pPages->pElems[i++] = __uuidof(CXySubFilterConsumerAboutPPage);

    return S_OK;
}

//////////////////////////////////////////////////////////////////////////////////////////

void XySubFilterConsumer::GetInputColorspaces( ColorSpaceId *preferredOrder, UINT *count )
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

void XySubFilterConsumer::GetOutputColorspaces( ColorSpaceId *preferredOrder, UINT *count )
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

// ISubRenderConsumer

STDMETHODIMP XySubFilterConsumer::GetMerit( ULONG *merit )
{
    CheckPointer(merit, E_POINTER);
    *merit = CONSUMRE_MERIT;//Video Post Processor
    return S_OK;
}

STDMETHODIMP XySubFilterConsumer::Connect( ISubRenderProvider *subtitleRenderer )
{
    CheckPointer(subtitleRenderer, E_POINTER);
    HRESULT hr = S_OK;
    if (m_provider!=subtitleRenderer)
    {
        if (m_provider!=NULL)
        {
            hr = m_provider->Disconnect();
            if (FAILED(hr))
            {
                XY_LOG_WARN("Failed to Connect with previous provider.");
            }
            m_provider = NULL;
        }

        hr = subtitleRenderer->SetBool("useDestAlpha", true);
        if (FAILED(hr))
        {
            XY_LOG_ERROR("Provider does NOT support useDestAlpha. Connection Failed.");
            return hr;
        }

        m_provider = subtitleRenderer;

        BaseAlphaBlender::SetupDefaultAlphaBlender(MSP_RGBA, MSP_RGB32, g_cpuid.m_flags);
        return S_OK;
    }
    else
    {
        XY_LOG_WARN("This provider has connected! "<<(void*)subtitleRenderer);
    }
    return S_FALSE;
}

STDMETHODIMP XySubFilterConsumer::Disconnect(void)
{
    XY_LOG_INFO(XY_LOG_VAR_2_STR(this));
    static bool entered = false;
    //If the provider also calls provider->Disconnect inside its Disconnect implementation, 
    //we won't fall into a dead loop.
    if (!entered)
    {
        entered = true;
        this->AddRef();//so that we won't be destroy because a call to m_provider->Disconnect
        CAutoLock cAutoLock(&m_csSubLock);

        ASSERT(m_provider);
        if (m_provider)
        {
            HRESULT hr = m_provider->Disconnect();
            if (FAILED(hr))
            {
                XY_LOG_WARN("Failed to disconnect with provider!");
            }
            m_provider = NULL;
        }
        else
        {
            XY_LOG_ERROR("No provider connected. It is expected to be called by a provider!");
        }
        this->Release();//it is OK now
        entered = false;
        return S_OK;
    }
    return S_FALSE;
}

STDMETHODIMP XySubFilterConsumer::DeliverFrame( REFERENCE_TIME start, REFERENCE_TIME stop, LPVOID context, ISubRenderFrame *subtitleFrame )
{
    CAutoLock cAutoLock(&m_csSubLock);
    //fix me
    m_sub_render_frame = subtitleFrame;
    return S_FALSE;
}

void XySubFilterConsumer::SetYuvMatrix()
{
#if FIX_ME
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
            yuv_matrix = ColorConvTable::BT601;
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
#endif
}

//
// IXyOptions
//

STDMETHODIMP XySubFilterConsumer::XySetBool( unsigned field, bool value )
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
        hr = E_NOTIMPL;
        break;
    default:
        break;
    }
    return hr;
}

STDMETHODIMP XySubFilterConsumer::XySetInt( unsigned field, int value )
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
    default:
        break;
    }

    return hr;
}

//
// OSD
//
void XySubFilterConsumer::ZeroObj4OSD()
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
        _tcscpy_s(lf.lfFaceName, _T("Arial"));
        m_hfont = CreateFontIndirect(&lf);
    }
}

void XySubFilterConsumer::DeleteObj4OSD()
{
    XY_LOG_INFO("");
    if(m_hfont) {DeleteObject(m_hfont); m_hfont = 0;}
    if(m_hbm) {DeleteObject(m_hbm); m_hbm = 0;}
    if(m_hdc) {DeleteObject(m_hdc); m_hdc = 0;}
}

void XySubFilterConsumer::InitObj4OSD()
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

static void LogSubPicStartStop( const REFERENCE_TIME& rtStart, const REFERENCE_TIME& rtStop, const CString& msg)
{
#if ENABLE_XY_LOG
    static REFERENCE_TIME s_rtStart = -1, s_rtStop = -1;
    if(s_rtStart!=rtStart || s_rtStop!=rtStop)
    {
        XY_LOG_INFO(msg.GetString());
        s_rtStart=rtStart;
        s_rtStop=rtStop;
    }
#endif
}

void BltLineRGB32(DWORD* d, BYTE* sub, int w, const GUID& subtype)
{
    if(subtype == MEDIASUBTYPE_YV12 || subtype == MEDIASUBTYPE_I420 || subtype == MEDIASUBTYPE_IYUV 
        || subtype == MEDIASUBTYPE_NV12 || subtype == MEDIASUBTYPE_NV21)
    {
        //TODO: Fix ME!
        BYTE* db = (BYTE*)d;
        BYTE* dbtend = db + w;

        for(; db < dbtend; sub++, db++)
        {
            *db = (*db+*sub)>>1;
        }
    }
    else if(subtype == MEDIASUBTYPE_P010 || subtype == MEDIASUBTYPE_P016)
    {
        //TODO: Fix ME!
        WORD* db = reinterpret_cast<WORD*>(d);
        WORD* dbtend = db + w;

        for(; db < dbtend; sub++, db++)
        {
            *db = (*db + (*sub<<8))>>1;
        }
    }
    else if(subtype == MEDIASUBTYPE_YUY2)
    {
        BYTE* ds = (BYTE*)d;
        BYTE* dstend = ds + w*2;

        for(; ds < dstend; sub++, ds+=2)
        {
            ds[0] = (ds[0]+*sub)>>1;
        }
    }
    else if (subtype == MEDIASUBTYPE_AYUV)
    {
        BYTE* db = (BYTE*)d;
        BYTE* dbtend = db + w*4;

        for(; db < dbtend; sub++, db+=4)
        {
            db[2] = (db[2]+*sub)>>1;
        }
    }
    else if(subtype == MEDIASUBTYPE_RGB555)
    {
        WORD* ds = (WORD*)d;
        WORD* dstend = ds + w;

        for(; ds < dstend; sub++, ds++)
        {
            DWORD tmp = *ds;
            WORD tmpsub = *sub;
            *ds  = ((tmp>>10) + tmpsub)>>1<<10;
            *ds |= (((tmp>>5)&0x001f) + tmpsub)>>1<<5;
            *ds |= ((tmp&0x001f) + tmpsub)>>1;
        }
    }
    else if(subtype == MEDIASUBTYPE_RGB565)
    {
        WORD* ds = (WORD*)d;
        WORD* dstend = ds + w;

        for(; ds < dstend; sub++, ds++)
        {
            WORD tmp = *ds;
            *ds  = ((tmp>>11) + (*sub>>3))>>1<<11;
            *ds |= (((tmp>>5)&0x003f) + (*sub>>2))>>1<<5;
            *ds |= ((tmp&0x001f) + (*sub>>3))>>1;
        }
    }
    else if(subtype == MEDIASUBTYPE_RGB24)
    {
        BYTE* dt = (BYTE*)d;
        BYTE* dstend = dt + w*3;

        for(; dt < dstend; sub++, dt+=3)
        {
            dt[0] = (dt[0]+*sub)>>1;
            dt[1] = (dt[1]+*sub)>>1;
            dt[2] = (dt[2]+*sub)>>1;
        }
    }
    else if(subtype == MEDIASUBTYPE_RGB32 || subtype == MEDIASUBTYPE_ARGB32)
    {
        BYTE* dt = (BYTE*)d;
        BYTE* dstend = dt + w*4;

        for(; dt < dstend; sub++, dt+=4)
        {
            dt[0] = (dt[0]+*sub)>>1;
            dt[1] = (dt[1]+*sub)>>1;
            dt[2] = (dt[2]+*sub)>>1;
        }
    }
}

HRESULT Copy(BYTE* pSub, BYTE* pIn, CSize sub, CSize in, int bpp, const GUID& subtype, DWORD black)
{
    int wIn = in.cx, hIn = in.cy, pitchIn = wIn*bpp>>3;
    int wSub = sub.cx, hSub = sub.cy, pitchSub = wSub*bpp>>3;
    bool fScale2x = wIn*2 <= wSub;

    if(fScale2x) wIn <<= 1, hIn <<= 1;

    int left = ((wSub - wIn)>>1)&~1;
    int mid = wIn;
    int right = left + ((wSub - wIn)&1);

    int dpLeft = left*bpp>>3;
    int dpMid = mid*bpp>>3;
    int dpRight = right*bpp>>3;

    ASSERT(wSub >= wIn);

    {
        int i = 0, j = 0;

        j += (hSub - hIn) >> 1;

        for(; i < j; i++, pSub += pitchSub)
        {
            memsetd(pSub, black, dpLeft+dpMid+dpRight);
        }

        j += hIn;

        if(hIn > hSub)
            pIn += pitchIn * ((hIn - hSub) >> (fScale2x?2:1));

        if(fScale2x)
        {
            ASSERT(0);
            return E_NOTIMPL;
        }
        else
        {
            for(int k = min(j, hSub); i < k; i++, pIn += pitchIn, pSub += pitchSub)
            {
                memsetd(pSub, black, dpLeft);
                memcpy(pSub + dpLeft, pIn, dpMid);
                memsetd(pSub + dpLeft+dpMid, black, dpRight);
            }
        }

        j = hSub;

        for(; i < j; i++, pSub += pitchSub)
        {
            memsetd(pSub, black, dpLeft+dpMid+dpRight);
        }
    }

    return NOERROR;
}

HRESULT XySubFilterConsumer::Copy(BYTE* pSub, BYTE* pIn, CSize sub, CSize in, int bpp, const GUID& subtype, DWORD black)
{
    return ::Copy(pSub, pIn, sub, in, bpp, subtype, black);	
}

void XySubFilterConsumer::PrintMessages(BYTE* pOut)
{
    if(!m_hdc || !m_hbm)
        return;

    const GUID& subtype = m_pOutput->CurrentMediaType().subtype;

    BITMAPINFOHEADER bihOut;
    ExtractBIH(&m_pOutput->CurrentMediaType(), &bihOut);

    CString msg, tmp;

    if(m_xy_bool_opt[DirectVobSubXyOptions::BOOL_OSD])
    {
        tmp.Format(_T("in: %dx%d %s\nout: %dx%d %s\n"), 
            m_w, m_h, 
            Subtype2String(m_pInput->CurrentMediaType().subtype),
            bihOut.biWidth, bihOut.biHeight, 
            Subtype2String(m_pOutput->CurrentMediaType().subtype));
        msg += tmp;

        tmp.Format(_T("fps: %.3f, media time: %d, subtitle time: %d [ms]\nframe number: %d (calculated)\nrate: %.4f\n"), 
            10000000.0/m_xy_ulonglong_opt[ULONGLOG_FRAME_RATE], 
            (int)m_tPrev.Millisecs(), (int)(CalcCurrentTime()/10000),
            (int)(m_tPrev.m_time / m_xy_ulonglong_opt[ULONGLOG_FRAME_RATE]),
            m_pInput->CurrentRate());
        msg += tmp;

#if FIX_ME
        CAutoLock cAutoLock(&m_csSubLock);
        if(m_provider)
        {
            int nSubPics = -1;
            REFERENCE_TIME rtNow = -1, rtStart = -1, rtStop = -1;
            m_provider->GetStats(nSubPics, rtNow, rtStart, rtStop);
            tmp.Format(_T("queue stats: %I64d - %I64d [ms]\n"), rtStart/10000, rtStop/10000);
            msg += tmp;

            for(int i = 0; i < nSubPics; i++)
            {
                m_provider->GetStats(i, rtStart, rtStop);
                tmp.Format(_T("%d: %I64d - %I64d [ms]\n"), i, rtStart/10000, rtStop/10000);
                msg += tmp;
            }
            LogSubPicStartStop(rtStart, rtStop, tmp);
        }

        //color space
        tmp.Format( _T("Colorspace: %ls %ls (%ls)\n"), 
            ColorConvTable::GetDefaultRangeType()==ColorConvTable::RANGE_PC ? _T("PC"):_T("TV"),
            ColorConvTable::GetDefaultYUVType()==ColorConvTable::BT601 ? _T("BT.601"):(ColorConvTable::GetDefaultYUVType()==ColorConvTable::BT2020 ? _T("BT.2020"):_T("BT.709")),
            m_xy_int_opt[INT_COLOR_SPACE]==CDirectVobSub::YuvMatrix_AUTO ? _T("Auto") :
            m_xy_int_opt[INT_COLOR_SPACE]==CDirectVobSub::GUESS ? _T("Guessed") : _T("Forced") );
        msg += tmp;
#endif
        CComQIPtr<IXyOptions> provider = m_provider;
        if (provider)
        {
            //print cache info
            SIZE layout_size, script_playres;
            provider->XyGetSize(SIZE_LAYOUT_WITH, &layout_size);
            provider->XyGetSize(SIZE_ASS_PLAY_RESOLUTION, &script_playres);
            tmp.Format( _T("Layout with size %dx%d, script playres %dx%d\n"),
                layout_size.cx, layout_size.cy, script_playres.cx, script_playres.cy );
            msg += tmp;

            //print cache info
            CachesInfo caches_info;
            XyFlyWeightInfo xy_fw_info;
            provider->XyGetBin2(DirectVobSubXyOptions::BIN2_CACHES_INFO, reinterpret_cast<LPVOID>(&caches_info), sizeof(caches_info));
            provider->XyGetBin2(DirectVobSubXyOptions::BIN2_XY_FLY_WEIGHT_INFO, reinterpret_cast<LPVOID>(&xy_fw_info), sizeof(xy_fw_info));
            tmp.Format(
                _T("Cache :stored_num/hit_count/query_count\n")\
                _T("  Parser cache 1:%ld/%ld/%ld\n")\
                _T("  Parser cache 2:%ld/%ld/%ld\n")\
                _T("\n")\
                _T("  LV 4:%ld/%ld/%ld\t\t")\
                _T("  LV 3:%ld/%ld/%ld\n")\
                _T("  LV 2:%ld/%ld/%ld\t\t")\
                _T("  LV 1:%ld/%ld/%ld\n")\
                _T("  LV 0:%ld/%ld/%ld\n")\
                _T("\n")\
                _T("  bitmap   :%ld/%ld/%ld\t\t")\
                _T("  scan line:%ld/%ld/%ld\n")\
                _T("  relay key:%ld/%ld/%ld\t\t")\
                _T("  clipper  :%ld/%ld/%ld\n")\
                _T("\n")\
                _T("  FW string pool    :%ld/%ld/%ld\t\t")\
                _T("  FW bitmap key pool:%ld/%ld/%ld\n")\
                ,
                caches_info.text_info_cache_cur_item_num, caches_info.text_info_cache_hit_count, caches_info.text_info_cache_query_count,
                caches_info.word_info_cache_cur_item_num, caches_info.word_info_cache_hit_count, caches_info.word_info_cache_query_count,
                caches_info.path_cache_cur_item_num,     caches_info.path_cache_hit_count,     caches_info.path_cache_query_count,
                caches_info.scanline_cache2_cur_item_num, caches_info.scanline_cache2_hit_count, caches_info.scanline_cache2_query_count,
                caches_info.non_blur_cache_cur_item_num, caches_info.non_blur_cache_hit_count, caches_info.non_blur_cache_query_count,
                caches_info.overlay_cache_cur_item_num,  caches_info.overlay_cache_hit_count,  caches_info.overlay_cache_query_count,
                caches_info.interpolate_cache_cur_item_num, caches_info.interpolate_cache_hit_count, caches_info.interpolate_cache_query_count,
                caches_info.bitmap_cache_cur_item_num, caches_info.bitmap_cache_hit_count, caches_info.bitmap_cache_query_count,
                caches_info.scanline_cache_cur_item_num, caches_info.scanline_cache_hit_count, caches_info.scanline_cache_query_count,
                caches_info.overlay_key_cache_cur_item_num, caches_info.overlay_key_cache_hit_count, caches_info.overlay_key_cache_query_count,
                caches_info.clipper_cache_cur_item_num, caches_info.clipper_cache_hit_count, caches_info.clipper_cache_query_count,

                xy_fw_info.xy_fw_string_w.cur_item_num, xy_fw_info.xy_fw_string_w.hit_count, xy_fw_info.xy_fw_string_w.query_count,
                xy_fw_info.xy_fw_grouped_draw_items_hash_key.cur_item_num, xy_fw_info.xy_fw_grouped_draw_items_hash_key.hit_count, xy_fw_info.xy_fw_grouped_draw_items_hash_key.query_count
                );
            msg += tmp;
        }
    }

    if(msg.IsEmpty()) return;

    HANDLE hOldBitmap = SelectObject(m_hdc, m_hbm);
    HANDLE hOldFont = SelectObject(m_hdc, m_hfont);

    SetTextColor(m_hdc, 0xffffff);
    SetBkMode(m_hdc, TRANSPARENT);
    SetMapMode(m_hdc, MM_TEXT);

    BITMAP bm;
    GetObject(m_hbm, sizeof(BITMAP), &bm);

    const int MARGIN = 8;
    CRect r(MARGIN, MARGIN, bm.bmWidth, bm.bmHeight);
    DrawText(m_hdc, msg, _tcslen(msg), &r, DT_CALCRECT|DT_EXTERNALLEADING|DT_NOPREFIX|DT_WORDBREAK);

    r &= CRect(0, 0, bm.bmWidth, bm.bmHeight);

    DrawText(m_hdc, msg, _tcslen(msg), &r, DT_LEFT|DT_TOP|DT_NOPREFIX|DT_WORDBREAK);

    BYTE* pIn = (BYTE*)bm.bmBits;

    int pitchIn = bm.bmWidthBytes;

    int pitchOut = bihOut.biWidth * bihOut.biBitCount >> 3;

    if( subtype == MEDIASUBTYPE_YV12 || subtype == MEDIASUBTYPE_I420 || subtype == MEDIASUBTYPE_IYUV 
        || subtype== MEDIASUBTYPE_NV12 || subtype==MEDIASUBTYPE_NV21 ) {
        pitchOut = bihOut.biWidth;
    }
    else if (subtype == MEDIASUBTYPE_P010 || subtype == MEDIASUBTYPE_P016) {
        pitchOut = bihOut.biWidth * 2;
    }

    if(bihOut.biHeight > 0 && bihOut.biCompression <= 3) // flip if the dst bitmap is flipped rgb (m_hbm is a top-down bitmap, not like the subpictures)
    {
        pOut += pitchOut * (abs(bihOut.biHeight)-1);
        pitchOut = -pitchOut;
    }

    for(int w = min(r.right + MARGIN, m_w), h = min(r.bottom,abs(m_h)); h--; pIn += pitchIn, pOut += pitchOut)
    {
        BltLineRGB32((DWORD*)pOut, pIn, w, subtype);
        memset(pIn+r.left, 0, r.Width());
    }

    SelectObject(m_hdc, hOldBitmap);
    SelectObject(m_hdc, hOldFont);
}
