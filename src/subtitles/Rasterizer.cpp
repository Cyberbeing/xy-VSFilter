/*
 *	Copyright (C) 2003-2006 Gabest
 *	http://www.gabest.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "stdafx.h"
#include <string.h>
#include <math.h>
#include <vector>
#include <algorithm>
#include "Rasterizer.h"
#include "SeparableFilter.h"
#include "xy_logger.h"
#include <boost/flyweight/key_value.hpp>

#ifndef _MAX	/* avoid collision with common (nonconforming) macros */
#define _MAX	(max)
#define _MIN	(min)
#define _IMPL_MAX max
#define _IMPL_MIN min
#else
#define _IMPL_MAX _MAX
#define _IMPL_MIN _MIN
#endif


//NOTE: signed or unsigned affects the result seriously
#define COMBINE_AYUV(a, y, u, v) ((((((((int)(a))<<8)|y)<<8)|u)<<8)|v)

#define SPLIT_AYUV(color, a, y, u, v) do { \
        *(v)=(color)&0xff; \
        *(u)=((color)>>8) &0xff; \
        *(y)=((color)>>16)&0xff;\
        *(a)=((color)>>24)&0xff;\
    } while(0)
                                    
class ass_synth_priv 
{
public:
    static const int VOLUME_BITS = 22;//should not exceed 32-8, and better not exceed 31-8

    ass_synth_priv(const double sigma);
    ass_synth_priv(const ass_synth_priv& priv);

    ~ass_synth_priv();
    int generate_tables(double sigma);
    
    int g_r;
    int g_w;

    unsigned *g;
    unsigned *gt2;

    double sigma;
};

struct ass_synth_priv_key
{
    const double& operator()(const ass_synth_priv& x)const
    {
        return x.sigma;
    }
};

struct ass_tmp_buf
{
public:
    ass_tmp_buf(size_t size);
    ass_tmp_buf(const ass_tmp_buf& buf);
    ~ass_tmp_buf();
    size_t size;
    unsigned *tmp;
};

struct ass_tmp_buf_get_size
{
    const size_t& operator()(const ass_tmp_buf& buf)const
    {                                              
        return buf.size;
    }
};

static const unsigned int maxcolor = 255;
static const unsigned base = 256;

ass_synth_priv::ass_synth_priv(const double sigma)
{
    g_r = 0;
    g_w = 0;

    g = NULL;
    gt2 = NULL;

    this->sigma = 0;
    generate_tables(sigma);
}

ass_synth_priv::ass_synth_priv(const ass_synth_priv& priv):g_r(priv.g_r),g_w(priv.g_w),sigma(priv.sigma)
{
    if (this->g_w > 0 && this != &priv) {
        this->g = (unsigned*)realloc(this->g, this->g_w * sizeof(unsigned));
        this->gt2 = (unsigned*)realloc(this->gt2, 256 * this->g_w * sizeof(unsigned));
        //if (this->g == null || this->gt2 == null) {
        //    return -1;
        //}
        memcpy(g, priv.g, this->g_w * sizeof(unsigned));
        memcpy(gt2, priv.gt2, 256 * this->g_w * sizeof(unsigned));
    }
}

ass_synth_priv::~ass_synth_priv()
{
    free(g); g=NULL;
    free(gt2); gt2=NULL;    
}

int ass_synth_priv::generate_tables(double sigma)
{
    const int TARGET_VOLUME = 1<<VOLUME_BITS;
    const int MAX_VOLUME_ERROR = VOLUME_BITS>=22 ? 16 : 1;

    double a = -1 / (sigma * sigma * 2);
    double exp_a = exp(a);
    
    double volume_factor = 0;
    double volume_start =  0, volume_end = 0;
    unsigned volume;

    if (this->sigma == sigma)
        return 0;
    else
        this->sigma = sigma;

    this->g_w = (int)ceil(sigma*3) | 1;
    this->g_r = this->g_w / 2;

    if (this->g_w > 0) {
        this->g = (unsigned*)realloc(this->g, this->g_w * sizeof(unsigned));
        this->gt2 = (unsigned*)realloc(this->gt2, 256 * this->g_w * sizeof(unsigned));        
        if (this->g == NULL || this->gt2 == NULL) {                      
            return -1;
        }        
    }

    if (this->g_w > 0) {
        volume_start = 0;        

        double exp_0 = 1.0;
        double exp_1 = exp_a;
        double exp_2 = exp_1 * exp_1;
        volume_start += exp_0;
        for(int i=0;i<this->g_r;++i)
        {
            exp_0 *= exp_1;
            exp_1 *= exp_2;
            volume_start += exp_0;
            volume_start += exp_0;
        }
        //euqivalent:
        //  for (i = 0; i < this->g_w; ++i) {
        //      volume_start += exp(a * (i - this->g_r) * (i - this->g_r));
        //  }
        
        volume_end = (TARGET_VOLUME+g_w)/volume_start; 
        volume_start = (TARGET_VOLUME-g_w)/volume_start;

        volume = 0;
        while( volume_start+0.000001<volume_end )
        {
            volume_factor = (volume_start+volume_end)*0.5;  
            volume = 0;

            exp_0 = volume_factor;
            exp_1 = exp_a;
            exp_2 = exp_1 * exp_1;

            volume = static_cast<int>(exp_0+.5);
            this->g[this->g_r] = volume;

            unsigned* p_left = this->g+this->g_r-1;
            unsigned* p_right= this->g+this->g_r+1;
            for(int i=0; i<this->g_r;++i,p_left--,p_right++)
            {
                exp_0 *= exp_1;
                exp_1 *= exp_2;
                *p_left = static_cast<int>(exp_0+.5);
                *p_right = *p_left;
                volume += (*p_left<<1);
            }
            //equivalent:
            //    for (i = 0; i < this->g_w; ++i) {    
            //        this->g[i] = (unsigned) ( exp(a * (i - this->g_r) * (i - this->g_r))* volume_factor + .5 );
            //        volume += this->g[i];
            //    }

            // volume don't have to be equal to TARGET_VOLUME,
            // even if volume=TARGET_VOLUME+MAX_VOLUME_ERROR,
            // max error introducing in later blur operation,
            // which is (dot_product(g_w, pixel))/TARGET_VOLUME with pixel<256,
            // would not exceed (MAX_VOLUME_ERROR*256)/TARGET_VOLUME,
            // as long as MAX_VOLUME_ERROR/TARGET_VOLUME is small enough, error introduced would be kept in safe range
            // 
            // NOTE: when it comes to rounding, no matter how small the error is, 
            // it may result a different rounding output
            if( volume>=TARGET_VOLUME && volume< (TARGET_VOLUME+MAX_VOLUME_ERROR) )
                break;
            else if(volume < TARGET_VOLUME)
            {
                volume_start = volume_factor;                
            }
            else if(volume >= TARGET_VOLUME+MAX_VOLUME_ERROR)
            {
                volume_end = volume_factor;
            }
        }
        if(volume==0)
        {
            volume_factor = volume_end;

            exp_0 = volume_factor;
            exp_1 = exp_a;
            exp_2 = exp_1 * exp_1;

            volume = static_cast<int>(exp_0+.5);
            this->g[this->g_r] = volume;

            unsigned* p_left = this->g+this->g_r-1;
            unsigned* p_right= this->g+this->g_r+1;
            for(int i=0; i<this->g_r;++i,p_left--,p_right++)
            {
                exp_0 *= exp_1;
                exp_1 *= exp_2;
                *p_left = static_cast<int>(exp_0+.5);
                *p_right = *p_left;
                volume += (*p_left<<1);
            }
            //equivalent:
            //    for (i = 0; i < this->g_w; ++i) {    
            //        this->g[i] = (unsigned) ( exp(a * (i - this->g_r) * (i - this->g_r))* volume_factor + .5 );
            //        volume += this->g[i];
            //    }
        }

        // gauss table:
        for (int mx = 0; mx < this->g_w; mx++) {
            int last_mul = 0;
            unsigned *p_gt2 = this->gt2 + mx;
            *p_gt2 = 0;
            for (int i = 1; i < 256; i++) {                
                last_mul = last_mul+this->g[mx];
                p_gt2 += this->g_w;
                *p_gt2 = last_mul;                
                //equivalent:
                //    this->gt2[this->g_w * i+ mx] = this->g[mx] * i;
            }
        }        
    }
    return 0;
}

ass_tmp_buf::ass_tmp_buf(size_t size)
{
    tmp = (unsigned *)malloc(size * sizeof(unsigned));
    this->size = size;
}

ass_tmp_buf::ass_tmp_buf(const ass_tmp_buf& buf)
    :size(buf.size)
{
    tmp = (unsigned *)malloc(size * sizeof(unsigned));
}

ass_tmp_buf::~ass_tmp_buf()
{
    free(tmp);
}

/*
 * \brief gaussian blur.  an fast pure c implementation from libass.
 */
