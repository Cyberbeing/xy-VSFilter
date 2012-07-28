#ifndef __XY_FILTER_BENCHMARK_F0ED9F75_C0A7_4B0E_910E_7A82ECE2D7AD_H__
#define __XY_FILTER_BENCHMARK_F0ED9F75_C0A7_4B0E_910E_7A82ECE2D7AD_H__

#include <gtest/gtest.h>
#include <wtypes.h>
#include <math.h>
#include <xmmintrin.h>
#include "xy_malloc.h"

typedef const UINT8 CUINT8, *PCUINT8;

//////////////////////////////////////////////////////////////////////

// transpose test

void xy_filter_c_v0(float *dst, int width, int height, int stride, const float *filter, int filter_width)
{
    ASSERT( stride>=4*(width+filter_width) );

    int xx_fix = width > filter_width ? 0 : filter_width - width;
    const float *filter_start = filter;

    BYTE* end = reinterpret_cast<BYTE*>(dst) + height*stride;
    BYTE* dst_byte = reinterpret_cast<BYTE*>(dst);
    for( ; dst_byte<end; dst_byte+=stride )
    {
        float *dst_f = reinterpret_cast<float*>(dst_byte); 
        float *dst2 = dst_f - filter_width;
        float *dst_endr = dst_f + width;
        float *dst_end0 = dst_endr - filter_width;
        float *dst_endl = dst_f - xx_fix;
        ASSERT(xx_fix==0 || dst_end0==dst_endl);

        ASSERT(filter_start == filter);
        filter_start += filter_width;
        const float *filter_end = filter_start;
        for (;dst2<dst_endl;dst2++, filter_start--)//left margin
        {
            const float *src = dst_f;
            float sum = 0;
            for(const float* f=filter_start;f<filter_end;f++, src++)
            {
                sum += src[0] * f[0];
            }
            *dst2 = sum;
        }
        for (;dst2<dst_f;dst2++, filter_start--, filter_end--)//if width < filter_width
        {
            const float *src = dst_f;
            float sum = 0;
            for(const float* f=filter_start;f<filter_end;f++, src++)
            {
                sum += src[0] * f[0];
            }
            *dst2 = sum;
        }
        ASSERT(filter_start==filter);
        for (;dst2<dst_end0;dst2++)
        {
            const float *src = dst2;

            float sum = 0;
            for(const float* f=filter_start;f<filter_end;f++, src++)
            {
                sum += src[0] * f[0];
            }
            *dst2 = sum;
        }
        for (;dst2<dst_endr;dst2++, filter_end--)//right margin
        {
            const float *src = dst2;
            float sum = 0;
            for(const float* f=filter;f<filter_end;f++, src++)
            {
                sum += src[0] * f[0];
            }
            *dst2 = sum;
        }
    }
}

void xy_filter_sse_v0(float *dst, int width, int height, int stride, const float *filter, int filter_width)
{
#ifdef XY_FILTER_4
#  undef XY_FILTER_4
#endif
    ASSERT( stride>=4*(width+filter_width) );
    ASSERT( ((stride|(4*width)|(4*filter_width)|reinterpret_cast<int>(dst)|reinterpret_cast<int>(filter))&15)==0 );

    int xx_fix = width > filter_width ? 0 : filter_width - width;
    const float *filter_start = filter;
    BYTE* dst_byte = reinterpret_cast<BYTE*>(dst);
    BYTE* end = dst_byte + height*stride;
    for( ; dst_byte<end; dst_byte+=stride )
    {
        float *dst_f = reinterpret_cast<float*>(dst_byte); 
        float *dst2 = dst_f - filter_width;
        float *dst_endr = dst_f + width;
        float *dst_end0 = dst_endr - filter_width;
        float *dst_endl = dst_f - xx_fix;
        ASSERT(xx_fix==0 || dst_end0==dst_endl);

        ASSERT(filter_start == filter);
        filter_start += filter_width;
        const float *filter_end = filter_start;

        for (;dst2<dst_endl;dst2+=4)//left margin
        {
            const float *src = dst_f;
            filter_start -= 4;

            //filter 4
            __m128 src4 = _mm_setzero_ps();/*1 2 3 4*/
            __m128 sum = _mm_setzero_ps();
            for(const float* f=filter_start;f<filter_end;f+=4,src+=4)
            {   
                __m128 src_5_8 = _mm_load_ps(src);/*5 6 7 8*/
                __m128 f4 = _mm_load_ps(f);

#define XY_FILTER_4(src4, src_5_8, f4, sum) \
    __m128 f4_1 = _mm_unpacklo_ps(f4,f4);\
    f4_1 = _mm_unpacklo_ps(f4_1, f4_1);\
    f4_1 = _mm_mul_ps(f4_1, src4);\
    sum = _mm_add_ps(sum, f4_1);\
    __m128 src_3_6 = _mm_shuffle_ps(src4, src_5_8, _MM_SHUFFLE(1,0,3,2));/*3 4 5 6*/\
    f4_1 = _mm_unpackhi_ps(f4,f4);\
    f4_1 = _mm_unpacklo_ps(f4_1,f4_1);\
    f4_1 = _mm_mul_ps(f4_1, src_3_6);\
    sum = _mm_add_ps(sum, f4_1);\
    src4 = _mm_shuffle_ps(src4, src_3_6, _MM_SHUFFLE(2,1,2,1));/*2 3 4 5*/\
    f4_1 = _mm_unpacklo_ps(f4,f4);\
    f4_1 = _mm_unpackhi_ps(f4_1, f4_1);\
    f4_1 = _mm_mul_ps(f4_1, src4);\
    sum = _mm_add_ps(sum, f4_1);\
    src_3_6 = _mm_shuffle_ps(src_3_6, src_5_8, _MM_SHUFFLE(2,1,2,1));/*4 5 6 7*/\
    f4_1 = _mm_unpackhi_ps(f4,f4);\
    f4_1 = _mm_unpackhi_ps(f4_1, f4_1);\
    f4_1 = _mm_mul_ps(f4_1, src_3_6);\
    sum = _mm_add_ps(sum, f4_1)

                { XY_FILTER_4(src4, src_5_8, f4, sum); }
                src4 = src_5_8;
            }
            //store result
            _mm_stream_ps(dst2, sum);
        }
        for (;dst2<dst_f;dst2+=4)//if width < filter_width
        {
            const float *src = dst_f;
            filter_start-=4;
            filter_end-=4;

            __m128 src4 = _mm_setzero_ps();/*1 2 3 4*/
            __m128 sum = _mm_setzero_ps();
            __m128 src_5_8, f4;
            for(const float* f=filter_start;f<filter_end;f+=4,src+=4)
            {   
                src_5_8 = _mm_load_ps(src);/*5 6 7 8*/
                f4 = _mm_load_ps(f);

                { XY_FILTER_4(src4, src_5_8, f4, sum); }
                src4 = src_5_8;
            }
            src_5_8 = _mm_setzero_ps();
            f4 = _mm_load_ps(filter_end);
            { XY_FILTER_4(src4, src_5_8, f4, sum); }
            //store result
            _mm_stream_ps(dst2, sum);
        }
        ASSERT(filter_start == filter);
        for (;dst2<dst_end0;dst2+=4)
        {
            const float *src = dst2;

            //filter 4
            __m128 src4 = _mm_load_ps(src);/*1 2 3 4*/
            __m128 sum = _mm_setzero_ps();
            for(const float* f=filter_start;f<filter_end;f+=4)
            {
                src+=4;
                __m128 src_5_8 = _mm_load_ps(src);/*5 6 7 8*/
                __m128 f4 = _mm_load_ps(f);

                { XY_FILTER_4(src4, src_5_8, f4, sum); }
                src4 = src_5_8;
            }
            //store result
            _mm_stream_ps(dst2, sum);
        }
        for (;dst2<dst_endr;dst2+=4)//right margin
        {
            const float *src = dst2;
            filter_end-=4;

            //filter 4
            __m128 src4 = _mm_load_ps(src);//1 2 3 4
            __m128 sum = _mm_setzero_ps();
            __m128 src_5_8, f4;
            for(const float* f=filter_start;f<filter_end;f+=4)
            {
                src+=4;
                src_5_8 = _mm_load_ps(src);//5 6 7 8
                f4 = _mm_load_ps(f);

                { XY_FILTER_4(src4, src_5_8, f4, sum); }

                src4 = src_5_8;
                //move new 4 in_n_out to old 4 in_n_out
            }
            src_5_8 = _mm_setzero_ps();
            f4 = _mm_load_ps(filter_end);
            { XY_FILTER_4(src4, src_5_8, f4, sum); }
            //store result
            _mm_stream_ps(dst2, sum);
        }
    }
#undef XY_FILTER_4
}

void xy_filter_sse_v1(float *dst, int width, int height, int stride, const float *filter, int filter_width)
{
#ifdef XY_FILTER_4
#  undef XY_FILTER_4
#endif
    ASSERT( stride>=4*(width+filter_width) );
    ASSERT( ((stride|(4*width)|(4*filter_width)|reinterpret_cast<int>(dst)|reinterpret_cast<int>(filter))&15)==0 );

    int xx_fix = width > filter_width ? 0 : filter_width - width;
    const float *filter_start = filter;
    BYTE* dst_byte = reinterpret_cast<BYTE*>(dst);
    BYTE* end = dst_byte + height*stride;
    for( ; dst_byte<end; dst_byte+=stride )
    {
        float *dst_f = reinterpret_cast<float*>(dst_byte); 
        float *dst2 = dst_f - filter_width;
        float *dst_endr = dst_f + width;
        float *dst_end0 = dst_endr - filter_width;
        float *dst_endl = dst_f - xx_fix;
        ASSERT(xx_fix==0 || dst_end0==dst_endl);

        ASSERT(filter_start == filter);
        filter_start += filter_width;
        const float *filter_end = filter_start;

        for (;dst2<dst_endl;dst2+=4)//left margin
        {
            const float *src = dst_f;
            filter_start -= 4;

            //filter 4
            __m128 src4 = _mm_setzero_ps();/*1 2 3 4*/
            __m128 sum = _mm_setzero_ps();
            for(const float* f=filter_start;f<filter_end;f+=4,src+=4)
            {   
                __m128 src_5_8 = _mm_load_ps(src);/*5 6 7 8*/
                __m128 f4 = _mm_load_ps(f);

#define XY_FILTER_4(src4, src_5_8, f4, sum) \
    __m128 f4_1 = _mm_shuffle_ps(f4, f4, _MM_SHUFFLE(0,0,0,0));\
    f4_1 = _mm_mul_ps(f4_1, src4);\
    sum = _mm_add_ps(sum, f4_1);\
    __m128 src_3_6 = _mm_shuffle_ps(src4, src_5_8, _MM_SHUFFLE(1,0,3,2));/*3 4 5 6*/\
    f4_1 = _mm_shuffle_ps(f4, f4, _MM_SHUFFLE(2,2,2,2));\
    f4_1 = _mm_mul_ps(f4_1, src_3_6);\
    sum = _mm_add_ps(sum, f4_1);\
    src4 = _mm_shuffle_ps(src4, src_3_6, _MM_SHUFFLE(2,1,2,1));/*2 3 4 5*/\
    f4_1 = _mm_shuffle_ps(f4, f4, _MM_SHUFFLE(1,1,1,1));\
    f4_1 = _mm_mul_ps(f4_1, src4);\
    sum = _mm_add_ps(sum, f4_1);\
    src_3_6 = _mm_shuffle_ps(src_3_6, src_5_8, _MM_SHUFFLE(2,1,2,1));/*4 5 6 7*/\
    f4_1 = _mm_shuffle_ps(f4, f4, _MM_SHUFFLE(3,3,3,3));\
    f4_1 = _mm_mul_ps(f4_1, src_3_6);\
    sum = _mm_add_ps(sum, f4_1)

                { XY_FILTER_4(src4, src_5_8, f4, sum); }
                src4 = src_5_8;
            }
            //store result
            _mm_stream_ps(dst2, sum);
        }
        for (;dst2<dst_f;dst2+=4)//if width < filter_width
        {
            const float *src = dst_f;
            filter_start-=4;
            filter_end-=4;

            __m128 src4 = _mm_setzero_ps();/*1 2 3 4*/
            __m128 sum = _mm_setzero_ps();
            __m128 src_5_8, f4;
            for(const float* f=filter_start;f<filter_end;f+=4,src+=4)
            {   
                src_5_8 = _mm_load_ps(src);/*5 6 7 8*/
                f4 = _mm_load_ps(f);

                { XY_FILTER_4(src4, src_5_8, f4, sum); }
                src4 = src_5_8;
            }
            src_5_8 = _mm_setzero_ps();
            f4 = _mm_load_ps(filter_end);
            { XY_FILTER_4(src4, src_5_8, f4, sum); }
            //store result
            _mm_stream_ps(dst2, sum);
        }
        ASSERT(filter_start == filter);
        for (;dst2<dst_end0;dst2+=4)
        {
            const float *src = dst2;

            //filter 4
            __m128 src4 = _mm_load_ps(src);/*1 2 3 4*/
            __m128 sum = _mm_setzero_ps();
            for(const float* f=filter_start;f<filter_end;f+=4)
            {
                src+=4;
                __m128 src_5_8 = _mm_load_ps(src);/*5 6 7 8*/
                __m128 f4 = _mm_load_ps(f);

                { XY_FILTER_4(src4, src_5_8, f4, sum); }
                src4 = src_5_8;
            }
            //store result
            _mm_stream_ps(dst2, sum);
        }
        for (;dst2<dst_endr;dst2+=4)//right margin
        {
            const float *src = dst2;
            filter_end-=4;

            //filter 4
            __m128 src4 = _mm_load_ps(src);//1 2 3 4
            __m128 sum = _mm_setzero_ps();
            __m128 src_5_8, f4;
            for(const float* f=filter_start;f<filter_end;f+=4)
            {
                src+=4;
                src_5_8 = _mm_load_ps(src);//5 6 7 8
                f4 = _mm_load_ps(f);

                { XY_FILTER_4(src4, src_5_8, f4, sum); }

                src4 = src_5_8;
                //move new 4 in_n_out to old 4 in_n_out
            }
            src_5_8 = _mm_setzero_ps();
            f4 = _mm_load_ps(filter_end);
            { XY_FILTER_4(src4, src_5_8, f4, sum); }
            //store result
            _mm_stream_ps(dst2, sum);
        }
    }
#undef XY_FILTER_4
}

