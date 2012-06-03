#pragma once

#include "../../../subtitles/STS.h"
#include "../BaseVideoFilter/BaseVideoFilter.h"

#ifdef __cplusplus
extern "C" {
#endif
        
    [uuid("85E5D6F9-BEFB-4E01-B047-758359CDF9AB")]
	interface IDirectVobSubXy : public IUnknown 
    {
        STDMETHOD(get_ColourSpace) (THIS_
                    int* colourSpace
                 ) PURE;

        STDMETHOD(put_ColourSpace) (THIS_
                    int colourSpace
                ) PURE;

        //

        STDMETHOD(get_YuvRange) (THIS_
            int* yuvRange
            ) PURE;

        STDMETHOD(put_YuvRange) (THIS_
            int yuvRange
            ) PURE;

        //

        STDMETHOD(get_OutputColorFormat) (THIS_
                    ColorSpaceId* preferredOrder,
                    bool* fSelected,
                    UINT* count
                 ) PURE;
        STDMETHOD(put_OutputColorFormat) (THIS_
                    const ColorSpaceId* preferredOrder,
                    const bool* fSelected,
                    UINT count
                 ) PURE;
		//

        STDMETHOD(get_InputColorFormat) (THIS_
                    ColorSpaceId* preferredOrder,
                    bool* fSelected,
                    UINT* count
                ) PURE;
        STDMETHOD(put_InputColorFormat) (THIS_
                    const ColorSpaceId* preferredOrder,
                    const bool* fSelected,
                    UINT count
                ) PURE;
        //

        STDMETHOD(get_OverlayCacheMaxItemNum) (THIS_
            int* overlay_cache_max_item_num
            ) PURE;

        STDMETHOD(put_OverlayCacheMaxItemNum) (THIS_
            int overlay_cache_max_item_num
            ) PURE;

        STDMETHOD(get_ScanLineDataCacheMaxItemNum) (THIS_
            int* scan_line_data_cache_max_item_num
            ) PURE;

        STDMETHOD(put_ScanLineDataCacheMaxItemNum) (THIS_
            int scan_line_data_cache_max_item_num
            ) PURE;

        STDMETHOD(get_PathDataCacheMaxItemNum) (THIS_
            int* path_data_cache_max_item_num
            ) PURE;

        STDMETHOD(put_PathDataCacheMaxItemNum) (THIS_
            int path_data_cache_max_item_num
            ) PURE;

        STDMETHOD(get_OverlayNoBlurCacheMaxItemNum) (THIS_
            int* overlay_no_blur_cache_max_item_num
            ) PURE;

        STDMETHOD(put_OverlayNoBlurCacheMaxItemNum) (THIS_
            int overlay_no_blur_cache_max_item_num
            ) PURE;

        struct CachesInfo
        {
            std::size_t path_cache_cur_item_num, path_cache_query_count,path_cache_hit_count,
                scanline_cache_cur_item_num, scanline_cache_query_count,scanline_cache_hit_count,
                non_blur_cache_cur_item_num, non_blur_cache_query_count,non_blur_cache_hit_count,
                overlay_cache_cur_item_num, overlay_cache_query_count,overlay_cache_hit_count,
                interpolate_cache_cur_item_num, interpolate_cache_query_count,interpolate_cache_hit_count,
                text_info_cache_cur_item_num, text_info_cache_query_count, text_info_cache_hit_count,
                word_info_cache_cur_item_num, word_info_cache_query_count, word_info_cache_hit_count;
        };
        STDMETHOD(get_CachesInfo) (THIS_
            CachesInfo* cache_info
            ) PURE;


        STDMETHOD(get_SubpixelPositionLevel) (THIS_
            int* subpixel_pos_level
            ) PURE;

        STDMETHOD(put_SubpixelPositionLevel) (THIS_
            int subpixel_pos_level
            ) PURE;

        //
        STDMETHOD(get_FollowUpstreamPreferredOrder) (THIS_
            bool *fFollowUpstreamPreferredOrder
            ) PURE;

        STDMETHOD(put_FollowUpstreamPreferredOrder) (THIS_
            bool fFollowUpstreamPreferredOrder
            ) PURE;
	};

#ifdef __cplusplus
}
#endif
