/*
 * (C) 2006-2013 see Authors.txt 
 *
 * This file is part of MPC-HC.
 *
 * MPC-HC is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * MPC-HC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "stdafx.h"
#include "HdmvSub.h"
#include "../DSUtil/GolombBuffer.h"

#if ENABLE_XY_LOG_HDMVSUB
#define TRACE_HDMVSUB(_x_)		{CString tmp;tmp.Format _x_; XY_LOG_TRACE( tmp.GetString() );}
#else
#define TRACE_HDMVSUB __noop
#endif


CHdmvSub::CHdmvSub(void)
    : CBaseSub(ST_HDMV)
    , m_nCurSegment(NO_SEGMENT)
    , m_pSegBuffer(NULL)
    , m_nTotalSegBuffer(0)
    , m_nSegBufferPos(0)
    , m_nSegSize(0)
    , m_pCurrentPresentationSegment(NULL)
{
}

CHdmvSub::~CHdmvSub()
{
    Reset();

    delete [] m_pSegBuffer;
    delete m_pCurrentPresentationSegment;
}


void CHdmvSub::AllocSegment(int nSize)
{
    if (nSize > m_nTotalSegBuffer) {
        delete [] m_pSegBuffer;
        m_pSegBuffer = DEBUG_NEW BYTE[nSize];
        m_nTotalSegBuffer = nSize;
    }
    m_nSegBufferPos = 0;
    m_nSegSize = nSize;
}

POSITION CHdmvSub::GetStartPosition(REFERENCE_TIME rt, double fps /* = 0*/)
{
    HDMV_PRESENTATION_SEGMENT* pPresentationSegment;

    // Cleanup old PG
    while (m_pPresentationSegments.GetCount() > 0) {
        pPresentationSegment = m_pPresentationSegments.GetHead();
        if (pPresentationSegment->rtStop < rt) {
            TRACE_HDMVSUB( (_T("CHdmvSub:HDMV Remove Presentation segment %d  %lS => %lS (rt=%lS)\n"), pPresentationSegment->composition_descriptor.nNumber,
                          ReftimeToString(pPresentationSegment->rtStart), ReftimeToString(pPresentationSegment->rtStop), ReftimeToString(rt)) );
            m_pPresentationSegments.RemoveHead();
            delete pPresentationSegment;
        } else {
            break;
        }
    }
 // log first 2 objects
 //   {
 //       POSITION pos = m_pPresentationSegments.GetHeadPosition();
 //       for (int i=0;i<2 && pos!=NULL; i++)
 //       {
 //           CompositionObject*	pObject = m_pPresentationSegments.GetNext(pos);
 //           TRACE_HDMVSUB( (_T("CHdmvSub:HDMV cur %d object %d  %lS => %lS\n"), i, pObject->GetRLEDataSize(),
 //               ReftimeToCString (pObject->m_rtStart), ReftimeToCString(pObject->m_rtStop)));
 //       }
 //   }

    POSITION	pos = m_pPresentationSegments.GetHeadPosition();

    while (pos) {
        HDMV_PRESENTATION_SEGMENT* pPresentationSegment = m_pPresentationSegments.GetAt (pos);

        if (rt >= pPresentationSegment->rtStart && rt < pPresentationSegment->rtStop) {
            break;
        }
        else if( rt < pPresentationSegment->rtStart )
        {
            pos = NULL;
            break;
        }

        m_pPresentationSegments.GetNext(pos);
    }
    return pos;
}

HRESULT CHdmvSub::ParseSample(IMediaSample* pSample)
{
    CheckPointer(pSample, E_POINTER);
    HRESULT hr;
    REFERENCE_TIME rtStart = INVALID_TIME, rtStop = INVALID_TIME;
    BYTE* pData = NULL;
    int lSampleLen;

    hr = pSample->GetPointer(&pData);
    if (FAILED(hr) || pData == NULL) {
        return hr;
    }
    lSampleLen = pSample->GetActualDataLength();

    pSample->GetTime(&rtStart, &rtStop);

    return ParseSample(pData, lSampleLen, rtStart, rtStop);
}