void xy_filter_sse_v2(float *dst, int width, int height, int stride, const float *filter, int filter_width)
{
#ifdef XY_FILTER_4
#  undef XY_FILTER_4
#endif
    ASSERT( stride>=4*(width+filter_width) );
    ASSERT( ((stride|(4*width)|(4*filter_width)|reinterpret_cast<int>(dst)|reinterpret_cast<int>(filter))&15)==0 );

    int xx_fix = width > filter_width ? 0 : filter_width - width;
    const float *filter_start = filter;
    BYTE* dst_byte = reinterpret_cast<BYTE*>(dst);
    BYTE* end = dst_byte + height*stride;
    for( ; dst_byte<end; dst_byte+=stride )
    {
        float *dst_f = reinterpret_cast<float*>(dst_byte); 
        float *dst2 = dst_f - filter_width;
        float *dst_endr = dst_f + width;
        float *dst_end0 = dst_endr - filter_width;
        float *dst_endl = dst_f - xx_fix;
        ASSERT(xx_fix==0 || dst_end0==dst_endl);

        ASSERT(filter_start == filter);
        filter_start += filter_width;
        const float *filter_end = filter_start;

        for (;dst2<dst_endl;dst2+=4)//left margin
        {
            const float *src = dst_f;
            filter_start -= 4;

            //filter 4
            __m128 src4 = _mm_setzero_ps();/*1 2 3 4*/
            __m128 sum = _mm_setzero_ps();
            for(const float* f=filter_start;f<filter_end;f+=4,src+=4)
            {   
                __m128 src_5_8 = _mm_load_ps(src);/*5 6 7 8*/
                __m128 f4 = _mm_load_ps(f);

#define XY_FILTER_4(src4, src_5_8, f4, sum) \
    __m128 f4_1 = _mm_shuffle_ps(f4, f4, _MM_SHUFFLE(0,0,0,0));\
    f4_1 = _mm_mul_ps(f4_1, src4);\
    sum = _mm_add_ps(sum, f4_1);\
    __m128 src_3_6 = _mm_shuffle_ps(src4, src_5_8, _MM_SHUFFLE(1,0,3,2));/*3 4 5 6*/\
    f4_1 = _mm_shuffle_ps(f4, f4, _MM_SHUFFLE(2,2,2,2));\
    f4_1 = _mm_mul_ps(f4_1, src_3_6);\
    sum = _mm_add_ps(sum, f4_1);\
    src4 = _mm_shuffle_ps(src4, src_3_6, _MM_SHUFFLE(2,1,2,1));/*2 3 4 5*/\
    f4_1 = _mm_shuffle_ps(f4, f4, _MM_SHUFFLE(1,1,1,1));\
    f4_1 = _mm_mul_ps(f4_1, src4);\
    sum = _mm_add_ps(sum, f4_1);\
    src_3_6 = _mm_shuffle_ps(src_3_6, src_5_8, _MM_SHUFFLE(2,1,2,1));/*4 5 6 7*/\
    f4_1 = _mm_shuffle_ps(f4, f4, _MM_SHUFFLE(3,3,3,3));\
    f4_1 = _mm_mul_ps(f4_1, src_3_6);\
    sum = _mm_add_ps(sum, f4_1)

                { XY_FILTER_4(src4, src_5_8, f4, sum); }
                src4 = src_5_8;
            }
            //store result
            _mm_store_ps(dst2, sum);
        }
        for (;dst2<dst_f;dst2+=4)//if width < filter_width
        {
            const float *src = dst_f;
            filter_start-=4;
            filter_end-=4;

            __m128 src4 = _mm_setzero_ps();/*1 2 3 4*/
            __m128 sum = _mm_setzero_ps();
            __m128 src_5_8, f4;
            for(const float* f=filter_start;f<filter_end;f+=4,src+=4)
            {   
                src_5_8 = _mm_load_ps(src);/*5 6 7 8*/
                f4 = _mm_load_ps(f);

                { XY_FILTER_4(src4, src_5_8, f4, sum); }
                src4 = src_5_8;
            }
            src_5_8 = _mm_setzero_ps();
            f4 = _mm_load_ps(filter_end);
            { XY_FILTER_4(src4, src_5_8, f4, sum); }
            //store result
            _mm_store_ps(dst2, sum);
        }
        ASSERT(filter_start == filter);
        for (;dst2<dst_end0;dst2+=4)
        {
            const float *src = dst2;

            //filter 4
            __m128 src4 = _mm_load_ps(src);/*1 2 3 4*/
            __m128 sum = _mm_setzero_ps();
            for(const float* f=filter_start;f<filter_end;f+=4)
            {
                src+=4;
                __m128 src_5_8 = _mm_load_ps(src);/*5 6 7 8*/
                __m128 f4 = _mm_load_ps(f);

                { XY_FILTER_4(src4, src_5_8, f4, sum); }
                src4 = src_5_8;
            }
            //store result
            _mm_store_ps(dst2, sum);
        }
        for (;dst2<dst_endr;dst2+=4)//right margin
        {
            const float *src = dst2;
            filter_end-=4;

            //filter 4
            __m128 src4 = _mm_load_ps(src);//1 2 3 4
            __m128 sum = _mm_setzero_ps();
            __m128 src_5_8, f4;
            for(const float* f=filter_start;f<filter_end;f+=4)
            {
                src+=4;
                src_5_8 = _mm_load_ps(src);//5 6 7 8
                f4 = _mm_load_ps(f);

                { XY_FILTER_4(src4, src_5_8, f4, sum); }

                src4 = src_5_8;
                //move new 4 in_n_out to old 4 in_n_out
            }
            src_5_8 = _mm_setzero_ps();
            f4 = _mm_load_ps(filter_end);
            { XY_FILTER_4(src4, src_5_8, f4, sum); }
            //store result
            _mm_store_ps(dst2, sum);
        }
    }
#undef XY_FILTER_4
}

static __forceinline void xy_filter_4_inline(__m128& src4, const __m128& src_5_8, const __m128& f4, __m128& sum)
{
    __m128 f4_1 = _mm_shuffle_ps(f4, f4, _MM_SHUFFLE(0,0,0,0));
    f4_1 = _mm_mul_ps(f4_1, src4);
    sum = _mm_add_ps(sum, f4_1);
    __m128 src_3_6 = _mm_shuffle_ps(src4, src_5_8, _MM_SHUFFLE(1,0,3,2));/*3 4 5 6*/
    f4_1 = _mm_shuffle_ps(f4, f4, _MM_SHUFFLE(2,2,2,2));
    f4_1 = _mm_mul_ps(f4_1, src_3_6);
    sum = _mm_add_ps(sum, f4_1);
    src4 = _mm_shuffle_ps(src4, src_3_6, _MM_SHUFFLE(2,1,2,1));/*2 3 4 5*/
    f4_1 = _mm_shuffle_ps(f4, f4, _MM_SHUFFLE(1,1,1,1));
    f4_1 = _mm_mul_ps(f4_1, src4);
    sum = _mm_add_ps(sum, f4_1);
    src_3_6 = _mm_shuffle_ps(src_3_6, src_5_8, _MM_SHUFFLE(2,1,2,1));/*4 5 6 7*/
    f4_1 = _mm_shuffle_ps(f4, f4, _MM_SHUFFLE(3,3,3,3));
    f4_1 = _mm_mul_ps(f4_1, src_3_6);
    sum = _mm_add_ps(sum, f4_1);
}

