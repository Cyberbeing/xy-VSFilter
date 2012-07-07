/************************************************************************/
/* author:      xy                                                      */
/* date:        20110914                                                */
/* description: color table for yuv rgb convertion,                     */
/*              support both bt.601 and bt.709                          */
/************************************************************************/
#ifndef __COLOR_CONV_TABLE_H_53D078BB_BBA1_441B_9728_67E29A1CB521__
#define __COLOR_CONV_TABLE_H_53D078BB_BBA1_441B_9728_67E29A1CB521__

#include <WTypes.h>

struct ColorConvTable
{
    enum YuvMatrixType
    {
        NONE
        , BT601
        , BT709
    };

    enum YuvRangeType 
    {
        RANGE_NONE
        , RANGE_TV
        , RANGE_PC
    };

    static void SetDefaultConvType(YuvMatrixType yuv_type, YuvRangeType range);

    static YuvMatrixType GetDefaultYUVType();
    static YuvRangeType GetDefaultRangeType();

    static DWORD Argb2Ayuv(DWORD argb);
    static DWORD Argb2Auyv(DWORD argb);
    static DWORD Rgb2Y(int r8, int g8, int b8);
    static DWORD PreMulArgb2Ayuv( int a8, int r8, int g8, int b8 );

    static DWORD Ayuv2Argb(DWORD ayuv);
    static DWORD Ayuv2Argb_TV_BT601(DWORD ayuv);
    static DWORD A8Y8U8V8_To_ARGB_TV_BT601( int a8, int y8, int u8, int v8 );
    static DWORD Ayuv2Argb_TV_BT709(DWORD ayuv);    
    static DWORD A8Y8U8V8_To_ARGB_TV_BT709( int a8, int y8, int u8, int v8 );
private:
    ColorConvTable();
};

#endif // end of __COLOR_CONV_TABLE_H_53D078BB_BBA1_441B_9728_67E29A1CB521__
