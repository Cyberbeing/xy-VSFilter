#include "stdafx.h"
#include "color_conv_table.h"
#include <string.h>

ColorConvTable* ColorConvTable::s_color_table_bt601 = NULL;
ColorConvTable* ColorConvTable::s_color_table_bt709 = NULL;
ColorConvTable::YUV_Type ColorConvTable::s_default_yuv_type = ColorConvTable::BT601;

const ColorConvTable* ColorConvTable::GetColorConvTable(const YUV_Type& yuv_type)
{
    ColorConvTable* result = NULL;
    switch(yuv_type)
    {
    case BT601:
        if(s_color_table_bt601==NULL) 
        {
            s_color_table_bt601 = new ColorConvTable();
            if(s_color_table_bt601) s_color_table_bt601->InitBT601ColorConvTable();
        }            
        result = s_color_table_bt601;
        break;
    case BT709:
        if(s_color_table_bt709 ==NULL)
        {
            s_color_table_bt709 = new ColorConvTable();
            if(s_color_table_bt709) s_color_table_bt709->InitBT709ColorConvTable();
        }            
        result = s_color_table_bt709;
        break;
    case NONE:
        result = NULL;
        break;
    }
    return result;        
}

void ColorConvTable::DestroyColorConvTable(const YUV_Type& yuv_type)
{
    switch(yuv_type)
    {
    case BT601:
        delete s_color_table_bt601;
        s_color_table_bt601 = NULL;
        break;
    case BT709:
        delete s_color_table_bt709;
        s_color_table_bt709 = NULL;
        break;
    case NONE:
        break;
    }
}

bool ColorConvTable::InitColorConvTable(const YUV_Type& yuv_type)
{
    bool result = false;
    switch(yuv_type)
    {
    case BT601:
        InitBT601ColorConvTable();
        result = true;
        break;
    case BT709:
        InitBT709ColorConvTable();
        result = true;
        break;
    case NONE:
        result = false;
        break;
    }
    return result;
}

void ColorConvTable::InitBT601ColorConvTable()
{
    yuv_type = BT601;
    c2y_cyb = int(0.114*219/255*65536+0.5);
    c2y_cyg = int(0.587*219/255*65536+0.5);
    c2y_cyr = int(0.299*219/255*65536+0.5);

    c2y_cu = int(1.0/2.018*1024+0.5);
    c2y_cv = int(1.0/1.596*1024+0.5);

    y2c_cbu = int(2.018*65536+0.5);
    y2c_cgu = int(0.391*65536+0.5);
    y2c_cgv = int(0.813*65536+0.5);
    y2c_crv = int(1.596*65536+0.5);

    cy_cy = int(255.0/219.0*65536+0.5);
    cy_cy2 = int(255.0/219.0*32768+0.5);
    ColorConvInit();
}

void ColorConvTable::InitBT709ColorConvTable()
{
    yuv_type = BT709;
    c2y_cyb = int(0.0722*219/255*65536+0.5);
    c2y_cyg = int(0.7152*219/255*65536+0.5);
    c2y_cyr = int(0.2126*219/255*65536+0.5);

    c2y_cu = int(1.0/2.113*1024+0.5);
    c2y_cv = int(1.0/1.793*1024+0.5);

    y2c_cbu = int(2.113*65536+0.5);
    y2c_cgu = int(0.213*65536+0.5);
    y2c_cgv = int(0.533*65536+0.5);
    y2c_crv = int(1.793*65536+0.5);

    cy_cy = int(255.0/219.0*65536+0.5);
    cy_cy2 = int(255.0/219.0*32768+0.5);
    ColorConvInit();
}

void ColorConvTable::ColorConvInit()
{    
    Clip = Clip_base + 256;
    for(int i = 0; i < 256; i++)
    {
        Clip_base[i] = 0;
        Clip_base[i+256] = i;
        Clip_base[i+512] = 255;
    }
    for(int i = 0; i < 256; i++)
    {
        c2y_yb[i] = c2y_cyb*i;
        c2y_yg[i] = c2y_cyg*i;
        c2y_yr[i] = c2y_cyr*i;
        y2c_bu[i] = y2c_cbu*(i-128);
        y2c_gu[i] = y2c_cgu*(i-128);
        y2c_gv[i] = y2c_cgv*(i-128);
        y2c_rv[i] = y2c_crv*(i-128);
    }
}