static void ass_gauss_blur(unsigned char *buffer, unsigned *tmp2,
                           int width, int height, int stride, const unsigned *m2,
                           int r, int mwidth)
{

    int x, y;

    unsigned char *s = buffer;
    unsigned *t = tmp2 + 1;
    for (y = 0; y < height; y++) {
        memset(t - 1, 0, (width + 1) * sizeof(*t));
        x = 0;
        if(x < r)//in case that r < 0
        {            
            const int src = s[x];
            if (src) {
                register unsigned *dstp = t + x - r;
                int mx;
                const unsigned *m3 = m2 + src * mwidth;
                unsigned sum = 0;
                for (mx = mwidth-1; mx >= r - x ; mx--) {                
                    sum += m3[mx];
                    dstp[mx] += sum;
                }
            }
        }

        for (x = 1; x < r; x++) {
            const int src = s[x];
            if (src) {
                register unsigned *dstp = t + x - r;
                int mx;
                const unsigned *m3 = m2 + src * mwidth;
                for (mx = r - x; mx < mwidth; mx++) {
                    dstp[mx] += m3[mx];
                }
            }
        }

        for (; x < width - r; x++) {
            const int src = s[x];
            if (src) {
                register unsigned *dstp = t + x - r;
                int mx;
                const unsigned *m3 = m2 + src * mwidth;
                for (mx = 0; mx < mwidth; mx++) {
                    dstp[mx] += m3[mx];
                }
            }
        }

        for (; x < width-1; x++) {
            const int src = s[x];
            if (src) {
                register unsigned *dstp = t + x - r;
                int mx;
                const int x2 = r + width - x;
                const unsigned *m3 = m2 + src * mwidth;
                for (mx = 0; mx < x2; mx++) {
                    dstp[mx] += m3[mx];
                }
            }
        }
        if(x==width-1) //important: x==width-1 failed, if r==0
        {
            const int src = s[x];
            if (src) {
                register unsigned *dstp = t + x - r;
                int mx;
                const int x2 = r + width - x;
                const unsigned *m3 = m2 + src * mwidth;
                unsigned sum = 0;
                for (mx = 0; mx < x2; mx++) {
                    sum += m3[mx];
                    dstp[mx] += sum;
                }
            }
        }

        s += stride;
        t += width + 1;
    }

    t = tmp2;
    for (x = 0; x < width; x++) {
        y = 0;
        if(y < r)//in case that r<0
        {            
            unsigned *srcp = t + y * (width + 1) + 1;
            int src = *srcp;
            if (src) {
                register unsigned *dstp = srcp - 1 + (mwidth -r +y)*(width + 1);
                const int src2 = (src + (1<<(ass_synth_priv::VOLUME_BITS-1))) >> ass_synth_priv::VOLUME_BITS;
                const unsigned *m3 = m2 + src2 * mwidth;
                unsigned sum = 0;
                int mx;
                *srcp = (1<<(ass_synth_priv::VOLUME_BITS-1));
                for (mx = mwidth-1; mx >=r - y ; mx--) {
                    sum += m3[mx];
                    *dstp += sum;
                    dstp -= width + 1;
                }
            }
        }
        for (y = 1; y < r; y++) {
            unsigned *srcp = t + y * (width + 1) + 1;
            int src = *srcp;
            if (src) {
                register unsigned *dstp = srcp - 1 + width + 1;
                const int src2 = (src + (1<<(ass_synth_priv::VOLUME_BITS-1))) >> ass_synth_priv::VOLUME_BITS;
                const unsigned *m3 = m2 + src2 * mwidth;

                int mx;
                *srcp = (1<<(ass_synth_priv::VOLUME_BITS-1));
                for (mx = r - y; mx < mwidth; mx++) {
                    *dstp += m3[mx];
                    dstp += width + 1;
                }
            }
        }
        for (; y < height - r; y++) {
            unsigned *srcp = t + y * (width + 1) + 1;
            int src = *srcp;
            if (src) {
                register unsigned *dstp = srcp - 1 - r * (width + 1);
                const int src2 = (src + (1<<(ass_synth_priv::VOLUME_BITS-1))) >> ass_synth_priv::VOLUME_BITS;
                const unsigned *m3 = m2 + src2 * mwidth;

                int mx;
                *srcp = (1<<(ass_synth_priv::VOLUME_BITS-1));
                for (mx = 0; mx < mwidth; mx++) {
                    *dstp += m3[mx];
                    dstp += width + 1;
                }
            }
        }
        for (; y < height-1; y++) {
            unsigned *srcp = t + y * (width + 1) + 1;
            int src = *srcp;
            if (src) {
                const int y2 = r + height - y;
                register unsigned *dstp = srcp - 1 - r * (width + 1);
                const int src2 = (src + (1<<(ass_synth_priv::VOLUME_BITS-1))) >> ass_synth_priv::VOLUME_BITS;
                const unsigned *m3 = m2 + src2 * mwidth;

                int mx;
                *srcp = (1<<(ass_synth_priv::VOLUME_BITS-1));
                for (mx = 0; mx < y2; mx++) {
                    *dstp += m3[mx];
                    dstp += width + 1;
                }
            }
        }
        if(y == height - 1)//important: y == height - 1 failed if r==0
        {
            unsigned *srcp = t + y * (width + 1) + 1;
            int src = *srcp;
            if (src) {
                const int y2 = r + height - y;
                register unsigned *dstp = srcp - 1 - r * (width + 1);
                const int src2 = (src + (1<<(ass_synth_priv::VOLUME_BITS-1))) >> ass_synth_priv::VOLUME_BITS;
                const unsigned *m3 = m2 + src2 * mwidth;
                unsigned sum = 0;
                int mx;
                *srcp = (1<<(ass_synth_priv::VOLUME_BITS-1));
                for (mx = 0; mx < y2; mx++) {
                    sum += m3[mx];
                    *dstp += sum;
                    dstp += width + 1;
                }
            }
        }
        t++;
    }

    t = tmp2;
    s = buffer;
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            s[x] = t[x] >> ass_synth_priv::VOLUME_BITS;
        }
        s += stride;
        t += width + 1;
    }
}

/**
 * \brief blur with [[1,2,1]. [2,4,2], [1,2,1]] kernel.
 */
static void be_blur(unsigned char *buf, unsigned *tmp_base, int w, int h, int stride)
{   
    WORD *col_pix_buf_base = reinterpret_cast<WORD*>(xy_malloc(w*sizeof(WORD)));
    WORD *col_sum_buf_base = reinterpret_cast<WORD*>(xy_malloc(w*sizeof(WORD)));
    if(!col_sum_buf_base || !col_pix_buf_base)
    {
        //ToDo: error handling
        return;
    }
    memset(col_pix_buf_base, 0, w*sizeof(WORD));
    memset(col_sum_buf_base, 0, w*sizeof(WORD));

    {
        int y = 1;
        unsigned char *src=buf+y*stride;

        WORD *col_pix_buf = col_pix_buf_base-2;//for aligment;
        WORD *col_sum_buf = col_sum_buf_base-2;//for aligment;

        int x = 2;
        int old_pix = src[x-1];
        int old_sum = old_pix + src[x-2];
        for ( ; x < w; x++) {
            int temp1 = src[x];
            int temp2 = old_pix + temp1;
            old_pix = temp1;
            temp1 = old_sum + temp2;
            old_sum = temp2;

            temp2 = col_pix_buf[x] + temp1;
            col_pix_buf[x] = temp1;
            //dst[x-1] = (col_sum_buf[x] + temp2 + 8) >> 4;
            col_sum_buf[x] = temp2;
        }
    }

    __m128i round = _mm_set1_epi16(8);
    for (int y = 2; y < h; y++) {
        unsigned char *src=buf+y*stride;
        unsigned char *dst=buf+(y-1)*stride;

        WORD *col_pix_buf = col_pix_buf_base-2;//for aligment;
        WORD *col_sum_buf = col_sum_buf_base-2;//for aligment;
                
        int x = 2;
        __m128i old_pix_128 = _mm_cvtsi32_si128(src[1]);
        __m128i old_sum_128 = _mm_cvtsi32_si128(src[0]+src[1]);
        for ( ; x < (w&(~7)); x+=8) {
            __m128i new_pix = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(src+x));
            new_pix = _mm_unpacklo_epi8(new_pix, _mm_setzero_si128());
            __m128i temp = _mm_slli_si128(new_pix,2);
            temp = _mm_add_epi16(temp, old_pix_128);
            temp = _mm_add_epi16(temp, new_pix);
            old_pix_128 = _mm_srli_si128(new_pix,14);

            new_pix = _mm_slli_si128(temp,2);
            new_pix = _mm_add_epi16(new_pix, old_sum_128);
            new_pix = _mm_add_epi16(new_pix, temp);
            old_sum_128 = _mm_srli_si128(temp, 14);

            __m128i old_col_pix = _mm_loadu_si128( reinterpret_cast<const __m128i*>(col_pix_buf+x) );
            __m128i old_col_sum = _mm_loadu_si128( reinterpret_cast<const __m128i*>(col_sum_buf+x) );
            _mm_storeu_si128( reinterpret_cast<__m128i*>(col_pix_buf+x), new_pix );
            temp = _mm_add_epi16(new_pix, old_col_pix);
            _mm_storeu_si128( reinterpret_cast<__m128i*>(col_sum_buf+x), temp );

            old_col_sum = _mm_add_epi16(old_col_sum, temp);
            old_col_sum = _mm_add_epi16(old_col_sum, round);
            old_col_sum = _mm_srli_epi16(old_col_sum, 4);
            old_col_sum = _mm_packus_epi16(old_col_sum, old_col_sum);
            _mm_storel_epi64( reinterpret_cast<__m128i*>(dst+x-1), old_col_sum );
        }
        int old_pix = src[x-1];
        int old_sum = old_pix + src[x-2];
        for ( ; x < w; x++) {
            int temp1 = src[x];
            int temp2 = old_pix + temp1;
            old_pix = temp1;
            temp1 = old_sum + temp2;
            old_sum = temp2;

            temp2 = col_pix_buf[x] + temp1;
            col_pix_buf[x] = temp1;
            dst[x-1] = (col_sum_buf[x] + temp2 + 8) >> 4;
            col_sum_buf[x] = temp2;
        }
    }
}

bool Rasterizer::Rasterize(const ScanLineData& scan_line_data, int xsub, int ysub, SharedPtrOverlay overlay)
{
    using namespace ::boost::flyweights;

    if(!overlay)
    {
        return false;
    }
    overlay->CleanUp();

    if(!scan_line_data.mWidth || !scan_line_data.mHeight)
    {
        return true;
    }
    xsub &= 7;
    ysub &= 7;
    //xsub = ysub = 0;
    int width = scan_line_data.mWidth + xsub;
    int height = scan_line_data.mHeight + ysub;
    overlay->mOffsetX = scan_line_data.mPathOffsetX - xsub;
    overlay->mOffsetY = scan_line_data.mPathOffsetY - ysub;
    int wide_border = (scan_line_data.mWideBorder+7)&~7;
    overlay->mfWideOutlineEmpty = scan_line_data.mWideOutline.empty();
    if(!overlay->mfWideOutlineEmpty)
    {
        width += 2*wide_border ;
        height += 2*wide_border ;
        xsub += wide_border ;
        ysub += wide_border ;
        overlay->mOffsetX -= wide_border;
        overlay->mOffsetY -= wide_border;
    }

    overlay->mOverlayWidth = ((width+7)>>3) + 1;
    overlay->mOverlayHeight = ((height+7)>>3) + 1;
    overlay->mOverlayPitch = (overlay->mOverlayWidth+15)&~15;

    overlay->mpOverlayBuffer.base = (byte*)xy_malloc(2 * overlay->mOverlayPitch * overlay->mOverlayHeight);
    memset(overlay->mpOverlayBuffer.base, 0, 2 * overlay->mOverlayPitch * overlay->mOverlayHeight);
    overlay->mpOverlayBuffer.body = overlay->mpOverlayBuffer.base;
    overlay->mpOverlayBuffer.border = overlay->mpOverlayBuffer.base + overlay->mOverlayPitch * overlay->mOverlayHeight;        

    // Are we doing a border?
    const ScanLineData::tSpanBuffer* pOutline[2] = {&(scan_line_data.mOutline), &(scan_line_data.mWideOutline)};
    for(int i = countof(pOutline)-1; i >= 0; i--)
    {
        ScanLineData::tSpanBuffer::const_iterator it = pOutline[i]->begin();
        ScanLineData::tSpanBuffer::const_iterator itEnd = pOutline[i]->end();
        byte* plan_selected = i==0 ? overlay->mpOverlayBuffer.body : overlay->mpOverlayBuffer.border;
        int pitch = overlay->mOverlayPitch;
        for(; it!=itEnd; ++it)
        {
            int y = (int)(((*it).first >> 32) - 0x40000000 + ysub);
            int x1 = (int)(((*it).first & 0xffffffff) - 0x40000000 + xsub);
            int x2 = (int)(((*it).second & 0xffffffff) - 0x40000000 + xsub);
            if(x2 > x1)
            {
                int first = x1>>3;
                int last = (x2-1)>>3;
                byte* dst = plan_selected + (pitch*(y>>3) + first);
                if(first == last)
                    *dst += x2-x1;
                else
                {
                    *dst += ((first+1)<<3) - x1;
                    dst += 1;
                    while(++first < last)
                    {
                        *dst += 0x08;
                        dst += 1;
                    }
                    *dst += x2 - (last<<3);
                }
            }
        }
    }

    return true;
}

