#include "stdafx.h"
#include "color_conv_table.h"
#include <string.h>

const int FRACTION_BITS = 16;
const int FRACTION_SCALE = 1<<16;

const int TV_Y_RANGE = 219;
const int TV_Y_MIN = 16;

const int PC_Y_RANGE = 255;
const int PC_Y_MIN = 0;

inline int clip(int value, int upper_bound)
{
    value &= ~(value>>31);//value = value > 0 ? value : 0
    return value^((value^upper_bound)&((upper_bound-value)>>31));//value = value < upper_bound ? value : upper_bound
}

#define FLOAT_TO_FIXED(f, SCALE) int((f)*(SCALE)+0.5)

#define DEFINE_RGB2YUV_FUNC(func, Y_RANGE_LOWER, Y_RANGE, double_cyb, double_cyg, double_cyr, double_cu, double_cv, YUV_POS) \
DWORD func(int r8, int g8, int b8)\
{\
    const int c2y_cyb = int(double_cyb*Y_RANGE/255*FRACTION_SCALE+0.5);\
    const int c2y_cyg = int(double_cyg*Y_RANGE/255*FRACTION_SCALE+0.5);\
    const int c2y_cyr = int(double_cyr*Y_RANGE/255*FRACTION_SCALE+0.5);\
    const int c2y_cu = int(1.0/double_cu*1024+0.5);/*Fix me: BT.601 uv range should be 16~240? */\
    const int c2y_cv = int(1.0/double_cv*1024+0.5);\
    const int cy_cy = int(255.0/Y_RANGE*FRACTION_SCALE+0.5);\
    \
    int y  = (c2y_cyr*r8 + c2y_cyg*g8 + c2y_cyb*b8 + Y_RANGE_LOWER*FRACTION_SCALE + FRACTION_SCALE/2) >> FRACTION_BITS;\
    int scaled_y = (y-Y_RANGE_LOWER) * cy_cy;\
    int u = ((((b8<<FRACTION_BITS) - scaled_y) >> 10) * c2y_cu + 128*FRACTION_SCALE + FRACTION_SCALE/2) >> FRACTION_BITS;\
    int v = ((((r8<<FRACTION_BITS) - scaled_y) >> 10) * c2y_cv + 128*FRACTION_SCALE + FRACTION_SCALE/2) >> FRACTION_BITS;\
    \
    u = clip(u, 255); \
    v = clip(v, 255); \
    return (y<<YUV_POS.y) | (u<<YUV_POS.u) | (v<<YUV_POS.v);\
}

#define DEFINE_YUV2RGB_FUNC(func, Y_RANGE_LOWER, Y_RANGE, double_BU, double_GU, double_GV, double_RV) \
DWORD func(int y, int u, int v)\
{\
    const int y2c_cbu = int(double_BU*FRACTION_SCALE+0.5);\
    const int y2c_cgu = int(double_GU*FRACTION_SCALE+0.5);\
    const int y2c_cgv = int(double_GV*FRACTION_SCALE+0.5);\
    const int y2c_crv = int(double_RV*FRACTION_SCALE+0.5);\
    const int cy_cy = int(255.0/Y_RANGE*FRACTION_SCALE+0.5);\
    \
    int scaled_y = (y - Y_RANGE_LOWER) * cy_cy;\
    int b = (scaled_y + y2c_cbu*(u-128) + FRACTION_SCALE/2) >> FRACTION_BITS;\
    int g = (scaled_y - y2c_cgu*(u-128) - y2c_cgv*(v-128) + FRACTION_SCALE/2) >> FRACTION_BITS;\
    int r = (scaled_y + y2c_crv*(v-128) + FRACTION_SCALE/2) >> FRACTION_BITS;\
    b = clip(b, 255); \
    g = clip(g, 255); \
    r = clip(r, 255); \
    return (r<<16) | (g<<8) | b;\
}