void xy_filter_sse_v3(float *dst, int width, int height, int stride, const float *filter, int filter_width)
{
    ASSERT( stride>=4*(width+filter_width) );
    ASSERT( ((stride|(4*width)|(4*filter_width)|reinterpret_cast<int>(dst)|reinterpret_cast<int>(filter))&15)==0 );

    int xx_fix = width > filter_width ? 0 : filter_width - width;
    const float *filter_start = filter;
    BYTE* dst_byte = reinterpret_cast<BYTE*>(dst);
    BYTE* end = dst_byte + height*stride;
    for( ; dst_byte<end; dst_byte+=stride )
    {
        float *dst_f = reinterpret_cast<float*>(dst_byte); 
        float *dst2 = dst_f - filter_width;
        float *dst_endr = dst_f + width;
        float *dst_end0 = dst_endr - filter_width;
        float *dst_endl = dst_f - xx_fix;
        ASSERT(xx_fix==0 || dst_end0==dst_endl);

        ASSERT(filter_start == filter);
        filter_start += filter_width;
        const float *filter_end = filter_start;

        for (;dst2<dst_endl;dst2+=4)//left margin
        {
            const float *src = dst_f;
            filter_start -= 4;

            //filter 4
            __m128 src4 = _mm_setzero_ps();/*1 2 3 4*/
            __m128 sum = _mm_setzero_ps();
            for(const float* f=filter_start;f<filter_end;f+=4,src+=4)
            {   
                __m128 src_5_8 = _mm_load_ps(src);/*5 6 7 8*/
                __m128 f4 = _mm_load_ps(f);

                xy_filter_4_inline(src4, src_5_8, f4, sum);

                src4 = src_5_8;
            }
            //store result
            _mm_store_ps(dst2, sum);
        }
        for (;dst2<dst_f;dst2+=4)//if width < filter_width
        {
            const float *src = dst_f;
            filter_start-=4;
            filter_end-=4;

            __m128 src4 = _mm_setzero_ps();/*1 2 3 4*/
            __m128 sum = _mm_setzero_ps();
            __m128 src_5_8, f4;
            for(const float* f=filter_start;f<filter_end;f+=4,src+=4)
            {   
                src_5_8 = _mm_load_ps(src);/*5 6 7 8*/
                f4 = _mm_load_ps(f);

                xy_filter_4_inline(src4, src_5_8, f4, sum);
                src4 = src_5_8;
            }
            src_5_8 = _mm_setzero_ps();
            f4 = _mm_load_ps(filter_end);
            xy_filter_4_inline(src4, src_5_8, f4, sum);
            //store result
            _mm_store_ps(dst2, sum);
        }
        ASSERT(filter_start == filter);
        for (;dst2<dst_end0;dst2+=4)
        {
            const float *src = dst2;

            //filter 4
            __m128 src4 = _mm_load_ps(src);/*1 2 3 4*/
            __m128 sum = _mm_setzero_ps();
            for(const float* f=filter_start;f<filter_end;f+=4)
            {
                src+=4;
                __m128 src_5_8 = _mm_load_ps(src);/*5 6 7 8*/
                __m128 f4 = _mm_load_ps(f);

                xy_filter_4_inline(src4, src_5_8, f4, sum);
                src4 = src_5_8;
            }
            //store result
            _mm_store_ps(dst2, sum);
        }
        for (;dst2<dst_endr;dst2+=4)//right margin
        {
            const float *src = dst2;
            filter_end-=4;

            //filter 4
            __m128 src4 = _mm_load_ps(src);//1 2 3 4
            __m128 sum = _mm_setzero_ps();
            __m128 src_5_8, f4;
            for(const float* f=filter_start;f<filter_end;f+=4)
            {
                src+=4;
                src_5_8 = _mm_load_ps(src);//5 6 7 8
                f4 = _mm_load_ps(f);

                xy_filter_4_inline(src4, src_5_8, f4, sum);

                src4 = src_5_8;
                //move new 4 in_n_out to old 4 in_n_out
            }
            src_5_8 = _mm_setzero_ps();
            f4 = _mm_load_ps(filter_end);
            xy_filter_4_inline(src4, src_5_8, f4, sum);
            //store result
            _mm_store_ps(dst2, sum);
        }
    }
}

/****
 * inline function sometimes generates stupid code
 * 
 * @src4, @src_5_8, @f4, @sum : __m128
 * @src_5_8, @f4: const
 * @sum : output
 * @src4: undefined
 **/ 
#define XY_FILTER_4(src4, src_5_8, f4, sum) \
    __m128 f4_1 = _mm_shuffle_ps(f4, f4, _MM_SHUFFLE(0,0,0,0));\
    f4_1 = _mm_mul_ps(f4_1, src4);\
    sum = _mm_add_ps(sum, f4_1);\
    __m128 src_3_6 = _mm_shuffle_ps(src4, src_5_8, _MM_SHUFFLE(1,0,3,2));/*3 4 5 6*/\
    f4_1 = _mm_shuffle_ps(f4, f4, _MM_SHUFFLE(2,2,2,2));\
    f4_1 = _mm_mul_ps(f4_1, src_3_6);\
    sum = _mm_add_ps(sum, f4_1);\
    src4 = _mm_shuffle_ps(src4, src_3_6, _MM_SHUFFLE(2,1,2,1));/*2 3 4 5*/\
    f4_1 = _mm_shuffle_ps(f4, f4, _MM_SHUFFLE(1,1,1,1));\
    f4_1 = _mm_mul_ps(f4_1, src4);\
    sum = _mm_add_ps(sum, f4_1);\
    src_3_6 = _mm_shuffle_ps(src_3_6, src_5_8, _MM_SHUFFLE(2,1,2,1));/*4 5 6 7*/\
    f4_1 = _mm_shuffle_ps(f4, f4, _MM_SHUFFLE(3,3,3,3));\
    f4_1 = _mm_mul_ps(f4_1, src_3_6);\
    sum = _mm_add_ps(sum, f4_1)


__forceinline void xy_filter_one_line_sse_v4(float *dst, int width, const float *filter, int filter_width)
{
    int xx_fix = width > filter_width ? 0 : filter_width - width;
    const float *filter_start = filter;
    float *dst2 = dst - filter_width;
    float *dst_endr = dst + width;
    float *dst_end0 = dst_endr - filter_width;
    float *dst_endl = dst - xx_fix;
    ASSERT(xx_fix==0 || dst_end0==dst_endl);

    ASSERT(filter_start == filter);
    filter_start += filter_width;
    const float *filter_end = filter_start;

    for (;dst2<dst_endl;dst2+=4)//left margin
    {
        const float *src = dst;
        filter_start -= 4;

        //filter 4
        __m128 src4 = _mm_setzero_ps();/*1 2 3 4*/
        __m128 sum = _mm_setzero_ps();
        for(const float* f=filter_start;f<filter_end;f+=4,src+=4)
        {   
            __m128 src_5_8 = _mm_load_ps(src);/*5 6 7 8*/
            __m128 f4 = _mm_load_ps(f);

            { XY_FILTER_4(src4, src_5_8, f4, sum); }

            src4 = src_5_8;
        }
        //store result
        _mm_store_ps(dst2, sum);
    }
    for (;dst2<dst;dst2+=4)//if width < filter_width
    {
        const float *src = dst;
        filter_start-=4;
        filter_end-=4;

        __m128 src4 = _mm_setzero_ps();/*1 2 3 4*/
        __m128 sum = _mm_setzero_ps();
        __m128 src_5_8, f4;
        for(const float* f=filter_start;f<filter_end;f+=4,src+=4)
        {   
            src_5_8 = _mm_load_ps(src);/*5 6 7 8*/
            f4 = _mm_load_ps(f);

            { XY_FILTER_4(src4, src_5_8, f4, sum); }
            src4 = src_5_8;
        }
        src_5_8 = _mm_setzero_ps();
        f4 = _mm_load_ps(filter_end);
        { XY_FILTER_4(src4, src_5_8, f4, sum); }
        //store result
        _mm_store_ps(dst2, sum);
    }
    ASSERT(filter_start == filter);
    for (;dst2<dst_end0;dst2+=4)
    {
        const float *src = dst2;

        //filter 4
        __m128 src4 = _mm_load_ps(src);/*1 2 3 4*/
        __m128 sum = _mm_setzero_ps();
        for(const float* f=filter_start;f<filter_end;f+=4)
        {
            src+=4;
            __m128 src_5_8 = _mm_load_ps(src);/*5 6 7 8*/
            __m128 f4 = _mm_load_ps(f);

            { XY_FILTER_4(src4, src_5_8, f4, sum); }
            src4 = src_5_8;
        }
        //store result
        _mm_store_ps(dst2, sum);
    }
    for (;dst2<dst_endr;dst2+=4)//right margin
    {
        const float *src = dst2;
        filter_end-=4;

        //filter 4
        __m128 src4 = _mm_load_ps(src);//1 2 3 4
        __m128 sum = _mm_setzero_ps();
        __m128 src_5_8, f4;
        for(const float* f=filter_start;f<filter_end;f+=4)
        {
            src+=4;
            src_5_8 = _mm_load_ps(src);//5 6 7 8
            f4 = _mm_load_ps(f);

            { XY_FILTER_4(src4, src_5_8, f4, sum); }

            src4 = src_5_8;
            //move new 4 in_n_out to old 4 in_n_out
        }
        src_5_8 = _mm_setzero_ps();
        f4 = _mm_load_ps(filter_end);
        { XY_FILTER_4(src4, src_5_8, f4, sum); }
        //store result
        _mm_store_ps(dst2, sum);
    }
}

void xy_filter_sse_v4(float *dst, int width, int height, int stride, const float *filter, int filter_width)
{
    ASSERT( stride>=4*(width+filter_width) );
    ASSERT( ((stride|(4*width)|(4*filter_width)|reinterpret_cast<int>(dst)|reinterpret_cast<int>(filter))&15)==0 );

    BYTE* dst_byte = reinterpret_cast<BYTE*>(dst);
    BYTE* end = dst_byte + height*stride;
    for( ; dst_byte<end; dst_byte+=stride )
    {
        xy_filter_one_line_sse_v4(reinterpret_cast<float*>(dst_byte), width, filter, filter_width);
    }
}

// v5
template<int LENGTH>
struct M128s
{
    __m128 x;
    M128s<LENGTH - 4> next;

    template<int Index> __forceinline __m128& GetAt()
    {
        return next.GetAt<Index - 4>();
    }
    template<> __forceinline __m128& GetAt<0>()
    {
        return x;
    }

    template<int Start, int Offset> __forceinline __m128& GetAt()
    {
        return GetAt<Start + Offset>();
    }

    __forceinline void Load(const float* src)
    {
        x = _mm_load_ps(src);
        next.Load(src+4);
    }
};

template<>
struct M128s<0>
{
    void Load(const float* src)
    {
    }
};

template<int FILTER_LENGTH, int START, int LENGTH>
struct Filter4
{
    static __forceinline void do_cal(__m128& src0_128, const float * src4, M128s<FILTER_LENGTH>& filter128s, __m128& sum)
    {
        Filter4<FILTER_LENGTH,START,LENGTH-4>::do_cal(src0_128, src4, filter128s, sum);
        __m128 src4_128 = _mm_load_ps(src4+LENGTH-4);
        XY_FILTER_4(src0_128, src4_128, filter128s.GetAt<START+LENGTH-4>(), sum);
        src0_128 = src4_128;
    }
};

template<int FILTER_LENGTH, int START>
struct Filter4<FILTER_LENGTH, START, 0>
{
    static __forceinline void do_cal(__m128& src0_128, const float * src4, M128s<FILTER_LENGTH>& filter128s, __m128& sum)
    {
    }
};

template<int FILTER_LENGTH,int MARGIN_LENGTH>
struct FilterAllMargin
{
    static __forceinline void cal_left_margin(float * src, M128s<FILTER_LENGTH>& filter128s)
    {
        //filter 4
        __m128 src0 = _mm_setzero_ps();
        __m128 sum = _mm_setzero_ps();
        Filter4<FILTER_LENGTH,MARGIN_LENGTH-4,FILTER_LENGTH-MARGIN_LENGTH+4>::do_cal(src0, src, filter128s, sum);
        _mm_store_ps(src-MARGIN_LENGTH, sum);
        FilterAllMargin<FILTER_LENGTH,MARGIN_LENGTH-4>::cal_left_margin(src, filter128s);
    }

    static __forceinline void cal_right_margin(float * src, M128s<FILTER_LENGTH>& filter128s)
    {
        //filter 4
        {
            __m128 src0 = _mm_load_ps(src);
            __m128 sum = _mm_setzero_ps();
            Filter4<FILTER_LENGTH,0,MARGIN_LENGTH-4>::do_cal(src0, src+4, filter128s, sum);
            __m128 src4 = _mm_setzero_ps();
            XY_FILTER_4(src0, src4, filter128s.GetAt<MARGIN_LENGTH-4>(), sum);
            //store result
            _mm_store_ps(src, sum);
        }
        FilterAllMargin<FILTER_LENGTH,MARGIN_LENGTH-4>::cal_right_margin(src+4, filter128s);
    }
};

template<int FILTER_LENGTH>
struct FilterAllMargin<FILTER_LENGTH,0>
{
    static __forceinline void cal_left_margin(float * src, M128s<FILTER_LENGTH>& filter128s)
    {
    }
    static __forceinline void cal_right_margin(float * src, M128s<FILTER_LENGTH>& filter128s)
    {
    }
};

/****
 * See @xy_filter_c
 * Constrain:
 *   FILTER_LENGTH%4 == 0 && FILTER_LENGTH<=width
 **/