// @return: true if actually a blur operation has done, or else false and output is leave unset.
bool Rasterizer::Blur(const Overlay& input_overlay, int fBlur, double fGaussianBlur, 
    SharedPtrOverlay output_overlay)
{
    using namespace ::boost::flyweights;

    if(!output_overlay)
    {
        return false;
    }
    output_overlay->CleanUp();

    output_overlay->mOffsetX = input_overlay.mOffsetX;
    output_overlay->mOffsetY = input_overlay.mOffsetY;
    output_overlay->mOverlayWidth = input_overlay.mOverlayWidth;
    output_overlay->mOverlayHeight = input_overlay.mOverlayHeight;
    output_overlay->mfWideOutlineEmpty = input_overlay.mfWideOutlineEmpty;

    int bluradjust = 0;
    if(fBlur || fGaussianBlur > 0.1)
    {
        if (fGaussianBlur > 0)
            bluradjust += (int)(fGaussianBlur*3*8 + 0.5) | 1;
        if (fBlur)
            bluradjust += 8;
        // Expand the buffer a bit when we're blurring, since that can also widen the borders a bit
        bluradjust = (bluradjust+7)&~7;

        output_overlay->mOffsetX -= bluradjust;
        output_overlay->mOffsetY -= bluradjust;
        output_overlay->mOverlayWidth += (bluradjust>>2);
        output_overlay->mOverlayHeight += (bluradjust>>2);
    }
    else
    {
        return false;
    }

    output_overlay->mOverlayPitch = (output_overlay->mOverlayWidth+15)&~15;

    output_overlay->mpOverlayBuffer.base = (byte*)xy_malloc(2 * output_overlay->mOverlayPitch * output_overlay->mOverlayHeight);
    memset(output_overlay->mpOverlayBuffer.base, 0, 2 * output_overlay->mOverlayPitch * output_overlay->mOverlayHeight);
    output_overlay->mpOverlayBuffer.body = output_overlay->mpOverlayBuffer.base;
    output_overlay->mpOverlayBuffer.border = output_overlay->mpOverlayBuffer.base + output_overlay->mOverlayPitch * output_overlay->mOverlayHeight;        

    //copy buffer
    for(int i = 1; i >= 0; i--)
    {
        byte* plan_selected = i==0 ? output_overlay->mpOverlayBuffer.body : output_overlay->mpOverlayBuffer.border;
        const byte* plan_input = i==0 ? input_overlay.mpOverlayBuffer.body : input_overlay.mpOverlayBuffer.border;

        plan_selected += (bluradjust>>3) + (bluradjust>>3)*output_overlay->mOverlayPitch;
        for (int j=0;j<input_overlay.mOverlayHeight;j++)
        {
            memcpy(plan_selected, plan_input, input_overlay.mOverlayPitch);
            plan_selected += output_overlay->mOverlayPitch;
            plan_input += input_overlay.mOverlayPitch;
        }
    }

    ass_tmp_buf tmp_buf( max((output_overlay->mOverlayPitch+1)*(output_overlay->mOverlayHeight+1),0) );        
    //flyweight<key_value<int, ass_tmp_buf, ass_tmp_buf_get_size>, no_locking> tmp_buf((overlay->mOverlayWidth+1)*(overlay->mOverlayPitch+1));
    // Do some gaussian blur magic    
    if (fGaussianBlur > 0.1)//(fGaussianBlur > 0) return true even if fGaussianBlur very small
    {
        byte* plan_selected= output_overlay->mfWideOutlineEmpty ? output_overlay->mpOverlayBuffer.body : output_overlay->mpOverlayBuffer.border;
        flyweight<key_value<double, ass_synth_priv, ass_synth_priv_key>, no_locking> fw_priv_blur(fGaussianBlur);
        const ass_synth_priv& priv_blur = fw_priv_blur.get();
        if (output_overlay->mOverlayWidth>=priv_blur.g_w && output_overlay->mOverlayHeight>=priv_blur.g_w)
        {   
            ass_gauss_blur(plan_selected, tmp_buf.tmp, output_overlay->mOverlayWidth, output_overlay->mOverlayHeight, output_overlay->mOverlayPitch, 
                priv_blur.gt2, priv_blur.g_r, priv_blur.g_w);
        }
    }

    for (int pass = 0; pass < fBlur; pass++)
    {
        if(output_overlay->mOverlayWidth >= 3 && output_overlay->mOverlayHeight >= 3)
        {            
            int pitch = output_overlay->mOverlayPitch;
            byte* plan_selected= output_overlay->mfWideOutlineEmpty ? output_overlay->mpOverlayBuffer.body : output_overlay->mpOverlayBuffer.border;
            be_blur(plan_selected, tmp_buf.tmp, output_overlay->mOverlayWidth, output_overlay->mOverlayHeight, pitch);
        }
    }
    return true;
}

///////////////////////////////////////////////////////////////////////////

static __forceinline void pixmix(DWORD *dst, DWORD color, DWORD alpha)
{
    int a = (((alpha)*(color>>24))>>6)&0xff;
    // Make sure both a and ia are in range 1..256 for the >>8 operations below to be correct
    int ia = 256-a;
    a+=1;
    *dst = ((((*dst&0x00ff00ff)*ia + (color&0x00ff00ff)*a)&0xff00ff00)>>8)
           | ((((*dst&0x0000ff00)*ia + (color&0x0000ff00)*a)&0x00ff0000)>>8)
           | ((((*dst>>8)&0x00ff0000)*ia)&0xff000000);
}

static __forceinline void pixmix2(DWORD *dst, DWORD color, DWORD shapealpha, DWORD clipalpha)
{
    int a = (((shapealpha)*(clipalpha)*(color>>24))>>12)&0xff;
    int ia = 256-a;
    a+=1;
    *dst = ((((*dst&0x00ff00ff)*ia + (color&0x00ff00ff)*a)&0xff00ff00)>>8)
           | ((((*dst&0x0000ff00)*ia + (color&0x0000ff00)*a)&0x00ff0000)>>8)
           | ((((*dst>>8)&0x00ff0000)*ia)&0xff000000);
}

#include <xmmintrin.h>
#include <emmintrin.h>

static __forceinline void pixmix_sse2(DWORD* dst, DWORD color, DWORD alpha)
{
//    alpha = (((alpha) * (color>>24)) >> 6) & 0xff;
    color &= 0xffffff;
    __m128i zero = _mm_setzero_si128();
    __m128i a = _mm_set1_epi32(((alpha+1) << 16) | (0x100 - alpha));
    __m128i d = _mm_unpacklo_epi8(_mm_cvtsi32_si128(*dst), zero);
    __m128i s = _mm_unpacklo_epi8(_mm_cvtsi32_si128(color), zero);
    __m128i r = _mm_unpacklo_epi16(d, s);
    r = _mm_madd_epi16(r, a);
    r = _mm_srli_epi32(r, 8);
    r = _mm_packs_epi32(r, r);
    r = _mm_packus_epi16(r, r);
    *dst = (DWORD)_mm_cvtsi128_si32(r);
}

static __forceinline void pixmix2_sse2(DWORD* dst, DWORD color, DWORD shapealpha, DWORD clipalpha)
{
    int alpha = (((shapealpha)*(clipalpha)*(color>>24))>>12)&0xff;
    color &= 0xffffff;
    __m128i zero = _mm_setzero_si128();
    __m128i a = _mm_set1_epi32(((alpha+1) << 16) | (0x100 - alpha));
    __m128i d = _mm_unpacklo_epi8(_mm_cvtsi32_si128(*dst), zero);
    __m128i s = _mm_unpacklo_epi8(_mm_cvtsi32_si128(color), zero);
    __m128i r = _mm_unpacklo_epi16(d, s);
    r = _mm_madd_epi16(r, a);
    r = _mm_srli_epi32(r, 8);
    r = _mm_packs_epi32(r, r);
    r = _mm_packus_epi16(r, r);
    *dst = (DWORD)_mm_cvtsi128_si32(r);
}

#include <mmintrin.h>

// Calculate a - b clamping to 0 instead of underflowing
static __forceinline DWORD safe_subtract(DWORD a, DWORD b)
{
    __m64 ap = _mm_cvtsi32_si64(a);
    __m64 bp = _mm_cvtsi32_si64(b);
    __m64 rp = _mm_subs_pu16(ap, bp);
    DWORD r = (DWORD)_mm_cvtsi64_si32(rp);
    _mm_empty();
    return r;
    //return (b > a) ? 0 : a - b;
}

/***
 * No aligned requirement
 * 
 **/