#define DEFINE_PREMUL_ARGB2AYUV_FUNC(func, Y_RANGE_LOWER, Y_RANGE, double_cyb, double_cyg, double_cyr, double_cu, double_cv, YUV_POS) \
DWORD func(int a8, int r8, int g8, int b8)\
{\
    const int c2y_cyb = int(double_cyb*Y_RANGE/255*FRACTION_SCALE+0.5);\
    const int c2y_cyg = int(double_cyg*Y_RANGE/255*FRACTION_SCALE+0.5);\
    const int c2y_cyr = int(double_cyr*Y_RANGE/255*FRACTION_SCALE+0.5);\
    const int c2y_cu = int(1.0/double_cu*1024+0.5);\
    const int c2y_cv = int(1.0/double_cv*1024+0.5);\
    const int cy_cy = int(255.0/Y_RANGE*FRACTION_SCALE+0.5);\
    \
    int a = (256-a8)<<(FRACTION_BITS-8);\
    int tmp  = c2y_cyr*r8 + c2y_cyg*g8 + c2y_cyb*b8 + FRACTION_SCALE/2;\
    int y = (tmp + a*Y_RANGE_LOWER) >> FRACTION_BITS;\
    int scaled_y = (tmp>>FRACTION_BITS) * cy_cy;\
    int u = ((((b8<<FRACTION_BITS) - scaled_y) >> 10) * c2y_cu + a*128 + FRACTION_SCALE/2) >> FRACTION_BITS;\
    int v = ((((r8<<FRACTION_BITS) - scaled_y) >> 10) * c2y_cv + a*128 + FRACTION_SCALE/2) >> FRACTION_BITS;\
    \
    u = clip(u, 255); \
    v = clip(v, 255); \
    return (a8<<24) | (y<<YUV_POS.y) | (u<<YUV_POS.u) | (v<<YUV_POS.v);\
}

#define DEFINE_RGB2Y_FUNC(func, Y_RANGE_LOWER, Y_RANGE, double_cyb, double_cyg, double_cyr, double_cu, double_cv) \
DWORD func(int r8, int g8, int b8)\
{\
    const int c2y_cyb = int(double_cyb*Y_RANGE/255*FRACTION_SCALE+0.5);\
    const int c2y_cyg = int(double_cyg*Y_RANGE/255*FRACTION_SCALE+0.5);\
    const int c2y_cyr = int(double_cyr*Y_RANGE/255*FRACTION_SCALE+0.5);\
    const int c2y_cu = int(1.0/double_cu*1024+0.5);\
    const int c2y_cv = int(1.0/double_cv*1024+0.5);\
    const int cy_cy = int(255.0/Y_RANGE*FRACTION_SCALE+0.5);\
    \
    return (c2y_cyr*r8 + c2y_cyg*g8 + c2y_cyb*b8 + Y_RANGE_LOWER*FRACTION_SCALE + FRACTION_SCALE/2) >> FRACTION_BITS;\
}


DWORD RGBToYUV_TV_BT601(int r8, int g8, int b8);
DWORD RGBToYUV_PC_BT601(int r8, int g8, int b8);
DWORD RGBToYUV_TV_BT709(int r8, int g8, int b8);
DWORD RGBToYUV_PC_BT709(int r8, int g8, int b8);

DWORD RGBToUYV_TV_BT601(int r8, int g8, int b8);
DWORD RGBToUYV_PC_BT601(int r8, int g8, int b8);
DWORD RGBToUYV_TV_BT709(int r8, int g8, int b8);
DWORD RGBToUYV_PC_BT709(int r8, int g8, int b8);

DWORD PREMUL_ARGB2AYUV_TV_BT601(int a8, int r8, int g8, int b8);
DWORD PREMUL_ARGB2AYUV_PC_BT601(int a8, int r8, int g8, int b8);
DWORD PREMUL_ARGB2AYUV_TV_BT709(int a8, int r8, int g8, int b8);
DWORD PREMUL_ARGB2AYUV_PC_BT709(int a8, int r8, int g8, int b8);

DWORD RGBToY_TV_BT601(int r8, int g8, int b8);
DWORD RGBToY_PC_BT601(int r8, int g8, int b8);
DWORD RGBToY_TV_BT709(int r8, int g8, int b8);
DWORD RGBToY_PC_BT709(int r8, int g8, int b8);

DWORD YUVToRGB_TV_BT601(int y, int u, int v);
DWORD YUVToRGB_PC_BT601(int y, int u, int v);
DWORD YUVToRGB_TV_BT709(int y, int u, int v);
DWORD YUVToRGB_PC_BT709(int y, int u, int v);

typedef ColorConvTable::YuvMatrixType YuvMatrixType;
typedef ColorConvTable::YuvRangeType YuvRangeType;

class ConvFunc
{
public:
    ConvFunc(YuvMatrixType yuv_type, YuvRangeType range){ InitConvFunc(yuv_type, range); }
    bool InitConvFunc(YuvMatrixType yuv_type, YuvRangeType range);

    typedef DWORD (*R8G8B8ToYuvFunc)(int r8, int g8, int b8);
    typedef DWORD (*PreMulArgbToAyuvFunc)(int a8, int r8, int g8, int b8);
    typedef R8G8B8ToYuvFunc R8G8B8ToY;
    typedef R8G8B8ToYuvFunc Y8U8V8ToRGBFunc;

