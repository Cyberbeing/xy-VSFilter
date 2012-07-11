#pragma once

#include "../../../subtitles/STS.h"
#include "../BaseVideoFilter/BaseVideoFilter.h"

#ifdef __cplusplus
extern "C" {
#endif
    namespace DirectVobSubXyIntOptions
    {
        enum//int
        {
            INT_COLOR_SPACE = 0,
            INT_YUV_RANGE,
            INT_OVERLAY_CACHE_MAX_ITEM_NUM,
            INT_SCAN_LINE_DATA_CACHE_MAX_ITEM_NUM,
            INT_PATH_DATA_CACHE_MAX_ITEM_NUM,
            INT_OVERLAY_NO_BLUR_CACHE_MAX_ITEM_NUM,

            INT_BITMAP_MRU_CACHE_ITEM_NUM,
            INT_CLIPPER_MRU_CACHE_ITEM_NUM,

            INT_TEXT_INFO_CACHE_ITEM_NUM,
            INT_ASS_TAG_LIST_CACHE_ITEM_NUM,

            INT_SUBPIXEL_VARIANCE_CACHE_ITEM_NUM,

            INT_SUBPIXEL_POS_LEVEL,
            INT_COUNT
        };
        enum//bool
        {
            BOOL_FOLLOW_UPSTREAM_PREFERRED_ORDER,
            BOOL_HIDE_TRAY_ICON,
            BOOL_COUNT
        };
        enum//ULONGLONG
        {
            //read only
            ULONGLONG_PATH_CACHE_CUR_ITEM_NUM, ULONGLONG_PATH_CACHE_QUERY_COUNT, ULONGLONG_PATH_CACHE_HIT_COUNT,
            ULONGLONG_SCANLINE_CACHE2_CUR_ITEM_NUM, ULONGLONG_SCANLINE_CACHE2_QUERY_COUNT, ULONGLONG_SCANLINE_CACHE2_HIT_COUNT,
            ULONGLONG_NON_BLUR_CACHE_CUR_ITEM_NUM, ULONGLONG_NON_BLUR_CACHE_QUERY_COUNT,ULONGLONG_NON_BLUR_CACHE_HIT_COUNT,
            ULONGLONG_OVERLAY_CACHE_CUR_ITEM_NUM, ULONGLONG_OVERLAY_CACHE_QUERY_COUNT,ULONGLONG_OVERLAY_CACHE_HIT_COUNT,
            ULONGLONG_BITMAP_CACHE_CUR_ITEM_NUM, ULONGLONG_BITMAP_CACHE_QUERY_COUNT, ULONGLONG_BITMAP_CACHE_HIT_COUNT,

            ULONGLONG_INTERPOLATE_CACHE_CUR_ITEM_NUM, ULONGLONG_INTERPOLATE_CACHE_QUERY_COUNT,ULONGLONG_INTERPOLATE_CACHE_HIT_COUNT,
            ULONGLONG_TEXT_INFO_CACHE_CUR_ITEM_NUM, ULONGLONG_TEXT_INFO_CACHE_QUERY_COUNT, ULONGLONG_TEXT_INFO_CACHE_HIT_COUNT,
            ULONGLONG_WORD_INFO_CACHE_CUR_ITEM_NUM, ULONGLONG_WORD_INFO_CACHE_QUERY_COUNT, ULONGLONG_WORD_INFO_CACHE_HIT_COUNT,

            ULONGLONG_SCANLINE_CACHE_CUR_ITEM_NUM, ULONGLONG_SCANLINE_CACHE_QUERY_COUNT,ULONGLONG_SCANLINE_CACHE_HIT_COUNT,
            ULONGLONG_OVERLAY_KEY_CACHE_CUR_ITEM_NUM, ULONGLONG_OVERLAY_KEY_CACHE_QUERY_COUNT, ULONGLONG_OVERLAY_KEY_CACHE_HIT_COUNT,
            ULONGLONG_CLIPPER_CACHE_CUR_ITEM_NUM, ULONGLONG_CLIPPER_CACHE_QUERY_COUNT, ULONGLONG_CLIPPER_CACHE_HIT_COUNT,
            ULONGLONG_COUNT
        };
        enum
        {
            //[ColorSpaceOpt1...ColorSpaceOptN]
            //size=N
            BIN_OUTPUT_COLOR_FORMAT,
            BIN_INPUT_COLOR_FORMAT,

            //struct CachesInfo
            //size = 1
            BIN_CACHES_INFO,

            BIN_COUNT
        };
        struct ColorSpaceOpt
        {
            int color_space;
            bool selected;
        };
        struct CachesInfo
        {
            std::size_t path_cache_cur_item_num, path_cache_query_count,path_cache_hit_count,
                scanline_cache2_cur_item_num, scanline_cache2_query_count,scanline_cache2_hit_count,
                non_blur_cache_cur_item_num, non_blur_cache_query_count,non_blur_cache_hit_count,
                overlay_cache_cur_item_num, overlay_cache_query_count,overlay_cache_hit_count,
                bitmap_cache_cur_item_num, bitmap_cache_query_count, bitmap_cache_hit_count,

                interpolate_cache_cur_item_num, interpolate_cache_query_count,interpolate_cache_hit_count,
                text_info_cache_cur_item_num, text_info_cache_query_count, text_info_cache_hit_count,
                word_info_cache_cur_item_num, word_info_cache_query_count, word_info_cache_hit_count,

                scanline_cache_cur_item_num, scanline_cache_query_count,scanline_cache_hit_count,
                overlay_key_cache_cur_item_num, overlay_key_cache_query_count, overlay_key_cache_hit_count,
                clipper_cache_cur_item_num, clipper_cache_query_count, clipper_cache_hit_count;
        };
    };

    [uuid("85E5D6F9-BEFB-4E01-B047-758359CDF9AB")]
	interface IDirectVobSubXy : public IUnknown 
    {
        STDMETHOD(XyGetBool     )(int field, bool      *value) = 0;
        STDMETHOD(XyGetInt      )(int field, int       *value) = 0;
        STDMETHOD(XyGetSize     )(int field, SIZE      *value) = 0;
        STDMETHOD(XyGetRect     )(int field, RECT      *value) = 0;
        STDMETHOD(XyGetUlonglong)(int field, ULONGLONG *value) = 0;
        STDMETHOD(XyGetDouble   )(int field, double    *value) = 0;
        STDMETHOD(XyGetString   )(int field, LPWSTR    *value, int *chars) = 0;
        STDMETHOD(XyGetBin      )(int field, LPVOID    *value, int *size ) = 0;

        STDMETHOD(XySetBool     )(int field, bool      value) = 0;
        STDMETHOD(XySetInt      )(int field, int       value) = 0;
        STDMETHOD(XySetSize     )(int field, SIZE      value) = 0;
        STDMETHOD(XySetRect     )(int field, RECT      value) = 0;
        STDMETHOD(XySetUlonglong)(int field, ULONGLONG value) = 0;
        STDMETHOD(XySetDouble   )(int field, double    value) = 0;
        STDMETHOD(XySetString   )(int field, LPWSTR    value, int chars) = 0;
        STDMETHOD(XySetBin      )(int field, LPVOID    value, int size ) = 0;
	};

#ifdef __cplusplus
}
#endif
