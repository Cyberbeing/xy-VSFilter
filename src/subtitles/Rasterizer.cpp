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
#include <cmath>
#include <vector>
#include <algorithm>
#include "Rasterizer.h"
#include "SeparableFilter.h"
#include "xy_logger.h"
#include <boost/flyweight/key_value.hpp>
#include "xy_bitmap.h"
#include "xy_widen_region.h"

#ifndef _MAX	/* avoid collision with common (nonconforming) macros */
#define _MAX	(std::max)
#define _MIN	(std::min)
#define _IMPL_MAX std::max
#define _IMPL_MIN std::min
#else
#define _IMPL_MAX _MAX
#define _IMPL_MIN _MIN
#endif

typedef const UINT8 CUINT8, *PCUINT8;

//NOTE: signed or unsigned affects the result seriously
#define COMBINE_AYUV(a, y, u, v) ((((((((int)(a))<<8)|y)<<8)|u)<<8)|v)

#define SPLIT_AYUV(color, a, y, u, v) do { \
        *(v)=(color)&0xff; \
        *(u)=((color)>>8) &0xff; \
        *(y)=((color)>>16)&0xff;\
        *(a)=((color)>>24)&0xff;\
    } while(0)

class GaussianCoefficients
{
public:
    int g_r;
    int g_w;
    int g_w_ex;
    float *g_f;

    double sigma;
public:
    GaussianCoefficients(const double sigma)
    {
        g_r = 0;
        g_w = 0;
        g_w_ex = 0;

        g_f = NULL;

        this->sigma = 0;
        init(sigma);
    }
    GaussianCoefficients(const GaussianCoefficients& priv)
        :g_r(priv.g_r),g_w(priv.g_w),sigma(priv.sigma),g_f(NULL)
        ,g_w_ex(priv.g_w_ex)
    {
        if (this->g_w_ex > 0 && this != &priv) {
            this->g_f = reinterpret_cast<float*>(xy_malloc(this->g_w_ex * sizeof(float)));
            ASSERT(this->g_f);
            memcpy(g_f, priv.g_f, this->g_w_ex * sizeof(g_f[0]));
        }
    }

    ~GaussianCoefficients()
    {
        xy_free(g_f); g_f=NULL;
    }

private:
    int init(double sigma)
    {
        double a = -1 / (sigma * sigma * 2);
        double exp_a = exp(a);

        double volume =  0;

        if (this->sigma == sigma)
            return 0;
        else
            this->sigma = sigma;

        this->g_w = (int)ceil(sigma*3) | 1;
        this->g_r = this->g_w / 2;
        this->g_w_ex = (this->g_w + 3) & ~3;

        if (this->g_w_ex > 0) {
            xy_free(this->g_f);
            this->g_f = reinterpret_cast<float*>(xy_malloc(this->g_w_ex * sizeof(float)));
            if (this->g_f == NULL) {
                return -1;
            }
        }

        if (this->g_w > 0) {
            volume = 0;        

            double exp_0 = 1.0;
            double exp_1 = exp_a;
            double exp_2 = exp_1 * exp_1;
            volume = exp_0;
            this->g_f[this->g_r] = exp_0;
            float* p_left = this->g_f+this->g_r-1;
            float* p_right= this->g_f+this->g_r+1;
            for(int i=0; i<this->g_r;++i,p_left--,p_right++)
            {
                exp_0 *= exp_1;
                exp_1 *= exp_2;

                *p_left = exp_0;
                *p_right = exp_0;

                volume += exp_0;
                volume += exp_0;
            }
            //equivalent:
            //  for (i = 0; i < this->g_w; ++i) {
            //    this->g[i] = (unsigned) ( exp(a * (i - this->g_r) * (i - this->g_r))* volume_factor + .5 );
            //    volume += this->g[i];
            //  }
            ASSERT(volume>0);
            for (int i=0;i<this->g_w;i++)
            {
                this->g_f[i] /= volume;
            }
            for (int i=this->g_w;i<this->g_w_ex;i++)
            {
                this->g_f[i] = 0;
            }
        }
        return 0;
    }

};

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