void AlphaBlt(byte* pY,
    const byte* pAlphaMask, 
    const byte Y, 
    int h, int w, int src_stride, int dst_stride)
{   
    __m128i zero = _mm_setzero_si128();
    __m128i s = _mm_set1_epi16(Y);               //s = c  0  c  0  c  0  c  0  c  0  c  0  c  0  c  0    

    for( ; h>0; h--, pAlphaMask += src_stride, pY += dst_stride )
    {
        const BYTE* sa = pAlphaMask;      
        BYTE* dy = pY;
        const BYTE* dy_first_mod16 = reinterpret_cast<BYTE*>((reinterpret_cast<int>(pY)+15)&~15);
        const BYTE* dy_end_mod16 = reinterpret_cast<BYTE*>(reinterpret_cast<int>(pY+w)&~15);
        const BYTE* dy_end = pY + w;   

        for(;dy < dy_first_mod16; sa++, dy++)
        {
            *dy = (*dy * (256 - *sa)+ Y*(*sa+1))>>8;
        }
        for(; dy < dy_end_mod16; sa+=8, dy+=16)
        {
            __m128i a = _mm_loadl_epi64((__m128i*)sa);

            //Y
            __m128i d = _mm_load_si128((__m128i*)dy);

            //__m128i ones = _mm_cmpeq_epi32(zero,zero); //ones = ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
            //__m128i ia = _mm_xor_si128(a,ones);        //ia   = ~a
            //ia = _mm_unpacklo_epi8(ia,zero);           //ia   = ~a0 0 ~a1 0 ~a2 0 ~a3 0 ~a4 0 ~a5 0 ~a6 0 ~a7 0
            a = _mm_unpacklo_epi8(a,zero);               //a= a0 0  a1 0  a2 0  a3 0  a4 0  a5 0  a6 0  a7 0            
            __m128i ones = _mm_set1_epi16(256);          //ones = 0  1  0  1  0  1  0  1  0  1  0  1  0  1  0  1
            __m128i ia = _mm_sub_epi16(ones, a);         //ia   = 256-a0 ... 256-a7
            ones = _mm_srli_epi16(ones, 8);
            a = _mm_add_epi16(a, ones);                  //a= 1+a0 ... 1+a7

            __m128i dl = _mm_unpacklo_epi8(d,zero);               //d    = b0 0  b1 0  b2 0  b3 0  b4 0  b5 0  b6 0  b7 0 
            __m128i sl = _mm_mullo_epi16(s,a);            //sl   = c0*a0  c1*a1  ... c7*a7
            
            dl = _mm_mullo_epi16(dl,ia);                   //d    = b0*~a0 b1*~a1 ... b7*~a7

            dl = _mm_add_epi16(dl,sl);                     //d   = d + sl
            dl = _mm_srli_epi16(dl, 8);                    //d   = d>>8
            
            sa += 8;
            a = _mm_loadl_epi64((__m128i*)sa);

            a = _mm_unpacklo_epi8(a,zero);            
            ones = _mm_slli_epi16(ones, 8);
            ia = _mm_sub_epi16(ones, a);
            ones = _mm_srli_epi16(ones, 8);
            a = _mm_add_epi16(a,ones);

            d = _mm_unpackhi_epi8(d,zero);
            sl = _mm_mullo_epi16(s,a);
            d = _mm_mullo_epi16(d,ia);
            d = _mm_add_epi16(d,sl);
            d = _mm_srli_epi16(d, 8);

            dl = _mm_packus_epi16(dl,d);

            _mm_store_si128((__m128i*)dy, dl);
        }
        for(;dy < dy_end; sa++, dy++)
        {
            *dy = (*dy * (256 - *sa)+ Y*(*sa+1))>>8;
        }
    }
    //__asm emms;
}

// For CPUID usage in Rasterizer::Draw
#include "../dsutil/vd.h"

static const __int64 _00ff00ff00ff00ff = 0x00ff00ff00ff00ffi64;

// Render a subpicture onto a surface.
// spd is the surface to render on.
// clipRect is a rectangular clip region to render inside.
// pAlphaMask is an alpha clipping mask.
// xsub and ysub ???
// switchpts seems to be an array of fill colours interlaced with coordinates.
//    switchpts[i*2] contains a colour and switchpts[i*2+1] contains the coordinate to use that colour from
// fBody tells whether to render the body of the subs.
// fBorder tells whether to render the border of the subs.
CRect Rasterizer::Draw(SubPicDesc& spd, SharedPtrOverlay overlay, CRect& clipRect, byte* pAlphaMask, 
    int xsub, int ysub, const DWORD* switchpts, bool fBody, bool fBorder)
{
    CRect bbox(0, 0, 0, 0);
    if(!switchpts || !fBody && !fBorder) return(bbox);

    // clip
    // Limit drawn area to intersection of rendering surface and rectangular clip area
    CRect r(0, 0, spd.w, spd.h);
    r &= clipRect;
    // Remember that all subtitle coordinates are specified in 1/8 pixels
    // (x+4)>>3 rounds to nearest whole pixel.
    // ??? What is xsub, ysub, mOffsetX and mOffsetY ?
    int overlayPitch = overlay->mOverlayPitch;
    int x = (xsub + overlay->mOffsetX + 4)>>3;
    int y = (ysub + overlay->mOffsetY + 4)>>3;
    int w = overlay->mOverlayWidth;
    int h = overlay->mOverlayHeight;
    int xo = 0, yo = 0;
    // Again, limiting?
    if(x < r.left) {xo = r.left-x; w -= r.left-x; x = r.left;}
    if(y < r.top) {yo = r.top-y; h -= r.top-y; y = r.top;}
    if(x+w > r.right) w = r.right-x;
    if(y+h > r.bottom) h = r.bottom-y;
    // Check if there's actually anything to render
    if(w <= 0 || h <= 0) return(bbox);
    bbox.SetRect(x, y, x+w, y+h);
    bbox &= CRect(0, 0, spd.w, spd.h);
    
    struct DM
    {
        enum
        {
            SSE2 = 1,
            ALPHA_MASK = 1<<1,
            SINGLE_COLOR = 1<<2,
            BODY = 1<<3,
            YV12 = 1<<4
        };
    };
    // CPUID from VDub
    bool fSSE2 = !!(g_cpuid.m_flags & CCpuID::sse2);
    bool fSingleColor = (switchpts[1]==0xffffffff);
    bool fYV12 = (spd.type==MSP_AY11);
    int draw_method = 0;
    //	if(pAlphaMask)
    //		draw_method |= DM::ALPHA_MASK;
    if(fSingleColor)
        draw_method |= DM::SINGLE_COLOR;
    //	if(fBody)
    //		draw_method |= DM::BODY;
    if(fSSE2)
        draw_method |= DM::SSE2;
    if(fYV12)
        draw_method |= DM::YV12;
    
    // draw
    // Grab the first colour
    DWORD color = switchpts[0];
    byte* s_base = (byte*)xy_malloc(overlay->mOverlayPitch * overlay->mOverlayHeight);
        
    if(fSingleColor)
    {
        overlay->FillAlphaMash(s_base, fBody, fBorder, xo, yo, w, h, 
            pAlphaMask==NULL ? NULL : pAlphaMask + spd.w * y + x, spd.w,
            color>>24 );
    }
    else
    {
        int last_x = xo;
        const DWORD *sw = switchpts;
        while( last_x<w+xo )
        {   
            byte alpha = sw[0]>>24; 
            while( sw[3]<w+xo && (sw[2]>>24)==alpha )
            {
                sw += 2;
            }
            int new_x = sw[3] < w+xo ? sw[3] : w+xo;
            overlay->FillAlphaMash(s_base, fBody, fBorder, 
                last_x, yo, new_x-last_x, h, 
                pAlphaMask==NULL ? NULL : pAlphaMask + spd.w * y + x + last_x - xo, spd.w,
                alpha );   
            last_x = new_x;
            sw += 2;
        }
    }

    const byte* s = s_base + overlay->mOverlayPitch*yo + xo;

    // How would this differ from src?
    unsigned long* dst = (unsigned long *)(((char *)spd.bits + spd.pitch * y) + ((x*spd.bpp)>>3));

    // Every remaining line in the bitmap to be rendered...
    switch(draw_method)
    {
    case   DM::SINGLE_COLOR |   DM::SSE2 | 0*DM::YV12 :
    {
        while(h--)
        {
            for(int wt=0; wt<w; ++wt)
                // The <<6 is due to pixmix expecting the alpha parameter to be
                // the multiplication of two 6-bit unsigned numbers but we
                // only have one here. (No alpha mask.)
                pixmix_sse2(&dst[wt], color, s[wt]);
            s += overlayPitch;
            dst = (unsigned long *)((char *)dst + spd.pitch);
        }
    }
    break;
    case   DM::SINGLE_COLOR | 0*DM::SSE2 | 0*DM::YV12 :
    {
        while(h--)
        {
            for(int wt=0; wt<w; ++wt)
                pixmix(&dst[wt], color, s[wt]);
            s += overlayPitch;
            dst = (unsigned long *)((char *)dst + spd.pitch);
        }
    }
    break;
    case 0*DM::SINGLE_COLOR |   DM::SSE2 | 0*DM::YV12 :
    {
        while(h--)
        {
            const DWORD *sw = switchpts;
            for(int wt=0; wt<w; ++wt)
            {
                // xo is the offset (usually negative) we have moved into the image
                // So if we have passed the switchpoint (?) switch to another colour
                // (So switchpts stores both colours *and* coordinates?)
                if(wt+xo >= sw[1]) {while(wt+xo >= sw[1]) sw += 2; color = sw[-2];}
                pixmix_sse2(&dst[wt], color, (s[wt]*(color>>24))>>8);
            }
            s += overlayPitch;
            dst = (unsigned long *)((char *)dst + spd.pitch);
        }
    }
    break;
    case 0*DM::SINGLE_COLOR | 0*DM::SSE2 | 0*DM::YV12 :
    {
        while(h--)
        {
            const DWORD *sw = switchpts;
            for(int wt=0; wt<w; ++wt)
            {
                if(wt+xo >= sw[1]) {while(wt+xo >= sw[1]) sw += 2; color = sw[-2];}
                pixmix(&dst[wt], color, (s[wt]*(color>>24))>>8);
            }
            s += overlayPitch;
            dst = (unsigned long *)((char *)dst + spd.pitch);
        }
    }
    break;
    case   DM::SINGLE_COLOR |   DM::SSE2 |   DM::YV12 :
    {
        unsigned char* dst_A = (unsigned char*)dst;
        unsigned char* dst_Y = dst_A + spd.pitch*spd.h;
        unsigned char* dst_U = dst_Y + spd.pitch*spd.h;
        unsigned char* dst_V = dst_U + spd.pitch*spd.h;

        AlphaBlt(dst_Y, s, ((color)>>16)&0xff, h, w, overlayPitch, spd.pitch);
        AlphaBlt(dst_U, s, ((color)>>8)&0xff, h, w, overlayPitch, spd.pitch);
        AlphaBlt(dst_V, s, ((color))&0xff, h, w, overlayPitch, spd.pitch);
        AlphaBlt(dst_A, s, 0, h, w, overlayPitch, spd.pitch);
    }
    break;
    case 0*DM::SINGLE_COLOR |   DM::SSE2 |   DM::YV12 :
    {
        unsigned char* dst_A = (unsigned char*)dst;
        unsigned char* dst_Y = dst_A + spd.pitch*spd.h;
        unsigned char* dst_U = dst_Y + spd.pitch*spd.h;
        unsigned char* dst_V = dst_U + spd.pitch*spd.h;

        const DWORD *sw = switchpts;
        int last_x = xo;
        color = sw[0];
        while(last_x<w+xo)
        {
            int new_x = sw[3] < w+xo ? sw[3] : w+xo;
            color = sw[0];
            sw += 2;
            AlphaBlt(dst_Y, s + last_x - xo, (color>>16)&0xff, h, new_x-last_x, overlayPitch, spd.pitch);
            AlphaBlt(dst_U, s + last_x - xo, (color>>8)&0xff, h, new_x-last_x, overlayPitch, spd.pitch);
            AlphaBlt(dst_V, s + last_x - xo, (color)&0xff, h, new_x-last_x, overlayPitch, spd.pitch);
            AlphaBlt(dst_A, s + last_x - xo, 0, h, new_x-last_x, overlayPitch, spd.pitch);

            dst_A += new_x - last_x;
            dst_Y += new_x - last_x;
            dst_U += new_x - last_x;
            dst_V += new_x - last_x;
            last_x = new_x;
        }
    }
    break;
    case   DM::SINGLE_COLOR | 0*DM::SSE2 |   DM::YV12 :
    {
//        char * debug_dst=(char*)dst;int h2 = h;
//        XY_DO_ONCE( xy_logger::write_file("G:\\b2_rt", (char*)&color, sizeof(color)) );
//        XY_DO_ONCE( xy_logger::write_file("G:\\b2_rt", debug_dst, (h2-1)*spd.pitch) );
//        debug_dst += spd.pitch*spd.h;
//        XY_DO_ONCE( xy_logger::write_file("G:\\b2_rt", debug_dst, (h2-1)*spd.pitch) );
//        debug_dst += spd.pitch*spd.h;
//        XY_DO_ONCE( xy_logger::write_file("G:\\b2_rt", debug_dst, (h2-1)*spd.pitch) );
//        debug_dst += spd.pitch*spd.h;
//        XY_DO_ONCE( xy_logger::write_file("G:\\b2_rt", debug_dst, (h2-1)*spd.pitch) );
//        debug_dst=(char*)dst;

        unsigned char* dst_A = (unsigned char*)dst;
        unsigned char* dst_Y = dst_A + spd.pitch*spd.h;
        unsigned char* dst_U = dst_Y + spd.pitch*spd.h;
        unsigned char* dst_V = dst_U + spd.pitch*spd.h;
        while(h--)
        {
            for(int wt=0; wt<w; ++wt)
            {
                DWORD temp = COMBINE_AYUV(dst_A[wt], dst_Y[wt], dst_U[wt], dst_V[wt]);
                pixmix(&temp, color, s[wt]);
                SPLIT_AYUV(temp, dst_A+wt, dst_Y+wt, dst_U+wt, dst_V+wt);
            }
            s += overlayPitch;
            dst_A += spd.pitch;
            dst_Y += spd.pitch;
            dst_U += spd.pitch;
            dst_V += spd.pitch;
        }
//        XY_DO_ONCE( xy_logger::write_file("G:\\a2_rt", debug_dst, (h2-1)*spd.pitch) );
//        debug_dst += spd.pitch*spd.h;
//        XY_DO_ONCE( xy_logger::write_file("G:\\a2_rt", debug_dst, (h2-1)*spd.pitch) );
//        debug_dst += spd.pitch*spd.h;
//        XY_DO_ONCE( xy_logger::write_file("G:\\a2_rt", debug_dst, (h2-1)*spd.pitch) );
//        debug_dst += spd.pitch*spd.h;
//        XY_DO_ONCE( xy_logger::write_file("G:\\a2_rt", debug_dst, (h2-1)*spd.pitch) );
    }
    break;
    case 0*DM::SINGLE_COLOR | 0*DM::SSE2 |   DM::YV12 :
    {
        unsigned char* dst_A = (unsigned char*)dst;
        unsigned char* dst_Y = dst_A + spd.pitch*spd.h;
        unsigned char* dst_U = dst_Y + spd.pitch*spd.h;
        unsigned char* dst_V = dst_U + spd.pitch*spd.h;
        while(h--)
        {
            const DWORD *sw = switchpts;
            for(int wt=0; wt<w; ++wt)
            {
                if(wt+xo >= sw[1]) {while(wt+xo >= sw[1]) sw += 2; color = sw[-2];}
                DWORD temp = COMBINE_AYUV(dst_A[wt], dst_Y[wt], dst_U[wt], dst_V[wt]);
                pixmix(&temp, color, (s[wt]*(color>>24))>>8);
                SPLIT_AYUV(temp, dst_A+wt, dst_Y+wt, dst_U+wt, dst_V+wt);
            }
            s += overlayPitch;
            dst_A += spd.pitch;
            dst_Y += spd.pitch;
            dst_U += spd.pitch;
            dst_V += spd.pitch;
        }
    }
    break;
    }
    // Remember to EMMS!
    // Rendering fails in funny ways if we don't do this.
    _mm_empty();
    xy_free(s_base);
    return bbox;
}

