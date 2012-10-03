/*
 * (C) 2006-2012 see Authors.txt
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
#include "CompositionObject.h"
#include "../DSUtil/GolombBuffer.h"
#include "../subpic/color_conv_table.h"


CompositionObject::CompositionObject()
{
    m_pRLEData      = NULL;
    m_nRLEDataSize  = 0;
    m_nRLEPos       = 0;
	
    memsetd(m_Colors, 0xFF000000, sizeof(m_Colors));
}

CompositionObject::~CompositionObject()
{
    delete [] m_pRLEData;
}

void CompositionObject::SetPalette(int nNbEntry, HDMV_PALETTE* pPalette, bool bIsHD)
{
    m_OriginalColorType = bIsHD ? YUV_Rec709 : YUV_Rec601;

    m_colorType = -1;

    m_Palette.SetCount(nNbEntry>0?nNbEntry:0);
    if(nNbEntry>0)
    {        
        memcpy(m_Palette.GetData(), pPalette, nNbEntry*sizeof(pPalette[0]));
    }
}

void CompositionObject::InitColor(const SubPicDesc& spd)
{
#define COMBINE_AYUV(a, y, u, v) ((((((((int)(a))<<8)|y)<<8)|u)<<8)|v)
    //fix me: move all color conv function into color_conv_table or dsutil

    int paletteNumber = m_Palette.GetCount();
    if (m_colorType!=spd.type)
    {
        m_colorType = -1;
        if(m_OriginalColorType!=NONE)
        {
            m_colorType = spd.type;
            switch(spd.type)
            {
            case MSP_AYUV_PLANAR:
            case MSP_AYUV:
                if ((m_OriginalColorType==YUV_Rec709 && ColorConvTable::GetDefaultYUVType()==ColorConvTable::BT709) ||
                    (m_OriginalColorType==YUV_Rec601 && ColorConvTable::GetDefaultYUVType()==ColorConvTable::BT601))
                {
                    for (int i=0;i<paletteNumber;i++)
                    {
                        m_Colors[m_Palette[i].entry_id] = COMBINE_AYUV(m_Palette[i].T, m_Palette[i].Y, m_Palette[i].Cr, m_Palette[i].Cb);
                    }
                }
                else if (m_OriginalColorType==YUV_Rec709)
                {
                    for (int i=0;i<paletteNumber;i++)
                    {
                        DWORD argb = ColorConvTable::A8Y8U8V8_To_ARGB_TV_BT709(m_Palette[i].T, m_Palette[i].Y, m_Palette[i].Cr, m_Palette[i].Cb);
                        m_Colors[m_Palette[i].entry_id] = ColorConvTable::Argb2Ayuv(argb);
                    }
                }
                else if (m_OriginalColorType==YUV_Rec601)
                {
                    for (int i=0;i<paletteNumber;i++)
                    {
                        DWORD argb = ColorConvTable::A8Y8U8V8_To_ARGB_TV_BT601(m_Palette[i].T, m_Palette[i].Y, m_Palette[i].Cr, m_Palette[i].Cb);
                        m_Colors[m_Palette[i].entry_id] = ColorConvTable::Argb2Ayuv(argb);
                    }
                }
                else
                {
                    m_colorType = -1;
                }
                break;
            case MSP_XY_AUYV:
                if ((m_OriginalColorType==YUV_Rec709 && ColorConvTable::GetDefaultYUVType()==ColorConvTable::BT709) ||
                    (m_OriginalColorType==YUV_Rec601 && ColorConvTable::GetDefaultYUVType()==ColorConvTable::BT601))
                {
                    for (int i=0;i<paletteNumber;i++)
                    {
                        m_Colors[m_Palette[i].entry_id] = COMBINE_AYUV(m_Palette[i].T, m_Palette[i].Cr, m_Palette[i].Y, m_Palette[i].Cb);
                    }
                }
                else if (m_OriginalColorType==YUV_Rec709)
                {
                    for (int i=0;i<paletteNumber;i++)
                    {
                        DWORD argb = YCrCbToRGB_Rec709(m_Palette[i].T, m_Palette[i].Y, m_Palette[i].Cr, m_Palette[i].Cb);
                        m_Colors[m_Palette[i].entry_id] = ColorConvTable::Argb2Auyv(argb);
                    }                    
                }
                else if (m_OriginalColorType==YUV_Rec601)
                {
                    for (int i=0;i<paletteNumber;i++)
                    {
                        DWORD argb = YCrCbToRGB_Rec601(m_Palette[i].T, m_Palette[i].Y, m_Palette[i].Cr, m_Palette[i].Cb);
                        m_Colors[m_Palette[i].entry_id] = ColorConvTable::Argb2Auyv(argb);
                    }
                }
                else
                {
                    m_colorType = -1;
                }
                break;
            case MSP_RGBA:
                if (m_OriginalColorType==YUV_Rec709)
                {
                    for (int i=0;i<paletteNumber;i++)
                    {
                        DWORD argb = YCrCbToRGB_Rec709(m_Palette[i].T, m_Palette[i].Y, m_Palette[i].Cr, m_Palette[i].Cb);
                        m_Colors[m_Palette[i].entry_id] = argb;
                    }                    
                }
                else if (m_OriginalColorType==YUV_Rec601)
                {
                    for (int i=0;i<paletteNumber;i++)
                    {
                        DWORD argb = YCrCbToRGB_Rec601(m_Palette[i].T, m_Palette[i].Y, m_Palette[i].Cr, m_Palette[i].Cb);
                        m_Colors[m_Palette[i].entry_id] = argb;
                    }
                }
                else
                {
                    m_colorType = -1;
                }
                break;
            default:
                m_colorType = -1;
                break;
            }
        }
        if (m_colorType == -1)
        {
            //todo fixme: log error
        }
    }
}

void CompositionObject::SetRLEData(const BYTE* pBuffer, int nSize, int nTotalSize)
{
    delete [] m_pRLEData;
    m_pRLEData     = DNew BYTE[nTotalSize];
    m_nRLEDataSize = nTotalSize;
    m_nRLEPos      = nSize;

    memcpy(m_pRLEData, pBuffer, nSize);
}

void CompositionObject::AppendRLEData(const BYTE* pBuffer, int nSize)
{
    ASSERT(m_nRLEPos + nSize <= m_nRLEDataSize);
    if (m_nRLEPos + nSize <= m_nRLEDataSize) {
        memcpy(m_pRLEData + m_nRLEPos, pBuffer, nSize);
        m_nRLEPos += nSize;
    }
}


void CompositionObject::RenderHdmv(SubPicDesc& spd)
{
    if (!m_pRLEData) {
        return;
    }

    CGolombBuffer GBuffer(m_pRLEData, m_nRLEDataSize);
    BYTE  bTemp;
    BYTE  bSwitch;
    BYTE  nPaletteIndex = 0;
    short nCount;
    short nX = m_horizontal_position;
    short nY = m_vertical_position;

    while ((nY < (m_vertical_position + m_height)) && !GBuffer.IsEOF()) {
        bTemp = GBuffer.ReadByte();
        if (bTemp != 0) {
            nPaletteIndex = bTemp;
            nCount = 1;
        } else {
            bSwitch = GBuffer.ReadByte();
            if (!(bSwitch & 0x80)) {
                if (!(bSwitch & 0x40)) {
                    nCount = bSwitch & 0x3F;
                    if (nCount > 0) {
                        nPaletteIndex = 0;
                    }
                } else {
                    nCount = (bSwitch & 0x3F) << 8 | (short)GBuffer.ReadByte();
                    nPaletteIndex = 0;
                }
            } else {
                if (!(bSwitch & 0x40)) {
                    nCount = bSwitch & 0x3F;
                    nPaletteIndex = GBuffer.ReadByte();
                } else {
                    nCount = (bSwitch & 0x3F) << 8 | (short)GBuffer.ReadByte();
                    nPaletteIndex = GBuffer.ReadByte();
                }
            }
        }

        if (nCount > 0) {
            if (nPaletteIndex != 0xFF) {    // Fully transparent (§9.14.4.2.2.1.1)
                FillSolidRect(spd, nX, nY, nCount, 1, m_Colors[nPaletteIndex]);
            }
            nX += nCount;
        } else {
            nY++;
            nX = m_horizontal_position;
        }
    }
}


void CompositionObject::RenderDvb(SubPicDesc& spd, short nX, short nY)
{
    if (!m_pRLEData) {
        return;
    }

    CGolombBuffer gb(m_pRLEData, m_nRLEDataSize);
    short sTopFieldLength;
    short sBottomFieldLength;

    sTopFieldLength    = gb.ReadShort();
    sBottomFieldLength = gb.ReadShort();

    DvbRenderField(spd, gb, nX, nY,     sTopFieldLength);
    DvbRenderField(spd, gb, nX, nY + 1, sBottomFieldLength);
}


void CompositionObject::DvbRenderField(SubPicDesc& spd, CGolombBuffer& gb, short nXStart, short nYStart, short nLength)
{
    //FillSolidRect(spd, nXStart, nYStart, m_width, m_height, 0xFFFF0000);  // Red opaque
    //FillSolidRect(spd, nXStart, nYStart, m_width, m_height, 0xCC00FF00);  // Green 80%
    //FillSolidRect(spd, nXStart, nYStart, m_width, m_height, 0x100000FF);  // Blue 60%
    //return;
    short nX = nXStart;
    short nY = nYStart;
    int nEnd = gb.GetPos() + nLength;
    while (gb.GetPos() < nEnd) {
        BYTE bType = gb.ReadByte();
        switch (bType) {
            case 0x10:
                Dvb2PixelsCodeString(spd, gb, nX, nY);
                break;
            case 0x11:
                Dvb4PixelsCodeString(spd, gb, nX, nY);
                break;
            case 0x12:
                Dvb8PixelsCodeString(spd, gb, nX, nY);
                break;
            case 0x20:
                gb.SkipBytes(2);
                break;
            case 0x21:
                gb.SkipBytes(4);
                break;
            case 0x22:
                gb.SkipBytes(16);
                break;
            case 0xF0:
                nX  = nXStart;
                nY += 2;
                break;
            default:
                ASSERT(FALSE);
                break;
        }
    }
}


void CompositionObject::Dvb2PixelsCodeString(SubPicDesc& spd, CGolombBuffer& gb, short& nX, short& nY)
{
    BYTE  bTemp;
    BYTE  nPaletteIndex = 0;
    short nCount;
    bool  bQuit = false;

    while (!bQuit && !gb.IsEOF()) {
        nCount = 0;
        nPaletteIndex = 0;
        bTemp = (BYTE)gb.BitRead(2);
        if (bTemp != 0) {
            nPaletteIndex = bTemp;
            nCount = 1;
        } else {
            if (gb.BitRead(1) == 1) {                               // switch_1
                nCount = 3 + (short)gb.BitRead(3);                  // run_length_3-9
                nPaletteIndex = (BYTE)gb.BitRead(2);
            } else {
                if (gb.BitRead(1) == 0) {                           // switch_2
                    switch (gb.BitRead(2)) {                        // switch_3
                        case 0:
                            bQuit = true;
                            break;
                        case 1:
                            nCount = 2;
                            break;
                        case 2:                                     // if (switch_3 == '10')
                            nCount = 12 + (short)gb.BitRead(4);     // run_length_12-27
                            nPaletteIndex = (BYTE)gb.BitRead(2);    // 4-bit_pixel-code
                            break;
                        case 3:
                            nCount = 29 + gb.ReadByte();            // run_length_29-284
                            nPaletteIndex = (BYTE)gb.BitRead(2);    // 4-bit_pixel-code
                            break;
                    }
                } else {
                    nCount = 1;
                }
            }
        }

        if (nX + nCount > m_width) {
            ASSERT(FALSE);
            break;
        }

        if (nCount > 0) {
            FillSolidRect(spd, nX, nY, nCount, 1, m_Colors[nPaletteIndex]);
            nX += nCount;
        }
    }

    gb.BitByteAlign();
}

void CompositionObject::Dvb4PixelsCodeString(SubPicDesc& spd, CGolombBuffer& gb, short& nX, short& nY)
{
    BYTE  bTemp;
    BYTE  nPaletteIndex = 0;
    short nCount;
    bool  bQuit = false;

    while (!bQuit && !gb.IsEOF()) {
        nCount = 0;
        nPaletteIndex = 0;
        bTemp = (BYTE)gb.BitRead(4);
        if (bTemp != 0) {
            nPaletteIndex = bTemp;
            nCount = 1;
        } else {
            if (gb.BitRead(1) == 0) {                           // switch_1
                nCount = (short)gb.BitRead(3);                  // run_length_3-9
                if (nCount != 0) {
                    nCount += 2;
                } else {
                    bQuit = true;
                }
            } else {
                if (gb.BitRead(1) == 0) {                           // switch_2
                    nCount = 4 + (short)gb.BitRead(2);              // run_length_4-7
                    nPaletteIndex = (BYTE)gb.BitRead(4);            // 4-bit_pixel-code
                } else {
                    switch (gb.BitRead(2)) {                        // switch_3
                        case 0:
                            nCount = 1;
                            break;
                        case 1:
                            nCount = 2;
                            break;
                        case 2:                                     // if (switch_3 == '10')
                            nCount = 9 + (short)gb.BitRead(4);      // run_length_9-24
                            nPaletteIndex = (BYTE)gb.BitRead(4);    // 4-bit_pixel-code
                            break;
                        case 3:
                            nCount = 25 + gb.ReadByte();            // run_length_25-280
                            nPaletteIndex = (BYTE)gb.BitRead(4);    // 4-bit_pixel-code
                            break;
                    }
                }
            }
        }

#if 0
        if (nX + nCount > m_width) {
            ASSERT(FALSE);
            break;
        }
#endif

        if (nCount > 0) {
            FillSolidRect(spd, nX, nY, nCount, 1, m_Colors[nPaletteIndex]);
            nX += nCount;
        }
    }

    gb.BitByteAlign();
}

void CompositionObject::Dvb8PixelsCodeString(SubPicDesc& spd, CGolombBuffer& gb, short& nX, short& nY)
{
    BYTE  bTemp;
    BYTE  nPaletteIndex = 0;
    short nCount;
    bool  bQuit = false;

    while (!bQuit && !gb.IsEOF()) {
        nCount = 0;
        nPaletteIndex = 0;
        bTemp = gb.ReadByte();
        if (bTemp != 0) {
            nPaletteIndex = bTemp;
            nCount = 1;
        } else {
            if (gb.BitRead(1) == 0) {                   // switch_1
                nCount = (short)gb.BitRead(7);          // run_length_1-127
                if (nCount == 0) {
                    bQuit = true;
                }
            } else {
                nCount = (short)gb.BitRead(7);          // run_length_3-127
                nPaletteIndex = gb.ReadByte();
            }
        }

        if (nX + nCount > m_width) {
            ASSERT(FALSE);
            break;
        }

        if (nCount > 0) {
            FillSolidRect(spd, nX, nY, nCount, 1, m_Colors[nPaletteIndex]);
            nX += nCount;
        }
    }

    gb.BitByteAlign();
}