    R8G8B8ToYuvFunc r8g8b8_to_yuv_func;
    R8G8B8ToYuvFunc r8g8b8_to_uyv_func;
    PreMulArgbToAyuvFunc pre_mul_argb_to_ayuv_func;
    R8G8B8ToY r8g8b8_to_y_func;
    Y8U8V8ToRGBFunc y8u8v8_to_rgb_func;

    YuvMatrixType _yuv_type;
    YuvRangeType _range_type;
};

ConvFunc s_default_conv_set(ColorConvTable::BT601, ColorConvTable::RANGE_TV);

bool ConvFunc::InitConvFunc(YuvMatrixType yuv_type, YuvRangeType range)
{
    bool result = true;

    if ( yuv_type==ColorConvTable::BT601 && range==ColorConvTable::RANGE_TV )
    {
        r8g8b8_to_yuv_func = RGBToYUV_TV_BT601;
        r8g8b8_to_uyv_func = RGBToUYV_TV_BT601;
        pre_mul_argb_to_ayuv_func = PREMUL_ARGB2AYUV_TV_BT601;
        r8g8b8_to_y_func = RGBToY_TV_BT601;
        y8u8v8_to_rgb_func = YUVToRGB_TV_BT601;

        _yuv_type = yuv_type;
        _range_type = range;
    }
    else if ( yuv_type==ColorConvTable::BT709 && range==ColorConvTable::RANGE_TV )
    {
        r8g8b8_to_yuv_func = RGBToYUV_TV_BT709;
        r8g8b8_to_uyv_func = RGBToUYV_TV_BT709;
        pre_mul_argb_to_ayuv_func = PREMUL_ARGB2AYUV_TV_BT709;
        r8g8b8_to_y_func = RGBToY_TV_BT709;
        y8u8v8_to_rgb_func = YUVToRGB_TV_BT709;

        _yuv_type = yuv_type;
        _range_type = range;
    }
    else if ( yuv_type==ColorConvTable::BT601 && range==ColorConvTable::RANGE_PC )
    {
        r8g8b8_to_yuv_func = RGBToYUV_PC_BT601;
        r8g8b8_to_uyv_func = RGBToUYV_PC_BT601;
        pre_mul_argb_to_ayuv_func = PREMUL_ARGB2AYUV_PC_BT601;
        r8g8b8_to_y_func = RGBToY_PC_BT601;
        y8u8v8_to_rgb_func = YUVToRGB_PC_BT601;

        _yuv_type = yuv_type;
        _range_type = range;
    }
    else if ( yuv_type==ColorConvTable::BT709 && range==ColorConvTable::RANGE_PC )
    {
        r8g8b8_to_yuv_func = RGBToYUV_PC_BT709;
        r8g8b8_to_uyv_func = RGBToUYV_PC_BT709;
        pre_mul_argb_to_ayuv_func = PREMUL_ARGB2AYUV_PC_BT709;
        r8g8b8_to_y_func = RGBToY_PC_BT709;
        y8u8v8_to_rgb_func = YUVToRGB_PC_BT709;

        _yuv_type = yuv_type;
        _range_type = range;
    }
    else
    {
        r8g8b8_to_yuv_func = RGBToYUV_TV_BT601;
        r8g8b8_to_uyv_func = RGBToUYV_TV_BT601;
        pre_mul_argb_to_ayuv_func = PREMUL_ARGB2AYUV_TV_BT601;
        r8g8b8_to_y_func = RGBToY_TV_BT601;
        y8u8v8_to_rgb_func = YUVToRGB_TV_BT601;

        _yuv_type = ColorConvTable::BT601;
        _range_type = ColorConvTable::RANGE_TV;
    }

    return result;
}

ColorConvTable::YuvMatrixType ColorConvTable::GetDefaultYUVType()
{
    return s_default_conv_set._yuv_type;
}

ColorConvTable::YuvRangeType ColorConvTable::GetDefaultRangeType()
{
    return s_default_conv_set._range_type;
}

void ColorConvTable::SetDefaultConvType( YuvMatrixType yuv_type, YuvRangeType range )
{
    if( s_default_conv_set._yuv_type != yuv_type || s_default_conv_set._range_type != range )
    {
        s_default_conv_set.InitConvFunc(yuv_type, range);
    }
}