template<int FILTER_LENGTH>
void xy_filter_sse_template_v0(float *dst, int width, int height, int stride, const float *filter)
{
    ASSERT( stride>=4*(width+FILTER_LENGTH) );
    ASSERT( ((stride|(4*width)|(4*FILTER_LENGTH)|reinterpret_cast<int>(dst)|reinterpret_cast<int>(filter))&15)==0 );

    M128s<FILTER_LENGTH> filter128s;
    filter128s.Load(filter);

    const float *filter_start = filter;
    BYTE* dst_byte = reinterpret_cast<BYTE*>(dst);
    BYTE* end = dst_byte + height*stride;
    for( ; dst_byte<end; dst_byte+=stride )
    {
        float *dst2 = reinterpret_cast<float*>(dst_byte);

        //left margin
        FilterAllMargin<FILTER_LENGTH,FILTER_LENGTH>::cal_left_margin(dst2, filter128s);
        float *dst_end1 = dst2 + width;
        float *dst_end0 = dst_end1 - FILTER_LENGTH;
        for (;dst2<dst_end0;dst2+=4)
        {
            const float *src = dst2;

            //filter 4
            __m128 src0 = _mm_load_ps(src);/*1 2 3 4*/
            src += 4;
            __m128 sum = _mm_setzero_ps();
            Filter4<FILTER_LENGTH,0,FILTER_LENGTH>::do_cal(src0, src, filter128s, sum);
            //store result
            _mm_store_ps(dst2, sum);
        }
        FilterAllMargin<FILTER_LENGTH,FILTER_LENGTH>::cal_right_margin(dst2, filter128s);
    }
}

void xy_filter_sse_v5(float *dst, int width, int height, int stride, const float *filter, int filter_width)
{
    typedef void (*Filter)(float *dst, int width, int height, int stride, const float *filter);
    const Filter filters[] = { xy_filter_sse_template_v0<4>, xy_filter_sse_template_v0<8>, xy_filter_sse_template_v0<12>, xy_filter_sse_template_v0<16>,
        xy_filter_sse_template_v0<20>, xy_filter_sse_template_v0<24>, xy_filter_sse_template_v0<28>
    };
    if (filter_width<=28 && filter_width<=width)
    {
        ASSERT(filter_width%4==0);
        filters[(filter_width-1)/4](dst, width, height, stride, filter);
    }
    else
    {
        xy_filter_sse_v4(dst, width, height, stride, filter, filter_width);
    }
}

// v5 end

__forceinline void xy_filter_one_line_sse_v6(float *dst, int width, const float *filter, int filter_width)
{
    int xx_fix = width > filter_width ? 0 : filter_width - width;
    const float *filter_start = filter;
    float *dst2 = dst - filter_width;
    float *dst_endr = dst + width;
    float *dst_end0 = dst_endr - filter_width - 4;
    float *dst_endl = dst - xx_fix;
    ASSERT(xx_fix==0 || dst_end0==dst_endl-4);

    ASSERT(filter_start == filter);
    filter_start += filter_width;
    const float *filter_end = filter_start;

    for (;dst2<dst_endl;dst2+=4)//left margin
    {
        const float *src = dst;
        filter_start -= 4;

        //filter 4
        __m128 src4 = _mm_setzero_ps();/*1 2 3 4*/
        __m128 sum = _mm_setzero_ps();
        for(const float* f=filter_start;f<filter_end;f+=4,src+=4)
        {   
            __m128 src_5_8 = _mm_load_ps(src);/*5 6 7 8*/
            __m128 f4 = _mm_load_ps(f);

            { XY_FILTER_4(src4, src_5_8, f4, sum); }

            src4 = src_5_8;
        }
        //store result
        _mm_store_ps(dst2, sum);
    }
    for (;dst2<dst;dst2+=4)//if width < filter_width
    {
        const float *src = dst;
        filter_start-=4;
        filter_end-=4;

        __m128 src4 = _mm_setzero_ps();/*1 2 3 4*/
        __m128 sum = _mm_setzero_ps();
        __m128 src_5_8, f4;
        for(const float* f=filter_start;f<filter_end;f+=4,src+=4)
        {   
            src_5_8 = _mm_load_ps(src);/*5 6 7 8*/
            f4 = _mm_load_ps(f);

            { XY_FILTER_4(src4, src_5_8, f4, sum); }
            src4 = src_5_8;
        }
        src_5_8 = _mm_setzero_ps();
        f4 = _mm_load_ps(filter_end);
        { XY_FILTER_4(src4, src_5_8, f4, sum); }
        //store result
        _mm_store_ps(dst2, sum);
    }
    ASSERT(filter_start == filter);
    for (;dst2<dst_end0;dst2+=8)
    {
        const float *src = dst2;           
        const float* f=filter_start;

        //filter 4
        __m128 src4 = _mm_load_ps(src);/*1 2 3 4*/
        src+=4;
        __m128 src_5_8;
        __m128 sum = _mm_setzero_ps();
        __m128 sum2 = _mm_setzero_ps();
        __m128 f4 = _mm_load_ps(f);
        f+=4;
        src_5_8 = _mm_load_ps(src);/*5 6 7 8*/
        src+=4;
        { XY_FILTER_4(src4, src_5_8, f4, sum); }
        for(;f<filter_end;f+=4,src+=4)
        {
            src4 = _mm_load_ps(src);/*1 2 3 4*/
            __m128 tmp = src_5_8;//important! 
            { XY_FILTER_4(tmp, src4, f4, sum2); }

            f4 = _mm_load_ps(f);
            { XY_FILTER_4(src_5_8, src4, f4, sum); }
            src_5_8 = src4;
        }
        src4 = _mm_load_ps(src);/*1 2 3 4*/
        { XY_FILTER_4(src_5_8, src4, f4, sum2); }

        //store result
        _mm_store_ps(dst2, sum);
        _mm_store_ps(dst2+4, sum2);
    }
    if (dst2==dst_end0)
    {
        const float *src = dst2;
        //filter 4
        __m128 src4 = _mm_load_ps(src);//1 2 3 4
        src+=4;
        __m128 sum = _mm_setzero_ps();
        for(const float* f=filter_start;f<filter_end;f+=4,src+=4)
        {
            __m128 src_5_8 = _mm_load_ps(src);//5 6 7 8
            __m128 f4 = _mm_load_ps(f);

            { XY_FILTER_4(src4, src_5_8, f4, sum); }
            src4 = src_5_8;
        }
        //store result
        _mm_store_ps(dst2, sum);
        dst2+=4;
    }
    for (;dst2<dst_endr;dst2+=4)//right margin
    {
        const float *src = dst2;
        filter_end-=4;

        //filter 4
        __m128 src4 = _mm_load_ps(src);//1 2 3 4
        __m128 sum = _mm_setzero_ps();
        __m128 src_5_8, f4;
        for(const float* f=filter_start;f<filter_end;f+=4)
        {
            src+=4;
            src_5_8 = _mm_load_ps(src);//5 6 7 8
            f4 = _mm_load_ps(f);

            { XY_FILTER_4(src4, src_5_8, f4, sum); }

            src4 = src_5_8;
            //move new 4 in_n_out to old 4 in_n_out
        }
        src_5_8 = _mm_setzero_ps();
        f4 = _mm_load_ps(filter_end);
        { XY_FILTER_4(src4, src_5_8, f4, sum); }
        //store result
        _mm_store_ps(dst2, sum);
    }
}

void xy_filter_sse_v6(float *dst, int width, int height, int stride, const float *filter, int filter_width)
{
    ASSERT( stride>=4*(width+filter_width) );
    ASSERT( ((stride|(4*width)|(4*filter_width)|reinterpret_cast<int>(dst)|reinterpret_cast<int>(filter))&15)==0 );

    BYTE* dst_byte = reinterpret_cast<BYTE*>(dst);
    BYTE* end = dst_byte + height*stride;
    for( ; dst_byte<end; dst_byte+=stride )
    {
        xy_filter_one_line_sse_v6(reinterpret_cast<float*>(dst_byte), width, filter, filter_width);
    }
}

// v7

//
// ref: "Comparing floating point numbers" by Bruce Dawson
// http://www.cygnus-software.com/papers/comparingfloats/comparingfloats.htm
//
bool AlmostEqual(float A, float B, int maxUlps=0)
{
    // Make sure maxUlps is non-negative and small enough that the
    // default NAN won't compare as equal to anything.
    ASSERT(maxUlps > 0 && maxUlps < 4 * 1024 * 1024);
    int aInt = *(int*)&A;
    // Make aInt lexicographically ordered as a twos-complement int
    if (aInt < 0)
        aInt = 0x80000000 - aInt;
    // Make bInt lexicographically ordered as a twos-complement int
    int bInt = *(int*)&B;
    if (bInt < 0)
        bInt = 0x80000000 - bInt;

    int intDiff = abs(aInt - bInt);
    if (intDiff <= maxUlps)
        return true;

    return false;
}

/****
 * @src4, @f4_1, @sum : __m128
 * @f4_1: const
 * @sum : output
 * @src4: undefined
 **/ 
#define XY_FILTER_4_1(src4, f4_1, sum) \
    src4 = _mm_mul_ps(src4, f4_1);\
    sum = _mm_add_ps(sum, src4);

/****
 * Constrain:
 *   LENGTH%4 == 0 || LENGTH%4 == 1
 **/
template<int LENGTH>
struct M128s_V1
{
    __m128 x;
    M128s_V1<LENGTH - 4> next;

    template<int Index> __forceinline __m128& GetAt()
    {
        return next.GetAt<Index - 4>();
    }
    template<> __forceinline __m128& GetAt<0>()
    {
        return x;
    }

    template<int Start, int Offset> __forceinline __m128& GetAt()
    {
        return GetAt<Start + Offset>();
    }

    __forceinline void Load(const float* src)
    {
        x = _mm_load_ps(src);
        next.Load(src+4);
    }
};

template<>
struct M128s_V1<1>
{
    __m128 x;

    template<int Index> __forceinline __m128& GetAt()
    {
        return x;
    }
    __forceinline void Load(const float* src)
    {
        x = _mm_set1_ps(*src);
    }
};

template<>
struct M128s_V1<0>
{
    void Load(const float* src)
    {
    }
};

template<int FILTER_LENGTH, int START, int LENGTH>
struct Filter4_V1
{
    static __forceinline void do_cal(__m128& src0_128, const float * src4, M128s_V1<FILTER_LENGTH>& filter128s, __m128& sum)
    {
        __m128 src4_128 = _mm_load_ps(src4);
        { XY_FILTER_4(src0_128, src4_128, filter128s.GetAt<START>(), sum); }
        Filter4_V1<FILTER_LENGTH,START+4,LENGTH-4>::do_cal(src4_128, src4+4, filter128s, sum);
        src0_128 = src4_128;
    }
};

template<int FILTER_LENGTH, int START>
struct Filter4_V1<FILTER_LENGTH, START, 1>
{
    static __forceinline void do_cal(__m128& src0_128, const float * src4, M128s_V1<FILTER_LENGTH>& filter128s, __m128& sum)
    {
        cal_tail<FILTER_LENGTH-START>(src0_128, src4, filter128s, sum);
    }
    template<int TAIL>
    static __forceinline void cal_tail(__m128& src0_128, const float * src4, M128s_V1<FILTER_LENGTH>& filter128s, __m128& sum);
    template<>
    static __forceinline void cal_tail<1>(__m128& src0_128, const float * src4, M128s_V1<FILTER_LENGTH>& filter128s, __m128& sum)
    {
        { XY_FILTER_4_1(src0_128, filter128s.GetAt<FILTER_LENGTH-1>(), sum); }
    }
};

template<int FILTER_LENGTH, int START>
struct Filter4_V1<FILTER_LENGTH, START, 0>
{
    static __forceinline void do_cal(__m128& src0_128, const float * src4, M128s_V1<FILTER_LENGTH>& filter128s, __m128& sum)
    {
    }
};

