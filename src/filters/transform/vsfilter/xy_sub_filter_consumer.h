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

#pragma once

#include <atlsync.h>
#include "DirectVobSub.h"
#include "../BaseVideoFilter/BaseVideoFilter.h"
#include "../../../subtitles/VobSubFile.h"
#include "../../../subtitles/RTS.h"
#include "../../../subtitles/SSF.h"
#include "../../../subpic/ISimpleSubPic.h"
#include "SubRenderIntf.h"


[uuid("db6974b5-9bd1-45f5-8462-27a88019b352")]
class XySubFilterConsumer
    : public CBaseVideoFilter
    , public CDirectVobSub
    , public ISpecifyPropertyPages
    , public SubRenderOptionsImpl
    , public ISubRenderConsumer
{
    CComPtr<ISubRenderProvider> m_provider;
    CComPtr<ISubRenderFrame> m_sub_render_frame;

    void InitConsumerFields();
    SubPicDesc m_spd;

    bool AdjustFrameSize(CSize& s);

    HRESULT TryNotCopy( IMediaSample* pIn, const CMediaType& mt, const BITMAPINFOHEADER& bihIn );

    ColorConvTable::YuvMatrixType m_video_yuv_matrix_decided_by_sub;
    ColorConvTable::YuvRangeType m_video_yuv_range_decided_by_sub;

    void SetYuvMatrix();
protected:
    void GetOutputSize(int& w, int& h, int& arx, int& ary);
    HRESULT Transform(IMediaSample* pIn);

    HRESULT AlphaBlt(SubPicDesc& spd);
public:
    XySubFilterConsumer(LPUNKNOWN punk, HRESULT* phr, const GUID& clsid = __uuidof(XySubFilterConsumer));
    virtual ~XySubFilterConsumer();

    DECLARE_IUNKNOWN;
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);

    DECLARE_ISUBRENDEROPTIONS;

    // CBaseFilter
    STDMETHODIMP JoinFilterGraph(IFilterGraph* pGraph, LPCWSTR pName);
    STDMETHODIMP QueryFilterInfo(FILTER_INFO* pInfo);

    // CTransformFilter
    HRESULT SetMediaType(PIN_DIRECTION dir, const CMediaType* pMediaType),
            CheckConnect(PIN_DIRECTION dir, IPin* pPin),
            CompleteConnect(PIN_DIRECTION dir, IPin* pReceivePin),
            BreakConnect(PIN_DIRECTION dir),
            StartStreaming(), 
            StopStreaming(),
            NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate);

    //CBaseVideoFilter
    void GetInputColorspaces(ColorSpaceId *preferredOrder, UINT *count);
    void GetOutputColorspaces(ColorSpaceId *preferredOrder, UINT *count);

    //ISubRenderConsumer
    STDMETHODIMP GetMerit(ULONG *merit);
    STDMETHODIMP Connect(ISubRenderProvider *subtitleRenderer);
    STDMETHODIMP Disconnect(void);
    STDMETHODIMP DeliverFrame(REFERENCE_TIME start, REFERENCE_TIME stop, LPVOID context, ISubRenderFrame *subtitleFrame);

    // IXyOptions
    STDMETHODIMP XySetBool     (unsigned field, bool      value);
    STDMETHODIMP XySetInt      (unsigned field, int       value);

    // ISpecifyPropertyPages
    STDMETHODIMP GetPages(CAUUID* pPages);

    // CPersistStream
    STDMETHODIMP GetClassID(CLSID* pClsid);

protected:
    //OSD
    void ZeroObj4OSD();
    void DeleteObj4OSD();
    void InitObj4OSD();
    void PrintMessages(BYTE* pOut);

    HDC m_hdc;
    HBITMAP m_hbm;
    HFONT m_hfont;

protected:
    HRESULT ChangeMediaType(int iPosition);

    /* ResX2 */
    CAutoVectorPtr<BYTE> m_pTempPicBuff;
    HRESULT Copy(BYTE* pSub, BYTE* pIn, CSize sub, CSize in, int bpp, const GUID& subtype, DWORD black);
    // segment start time, absolute time
    CRefTime m_tPrev;
    REFERENCE_TIME CalcCurrentTime();
    // 3.x- versions of microsoft's mpeg4 codec output flipped image
    bool m_fMSMpeg4Fix;

    CCritSec m_csSubLock;

private:
    VIDEOINFOHEADER2 m_CurrentVIH2;
};