HRESULT CHdmvSub::ParseSample(BYTE* pData, int lSampleLen, REFERENCE_TIME rtStart, REFERENCE_TIME rtStop)
{
    HRESULT hr = S_OK;

    if (pData) {
        CGolombBuffer SampleBuffer(pData, lSampleLen);

        while (!SampleBuffer.IsEOF()) {
            if (m_nCurSegment == NO_SEGMENT) {
                HDMV_SEGMENT_TYPE nSegType = (HDMV_SEGMENT_TYPE)SampleBuffer.ReadByte();
                unsigned short nUnitSize = SampleBuffer.ReadShort();
                lSampleLen -= 3;

                switch (nSegType) {
                case PALETTE:
                case OBJECT:
                case PRESENTATION_SEG:
                case END_OF_DISPLAY:
                    m_nCurSegment = nSegType;
                    AllocSegment(nUnitSize);
                    break;

                case WINDOW_DEF:
                case INTERACTIVE_SEG:
                case HDMV_SUB1:
                case HDMV_SUB2:
                    // Ignored stuff...
                    SampleBuffer.SkipBytes(nUnitSize);
                    break;
                default:
                    return VFW_E_SAMPLE_REJECTED;
                }
            }

            if (m_nCurSegment != NO_SEGMENT) {
                if (m_nSegBufferPos < m_nSegSize) {
                    int nSize = min(m_nSegSize - m_nSegBufferPos, lSampleLen);
                    SampleBuffer.ReadBuffer(m_pSegBuffer + m_nSegBufferPos, nSize);
                    m_nSegBufferPos += nSize;
                }

                if (m_nSegBufferPos >= m_nSegSize) {
                    CGolombBuffer SegmentBuffer(m_pSegBuffer, m_nSegSize);

                    switch (m_nCurSegment) {
                    case PALETTE:
                        TRACE_HDMVSUB( (_T("CHdmvSub:PALETTE            rtStart=%10I64d\n"), rtStart) );
                        ParsePalette(&SegmentBuffer, m_nSegSize);
                        break;
                    case OBJECT:
                        TRACE_HDMVSUB( (_T("CHdmvSub:OBJECT             %lS\n"), ReftimeToCString(rtStart)) );
                        ParseObject(&SegmentBuffer, m_nSegSize);
                        break;
                    case PRESENTATION_SEG:
                        TRACE_HDMVSUB( (_T("CHdmvSub:PRESENTATION_SEG   %lS (size=%d)\n"), ReftimeToCString(rtStart), m_nSegSize) );

                        // Enqueue the current presentation segment if any
                        EnqueuePresentationSegment(rtStart);
                        // Parse the new presentation segment
                        ParsePresentationSegment(rtStart, &SegmentBuffer);

                        break;
                    case WINDOW_DEF:
                        //TRACE_HDMVSUB( (_T("CHdmvSub:WINDOW_DEF         %lS\n"), ReftimeToCString(rtStart)) );
                        break;
                    case END_OF_DISPLAY:
                        //TRACE_HDMVSUB( (_T("CHdmvSub:END_OF_DISPLAY     %lS\n"), ReftimeToCString(rtStart)) );
                        break;
                    default:
                        TRACE_HDMVSUB( (_T("CHdmvSub:UNKNOWN Seg %d     rtStart=0x%10dd\n"), m_nCurSegment, rtStart) );
                    }

                    m_nCurSegment = NO_SEGMENT;
                }
            }
        }
    }

    return hr;
}