template<int FILTER_LENGTH,int MARGIN_LENGTH>
struct FilterAllLeftMargin_V1
{
    static __forceinline void cal(float * src, M128s_V1<FILTER_LENGTH>& filter128s)
    {
        do_cal<FILTER_LENGTH%4>(src, filter128s);
    }
    template<int FILTER_TAIL>
    static __forceinline void do_cal(float * src, M128s_V1<FILTER_LENGTH>& filter128s)
    {
        //filter 4
        __m128 src0 = _mm_setzero_ps();
        __m128 sum = _mm_setzero_ps();
        Filter4_V1<FILTER_LENGTH,MARGIN_LENGTH-4,FILTER_LENGTH-MARGIN_LENGTH+4>::do_cal(src0, src, filter128s, sum);
        _mm_store_ps(src-MARGIN_LENGTH, sum);
        FilterAllLeftMargin_V1<FILTER_LENGTH,MARGIN_LENGTH-4>::do_cal<0>(src, filter128s);
    }
    template<>
    static __forceinline void do_cal<1>(float * src, M128s_V1<FILTER_LENGTH>& filter128s)
    {
        //filter 4
        __m128 sum = _mm_setzero_ps();
        //Only one of the last 4 filter coefficiences is non-zero
        _mm_store_ps(src-MARGIN_LENGTH, sum);
        FilterAllLeftMargin_V1<FILTER_LENGTH,MARGIN_LENGTH-4>::do_cal<0>(src, filter128s);
    }
};

template<int FILTER_LENGTH, int MARGIN_LENGTH>
struct FilterAllRightMargin_V1
{
    static __forceinline void cal(float * src, M128s_V1<FILTER_LENGTH>& filter128s)
    {
        do_cal<FILTER_LENGTH%4>(src, filter128s);
    }
    template<int FILTER_TAIL>
    static __forceinline void do_cal(float * src, M128s_V1<FILTER_LENGTH>& filter128s)
    {
        //filter 4
        {
            __m128 src0 = _mm_load_ps(src);
            __m128 sum = _mm_setzero_ps();
            Filter4_V1<FILTER_LENGTH,0,MARGIN_LENGTH-4>::do_cal(src0, src+4, filter128s, sum);
            __m128 src4 = _mm_setzero_ps();
            { XY_FILTER_4(src0, src4, filter128s.GetAt<MARGIN_LENGTH-4>(), sum); }
            //store result
            _mm_store_ps(src, sum);
        }
        FilterAllRightMargin_V1<FILTER_LENGTH,MARGIN_LENGTH-4>::do_cal<0>(src+4, filter128s);
    }
    template<>
    static __forceinline void do_cal<1>(float * src, M128s_V1<FILTER_LENGTH>& filter128s)
    {
        //filter 4
        {
            __m128 src0 = _mm_load_ps(src);
            __m128 sum = _mm_setzero_ps();
            Filter4_V1<FILTER_LENGTH,0,MARGIN_LENGTH-4>::do_cal(src0, src+4, filter128s, sum);
            //Only one of the last 4 filter coefficiences is non-zero
            { XY_FILTER_4_1(src0, filter128s.GetAt<MARGIN_LENGTH-4>(), sum); }
            //store result
            _mm_store_ps(src, sum);
        }
        FilterAllRightMargin_V1<FILTER_LENGTH,MARGIN_LENGTH-4>::do_cal<0>(src+4, filter128s);
    }
};

template<int FILTER_LENGTH>
struct FilterAllLeftMargin_V1<FILTER_LENGTH,0>
{
    template<int FILTER_TAIL>
    static __forceinline void do_cal(float * src, M128s_V1<FILTER_LENGTH>& filter128s)
    {
    }
};

template<int FILTER_LENGTH>
struct FilterAllRightMargin_V1<FILTER_LENGTH,0>
{
    template<int FILTER_TAIL>
    static __forceinline void do_cal(float * src, M128s_V1<FILTER_LENGTH>& filter128s)
    {
    }
};

/****
 * Equivalent:
 *   xy_filter_c(float *dst, int width, int height, int stride, const float *filter, (FILTER_LENGTH+3)&~3 );
 * See @xy_filter_c
 * Constrain:
 *   FILTER_LENGTH<=width && width%4==0
 **/
template<int FILTER_LENGTH>
void xy_filter_sse_template_v1(float *dst, int width, int height, int stride, const float *filter)
{
    ASSERT( stride>=4*(width+FILTER_LENGTH) );
    ASSERT( ((stride|(4*width)|(4*FILTER_LENGTH)|reinterpret_cast<int>(dst)|reinterpret_cast<int>(filter))&15)==0 );

    M128s_V1<FILTER_LENGTH> filter128s;
    filter128s.Load(filter);

    const float *filter_start = filter;
    BYTE* dst_byte = reinterpret_cast<BYTE*>(dst);
    BYTE* end = dst_byte + height*stride;
    for( ; dst_byte<end; dst_byte+=stride )
    {
        float *dst2 = reinterpret_cast<float*>(dst_byte);

        //left margin
        FilterAllLeftMargin_V1<FILTER_LENGTH,((FILTER_LENGTH+3)&~3)>::cal(dst2, filter128s);
        float *dst_end1 = dst2 + width;
        float *dst_end0 = dst_end1 - ((FILTER_LENGTH+3)&~3);
        for (;dst2<dst_end0;dst2+=4)
        {
            const float *src = dst2;

            //filter 4
            __m128 src0 = _mm_load_ps(src);/*1 2 3 4*/
            src += 4;
            __m128 sum = _mm_setzero_ps();
            Filter4_V1<FILTER_LENGTH,0,FILTER_LENGTH>::do_cal(src0, src, filter128s, sum);
            //store result
            _mm_store_ps(dst2, sum);
        }
        FilterAllRightMargin_V1<FILTER_LENGTH,((FILTER_LENGTH+3)&~3)>::cal(dst2, filter128s);
    }
}

/****
 * See @xy_filter_c
 **/
void xy_filter_sse_v7(float *dst, int width, int height, int stride, const float *filter, int filter_width)
{
    typedef void (*Filter)(float *dst, int width, int height, int stride, const float *filter);
    const Filter filters[] = { 
        NULL,
        xy_filter_sse_template_v1<1>, xy_filter_sse_template_v1<4>, xy_filter_sse_template_v1<4>, xy_filter_sse_template_v1<4>, 
        xy_filter_sse_template_v1<5>, xy_filter_sse_template_v1<8>, xy_filter_sse_template_v1<8>, xy_filter_sse_template_v1<8>, 
        xy_filter_sse_template_v1<9>, xy_filter_sse_template_v1<12>, xy_filter_sse_template_v1<12>, xy_filter_sse_template_v1<12>, 
        xy_filter_sse_template_v1<13>, xy_filter_sse_template_v1<16>, xy_filter_sse_template_v1<16>, xy_filter_sse_template_v1<16>,
        xy_filter_sse_template_v1<17>, xy_filter_sse_template_v1<20>, xy_filter_sse_template_v1<20>, xy_filter_sse_template_v1<20>, 
        xy_filter_sse_template_v1<21>, xy_filter_sse_template_v1<24>, xy_filter_sse_template_v1<24>, xy_filter_sse_template_v1<24>, 
        xy_filter_sse_template_v1<25>, xy_filter_sse_template_v1<28>, xy_filter_sse_template_v1<28>, xy_filter_sse_template_v1<28>
    };
    if (filter_width<=28 && filter_width<=width)
    {
        int tmp = filter_width;
        while( AlmostEqual(filter[tmp-1],0.0f) && filter_width-tmp<3)//Skip tail zero, but limited to 3. We cannot support more current.
            tmp--;
        filters[tmp](dst, width, height, stride, filter);
    }
    else
    {
        xy_filter_sse_v6(dst, width, height, stride, filter, filter_width);
    }   
}

// v7 end


// v8

/****
 * @src4, @src_5_8, @f3_1, @f3_2, @sum: __m128
 * @src4, @src_5_8, @f3_1, @f3_2: const
 * @sum: output
 **/
#define XY_3_TAG_SYMMETRIC_FILTER_4(src4, src_5_8, f3_1, f3_2, sum) \
    __m128 src_3_6 = _mm_shuffle_ps(src4, src_5_8, _MM_SHUFFLE(1,0,3,2));/*3 4 5 6*/\
    __m128 src_2_5 = _mm_shuffle_ps(src4, src_3_6, _MM_SHUFFLE(2,1,2,1));/*2 3 4 5*/\
    sum = _mm_mul_ps(f3_1, src4);\
    __m128 mul2 = _mm_mul_ps(f3_2, src_2_5);\
    __m128 mul3 = _mm_mul_ps(f3_1, src_3_6);\
    sum = _mm_add_ps(sum, mul2);\
    sum = _mm_add_ps(sum, mul3);

/****
 * Equivalent:
 *   xy_filter_c(float *dst, int width, int height, int stride, const float *filter, 4 );
 * See @xy_filter_c
 * Constrain:
 *   filter[3] == 0 && filter[0] == filter[2] (symmetric) (&& sum(filter)==1)
 **/
void xy_3_tag_symmetric_filter_sse(float *dst, int width, int height, int stride, const float *filter)
{
    const int filter_width = 4;
    ASSERT( stride>=4*(width+filter_width) );
    ASSERT( ((stride|(4*width)|(4*filter_width)|reinterpret_cast<int>(dst)|reinterpret_cast<int>(filter))&15)==0 );

    ASSERT(filter_width==4 && filter[3]==0 && filter[2]==filter[0]);
    
    __m128 f3_1 = _mm_set1_ps(filter[0]);
    __m128 f3_2 = _mm_set1_ps(filter[1]);

    BYTE* dst_byte = reinterpret_cast<BYTE*>(dst);
    BYTE* end = dst_byte + height*stride;
    for( ; dst_byte<end; dst_byte+=stride )
    {
        float *dst_f = reinterpret_cast<float*>(dst_byte);
        float *dst2 = dst_f;

        float *dst_end0 = dst_f + width - 4;
        //filter 4
        __m128 src4 = _mm_load_ps(dst2);/*1 2 3 4*/
        {
            __m128 sum;
            __m128 src_4 = _mm_setzero_ps();
            { XY_3_TAG_SYMMETRIC_FILTER_4(src_4, src4, f3_1, f3_2, sum); }
            _mm_store_ps(dst2, sum);
        }        
        for (;dst2<dst_end0;dst2+=4)
        {
            __m128 sum;
            __m128 src_5_8 = _mm_load_ps(dst2+4);/*5 6 7 8*/
            { XY_3_TAG_SYMMETRIC_FILTER_4(src4, src_5_8, f3_1, f3_2, sum); }
            src4 = src_5_8;
            //store result
            _mm_store_ps(dst2, sum);
        }
        {
            __m128 sum;
            __m128 src_5_8 = _mm_setzero_ps();/*5 6 7 8*/
            { XY_3_TAG_SYMMETRIC_FILTER_4(src4, src_5_8, f3_1, f3_2, sum); }
            src4 = src_5_8;
            //store result
            _mm_store_ps(dst2, sum);
        }
    }
}

void xy_filter_sse_v8(float *dst, int width, int height, int stride, const float *filter, int filter_width)
{
    typedef void (*Filter)(float *dst, int width, int height, int stride, const float *filter);
    const Filter filters[] = { 
        NULL,
        xy_filter_sse_template_v1<1>, xy_filter_sse_template_v1<4>, xy_filter_sse_template_v1<4>, xy_filter_sse_template_v1<4>, 
        xy_filter_sse_template_v1<5>, xy_filter_sse_template_v1<8>, xy_filter_sse_template_v1<8>, xy_filter_sse_template_v1<8>, 
        xy_filter_sse_template_v1<9>, xy_filter_sse_template_v1<12>, xy_filter_sse_template_v1<12>, xy_filter_sse_template_v1<12>, 
        xy_filter_sse_template_v1<13>, xy_filter_sse_template_v1<16>, xy_filter_sse_template_v1<16>, xy_filter_sse_template_v1<16>,
        xy_filter_sse_template_v1<17>, xy_filter_sse_template_v1<20>, xy_filter_sse_template_v1<20>, xy_filter_sse_template_v1<20>, 
        xy_filter_sse_template_v1<21>, xy_filter_sse_template_v1<24>, xy_filter_sse_template_v1<24>, xy_filter_sse_template_v1<24>, 
        xy_filter_sse_template_v1<25>, xy_filter_sse_template_v1<28>, xy_filter_sse_template_v1<28>, xy_filter_sse_template_v1<28>
    };
    if (filter_width<=28 && filter_width<=width)
    {
        int tmp = filter_width;
        // Skip tail zero, but we cannot (and don't have to) support more than 3 tail zeros currently.
        while( AlmostEqual(filter[tmp-1],0.0f) && filter_width-tmp<3 )
            tmp--;
        if (tmp==3&&filter[0]==filter[2])
        {
            xy_3_tag_symmetric_filter_sse(dst, width, height, stride, filter);
        }
        else
        {
            filters[tmp](dst, width, height, stride, filter);
        }
    }
    else
    {
        xy_filter_sse_v6(dst, width, height, stride, filter, filter_width);
    }   
}