///////////////////////////////////////////////////////////////

// Overlay

void Overlay::_DoFillAlphaMash(byte* outputAlphaMask, const byte* pBody, const byte* pBorder, int x, int y, int w, int h, const byte* pAlphaMask, int pitch, DWORD color_alpha )
{
    pBody = pBody!=NULL ? pBody + y*mOverlayPitch + x: NULL;
    pBorder = pBorder!=NULL ? pBorder + y*mOverlayPitch + x: NULL;
    byte* dst = outputAlphaMask + y*mOverlayPitch + x;

    const int x0 = ((reinterpret_cast<int>(dst)+3)&~3) - reinterpret_cast<int>(dst);
    const int x00 = ((reinterpret_cast<int>(dst)+15)&~15) - reinterpret_cast<int>(dst);
    const int x_end00  = ((reinterpret_cast<int>(dst)+w)&~15) - reinterpret_cast<int>(dst);
    const int x_end0 = ((reinterpret_cast<int>(dst)+w)&~3) - reinterpret_cast<int>(dst);
    const int x_end = w;

    __m64 color_alpha_64 = _mm_set1_pi16(color_alpha);
    __m128i color_alpha_128 = _mm_set1_epi16(color_alpha);

    if(pAlphaMask==NULL && pBody!=NULL && pBorder!=NULL)
    {       
        /*
        __asm
        {
        mov        eax, color_alpha
        movd	   XMM3, eax
        punpcklwd  XMM3, XMM3
        pshufd	   XMM3, XMM3, 0
        } 
        */
        while(h--)
        {
            int j=0;
            for( ; j<x0; j++ )
            {
                int temp = pBorder[j]-pBody[j];
                temp = temp<0 ? 0 : temp;
                dst[j] = (temp * color_alpha)>>6;
            }
            for( ;j<x00;j+=4 )
            {
                __m64 border = _mm_cvtsi32_si64(*reinterpret_cast<const int*>(pBorder+j));
                __m64 body = _mm_cvtsi32_si64(*reinterpret_cast<const int*>(pBody+j));
                border = _mm_subs_pu8(border, body);                    
                __m64 zero = _mm_setzero_si64();
                border = _mm_unpacklo_pi8(border, zero);
                border = _mm_mullo_pi16(border, color_alpha_64);
                border = _mm_srli_pi16(border, 6);
                border = _mm_packs_pu16(border,border);
                *reinterpret_cast<int*>(dst+j) = _mm_cvtsi64_si32(border);
            }
            __m128i zero = _mm_setzero_si128();
            for( ;j<x_end00;j+=16)
            {
                __m128i border = _mm_loadu_si128(reinterpret_cast<const __m128i*>(pBorder+j));
                __m128i body = _mm_loadu_si128(reinterpret_cast<const __m128i*>(pBody+j));
                border = _mm_subs_epu8(border,body);
                __m128i srchi = border;   
                border = _mm_unpacklo_epi8(border, zero);
                srchi = _mm_unpackhi_epi8(srchi, zero);
                border = _mm_mullo_epi16(border, color_alpha_128);
                srchi = _mm_mullo_epi16(srchi, color_alpha_128);
                border = _mm_srli_epi16(border, 6);
                srchi = _mm_srli_epi16(srchi, 6);
                border = _mm_packus_epi16(border, srchi);
                _mm_storeu_si128(reinterpret_cast<__m128i*>(dst+j), border);
            }
            for( ;j<x_end0;j+=4)
            {
                __m64 border = _mm_cvtsi32_si64(*reinterpret_cast<const int*>(pBorder+j));
                __m64 body = _mm_cvtsi32_si64(*reinterpret_cast<const int*>(pBody+j));
                border = _mm_subs_pu8(border, body);                    
                __m64 zero = _mm_setzero_si64();
                border = _mm_unpacklo_pi8(border, zero);
                border = _mm_mullo_pi16(border, color_alpha_64);
                border = _mm_srli_pi16(border, 6);
                border = _mm_packs_pu16(border,border);
                *reinterpret_cast<int*>(dst+j) = _mm_cvtsi64_si32(border);
            }
            for( ;j<x_end;j++)
            {
                int temp = pBorder[j]-pBody[j];
                temp = temp<0 ? 0 : temp;
                dst[j] = (temp * color_alpha)>>6;
            }
            pBody += mOverlayPitch;
            pBorder += mOverlayPitch;
            //pAlphaMask += pitch;
            dst += mOverlayPitch;
        }
    }
    else if( ((pBody==NULL) + (pBorder==NULL))==1 && pAlphaMask==NULL)
    {
        const BYTE* src1 = pBody!=NULL ? pBody : pBorder;
        while(h--)
        {
            int j=0;
            for( ; j<x0; j++ )
            {
                dst[j] = (src1[j] * color_alpha)>>6;
            }
            for( ;j<x00;j+=4 )
            {
                __m64 src = _mm_cvtsi32_si64(*reinterpret_cast<const int*>(src1+j));
                __m64 zero = _mm_setzero_si64();
                src = _mm_unpacklo_pi8(src, zero);
                src = _mm_mullo_pi16(src, color_alpha_64);
                src = _mm_srli_pi16(src, 6);
                src = _mm_packs_pu16(src,src);
                *reinterpret_cast<int*>(dst+j) = _mm_cvtsi64_si32(src);
            }
            __m128i zero = _mm_setzero_si128();
            for( ;j<x_end00;j+=16)
            {
                __m128i src = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src1+j));
                __m128i srchi = src;
                src = _mm_unpacklo_epi8(src, zero);
                srchi = _mm_unpackhi_epi8(srchi, zero);
                src = _mm_mullo_epi16(src, color_alpha_128);
                srchi = _mm_mullo_epi16(srchi, color_alpha_128);
                src = _mm_srli_epi16(src, 6);
                srchi = _mm_srli_epi16(srchi, 6);
                src = _mm_packus_epi16(src, srchi);
                _mm_storeu_si128(reinterpret_cast<__m128i*>(dst+j), src);
            }
            for( ;j<x_end0;j+=4)
            {
                __m64 src = _mm_cvtsi32_si64(*reinterpret_cast<const int*>(src1+j));
                __m64 zero = _mm_setzero_si64();
                src = _mm_unpacklo_pi8(src, zero);
                src = _mm_mullo_pi16(src, color_alpha_64);
                src = _mm_srli_pi16(src, 6);
                src = _mm_packs_pu16(src,src);
                *reinterpret_cast<int*>(dst+j) = _mm_cvtsi64_si32(src);
            }
            for( ;j<x_end;j++)
            {
                dst[j] = (src1[j] * color_alpha)>>6;
            }
            src1 += mOverlayPitch;
            //pAlphaMask += pitch;
            dst += mOverlayPitch;
        }
    }
    else if( ((pBody==NULL) + (pBorder==NULL))==1 && pAlphaMask!=NULL)
    {
        const BYTE* src1 = pBody!=NULL ? pBody : pBorder;
        while(h--)
        {
            int j=0;
            for( ; j<x0; j++ )
            {
                dst[j] = (src1[j] * pAlphaMask[j] * color_alpha)>>12;
            }
            for( ;j<x00;j+=4 )
            {
                __m64 src = _mm_cvtsi32_si64(*reinterpret_cast<const int*>(src1+j));
                __m64 mask = _mm_cvtsi32_si64(*reinterpret_cast<const int*>(pAlphaMask+j));
                __m64 zero = _mm_setzero_si64();
                src = _mm_unpacklo_pi8(src, zero);
                src = _mm_mullo_pi16(src, color_alpha_64);
                mask = _mm_unpacklo_pi8(zero, mask); //important!
                src = _mm_mulhi_pi16(src, mask); //important!
                src = _mm_srli_pi16(src, 12+8-16); //important!
                src = _mm_packs_pu16(src,src);
                *reinterpret_cast<int*>(dst+j) = _mm_cvtsi64_si32(src);
            }
            __m128i zero = _mm_setzero_si128();
            for( ;j<x_end00;j+=16)
            {
                __m128i src = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src1+j));
                __m128i mask = _mm_loadu_si128(reinterpret_cast<const __m128i*>(pAlphaMask+j));
                __m128i srchi = src;
                __m128i maskhi = mask;                 
                src = _mm_unpacklo_epi8(src, zero);
                srchi = _mm_unpackhi_epi8(srchi, zero);
                mask = _mm_unpacklo_epi8(zero, mask); //important!
                maskhi = _mm_unpackhi_epi8(zero, maskhi);
                src = _mm_mullo_epi16(src, color_alpha_128);
                srchi = _mm_mullo_epi16(srchi, color_alpha_128);
                src = _mm_mulhi_epu16(src, mask); //important!
                srchi = _mm_mulhi_epu16(srchi, maskhi);
                src = _mm_srli_epi16(src, 12+8-16); //important!
                srchi = _mm_srli_epi16(srchi, 12+8-16);
                src = _mm_packus_epi16(src, srchi);
                _mm_storeu_si128(reinterpret_cast<__m128i*>(dst+j), src);
            }
            for( ;j<x_end0;j+=4)
            {
                __m64 src = _mm_cvtsi32_si64(*reinterpret_cast<const int*>(src1+j));
                __m64 mask = _mm_cvtsi32_si64(*reinterpret_cast<const int*>(pAlphaMask+j));
                __m64 zero = _mm_setzero_si64();
                src = _mm_unpacklo_pi8(src, zero);
                src = _mm_mullo_pi16(src, color_alpha_64);
                mask = _mm_unpacklo_pi8(zero, mask); //important!
                src = _mm_mulhi_pi16(src, mask); //important!
                src = _mm_srli_pi16(src, 12+8-16); //important!
                src = _mm_packs_pu16(src,src);
                *reinterpret_cast<int*>(dst+j) = _mm_cvtsi64_si32(src);
            }
            for( ;j<x_end;j++)
            {
                dst[j] = (src1[j] * pAlphaMask[j] * color_alpha)>>12;
            }
            src1 += mOverlayPitch;
            pAlphaMask += pitch;
            dst += mOverlayPitch;
        }
    }
    else if( pAlphaMask!=NULL && pBody!=NULL && pBorder!=NULL )
    {
        while(h--)
        {
            int j=0;
            for( ; j<x0; j++ )
            {
                int temp = pBorder[j]-pBody[j];
                temp = temp<0 ? 0 : temp;
                dst[j] = (temp * pAlphaMask[j] * color_alpha)>>12;
            }
            for( ;j<x00;j+=4 )
            {
                __m64 border = _mm_cvtsi32_si64(*reinterpret_cast<const int*>(pBorder+j));
                __m64 body = _mm_cvtsi32_si64(*reinterpret_cast<const int*>(pBody+j));
                border = _mm_subs_pu8(border, body);
                __m64 mask = _mm_cvtsi32_si64(*reinterpret_cast<const int*>(pAlphaMask+j));
                __m64 zero = _mm_setzero_si64();
                border = _mm_unpacklo_pi8(border, zero);
                border = _mm_mullo_pi16(border, color_alpha_64);
                mask = _mm_unpacklo_pi8(zero, mask); //important!
                border = _mm_mulhi_pi16(border, mask); //important!
                border = _mm_srli_pi16(border, 12+8-16); //important!
                border = _mm_packs_pu16(border,border);
                *reinterpret_cast<int*>(dst+j) = _mm_cvtsi64_si32(border);
            }
            __m128i zero = _mm_setzero_si128();
            for( ;j<x_end00;j+=16)
            {
                __m128i border = _mm_loadu_si128(reinterpret_cast<const __m128i*>(pBorder+j));
                __m128i body = _mm_loadu_si128(reinterpret_cast<const __m128i*>(pBody+j));
                border = _mm_subs_epu8(border,body);

                __m128i mask = _mm_loadu_si128(reinterpret_cast<const __m128i*>(pAlphaMask+j));
                __m128i srchi = border;
                __m128i maskhi = mask;                 
                border = _mm_unpacklo_epi8(border, zero);
                srchi = _mm_unpackhi_epi8(srchi, zero);
                mask = _mm_unpacklo_epi8(zero, mask); //important!
                maskhi = _mm_unpackhi_epi8(zero, maskhi);
                border = _mm_mullo_epi16(border, color_alpha_128);
                srchi = _mm_mullo_epi16(srchi, color_alpha_128);
                border = _mm_mulhi_epu16(border, mask); //important!
                srchi = _mm_mulhi_epu16(srchi, maskhi);
                border = _mm_srli_epi16(border, 12+8-16); //important!
                srchi = _mm_srli_epi16(srchi, 12+8-16);
                border = _mm_packus_epi16(border, srchi);
                _mm_storeu_si128(reinterpret_cast<__m128i*>(dst+j), border);
            }
            for( ;j<x_end0;j+=4)
            {
                __m64 border = _mm_cvtsi32_si64(*reinterpret_cast<const int*>(pBorder+j));
                __m64 body = _mm_cvtsi32_si64(*reinterpret_cast<const int*>(pBody+j));
                border = _mm_subs_pu8(border, body);
                __m64 mask = _mm_cvtsi32_si64(*reinterpret_cast<const int*>(pAlphaMask+j));
                __m64 zero = _mm_setzero_si64();
                border = _mm_unpacklo_pi8(border, zero);
                border = _mm_mullo_pi16(border, color_alpha_64);
                mask = _mm_unpacklo_pi8(zero, mask); //important!
                border = _mm_mulhi_pi16(border, mask); //important!
                border = _mm_srli_pi16(border, 12+8-16); //important!
                border = _mm_packs_pu16(border,border);
                *reinterpret_cast<int*>(dst+j) = _mm_cvtsi64_si32(border);
            }
            for( ;j<x_end;j++)
            {
                int temp = pBorder[j]-pBody[j];
                temp = temp<0 ? 0 : temp;
                dst[j] = (temp * pAlphaMask[j] * color_alpha)>>12;
            }
            pBody += mOverlayPitch;
            pBorder += mOverlayPitch;
            pAlphaMask += pitch;
            dst += mOverlayPitch;
        }
    }
    else
    {
        //should NOT happen!
        ASSERT(0);
    }
}