// GaussianFilter = GaussianCoefficients or ass_synth_priv
template<typename GaussianFilter>
struct GaussianFilterKey
{
    const double& operator()(const GaussianFilter& x)const
    {
        return x.sigma;
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

/*
 * \brief gaussian blur.  an fast pure c implementation from libass.
 */
static void ass_gauss_blur(unsigned char *buffer, unsigned *tmp2,
                           int width, int height, int stride, 
                           const unsigned *g_t_x, int g_r_x, int g_width_x, 
                           const unsigned *g_t_y, int g_r_y, int g_width_y)
{

    int x, y;

    unsigned char *s = buffer;
    unsigned *t = tmp2 + 1;
    for (y = 0; y < height; y++) {
        memset(t - 1, 0, (width + 1) * sizeof(*t));
        x = 0;
        if(x < g_r_x)//in case that r < 0
        {            
            const int src = s[x];
            if (src) {
                register unsigned *dstp = t + x - g_r_x;
                int mx;
                const unsigned *m3 = g_t_x + src * g_width_x;
                unsigned sum = 0;
                for (mx = g_width_x-1; mx >= g_r_x - x ; mx--) {
                    sum += m3[mx];
                    dstp[mx] += sum;
                }
            }
        }

        for (x = 1; x < g_r_x; x++) {
            const int src = s[x];
            if (src) {
                register unsigned *dstp = t + x - g_r_x;
                int mx;
                const unsigned *m3 = g_t_x + src * g_width_x;
                for (mx = g_r_x - x; mx < g_width_x; mx++) {
                    dstp[mx] += m3[mx];
                }
            }
        }

        for (; x < width - g_r_x; x++) {
            const int src = s[x];
            if (src) {
                register unsigned *dstp = t + x - g_r_x;
                int mx;
                const unsigned *m3 = g_t_x + src * g_width_x;
                for (mx = 0; mx < g_width_x; mx++) {
                    dstp[mx] += m3[mx];
                }
            }
        }

        for (; x < width-1; x++) {
            const int src = s[x];
            if (src) {
                register unsigned *dstp = t + x - g_r_x;
                int mx;
                const int x2 = g_r_x + width - x;
                const unsigned *m3 = g_t_x + src * g_width_x;
                for (mx = 0; mx < x2; mx++) {
                    dstp[mx] += m3[mx];
                }
            }
        }
        if(x==width-1) //important: x==width-1 failed, if r==0
        {
            const int src = s[x];
            if (src) {
                register unsigned *dstp = t + x - g_r_x;
                int mx;
                const int x2 = g_r_x + width - x;
                const unsigned *m3 = g_t_x + src * g_width_x;
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
        if(y < g_r_y)//in case that r<0
        {            
            unsigned *srcp = t + y * (width + 1) + 1;
            int src = *srcp;
            if (src) {
                register unsigned *dstp = srcp - 1 + (g_width_y -g_r_y +y)*(width + 1);
                const int src2 = (src + (1<<(ass_synth_priv::VOLUME_BITS-1))) >> ass_synth_priv::VOLUME_BITS;
                const unsigned *m3 = g_t_y + src2 * g_width_y;
                unsigned sum = 0;
                int mx;
                *srcp = (1<<(ass_synth_priv::VOLUME_BITS-1));
                for (mx = g_width_y-1; mx >=g_r_y - y ; mx--) {
                    sum += m3[mx];
                    *dstp += sum;
                    dstp -= width + 1;
                }
            }
        }
        for (y = 1; y < g_r_y; y++) {
            unsigned *srcp = t + y * (width + 1) + 1;
            int src = *srcp;
            if (src) {
                register unsigned *dstp = srcp - 1 + width + 1;
                const int src2 = (src + (1<<(ass_synth_priv::VOLUME_BITS-1))) >> ass_synth_priv::VOLUME_BITS;
                const unsigned *m3 = g_t_y + src2 * g_width_y;

                int mx;
                *srcp = (1<<(ass_synth_priv::VOLUME_BITS-1));
                for (mx = g_r_y - y; mx < g_width_y; mx++) {
                    *dstp += m3[mx];
                    dstp += width + 1;
                }
            }
        }
        for (; y < height - g_r_y; y++) {
            unsigned *srcp = t + y * (width + 1) + 1;
            int src = *srcp;
            if (src) {
                register unsigned *dstp = srcp - 1 - g_r_y * (width + 1);
                const int src2 = (src + (1<<(ass_synth_priv::VOLUME_BITS-1))) >> ass_synth_priv::VOLUME_BITS;
                const unsigned *m3 = g_t_y + src2 * g_width_y;

                int mx;
                *srcp = (1<<(ass_synth_priv::VOLUME_BITS-1));
                for (mx = 0; mx < g_width_y; mx++) {
                    *dstp += m3[mx];
                    dstp += width + 1;
                }
            }
        }
        for (; y < height-1; y++) {
            unsigned *srcp = t + y * (width + 1) + 1;
            int src = *srcp;
            if (src) {
                const int y2 = g_r_y + height - y;
                register unsigned *dstp = srcp - 1 - g_r_y * (width + 1);
                const int src2 = (src + (1<<(ass_synth_priv::VOLUME_BITS-1))) >> ass_synth_priv::VOLUME_BITS;
                const unsigned *m3 = g_t_y + src2 * g_width_y;

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
                const int y2 = g_r_y + height - y;
                register unsigned *dstp = srcp - 1 - g_r_y * (width + 1);
                const int src2 = (src + (1<<(ass_synth_priv::VOLUME_BITS-1))) >> ass_synth_priv::VOLUME_BITS;
                const unsigned *m3 = g_t_y + src2 * g_width_y;
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

void xy_gaussian_blur(PUINT8 dst, int dst_stride,
    PCUINT8 src, int width, int height, int stride, 
    const float *gt_x, int r_x, int gt_ex_width_x, 
    const float *gt_y, int r_y, int gt_ex_width_y);

void xy_be_blur(PUINT8 src, int width, int height, int stride, float pass_x, float pass_y);

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
    WORD *col_pix_buf = col_pix_buf_base-2;//for aligment;
    WORD *col_sum_buf = col_sum_buf_base-2;//for aligment;
    {
        int y = 0;
        unsigned char *src=buf+y*stride;

        int x = 2;
        int old_pix = src[x-1];
        int old_sum = old_pix + src[x-2];
        for ( ; x < w; x++) {
            int temp1 = src[x];
            int temp2 = old_pix + temp1;
            old_pix = temp1;
            temp1 = old_sum + temp2;
            old_sum = temp2;
            col_pix_buf[x] = temp1;
        }
    }
    {
        int y = 1;
        unsigned char *src=buf+y*stride;


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
            //dst[x-1] = (col_sum_buf[x] + temp2) >> 4;
            col_sum_buf[x] = temp2;
        }
    }

    //__m128i round = _mm_set1_epi16(8);
    for (int y = 2; y < h; y++) {
        unsigned char *src=buf+y*stride;
        unsigned char *dst=buf+(y-1)*stride;

                
        int x = 2;
        __m128i old_pix_128 = _mm_cvtsi32_si128(src[1]);
        __m128i old_sum_128 = _mm_cvtsi32_si128(src[0]+src[1]);
        for ( ; x < ((w-2)&(~7)); x+=8) {
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
            //old_col_sum = _mm_add_epi16(old_col_sum, round);
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
            dst[x-1] = (col_sum_buf[x] + temp2) >> 4;
            col_sum_buf[x] = temp2;
        }
    }

    xy_free(col_sum_buf_base);
    xy_free(col_pix_buf_base);
}

/**
 * see @be_blur
 */
static void be_blur_c(unsigned char *buf, unsigned *tmp_base, int w, int h, int stride)
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
    WORD *col_pix_buf = col_pix_buf_base-2;//for aligment;
    WORD *col_sum_buf = col_sum_buf_base-2;//for aligment;
    {
        int y = 0;
        unsigned char *src=buf+y*stride;

        int x = 2;
        int old_pix = src[x-1];
        int old_sum = old_pix + src[x-2];
        for ( ; x < w; x++) {
            int temp1 = src[x];
            int temp2 = old_pix + temp1;
            old_pix = temp1;
            temp1 = old_sum + temp2;
            old_sum = temp2;
            col_pix_buf[x] = temp1;
        }
    }
    {
        int y = 1;
        unsigned char *src=buf+y*stride;


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
            //dst[x-1] = (col_sum_buf[x] + temp2) >> 4;
            col_sum_buf[x] = temp2;
        }
    }

    for (int y = 2; y < h; y++) {
        unsigned char *src=buf+y*stride;
        unsigned char *dst=buf+(y-1)*stride;

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
            dst[x-1] = (col_sum_buf[x] + temp2) >> 4;
            col_sum_buf[x] = temp2;
        }
    }

    xy_free(col_sum_buf_base);
    xy_free(col_pix_buf_base);
}

static void Bilinear(unsigned char *buf, int w, int h, int stride, int x_factor, int y_factor)
{   
    WORD *col_pix_buf_base = reinterpret_cast<WORD*>(xy_malloc(w*sizeof(WORD)));
    if(!col_pix_buf_base)
    {
        //ToDo: error handling
        return;
    }
    memset(col_pix_buf_base, 0, w*sizeof(WORD));

    for (int y = 0; y < h; y++){
        unsigned char *src=buf+y*stride;

        WORD *col_pix_buf = col_pix_buf_base;
        int last=0;
        for(int x = 0; x < w; x++)
        {
            int temp1 = src[x];
            int temp2 = temp1*x_factor;
            temp1 <<= 3;
            temp1 -= temp2;
            temp1 += last;
            last = temp2;

            temp2 = temp1*y_factor;
            temp1 <<= 3;
            temp1 -= temp2;
            temp1 += col_pix_buf[x];
            src[x] = ((temp1+32)>>6);
            col_pix_buf[x] = temp2;
        }
    }
    xy_free(col_pix_buf_base);
}

extern "C" void* memsetSSE2(void* dest, int c, size_t count);

bool Rasterizer::Rasterize(const ScanLineData2& scan_line_data2, int xsub, int ysub, SharedPtrOverlay overlay)
{
    using namespace ::boost::flyweights;

    if(!overlay)
    {
        return false;
    }
    overlay->CleanUp();
    const ScanLineData& scan_line_data = *scan_line_data2.m_scan_line_data;
    if(!scan_line_data.mWidth || !scan_line_data.mHeight)
    {
        return true;
    }
    xsub &= 7;
    ysub &= 7;
    //xsub = ysub = 0;
    int width = scan_line_data.mWidth + xsub;
    int height = scan_line_data.mHeight + ysub;
    overlay->mfWideOutlineEmpty = scan_line_data2.mWideOutline.empty();
    if(!overlay->mfWideOutlineEmpty)
    {
        int wide_border = (scan_line_data2.mWideBorder+7)&~7;

        width += 2*wide_border ;
        height += 2*wide_border ;
        xsub += wide_border ;
        ysub += wide_border ;
    }
    overlay->mOffsetX = scan_line_data2.mPathOffsetX - xsub;
    overlay->mOffsetY = scan_line_data2.mPathOffsetY - ysub;

    overlay->mWidth = width;
    overlay->mHeight = height;
    overlay->mOverlayWidth = ((width+7)>>3) + 1;
    overlay->mOverlayHeight = ((height+7)>>3) + 1;
    overlay->mOverlayPitch = (overlay->mOverlayWidth+15)&~15;

    BYTE* body = reinterpret_cast<BYTE*>(xy_malloc(overlay->mOverlayPitch * overlay->mOverlayHeight));
    if( body==NULL )
    {
        return false;
    }
    overlay->mBody.reset(body, xy_free);
    memsetSSE2(body, 0, overlay->mOverlayPitch * overlay->mOverlayHeight);
    BYTE* border = NULL;
    if (!overlay->mfWideOutlineEmpty)
    {
        border = reinterpret_cast<BYTE*>(xy_malloc(overlay->mOverlayPitch * overlay->mOverlayHeight));
        if (border==NULL)
        {
            return false;
        }
        overlay->mBorder.reset(border, xy_free);
        memsetSSE2(border, 0, overlay->mOverlayPitch * overlay->mOverlayHeight);
    }

    // Are we doing a border?
    const tSpanBuffer* pOutline[2] = {&(scan_line_data.mOutline), &(scan_line_data2.mWideOutline)};
    for(int i = countof(pOutline)-1; i >= 0; i--)
    {
        tSpanBuffer::const_iterator it = pOutline[i]->begin();
        tSpanBuffer::const_iterator itEnd = pOutline[i]->end();
        byte* plan_selected = i==0 ? body : border;
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

const float Rasterizer::GAUSSIAN_BLUR_THREHOLD = 0.333333f;

bool Rasterizer::IsItReallyBlur( float be_strength, double gaussian_blur_strength )
{
    if (be_strength<=0 && gaussian_blur_strength<=GAUSSIAN_BLUR_THREHOLD)
    {
        return false;
    }
    return true;
}

// @return: true if actually a blur operation has done, or else false and output is leave unset.
// To Do: rewrite it or delete it
bool Rasterizer::OldFixedPointBlur(const Overlay& input_overlay, float be_strength, double gaussian_blur_strength, 
    double target_scale_x, double target_scale_y, SharedPtrOverlay output_overlay)
{
    using namespace ::boost::flyweights;
    
    ASSERT(IsItReallyBlur(be_strength, gaussian_blur_strength));
    if(!output_overlay)
    {
        return false;
    }
    output_overlay->CleanUp();

    output_overlay->mOffsetX = input_overlay.mOffsetX;
    output_overlay->mOffsetY = input_overlay.mOffsetY;
    output_overlay->mWidth = input_overlay.mWidth;
    output_overlay->mHeight = input_overlay.mHeight;
    output_overlay->mOverlayWidth = input_overlay.mOverlayWidth;
    output_overlay->mOverlayHeight = input_overlay.mOverlayHeight;
    output_overlay->mfWideOutlineEmpty = input_overlay.mfWideOutlineEmpty;

    double gaussian_blur_strength_x = gaussian_blur_strength*target_scale_x;
    double gaussian_blur_strength_y = gaussian_blur_strength*target_scale_y;

    int gaussian_blur_radius_x = (static_cast<int>( ceil(gaussian_blur_strength_x*3) ) | 1)/2;//fix me: rounding err?    
    int gaussian_blur_radius_y = (static_cast<int>( ceil(gaussian_blur_strength_y*3) ) | 1)/2;//fix me: rounding err?
    if( gaussian_blur_radius_x < 1 && gaussian_blur_strength>GAUSSIAN_BLUR_THREHOLD )
        gaussian_blur_radius_x = 1;//make sure that it really do a blur
    if( gaussian_blur_radius_y < 1 && gaussian_blur_strength>GAUSSIAN_BLUR_THREHOLD )
        gaussian_blur_radius_y = 1;//make sure that it really do a blur

    int bluradjust_x = 0, bluradjust_y = 0;
    if ( IsItReallyBlur(be_strength, gaussian_blur_strength) )
    {
        if (gaussian_blur_strength > 0)
        {
            bluradjust_x += gaussian_blur_radius_x * 8;
            bluradjust_y += gaussian_blur_radius_y * 8;
        }
        if (be_strength)
        {
            int be_adjust_x = static_cast<int>( target_scale_x*std::sqrt(be_strength*0.25f)+0.5 );//fix me: rounding err?
            be_adjust_x *= 8;
            int be_adjust_y = static_cast<int>(target_scale_y*std::sqrt(be_strength*0.25f)+0.5);//fix me: rounding err?
            be_adjust_y *= 8;

            bluradjust_x += be_adjust_x;
            bluradjust_y += be_adjust_y;
        }
        // Expand the buffer a bit when we're blurring, since that can also widen the borders a bit
        bluradjust_x = (bluradjust_x+7)&~7;
        bluradjust_y = (bluradjust_y+7)&~7;

        output_overlay->mOffsetX -= bluradjust_x;
        output_overlay->mOffsetY -= bluradjust_y;
        output_overlay->mWidth += (bluradjust_x<<1);
        output_overlay->mHeight += (bluradjust_y<<1);
        output_overlay->mOverlayWidth += (bluradjust_x>>2);
        output_overlay->mOverlayHeight += (bluradjust_y>>2);
    }
    else
    {
        return false;
    }

    output_overlay->mOverlayPitch = (output_overlay->mOverlayWidth+15)&~15;

    BYTE* body = reinterpret_cast<BYTE*>(xy_malloc(output_overlay->mOverlayPitch * output_overlay->mOverlayHeight));
    if( body==NULL )
    {
        return false;
    }
    output_overlay->mBody.reset(body, xy_free);
    memset(body, 0, output_overlay->mOverlayPitch * output_overlay->mOverlayHeight);
    BYTE* border = NULL;
    if (!output_overlay->mfWideOutlineEmpty)
    {
        border = reinterpret_cast<BYTE*>(xy_malloc(output_overlay->mOverlayPitch * output_overlay->mOverlayHeight));
        if (border==NULL)
        {
            return false;
        }
        output_overlay->mBorder.reset(border, xy_free);
        memset(border, 0, output_overlay->mOverlayPitch * output_overlay->mOverlayHeight);
    }

    //copy buffer
    for(int i = 1; i >= 0; i--)
    {
        byte* plan_selected = i==0 ? body : border;
        const byte* plan_input = i==0 ? input_overlay.mBody.get() : input_overlay.mBorder.get();

        plan_selected += (bluradjust_x>>3) + (bluradjust_y>>3)*output_overlay->mOverlayPitch;
        if ( plan_selected!=NULL && plan_input!=NULL )
        {
            for (int j=0;j<input_overlay.mOverlayHeight;j++)
            {
                memcpy(plan_selected, plan_input, input_overlay.mOverlayPitch);
                plan_selected += output_overlay->mOverlayPitch;
                plan_input += input_overlay.mOverlayPitch;
            }
        }
    }

    ::boost::shared_array<unsigned> tmp_buf( DEBUG_NEW unsigned[max((output_overlay->mOverlayPitch+1)*(output_overlay->mOverlayHeight+1),0)] );
    //flyweight<key_value<int, ass_tmp_buf, ass_tmp_buf_get_size>, no_locking> tmp_buf((overlay->mOverlayWidth+1)*(overlay->mOverlayPitch+1));
    // Do some gaussian blur magic    
    if ( gaussian_blur_strength > GAUSSIAN_BLUR_THREHOLD )
    {
        byte* plan_selected= output_overlay->mfWideOutlineEmpty ? body : border;
        
        flyweight<key_value<double, ass_synth_priv, GaussianFilterKey<ass_synth_priv>>, no_locking>
            fw_priv_blur_x(gaussian_blur_strength_x);
        flyweight<key_value<double, ass_synth_priv, GaussianFilterKey<ass_synth_priv>>, no_locking>
            fw_priv_blur_y(gaussian_blur_strength_y);

        const ass_synth_priv& priv_blur_x = fw_priv_blur_x.get();
        const ass_synth_priv& priv_blur_y = fw_priv_blur_y.get();
        if (output_overlay->mOverlayWidth>=priv_blur_x.g_w && output_overlay->mOverlayHeight>=priv_blur_y.g_w)
        {   
            ass_gauss_blur(plan_selected, tmp_buf.get(), output_overlay->mOverlayWidth, output_overlay->mOverlayHeight, output_overlay->mOverlayPitch, 
                priv_blur_x.gt2, priv_blur_x.g_r, priv_blur_x.g_w,
                priv_blur_y.gt2, priv_blur_y.g_r, priv_blur_y.g_w);
        }
    }

    float scaled_be_strength = be_strength * 0.5f * (target_scale_x+target_scale_y);
    int pass_num = static_cast<int>(scaled_be_strength);
    int pitch = output_overlay->mOverlayPitch;
    byte* blur_plan = output_overlay->mfWideOutlineEmpty ? body : border;

    for (int pass = 0; pass < pass_num; pass++)
    {
        if(output_overlay->mOverlayWidth >= 3 && output_overlay->mOverlayHeight >= 3)
        {
            be_blur(blur_plan, tmp_buf.get(), output_overlay->mOverlayWidth, output_overlay->mOverlayHeight, pitch);
        }
    }
    if (scaled_be_strength>pass_num)
    {
        xy_be_blur(blur_plan, output_overlay->mOverlayWidth, output_overlay->mOverlayHeight, pitch, 
            scaled_be_strength-pass_num, scaled_be_strength-pass_num);
    }

    return true;
}

// @return: true if actually a blur operation has done, or else false and output is leave unset.
bool Rasterizer::Blur(const Overlay& input_overlay, float be_strength, 
    double gaussian_blur_strength, 
    double target_scale_x, double target_scale_y, 
    SharedPtrOverlay output_overlay)
{
    using namespace ::boost::flyweights;

    ASSERT(IsItReallyBlur(be_strength, gaussian_blur_strength));
    if(!output_overlay || !IsItReallyBlur(be_strength, gaussian_blur_strength))
    {
        return false;
    }
    if (input_overlay.mOverlayWidth<=0 || input_overlay.mOverlayHeight<=0)
    {
        return true;
    }

    if (gaussian_blur_strength>0)
    {
        if (be_strength)//this insane thing should NEVER happen
        {
            SharedPtrOverlay tmp(DEBUG_NEW Overlay());

            bool rv = GaussianBlur(input_overlay, gaussian_blur_strength, target_scale_x, target_scale_y, tmp);
            ASSERT(rv);
            rv = BeBlur(*tmp, be_strength, target_scale_x, target_scale_y, output_overlay);
            ASSERT(rv);
        }
        else
        {
            bool rv = GaussianBlur(input_overlay, gaussian_blur_strength, target_scale_x, target_scale_y, output_overlay);
            ASSERT(rv);
        }
    }
    else if (be_strength)
    {
        bool rv = BeBlur(input_overlay, be_strength, target_scale_x, target_scale_y, output_overlay);
        ASSERT(rv);
    }
    return true;
}

bool Rasterizer::GaussianBlur( const Overlay& input_overlay, double gaussian_blur_strength, 
    double target_scale_x, double target_scale_y, 
    SharedPtrOverlay output_overlay )
{
    using namespace ::boost::flyweights;

    ASSERT(output_overlay);
    output_overlay->CleanUp();
    output_overlay->mfWideOutlineEmpty = input_overlay.mfWideOutlineEmpty;

    ASSERT(gaussian_blur_strength > 0);

    double gaussian_blur_strength_x = gaussian_blur_strength*target_scale_x;
    double gaussian_blur_strength_y = gaussian_blur_strength*target_scale_y;

    int gaussian_blur_radius_x = (static_cast<int>( ceil(gaussian_blur_strength_x*3) ) | 1)/2;//fix me: rounding err?    
    int gaussian_blur_radius_y = (static_cast<int>( ceil(gaussian_blur_strength_y*3) ) | 1)/2;//fix me: rounding err?
    if( gaussian_blur_radius_x < 1 && gaussian_blur_strength>GAUSSIAN_BLUR_THREHOLD )
        gaussian_blur_radius_x = 1;//make sure that it really do a blur
    if( gaussian_blur_radius_y < 1 && gaussian_blur_strength>GAUSSIAN_BLUR_THREHOLD )
        gaussian_blur_radius_y = 1;//make sure that it really do a blur

    flyweight<key_value<double, GaussianCoefficients, GaussianFilterKey<GaussianCoefficients>>, no_locking>
        fw_filter_x(gaussian_blur_strength_x);
    flyweight<key_value<double, GaussianCoefficients, GaussianFilterKey<GaussianCoefficients>>, no_locking>
        fw_filter_y(gaussian_blur_strength_y);

    const GaussianCoefficients& filter_x = fw_filter_x.get();
    const GaussianCoefficients& filter_y = fw_filter_y.get();

    int bluradjust_x = filter_x.g_r * 8;
    int bluradjust_y = filter_y.g_r * 8;
    output_overlay->mOffsetX       = input_overlay.mOffsetX - bluradjust_x;
    output_overlay->mOffsetY       = input_overlay.mOffsetY - bluradjust_y;
    output_overlay->mWidth         = input_overlay.mWidth + (bluradjust_x<<1);
    output_overlay->mHeight        = input_overlay.mHeight + (bluradjust_y<<1);
    output_overlay->mOverlayWidth  = input_overlay.mOverlayWidth + (bluradjust_x>>2);
    output_overlay->mOverlayHeight = input_overlay.mOverlayHeight + (bluradjust_y>>2);

    output_overlay->mOverlayPitch = (output_overlay->mOverlayWidth+15)&~15;

    BYTE* blur_plan = reinterpret_cast<BYTE*>(xy_malloc(output_overlay->mOverlayPitch * output_overlay->mOverlayHeight));
    //memset(blur_plan, 0, output_overlay->mOverlayPitch * output_overlay->mOverlayHeight);

    const BYTE* plan_input = input_overlay.mfWideOutlineEmpty ? input_overlay.mBody.get() : input_overlay.mBorder.get();    
    ASSERT(output_overlay->mOverlayWidth>=filter_x.g_w && output_overlay->mOverlayHeight>=filter_y.g_w);
    xy_gaussian_blur(blur_plan, output_overlay->mOverlayPitch, 
        plan_input, input_overlay.mOverlayWidth, input_overlay.mOverlayHeight, input_overlay.mOverlayPitch, 
        filter_x.g_f, filter_x.g_r, filter_x.g_w_ex, 
        filter_y.g_f, filter_y.g_r, filter_y.g_w_ex);
    if (input_overlay.mfWideOutlineEmpty)
    {
        output_overlay->mBody.reset(blur_plan, xy_free);
    }
    else
    {
        output_overlay->mBorder.reset(blur_plan, xy_free);

        BYTE* body = reinterpret_cast<BYTE*>(xy_malloc(output_overlay->mOverlayPitch * output_overlay->mOverlayHeight));
        if( body==NULL )
        {
            return false;
        }        
        output_overlay->mBody.reset(body, xy_free);
        memset(body, 0, output_overlay->mOverlayPitch * (bluradjust_y>>3));
        body += (bluradjust_y>>3)*output_overlay->mOverlayPitch;
        plan_input = input_overlay.mBody.get();
        ASSERT(plan_input);
        for (int j=0;j<input_overlay.mOverlayHeight;j++)
        {
            memset(body, 0, (bluradjust_x>>3));
            memcpy(body+(bluradjust_x>>3), plan_input, input_overlay.mOverlayWidth);
            memset(body+(bluradjust_x>>3)+input_overlay.mOverlayWidth, 0, (bluradjust_x>>3));
            body += output_overlay->mOverlayPitch;
            plan_input += input_overlay.mOverlayPitch;
        }
        memset(body, 0, output_overlay->mOverlayPitch * (bluradjust_y>>3));
    }
    return true;
}

bool Rasterizer::BeBlur( const Overlay& input_overlay, float be_strength, 
    float target_scale_x, float target_scale_y, SharedPtrOverlay output_overlay )
{
    ASSERT(output_overlay);
    output_overlay->CleanUp();
    output_overlay->mfWideOutlineEmpty = input_overlay.mfWideOutlineEmpty;

    ASSERT(be_strength>0 && target_scale_x>0 && target_scale_y>0);
    int bluradjust_x = static_cast<int>( target_scale_x*std::sqrt(be_strength*0.25f)+0.5 );//fix me: rounding err?
    bluradjust_x *= 8;
    int bluradjust_y = static_cast<int>(target_scale_y*std::sqrt(be_strength*0.25f)+0.5);//fix me: rounding err?
    bluradjust_y *= 8;

    output_overlay->mOffsetX       = input_overlay.mOffsetX - bluradjust_x;
    output_overlay->mOffsetY       = input_overlay.mOffsetY - bluradjust_y;
    output_overlay->mWidth         = input_overlay.mWidth + (bluradjust_x<<1);
    output_overlay->mHeight        = input_overlay.mHeight + (bluradjust_y<<1);
    output_overlay->mOverlayWidth  = input_overlay.mOverlayWidth + (bluradjust_x>>2);
    output_overlay->mOverlayHeight = input_overlay.mOverlayHeight + (bluradjust_y>>2);

    output_overlay->mOverlayPitch = (output_overlay->mOverlayWidth+15)&~15;

    BYTE* body = reinterpret_cast<BYTE*>(xy_malloc(output_overlay->mOverlayPitch * output_overlay->mOverlayHeight));
    if( body==NULL )
    {
        return false;
    }
    output_overlay->mBody.reset(body, xy_free);
    memset(body, 0, output_overlay->mOverlayPitch * output_overlay->mOverlayHeight);
    BYTE* border = NULL;
    if (!output_overlay->mfWideOutlineEmpty)
    {
        border = reinterpret_cast<BYTE*>(xy_malloc(output_overlay->mOverlayPitch * output_overlay->mOverlayHeight));
        if (border==NULL)
        {
            return false;
        }
        output_overlay->mBorder.reset(border, xy_free);
        memset(border, 0, output_overlay->mOverlayPitch * output_overlay->mOverlayHeight);
    }

    //copy buffer
    for(int i = 1; i >= 0; i--)
    {
        byte* plan_selected = i==0 ? body : border;
        const byte* plan_input = i==0 ? input_overlay.mBody.get() : input_overlay.mBorder.get();

        plan_selected += (bluradjust_x>>3) + (bluradjust_y>>3)*output_overlay->mOverlayPitch;
        if ( plan_selected!=NULL && plan_input!=NULL )
        {
            for (int j=0;j<input_overlay.mOverlayHeight;j++)
            {
                memcpy(plan_selected, plan_input, input_overlay.mOverlayWidth*sizeof(plan_input[0]));
                plan_selected += output_overlay->mOverlayPitch;
                plan_input += input_overlay.mOverlayPitch;
            }
        }
    }
    if (be_strength<=0)
    {
        return true;
    }

    float scaled_be_strength = be_strength * 0.5f * (target_scale_x+target_scale_y);
    int pass_num = static_cast<int>(scaled_be_strength);
    int pitch = output_overlay->mOverlayPitch;
    byte* blur_plan = output_overlay->mfWideOutlineEmpty ? body : border;
    ::boost::shared_array<unsigned> tmp_buf( DEBUG_NEW unsigned[max((output_overlay->mOverlayPitch+1)*(output_overlay->mOverlayHeight+1),0)] );
    for (int pass = 0; pass < pass_num; pass++)
    {
        if(output_overlay->mOverlayWidth >= 3 && output_overlay->mOverlayHeight >= 3)
        {
            be_blur(blur_plan, tmp_buf.get(), output_overlay->mOverlayWidth, output_overlay->mOverlayHeight, pitch);
        }
    }
    if (scaled_be_strength>pass_num)
    {
        xy_be_blur(blur_plan, output_overlay->mOverlayWidth, output_overlay->mOverlayHeight, pitch, 
            scaled_be_strength-pass_num, scaled_be_strength-pass_num);
    }

    return true;
}

///////////////////////////////////////////////////////////////////////////

static __forceinline void   pixmix_c   (DWORD *dst, DWORD color, DWORD alpha);
static __forceinline void   pixmix_sse2(DWORD *dst, DWORD color, DWORD alpha);

static __forceinline void pixmix_c(DWORD *dst, DWORD color, DWORD alpha)
{
    int a = alpha;
    // Make sure both a and ia are in range 1..256 for the >>8 operations below to be correct
    int ia = 256-a;
    a+=1;
    *dst = ((((*dst&0x00ff00ff)*ia + (color&0x00ff00ff)*a)&0xff00ff00)>>8)
           | ((((*dst&0x0000ff00)*ia + (color&0x0000ff00)*a)&0x00ff0000)>>8)
           | ((((*dst>>8)&0x00ff0000)*ia)&0xff000000);
}

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

///////////////////////////////////////////////////////////////////////////

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
///////////////////////////////////////////////////////////////////////////

static __forceinline void  packed_pix_mix_c   (BYTE* dst, const BYTE* alpha, int w, DWORD color);
static __forceinline void  packed_pix_mix_sse2(BYTE* dst, const BYTE* alpha, int w, DWORD color);

static __forceinline void packed_pix_mix_c(BYTE* dst, const BYTE* alpha, int w, DWORD color)
{
    DWORD * dst_w = (DWORD *)dst;
    for(int wt=0; wt<w; ++wt)
        pixmix_c(&dst_w[wt], color, alpha[wt]);
}

static __forceinline __m128i packed_pix_mix_sse2(const __m128i& dst, 
    const __m128i& c_r, const __m128i& c_g, const __m128i& c_b, const __m128i& a)
{
    __m128i d_a, d_r, d_g, d_b;

    d_a = _mm_srli_epi32(dst, 24);

    d_r = _mm_slli_epi32(dst, 8);
    d_r = _mm_srli_epi32(d_r, 24);

    d_g = _mm_slli_epi32(dst, 16);
    d_g = _mm_srli_epi32(d_g, 24);

    d_b = _mm_slli_epi32(dst, 24);
    d_b = _mm_srli_epi32(d_b, 24);

    //d_a = _mm_or_si128(d_a, c_a);
    d_r = _mm_or_si128(d_r, c_r);
    d_g = _mm_or_si128(d_g, c_g);
    d_b = _mm_or_si128(d_b, c_b);

    d_a = _mm_mullo_epi16(d_a, a);
    d_r = _mm_madd_epi16(d_r, a);
    d_g = _mm_madd_epi16(d_g, a);
    d_b = _mm_madd_epi16(d_b, a);

    d_a = _mm_srli_epi32(d_a, 8);
    d_r = _mm_srli_epi32(d_r, 8);
    d_g = _mm_srli_epi32(d_g, 8);
    d_b = _mm_srli_epi32(d_b, 8);
    
    d_a = _mm_slli_epi32(d_a, 24);
    d_r = _mm_slli_epi32(d_r, 16);
    d_g = _mm_slli_epi32(d_g, 8);
    
    d_b = _mm_or_si128(d_b, d_g);
    d_b = _mm_or_si128(d_b, d_r);
    return _mm_or_si128(d_b, d_a);
}

static __forceinline void packed_pix_mix_sse2(BYTE* dst, const BYTE* alpha, int w, DWORD color)
{
    __m128i c_r = _mm_set1_epi32( (color & 0xFF0000) );
    __m128i c_g = _mm_set1_epi32( (color & 0xFF00)<<8 );
    __m128i c_b = _mm_set1_epi32( (color & 0xFF)<<16 );

    __m128i zero = _mm_setzero_si128();

    __m128i ones = _mm_set1_epi16(0x1);

    const BYTE *alpha_end0 = alpha + (w&~15);
    const BYTE *alpha_end = alpha + w;
    for ( ; alpha<alpha_end0; alpha+=16, dst+=16*4 )
    {
        __m128i a = _mm_loadu_si128(reinterpret_cast<const __m128i*>(alpha));
        __m128i d1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(dst));
        __m128i d2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(dst+16));
        __m128i d3 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(dst+32));
        __m128i d4 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(dst+48));

        __m128i ra;
#ifdef _DEBUG
        ra = _mm_setzero_si128();
#endif // _DEBUG
        ra = _mm_cmpeq_epi32(ra, ra);
        ra = _mm_xor_si128(ra, a);
        __m128i a1 = _mm_unpacklo_epi8(ra, a);
        __m128i a2 = _mm_unpackhi_epi8(a1, zero);
        a1 = _mm_unpacklo_epi8(a1, zero);
        a1 = _mm_add_epi16(a1, ones);
        a2 = _mm_add_epi16(a2, ones);

        __m128i a3 = _mm_unpackhi_epi8(ra, a);
        __m128i a4 = _mm_unpackhi_epi8(a3,zero);
        a3 = _mm_unpacklo_epi8(a3,zero);
        a3 = _mm_add_epi16(a3, ones);
        a4 = _mm_add_epi16(a4, ones);

        d1 = packed_pix_mix_sse2(d1, c_r, c_g, c_b, a1);
        d2 = packed_pix_mix_sse2(d2, c_r, c_g, c_b, a2);
        d3 = packed_pix_mix_sse2(d3, c_r, c_g, c_b, a3);
        d4 = packed_pix_mix_sse2(d4, c_r, c_g, c_b, a4);

        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), d1);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst+16), d2);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst+32), d3);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst+48), d4);
    }
    DWORD * dst_w = reinterpret_cast<DWORD*>(dst);
    for ( ; alpha<alpha_end; alpha++, dst_w++ )
    {
        pixmix_sse2(dst_w, color, *alpha);
    }
}