// v8 end

// v9


/****
 * inline function sometimes generates stupid code
 * 
 * @src4, @src_5_8, @f4, @sum : __m128
 * @src_5_8, @f4: const
 * @sum : output
 * @src4: undefined
 **/ 
#define XY_FILTER_4_3(src4, src_5_8, f4, sum) \
    __m128 f4_1 = _mm_shuffle_ps(f4, f4, _MM_SHUFFLE(0,0,0,0));\
    f4_1 = _mm_mul_ps(f4_1, src4);\
    sum = _mm_add_ps(sum, f4_1);\
    __m128 src_3_6 = _mm_shuffle_ps(src4, src_5_8, _MM_SHUFFLE(1,0,3,2));/*3 4 5 6*/\
    f4_1 = _mm_shuffle_ps(f4, f4, _MM_SHUFFLE(2,2,2,2));\
    f4_1 = _mm_mul_ps(f4_1, src_3_6);\
    sum = _mm_add_ps(sum, f4_1);\
    src4 = _mm_shuffle_ps(src4, src_3_6, _MM_SHUFFLE(2,1,2,1));/*2 3 4 5*/\
    f4_1 = _mm_shuffle_ps(f4, f4, _MM_SHUFFLE(1,1,1,1));\
    f4_1 = _mm_mul_ps(f4_1, src4);\
    sum = _mm_add_ps(sum, f4_1);

/****
 * Constrain:
 *   LENGTH%4 == 0 || LENGTH%4 == 1 || LENGTH%4 == 3
 **/
template<int LENGTH>
struct M128s_V2
{
    __m128 x;
    M128s_V2<LENGTH - 4> next;

    template<int Index> __forceinline __m128& GetAt()
    {
        return next.GetAt<Index - 4>();
    }
    template<> __forceinline __m128& GetAt<0>()
    {
        return x;
    }

    template<int Start, int Offset> __forceinline __m128& GetAt()
    {
        return GetAt<Start + Offset>();
    }

    __forceinline void Load(const float* src)
    {
        x = _mm_load_ps(src);
        next.Load(src+4);
    }
};

template<>
struct M128s_V2<3>
{
    __m128 x;

    template<int Index> __forceinline __m128& GetAt()
    {
        return x;
    }
    __forceinline void Load(const float* src)
    {
        x = _mm_load_ps(src);
    }
};

template<>
struct M128s_V2<1>
{
    __m128 x;

    template<int Index> __forceinline __m128& GetAt()
    {
        return x;
    }
    __forceinline void Load(const float* src)
    {
        x = _mm_set1_ps(*src);
    }
};

template<>
struct M128s_V2<0>
{
    void Load(const float* src)
    {
    }
};

template<int FILTER_LENGTH, int START, int LENGTH>
struct Filter4_V2
{
    static __forceinline void do_cal(__m128& src0_128, const float * src4, M128s_V2<FILTER_LENGTH>& filter128s, __m128& sum)
    {
        __m128 src4_128 = _mm_load_ps(src4);
        { XY_FILTER_4(src0_128, src4_128, filter128s.GetAt<START>(), sum); }
        Filter4_V2<FILTER_LENGTH,START+4,LENGTH-4>::do_cal(src4_128, src4+4, filter128s, sum);
        src0_128 = src4_128;
    }
};

template<int FILTER_LENGTH, int START>
struct Filter4_V2<FILTER_LENGTH, START, 3>
{
    static __forceinline void do_cal(__m128& src0_128, const float * src4, M128s_V2<FILTER_LENGTH>& filter128s, __m128& sum)
    {
        cal_tail<FILTER_LENGTH-START>(src0_128, src4, filter128s, sum);
    }
    template<int TAIL>
    static __forceinline void cal_tail(__m128& src0_128, const float * src4, M128s_V2<FILTER_LENGTH>& filter128s, __m128& sum);
    template<>
    static __forceinline void cal_tail<3>(__m128& src0_128, const float * src4, M128s_V2<FILTER_LENGTH>& filter128s, __m128& sum)
    {
        __m128 src4_128 = _mm_load_ps(src4);
        { XY_FILTER_4_3(src0_128, src4_128, filter128s.GetAt<START>(), sum); }
        src0_128 = src4_128;
    }
};

template<int FILTER_LENGTH, int START>
struct Filter4_V2<FILTER_LENGTH, START, 1>
{
    static __forceinline void do_cal(__m128& src0_128, const float * src4, M128s_V2<FILTER_LENGTH>& filter128s, __m128& sum)
    {
        cal_tail<FILTER_LENGTH-START>(src0_128, src4, filter128s, sum);
    }
    template<int TAIL>
    static __forceinline void cal_tail(__m128& src0_128, const float * src4, M128s_V2<FILTER_LENGTH>& filter128s, __m128& sum);
    template<>
    static __forceinline void cal_tail<1>(__m128& src0_128, const float * src4, M128s_V2<FILTER_LENGTH>& filter128s, __m128& sum)
    {
        { XY_FILTER_4_1(src0_128, filter128s.GetAt<START>(), sum); }
    }
};

template<int FILTER_LENGTH, int START>
struct Filter4_V2<FILTER_LENGTH, START, 0>
{
    static __forceinline void do_cal(__m128& src0_128, const float * src4, M128s_V2<FILTER_LENGTH>& filter128s, __m128& sum)
    {
    }
};

template<int FILTER_LENGTH,int MARGIN_LENGTH>
struct FilterAllLeftMargin_V2
{
    static __forceinline void cal(float * src, M128s_V2<FILTER_LENGTH>& filter128s)
    {
        do_cal<FILTER_LENGTH%4>(src, filter128s);
    }
    template<int FILTER_TAIL>
    static __forceinline void do_cal(float * src, M128s_V2<FILTER_LENGTH>& filter128s)
    {
        //filter 4
        __m128 src0 = _mm_setzero_ps();
        __m128 sum = _mm_setzero_ps();
        Filter4_V2<FILTER_LENGTH,MARGIN_LENGTH-4,FILTER_LENGTH-MARGIN_LENGTH+4>::do_cal(src0, src, filter128s, sum);
        _mm_store_ps(src-MARGIN_LENGTH, sum);
        FilterAllLeftMargin_V2<FILTER_LENGTH,MARGIN_LENGTH-4>::do_cal<0>(src, filter128s);
    }
    template<>
    static __forceinline void do_cal<1>(float * src, M128s_V2<FILTER_LENGTH>& filter128s)
    {
        //filter 4
        __m128 sum = _mm_setzero_ps();
        //Only one of the last 4 filter coefficiences is non-zero
        _mm_store_ps(src-MARGIN_LENGTH, sum);
        FilterAllLeftMargin_V2<FILTER_LENGTH,MARGIN_LENGTH-4>::do_cal<0>(src, filter128s);
    }
};

template<int FILTER_LENGTH, int MARGIN_LENGTH>
struct FilterAllRightMargin_V2
{
    static __forceinline void cal(float * src, M128s_V2<FILTER_LENGTH>& filter128s)
    {
        do_cal<FILTER_LENGTH%4>(src, filter128s);
    }
    template<int FILTER_TAIL>
    static __forceinline void do_cal(float * src, M128s_V2<FILTER_LENGTH>& filter128s)
    {
        //filter 4
        {
            __m128 src0 = _mm_load_ps(src);
            __m128 sum = _mm_setzero_ps();
            Filter4_V2<FILTER_LENGTH,0,MARGIN_LENGTH-4>::do_cal(src0, src+4, filter128s, sum);
            __m128 src4 = _mm_setzero_ps();
            { XY_FILTER_4(src0, src4, filter128s.GetAt<MARGIN_LENGTH-4>(), sum); }
            //store result
            _mm_store_ps(src, sum);
        }
        FilterAllRightMargin_V2<FILTER_LENGTH,MARGIN_LENGTH-4>::do_cal<0>(src+4, filter128s);
    }
    template<>
    static __forceinline void do_cal<3>(float * src, M128s_V2<FILTER_LENGTH>& filter128s)
    {
        //filter 4
        {
            __m128 src0 = _mm_load_ps(src);
            __m128 sum = _mm_setzero_ps();
            Filter4_V2<FILTER_LENGTH,0,MARGIN_LENGTH-4>::do_cal(src0, src+4, filter128s, sum);
            __m128 src4 = _mm_setzero_ps();
            { XY_FILTER_4_3(src0, src4, filter128s.GetAt<MARGIN_LENGTH-4>(), sum); }
            //store result
            _mm_store_ps(src, sum);
        }
        FilterAllRightMargin_V2<FILTER_LENGTH,MARGIN_LENGTH-4>::do_cal<0>(src+4, filter128s);
    }
    template<>
    static __forceinline void do_cal<1>(float * src, M128s_V2<FILTER_LENGTH>& filter128s)
    {
        //filter 4
        {
            __m128 src0 = _mm_load_ps(src);
            __m128 sum = _mm_setzero_ps();
            Filter4_V2<FILTER_LENGTH,0,MARGIN_LENGTH-4>::do_cal(src0, src+4, filter128s, sum);
            //Only one of the last 4 filter coefficiences is non-zero
            { XY_FILTER_4_1(src0, filter128s.GetAt<MARGIN_LENGTH-4>(), sum); }
            //store result
            _mm_store_ps(src, sum);
        }
        FilterAllRightMargin_V2<FILTER_LENGTH,MARGIN_LENGTH-4>::do_cal<0>(src+4, filter128s);
    }
};

template<int FILTER_LENGTH>
struct FilterAllLeftMargin_V2<FILTER_LENGTH,0>
{
    template<int FILTER_TAIL>
    static __forceinline void do_cal(float * src, M128s_V2<FILTER_LENGTH>& filter128s)
    {
    }
};

template<int FILTER_LENGTH>
struct FilterAllRightMargin_V2<FILTER_LENGTH,0>
{
    template<int FILTER_TAIL>
    static __forceinline void do_cal(float * src, M128s_V2<FILTER_LENGTH>& filter128s)
    {
    }
};

/****
 * Equivalent:
 *   xy_filter_c(float *dst, int width, int height, int stride, const float *filter, (FILTER_LENGTH+3)&~3 );
 * See @xy_filter_c
 * Constrain:
 *   FILTER_LENGTH<=width && width%4==0
 **/
template<int FILTER_LENGTH>
void xy_filter_sse_template_v2(float *dst, int width, int height, int stride, const float *filter)
{
    ASSERT( stride>=4*(width+FILTER_LENGTH) );
    ASSERT( ((stride|(4*width)|(4*FILTER_LENGTH)|reinterpret_cast<int>(dst)|reinterpret_cast<int>(filter))&15)==0 );

    M128s_V2<FILTER_LENGTH> filter128s;
    filter128s.Load(filter);

    const float *filter_start = filter;
    BYTE* dst_byte = reinterpret_cast<BYTE*>(dst);
    BYTE* end = dst_byte + height*stride;
    for( ; dst_byte<end; dst_byte+=stride )
    {
        float *dst2 = reinterpret_cast<float*>(dst_byte);

        //left margin
        FilterAllLeftMargin_V2<FILTER_LENGTH,((FILTER_LENGTH+3)&~3)>::cal(dst2, filter128s);
        float *dst_end1 = dst2 + width;
        float *dst_end0 = dst_end1 - ((FILTER_LENGTH+3)&~3);
        for (;dst2<dst_end0;dst2+=4)
        {
            const float *src = dst2;

            //filter 4
            __m128 src0 = _mm_load_ps(src);/*1 2 3 4*/
            src += 4;
            __m128 sum = _mm_setzero_ps();
            Filter4_V2<FILTER_LENGTH,0,FILTER_LENGTH>::do_cal(src0, src, filter128s, sum);
            //store result
            _mm_store_ps(dst2, sum);
        }
        FilterAllRightMargin_V2<FILTER_LENGTH,((FILTER_LENGTH+3)&~3)>::cal(dst2, filter128s);
    }
}

