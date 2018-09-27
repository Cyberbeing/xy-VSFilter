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

#pragma once

#include "Rasterizer.h"


struct HDMV_PALETTE {
    BYTE    entry_id;
    BYTE    Y;
    BYTE    Cr;
    BYTE    Cb;
    BYTE    T;      // HDMV rule : 0 transparent, 255 opaque (compatible DirectX)
};

class CGolombBuffer;

class CompositionObject : Rasterizer
{
public:
    enum ColorType 
    {
        NONE,
        YUV_Rec601,
        YUV_Rec709,
        YUV_Rec2020,
        RGB
    };
    enum YuvRangeType 
    {
        RANGE_NONE
        , RANGE_TV
        , RANGE_PC
    };
public:
    short m_object_id_ref;
    BYTE  m_window_id_ref;
    bool  m_object_cropped_flag;
    bool  m_forced_on_flag;
    BYTE  m_version_number;

    short m_horizontal_position;
    short m_vertical_position;
    short m_width;
    short m_height;

    short m_cropping_horizontal_position;
    short m_cropping_vertical_position;
    short m_cropping_width;
    short m_cropping_height;

    CompositionObject();
    ~CompositionObject();

    void  SetRLEData(const BYTE* pBuffer, int nSize, int nTotalSize);
    void  AppendRLEData(const BYTE* pBuffer, int nSize);
    const BYTE* GetRLEData() { return m_pRLEData; };
    int   GetRLEDataSize() { return m_nRLEDataSize; };
    bool  IsRLEComplete() { return m_nRLEPos >= m_nRLEDataSize; };
    void  RenderHdmv(SubPicDesc& spd);
    void  RenderDvb(SubPicDesc& spd, short nX, short nY, RECT *dirtyRect = NULL /* in-out */);
    void  WriteSeg(SubPicDesc& spd, short nX, short nY, short nCount, short nPaletteIndex);
    void  InitColor(const SubPicDesc& spd);
    void  SetPalette(int nNbEntry, HDMV_PALETTE* pPalette, ColorType color_type, YuvRangeType yuv_range);
    void  SetPalette(int nNbEntry, DWORD* dwColors);
    bool  HavePalette() {
        return !m_Palette.IsEmpty();
    };

    CompositionObject* Copy() {
        CompositionObject* pCompositionObject = DEBUG_NEW CompositionObject();

        pCompositionObject->m_Palette.SetCount(m_Palette.GetCount());
        for (int i=0,n=m_Palette.GetCount();i<n;i++)
        {
            pCompositionObject->m_Palette[i] = m_Palette[i];
        }
        pCompositionObject->m_OriginalColorType = m_OriginalColorType;
        pCompositionObject->m_OriginalYuvRangeType = m_OriginalYuvRangeType;

        memcpy(pCompositionObject->m_Colors, m_Colors, sizeof(m_Colors));
        pCompositionObject->m_colorType = m_colorType;

        pCompositionObject->m_pRLEData = NULL;
        pCompositionObject->SetRLEData(m_pRLEData, m_nRLEDataSize, m_nRLEDataSize);

        return pCompositionObject;
    }
public:
    static bool m_output_tv_range_rgb;//fix me
private:
    BYTE* m_pRLEData;
    int   m_nRLEDataSize;
    int   m_nRLEPos;

    CAtlArray<HDMV_PALETTE> m_Palette;
    ColorType   m_OriginalColorType;
    YuvRangeType m_OriginalYuvRangeType;

    DWORD       m_Colors[256];
    int         m_colorType;

    void  DvbRenderField(SubPicDesc& spd, CGolombBuffer& gb, short nXStart, short nYStart, short nLength, RECT *dirtyRect = NULL);
    void  Dvb2PixelsCodeString(SubPicDesc& spd, CGolombBuffer& gb, short& nX, short& nY, RECT *dirtyRect = NULL);
    void  Dvb4PixelsCodeString(SubPicDesc& spd, CGolombBuffer& gb, short& nX, short& nY, RECT *dirtyRect = NULL);
    void  Dvb8PixelsCodeString(SubPicDesc& spd, CGolombBuffer& gb, short& nX, short& nY, RECT *dirtyRect = NULL);
};
