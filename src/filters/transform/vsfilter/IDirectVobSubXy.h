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

        INT_VSFILTER_COMPACT_RGB_CORRECTION,//see @VsfilterCompactRgbCorrection

        INT_LANGUAGE_COUNT,

        INT_SELECTED_LANGUAGE,

        BOOL_FOLLOW_UPSTREAM_PREFERRED_ORDER,
        BOOL_HIDE_TRAY_ICON,
        BOOL_HIDE_SUBTITLES,

        INT_MAX_BITMAP_COUNT,
        BOOL_COMBINE_BITMAPS,
        INT_MAX_BITMAP_COUNT2,

        SIZE_ORIGINAL_VIDEO,
        SIZE_ASS_PLAY_RESOLUTION,
        SIZE_USER_SPECIFIED_LAYOUT_SIZE,
        SIZE_LAYOUT_WITH,//read only
        SIZE_AR_ADJUSTED_VIDEO,

        RECT_VIDEO_OUTPUT,
        RECT_SUBTITLE_TARGET,

        DOUBLE_FPS,

        BOOL_MEDIA_FPS_ENABLED,
        DOUBLE_MEDIA_FPS,

        STRING_LOAD_EXT_LIST,
        STRING_PGS_YUV_RANGE,//TV,PC,GUESS(default)
        STRING_PGS_YUV_MATRIX,//BT601,BT709,GUESS(default)
        STRING_FILE_NAME,

        STRING_NAME,
        STRING_VERSION,
        STRING_YUV_MATRIX,
        STRING_CONSUMER_YUV_MATRIX,
        STRING_CONNECTED_CONSUMER,
        STRING_CONSUMER_VERSION,

        //[ColorSpaceOpt1...ColorSpaceOptN]
        //size=N
        BIN_OUTPUT_COLOR_FORMAT,
        BIN_INPUT_COLOR_FORMAT,

        //struct CachesInfo
        //size = sizeof(CachesInfo)
        BIN2_CACHES_INFO,

        //struct XyFlyWeightInfo
        //size = sizeof(XyFlyWeightInfo)
        BIN2_XY_FLY_WEIGHT_INFO,

        // for XySubFilterConsumer
        ULONGLOG_FRAME_RATE,
        DOUBLE_REFRESH_RATE,
        SIZE_DISPLAY_MODE,

        BOOL_FLIP_PICTURE,
        BOOL_FLIP_SUBTITLE,

        BOOL_OSD,
        BOOL_FORCED_LOAD,
        BOOL_SAVE_FULL_PATH,
        BOOL_PRE_BUFFERING,

        BOOL_OVERRIDE_PLACEMENT,
        SIZE_PLACEMENT_PERC,// in 1%

        BOOL_VOBSUBSETTINGS_BUFFER,
        BOOL_VOBSUBSETTINGS_ONLY_SHOW_FORCED_SUBS,
        BOOL_VOBSUBSETTINGS_POLYGONIZE,

        INT_EXTEND_PICTURE_HORIZONTAL,
        INT_EXTEND_PICTURE_VERTICAL,
        INT_EXTEND_PICTURE_RESX2,
        SIZE_EXTEND_PICTURE_RESX2MIN,

        INT_ASPECT_RATIO_SETTINGS,

        INT_LOAD_SETTINGS_LEVEL,
        BOOL_LOAD_SETTINGS_EXTENAL,
        BOOL_LOAD_SETTINGS_WEB,
        BOOL_LOAD_SETTINGS_EMBEDDED,

        BOOL_SUB_FRAME_USE_DST_ALPHA,

        //The following is not really supported yet
        void_LanguageName,
        void_TextSettings,
        void_SubtitleTiming,
        void_ZoomRect,
        void_ColorFormat,
        void_SubtitleReloader,
        void_HasConfigDialog,
        void_ShowConfigDialog,
        void_IsSubtitleReloaderLocked,
        void_LockSubtitleReloader,
        void_AdviseSubClock,

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
    enum VsfilterCompactRgbCorrection
    {
        RGB_CORRECTION_AUTO = 0,
        RGB_CORRECTION_NEVER,
        RGB_CORRECTION_ALWAYS,
        RGB_CORRECTION_COUNT
    };
    const XyOptionsImpl::Option DirectVobFilterOptions[] = {
        {XyOptionsImpl::OPTION_TYPE_INT   , XyOptionsImpl::OPTION_MODE_RW, INT_COLOR_SPACE},
        {XyOptionsImpl::OPTION_TYPE_INT   , XyOptionsImpl::OPTION_MODE_RW, INT_YUV_RANGE},
        {XyOptionsImpl::OPTION_TYPE_INT   , XyOptionsImpl::OPTION_MODE_RW, INT_OVERLAY_CACHE_MAX_ITEM_NUM},
        {XyOptionsImpl::OPTION_TYPE_INT   , XyOptionsImpl::OPTION_MODE_RW, INT_SCAN_LINE_DATA_CACHE_MAX_ITEM_NUM},
        {XyOptionsImpl::OPTION_TYPE_INT   , XyOptionsImpl::OPTION_MODE_RW, INT_PATH_DATA_CACHE_MAX_ITEM_NUM},
        {XyOptionsImpl::OPTION_TYPE_INT   , XyOptionsImpl::OPTION_MODE_RW, INT_OVERLAY_NO_BLUR_CACHE_MAX_ITEM_NUM},

        {XyOptionsImpl::OPTION_TYPE_INT   , XyOptionsImpl::OPTION_MODE_RW, INT_BITMAP_MRU_CACHE_ITEM_NUM},
        {XyOptionsImpl::OPTION_TYPE_INT   , XyOptionsImpl::OPTION_MODE_RW, INT_CLIPPER_MRU_CACHE_ITEM_NUM},

        {XyOptionsImpl::OPTION_TYPE_INT   , XyOptionsImpl::OPTION_MODE_RW, INT_TEXT_INFO_CACHE_ITEM_NUM},
        {XyOptionsImpl::OPTION_TYPE_INT   , XyOptionsImpl::OPTION_MODE_RW, INT_ASS_TAG_LIST_CACHE_ITEM_NUM},

        {XyOptionsImpl::OPTION_TYPE_INT   , XyOptionsImpl::OPTION_MODE_RW, INT_SUBPIXEL_VARIANCE_CACHE_ITEM_NUM},

        {XyOptionsImpl::OPTION_TYPE_INT   , XyOptionsImpl::OPTION_MODE_RW, INT_SUBPIXEL_POS_LEVEL},

        {XyOptionsImpl::OPTION_TYPE_INT   , XyOptionsImpl::OPTION_MODE_RW, INT_LAYOUT_SIZE_OPT},

        {XyOptionsImpl::OPTION_TYPE_INT   , XyOptionsImpl::OPTION_MODE_READ, INT_LANGUAGE_COUNT},

        {XyOptionsImpl::OPTION_TYPE_INT   , XyOptionsImpl::OPTION_MODE_RW, INT_SELECTED_LANGUAGE},

        {XyOptionsImpl::OPTION_TYPE_BOOL  , XyOptionsImpl::OPTION_MODE_RW, BOOL_FOLLOW_UPSTREAM_PREFERRED_ORDER},
        {XyOptionsImpl::OPTION_TYPE_BOOL  , XyOptionsImpl::OPTION_MODE_RW, BOOL_HIDE_TRAY_ICON},
        {XyOptionsImpl::OPTION_TYPE_BOOL  , XyOptionsImpl::OPTION_MODE_RW, BOOL_MEDIA_FPS_ENABLED},
        {XyOptionsImpl::OPTION_TYPE_BOOL  , XyOptionsImpl::OPTION_MODE_RW, BOOL_HIDE_SUBTITLES},

        {XyOptionsImpl::OPTION_TYPE_DOUBLE, XyOptionsImpl::OPTION_MODE_RW, DOUBLE_MEDIA_FPS},

        {XyOptionsImpl::OPTION_TYPE_SIZE  , XyOptionsImpl::OPTION_MODE_READ, SIZE_ORIGINAL_VIDEO},
        {XyOptionsImpl::OPTION_TYPE_SIZE  , XyOptionsImpl::OPTION_MODE_READ, SIZE_ASS_PLAY_RESOLUTION},
        {XyOptionsImpl::OPTION_TYPE_SIZE  , XyOptionsImpl::OPTION_MODE_RW, SIZE_USER_SPECIFIED_LAYOUT_SIZE},
        {XyOptionsImpl::OPTION_TYPE_SIZE  , XyOptionsImpl::OPTION_MODE_READ, SIZE_LAYOUT_WITH},
        {XyOptionsImpl::OPTION_TYPE_SIZE  , XyOptionsImpl::OPTION_MODE_RW, SIZE_AR_ADJUSTED_VIDEO},

        {XyOptionsImpl::OPTION_TYPE_STRING, XyOptionsImpl::OPTION_MODE_RW, STRING_LOAD_EXT_LIST},
        {XyOptionsImpl::OPTION_TYPE_STRING, XyOptionsImpl::OPTION_MODE_RW, STRING_PGS_YUV_RANGE},
        {XyOptionsImpl::OPTION_TYPE_STRING, XyOptionsImpl::OPTION_MODE_RW, STRING_PGS_YUV_MATRIX},
        {XyOptionsImpl::OPTION_TYPE_STRING, XyOptionsImpl::OPTION_MODE_RW, STRING_FILE_NAME},

        {XyOptionsImpl::OPTION_TYPE_STRING, XyOptionsImpl::OPTION_MODE_READ, STRING_NAME},
        {XyOptionsImpl::OPTION_TYPE_STRING, XyOptionsImpl::OPTION_MODE_READ, STRING_VERSION},

        {XyOptionsImpl::OPTION_TYPE_BIN   , XyOptionsImpl::OPTION_MODE_RW, BIN_OUTPUT_COLOR_FORMAT},
        {XyOptionsImpl::OPTION_TYPE_BIN   , XyOptionsImpl::OPTION_MODE_RW, BIN_INPUT_COLOR_FORMAT},

        {XyOptionsImpl::OPTION_TYPE_BIN2  , XyOptionsImpl::OPTION_MODE_READ, BIN2_CACHES_INFO},

        {XyOptionsImpl::OPTION_TYPE_BIN2  , XyOptionsImpl::OPTION_MODE_READ, BIN2_XY_FLY_WEIGHT_INFO},

        {XyOptionsImpl::OPTION_TYPE_BIN   , XyOptionsImpl::OPTION_MODE_RW, void_LanguageName},
        {XyOptionsImpl::OPTION_TYPE_BOOL  , XyOptionsImpl::OPTION_MODE_RW, BOOL_OVERRIDE_PLACEMENT},
        {XyOptionsImpl::OPTION_TYPE_SIZE  , XyOptionsImpl::OPTION_MODE_RW, SIZE_PLACEMENT_PERC},
        {XyOptionsImpl::OPTION_TYPE_BOOL  , XyOptionsImpl::OPTION_MODE_RW, BOOL_VOBSUBSETTINGS_BUFFER},
        {XyOptionsImpl::OPTION_TYPE_BOOL  , XyOptionsImpl::OPTION_MODE_RW, BOOL_VOBSUBSETTINGS_ONLY_SHOW_FORCED_SUBS},
        {XyOptionsImpl::OPTION_TYPE_BOOL  , XyOptionsImpl::OPTION_MODE_RW, BOOL_VOBSUBSETTINGS_POLYGONIZE},
        {XyOptionsImpl::OPTION_TYPE_BIN   , XyOptionsImpl::OPTION_MODE_RW, void_TextSettings},
        {XyOptionsImpl::OPTION_TYPE_BOOL  , XyOptionsImpl::OPTION_MODE_RW, BOOL_FLIP_PICTURE},
        {XyOptionsImpl::OPTION_TYPE_BOOL  , XyOptionsImpl::OPTION_MODE_RW, BOOL_FLIP_SUBTITLE},
        {XyOptionsImpl::OPTION_TYPE_BOOL  , XyOptionsImpl::OPTION_MODE_RW, BOOL_OSD},
        {XyOptionsImpl::OPTION_TYPE_BOOL  , XyOptionsImpl::OPTION_MODE_RW, BOOL_SAVE_FULL_PATH},
        {XyOptionsImpl::OPTION_TYPE_BIN   , XyOptionsImpl::OPTION_MODE_RW, void_SubtitleTiming},
        {XyOptionsImpl::OPTION_TYPE_BIN   , XyOptionsImpl::OPTION_MODE_RW, void_ZoomRect},
        {XyOptionsImpl::OPTION_TYPE_BIN   , XyOptionsImpl::OPTION_MODE_RW, void_ColorFormat},
        {XyOptionsImpl::OPTION_TYPE_BIN   , XyOptionsImpl::OPTION_MODE_RW, void_SubtitleReloader},
        //{XyOptionsImpl::OPTION_TYPE_INT   , XyOptionsImpl::OPTION_MODE_RW, INT_EXTEND_PICTURE_HORIZONTAL},
        //{XyOptionsImpl::OPTION_TYPE_INT   , XyOptionsImpl::OPTION_MODE_RW, INT_EXTEND_PICTURE_VERTICAL},
        //{XyOptionsImpl::OPTION_TYPE_INT   , XyOptionsImpl::OPTION_MODE_RW, INT_EXTEND_PICTURE_RESX2},
        //{XyOptionsImpl::OPTION_TYPE_SIZE  , XyOptionsImpl::OPTION_MODE_RW, SIZE_EXTEND_PICTURE_RESX2MIN},
        {XyOptionsImpl::OPTION_TYPE_INT   , XyOptionsImpl::OPTION_MODE_RW, INT_LOAD_SETTINGS_LEVEL},
        {XyOptionsImpl::OPTION_TYPE_BOOL  , XyOptionsImpl::OPTION_MODE_RW, BOOL_LOAD_SETTINGS_EXTENAL},
        {XyOptionsImpl::OPTION_TYPE_BOOL  , XyOptionsImpl::OPTION_MODE_RW, BOOL_LOAD_SETTINGS_WEB},
        {XyOptionsImpl::OPTION_TYPE_BOOL  , XyOptionsImpl::OPTION_MODE_RW, BOOL_LOAD_SETTINGS_EMBEDDED},

        {XyOptionsImpl::OPTION_TYPE_BOOL  , XyOptionsImpl::OPTION_MODE_RW, BOOL_FORCED_LOAD},
        {XyOptionsImpl::OPTION_TYPE_INT   , XyOptionsImpl::OPTION_MODE_RW, INT_ASPECT_RATIO_SETTINGS},
        {XyOptionsImpl::OPTION_TYPE_BIN   , XyOptionsImpl::OPTION_MODE_RW, void_HasConfigDialog},
        {XyOptionsImpl::OPTION_TYPE_BIN   , XyOptionsImpl::OPTION_MODE_RW, void_ShowConfigDialog},
        {XyOptionsImpl::OPTION_TYPE_BIN   , XyOptionsImpl::OPTION_MODE_RW, void_IsSubtitleReloaderLocked},
        {XyOptionsImpl::OPTION_TYPE_BIN   , XyOptionsImpl::OPTION_MODE_RW, void_LockSubtitleReloader},
        {XyOptionsImpl::OPTION_TYPE_BIN   , XyOptionsImpl::OPTION_MODE_RW, void_AdviseSubClock},

        {XyOptionsImpl::OPTION_TYPE_END_FLAG}
    };

    const XyOptionsImpl::Option XyVobFilterOptions[] = {
        //{XyOptionsImpl::OPTION_TYPE_INT   , XyOptionsImpl::OPTION_MODE_RW, INT_COLOR_SPACE},
        //{XyOptionsImpl::OPTION_TYPE_INT   , XyOptionsImpl::OPTION_MODE_RW, INT_YUV_RANGE},
        {XyOptionsImpl::OPTION_TYPE_INT   , XyOptionsImpl::OPTION_MODE_RW, INT_OVERLAY_CACHE_MAX_ITEM_NUM},
        {XyOptionsImpl::OPTION_TYPE_INT   , XyOptionsImpl::OPTION_MODE_RW, INT_SCAN_LINE_DATA_CACHE_MAX_ITEM_NUM},
        {XyOptionsImpl::OPTION_TYPE_INT   , XyOptionsImpl::OPTION_MODE_RW, INT_PATH_DATA_CACHE_MAX_ITEM_NUM},
        {XyOptionsImpl::OPTION_TYPE_INT   , XyOptionsImpl::OPTION_MODE_RW, INT_OVERLAY_NO_BLUR_CACHE_MAX_ITEM_NUM},

        {XyOptionsImpl::OPTION_TYPE_INT   , XyOptionsImpl::OPTION_MODE_RW, INT_BITMAP_MRU_CACHE_ITEM_NUM},
        {XyOptionsImpl::OPTION_TYPE_INT   , XyOptionsImpl::OPTION_MODE_RW, INT_CLIPPER_MRU_CACHE_ITEM_NUM},

        {XyOptionsImpl::OPTION_TYPE_INT   , XyOptionsImpl::OPTION_MODE_RW, INT_TEXT_INFO_CACHE_ITEM_NUM},
        {XyOptionsImpl::OPTION_TYPE_INT   , XyOptionsImpl::OPTION_MODE_RW, INT_ASS_TAG_LIST_CACHE_ITEM_NUM},

        {XyOptionsImpl::OPTION_TYPE_INT   , XyOptionsImpl::OPTION_MODE_RW, INT_SUBPIXEL_VARIANCE_CACHE_ITEM_NUM},

        {XyOptionsImpl::OPTION_TYPE_INT   , XyOptionsImpl::OPTION_MODE_RW, INT_SUBPIXEL_POS_LEVEL},

        //{XyOptionsImpl::OPTION_TYPE_INT   , XyOptionsImpl::OPTION_MODE_RW, INT_LAYOUT_SIZE_OPT},

        {XyOptionsImpl::OPTION_TYPE_INT   , XyOptionsImpl::OPTION_MODE_RW, INT_VSFILTER_COMPACT_RGB_CORRECTION},

        {XyOptionsImpl::OPTION_TYPE_INT   , XyOptionsImpl::OPTION_MODE_READ, INT_LANGUAGE_COUNT},

        {XyOptionsImpl::OPTION_TYPE_INT   , XyOptionsImpl::OPTION_MODE_RW, INT_SELECTED_LANGUAGE},

        {XyOptionsImpl::OPTION_TYPE_BOOL  , XyOptionsImpl::OPTION_MODE_RW, BOOL_HIDE_TRAY_ICON},
        {XyOptionsImpl::OPTION_TYPE_BOOL  , XyOptionsImpl::OPTION_MODE_RW, BOOL_HIDE_SUBTITLES},

        {XyOptionsImpl::OPTION_TYPE_INT   , XyOptionsImpl::OPTION_MODE_RW, INT_MAX_BITMAP_COUNT},
        {XyOptionsImpl::OPTION_TYPE_BOOL  , XyOptionsImpl::OPTION_MODE_RW, BOOL_COMBINE_BITMAPS},
        {XyOptionsImpl::OPTION_TYPE_INT   , XyOptionsImpl::OPTION_MODE_RW, INT_MAX_BITMAP_COUNT2},

        {XyOptionsImpl::OPTION_TYPE_SIZE  , XyOptionsImpl::OPTION_MODE_READ, SIZE_ORIGINAL_VIDEO},
        {XyOptionsImpl::OPTION_TYPE_SIZE  , XyOptionsImpl::OPTION_MODE_READ, SIZE_ASS_PLAY_RESOLUTION},
        {XyOptionsImpl::OPTION_TYPE_SIZE  , XyOptionsImpl::OPTION_MODE_RW, SIZE_USER_SPECIFIED_LAYOUT_SIZE},
        //{XyOptionsImpl::OPTION_TYPE_SIZE  , XyOptionsImpl::OPTION_MODE_RW, SIZE_LAYOUT_WITH},
        {XyOptionsImpl::OPTION_TYPE_SIZE  , XyOptionsImpl::OPTION_MODE_READ, SIZE_AR_ADJUSTED_VIDEO},

        {XyOptionsImpl::OPTION_TYPE_RECT  , XyOptionsImpl::OPTION_MODE_READ, RECT_VIDEO_OUTPUT},
        {XyOptionsImpl::OPTION_TYPE_RECT  , XyOptionsImpl::OPTION_MODE_READ, RECT_SUBTITLE_TARGET},

        {XyOptionsImpl::OPTION_TYPE_DOUBLE, XyOptionsImpl::OPTION_MODE_RW, DOUBLE_FPS},

        {XyOptionsImpl::OPTION_TYPE_BOOL  , XyOptionsImpl::OPTION_MODE_RW, BOOL_MEDIA_FPS_ENABLED},
        {XyOptionsImpl::OPTION_TYPE_DOUBLE, XyOptionsImpl::OPTION_MODE_RW, DOUBLE_MEDIA_FPS},

        {XyOptionsImpl::OPTION_TYPE_STRING, XyOptionsImpl::OPTION_MODE_RW, STRING_LOAD_EXT_LIST},
        {XyOptionsImpl::OPTION_TYPE_STRING, XyOptionsImpl::OPTION_MODE_RW, STRING_PGS_YUV_RANGE},
        {XyOptionsImpl::OPTION_TYPE_STRING, XyOptionsImpl::OPTION_MODE_RW, STRING_PGS_YUV_MATRIX},
        {XyOptionsImpl::OPTION_TYPE_STRING, XyOptionsImpl::OPTION_MODE_RW, STRING_FILE_NAME},

        {XyOptionsImpl::OPTION_TYPE_STRING, XyOptionsImpl::OPTION_MODE_READ, STRING_NAME},
        {XyOptionsImpl::OPTION_TYPE_STRING, XyOptionsImpl::OPTION_MODE_READ, STRING_VERSION},
        {XyOptionsImpl::OPTION_TYPE_STRING, XyOptionsImpl::OPTION_MODE_READ, STRING_YUV_MATRIX},
        {XyOptionsImpl::OPTION_TYPE_STRING, XyOptionsImpl::OPTION_MODE_READ, STRING_CONSUMER_YUV_MATRIX},
        {XyOptionsImpl::OPTION_TYPE_STRING, XyOptionsImpl::OPTION_MODE_READ, STRING_CONNECTED_CONSUMER},
        {XyOptionsImpl::OPTION_TYPE_STRING, XyOptionsImpl::OPTION_MODE_READ, STRING_CONSUMER_VERSION},

        {XyOptionsImpl::OPTION_TYPE_BIN2  , XyOptionsImpl::OPTION_MODE_READ, BIN2_CACHES_INFO},

        {XyOptionsImpl::OPTION_TYPE_BIN2  , XyOptionsImpl::OPTION_MODE_READ, BIN2_XY_FLY_WEIGHT_INFO},

        {XyOptionsImpl::OPTION_TYPE_BIN   , XyOptionsImpl::OPTION_MODE_RW, void_LanguageName},
        {XyOptionsImpl::OPTION_TYPE_BOOL  , XyOptionsImpl::OPTION_MODE_RW, BOOL_OVERRIDE_PLACEMENT},
        {XyOptionsImpl::OPTION_TYPE_SIZE  , XyOptionsImpl::OPTION_MODE_RW, SIZE_PLACEMENT_PERC},
        {XyOptionsImpl::OPTION_TYPE_BOOL  , XyOptionsImpl::OPTION_MODE_RW, BOOL_VOBSUBSETTINGS_BUFFER},
        {XyOptionsImpl::OPTION_TYPE_BOOL  , XyOptionsImpl::OPTION_MODE_RW, BOOL_VOBSUBSETTINGS_ONLY_SHOW_FORCED_SUBS},
        {XyOptionsImpl::OPTION_TYPE_BOOL  , XyOptionsImpl::OPTION_MODE_RW, BOOL_VOBSUBSETTINGS_POLYGONIZE},
        {XyOptionsImpl::OPTION_TYPE_BIN   , XyOptionsImpl::OPTION_MODE_RW, void_TextSettings},
        {XyOptionsImpl::OPTION_TYPE_BIN   , XyOptionsImpl::OPTION_MODE_RW, void_SubtitleTiming},
        //{XyOptionsImpl::OPTION_TYPE_BIN   , XyOptionsImpl::OPTION_MODE_RW, void_ZoomRect},
        {XyOptionsImpl::OPTION_TYPE_BIN   , XyOptionsImpl::OPTION_MODE_RW, void_SubtitleReloader},
        {XyOptionsImpl::OPTION_TYPE_INT   , XyOptionsImpl::OPTION_MODE_RW, INT_LOAD_SETTINGS_LEVEL},
        {XyOptionsImpl::OPTION_TYPE_BOOL  , XyOptionsImpl::OPTION_MODE_RW, BOOL_LOAD_SETTINGS_EXTENAL},
        {XyOptionsImpl::OPTION_TYPE_BOOL  , XyOptionsImpl::OPTION_MODE_RW, BOOL_LOAD_SETTINGS_WEB},
        {XyOptionsImpl::OPTION_TYPE_BOOL  , XyOptionsImpl::OPTION_MODE_RW, BOOL_LOAD_SETTINGS_EMBEDDED},
        {XyOptionsImpl::OPTION_TYPE_BOOL  , XyOptionsImpl::OPTION_MODE_RW, BOOL_FORCED_LOAD},
        {XyOptionsImpl::OPTION_TYPE_INT   , XyOptionsImpl::OPTION_MODE_RW, INT_ASPECT_RATIO_SETTINGS},
        //{XyOptionsImpl::OPTION_TYPE_BIN   , XyOptionsImpl::OPTION_MODE_RW, void_HasConfigDialog},
        //{XyOptionsImpl::OPTION_TYPE_BIN   , XyOptionsImpl::OPTION_MODE_RW, void_ShowConfigDialog},
        {XyOptionsImpl::OPTION_TYPE_BIN   , XyOptionsImpl::OPTION_MODE_RW, void_IsSubtitleReloaderLocked},
        {XyOptionsImpl::OPTION_TYPE_BIN   , XyOptionsImpl::OPTION_MODE_RW, void_LockSubtitleReloader},
        //{XyOptionsImpl::OPTION_TYPE_BIN   , XyOptionsImpl::OPTION_MODE_RW, void_AdviseSubClock},
        {XyOptionsImpl::OPTION_TYPE_BOOL  , XyOptionsImpl::OPTION_MODE_RW, BOOL_SUB_FRAME_USE_DST_ALPHA},

        {XyOptionsImpl::OPTION_TYPE_END_FLAG}
    };

    const XyOptionsImpl::Option XyConsumerVobFilterOptions[] = {
        {XyOptionsImpl::OPTION_TYPE_STRING, XyOptionsImpl::OPTION_MODE_READ, STRING_NAME},
        {XyOptionsImpl::OPTION_TYPE_STRING, XyOptionsImpl::OPTION_MODE_READ, STRING_VERSION},

        {XyOptionsImpl::OPTION_TYPE_INT   , XyOptionsImpl::OPTION_MODE_RW, INT_COLOR_SPACE},
        {XyOptionsImpl::OPTION_TYPE_INT   , XyOptionsImpl::OPTION_MODE_RW, INT_YUV_RANGE},
        {XyOptionsImpl::OPTION_TYPE_BOOL  , XyOptionsImpl::OPTION_MODE_RW, BOOL_FOLLOW_UPSTREAM_PREFERRED_ORDER},

        {XyOptionsImpl::OPTION_TYPE_SIZE  , XyOptionsImpl::OPTION_MODE_READ, SIZE_ORIGINAL_VIDEO},
        {XyOptionsImpl::OPTION_TYPE_SIZE  , XyOptionsImpl::OPTION_MODE_RW  , SIZE_AR_ADJUSTED_VIDEO},
        {XyOptionsImpl::OPTION_TYPE_RECT  , XyOptionsImpl::OPTION_MODE_READ, RECT_VIDEO_OUTPUT},
        {XyOptionsImpl::OPTION_TYPE_RECT  , XyOptionsImpl::OPTION_MODE_READ, RECT_SUBTITLE_TARGET},
        {XyOptionsImpl::OPTION_TYPE_ULONGLONG, XyOptionsImpl::OPTION_MODE_READ, ULONGLOG_FRAME_RATE},

        {XyOptionsImpl::OPTION_TYPE_DOUBLE, XyOptionsImpl::OPTION_MODE_READ, DOUBLE_REFRESH_RATE},
        {XyOptionsImpl::OPTION_TYPE_STRING, XyOptionsImpl::OPTION_MODE_READ, STRING_YUV_MATRIX},

        {XyOptionsImpl::OPTION_TYPE_BIN   , XyOptionsImpl::OPTION_MODE_RW, BIN_OUTPUT_COLOR_FORMAT},
        {XyOptionsImpl::OPTION_TYPE_BIN   , XyOptionsImpl::OPTION_MODE_RW, BIN_INPUT_COLOR_FORMAT},

        {XyOptionsImpl::OPTION_TYPE_INT   , XyOptionsImpl::OPTION_MODE_RW, INT_EXTEND_PICTURE_HORIZONTAL},
        {XyOptionsImpl::OPTION_TYPE_INT   , XyOptionsImpl::OPTION_MODE_RW, INT_EXTEND_PICTURE_VERTICAL},
        {XyOptionsImpl::OPTION_TYPE_INT   , XyOptionsImpl::OPTION_MODE_RW, INT_EXTEND_PICTURE_RESX2},
        {XyOptionsImpl::OPTION_TYPE_SIZE  , XyOptionsImpl::OPTION_MODE_RW, SIZE_EXTEND_PICTURE_RESX2MIN},

        {XyOptionsImpl::OPTION_TYPE_BOOL  , XyOptionsImpl::OPTION_MODE_RW, BOOL_FLIP_PICTURE},
        {XyOptionsImpl::OPTION_TYPE_BOOL  , XyOptionsImpl::OPTION_MODE_RW, BOOL_FLIP_SUBTITLE},
        {XyOptionsImpl::OPTION_TYPE_BOOL  , XyOptionsImpl::OPTION_MODE_RW, BOOL_OSD},

        {XyOptionsImpl::OPTION_TYPE_END_FLAG}
    };
};