static __forceinline void  packed_pix_mix_c              (BYTE* dst, BYTE alpha, int w, DWORD color);
static __forceinline void  packed_pix_mix_sse2           (BYTE* dst, BYTE alpha, int w, DWORD color);

static __forceinline void  packed_pix_mix_c              (BYTE* dst, BYTE alpha, int w, DWORD color)
{
    const BYTE *dst_end = dst + 4*w;
    for ( ; dst<dst_end; dst+=4 )
    {
        pixmix_c(reinterpret_cast<DWORD*>(dst), color, alpha);
    }
}

static __forceinline void packed_pix_mix_sse2(BYTE* dst, BYTE alpha, int w, DWORD color)
{
    __m128i c_r = _mm_set1_epi32( (color & 0xFF0000) );
    __m128i c_g = _mm_set1_epi32( (color & 0xFF00)<<8 );
    __m128i c_b = _mm_set1_epi32( (color & 0xFF)<<16 );
    __m128i a = _mm_set1_epi32(((alpha+1) << 16) | (0x100 - alpha));

    __m128i zero = _mm_setzero_si128();

    __m128i ones = _mm_set1_epi16(0x1);

    const BYTE *dst_end0 = dst + ((4*w)&~63);
    const BYTE *dst_end = dst + 4*w;
    for ( ; dst<dst_end0; dst+=16*4 )
    {
        __m128i d1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(dst));
        __m128i d2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(dst+16));
        __m128i d3 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(dst+32));
        __m128i d4 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(dst+48));

        d1 = packed_pix_mix_sse2(d1, c_r, c_g, c_b, a);
        d2 = packed_pix_mix_sse2(d2, c_r, c_g, c_b, a);
        d3 = packed_pix_mix_sse2(d3, c_r, c_g, c_b, a);
        d4 = packed_pix_mix_sse2(d4, c_r, c_g, c_b, a);

        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), d1);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst+16), d2);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst+32), d3);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst+48), d4);
    }
    for ( ; dst<dst_end; dst+=4 )
    {
        pixmix_sse2(reinterpret_cast<DWORD*>(dst), color, alpha);
    }
}