void xy_filter_sse_v9(float *dst, int width, int height, int stride, const float *filter, int filter_width)
{
    typedef void (*Filter)(float *dst, int width, int height, int stride, const float *filter);
    const Filter filters[] = { 
        NULL,
        xy_filter_sse_template_v2<1>, xy_filter_sse_template_v2<4>, xy_filter_sse_template_v2<3>, xy_filter_sse_template_v2<4>, 
        xy_filter_sse_template_v2<5>, xy_filter_sse_template_v2<8>, xy_filter_sse_template_v2<7>, xy_filter_sse_template_v2<8>, 
        xy_filter_sse_template_v2<9>, xy_filter_sse_template_v2<12>, xy_filter_sse_template_v2<11>, xy_filter_sse_template_v2<12>, 
        xy_filter_sse_template_v2<13>, xy_filter_sse_template_v2<16>, xy_filter_sse_template_v2<15>, xy_filter_sse_template_v2<16>,
        xy_filter_sse_template_v2<17>, xy_filter_sse_template_v2<20>, xy_filter_sse_template_v2<19>, xy_filter_sse_template_v2<20>, 
        xy_filter_sse_template_v2<21>, xy_filter_sse_template_v2<24>, xy_filter_sse_template_v2<23>, xy_filter_sse_template_v2<24>, 
        xy_filter_sse_template_v2<25>, xy_filter_sse_template_v2<28>, xy_filter_sse_template_v2<27>, xy_filter_sse_template_v2<28>
    };
    if (filter_width<=28 && filter_width<=width)
    {
        int tmp = filter_width;
        // Skip tail zero, but we cannot (and don't have to) support more than 3 tail zeros currently.
        while( AlmostEqual(filter[tmp-1],0.0f) && filter_width-tmp<3 )
            tmp--;
        if (tmp==3&&filter[0]==filter[2])
        {
            xy_3_tag_symmetric_filter_sse(dst, width, height, stride, filter);
        }
        else
        {
            filters[tmp](dst, width, height, stride, filter);
        }
    }
    else
    {
        xy_filter_sse_v6(dst, width, height, stride, filter, filter_width);
    }   
}

// v9 end

class XyFilterTest : public ::testing::Test 
{
public:
    static const int MAX_FILTER_LENGTH = 256;
    static const int MAX_DATA_BUFF_SIZE = 512*512;
    static const int MAX_BUFF_SIZE = MAX_DATA_BUFF_SIZE + 2*MAX_FILTER_LENGTH;

    float *buff_base;

    float *filter_f;

    float *data;
    int w, h, pitch;
    int filter_width;
    int ex_filter_width;

    XyFilterTest():buff_base(NULL),filter_f(NULL),data(NULL)
    {
        buff_base = (float*)xy_malloc(MAX_BUFF_SIZE*sizeof(float));
        data = buff_base + MAX_FILTER_LENGTH;
        filter_f = data + MAX_DATA_BUFF_SIZE;
    }
    ~XyFilterTest()
    {
        xy_free(buff_base);buff_base=NULL;
    }

    const XyFilterTest& copy (const XyFilterTest& rhs)
    {
        w = rhs.w;
        h = rhs.h;
        pitch = rhs.pitch;
        filter_width = rhs.filter_width;
        ex_filter_width = rhs.ex_filter_width;

        memcpy(buff_base, rhs.buff_base, MAX_BUFF_SIZE*sizeof(float));        
        return *this;
    }

    void FillRandData(int w, int h, int filter_width)
    {
        this->w = w;
        this->h = h;
        this->filter_width = filter_width;
        this->ex_filter_width = ((filter_width+3)&~3);        
        this->pitch = ((w+ex_filter_width+3)&~3)*sizeof(float);

        ASSERT(this->pitch*this->h <= MAX_DATA_BUFF_SIZE);

        for (int i=0;i<h;i++)
        {
            float *data2 = (float*)((PUINT8)data + i*pitch);
            for (int j=0;j<w+filter_width;j++)
            {
                data2[j] = rand()&0xFF;
            }
        }
        float sum = 0;
        for (int i=0;i<filter_width/2+1;i++)
        {
            filter_f[i] = abs((rand()&0xFFFFF)+0xFFFFF);
            sum += filter_f[i];
        }
        for (int i=filter_width/2+1;i<filter_width;i++)
        {
            filter_f[i] = filter_f[filter_width-i-1];
            sum += filter_f[i];
        }
        for (int i=0;i<filter_width;i++)
        {
            filter_f[i] /= sum;
        }
        for (int i=filter_width;i<ex_filter_width;i++)
        {
            filter_f[i] = 0;
        }
    }
protected:
    virtual void SetUp() 
    {
    }
private:
    XyFilterTest(const XyFilterTest&);
    const XyFilterTest& operator= (const XyFilterTest& rhs);
};

#define FilterTest(width, height, FILTER_LENGTH, loop_num, function) \
TEST_F(XyFilterTest, function ## _ ## width ## _ ## height ## _ ## FILTER_LENGTH ## _ ## loop_num ) \
{\
    FillRandData(width, height, FILTER_LENGTH);\
    for(int i=0;i<loop_num;i++)\
    {\
        function(data, w, h, pitch, filter_f, (FILTER_LENGTH+3)&~3);\
    }\
    /*ASSERT_EQ(data[-filter_width],0)*/;/*anit compiler optimize*/\
    ASSERT_EQ(0,0);\
}

#define DUAL_TEST(width, height, FILTER_LENGTH, loop_num) \
    FilterTest(width, height, FILTER_LENGTH, loop_num, xy_filter_sse_v8) \
    FilterTest(width, height, FILTER_LENGTH, loop_num, xy_filter_sse_v9)

//DUAL_TEST(128, 16, 3, 30000)
//    DUAL_TEST(256, 16, 3, 30000)
//    DUAL_TEST(512, 16, 3, 30000)
//    DUAL_TEST(128, 32, 3, 30000)
//    DUAL_TEST(256, 32, 3, 30000)
//    DUAL_TEST(512, 32, 3, 30000)
//    DUAL_TEST(128, 64, 3, 30000)
//    DUAL_TEST(256, 64, 3, 30000)
//    DUAL_TEST(512, 64, 3, 30000)
DUAL_TEST(128, 16, 19, 20000)
    DUAL_TEST(256, 16, 19, 20000)
    DUAL_TEST(512, 16, 19, 20000)
    DUAL_TEST(128, 32, 19, 20000)
    DUAL_TEST(256, 32, 19, 20000)
    DUAL_TEST(512, 32, 19, 20000)
    DUAL_TEST(128, 64, 19, 20000)
    DUAL_TEST(256, 64, 19, 20000)
    DUAL_TEST(512, 64, 19, 20000)
//DUAL_TEST(128, 16, 20, 20000)
//    DUAL_TEST(256, 16, 20, 20000)
//    DUAL_TEST(512, 16, 20, 20000)
//    DUAL_TEST(128, 32, 20, 20000)
//    DUAL_TEST(256, 32, 20, 20000)
//    DUAL_TEST(512, 32, 20, 20000)
//    DUAL_TEST(128, 64, 20, 20000)
//    DUAL_TEST(256, 64, 20, 20000)
//    DUAL_TEST(512, 64, 20, 20000)
//DUAL_TEST(128, 16, 21, 20000)
//    DUAL_TEST(256, 16, 21, 20000)
//    DUAL_TEST(512, 16, 21, 20000)
//    DUAL_TEST(128, 32, 21, 20000)
//    DUAL_TEST(256, 32, 21, 20000)
//    DUAL_TEST(512, 32, 21, 20000)
//    DUAL_TEST(128, 64, 21, 20000)
//    DUAL_TEST(256, 64, 21, 20000)
//    DUAL_TEST(512, 64, 21, 20000)

//////////////////////////////////////////////////////////////////////

// transpose test

void xy_float_2_float_transpose_c_v0(float *dst, int dst_width, int dst_stride, 
    const float *src, int width, int height, int src_stride)
{
    ASSERT(dst_width >= height);
    PUINT8 dst_byte = reinterpret_cast<PUINT8>(dst);
    const float* src_end = src + width;
    PCUINT8 src2_end = reinterpret_cast<PCUINT8>(src) + height*src_stride;
    for( ; src<src_end; src++, dst_byte+=dst_stride )
    {
        PCUINT8 src2 = reinterpret_cast<PCUINT8>(src);

        float *dst2 = reinterpret_cast<float*>(dst_byte);        
        for (;src2<src2_end;src2+=src_stride,dst2++)
        {
            *dst2 = *reinterpret_cast<const float*>(src2);
        }
        float *dst2_end = reinterpret_cast<float*>(dst_byte) + dst_width;
        for (;dst2<dst2_end;dst2++)
        {
            *dst2 = 0;
        }
    }
}

void xy_float_2_float_transpose_sse_v0(float *dst, int dst_width, int dst_stride, 
    const float *src, int width, int height, int src_stride)
{
    typedef float DstT;
    typedef const float SrcT;

    ASSERT( (((int)dst|dst_stride)&15)==0 );
    ASSERT(dst_width >= height);
    PUINT8 dst_byte = reinterpret_cast<PUINT8>(dst);
    SrcT* src_end = src + width;
    PCUINT8 src2_end1 = reinterpret_cast<PCUINT8>(src) + (height&~3)*src_stride;
    PCUINT8 src2_end2 = reinterpret_cast<PCUINT8>(src) + height*src_stride;
    for( ; src<src_end; src++, dst_byte+=dst_stride )
    {
        PCUINT8 src2 = reinterpret_cast<PCUINT8>(src);

        DstT *dst2 = reinterpret_cast<DstT*>(dst_byte);
        for (;src2<src2_end1;src2+=4*src_stride,dst2+=4)
        {
            __m128 m1 = _mm_set_ps(
                *(SrcT*)(src2+3*src_stride), 
                *(SrcT*)(src2+2*src_stride), 
                *(SrcT*)(src2+src_stride), 
                *(SrcT*)(src2));
            _mm_store_ps(dst2, m1);
        }
        for (;src2<src2_end2;src2+=src_stride,dst2++)
        {
            *dst2 = *reinterpret_cast<SrcT*>(src2);
        }
        float *dst2_end = reinterpret_cast<DstT*>(dst_byte) + dst_width;
        for (;dst2<dst2_end;dst2++)
        {
            *dst2 = 0;
        }
    }
}

void float_2_byte_transpose_v0(UINT8 *dst, int dst_width, int dst_stride, 
    const float *src, int width, int height, int src_stride)
{
    ASSERT(dst_width >= height);
    PUINT8 dst_byte = reinterpret_cast<PUINT8>(dst);
    const float* src_end = src + width;
    PCUINT8 src2_end = reinterpret_cast<PCUINT8>(src) + height*src_stride;
    for( ; src<src_end; src++, dst_byte+=dst_stride )
    {
        PCUINT8 src2 = reinterpret_cast<PCUINT8>(src);

        UINT8 *dst2 = reinterpret_cast<UINT8*>(dst_byte);
        for (;src2<src2_end;src2+=src_stride,dst2++)
        {
            *dst2 = static_cast<UINT8>(*reinterpret_cast<const float*>(src2)+0.5);
        }
        UINT8 *dst2_end = reinterpret_cast<UINT8*>(dst_byte) + dst_width;
        for (;dst2<dst2_end;dst2++)
        {
            *dst2 = 0;
        }
    }
}

void float_2_byte_transpose_sse_v0(UINT8 *dst, int dst_width, int dst_stride, 
    const float *src, int width, int height, int src_stride)
{
    typedef UINT8 DstT;
    typedef const float SrcT;

    ASSERT(dst_width >= height);
    PUINT8 dst_byte = reinterpret_cast<PUINT8>(dst);
    SrcT* src_end = src + width;
    PCUINT8 src2_end00 = reinterpret_cast<PCUINT8>(src) + (height&~15)*src_stride;
    PCUINT8 src2_end = reinterpret_cast<PCUINT8>(src) + height*src_stride;
    for( ; src<src_end; src++, dst_byte+=dst_stride )
    {
        PCUINT8 src2 = reinterpret_cast<PCUINT8>(src);

        DstT *dst2 = reinterpret_cast<DstT*>(dst_byte);
        for (;src2<src2_end00;src2+=16*src_stride,dst2+=16)
        {
            __m128 m1 = _mm_set_ps(
                *(SrcT*)(src2+3*src_stride),
                *(SrcT*)(src2+2*src_stride), 
                *(SrcT*)(src2+src_stride), 
                *(SrcT*)(src2));
            __m128 m2 = _mm_set_ps(
                *(SrcT*)(src2+7*src_stride),
                *(SrcT*)(src2+6*src_stride),
                *(SrcT*)(src2+5*src_stride),
                *(SrcT*)(src2+4*src_stride));
            __m128 m3 = _mm_set_ps(
                *(SrcT*)(src2+11*src_stride),
                *(SrcT*)(src2+10*src_stride),
                *(SrcT*)(src2+9*src_stride), 
                *(SrcT*)(src2+8*src_stride));
            __m128 m4 = _mm_set_ps(
                *(SrcT*)(src2+15*src_stride),
                *(SrcT*)(src2+14*src_stride),
                *(SrcT*)(src2+13*src_stride),
                *(SrcT*)(src2+12*src_stride));

            __m128i i1 = _mm_cvtps_epi32(m1);
            __m128i i2 = _mm_cvtps_epi32(m2);
            __m128i i3 = _mm_cvtps_epi32(m3);
            __m128i i4 = _mm_cvtps_epi32(m4);

            i1 = _mm_packs_epi32(i1,i2);
            i3 = _mm_packs_epi32(i3,i4);
            i1 = _mm_packus_epi16(i1,i3);

            _mm_store_si128((__m128i*)dst2, i1);
        }
        for (;src2<src2_end;src2+=src_stride,dst2++)
        {
            *dst2 = static_cast<DstT>(*reinterpret_cast<SrcT*>(src2)+0.5);
        }
        DstT *dst2_end = reinterpret_cast<DstT*>(dst_byte) + dst_width;
        for (;dst2<dst2_end;dst2++)
        {
            *dst2 = 0;
        }
    }
}

void byte_2_float_c_v0(float *dst, int dst_width, int dst_stride, 
    PCUINT8 src, int width, int height, int stride)
{
    PUINT8 dst_byte = reinterpret_cast<PUINT8>(dst);

    PCUINT8 src_end = src + height*stride;
    for( ; src<src_end; src+=stride, dst_byte+=dst_stride )
    {
        PCUINT8 src2 = src;
        PCUINT8 src_end = src2 + width;
        float *dst2 = reinterpret_cast<float*>(dst_byte);

        for (;src2<src_end;src2++, dst2++)
        {
            *dst2 = *src2;
        }
        float *dst2_end=reinterpret_cast<float*>(dst_byte)+dst_width;
        for (;dst2<dst2_end;dst2++)
        {
            *dst2=0;
        }
    }
}

void byte_2_float_sse_v0(float *dst, int dst_width, int dst_stride, 
    PCUINT8 src, int width, int height, int stride)
{
    ASSERT( dst_width>=width );
    ASSERT( ((reinterpret_cast<int>(dst)|dst_stride)&15)==0 );
    PUINT8 dst_byte = reinterpret_cast<PUINT8>(dst);

    PCUINT8 src_end = src + height*stride;
    for( ; src<src_end; src+=stride, dst_byte+=dst_stride )
    {
        PCUINT8 src2 = src;
        PCUINT8 src2_end0 = src2 + (width&~15); 
        float *dst2 = reinterpret_cast<float*>(dst_byte);

        for (;src2<src2_end0;src2+=16, dst2+=16)
        {
            //filter 4
            __m128i src16 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src2));
            __m128i src16_lo = _mm_unpacklo_epi8(src16, _mm_setzero_si128()); 
            __m128i src16_hi = _mm_unpackhi_epi8(src16, _mm_setzero_si128());
            __m128i src16_lo_lo = _mm_unpacklo_epi8(src16_lo, _mm_setzero_si128());
            __m128i src16_lo_hi = _mm_unpackhi_epi8(src16_lo, _mm_setzero_si128());
            __m128i src16_hi_lo = _mm_unpacklo_epi8(src16_hi, _mm_setzero_si128());
            __m128i src16_hi_hi = _mm_unpackhi_epi8(src16_hi, _mm_setzero_si128());
            __m128 dst_f1 =  _mm_cvtepi32_ps(src16_lo_lo);
            __m128 dst_f2 =  _mm_cvtepi32_ps(src16_lo_hi);
            __m128 dst_f3 =  _mm_cvtepi32_ps(src16_hi_lo);
            __m128 dst_f4 =  _mm_cvtepi32_ps(src16_hi_hi);
            _mm_stream_ps(dst2, dst_f1);
            _mm_stream_ps(dst2+4, dst_f2);
            _mm_stream_ps(dst2+8, dst_f3);
            _mm_stream_ps(dst2+12, dst_f4);
        }
        PCUINT8 src2_end = src + width;
        for (;src2<src2_end;src2++,dst2++)
        {
            *dst2 = *src2;
        }
        float *dst2_end=reinterpret_cast<float*>(dst_byte)+dst_width;
        for (;dst2<dst2_end;dst2++)
        {
            *dst2=0;
        }
    }
}