void Overlay::FillAlphaMash( byte* outputAlphaMask, bool fBody, bool fBorder, int x, int y, int w, int h, const byte* pAlphaMask, int pitch, DWORD color_alpha)
{
    if(!fBorder && fBody && pAlphaMask==NULL)
    {
        _DoFillAlphaMash(outputAlphaMask, mpOverlayBuffer.body, NULL, x, y, w, h, pAlphaMask, pitch, color_alpha);        
    }
    else if(/*fBorder &&*/ fBody && pAlphaMask==NULL)
    {
        _DoFillAlphaMash(outputAlphaMask, NULL, mpOverlayBuffer.border, x, y, w, h, pAlphaMask, pitch, color_alpha);        
    }
    else if(!fBody && fBorder /* pAlphaMask==NULL or not*/)
    {
        _DoFillAlphaMash(outputAlphaMask, mpOverlayBuffer.body, mpOverlayBuffer.border, x, y, w, h, pAlphaMask, pitch, color_alpha);        
    }
    else if(!fBorder && fBody && pAlphaMask!=NULL)
    {
        _DoFillAlphaMash(outputAlphaMask, mpOverlayBuffer.body, NULL, x, y, w, h, pAlphaMask, pitch, color_alpha);        
    }
    else if(fBorder && fBody && pAlphaMask!=NULL)
    {
        _DoFillAlphaMash(outputAlphaMask, NULL, mpOverlayBuffer.border, x, y, w, h, pAlphaMask, pitch, color_alpha);        
    }
    else
    {
        //should NOT happen
        ASSERT(0);
    }
}

///////////////////////////////////////////////////////////////

// PathData