///////////////////////////////////////////////////////////////////////////

#include <mmintrin.h>

// Calculate a - b clamping to 0 instead of underflowing
static __forceinline DWORD safe_subtract(DWORD a, DWORD b)
{
#ifndef _WIN64
    __m64 ap = _mm_cvtsi32_si64(a);
    __m64 bp = _mm_cvtsi32_si64(b);
    __m64 rp = _mm_subs_pu16(ap, bp);
    DWORD r = (DWORD)_mm_cvtsi64_si32(rp);
    _mm_empty();
    return r;
#else
    return (b > a) ? 0 : a - b;
#endif
}

///////////////////////////////////////////////////////////////////////////

static __forceinline void packed_pix_add_c(BYTE* dst, const BYTE* alpha, int w, DWORD color)
{
    DWORD c_r = (color&0x00ff0000);
    DWORD c_g = (color&0x0000ff00);
    DWORD c_b = (color&0x000000ff);
    const BYTE *alpha_end = alpha + w;
    DWORD * dst_w = reinterpret_cast<DWORD*>(dst);
    for ( ; alpha<alpha_end; alpha++, dst_w++ )
    {
        DWORD d_a = ((*dst_w&0xff000000)>>24);
        DWORD d_r = (*dst_w&0x00ff0000);
        DWORD d_g = (*dst_w&0x0000ff00);
        DWORD d_b = (*dst_w&0x000000ff);

        DWORD a = *alpha;
        d_a = d_a >= a ? d_a - a : 0;
        a++;
        d_r += ((c_r*a>>8)&0x00ff0000);
        d_g += ((c_g*a>>8)&0x0000ff00);
        d_b += ((c_b*a>>8)&0x000000ff);
        d_r = d_r<=0x00ff0000 ? d_r:0x00ff0000;
        d_g = d_g<=0x0000ff00 ? d_g:0x0000ff00;
        d_b = d_b<=0x000000ff ? d_b:0x000000ff;

        *dst_w = (d_a<<24) | d_r | d_g | d_b;
    }
}

static __forceinline __m128i packed_pix_add_sse2(const __m128i& dst_argbargbargbargb, 
    const __m128i& _r_b_r_b_r_b_r_b, const __m128i& _a_g_a_g_a_g_a_g, const __m128i& _a_a_a_a_a_a_a_a,
    const __m128i& F___F___F___F___)
{
    __m128i _r_b, a_g_, tmp;

    _r_b = _mm_mullo_epi16(_r_b_r_b_r_b_r_b, _a_a_a_a_a_a_a_a);
    a_g_ = _mm_mullo_epi16(_a_g_a_g_a_g_a_g, _a_a_a_a_a_a_a_a);
    _r_b = _mm_srli_epi16(_r_b, 8);
    a_g_ = _mm_srli_epi16(a_g_, 8);
    a_g_ = _mm_slli_epi16(a_g_, 8);
    a_g_ = _mm_or_si128  (a_g_, _r_b);

    tmp = _mm_xor_si128(dst_argbargbargbargb, F___F___F___F___);
    tmp = _mm_adds_epu8(tmp, a_g_);
    return _mm_xor_si128(tmp, F___F___F___F___);
}

static __forceinline void packed_pix_add_sse2(BYTE* dst, const BYTE* alpha, int w, DWORD color)
{
    DWORD _r_b =   (color & 0xFF00FF);
    DWORD _f_g = (((color & 0x00FF00)>>8)|0xFF0000);

    __m128i _r_b_r_b_r_b_r_b = _mm_set1_epi32( _r_b );
    __m128i _F_g_F_g_F_g_F_g = _mm_set1_epi32( _f_g );
    __m128i F___F___F___F___ = _mm_set1_epi32(0xFF000000);

    __m128i zero = _mm_setzero_si128();

    __m128i ones = _mm_set1_epi16(0x1);

    const BYTE *alpha_end0 = alpha + (w&~15);
    const BYTE *alpha_end = alpha + w;
    for ( ; alpha<alpha_end0; alpha+=16, dst+=16*4 )
    {
        __m128i a  = _mm_loadu_si128(reinterpret_cast<const __m128i*>(alpha));
        __m128i d1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(dst));
        __m128i d2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(dst+16));
        __m128i d3 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(dst+32));
        __m128i d4 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(dst+48));

        __m128i a1, a2, a3, a4;
        a1 = _mm_unpacklo_epi8(a , zero);
        a1 = _mm_adds_epu16   (a1, ones);
        a2 = _mm_unpackhi_epi16(a1, a1);
        a1 = _mm_unpacklo_epi16(a1, a1);

        a3 = _mm_unpackhi_epi8(a , zero);
        a3 = _mm_adds_epu16   (a3, ones);
        a4 = _mm_unpackhi_epi16(a3, a3);
        a3 = _mm_unpacklo_epi16(a3, a3);

        // FF * (a+1) >> 8 = a
        // d? += (a, r*(a+1)>>8, g*(a+1)>>8, b*(a+1)>>8)
        d1 = packed_pix_add_sse2(d1, _r_b_r_b_r_b_r_b, _F_g_F_g_F_g_F_g, a1, F___F___F___F___);
        d2 = packed_pix_add_sse2(d2, _r_b_r_b_r_b_r_b, _F_g_F_g_F_g_F_g, a2, F___F___F___F___);
        d3 = packed_pix_add_sse2(d3, _r_b_r_b_r_b_r_b, _F_g_F_g_F_g_F_g, a3, F___F___F___F___);
        d4 = packed_pix_add_sse2(d4, _r_b_r_b_r_b_r_b, _F_g_F_g_F_g_F_g, a4, F___F___F___F___);

        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst)   , d1);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst+16), d2);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst+32), d3);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst+48), d4);
    }
    DWORD * dst_w = reinterpret_cast<DWORD*>(dst);
    for ( ; alpha<alpha_end; alpha++, dst_w++ )
    {
        int a = *alpha + 1;
        DWORD r_b = ((_r_b * a)&0xff00ff00)>>8;
        DWORD a_g = ((_f_g * a)&0xff00ff00)>>8;// FF * (*alpha+1) >> 8 = alpha
        *dst_w ^= 0xff000000;
        r_b +=  (*dst_w & 0x00ff00ff);
        a_g += ((*dst_w & 0xff00ff00)>>8);
        r_b = r_b ^ ((r_b ^ 0xff00ff) & (((r_b&0xff00ff00)>>8)*0x0FFF));//r < 0xff ? r : 0xff;
        a_g = a_g ^ ((a_g ^ 0xff00ff) & (((a_g&0xff00ff00)>>8)*0x0FFF));
        a_g <<= 8;
        *dst_w = (r_b | a_g);
        *dst_w ^= 0xff000000;
    }
}


///////////////////////////////////////////////////////////////////////////

typedef void (*AlphaBlt8bppFunc)(byte* pY, const byte* am, const byte Y, int h, int w, int src_stride, int dst_stride);
void           AlphaBlt8bppC    (byte* pY, const byte* am, const byte Y, int h, int w, int src_stride, int dst_stride);
void           AlphaBlt8bppSse2 (byte* pY, const byte* am, const byte Y, int h, int w, int src_stride, int dst_stride);

void AlphaBlt8bppC(byte* pY,
    const byte* pAlphaMask, 
    const byte Y, 
    int h, int w, int src_stride, int dst_stride)
{
    for( ; h>0; h--, pAlphaMask += src_stride, pY += dst_stride )
    {
        const BYTE* sa = pAlphaMask;
        BYTE* dy = pY;
        const BYTE* dy_end = pY + w;

        for(;dy < dy_end; sa++, dy++)
        {
            *dy = (*dy * (256 - *sa)+ Y*(*sa+1))>>8;
        }
    }
}

/***
 * No aligned requirement
 * 
 **/
void AlphaBlt8bppSse2(byte* pY,
    const byte* pAlphaMask, 
    const byte Y, 
    int h, int w, int src_stride, int dst_stride)
{   
    __m128i zero = _mm_setzero_si128();
    __m128i s = _mm_set1_epi16(Y);               //s = c  0  c  0  c  0  c  0  c  0  c  0  c  0  c  0    

    __m128i ones;
#ifdef _DEBUG
    ones = _mm_setzero_si128();
#endif // _DEBUG
    ones = _mm_cmpeq_epi32(ones, ones);
    ones = _mm_srli_epi16(ones, 15);
    ones = _mm_slli_epi16(ones, 8);

    if( w>16 )//IMPORTANT! The result of the following code is undefined with w<15.
    {
        for( ; h>0; h--, pAlphaMask += src_stride, pY += dst_stride )
        {
            const BYTE* sa = pAlphaMask;      
            BYTE* dy = pY;
            const BYTE* dy_first_mod16 = reinterpret_cast<BYTE*>((reinterpret_cast<intptr_t>(pY)+15)&~15);  //IMPORTANT! w must >= 15
            const BYTE* dy_end_mod16 = reinterpret_cast<BYTE*>(reinterpret_cast<intptr_t>(pY+w)&~15);
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

                a = _mm_unpacklo_epi8(a,zero);               //a= a0 0  a1 0  a2 0  a3 0  a4 0  a5 0  a6 0  a7 0
                __m128i ia = _mm_sub_epi16(ones,a);         //ia   = 256-a0 ... 256-a7

                __m128i dl = _mm_unpacklo_epi8(d,zero);               //d    = b0 0  b1 0  b2 0  b3 0  b4 0  b5 0  b6 0  b7 0 
                __m128i sl = _mm_mullo_epi16(s,a);            //sl   = c0*a0  c1*a1  ... c7*a7
                sl = _mm_add_epi16(sl,s);

                dl = _mm_mullo_epi16(dl,ia);                   //d    = b0*~a0 b1*~a1 ... b7*~a7

                dl = _mm_add_epi16(dl,sl);                     //d   = (256-a)*d + s + a*s
                dl = _mm_srli_epi16(dl,8);                    //d   = d>>8

                sa += 8;
                a = _mm_loadl_epi64((__m128i*)sa);

                a = _mm_unpacklo_epi8(a,zero);
                ia = _mm_sub_epi16(ones,a);

                d = _mm_unpackhi_epi8(d,zero);
                sl = _mm_mullo_epi16(s,a);
                sl = _mm_add_epi16(sl,s);

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
    }
    else
    {
        AlphaBlt8bppC(pY, pAlphaMask, Y, h, w, src_stride, dst_stride);
    }
}

typedef 
void (*AlphaBlt8bppSingleAlphaFunc)(byte* pY, const byte alpha, const byte Y, int h, int w, int dst_stride);
void   AlphaBlt8bppSingleAlphaC    (byte* pY, const byte alpha, const byte Y, int h, int w, int dst_stride);
void   AlphaBlt8bppSingleAlphaSse2 (byte* pY, const byte alpha, const byte Y, int h, int w, int dst_stride);

void AlphaBlt8bppSingleAlphaC(byte* pY,
    const byte alpha, 
    const byte Y, 
    int h, int w, int dst_stride)
{   
    int yPremul = Y*(alpha+1);
    int dstAlpha = 0x100 - alpha;

    for( ; h>0; h--, pY += dst_stride )
    {
        BYTE* dy = pY;
        const BYTE* dy_end = pY + w;

        for(;dy < dy_end; dy++)
        {
            *dy = (*dy * dstAlpha + yPremul)>>8;
        }
    }
}

/***
 * No aligned requirement
 * 
 **/
void AlphaBlt8bppSingleAlphaSse2(byte* pY,
    const byte alpha, 
    const byte Y, 
    int h, int w, int dst_stride)
{   
    int yPremul = Y*(alpha+1);
    int dstAlpha = 0x100 - alpha;
    if( w>32 )//IMPORTANT! The result of the following code is undefined with w<15.
    {
        __m128i zero = _mm_setzero_si128();
        __m128i s = _mm_set1_epi16(yPremul);    //s = c  0  c  0  c  0  c  0  c  0  c  0  c  0  c  0            
        __m128i ia = _mm_set1_epi16(dstAlpha);
        for( ; h>0; h--, pY += dst_stride )
        {   
            BYTE* dy = pY;
            const BYTE* dy_first_mod16 = reinterpret_cast<BYTE*>((reinterpret_cast<intptr_t>(pY)+15)&~15);  //IMPORTANT! w must >= 15
            const BYTE* dy_end_mod16 = reinterpret_cast<BYTE*>(reinterpret_cast<intptr_t>(pY+w)&~15);
            const BYTE* dy_end = pY + w;   

            for(;dy < dy_first_mod16; dy++)
            {
                *dy = (*dy * dstAlpha + yPremul)>>8;
            }
            for(; dy < dy_end_mod16; dy+=16)
            {
                //Y
                __m128i d = _mm_load_si128(reinterpret_cast<const __m128i*>(dy));
                __m128i dl = _mm_unpacklo_epi8(d,zero);        //d    = b0 0  b1 0  b2 0  b3 0  b4 0  b5 0  b6 0  b7 0                 

                dl = _mm_mullo_epi16(dl,ia);                   //d    = b0*~a0 b1*~a1 ... b7*~a7
                dl = _mm_adds_epu16(dl,s);                     //d   = d + s
                dl = _mm_srli_epi16(dl, 8);                    //d   = d>>8
                
                d = _mm_unpackhi_epi8(d,zero);                
                d = _mm_mullo_epi16(d,ia);
                d = _mm_adds_epu16(d,s);
                d = _mm_srli_epi16(d, 8);

                dl = _mm_packus_epi16(dl,d);
                
                _mm_store_si128(reinterpret_cast<__m128i*>(dy), dl);
            }
            for(;dy < dy_end; dy++)
            {
                *dy = (*dy * dstAlpha + yPremul)>>8;
            }
        }
    }
    else
    {
        AlphaBlt8bppSingleAlphaC(pY, alpha, Y, h, w, dst_stride);
    }
}

///////////////////////////////////////////////////////////////////////////

// For CPUID usage in Rasterizer::Draw
#include "../dsutil/vd.h"

void OverlapRegion(tSpanBuffer& dst, const tSpanBuffer& src, int dx, int dy)
{
    tSpanBuffer temp;
    temp.reserve(dst.size() + src.size());
    dst.swap(temp);
    tSpanBuffer::iterator itA = temp.begin();
    tSpanBuffer::iterator itAE = temp.end();
    tSpanBuffer::const_iterator itB = src.begin();
    tSpanBuffer::const_iterator itBE = src.end();
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
                while (itA != itAE && (*itA).first <= x2) {
                    x2 = _MAX(x2, (*itA).second);
                    ++itA;
                }

                // If we run out of B spans or the B span doesn't overlap,
                // then the next A span can't either (because A spans don't
                // overlap) and we exit.
                if(itB == itBE || (*itB).first + offset1 > x2)
                    break;
                do {x2 = _MAX(x2, (*itB).second + offset2); ++itB;}
                while(itB != itBE && (*itB).first + offset1 <= x2);
                // If we run out of A spans or the A span doesn't overlap,
                // then the next B span can't either (because B spans don't
                // overlap) and we exit.

                if (itA == itAE || (*itA).first > x2)
                    break;
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
                while (itB != itBE && (*itB).first + offset1 <= x2) {
                    x2 = _MAX(x2, (*itB).second + offset2);
                    ++itB;
                }

                // If we run out of A spans or the A span doesn't overlap,
                // then the next B span can't either (because B spans don't
                // overlap) and we exit.

                if(itA == itAE || (*itA).first > x2)
                    break;
                do {x2 = _MAX(x2, (*itA).second); ++itA;}
                while(itA != itAE && (*itA).first <= x2);
                // If we run out of B spans or the B span doesn't overlap,
                // then the next A span can't either (because A spans don't
                // overlap) and we exit.
                if(itB == itBE || (*itB).first + offset1 > x2)
                    break;
            }
            // Flush span.
            dst.push_back(tSpan(x1, x2));
        }
    }
    // Copy over leftover spans.
    while(itA != itAE)
        dst.push_back(*itA);
        ++itA;
    while(itB != itBE)
    {
        unsigned __int64 x1 = (*itB).first + offset1;
        unsigned __int64 x2 = (*itB).second + offset2;
        ++itB;
        while (itB != itBE && (*itB).first + offset1 <= x2) {
            x2 = _MAX(x2, (*itB).second + offset2);
            ++itB;
        }
        dst.push_back(tSpan(x1, x2));
    }
}