DWORD ColorConvTable::Argb2Auyv( DWORD argb )
{
    int r = (argb & 0x00ff0000) >> 16;
    int g = (argb & 0x0000ff00) >> 8;
    int b = (argb & 0x000000ff);
    return (argb & 0xff000000) | s_default_conv_set.r8g8b8_to_uyv_func(r, g, b);
}

DWORD ColorConvTable::Argb2Ayuv( DWORD argb )
{
    int r = (argb & 0x00ff0000) >> 16;
    int g = (argb & 0x0000ff00) >> 8;
    int b = (argb & 0x000000ff);
    return (argb & 0xff000000) | s_default_conv_set.r8g8b8_to_yuv_func(r, g, b);
}

DWORD ColorConvTable::Argb2Ayuv_TV_BT601( DWORD argb )
{
    int r = (argb & 0x00ff0000) >> 16;
    int g = (argb & 0x0000ff00) >> 8;
    int b = (argb & 0x000000ff);
    return (argb & 0xff000000) | RGBToYUV_TV_BT601(r, g, b);
}

DWORD ColorConvTable::Ayuv2Auyv( DWORD ayuv )
{
    int y = (ayuv & 0x00ff0000) >> 8;
    int u = (ayuv & 0x0000ff00) << 8;
    return (ayuv & 0xff0000ff)| u | y;
}

DWORD ColorConvTable::PreMulArgb2Ayuv( int a8, int r8, int g8, int b8 )
{
    return s_default_conv_set.pre_mul_argb_to_ayuv_func(a8, r8, g8, b8);
}

DWORD ColorConvTable::Rgb2Y( int r8, int g8, int b8 )
{
    return s_default_conv_set.r8g8b8_to_y_func(r8, g8, b8);
}

DWORD ColorConvTable::Ayuv2Argb_TV_BT601( DWORD ayuv )
{
    int y = (ayuv & 0x00ff0000) >> 16;
    int u = (ayuv & 0x0000ff00) >> 8;
    int v = (ayuv & 0x000000ff);
    return (ayuv & 0xff000000) | YUVToRGB_TV_BT601(y, u, v);
}

DWORD ColorConvTable::Ayuv2Argb_TV_BT709( DWORD ayuv )
{
    int y = (ayuv & 0x00ff0000) >> 16;
    int u = (ayuv & 0x0000ff00) >> 8;
    int v = (ayuv & 0x000000ff);
    return (ayuv & 0xff000000) | YUVToRGB_TV_BT709(y, u, v);
}

DWORD ColorConvTable::Ayuv2Argb( DWORD ayuv )
{
    int y = (ayuv & 0x00ff0000) >> 16;
    int u = (ayuv & 0x0000ff00) >> 8;
    int v = (ayuv & 0x000000ff);
    return (ayuv & 0xff000000) | s_default_conv_set.y8u8v8_to_rgb_func(y, u, v);
}

DWORD ColorConvTable::A8Y8U8V8_To_ARGB_TV_BT601( int a8, int y8, int u8, int v8 )
{
    return (a8<<24) | YUVToRGB_TV_BT601(y8, u8, v8);
}

DWORD ColorConvTable::A8Y8U8V8_To_ARGB_PC_BT601( int a8, int y8, int u8, int v8 )
{
    return (a8<<24) | YUVToRGB_PC_BT601(y8, u8, v8);
}

DWORD ColorConvTable::A8Y8U8V8_To_ARGB_TV_BT709( int a8, int y8, int u8, int v8 )
{
    return (a8<<24) | YUVToRGB_TV_BT709(y8, u8, v8);
}

DWORD ColorConvTable::A8Y8U8V8_To_ARGB_PC_BT709( int a8, int y8, int u8, int v8 )
{
    return (a8<<24) | YUVToRGB_PC_BT709(y8, u8, v8);
}

DWORD ColorConvTable::A8Y8U8V8_PC_To_TV( int a8, int y8, int u8, int v8 )
{
    const int FRACTION_SCALE = 1<<16;
    const int YUV_MIN = 16;
    const int cy = int(219.0/255*FRACTION_SCALE+0.5);
    const int cuv = int(224.0/255*FRACTION_SCALE+0.5);/*Fixme: the RGBToYUVs we used doesnot seem to stretch chroma correctly*/
    y8 = ((y8*cy)>>16) + YUV_MIN;
    u8 = ((u8*cuv)>>16) + YUV_MIN;
    v8 = ((v8*cuv)>>16) + YUV_MIN;
    return (a8<<24) | (y8<<16) | (u8<<8) | v8;
}