int CHdmvSub::ParsePresentationSegment(REFERENCE_TIME rt, CGolombBuffer* pGBuffer)
{
    if (pGBuffer->RemainingSize() < 11) {
        return 0;
    }

    m_pCurrentPresentationSegment = DEBUG_NEW HDMV_PRESENTATION_SEGMENT();

    m_pCurrentPresentationSegment->rtStart = rt;
    m_pCurrentPresentationSegment->rtStop  = _I64_MAX;

    ParseVideoDescriptor(pGBuffer, &m_pCurrentPresentationSegment->video_descriptor);
    ParseCompositionDescriptor(pGBuffer, &m_pCurrentPresentationSegment->composition_descriptor);
    m_pCurrentPresentationSegment->palette_update_flag = !!(pGBuffer->ReadByte() & 0x80);
    m_pCurrentPresentationSegment->CLUT.id = pGBuffer->ReadByte();
    m_pCurrentPresentationSegment->objectCount = pGBuffer->ReadByte();

    TRACE_HDMVSUB( (_T("CHdmvSub::ParsePresentationSegment Size = %d, state = %#x, nObjectNumber = %d\n"), pGBuffer->GetSize(),
                  m_pCurrentPresentationSegment->composition_descriptor.bState, m_pCurrentPresentationSegment->objectCount) );

    if (pGBuffer->RemainingSize() < (m_pCurrentPresentationSegment->objectCount * 8)) {
        return 0;
    }

    for (int i = 0; i < m_pCurrentPresentationSegment->objectCount; i++) {
        CompositionObject* pCompositionObject = DEBUG_NEW CompositionObject();
        ParseCompositionObject(pGBuffer, pCompositionObject);
        m_pCurrentPresentationSegment->objects.AddTail(pCompositionObject);
    }

    return m_pCurrentPresentationSegment->objectCount;
}

void CHdmvSub::EnqueuePresentationSegment(REFERENCE_TIME rt)
{
    if (m_pCurrentPresentationSegment) {
        if (m_pCurrentPresentationSegment->objectCount > 0) {
            m_pCurrentPresentationSegment->rtStop = rt;
            m_pCurrentPresentationSegment->CLUT = m_CLUTs[m_pCurrentPresentationSegment->CLUT.id];

            // Get the objects' data
            POSITION pos = m_pCurrentPresentationSegment->objects.GetHeadPosition();
            while (pos) {
                CompositionObject* pObject = m_pCurrentPresentationSegment->objects.GetNext(pos);

                CompositionObject& pObjectData = m_compositionObjects[pObject->m_object_id_ref];

                pObject->m_width = pObjectData.m_width;
                pObject->m_height = pObjectData.m_height;

                pObject->SetRLEData(pObjectData.GetRLEData(), pObjectData.GetRLEDataSize(), pObjectData.GetRLEDataSize());
            }

            m_pPresentationSegments.AddTail(m_pCurrentPresentationSegment);
            TRACE_HDMVSUB( (_T("CHdmvSub: Enqueue Presentation Segment %d - %lS => %lS\n"), m_pCurrentPresentationSegment->composition_descriptor.nNumber,
                          ReftimeToCString(m_pCurrentPresentationSegment->rtStart), ReftimeToCString(m_pCurrentPresentationSegment->rtStop)) );
        } else {
            TRACE_HDMVSUB( (_T("CHdmvSub: Delete empty Presentation Segment %d\n"), m_pCurrentPresentationSegment->composition_descriptor.nNumber) );
            delete m_pCurrentPresentationSegment;
        }

        m_pCurrentPresentationSegment = NULL;
    }
}

void CHdmvSub::ParsePalette(CGolombBuffer* pGBuffer, unsigned short nSize)  // #497
{
    BYTE palette_id = pGBuffer->ReadByte();
    HDMV_CLUT& CLUT = m_CLUTs[palette_id];

    CLUT.id = palette_id;
    CLUT.version_number = pGBuffer->ReadByte();

    ASSERT((nSize - 2) % sizeof(HDMV_PALETTE) == 0);
    CLUT.size = (nSize - 2) / sizeof(HDMV_PALETTE);

    for (int i = 0; i < CLUT.size; i++) {
        CLUT.palette[i].entry_id = pGBuffer->ReadByte();

        CLUT.palette[i].Y  = pGBuffer->ReadByte();
        CLUT.palette[i].Cr = pGBuffer->ReadByte();
        CLUT.palette[i].Cb = pGBuffer->ReadByte();
        CLUT.palette[i].T  = pGBuffer->ReadByte();
    }
}