SharedPtrByte Rasterizer::CompositeAlphaMask(const SharedPtrOverlay& overlay, const CRect& clipRect, 
    const GrayImage2* alpha_mask, 
    int xsub, int ysub, const DWORD* switchpts, bool fBody, bool fBorder, 
    CRect *outputDirtyRect)
{
    //fix me: check and log error
    SharedPtrByte result;
    *outputDirtyRect = CRect(0, 0, 0, 0);
    if (!switchpts || !fBody && !fBorder) return result;
    if (fBorder && !overlay->mBorder) return result;

    CRect r = clipRect;
    if (alpha_mask!=NULL)
    {
        r &= CRect(alpha_mask->left_top, alpha_mask->size);
    }

    // Remember that all subtitle coordinates are specified in 1/8 pixels
    // (x+4)>>3 rounds to nearest whole pixel.
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
    if(w <= 0 || h <= 0) return(result);
    outputDirtyRect->SetRect(x, y, x+w, y+h);

    bool fSingleColor = (switchpts[1]==0xffffffff);

    // draw
    // Grab the first colour
    DWORD color = switchpts[0];
    byte* s_base = (byte*)xy_malloc(overlay->mOverlayPitch * overlay->mOverlayHeight);
    const byte* alpha_mask_data = alpha_mask != NULL ? alpha_mask->data.get() : NULL;
    const int alpha_mask_pitch = alpha_mask != NULL ? alpha_mask->pitch : 0;
    if(alpha_mask_data!=NULL )
        alpha_mask_data += alpha_mask->pitch * y + x - alpha_mask->left_top.y*alpha_mask->pitch - alpha_mask->left_top.x;

    if(fSingleColor)
    {
        overlay->FillAlphaMash(s_base, fBody, fBorder, xo, yo, w, h, 
            alpha_mask_data, alpha_mask_pitch,
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
                alpha_mask_data, alpha_mask_pitch,
                alpha );   
            last_x = new_x;
            sw += 2;
        }
    }
    result.reset( s_base, xy_free );
    return result;
}


///////////////////////////////////////////////////////////////////////////

void DrawSingleColorPackPix(      DWORD *dst,
                            const BYTE  *src, 
                            DWORD        color, 
                            int          w, 
                            int          h,
                            int          src_pitch,
                            int          dst_pitch)
{
    while(h--)
    {
        packed_pix_mix_sse2((BYTE*)dst, src, w, color);
        src += src_pitch;
        dst = (unsigned long *)((char *)dst + dst_pitch);
    }
}


///////////////////////////////////////////////////////////////////////////

void DrawMultiColorPackPix(     DWORD *dst,
                          const BYTE  *src, 
                          const DWORD *switchpts, 
                          int          xo, /*fix me: remove this parameter*/
                          int          w, 
                          int          h,
                          int          src_pitch,
                          int          dst_pitch)
{
    DWORD color = switchpts[0];
    while(h--)
    {
        const DWORD *sw = switchpts;
        for(int wt=0; wt<w; ++wt)
        {
            if(wt+xo >= sw[1]) {while(wt+xo >= sw[1]) sw += 2; color = sw[-2];}
            pixmix_sse2(&dst[wt], color, src[wt]);
        }
        src += src_pitch;
        dst = (DWORD *)((BYTE *)dst + dst_pitch);
    }
}

// 
// draw overlay[clipRect] to bitmap[0,0,w,h]
// 
void Rasterizer::Draw(XyBitmap* bitmap, SharedPtrOverlay overlay, const CRect& clipRect, byte* s_base, 
    int xsub, int ysub, const DWORD* switchpts, bool fBody, bool fBorder)
{
    if (!switchpts || !fBody && !fBorder) return;
    if (bitmap==NULL)
    {
        ASSERT(0);
        return;
    }
    // clip
    // Limit drawn area to rectangular clip area
    CRect r = clipRect;
    // Remember that all subtitle coordinates are specified in 1/8 pixels
    // (x+4)>>3 rounds to nearest whole pixel.
    int overlayPitch = overlay->mOverlayPitch;
    int x = (xsub + overlay->mOffsetX + 4)>>3;
    int y = (ysub + overlay->mOffsetY + 4)>>3;
    int w = overlay->mOverlayWidth;
    int h = overlay->mOverlayHeight;
    int xo = 0, yo = 0;

    if(x < r.left) {xo = r.left-x; w -= r.left-x; x = r.left;}
    if(y < r.top) {yo = r.top-y; h -= r.top-y; y = r.top;}
    if(x+w > r.right) w = r.right-x;
    if(y+h > r.bottom) h = r.bottom-y;
    // Check if there's actually anything to render
    if (w <= 0 || h <= 0) return;
    // must have enough space to draw into
    ASSERT(x >= bitmap->x && y >= bitmap->y && x+w <= bitmap->x + bitmap->w && y+h <= bitmap->y + bitmap->h );

    bool fSingleColor = (switchpts[1]==0xffffffff);
    bool PLANAR = (bitmap->type==XyBitmap::PLANNA);
    int draw_method = 0;
    if(fSingleColor)
        draw_method |= DM::SINGLE_COLOR;
    if(PLANAR)
        draw_method |= DM::AYUV_PLANAR;

    // draw
    // Grab the first colour
    DWORD color = switchpts[0];
    const byte* s = s_base + overlay->mOverlayPitch*yo + xo;
    
    int dst_offset = 0;
    if (bitmap->type==XyBitmap::PLANNA)
        dst_offset = bitmap->pitch*(y-bitmap->y) + x - bitmap->x;
    else
        dst_offset = bitmap->pitch*(y-bitmap->y) + (x - bitmap->x)*4;
    unsigned long* dst = (unsigned long*)((BYTE*)bitmap->plans[0] + dst_offset);

    // Every remaining line in the bitmap to be rendered...
    switch(draw_method)
    {
    case   DM::SINGLE_COLOR | 0*DM::AYUV_PLANAR :
    {
        DrawSingleColorPackPix(dst, s, color, w, h, overlayPitch, bitmap->pitch);
    }
    break;
    case 0*DM::SINGLE_COLOR | 0*DM::AYUV_PLANAR :
    {
        DrawMultiColorPackPix(dst, s, switchpts, xo, w, h, overlayPitch, bitmap->pitch);
    }
    break;
    case   DM::SINGLE_COLOR |   DM::AYUV_PLANAR :
    {
        unsigned char* dst_A = bitmap->plans[0] + dst_offset;
        unsigned char* dst_Y = bitmap->plans[1] + dst_offset;
        unsigned char* dst_U = bitmap->plans[2] + dst_offset;
        unsigned char* dst_V = bitmap->plans[3] + dst_offset;

        AlphaBlt8bppSse2(dst_Y, s, ((color)>>16)&0xff, h, w, overlayPitch, bitmap->pitch);
        AlphaBlt8bppSse2(dst_U, s, ((color)>>8 )&0xff, h, w, overlayPitch, bitmap->pitch);
        AlphaBlt8bppSse2(dst_V, s, ((color)    )&0xff, h, w, overlayPitch, bitmap->pitch);
        AlphaBlt8bppSse2(dst_A, s,                  0, h, w, overlayPitch, bitmap->pitch);
    }
    break;
    case 0*DM::SINGLE_COLOR |   DM::AYUV_PLANAR :
    {
        unsigned char* dst_A = bitmap->plans[0] + dst_offset;
        unsigned char* dst_Y = bitmap->plans[1] + dst_offset;
        unsigned char* dst_U = bitmap->plans[2] + dst_offset;
        unsigned char* dst_V = bitmap->plans[3] + dst_offset;

        const DWORD *sw = switchpts;
        int last_x = xo;
        color = sw[0];
        while(last_x<w+xo)
        {
            int new_x = sw[3] < w+xo ? sw[3] : w+xo;
            color = sw[0];
            sw += 2;
            if( new_x < last_x )
                continue;
            AlphaBlt8bppSse2(dst_Y, s + last_x - xo, (color>>16)&0xff, h, new_x-last_x, overlayPitch, bitmap->pitch);
            AlphaBlt8bppSse2(dst_U, s + last_x - xo, (color>>8 )&0xff, h, new_x-last_x, overlayPitch, bitmap->pitch);
            AlphaBlt8bppSse2(dst_V, s + last_x - xo, (color    )&0xff, h, new_x-last_x, overlayPitch, bitmap->pitch);
            AlphaBlt8bppSse2(dst_A, s + last_x - xo,                0, h, new_x-last_x, overlayPitch, bitmap->pitch);

            dst_A += new_x - last_x;
            dst_Y += new_x - last_x;
            dst_U += new_x - last_x;
            dst_V += new_x - last_x;
            last_x = new_x;
        }
    }
    break;
    }
    return;
}

void Rasterizer::FillSolidRect(SubPicDesc& spd, int x, int y, int nWidth, int nHeight, DWORD argb)
{
    bool AYUV_PLANAR = (spd.type==MSP_AYUV_PLANAR);
    int draw_method = 0;
    if(AYUV_PLANAR)
        draw_method |= DM::AYUV_PLANAR;

    switch (draw_method)
    {
    case 0*DM::AYUV_PLANAR :
    {
        BYTE *dst = ((BYTE*)spd.bits + spd.pitch * y) + x*4;
        for (int wy=y; wy<y+nHeight; wy++) {
            packed_pix_mix_sse2(dst, argb>>24, nWidth, argb);
            dst += spd.pitch;
        }
    }
    break;
    case   DM::AYUV_PLANAR :
    {
        BYTE* dst = reinterpret_cast<BYTE*>(spd.bits) + spd.pitch * y + x;
        BYTE* dst_A = dst;
        BYTE* dst_Y = dst_A + spd.pitch*spd.h;
        BYTE* dst_U = dst_Y + spd.pitch*spd.h;
        BYTE* dst_V = dst_U + spd.pitch*spd.h;
        AlphaBlt8bppSingleAlphaSse2(dst_Y, argb>>24, ((argb)>>16)&0xff, nHeight, nWidth, spd.pitch);
        AlphaBlt8bppSingleAlphaSse2(dst_U, argb>>24, ((argb)>>8)&0xff , nHeight, nWidth, spd.pitch);
        AlphaBlt8bppSingleAlphaSse2(dst_V, argb>>24, ((argb))&0xff    , nHeight, nWidth, spd.pitch);
        AlphaBlt8bppSingleAlphaSse2(dst_A, argb>>24,                 0, nHeight, nWidth, spd.pitch);
    }
    break;
    }
}

///////////////////////////////////////////////////////////////

typedef void (*MixLineFunc)(BYTE* dst, const BYTE* alpha, int w, DWORD color);

typedef void (*DoAdditionDrawFunc)(BYTE* dst, const BYTE* src, DWORD color, int w, int h, int src_pitch, int dst_pitch);
typedef void (*DoAdditionDrawMultiColorFunc)(      BYTE  *dst, 
                                             const BYTE  *src, 
                                             const DWORD *switchpts, 
                                             int          w, 
                                             int          h,
                                             int          src_pitch,
                                             int          dst_pitch);

void DoAdditionDraw(BYTE* dst, const BYTE* src, DWORD color, int w, int h, int src_pitch, int dst_pitch)
{
    while(h--)
    {
        packed_pix_add_sse2( dst, src, w, color );
        src += src_pitch;
        dst += dst_pitch;
    }
}