DWORD ColorConvTable::Argb2Auyv( DWORD argb )
{
    const ColorConvTable* color_conv_table = ColorConvTable::GetDefaultColorConvTable();
    DWORD axxv;
    int r = (argb & 0x00ff0000) >> 16;
    int g = (argb & 0x0000ff00) >> 8;
    int b = (argb & 0x000000ff);
    int y = (color_conv_table->c2y_cyb * b + color_conv_table->c2y_cyg * g + color_conv_table->c2y_cyr * r + 0x108000) >> 16;
    int scaled_y = (y-16) * color_conv_table->cy_cy;
    int u = ((((b<<16) - scaled_y) >> 10) * color_conv_table->c2y_cu + 0x800000 + 0x8000) >> 16;
    int v = ((((r<<16) - scaled_y) >> 10) * color_conv_table->c2y_cv + 0x800000 + 0x8000) >> 16;
    u *= (u>0);
    u = 255 - (255-u)*(u<256);
    v *= (v>0);
    v = 255 - (255-v)*(v<256);

    axxv = (argb & 0xff000000) | (y<<8) | (u<<16) | v;

    return axxv;
}

DWORD ColorConvTable::Argb2Ayuv( DWORD argb )
{
    const ColorConvTable* color_conv_table = ColorConvTable::GetDefaultColorConvTable();
    DWORD axxv;
    int r = (argb & 0x00ff0000) >> 16;
    int g = (argb & 0x0000ff00) >> 8;
    int b = (argb & 0x000000ff);
    int y = (color_conv_table->c2y_cyb * b + color_conv_table->c2y_cyg * g + color_conv_table->c2y_cyr * r + 0x108000) >> 16;
    int scaled_y = (y-16) * color_conv_table->cy_cy;
    int u = ((((b<<16) - scaled_y) >> 10) * color_conv_table->c2y_cu + 0x800000 + 0x8000) >> 16;
    int v = ((((r<<16) - scaled_y) >> 10) * color_conv_table->c2y_cv + 0x800000 + 0x8000) >> 16;
    u *= (u>0);
    u = 255 - (255-u)*(u<256);
    v *= (v>0);
    v = 255 - (255-v)*(v<256);

    axxv = (argb & 0xff000000) | (y<<16) | (u<<8) | v;

    return axxv;    
}

DWORD ColorConvTable::Ayuv2Argb_BT601( DWORD ayuv )
{
    return YCrCbToRGB_Rec601( ayuv>>24, (ayuv>>16)&0xff, (ayuv>>8)&0xff, ayuv&0xff ); //todo: fix me
}

DWORD ColorConvTable::Ayuv2Argb_BT709( DWORD ayuv )
{
    return YCrCbToRGB_Rec709( ayuv>>24, (ayuv>>16)&0xff, (ayuv>>8)&0xff, ayuv&0xff ); //todo: fix me
}

DWORD ColorConvTable::Ayuv2Argb( DWORD ayuv )
{
    switch(s_default_yuv_type)
    {
    case BT601:
        return Ayuv2Argb_BT601(ayuv);
        break;
    case BT709:
        return Ayuv2Argb_BT709(ayuv);
        break;
    case NONE:
        //todo fixme: log error
        ASSERT(0);
        break;
    }
}

//
//unsigned char Clip_base[256*3];
//unsigned char* Clip = Clip_base + 256;
//
//bool fClip_baseInitOK = false;
//
//void ColorConvInit()
//{
//    if(fClip_baseInitOK) return;
//    int i;
//    for(i = 0; i < 256; i++)
//    {
//        Clip_base[i] = 0;
//        Clip_base[i+256] = i;
//        Clip_base[i+512] = 255;
//    }
//    fClip_baseInitOK = true;
//}
//
//#define rgb2yuv(r1,g1,b1,r2,g2,b2) \
//    int y1 = (c2y_yb[b1] + c2y_yg[g1] + c2y_yr[r1] + 0x108000) >> 16; \
//    int y2 = (c2y_yb[b2] + c2y_yg[g2] + c2y_yr[r2] + 0x108000) >> 16; \
//    \
//    int scaled_y = (y1+y2-32) * cy_cy2; \
//    \
//    unsigned char u = Clip[(((((b1+b2)<<15) - scaled_y) >> 10) * c2y_cu + 0x800000 + 0x8000) >> 16]; \
//    unsigned char v = Clip[(((((r1+r2)<<15) - scaled_y) >> 10) * c2y_cv + 0x800000 + 0x8000) >> 16]; \