DWORD ColorConvTable::A8Y8U8V8_TV_To_PC( int a8, int y8, int u8, int v8 )
{
    const int FRACTION_SCALE = 1<<16;
    const int YUV_MIN = 16;
    const int cy = int(255/219.0*FRACTION_SCALE+0.5);
    const int cuv = int(255/224.0*FRACTION_SCALE+0.5);/*Fixme: the RGBToYUVs we used doesnot seem to stretch chroma correctly*/
    y8 = ((y8-YUV_MIN)*cy)>>16;
    u8 = ((u8-YUV_MIN)*cuv)>>16;
    v8 = ((v8-YUV_MIN)*cuv)>>16;
    return (a8<<24) | (y8<<16) | (u8<<8) | v8;
}

struct YuvPos
{
    int y;
    int u;
    int v;
};
const YuvPos POS_YUV = {16, 8, 0};
const YuvPos POS_UYV = {8, 16, 0};


DEFINE_RGB2YUV_FUNC(RGBToYUV_TV_BT601, 16, 219, 0.114, 0.587, 0.299, 2.018, 1.596, POS_YUV)
DEFINE_RGB2YUV_FUNC(RGBToYUV_PC_BT601,  0, 255, 0.114, 0.587, 0.299, 2.018, 1.596, POS_YUV)
DEFINE_RGB2YUV_FUNC(RGBToYUV_TV_BT709, 16, 219, 0.0722, 0.7152, 0.2126, 2.113, 1.793, POS_YUV)
DEFINE_RGB2YUV_FUNC(RGBToYUV_PC_BT709,  0, 255, 0.0722, 0.7152, 0.2126, 2.113, 1.793, POS_YUV)


DEFINE_RGB2YUV_FUNC(RGBToUYV_TV_BT601, 16, 219, 0.114, 0.587, 0.299, 2.018, 1.596, POS_UYV)
DEFINE_RGB2YUV_FUNC(RGBToUYV_PC_BT601,  0, 255, 0.114, 0.587, 0.299, 2.018, 1.596, POS_UYV)
DEFINE_RGB2YUV_FUNC(RGBToUYV_TV_BT709, 16, 219, 0.0722, 0.7152, 0.2126, 2.113, 1.793, POS_UYV)
DEFINE_RGB2YUV_FUNC(RGBToUYV_PC_BT709,  0, 255, 0.0722, 0.7152, 0.2126, 2.113, 1.793, POS_UYV)


DEFINE_YUV2RGB_FUNC(YUVToRGB_TV_BT601, 16, 219, 2.018, 0.391, 0.813, 1.596)
DEFINE_YUV2RGB_FUNC(YUVToRGB_PC_BT601,  0, 255, 2.018, 0.391, 0.813, 1.596)
DEFINE_YUV2RGB_FUNC(YUVToRGB_TV_BT709, 16, 219, 2.113, 0.213, 0.533, 1.793)
DEFINE_YUV2RGB_FUNC(YUVToRGB_PC_BT709,  0, 255, 2.113, 0.213, 0.533, 1.793)


DEFINE_PREMUL_ARGB2AYUV_FUNC(PREMUL_ARGB2AYUV_TV_BT601, 16, 219, 0.114, 0.587, 0.299, 2.018, 1.596, POS_YUV)
DEFINE_PREMUL_ARGB2AYUV_FUNC(PREMUL_ARGB2AYUV_PC_BT601,  0, 255, 0.114, 0.587, 0.299, 2.018, 1.596, POS_YUV)
DEFINE_PREMUL_ARGB2AYUV_FUNC(PREMUL_ARGB2AYUV_TV_BT709, 16, 219, 0.0722, 0.7152, 0.2126, 2.113, 1.793, POS_YUV)
DEFINE_PREMUL_ARGB2AYUV_FUNC(PREMUL_ARGB2AYUV_PC_BT709,  0, 255, 0.0722, 0.7152, 0.2126, 2.113, 1.793, POS_YUV)


DEFINE_RGB2Y_FUNC(RGBToY_TV_BT601, 16, 219, 0.114, 0.587, 0.299, 2.018, 1.596)
DEFINE_RGB2Y_FUNC(RGBToY_PC_BT601,  0, 255, 0.114, 0.587, 0.299, 2.018, 1.596)
DEFINE_RGB2Y_FUNC(RGBToY_TV_BT709, 16, 219, 0.0722, 0.7152, 0.2126, 2.113, 1.793)
DEFINE_RGB2Y_FUNC(RGBToY_PC_BT709,  0, 255, 0.0722, 0.7152, 0.2126, 2.113, 1.793)