void DoAdditionDrawMultiColor(      BYTE  *dst, 
                              const BYTE  *src, 
                              const DWORD *switchpts, 
                              int          w, 
                              int          h,
                              int          src_pitch,
                              int          dst_pitch)
{
    ASSERT(w>=0);
    while(h--)
    {
        const DWORD *sw = switchpts;
        DWORD *dst1 = (DWORD *)dst;
        const BYTE *src1 = src;
        DWORD last_x = switchpts[1];
        while(last_x<(DWORD)w)
        {
            DWORD new_x = sw[3] < (DWORD)w ? sw[3] : (DWORD)w;
            DWORD color = sw[0];
            sw += 2;
            if( new_x < last_x )
                continue;
            packed_pix_add_sse2((BYTE*)dst1, src1, new_x-last_x, color);
            dst1 += new_x - last_x;
            src1 += new_x - last_x;
            last_x = new_x;
        }
        dst += dst_pitch;
        src += src_pitch;
    }
}

// 
// draw overlay[clipRect] to bitmap[0,0,w,h] using addition instead of alphablend, i.e. bitmap += overlay
// 
void Rasterizer::AdditionDraw(XyBitmap         *bitmap   , 
                              SharedPtrOverlay  overlay  , 
                              const CRect      &clipRect , 
                              byte             *s_base   , 
                              int               xsub     ,
                              int               ysub     ,
                              const DWORD      *switchpts, 
                              bool              fBody    , 
                              bool              fBorder)
{
    if (!switchpts || !fBody && !fBorder) return;
    if (bitmap==NULL)
    {
        ASSERT(0);
        return;
    }
    // clip
    // Limit drawn area to rectangular clip area
    CRect r = clipRect;
    // Remember that all subtitle coordinates are specified in 1/8 pixels
    // (x+4)>>3 rounds to nearest whole pixel.
    int overlayPitch =  overlay->mOverlayPitch;
    int x            = (overlay->mOffsetX + xsub + 4)>>3;
    int y            = (overlay->mOffsetY + ysub + 4)>>3;
    int w            =  overlay->mOverlayWidth;
    int h            =  overlay->mOverlayHeight;
    int xo = 0, yo = 0;

    if(x < r.left) {xo = r.left-x; w -= r.left-x; x = r.left;}
    if(y < r.top ) {yo = r.top -y; h -= r.top -y; y = r.top ;}
    if(x+w > r.right ) w = r.right -x;
    if(y+h > r.bottom) h = r.bottom-y;
    // Check if there's actually anything to render
    if (w <= 0 || h <= 0) return;
    // must have enough space to draw into
    if (!(x >= bitmap->x && y >= bitmap->y && x+w <= bitmap->x + bitmap->w && y+h <= bitmap->y + bitmap->h))
    {
        XY_LOG_FATAL("Overflow!");
        ASSERT(0);
    }

    bool fSingleColor = (switchpts[1]==0xffffffff);
    bool PLANAR = (bitmap->type==XyBitmap::PLANNA);

    // draw
    // Grab the first colour
    DWORD color = switchpts[0];
    const byte* s = s_base + overlay->mOverlayPitch*yo + xo;

    int dst_offset = 0;
    if (bitmap->type==XyBitmap::PLANNA)
        dst_offset = bitmap->pitch*(y-bitmap->y) +  x - bitmap->x;
    else
        dst_offset = bitmap->pitch*(y-bitmap->y) + (x - bitmap->x)*4;
    BYTE* dst = (BYTE*)bitmap->plans[0] + dst_offset;

    if (PLANAR)
    {
        ASSERT(0);
        XY_LOG_FATAL("Not Supported yet");
        return;
    }
    if (fSingleColor)
    {
        DoAdditionDraw(dst, s, color, w, h, overlayPitch, bitmap->pitch);
    }
    else
    {
        const int MAX_COLOR_NUM = 4;
        DWORD new_switchpts[MAX_COLOR_NUM*2];
        new_switchpts[0] = switchpts[0];
        new_switchpts[1] = 0;
        int i=1;
        for (;i<MAX_COLOR_NUM;i++)
        {
            new_switchpts[2*i] = switchpts[2*i];
            new_switchpts[2*i+1] = switchpts[2*i+1] > xo ? switchpts[2*i+1] - xo : 0;
            if (new_switchpts[2*i+1]>= w)
            {
                new_switchpts[2*i+1] = w;
                break;
            }
        }
        if (i==MAX_COLOR_NUM)
        {
            XY_LOG_WARN("Unexpected!");
        }
        DoAdditionDrawMultiColor(dst, s, new_switchpts, w, h, overlayPitch, bitmap->pitch);
    }
    return;
}

///////////////////////////////////////////////////////////////

// Overlay

void FillAlphaMashBody_c   (BYTE* dst, const BYTE* src, int color_alpha, int w, int h, int pitch);
void FillAlphaMashBody_sse2(BYTE* dst, const BYTE* src, int color_alpha, int w, int h, int pitch);

void FillAlphaMashBorder_c   (BYTE* dst, const BYTE* border, const BYTE* body, int color_alpha, int w, int h, int pitch);
void FillAlphaMashBorder_sse2(BYTE* dst, const BYTE* border, const BYTE* body, int color_alpha, int w, int h, int pitch);

void FillAlphaMashBodyMasked_c   (BYTE* dst, const BYTE* src, int color_alpha, int w, int h, int pitch,
    const BYTE* am, int am_pitch);
void FillAlphaMashBodyMasked_sse2(BYTE* dst, const BYTE* src, int color_alpha, int w, int h, int pitch,
    const BYTE* am, int am_pitch);

void FillAlphaMashBorderMasked_c   (BYTE* dst, const BYTE* border, const BYTE* body, int color_alpha, int w, int h, int pitch,
    const BYTE* am, int am_pitch);
void FillAlphaMashBorderMasked_sse2(BYTE* dst, const BYTE* border, const BYTE* body, int color_alpha, int w, int h, int pitch,
    const BYTE* am, int am_pitch);

void FillAlphaMashBody_c(BYTE* dst, const BYTE* src, int color_alpha, int w, int h, int pitch)
{
    while(h--)
    {
        int j=0;
        for( ; j<w; j++ )
        {
            dst[j] = ((src[j] * color_alpha)+32)>>6;
        }
        src += pitch;
        dst += pitch;
    }
}

void FillAlphaMashBody_sse2(BYTE* dst, const BYTE* src, int color_alpha, int w, int h, int pitch)
{
#ifndef _WIN64
    const int ROUDN_ERR = 1<<(6-1);

    const int x0 = ((reinterpret_cast<int>(dst)+3)&~3) - reinterpret_cast<int>(dst) < w ?
                   ((reinterpret_cast<int>(dst)+3)&~3) - reinterpret_cast<int>(dst) : w; //IMPORTANT! Should not exceed w.
    const int x00 = ((reinterpret_cast<int>(dst)+15)&~15) - reinterpret_cast<int>(dst) < w ?
                    ((reinterpret_cast<int>(dst)+15)&~15) - reinterpret_cast<int>(dst) : w;//IMPORTANT! Should not exceed w.
    const int x_end00  = ((reinterpret_cast<int>(dst)+w)&~15) - reinterpret_cast<int>(dst);
    const int x_end0 = ((reinterpret_cast<int>(dst)+w)&~3) - reinterpret_cast<int>(dst);
    const int x_end = w;
    __m64   color_alpha_64  = _mm_set1_pi16 (color_alpha);
    __m64   round_err_64    = _mm_set1_pi16 (ROUDN_ERR);
    __m128i color_alpha_128 = _mm_set1_epi16(color_alpha);
    __m128i round_err_128   = _mm_set1_epi16(ROUDN_ERR);
    while(h--)
    {
        int j=0;
        for( ; j<x0; j++ )
        {
            dst[j] = ((src[j] * color_alpha)+ROUDN_ERR)>>6;
        }
        for( ;j<x00;j+=4 )
        {
            __m64 src1 = _mm_cvtsi32_si64(*reinterpret_cast<const int*>(src+j));
            __m64 zero = _mm_setzero_si64();
            src1 = _mm_unpacklo_pi8(src1, zero);
            src1 = _mm_mullo_pi16(src1, color_alpha_64);
            src1 = _mm_adds_pu16(src1, round_err_64);
            src1 = _mm_srli_pi16(src1, 6);
            src1 = _mm_packs_pu16(src1,src1);
            *reinterpret_cast<int*>(dst+j) = _mm_cvtsi64_si32(src1);
        }
        __m128i zero = _mm_setzero_si128();
        for( ;j<x_end00;j+=16)
        {
            __m128i src1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src+j));
            __m128i srchi = src1;
            src1 = _mm_unpacklo_epi8(src1, zero);
            srchi = _mm_unpackhi_epi8(srchi, zero);
            src1 = _mm_mullo_epi16(src1, color_alpha_128);
            srchi = _mm_mullo_epi16(srchi, color_alpha_128);
            src1 = _mm_adds_epu16(src1, round_err_128);
            src1 = _mm_srli_epi16(src1, 6);
            srchi = _mm_adds_epu16(srchi, round_err_128);
            srchi = _mm_srli_epi16(srchi, 6);
            src1 = _mm_packus_epi16(src1, srchi);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(dst+j), src1);
        }
        for( ;j<x_end0;j+=4)
        {
            __m64 src1 = _mm_cvtsi32_si64(*reinterpret_cast<const int*>(src+j));
            __m64 zero = _mm_setzero_si64();
            src1 = _mm_unpacklo_pi8(src1, zero);
            src1 = _mm_mullo_pi16(src1, color_alpha_64);
            src1 = _mm_adds_pu16(src1, round_err_64);
            src1 = _mm_srli_pi16(src1, 6);
            src1 = _mm_packs_pu16(src1,src1);
            *reinterpret_cast<int*>(dst+j) = _mm_cvtsi64_si32(src1);
        }
        for( ;j<x_end;j++)
        {
            dst[j] = ((src[j] * color_alpha)+ROUDN_ERR)>>6;
        }
        src += pitch;
        dst += pitch;
    }
    _mm_empty();
#else
    FillAlphaMashBody_c(dst, src, color_alpha, w, h, pitch);
#endif
}

void FillAlphaMashBorder_c(BYTE* dst, const BYTE* border, const BYTE* body, int color_alpha, int w, int h, int pitch)
{
    while(h--)
    {
        int j=0;
        for( ;j<w;j++)
        {
            int temp = border[j]-body[j];
            temp = temp<0 ? 0 : temp;
            dst[j] = ((temp * color_alpha)+32)>>6;
        }
        body   += pitch;
        border += pitch;
        dst    += pitch;
    }
}

void FillAlphaMashBorder_sse2( BYTE* dst, const BYTE* border, const BYTE* body, int color_alpha, int w, int h, int pitch )
{
#ifndef _WIN64
    const int ROUDN_ERR = 1<<(6-1);

    const int x0 = ((reinterpret_cast<int>(dst)+3)&~3) - reinterpret_cast<int>(dst) < w ?
                   ((reinterpret_cast<int>(dst)+3)&~3) - reinterpret_cast<int>(dst) : w; //IMPORTANT! Should not exceed w.
    const int x00 = ((reinterpret_cast<int>(dst)+15)&~15) - reinterpret_cast<int>(dst) < w ?
                    ((reinterpret_cast<int>(dst)+15)&~15) - reinterpret_cast<int>(dst) : w;//IMPORTANT! Should not exceed w.
    const int x_end00  = ((reinterpret_cast<int>(dst)+w)&~15) - reinterpret_cast<int>(dst);
    const int x_end0 = ((reinterpret_cast<int>(dst)+w)&~3) - reinterpret_cast<int>(dst);
    const int x_end = w;
    __m64   color_alpha_64  = _mm_set1_pi16 (color_alpha);
    __m64   round_err_64    = _mm_set1_pi16 (ROUDN_ERR);
    __m128i color_alpha_128 = _mm_set1_epi16(color_alpha);
    __m128i round_err_128   = _mm_set1_epi16(ROUDN_ERR);
    while(h--)
    {
        int j=0;
        for( ; j<x0; j++ )
        {
            int temp = border[j]-body[j];
            temp = temp<0 ? 0 : temp;
            dst[j] = ((temp * color_alpha)+ROUDN_ERR)>>6;
        }
        for( ;j<x00;j+=4 )
        {
            __m64 border1 = _mm_cvtsi32_si64(*reinterpret_cast<const int*>(border+j));
            __m64 body1 = _mm_cvtsi32_si64(*reinterpret_cast<const int*>(body+j));
            border1 = _mm_subs_pu8(border1, body1);
            __m64 zero = _mm_setzero_si64();
            border1 = _mm_unpacklo_pi8(border1, zero);
            border1 = _mm_mullo_pi16(border1, color_alpha_64);
            border1 = _mm_adds_pu16(border1, round_err_64);
            border1 = _mm_srli_pi16(border1, 6);
            border1 = _mm_packs_pu16(border1,border1);
            *reinterpret_cast<int*>(dst+j) = _mm_cvtsi64_si32(border1);
        }
        __m128i zero = _mm_setzero_si128();
        for( ;j<x_end00;j+=16)
        {
            __m128i border1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(border+j));
            __m128i body1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(body+j));
            border1 = _mm_subs_epu8(border1,body1);
            __m128i srchi = border1;   
            border1 = _mm_unpacklo_epi8(border1, zero);
            srchi   = _mm_unpackhi_epi8(srchi  , zero);
            border1 = _mm_mullo_epi16(border1, color_alpha_128);
            srchi   = _mm_mullo_epi16(srchi  , color_alpha_128);
            border1 = _mm_adds_epu16(border1, round_err_128);
            srchi   = _mm_adds_epu16(srchi  , round_err_128);
            border1 = _mm_srli_epi16(border1, 6);
            srchi   = _mm_srli_epi16(srchi  , 6);
            border1 = _mm_packus_epi16(border1, srchi);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(dst+j), border1);
        }
        for( ;j<x_end0;j+=4)
        {
            __m64 border1 = _mm_cvtsi32_si64(*reinterpret_cast<const int*>(border+j));
            __m64 body1 = _mm_cvtsi32_si64(*reinterpret_cast<const int*>(body+j));
            border1 = _mm_subs_pu8(border1, body1);
            __m64 zero = _mm_setzero_si64();
            border1 = _mm_unpacklo_pi8(border1, zero);
            border1 = _mm_mullo_pi16(border1, color_alpha_64);
            border1 = _mm_adds_pu16(border1, round_err_64);
            border1 = _mm_srli_pi16(border1, 6);
            border1 = _mm_packs_pu16(border1,border1);
            *reinterpret_cast<int*>(dst+j) = _mm_cvtsi64_si32(border1);
        }
        for( ;j<x_end;j++)
        {
            int temp = border[j]-body[j];
            temp = temp<0 ? 0 : temp;
            dst[j] = ((temp * color_alpha)+ROUDN_ERR)>>6;
        }
        body  += pitch;
        border += pitch;
        dst    += pitch;
    }
    _mm_empty();