void CHdmvSub::ParseObject(CGolombBuffer* pGBuffer, unsigned short nUnitSize)   // #498
{
    short object_id = pGBuffer->ReadShort();

    if (object_id >= _countof(m_compositionObjects)) {
        TRACE_HDMVSUB((_T("CHdmvSub::ParseObject() : FAILED, object_id - %d"), object_id));
        return;
    }

    CompositionObject& pObject = m_compositionObjects[object_id];

    pObject.m_version_number = pGBuffer->ReadByte();
    BYTE m_sequence_desc = pGBuffer->ReadByte();

    if (m_sequence_desc & 0x80) {
        DWORD object_data_length  = (DWORD)pGBuffer->BitRead(24);

        pObject.m_width = pGBuffer->ReadShort();
        pObject.m_height = pGBuffer->ReadShort();

        pObject.SetRLEData(pGBuffer->GetBufferPos(), nUnitSize - 11, object_data_length - 4);

        TRACE_HDMVSUB( (_T("CHdmvSub:ParseObject %d (size=%ld, %dx%d)\n"), object_id, object_data_length, pObject.m_width, pObject.m_height) );
    } else {
        pObject.AppendRLEData(pGBuffer->GetBufferPos(), nUnitSize - 4);
    }
}

void CHdmvSub::ParseCompositionObject(CGolombBuffer* pGBuffer, CompositionObject* pCompositionObject)
{
    BYTE bTemp;
    pCompositionObject->m_object_id_ref = pGBuffer->ReadShort();
    pCompositionObject->m_window_id_ref = pGBuffer->ReadByte();
    bTemp = pGBuffer->ReadByte();
    pCompositionObject->m_object_cropped_flag = !!(bTemp & 0x80);
    pCompositionObject->m_forced_on_flag = !!(bTemp & 0x40);
    pCompositionObject->m_horizontal_position = pGBuffer->ReadShort();
    pCompositionObject->m_vertical_position = pGBuffer->ReadShort();

    if (pCompositionObject->m_object_cropped_flag) {
        pCompositionObject->m_cropping_horizontal_position = pGBuffer->ReadShort();
        pCompositionObject->m_cropping_vertical_position = pGBuffer->ReadShort();
        pCompositionObject->m_cropping_width = pGBuffer->ReadShort();
        pCompositionObject->m_cropping_height = pGBuffer->ReadShort();
    }
}

void CHdmvSub::ParseVideoDescriptor(CGolombBuffer* pGBuffer, VIDEO_DESCRIPTOR* pVideoDescriptor)
{
    pVideoDescriptor->nVideoWidth = pGBuffer->ReadShort();
    pVideoDescriptor->nVideoHeight = pGBuffer->ReadShort();
    pVideoDescriptor->bFrameRate = pGBuffer->ReadByte();
}

void CHdmvSub::ParseCompositionDescriptor(CGolombBuffer* pGBuffer, COMPOSITION_DESCRIPTOR* pCompositionDescriptor)
{
    pCompositionDescriptor->nNumber = pGBuffer->ReadShort();
    pCompositionDescriptor->bState  = pGBuffer->ReadByte() >> 6;
}

