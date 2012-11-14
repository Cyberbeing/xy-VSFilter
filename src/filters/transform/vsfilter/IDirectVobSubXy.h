#pragma once

#include "../BaseVideoFilter/BaseVideoFilter.h"
#include "XyOptionsImpl.h"

namespace DirectVobSubXyOptions
{
    enum
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

        INT_LAYOUT_SIZE_OPT,//see @LayoutSizeOpt

        BOOL_FOLLOW_UPSTREAM_PREFERRED_ORDER,
        BOOL_HIDE_TRAY_ICON,

        SIZE_ORIGINAL_VIDEO,
        SIZE_ASS_PLAY_RESOLUTION,
        SIZE_USER_SPECIFIED_LAYOUT_SIZE,
        SIZE_LAYOUT_WITH,//read only

        STRING_LOAD_EXT_LIST,
        STRING_PGS_YUV_RANGE,//TV,PC,GUESS(default)
        STRING_PGS_YUV_MATRIX,//BT601,BT709,GUESS(default)

        //[ColorSpaceOpt1...ColorSpaceOptN]
        //size=N
        BIN_OUTPUT_COLOR_FORMAT,
        BIN_INPUT_COLOR_FORMAT,

        //struct CachesInfo
        //size = 1
        BIN_CACHES_INFO,

        //struct XyFlyWeightInfo
        //size = 1
        BIN_XY_FLY_WEIGHT_INFO,

        OPTION_COUNT
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

    struct CacheInfo
    {
        std::size_t cur_item_num, query_count, hit_count;
    };
    struct XyFlyWeightInfo
    {
        CacheInfo xy_fw_string_w;
        CacheInfo xy_fw_grouped_draw_items_hash_key;
    };
    enum LayoutSizeOpt
    {
        LAYOUT_SIZE_OPT_FOLLOW_ORIGINAL_VIDEO_SIZE,
        LAYOUT_SIZE_OPT_USER_SPECIFIED,
        LAYOUT_SIZE_OPT_COUNT
    };

    const XyOptionsImpl::Option DirectVobFilterOptions[] = {
        {XyOptionsImpl::OPTION_TYPE_INT   , INT_COLOR_SPACE},
        {XyOptionsImpl::OPTION_TYPE_INT   , INT_YUV_RANGE},
        {XyOptionsImpl::OPTION_TYPE_INT   , INT_OVERLAY_CACHE_MAX_ITEM_NUM},
        {XyOptionsImpl::OPTION_TYPE_INT   , INT_SCAN_LINE_DATA_CACHE_MAX_ITEM_NUM},
        {XyOptionsImpl::OPTION_TYPE_INT   , INT_PATH_DATA_CACHE_MAX_ITEM_NUM},
        {XyOptionsImpl::OPTION_TYPE_INT   , INT_OVERLAY_NO_BLUR_CACHE_MAX_ITEM_NUM},

        {XyOptionsImpl::OPTION_TYPE_INT   , INT_BITMAP_MRU_CACHE_ITEM_NUM},
        {XyOptionsImpl::OPTION_TYPE_INT   , INT_CLIPPER_MRU_CACHE_ITEM_NUM},

        {XyOptionsImpl::OPTION_TYPE_INT   , INT_TEXT_INFO_CACHE_ITEM_NUM},
        {XyOptionsImpl::OPTION_TYPE_INT   , INT_ASS_TAG_LIST_CACHE_ITEM_NUM},

        {XyOptionsImpl::OPTION_TYPE_INT   , INT_SUBPIXEL_VARIANCE_CACHE_ITEM_NUM},

        {XyOptionsImpl::OPTION_TYPE_INT   , INT_SUBPIXEL_POS_LEVEL},

        {XyOptionsImpl::OPTION_TYPE_INT   , INT_LAYOUT_SIZE_OPT},

        {XyOptionsImpl::OPTION_TYPE_BOOL  , BOOL_FOLLOW_UPSTREAM_PREFERRED_ORDER},
        {XyOptionsImpl::OPTION_TYPE_BOOL  , BOOL_HIDE_TRAY_ICON},

        {XyOptionsImpl::OPTION_TYPE_SIZE  , SIZE_ORIGINAL_VIDEO},
        {XyOptionsImpl::OPTION_TYPE_SIZE  , SIZE_ASS_PLAY_RESOLUTION},
        {XyOptionsImpl::OPTION_TYPE_SIZE  , SIZE_USER_SPECIFIED_LAYOUT_SIZE},
        {XyOptionsImpl::OPTION_TYPE_SIZE  , SIZE_LAYOUT_WITH},

        {XyOptionsImpl::OPTION_TYPE_STRING, STRING_LOAD_EXT_LIST},
        {XyOptionsImpl::OPTION_TYPE_STRING, STRING_PGS_YUV_RANGE},
        {XyOptionsImpl::OPTION_TYPE_STRING, STRING_PGS_YUV_MATRIX},

        {XyOptionsImpl::OPTION_TYPE_BIN   , BIN_OUTPUT_COLOR_FORMAT},
        {XyOptionsImpl::OPTION_TYPE_BIN   , BIN_INPUT_COLOR_FORMAT},

        {XyOptionsImpl::OPTION_TYPE_BIN   , BIN_CACHES_INFO},

        {XyOptionsImpl::OPTION_TYPE_BIN   , BIN_XY_FLY_WEIGHT_INFO},
        {XyOptionsImpl::OPTION_TYPE_END_FLAG}
    };
};