#else
    FillAlphaMashBorder_c(dst, border, body, color_alpha, w, h, pitch);
#endif
}

void FillAlphaMashBodyMasked_c(       BYTE* dst        , 
                                const BYTE* src        , 
                                int         color_alpha, 
                                int         w          , 
                                int         h          , 
                                int         pitch      , 
                                const BYTE* am         , 
                                int         am_pitch)
{
    const int ROUND_ERR = 1<<(12-1);
    while(h--)
    {
        int j=0;
        for( ; j<w; j++ )
        {
            dst[j] = ((src[j] * am[j] * color_alpha)+ROUND_ERR)>>12;
        }
        src += pitch;
        am  += am_pitch;
        dst += pitch;
    }
}

void FillAlphaMashBodyMasked_sse2(       BYTE* dst        , 
                                   const BYTE* src        , 
                                   int         color_alpha, 
                                   int         w          , 
                                   int         h          , 
                                   int         pitch      , 
                                   const BYTE* am         , 
                                   int         am_pitch)
{
#ifndef _WIN64
    const int ROUND_ERR = 1<<(12-1);

    const int x0 = ((reinterpret_cast<int>(dst)+3)&~3) - reinterpret_cast<int>(dst) < w ?
                   ((reinterpret_cast<int>(dst)+3)&~3) - reinterpret_cast<int>(dst) : w; //IMPORTANT! Should not exceed w.
    const int x00 = ((reinterpret_cast<int>(dst)+15)&~15) - reinterpret_cast<int>(dst) < w ?
                    ((reinterpret_cast<int>(dst)+15)&~15) - reinterpret_cast<int>(dst) : w;//IMPORTANT! Should not exceed w.
    const int x_end00  = ((reinterpret_cast<int>(dst)+w)&~15) - reinterpret_cast<int>(dst);
    const int x_end0 = ((reinterpret_cast<int>(dst)+w)&~3) - reinterpret_cast<int>(dst);
    const int x_end = w;
    __m64   color_alpha_64  = _mm_set1_pi16 (color_alpha);
    __m64   round_err_64    = _mm_set1_pi16 (ROUND_ERR>>8);//important!
    __m128i color_alpha_128 = _mm_set1_epi16(color_alpha);
    __m128i round_err_128   = _mm_set1_epi16(ROUND_ERR>>8);//important!
    while(h--)
    {
        int j=0;
        for( ; j<x0; j++ )
        {
            dst[j] = ((src[j] * am[j] * color_alpha)+ROUND_ERR)>>12;
        }
        for( ;j<x00;j+=4 )
        {
            __m64 src1 = _mm_cvtsi32_si64(*reinterpret_cast<const int*>(src+j));
            __m64 mask = _mm_cvtsi32_si64(*reinterpret_cast<const int*>(am+j));
            __m64 zero = _mm_setzero_si64();
            src1 = _mm_unpacklo_pi8(src1, zero);
            src1 = _mm_mullo_pi16(src1, color_alpha_64);
            mask = _mm_unpacklo_pi8(zero, mask); //important!
            src1 = _mm_mulhi_pi16(src1, mask); //important!
            src1 = _mm_adds_pu16(src1, round_err_64);
            src1 = _mm_srli_pi16(src1, 12+8-16); //important!
            src1 = _mm_packs_pu16(src1,src1);
            *reinterpret_cast<int*>(dst+j) = _mm_cvtsi64_si32(src1);
        }
        __m128i zero = _mm_setzero_si128();
        for( ;j<x_end00;j+=16)
        {
            __m128i src1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src+j));
            __m128i mask = _mm_loadu_si128(reinterpret_cast<const __m128i*>(am+j));
            __m128i src1hi = src1;
            __m128i maskhi = mask;

            src1   = _mm_unpacklo_epi8(src1  , zero           );
            src1hi = _mm_unpackhi_epi8(src1hi, zero           );
            mask   = _mm_unpacklo_epi8(zero  , mask           ); //important!
            maskhi = _mm_unpackhi_epi8(zero  , maskhi         );
            src1   = _mm_mullo_epi16  (src1  , color_alpha_128);
            src1hi = _mm_mullo_epi16  (src1hi, color_alpha_128);
            src1   = _mm_mulhi_epu16  (src1  , mask           ); //important!
            src1hi = _mm_mulhi_epu16  (src1hi, maskhi         );
            src1   = _mm_adds_epu16   (src1  , round_err_128  );
            src1hi = _mm_adds_epu16   (src1hi, round_err_128  );
            src1   = _mm_srli_epi16   (src1  , 12+8-16        ); //important!
            src1hi = _mm_srli_epi16   (src1hi, 12+8-16        );
            src1   = _mm_packus_epi16 (src1  , src1hi         );

            _mm_storeu_si128(reinterpret_cast<__m128i*>(dst+j), src1);
        }
        for( ;j<x_end0;j+=4)
        {
            __m64 src1 = _mm_cvtsi32_si64(*reinterpret_cast<const int*>(src+j));
            __m64 mask = _mm_cvtsi32_si64(*reinterpret_cast<const int*>(am+j));
            __m64 zero = _mm_setzero_si64();
            src1 = _mm_unpacklo_pi8(src1, zero);
            src1 = _mm_mullo_pi16(src1, color_alpha_64);
            mask = _mm_unpacklo_pi8(zero, mask); //important!
            src1 = _mm_mulhi_pi16(src1, mask); //important!
            src1 = _mm_adds_pu16(src1, round_err_64);
            src1 = _mm_srli_pi16(src1, 12+8-16); //important!
            src1 = _mm_packs_pu16(src1,src1);
            *reinterpret_cast<int*>(dst+j) = _mm_cvtsi64_si32(src1);
        }
        for( ;j<x_end;j++)
        {
            dst[j] = ((src[j] * am[j] * color_alpha)+ROUND_ERR)>>12;
        }
        src += pitch;
        am  += am_pitch;
        dst += pitch;
    }
    _mm_empty();
#else
    FillAlphaMashBodyMasked_c(dst, src, color_alpha, w, h, pitch, am, am_pitch);
#endif
}

void FillAlphaMashBorderMasked_c(       BYTE* dst        , 
                                  const BYTE* border     , 
                                  const BYTE* body       ,
                                  int         color_alpha, 
                                  int         w          , 
                                  int         h          , 
                                  int         pitch      , 
                                  const BYTE* am         , 
                                  int         am_pitch)
{
    const int ROUND_ERR = 1<<(12-1);
    while(h--)
    {
        int j=0;
        for( ; j<w; j++ )
        {
            int temp = border[j]-body[j];
            temp = temp<0 ? 0 : temp;
            dst[j] = ((temp * am[j] * color_alpha)+ROUND_ERR)>>12;
        }
        body   += pitch;
        border += pitch;
        am     += am_pitch;
        dst    += pitch;
    }
}

void FillAlphaMashBorderMasked_sse2(       BYTE* dst        , 
                                     const BYTE* border     , 
                                     const BYTE* body       ,
                                     int         color_alpha, 
                                     int         w          , 
                                     int         h          , 
                                     int         pitch      , 
                                     const BYTE* am         , 
                                     int         am_pitch)
{
#ifndef _WIN64
    const int ROUND_ERR = 1<<(12-1);

    const int x0 = ((reinterpret_cast<int>(dst)+3)&~3) - reinterpret_cast<int>(dst) < w ?
                   ((reinterpret_cast<int>(dst)+3)&~3) - reinterpret_cast<int>(dst) : w; //IMPORTANT! Should not exceed w.
    const int x00 = ((reinterpret_cast<int>(dst)+15)&~15) - reinterpret_cast<int>(dst) < w ?
                    ((reinterpret_cast<int>(dst)+15)&~15) - reinterpret_cast<int>(dst) : w;//IMPORTANT! Should not exceed w.
    const int x_end00  = ((reinterpret_cast<int>(dst)+w)&~15) - reinterpret_cast<int>(dst);
    const int x_end0 = ((reinterpret_cast<int>(dst)+w)&~3) - reinterpret_cast<int>(dst);
    const int x_end = w;
    __m64   color_alpha_64  = _mm_set1_pi16 (color_alpha);
    __m64   round_err_64    = _mm_set1_pi16 (ROUND_ERR>>8);//important!
    __m128i color_alpha_128 = _mm_set1_epi16(color_alpha);
    __m128i round_err_128   = _mm_set1_epi16(ROUND_ERR>>8);//important!
    while(h--)
    {
        int j=0;
        for( ; j<x0; j++ )
        {
            int temp = border[j]-body[j];
            temp = temp<0 ? 0 : temp;
            dst[j] = ((temp * am[j] * color_alpha)+ROUND_ERR)>>12;
        }
        for( ;j<x00;j+=4 )
        {
            __m64 border1 = _mm_cvtsi32_si64(*reinterpret_cast<const int*>(border+j));
            __m64 body1   = _mm_cvtsi32_si64(*reinterpret_cast<const int*>(body  +j));
            border1 = _mm_subs_pu8(border1, body1);
            __m64 mask = _mm_cvtsi32_si64(*reinterpret_cast<const int*>(am+j));
            __m64 zero = _mm_setzero_si64();
            border1 = _mm_unpacklo_pi8(border1, zero          );
            border1 = _mm_mullo_pi16  (border1, color_alpha_64);
            mask    = _mm_unpacklo_pi8(zero   , mask          ); //important!
            border1 = _mm_mulhi_pi16  (border1, mask          ); //important!
            border1 = _mm_adds_pu16   (border1, round_err_64  );
            border1 = _mm_srli_pi16   (border1, 12+8-16       ); //important!
            border1 = _mm_packs_pu16  (border1, border1       );
            *reinterpret_cast<int*>(dst+j) = _mm_cvtsi64_si32(border1);
        }
        __m128i zero = _mm_setzero_si128();
        for( ;j<x_end00;j+=16)
        {
            __m128i border1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(border+j));
            __m128i body1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(body+j));
            border1 = _mm_subs_epu8(border1,body1);

            __m128i mask = _mm_loadu_si128(reinterpret_cast<const __m128i*>(am+j));
            __m128i srchi = border1;
            __m128i maskhi = mask;

            border1 = _mm_unpacklo_epi8(border1, zero           );
            srchi   = _mm_unpackhi_epi8(srchi  , zero           );
            mask    = _mm_unpacklo_epi8(zero   , mask           ); //important!
            maskhi  = _mm_unpackhi_epi8(zero   , maskhi         );
            border1 = _mm_mullo_epi16  (border1, color_alpha_128);
            srchi   = _mm_mullo_epi16  (srchi  , color_alpha_128);
            border1 = _mm_mulhi_epu16  (border1, mask           ); //important!
            srchi   = _mm_mulhi_epu16  (srchi  , maskhi         );
            border1 = _mm_adds_epu16   (border1, round_err_128  );
            srchi   = _mm_adds_epu16   (srchi  , round_err_128  );
            border1 = _mm_srli_epi16   (border1, 12+8-16        ); //important!
            srchi   = _mm_srli_epi16   (srchi  , 12+8-16        );
            border1 = _mm_packus_epi16 (border1, srchi          );

            _mm_storeu_si128(reinterpret_cast<__m128i*>(dst+j), border1);
        }
        for( ;j<x_end0;j+=4)
        {
            __m64 border1 = _mm_cvtsi32_si64(*reinterpret_cast<const int*>(border+j));
            __m64 body1   = _mm_cvtsi32_si64(*reinterpret_cast<const int*>(body  +j));
            border1 = _mm_subs_pu8(border1, body1);
            __m64 mask = _mm_cvtsi32_si64(*reinterpret_cast<const int*>(am+j));
            __m64 zero = _mm_setzero_si64();
            border1 = _mm_unpacklo_pi8(border1, zero          );
            border1 = _mm_mullo_pi16  (border1, color_alpha_64);
            mask    = _mm_unpacklo_pi8(zero   , mask          ); //important!
            border1 = _mm_mulhi_pi16  (border1, mask          ); //important!
            border1 = _mm_adds_pu16   (border1, round_err_64  );
            border1 = _mm_srli_pi16   (border1, 12+8-16       ); //important!
            border1 = _mm_packs_pu16  (border1, border1       );
            *reinterpret_cast<int*>(dst+j) = _mm_cvtsi64_si32(border1);
        }
        for( ;j<x_end;j++)
        {
            int temp = border[j]-body[j];
            temp = temp<0 ? 0 : temp;
            dst[j] = ((temp * am[j] * color_alpha)+ROUND_ERR)>>12;
        }
        body   += pitch;
        border += pitch;
        am     += am_pitch;
        dst    += pitch;
    }
    _mm_empty();
#else
    FillAlphaMashBorderMasked_c(dst, border, body, color_alpha,w,h,pitch,am,am_pitch);
#endif
}