void CHdmvSub::Render(SubPicDesc& spd, REFERENCE_TIME rt, RECT& bbox)
{
    HDMV_PRESENTATION_SEGMENT* pPresentationSegment = FindPresentationSegment(rt);

    bbox.left   = LONG_MAX;
    bbox.top    = LONG_MAX;
    bbox.right  = 0;
    bbox.bottom = 0;

    if (pPresentationSegment) {
        POSITION pos = pPresentationSegment->objects.GetHeadPosition();

        TRACE_HDMVSUB( (_T("CHdmvSub:Render Presentation segment %d --> %lS - %lS\n"), pPresentationSegment->composition_descriptor.nNumber,
                      ReftimeToCString(pPresentationSegment->rtStart), ReftimeToCString(pPresentationSegment->rtStop)) );

        while (pos) {
            CompositionObject* pObject = pPresentationSegment->objects.GetNext(pos);

            if (pObject->GetRLEDataSize() && pObject->m_width > 0 && pObject->m_height > 0
                && spd.w >= (pObject->m_horizontal_position + pObject->m_width) 
                && spd.h >= (pObject->m_vertical_position + pObject->m_height)) 
            {
                CompositionObject::ColorType color_type = m_colorTypeSetting;
                if (color_type==CompositionObject::NONE)
                {
                    color_type = pPresentationSegment->video_descriptor.nVideoWidth >= 720 ? 
                        CompositionObject::YUV_Rec709 : CompositionObject::YUV_Rec601;
                }
                pObject->SetPalette(pPresentationSegment->CLUT.size, pPresentationSegment->CLUT.palette, color_type, 
                    m_yuvRangeSetting==CompositionObject::RANGE_NONE ? CompositionObject::RANGE_TV : m_yuvRangeSetting);

                bbox.left   = min(pObject->m_horizontal_position, bbox.left);
                bbox.top    = min(pObject->m_vertical_position, bbox.top);
                bbox.right  = max(pObject->m_horizontal_position + pObject->m_width, bbox.right);
                bbox.bottom = max(pObject->m_vertical_position + pObject->m_height, bbox.bottom);

                ASSERT(spd.h>=0);
                bbox.left = bbox.left > 0 ? bbox.left : 0;
                bbox.top = bbox.top > 0 ? bbox.top : 0;
                bbox.right = bbox.right < spd.w ? bbox.right : spd.w;
                bbox.bottom = bbox.bottom < spd.h ? bbox.bottom : spd.h;

                pObject->InitColor(spd);
                TRACE_HDMVSUB( (_T(" --> Object %d (Pos=%dx%d, Res=%dx%d, SPDRes=%dx%d)\n"),
                              pObject->m_object_id_ref, pObject->m_horizontal_position, pObject->m_vertical_position, pObject->m_width, pObject->m_height, spd.w, spd.h) );
                pObject->RenderHdmv(spd);
            } else {
                TRACE_HDMVSUB( (_T(" --> Invalid object %d\n"), pObject->m_object_id_ref) );
            }
        }
    }
}

HRESULT CHdmvSub::GetTextureSize(POSITION pos, SIZE& MaxTextureSize, SIZE& VideoSize, POINT& VideoTopLeft)
{
    HDMV_PRESENTATION_SEGMENT* pPresentationSegment = m_pPresentationSegments.GetAt(pos);
    if (pPresentationSegment) {
        MaxTextureSize.cx = VideoSize.cx = pPresentationSegment->video_descriptor.nVideoWidth;
        MaxTextureSize.cy = VideoSize.cy = pPresentationSegment->video_descriptor.nVideoHeight;

        // The subs will be directly rendered into the proper position!
        VideoTopLeft.x = 0; //pObject->m_horizontal_position;
        VideoTopLeft.y = 0; //pObject->m_vertical_position;

        return S_OK;
    }

    ASSERT(FALSE);
    return E_INVALIDARG;
}

void CHdmvSub::Reset()
{
    HDMV_PRESENTATION_SEGMENT* pPresentationSegment;
    while (m_pPresentationSegments.GetCount() > 0) {
        pPresentationSegment = m_pPresentationSegments.RemoveHead();
        delete pPresentationSegment;
    }
}

HRESULT CHdmvSub::SetYuvType( ColorType colorType, YuvRangeType yuvRangeType )
{
    m_colorTypeSetting = colorType;
    m_yuvRangeSetting = yuvRangeType;
    return S_OK;
}

CHdmvSub::HDMV_PRESENTATION_SEGMENT* CHdmvSub::FindPresentationSegment(REFERENCE_TIME rt)
{
    POSITION pos = m_pPresentationSegments.GetHeadPosition();

    while (pos) {
        HDMV_PRESENTATION_SEGMENT* pPresentationSegment = m_pPresentationSegments.GetNext(pos);

        if (pPresentationSegment->rtStart <= rt && pPresentationSegment->rtStop > rt) {
            return pPresentationSegment;
        }
    }

    return NULL;
}

CompositionObject* CHdmvSub::FindObject(HDMV_PRESENTATION_SEGMENT* pPresentationSegment, short sObjectId)
{
    POSITION pos = pPresentationSegment->objects.GetHeadPosition();

    while (pos) {
        CompositionObject* pObject = pPresentationSegment->objects.GetNext(pos);

        if (pObject->m_object_id_ref == sObjectId) {
            return pObject;
        }
    }

    return NULL;
}