PathData::PathData():mpPathTypes(NULL), mpPathPoints(NULL), mPathPoints(0)
{
}

PathData::PathData( const PathData& src )
{
    //TODO: deal with the case that src.mPathPoints<0 
    if(mPathPoints!=src.mPathPoints && src.mPathPoints>0)
    {
        mPathPoints = src.mPathPoints;
        mpPathTypes = (BYTE*)realloc(mpPathTypes, mPathPoints * sizeof(BYTE));
        mpPathPoints = (POINT*)realloc(mpPathPoints, mPathPoints * sizeof(POINT));
    }
    if(src.mPathPoints>0)
    {
        memcpy(mpPathTypes, src.mpPathTypes, mPathPoints*sizeof(BYTE));
        memcpy(mpPathPoints, src.mpPathPoints, mPathPoints*sizeof(POINT));
    }
}

const PathData& PathData::operator=( const PathData& src )
{
    if(this!=&src)
    {
        if(mPathPoints!=src.mPathPoints && src.mPathPoints>0)
        {
            mPathPoints = src.mPathPoints;
            mpPathTypes = (BYTE*)realloc(mpPathTypes, mPathPoints * sizeof(BYTE));
            mpPathPoints = (POINT*)realloc(mpPathPoints, mPathPoints * sizeof(POINT));
        }
        if(src.mPathPoints>0)
        {
            memcpy(mpPathTypes, src.mpPathTypes, mPathPoints*sizeof(BYTE));
            memcpy(mpPathPoints, src.mpPathPoints, mPathPoints*sizeof(POINT));
        }
    }
    return *this;
}

PathData::~PathData()
{
    _TrashPath();
}

void PathData::_TrashPath()
{
    delete [] mpPathTypes;
    delete [] mpPathPoints;
    mpPathTypes = NULL;
    mpPathPoints = NULL;
    mPathPoints = 0;
}

bool PathData::BeginPath(HDC hdc)
{
    _TrashPath();
    return !!::BeginPath(hdc);
}

bool PathData::EndPath(HDC hdc)
{
    ::CloseFigure(hdc);
    if(::EndPath(hdc))
    {
        mPathPoints = GetPath(hdc, NULL, NULL, 0);
        if(!mPathPoints)
            return true;
        mpPathTypes = (BYTE*)malloc(sizeof(BYTE) * mPathPoints);
        mpPathPoints = (POINT*)malloc(sizeof(POINT) * mPathPoints);
        if(mPathPoints == GetPath(hdc, mpPathPoints, mpPathTypes, mPathPoints))
            return true;
    }
    ::AbortPath(hdc);
    return false;
}

bool PathData::PartialBeginPath(HDC hdc, bool bClearPath)
{
    if(bClearPath)
        _TrashPath();
    return !!::BeginPath(hdc);
}

bool PathData::PartialEndPath(HDC hdc, long dx, long dy)
{
    ::CloseFigure(hdc);
    if(::EndPath(hdc))
    {
        int nPoints;
        BYTE* pNewTypes;
        POINT* pNewPoints;
        nPoints = GetPath(hdc, NULL, NULL, 0);
        if(!nPoints)
            return true;
        pNewTypes = (BYTE*)realloc(mpPathTypes, (mPathPoints + nPoints) * sizeof(BYTE));
        pNewPoints = (POINT*)realloc(mpPathPoints, (mPathPoints + nPoints) * sizeof(POINT));
        if(pNewTypes)
            mpPathTypes = pNewTypes;
        if(pNewPoints)
            mpPathPoints = pNewPoints;
        BYTE* pTypes = new BYTE[nPoints];
        POINT* pPoints = new POINT[nPoints];
        if(pNewTypes && pNewPoints && nPoints == GetPath(hdc, pPoints, pTypes, nPoints))
        {
            for(int i = 0; i < nPoints; ++i)
            {
                mpPathPoints[mPathPoints + i].x = pPoints[i].x + dx;
                mpPathPoints[mPathPoints + i].y = pPoints[i].y + dy;
                mpPathTypes[mPathPoints + i] = pTypes[i];
            }
            mPathPoints += nPoints;
            delete[] pTypes;
            delete[] pPoints;
            return true;
        }
        else
            DebugBreak();
        delete[] pTypes;
        delete[] pPoints;
    }
    ::AbortPath(hdc);
    return false;
}

//////////////////////////////////////////////////////////////////////////

// ScanLineData

ScanLineData::ScanLineData():mPathOffsetX(0),mPathOffsetY(0)
{
}

ScanLineData::~ScanLineData()
{    
}

void ScanLineData::_ReallocEdgeBuffer(int edges)
{
    mEdgeHeapSize = edges;
    mpEdgeBuffer = (Edge*)realloc(mpEdgeBuffer, sizeof(Edge)*edges);
}

void ScanLineData::_EvaluateBezier(const PathData& path_data, int ptbase, bool fBSpline)
{
    const POINT* pt0 = path_data.mpPathPoints + ptbase;
    const POINT* pt1 = path_data.mpPathPoints + ptbase + 1;
    const POINT* pt2 = path_data.mpPathPoints + ptbase + 2;
    const POINT* pt3 = path_data.mpPathPoints + ptbase + 3;
    double x0 = pt0->x;
    double x1 = pt1->x;
    double x2 = pt2->x;
    double x3 = pt3->x;
    double y0 = pt0->y;
    double y1 = pt1->y;
    double y2 = pt2->y;
    double y3 = pt3->y;
    double cx3, cx2, cx1, cx0, cy3, cy2, cy1, cy0;
    if(fBSpline)
    {
        // 1   [-1 +3 -3 +1]
        // - * [+3 -6 +3  0]
        // 6   [-3  0 +3  0]
        //	   [+1 +4 +1  0]
        double _1div6 = 1.0/6.0;
        cx3 = _1div6*(-  x0+3*x1-3*x2+x3);
        cx2 = _1div6*( 3*x0-6*x1+3*x2);
        cx1 = _1div6*(-3*x0	   +3*x2);
        cx0 = _1div6*(   x0+4*x1+1*x2);
        cy3 = _1div6*(-  y0+3*y1-3*y2+y3);
        cy2 = _1div6*( 3*y0-6*y1+3*y2);
        cy1 = _1div6*(-3*y0     +3*y2);
        cy0 = _1div6*(   y0+4*y1+1*y2);
    }
    else // bezier
    {
        // [-1 +3 -3 +1]
        // [+3 -6 +3  0]
        // [-3 +3  0  0]
        // [+1  0  0  0]
        cx3 = -  x0+3*x1-3*x2+x3;
        cx2 =  3*x0-6*x1+3*x2;
        cx1 = -3*x0+3*x1;
        cx0 =    x0;
        cy3 = -  y0+3*y1-3*y2+y3;
        cy2 =  3*y0-6*y1+3*y2;
        cy1 = -3*y0+3*y1;
        cy0 =    y0;
    }
    //
    // This equation is from Graphics Gems I.
    //
    // The idea is that since we're approximating a cubic curve with lines,
    // any error we incur is due to the curvature of the line, which we can
    // estimate by calculating the maximum acceleration of the curve.  For
    // a cubic, the acceleration (second derivative) is a line, meaning that
    // the absolute maximum acceleration must occur at either the beginning
    // (|c2|) or the end (|c2+c3|).  Our bounds here are a little more
    // conservative than that, but that's okay.
    //
    // If the acceleration of the parametric formula is zero (c2 = c3 = 0),
    // that component of the curve is linear and does not incur any error.
    // If a=0 for both X and Y, the curve is a line segment and we can
    // use a step size of 1.
    double maxaccel1 = fabs(2*cy2) + fabs(6*cy3);
    double maxaccel2 = fabs(2*cx2) + fabs(6*cx3);
    double maxaccel = maxaccel1 > maxaccel2 ? maxaccel1 : maxaccel2;
    double h = 1.0;
    if(maxaccel > 8.0) h = sqrt(8.0 / maxaccel);
    if(!fFirstSet) {firstp.x = (LONG)cx0; firstp.y = (LONG)cy0; lastp = firstp; fFirstSet = true;}
    for(double t = 0; t < 1.0; t += h)
    {
        double x = cx0 + t*(cx1 + t*(cx2 + t*cx3));
        double y = cy0 + t*(cy1 + t*(cy2 + t*cy3));
        _EvaluateLine(lastp.x, lastp.y, (int)x, (int)y);
    }
    double x = cx0 + cx1 + cx2 + cx3;
    double y = cy0 + cy1 + cy2 + cy3;
    _EvaluateLine(lastp.x, lastp.y, (int)x, (int)y);
}

void ScanLineData::_EvaluateLine(const PathData& path_data, int pt1idx, int pt2idx)
{
    const POINT* pt1 = path_data.mpPathPoints + pt1idx;
    const POINT* pt2 = path_data.mpPathPoints + pt2idx;
    _EvaluateLine(pt1->x, pt1->y, pt2->x, pt2->y);
}

void ScanLineData::_EvaluateLine(int x0, int y0, int x1, int y1)
{
    if(lastp.x != x0 || lastp.y != y0)
    {
        _EvaluateLine(lastp.x, lastp.y, x0, y0);
    }
    if(!fFirstSet) {firstp.x = x0; firstp.y = y0; fFirstSet = true;}
    lastp.x = x1;
    lastp.y = y1;
    if(y1 > y0)	// down
    {
        __int64 xacc = (__int64)x0 << 13;
        // prestep y0 down
        int dy = y1 - y0;
        int y = ((y0 + 3)&~7) + 4;
        int iy = y >> 3;
        y1 = (y1 - 5) >> 3;
        if(iy <= y1)
        {
            __int64 invslope = (__int64(x1 - x0) << 16) / dy;
            while(mEdgeNext + y1 + 1 - iy > mEdgeHeapSize)
                _ReallocEdgeBuffer(mEdgeHeapSize*2);
            xacc += (invslope * (y - y0)) >> 3;
            while(iy <= y1)
            {
                int ix = (int)((xacc + 32768) >> 16);
                mpEdgeBuffer[mEdgeNext].next = mpScanBuffer[iy];
                mpEdgeBuffer[mEdgeNext].posandflag = ix*2 + 1;
                mpScanBuffer[iy] = mEdgeNext++;
                ++iy;
                xacc += invslope;
            }
        }
    }
    else if(y1 < y0) // up
    {
        __int64 xacc = (__int64)x1 << 13;
        // prestep y1 down
        int dy = y0 - y1;
        int y = ((y1 + 3)&~7) + 4;
        int iy = y >> 3;
        y0 = (y0 - 5) >> 3;
        if(iy <= y0)
        {
            __int64 invslope = (__int64(x0 - x1) << 16) / dy;
            while(mEdgeNext + y0 + 1 - iy > mEdgeHeapSize)
                _ReallocEdgeBuffer(mEdgeHeapSize*2);
            xacc += (invslope * (y - y1)) >> 3;
            while(iy <= y0)
            {
                int ix = (int)((xacc + 32768) >> 16);
                mpEdgeBuffer[mEdgeNext].next = mpScanBuffer[iy];
                mpEdgeBuffer[mEdgeNext].posandflag = ix*2;
                mpScanBuffer[iy] = mEdgeNext++;
                ++iy;
                xacc += invslope;
            }
        }
    }
}