void Overlay::FillAlphaMash( byte* outputAlphaMask, bool fBody, bool fBorder, int x, int y, int w, int h, const byte* pAlphaMask, int pitch, DWORD color_alpha)
{
    const BYTE * body = mBody.get();
    const BYTE * border = mBorder.get();
    body   = body  !=NULL ? body   + y*mOverlayPitch + x: NULL;
    border = border!=NULL ? border + y*mOverlayPitch + x: NULL;
    byte* dst = outputAlphaMask + y*mOverlayPitch + x;

    if(!fBorder && fBody && pAlphaMask==NULL)
    {
        FillAlphaMashBody_sse2(dst, body, color_alpha, w, h, mOverlayPitch);
    }
    else if(fBorder && fBody && pAlphaMask==NULL)
    {
        FillAlphaMashBody_sse2(dst, border, color_alpha, w, h, mOverlayPitch);
    }
    else if(fBorder && !fBody && pAlphaMask==NULL)
    {
        FillAlphaMashBorder_sse2(dst, border, body, color_alpha, w, h, mOverlayPitch);
    }
    else if(!fBorder && fBody && pAlphaMask!=NULL)
    {
        FillAlphaMashBodyMasked_sse2(dst, body, color_alpha, w, h, mOverlayPitch, pAlphaMask, pitch);
    }
    else if(fBorder && fBody && pAlphaMask!=NULL)
    {
        FillAlphaMashBodyMasked_sse2(dst, border, color_alpha, w, h, mOverlayPitch, pAlphaMask, pitch);
    }
    else if(fBorder && !fBody && pAlphaMask!=NULL)
    {
        FillAlphaMashBorderMasked_sse2(dst, border, body, color_alpha, w, h, mOverlayPitch, pAlphaMask, pitch);
    }
    else
    {
        //should NOT happen
        ASSERT(0);
        XY_LOG_FATAL("Unexpected!");
    }
}

Overlay* Overlay::GetSubpixelVariance(unsigned int xshift, unsigned int yshift)
{
    Overlay* overlay = DEBUG_NEW Overlay();
    if(!overlay)
    {
        return NULL;
    }
    xshift &= 7;
    yshift &= 7;

    overlay->mOffsetX = mOffsetX - xshift;
    overlay->mOffsetY = mOffsetY - yshift;
    overlay->mWidth = mWidth + xshift;
    overlay->mHeight = mHeight + yshift;

    overlay->mOverlayWidth = ((overlay->mWidth+7)>>3) + 1;
    overlay->mOverlayHeight = ((overlay->mHeight + 7)>>3) + 1;
    overlay->mOverlayPitch = (overlay->mOverlayWidth+15)&~15;
    

    overlay->mfWideOutlineEmpty = mfWideOutlineEmpty;

    if (overlay->mOverlayPitch * overlay->mOverlayHeight<=0)
    {
        return NULL;
    }

    BYTE* body = reinterpret_cast<BYTE*>(xy_malloc(overlay->mOverlayPitch * overlay->mOverlayHeight));
    if( body==NULL )
    {
        return NULL;
    }
    overlay->mBody.reset(body, xy_free);
    memset(body, 0, overlay->mOverlayPitch*overlay->mOverlayHeight);
    BYTE* border = NULL;
    if (!overlay->mfWideOutlineEmpty)
    {
        border = reinterpret_cast<BYTE*>(xy_malloc(overlay->mOverlayPitch * overlay->mOverlayHeight));
        if (border==NULL)
        {
            return NULL;
        }
        overlay->mBorder.reset(border, xy_free);
        memset(border, 0, overlay->mOverlayPitch*overlay->mOverlayHeight);
    }
    
    if( overlay->mOverlayPitch==mOverlayPitch && overlay->mOverlayWidth==mOverlayWidth &&
        overlay->mOverlayHeight>=mOverlayHeight )
    {
        if (body && mBody)
        {
            memcpy(body, mBody.get(), mOverlayPitch * mOverlayHeight);
        }
        else if ( (!!body)!=(!!mBody)/*==NULL*/)
        {
            return NULL;
        }
        
        if (border && mBorder)
        {
            memcpy(border, mBorder.get(), mOverlayPitch * mOverlayHeight);
        }
        else if ( (!!border)!=(!!mBorder)/*==NULL*/ )
        {
            return NULL;
        }
    }
    else
    {
        byte* dst = body;
        const byte* src = mBody.get();
        for (int i=0;i<mOverlayHeight;i++)
        {
            memcpy(dst, src, mOverlayWidth);
            dst += overlay->mOverlayPitch;
            src += mOverlayPitch;
        }
        if (!overlay->mfWideOutlineEmpty)
        {
            ASSERT(border && mBorder);
            dst = border;
            src = mBorder.get();
            for (int i=0;i<mOverlayHeight;i++)
            {
                memcpy(dst, src, mOverlayWidth);
                dst += overlay->mOverlayPitch;
                src += mOverlayPitch;
            }
        }
    }
    //not equal
    //  Bilinear(overlay->mpOverlayBuffer.base, overlay->mOverlayWidth, 2*overlay->mOverlayHeight, overlay->mOverlayPitch, xshift, yshift);
    Bilinear(body, overlay->mOverlayWidth, overlay->mOverlayHeight, overlay->mOverlayPitch, xshift, yshift);
    if (!overlay->mfWideOutlineEmpty)
    {
        Bilinear(border, overlay->mOverlayWidth, overlay->mOverlayHeight, overlay->mOverlayPitch, xshift, yshift);
    }    
    return overlay;
}

///////////////////////////////////////////////////////////////

// PathData

PathData::PathData():mpPathTypes(NULL), mpPathPoints(NULL), mPathPoints(0)
{
}

PathData::PathData( const PathData& src ):mpPathTypes(NULL), mpPathPoints(NULL), mPathPoints(src.mPathPoints)
{
    //TODO: deal with the case that src.mPathPoints<0 
    if(mPathPoints>0)
    {
        mpPathTypes = static_cast<BYTE*>(malloc(mPathPoints * sizeof(BYTE)));
        mpPathPoints = static_cast<POINT*>(malloc(mPathPoints * sizeof(POINT)));
    }
    if(mPathPoints>0)
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
            _TrashPath();
            mPathPoints = src.mPathPoints;
            mpPathTypes = static_cast<BYTE*>(malloc(mPathPoints * sizeof(BYTE)));
            mpPathPoints = static_cast<POINT*>(malloc(mPathPoints * sizeof(POINT)));//better than realloc
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

bool PathData::operator==( const PathData& rhs ) const
{
    return (this==&rhs) || (
        mPathPoints==rhs.mPathPoints 
        && !memcmp(mpPathTypes, rhs.mpPathTypes, mPathPoints * sizeof(BYTE) ) 
        && !memcmp(mpPathPoints, rhs.mpPathPoints, mPathPoints * sizeof(POINT) )
        );
}

void PathData::_TrashPath()
{
    if (mpPathTypes)
    {
        free(mpPathTypes);
        mpPathTypes = NULL;
    }
    if (mpPathPoints)
    {
        free(mpPathPoints);
        mpPathPoints = NULL;
    }
    mPathPoints = 0;
}

bool PathData::BeginPath(HDC hdc)
{
    _TrashPath();
    return !!::BeginPath(hdc);
}

bool PathData::EndPath(HDC hdc)
{
    bool succeeded = false;
    succeeded = !!::CloseFigure(hdc);
    ASSERT(succeeded);
    succeeded = !!::EndPath(hdc);
    ASSERT(succeeded);
    if(succeeded)
    {
        mPathPoints = GetPath(hdc, NULL, NULL, 0);
        if(!mPathPoints)
            return true;
        mpPathTypes = (BYTE*)malloc(sizeof(BYTE) * mPathPoints);
        mpPathPoints = (POINT*)malloc(sizeof(POINT) * mPathPoints);
        if(mPathPoints == GetPath(hdc, mpPathPoints, mpPathTypes, mPathPoints))
            return true;
    }
    succeeded = !!::AbortPath(hdc);
    ASSERT(succeeded);
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
        BYTE* pTypes = DEBUG_NEW BYTE[nPoints];
        POINT* pPoints = DEBUG_NEW POINT[nPoints];
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

void PathData::AlignLeftTop(CPoint *left_top, CSize *size)
{
    int minx = INT_MAX;
    int miny = INT_MAX;
    int maxx = INT_MIN;
    int maxy = INT_MIN;
    for(int i=0; i<mPathPoints; ++i)
    {
        int ix = mpPathPoints[i].x;
        int iy = mpPathPoints[i].y;
        if(ix < minx) minx = ix;
        if(ix > maxx) maxx = ix;
        if(iy < miny) miny = iy;
        if(iy > maxy) maxy = iy;
    }
    if(minx > maxx || miny > maxy)
    {
        _TrashPath();
        *left_top = CPoint(0, 0);
        *size = CSize(0, 0);
        return;
    }
    minx = (minx >> 3) & ~7;
    miny = (miny >> 3) & ~7;
    maxx = (maxx + 7) >> 3;
    maxy = (maxy + 7) >> 3;
    for(int i=0; i<mPathPoints; ++i)
    {
        mpPathPoints[i].x -= minx*8;
        mpPathPoints[i].y -= miny*8;
    }
    *left_top = CPoint(minx, miny);
    *size = CSize(maxx+1-minx, maxy+1-miny);
    return;
}

//////////////////////////////////////////////////////////////////////////

// ScanLineData

ScanLineData::ScanLineData()
{
}

ScanLineData::~ScanLineData()
{    
}

void ScanLineData::_ReallocEdgeBuffer(unsigned edges)
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

    if (y1 > y0) {
        _EvaluateLine<LINE_UP>(x0, y0, x1, y1);
    } else if (y1 < y0) {
        _EvaluateLine<LINE_DOWN>(x1, y1, x0, y0);
    }
}

template<int flag>
void ScanLineData::_EvaluateLine(int x0, int y0, int x1, int y1)
{
    __int64 xacc = (__int64)x0 << 13;

    // prestep

    int dy = y1 - y0;
    int y = ((y0 + 3) & ~7) + 4;
    int iy = y >> 3;

    y1 = (y1 - 5) >> 3;

    if (iy <= y1) {
        __int64 invslope = (__int64(x1 - x0) << 16) / dy;

        if (mEdgeNext + y1 + 1 - iy > mEdgeHeapSize) {
            unsigned int new_edge_heap_size = mEdgeHeapSize * 2;
            while (mEdgeNext + y1 + 1 - iy > new_edge_heap_size) {
                new_edge_heap_size *= 2;
            }
            _ReallocEdgeBuffer(new_edge_heap_size);
        }

        xacc += (invslope * (y - y0)) >> 3;

        while (iy <= y1) {
            int ix = (int)((xacc + 32768) >> 16);

            mpEdgeBuffer[mEdgeNext].next = mpScanBuffer[iy];
            mpEdgeBuffer[mEdgeNext].posandflag = (ix << 1) | flag;

            mpScanBuffer[iy] = mEdgeNext++;

            ++iy;
            xacc += invslope;
        }
    }
}

bool ScanLineData::ScanConvert(const PathData& path_data, const CSize& size)
{
    int lastmoveto = -1;
    int i;
    // Drop any outlines we may have.
    mOutline.clear();
    // Determine bounding box
    if(!path_data.mPathPoints)
    {
        mWidth = mHeight = 0;
        return false;
    }
    mWidth = size.cx;
    mHeight = size.cy;
    // Initialize edge buffer.  We use edge 0 as a sentinel.
    mEdgeNext = 1;
    mEdgeHeapSize = 2048;
    mpEdgeBuffer = (Edge*)malloc(sizeof(Edge)*mEdgeHeapSize);
    // Initialize scanline list.
    mpScanBuffer = DEBUG_NEW unsigned int[mHeight];
    memset(mpScanBuffer, 0, mHeight*sizeof(unsigned int));
    // Scan convert the outline.  Yuck, Bezier curves....
    // Unfortunately, Windows 95/98 GDI has a bad habit of giving us text
    // paths with all but the first figure left open, so we can't rely
    // on the PT_CLOSEFIGURE flag being used appropriately.
    fFirstSet = false;
    firstp.x = firstp.y = 0;
    lastp.x = lastp.y = 0;
    for(i=0; i<path_data.mPathPoints; ++i)
    {
        BYTE t = path_data.mpPathTypes[i] & ~PT_CLOSEFIGURE;
        switch(t)
        {
        case PT_MOVETO:
            if(lastmoveto >= 0 && firstp != lastp)
                _EvaluateLine(lastp.x, lastp.y, firstp.x, firstp.y);
            lastmoveto = i;
            fFirstSet = false;
            lastp = path_data.mpPathPoints[i];
            break;
        case PT_MOVETONC:
            break;
        case PT_LINETO:
            if(path_data.mPathPoints - (i-1) >= 2) _EvaluateLine(path_data, i-1, i);
            break;
        case PT_BEZIERTO:
            if(path_data.mPathPoints - (i-1) >= 4) _EvaluateBezier(path_data, i-1, false);
            i += 2;
            break;
        case PT_BSPLINETO:
            if(path_data.mPathPoints - (i-1) >= 4) _EvaluateBezier(path_data, i-1, true);
            i += 2;
            break;
        case PT_BSPLINEPATCHTO:
            if(path_data.mPathPoints - (i-3) >= 4) _EvaluateBezier(path_data, i-3, true);
            break;
        }
    }
    if(lastmoveto >= 0 && firstp != lastp)
        _EvaluateLine(lastp.x, lastp.y, firstp.x, firstp.y);
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

void ScanLineData::DeleteOutlines()
{    
    mOutline.clear();
}

bool ScanLineData2::CreateWidenedRegion(int rx, int ry)
{
    if(rx < 0) rx = 0;
    if(ry < 0) ry = 0;
    mWideBorder = max(rx,ry);
    mWideOutline.clear();

    const tSpanBuffer& out_line = m_scan_line_data->mOutline;
    if (ry > 0)
    {
        WidenRegionCreater *widen_region_creater = WidenRegionCreater::GetDefaultWidenRegionCreater();
        widen_region_creater->xy_overlap_region(&mWideOutline, out_line, rx, ry);
    }
    else if (ry == 0 && rx > 0)
    {
        // There are artifacts if we don't make at least two overlaps of the line, even at same Y coord
        OverlapRegion(mWideOutline, out_line, rx, 0);
        OverlapRegion(mWideOutline, out_line, rx, 0);
    }
    return true;
}