void byte_2_float_sse_v1(float *dst, int dst_width, int dst_stride, 
    PCUINT8 src, int width, int height, int stride)
{
    ASSERT( dst_width>=width );
    ASSERT( ((reinterpret_cast<int>(dst)|dst_stride)&15)==0 );
    PUINT8 dst_byte = reinterpret_cast<PUINT8>(dst);

    PCUINT8 src_end = src + height*stride;
    for( ; src<src_end; src+=stride, dst_byte+=dst_stride )
    {
        PCUINT8 src2 = src;
        PCUINT8 src2_end0 = src2 + (width&~15); 
        float *dst2 = reinterpret_cast<float*>(dst_byte);

        for (;src2<src2_end0;src2+=16, dst2+=16)
        {
            //filter 4
            __m128i src16 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src2));
            __m128i src16_lo = _mm_unpacklo_epi8(src16, _mm_setzero_si128()); 
            __m128i src16_hi = _mm_unpackhi_epi8(src16, _mm_setzero_si128());
            __m128i src16_lo_lo = _mm_unpacklo_epi8(src16_lo, _mm_setzero_si128());
            __m128i src16_lo_hi = _mm_unpackhi_epi8(src16_lo, _mm_setzero_si128());
            __m128i src16_hi_lo = _mm_unpacklo_epi8(src16_hi, _mm_setzero_si128());
            __m128i src16_hi_hi = _mm_unpackhi_epi8(src16_hi, _mm_setzero_si128());
            __m128 dst_f1 =  _mm_cvtepi32_ps(src16_lo_lo);
            __m128 dst_f2 =  _mm_cvtepi32_ps(src16_lo_hi);
            __m128 dst_f3 =  _mm_cvtepi32_ps(src16_hi_lo);
            __m128 dst_f4 =  _mm_cvtepi32_ps(src16_hi_hi);
            _mm_store_ps(dst2, dst_f1);
            _mm_store_ps(dst2+4, dst_f2);
            _mm_store_ps(dst2+8, dst_f3);
            _mm_store_ps(dst2+12, dst_f4);
        }
        PCUINT8 src2_end = src + width;
        for (;src2<src2_end;src2++,dst2++)
        {
            *dst2 = *src2;
        }
        float *dst2_end=reinterpret_cast<float*>(dst_byte)+dst_width;
        for (;dst2<dst2_end;dst2++)
        {
            *dst2=0;
        }
    }
}

class XyTransposeTest : public ::testing::Test 
{
public:
    static const int MAX_FILTER_LENGTH = 128;
    static const int MAX_DATA_BUFF_SIZE = 512*128;

    UINT8 data_byte_base[MAX_DATA_BUFF_SIZE+15];
    UINT8 *data_b;

    float data_base_f[2*MAX_DATA_BUFF_SIZE+3];
    float *data_f;
    float *data_f2;
    int w, h, pitch;

protected:
    virtual void SetUp() 
    {
    }

    void FillRandData(int w, int h)
    {
        this->w = w;
        this->h = h;
        ASSERT(w%4==0);

        this->pitch = ((w+3)&~3)*sizeof(float);
        ASSERT(this->pitch*this->h <= MAX_DATA_BUFF_SIZE);

        data_f = data_base_f;
        if ((int)(data_f)&15)
        {
            data_f = (float*)(((int)data_f + 15) & ~15);
        }
        for (int i=0;i<h;i++)
        {
            float *data2 = (float*)((PUINT8)data_f + i*pitch);
            for (int j=0;j<w;j++)
            {
                data2[j] = rand()*0.1f/0xFF;
            }
        }
        data_f2 = data_f + h*pitch;

        data_b = data_byte_base;
        if ((int)(data_b)&15)
        {
            data_b = (UINT8*)(((int)data_b + 15) & ~15);
        }
    }
};


#define Float2FloatTest(width, height, loop_num, function) \
TEST_F(XyTransposeTest, function ## _ ## width ## _ ## height ## _ ## loop_num ) \
{\
    FillRandData(width, height);\
    for(int i=0;i<loop_num;i++)\
    {\
        function(data_f2, height, (width+15)&~15, data_f, width, height, pitch);\
    }\
    ASSERT_EQ(data_f2[0],data_f2[0]);\
}

Float2FloatTest( 272, 80, 10000, xy_float_2_float_transpose_c_v0)
    Float2FloatTest( 272, 80, 10000, xy_float_2_float_transpose_sse_v0)


#define Float2ByteTest(width, height, loop_num, function) \
TEST_F(XyTransposeTest, function ## _ ## width ## _ ## height ## _ ## loop_num ) \
{\
    FillRandData(width, height);\
    for(int i=0;i<loop_num;i++)\
    {\
        function(data_b, height, (width+15)&~15, data_f, width, height, pitch);\
    }\
    ASSERT_EQ(data_b[0],data_b[0]);\
}

Float2ByteTest( 272, 80, 10000, float_2_byte_transpose_v0)
    Float2ByteTest( 272, 80, 10000, float_2_byte_transpose_sse_v0)


#define Byte2FloatTest(dst_width, width, height, loop_num, function) \
TEST_F(XyTransposeTest, function ## _ ## width ## _ ## height ## _ ## loop_num ) \
{\
    FillRandData(width, height);\
    for(int i=0;i<loop_num;i++)\
    {\
        function(data_f, width, (width+15)&~15, data_b, width, height, pitch);\
    }\
    ASSERT_EQ(data_b[0],data_b[0]);\
}


Byte2FloatTest( 272, 256, 64, 10000, byte_2_float_c_v0)
Byte2FloatTest( 272, 256, 64, 10000, byte_2_float_sse_v0)
Byte2FloatTest( 272, 256, 64, 10000, byte_2_float_sse_v1)

#endif // __XY_FILTER_BENCHMARK_F0ED9F75_C0A7_4B0E_910E_7A82ECE2D7AD_H__