bool ScanLineData::ScanConvert(SharedPtrPathData path_data)
{
    int lastmoveto = -1;
    int i;
    // Drop any outlines we may have.
    mOutline.clear();
    mWideOutline.clear();
    mWideBorder = 0;
    // Determine bounding box
    if(!path_data->mPathPoints)
    {
        mPathOffsetX = mPathOffsetY = 0;
        mWidth = mHeight = 0;
        return 0;
    }
    int minx = INT_MAX;
    int miny = INT_MAX;
    int maxx = INT_MIN;
    int maxy = INT_MIN;
    for(i=0; i<path_data->mPathPoints; ++i)
    {
        int ix = path_data->mpPathPoints[i].x;
        int iy = path_data->mpPathPoints[i].y;
        if(ix < minx) minx = ix;
        if(ix > maxx) maxx = ix;
        if(iy < miny) miny = iy;
        if(iy > maxy) maxy = iy;
    }
    minx = (minx >> 3) & ~7;
    miny = (miny >> 3) & ~7;
    maxx = (maxx + 7) >> 3;
    maxy = (maxy + 7) >> 3;
    for(i=0; i<path_data->mPathPoints; ++i)
    {
        path_data->mpPathPoints[i].x -= minx*8;
        path_data->mpPathPoints[i].y -= miny*8;
    }
    if(minx > maxx || miny > maxy)
    {
        mWidth = mHeight = 0;
        mPathOffsetX = mPathOffsetY = 0;
        path_data->_TrashPath();
        return true;
    }
    mWidth = maxx + 1 - minx;
    mHeight = maxy + 1 - miny;
    mPathOffsetX = minx;
    mPathOffsetY = miny;
    // Initialize edge buffer.  We use edge 0 as a sentinel.
    mEdgeNext = 1;
    mEdgeHeapSize = 2048;
    mpEdgeBuffer = (Edge*)malloc(sizeof(Edge)*mEdgeHeapSize);
    // Initialize scanline list.
    mpScanBuffer = new unsigned int[mHeight];
    memset(mpScanBuffer, 0, mHeight*sizeof(unsigned int));
    // Scan convert the outline.  Yuck, Bezier curves....
    // Unfortunately, Windows 95/98 GDI has a bad habit of giving us text
    // paths with all but the first figure left open, so we can't rely
    // on the PT_CLOSEFIGURE flag being used appropriately.
    fFirstSet = false;
    firstp.x = firstp.y = 0;
    lastp.x = lastp.y = 0;
    for(i=0; i<path_data->mPathPoints; ++i)
    {
        BYTE t = path_data->mpPathTypes[i] & ~PT_CLOSEFIGURE;
        switch(t)
        {
        case PT_MOVETO:
            if(lastmoveto >= 0 && firstp != lastp)
                _EvaluateLine(lastp.x, lastp.y, firstp.x, firstp.y);
            lastmoveto = i;
            fFirstSet = false;
            lastp = path_data->mpPathPoints[i];
            break;
        case PT_MOVETONC:
            break;
        case PT_LINETO:
            if(path_data->mPathPoints - (i-1) >= 2) _EvaluateLine(*path_data, i-1, i);
            break;
        case PT_BEZIERTO:
            if(path_data->mPathPoints - (i-1) >= 4) _EvaluateBezier(*path_data, i-1, false);
            i += 2;
            break;
        case PT_BSPLINETO:
            if(path_data->mPathPoints - (i-1) >= 4) _EvaluateBezier(*path_data, i-1, true);
            i += 2;
            break;
        case PT_BSPLINEPATCHTO:
            if(path_data->mPathPoints - (i-3) >= 4) _EvaluateBezier(*path_data, i-3, true);
            break;
        }
    }
    if(lastmoveto >= 0 && firstp != lastp)
        _EvaluateLine(lastp.x, lastp.y, firstp.x, firstp.y);
    // Free the path since we don't need it anymore.
    path_data->_TrashPath();
    // Convert the edges to spans.  We couldn't do this before because some of
    // the regions may have winding numbers >+1 and it would have been a pain
    // to try to adjust the spans on the fly.  We use one heap to detangle
    // a scanline's worth of edges from the singly-linked lists, and another
    // to collect the actual scans.
    std::vector<int> heap;
    mOutline.reserve(mEdgeNext / 2);
    __int64 y = 0;
    for(y=0; y<mHeight; ++y)
    {
        int count = 0;
        // Detangle scanline into edge heap.
        for(unsigned ptr = (unsigned)(mpScanBuffer[y]&0xffffffff); ptr; ptr = mpEdgeBuffer[ptr].next)
        {
            heap.push_back(mpEdgeBuffer[ptr].posandflag);
        }
        // Sort edge heap.  Note that we conveniently made the opening edges
        // one more than closing edges at the same spot, so we won't have any
        // problems with abutting spans.
        std::sort(heap.begin(), heap.end()/*begin() + heap.size()*/);
        // Process edges and add spans.  Since we only check for a non-zero
        // winding number, it doesn't matter which way the outlines go!
        std::vector<int>::iterator itX1 = heap.begin();
        std::vector<int>::iterator itX2 = heap.end(); // begin() + heap.size();
        int x1, x2;
        for(; itX1 != itX2; ++itX1)
        {
            int x = *itX1;
            if(!count)
                x1 = (x>>1);
            if(x&1)
                ++count;
            else
                --count;
            if(!count)
            {
                x2 = (x>>1);
                if(x2>x1)
                    mOutline.push_back(std::pair<__int64,__int64>((y<<32)+x1+0x4000000040000000i64, (y<<32)+x2+0x4000000040000000i64)); // G: damn Avery, this is evil! :)
            }
        }
        heap.clear();
    }
    // Dump the edge and scan buffers, since we no longer need them.
    free(mpEdgeBuffer);
    delete [] mpScanBuffer;
    // All done!
    return true;
}

using namespace std;

void ScanLineData::_OverlapRegion(tSpanBuffer& dst, tSpanBuffer& src, int dx, int dy)
{
    tSpanBuffer temp;
    temp.reserve(dst.size() + src.size());
    dst.swap(temp);
    tSpanBuffer::iterator itA = temp.begin();
    tSpanBuffer::iterator itAE = temp.end();
    tSpanBuffer::iterator itB = src.begin();
    tSpanBuffer::iterator itBE = src.end();
    // Don't worry -- even if dy<0 this will still work! // G: hehe, the evil twin :)
    unsigned __int64 offset1 = (((__int64)dy)<<32) - dx;
    unsigned __int64 offset2 = (((__int64)dy)<<32) + dx;
    while(itA != itAE && itB != itBE)
    {
        if((*itB).first + offset1 < (*itA).first)
        {
            // B span is earlier.  Use it.
            unsigned __int64 x1 = (*itB).first + offset1;
            unsigned __int64 x2 = (*itB).second + offset2;
            ++itB;
            // B spans don't overlap, so begin merge loop with A first.
            for(;;)
            {
                // If we run out of A spans or the A span doesn't overlap,
                // then the next B span can't either (because B spans don't
                // overlap) and we exit.
                if(itA == itAE || (*itA).first > x2)
                    break;
                do {x2 = _MAX(x2, (*itA++).second);}
                while(itA != itAE && (*itA).first <= x2);
                // If we run out of B spans or the B span doesn't overlap,
                // then the next A span can't either (because A spans don't
                // overlap) and we exit.
                if(itB == itBE || (*itB).first + offset1 > x2)
                    break;
                do {x2 = _MAX(x2, (*itB++).second + offset2);}
                while(itB != itBE && (*itB).first + offset1 <= x2);
            }
            // Flush span.
            dst.push_back(tSpan(x1, x2));
        }
        else
        {
            // A span is earlier.  Use it.
            unsigned __int64 x1 = (*itA).first;
            unsigned __int64 x2 = (*itA).second;
            ++itA;
            // A spans don't overlap, so begin merge loop with B first.
            for(;;)
            {
                // If we run out of B spans or the B span doesn't overlap,
                // then the next A span can't either (because A spans don't
                // overlap) and we exit.
                if(itB == itBE || (*itB).first + offset1 > x2)
                    break;
                do {x2 = _MAX(x2, (*itB++).second + offset2);}
                while(itB != itBE && (*itB).first + offset1 <= x2);
                // If we run out of A spans or the A span doesn't overlap,
                // then the next B span can't either (because B spans don't
                // overlap) and we exit.
                if(itA == itAE || (*itA).first > x2)
                    break;
                do {x2 = _MAX(x2, (*itA++).second);}
                while(itA != itAE && (*itA).first <= x2);
            }
            // Flush span.
            dst.push_back(tSpan(x1, x2));
        }
    }
    // Copy over leftover spans.
    while(itA != itAE)
        dst.push_back(*itA++);
    while(itB != itBE)
    {
        dst.push_back(tSpan((*itB).first + offset1, (*itB).second + offset2));
        ++itB;
    }
}

bool ScanLineData::CreateWidenedRegion(int rx, int ry)
{
    if(rx < 0) rx = 0;
    if(ry < 0) ry = 0;
    mWideBorder = max(rx,ry);
    if (ry > 0)
    {
        // Do a half circle.
        // _OverlapRegion mirrors this so both halves are done.
        for(int y = -ry; y <= ry; ++y)
        {
            int x = (int)(0.5 + sqrt(float(ry*ry - y*y)) * float(rx)/float(ry));
            _OverlapRegion(mWideOutline, mOutline, x, y);
        }
    }
    else if (ry == 0 && rx > 0)
    {
        // There are artifacts if we don't make at least two overlaps of the line, even at same Y coord
        _OverlapRegion(mWideOutline, mOutline, rx, 0);
        _OverlapRegion(mWideOutline, mOutline, rx, 0);
    }
    return true;
}

void ScanLineData::DeleteOutlines()
{
    mWideOutline.clear();
    mOutline.clear();
}