/************************************************************************/
/* author:      xy                                                      */
/* date:        20110914                                                */
/* description: color table for yuv rgb convertion,                     */
/*              support both bt.601 and bt.709                          */
/************************************************************************/
#ifndef __COLOR_CONV_TABLE_H_53D078BB_BBA1_441B_9728_67E29A1CB521__
#define __COLOR_CONV_TABLE_H_53D078BB_BBA1_441B_9728_67E29A1CB521__

struct ColorConvTable
{
    enum YUV_Type
    {
        NONE,
        BT601,
        BT709,
    };
    
    static const ColorConvTable* GetColorConvTable(const YUV_Type& yuv_type);
    static void DestroyColorConvTable(const YUV_Type& yuv_type);
    static YUV_Type SetDefaultYUVType(const YUV_Type& yuv_type) { return s_default_yuv_type = yuv_type; }
    static YUV_Type GetDefaultYUVType() { return s_default_yuv_type; };
    static const ColorConvTable* GetDefaultColorConvTable() { return GetColorConvTable(s_default_yuv_type); }

    YUV_Type yuv_type;

    unsigned char Clip_base[256*3];
    const unsigned char* Clip;

    int c2y_cyb;
    int c2y_cyg;
    int c2y_cyr;
    int c2y_cu;
    int c2y_cv;

    int c2y_yb[256];
    int c2y_yg[256];
    int c2y_yr[256];

    int y2c_cbu;
    int y2c_cgu;
    int y2c_cgv;
    int y2c_crv;
    int y2c_bu[256];
    int y2c_gu[256];
    int y2c_gv[256];
    int y2c_rv[256];

    int cy_cy;
    int cy_cy2;

private:
    ColorConvTable(){}

    bool InitColorConvTable(const YUV_Type& yuv_type);

    void InitBT601ColorConvTable();

    void InitBT709ColorConvTable();

    void ColorConvInit();

    static ColorConvTable* s_color_table_bt601;
    static ColorConvTable* s_color_table_bt709;
    static YUV_Type s_default_yuv_type;
};

#endif // end of __COLOR_CONV_TABLE_H_53D078BB_BBA1_441B_9728_67E29A1CB521__
