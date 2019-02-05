#include "stdafx.h"
#include "../dsutil/vd.h"

typedef const UINT8 CUINT8, *PCUINT8;
typedef const UINT CUINT, *PCUINT;

//
// ref: "Comparing floating point numbers" by Bruce Dawson
// http://www.cygnus-software.com/papers/comparingfloats/comparingfloats.htm
//
bool AlmostEqual(float A, float B, int maxUlps=0)
{
    // Make sure maxUlps is non-negative and small enough that the
    // default NAN won't compare as equal to anything.
    ASSERT(maxUlps >= 0 && maxUlps < 4 * 1024 * 1024);
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
 * See @xy_filter_c
 **/
__forceinline void xy_filter_one_line_c(float *dst, int width, const float *filter, int filter_width)
{
    const float *filter_start = filter;
    int xx_fix = width > filter_width ? 0 : filter_width - width;
    float *dst2 = dst - filter_width;
    float *dst_endr = dst + width;
    float *dst_end0 = dst_endr - filter_width;
    float *dst_endl = dst - xx_fix;
    ASSERT(xx_fix==0 || dst_end0==dst_endl);

    ASSERT(filter_start == filter);
    filter_start += filter_width;
    const float *filter_end = filter_start;
    for (;dst2<dst_endl;dst2++, filter_start--)//left margin
    {
        const float *src = dst;
        float sum = 0;
        for(const float* f=filter_start;f<filter_end;f++, src++)
        {
            sum += src[0] * f[0];
        }
        *dst2 = sum;
    }
    for (;dst2<dst;dst2++, filter_start--, filter_end--)//if width < filter_width
    {
        const float *src = dst;
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

/****
 * dst memory layout:
 * 1. Source content starts from @dst;
 * 2. Output content starts from @dst-@mwitdth;
 * 
 * |-                     stride                            -|
 * | <- @dst-@mwidth --------| <- @dst+0 --------------------|
 * |-                       -|-                             -|
 * |- margin                -|-  src width*height items     -|
 * |- mwidth*heigth items   -|-                             -|
 * |- do NOT need to init   -|-                             -|
 * |- when input            -|-                             -|
 * |---------------------------------------------------------|
 **/
void xy_filter_c(float *dst, int width, int height, int stride, const float *filter, int filter_width)
{
    ASSERT( stride>=4*(width+filter_width) );
    BYTE* end = reinterpret_cast<BYTE*>(dst) + height*stride;
    BYTE* dst_byte = reinterpret_cast<BYTE*>(dst);
    for( ; dst_byte<end; dst_byte+=stride )
    {
        xy_filter_one_line_c(reinterpret_cast<float*>(dst_byte), width, filter, filter_width);
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

/****
 * @src4, @f4_1, @sum : __m128
 * @f4_1: const
 * @sum : output
 * @src4: undefined
 **/ 
#define XY_FILTER_4_1(src4, f4_1, sum) \
    src4 = _mm_mul_ps(src4, f4_1);\
    sum = _mm_add_ps(sum, src4);

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

        //filter 8
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

/****
 * See @xy_filter_c
 **/
void xy_filter_sse_v6(float *dst, int width, int height, int stride, const float *filter, int filter_width)
{
    ASSERT( stride>=4*(width+filter_width) );
    ASSERT( ((stride|(4*width)|(4*filter_width)|reinterpret_cast<intptr_t>(dst)|reinterpret_cast<intptr_t>(filter))&15)==0 );

    BYTE* dst_byte = reinterpret_cast<BYTE*>(dst);
    BYTE* end = dst_byte + height*stride;
    for( ; dst_byte<end; dst_byte+=stride )
    {
        xy_filter_one_line_sse_v6(reinterpret_cast<float*>(dst_byte), width, filter, filter_width);
    }
}

/****
 * Constrain:
 *   LENGTH%4 == 0 || LENGTH%4 == 1
 **/
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
struct M128s<1>
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
        __m128 src4_128 = _mm_load_ps(src4);
        { XY_FILTER_4(src0_128, src4_128, filter128s.GetAt<START>(), sum); }
        Filter4<FILTER_LENGTH,START+4,LENGTH-4>::do_cal(src4_128, src4+4, filter128s, sum);
        src0_128 = src4_128;
    }
};

template<int FILTER_LENGTH, int START>
struct Filter4<FILTER_LENGTH, START, 1>
{
    static __forceinline void do_cal(__m128& src0_128, const float * src4, M128s<FILTER_LENGTH>& filter128s, __m128& sum)
    {
        cal_tail<FILTER_LENGTH-START>(src0_128, src4, filter128s, sum);
    }
    template<int TAIL>
    static __forceinline void cal_tail(__m128& src0_128, const float * src4, M128s<FILTER_LENGTH>& filter128s, __m128& sum);
    template<>
    static __forceinline void cal_tail<1>(__m128& src0_128, const float * src4, M128s<FILTER_LENGTH>& filter128s, __m128& sum)
    {
        { XY_FILTER_4_1(src0_128, filter128s.GetAt<FILTER_LENGTH-1>(), sum); }
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
struct FilterAllLeftMargin
{
    static __forceinline void cal(float * src, M128s<FILTER_LENGTH>& filter128s)
    {
        do_cal<FILTER_LENGTH%4>(src, filter128s);
    }
    template<int FILTER_TAIL>
    static __forceinline void do_cal(float * src, M128s<FILTER_LENGTH>& filter128s)
    {
        //filter 4
        __m128 src0 = _mm_setzero_ps();
        __m128 sum = _mm_setzero_ps();
        Filter4<FILTER_LENGTH,MARGIN_LENGTH-4,FILTER_LENGTH-MARGIN_LENGTH+4>::do_cal(src0, src, filter128s, sum);
        _mm_store_ps(src-MARGIN_LENGTH, sum);
        FilterAllLeftMargin<FILTER_LENGTH,MARGIN_LENGTH-4>::do_cal<0>(src, filter128s);
    }
    template<>
    static __forceinline void do_cal<1>(float * src, M128s<FILTER_LENGTH>& filter128s)
    {
        //filter 4
        __m128 sum = _mm_setzero_ps();
        //Only one of the last 4 filter coefficiences is non-zero
        _mm_store_ps(src-MARGIN_LENGTH, sum);
        FilterAllLeftMargin<FILTER_LENGTH,MARGIN_LENGTH-4>::do_cal<0>(src, filter128s);
    }
};

template<int FILTER_LENGTH, int MARGIN_LENGTH>
struct FilterAllRightMargin
{
    static __forceinline void cal(float * src, M128s<FILTER_LENGTH>& filter128s)
    {
        do_cal<FILTER_LENGTH%4>(src, filter128s);
    }
    template<int FILTER_TAIL>
    static __forceinline void do_cal(float * src, M128s<FILTER_LENGTH>& filter128s)
    {
        //filter 4
        {
            __m128 src0 = _mm_load_ps(src);
            __m128 sum = _mm_setzero_ps();
            Filter4<FILTER_LENGTH,0,MARGIN_LENGTH-4>::do_cal(src0, src+4, filter128s, sum);
            __m128 src4 = _mm_setzero_ps();
            { XY_FILTER_4(src0, src4, filter128s.GetAt<MARGIN_LENGTH-4>(), sum); }
            //store result
            _mm_store_ps(src, sum);
        }
        FilterAllRightMargin<FILTER_LENGTH,MARGIN_LENGTH-4>::do_cal<0>(src+4, filter128s);
    }
    template<>
    static __forceinline void do_cal<1>(float * src, M128s<FILTER_LENGTH>& filter128s)
    {
        //filter 4
        {
            __m128 src0 = _mm_load_ps(src);
            __m128 sum = _mm_setzero_ps();
            Filter4<FILTER_LENGTH,0,MARGIN_LENGTH-4>::do_cal(src0, src+4, filter128s, sum);
            //Only one of the last 4 filter coefficiences is non-zero
            { XY_FILTER_4_1(src0, filter128s.GetAt<MARGIN_LENGTH-4>(), sum); }
            //store result
            _mm_store_ps(src, sum);
        }
        FilterAllRightMargin<FILTER_LENGTH,MARGIN_LENGTH-4>::do_cal<0>(src+4, filter128s);
    }
};

template<int FILTER_LENGTH>
struct FilterAllLeftMargin<FILTER_LENGTH,0>
{
    template<int FILTER_TAIL>
    static __forceinline void do_cal(float * src, M128s<FILTER_LENGTH>& filter128s)
    {
    }
};

template<int FILTER_LENGTH>
struct FilterAllRightMargin<FILTER_LENGTH,0>
{
    template<int FILTER_TAIL>
    static __forceinline void do_cal(float * src, M128s<FILTER_LENGTH>& filter128s)
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
void xy_filter_sse_template(float *dst, int width, int height, int stride, const float *filter)
{
    ASSERT( stride>=4*(width+FILTER_LENGTH) );
    ASSERT( ((stride|(4*width)|reinterpret_cast<intptr_t>(dst)|reinterpret_cast<intptr_t>(filter))&15)==0 );

    M128s<FILTER_LENGTH> filter128s;
    filter128s.Load(filter);

    const float *filter_start = filter;
    BYTE* dst_byte = reinterpret_cast<BYTE*>(dst);
    BYTE* end = dst_byte + height*stride;
    for( ; dst_byte<end; dst_byte+=stride )
    {
        float *dst2 = reinterpret_cast<float*>(dst_byte);

        //left margin
        FilterAllLeftMargin<FILTER_LENGTH,((FILTER_LENGTH+3)&~3)>::cal(dst2, filter128s);
        float *dst_end1 = dst2 + width;
        float *dst_end0 = dst_end1 - ((FILTER_LENGTH+3)&~3);
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
        FilterAllRightMargin<FILTER_LENGTH,((FILTER_LENGTH+3)&~3)>::cal(dst2, filter128s);
    }
}


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
    ASSERT( ((stride|(4*width)|(4*filter_width)|reinterpret_cast<intptr_t>(dst)|reinterpret_cast<intptr_t>(filter))&15)==0 );

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
            _mm_store_ps(dst2-4, sum);
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


/****
 * See @xy_filter_c
 **/
void xy_filter_sse(float *dst, int width, int height, int stride, const float *filter, int filter_width)
{
    typedef void (*Filter)(float *dst, int width, int height, int stride, const float *filter);
    const Filter filters[] = { 
        NULL,
        xy_filter_sse_template<1>, xy_filter_sse_template<4>, xy_filter_sse_template<4>, xy_filter_sse_template<4>, 
        xy_filter_sse_template<5>, xy_filter_sse_template<8>, xy_filter_sse_template<8>, xy_filter_sse_template<8>, 
        xy_filter_sse_template<9>, xy_filter_sse_template<12>, xy_filter_sse_template<12>, xy_filter_sse_template<12>, 
        xy_filter_sse_template<13>, xy_filter_sse_template<16>, xy_filter_sse_template<16>, xy_filter_sse_template<16>,
        xy_filter_sse_template<17>, xy_filter_sse_template<20>, xy_filter_sse_template<20>, xy_filter_sse_template<20>, 
        xy_filter_sse_template<21>, xy_filter_sse_template<24>, xy_filter_sse_template<24>, xy_filter_sse_template<24>, 
        xy_filter_sse_template<25>, xy_filter_sse_template<28>, xy_filter_sse_template<28>, xy_filter_sse_template<28>
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

/****
 * Copy and convert src to dst line by line.
 * @dst_width MUST >= @width
 * if @dst_width>@width, the extra elements will be filled with 0.
 **/
void xy_byte_2_float_c(float *dst, int dst_width, int dst_stride, 
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

/****
 * See @xy_byte_2_float_c
 **/
void xy_byte_2_float_sse(float *dst, int dst_width, int dst_stride, 
    PCUINT8 src, int width, int height, int stride)
{
    ASSERT( dst_width>=width );
    ASSERT( ((reinterpret_cast<intptr_t>(dst)|dst_stride)&15)==0 );
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

/****
 * Copy transposed Matrix src to dst.
 * @dst_width MUST >= @height.
 * if @dst_width > @height, the extra elements will be filled with 0.
 **/
void xy_float_2_float_transpose_c(float *dst, int dst_width, int dst_stride, 
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

/****
 * see @xy_float_2_float_transpose_c
 **/
void xy_float_2_float_transpose_sse(float *dst, int dst_width, int dst_stride, 
    const float *src, int width, int height, int src_stride)
{
    typedef float DstT;
    typedef const float SrcT;

    ASSERT( (((intptr_t)dst|dst_stride)&15)==0 );
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

/****
 * Transpose and round Matrix src, then copy to dst.
 * @dst_width MUST >= @height.
 * if @dst_width > @height, the extra elements will be filled with 0.
 **/
void xy_float_2_byte_transpose_c(UINT8 *dst, int dst_width, int dst_stride, 
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

void xy_float_2_byte_transpose_sse(UINT8 *dst, int dst_width, int dst_stride, 
    const float *src, int width, int height, int src_stride)
{
    typedef UINT8 DstT;
    typedef const float SrcT;

    ASSERT(dst_width >= height);
    ASSERT((((intptr_t)dst|dst_stride)&15)==0);
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

/****
 * To Do: decent CPU capability check
 **/
void xy_gaussian_blur(PUINT8 dst, int dst_stride,
    PCUINT8 src, int width, int height, int stride, 
    const float *gt_x, int r_x, int gt_ex_width_x, 
    const float *gt_y, int r_y, int gt_ex_width_y)
{
    ASSERT(width<=stride && width+2*r_x<=dst_stride);
    int ex_mask_width_x = ((r_x*2+1)+3)&~3;
    ASSERT(ex_mask_width_x<=gt_ex_width_x);
    if (ex_mask_width_x>gt_ex_width_x)
    {
        int o=0;
        o=o/o;
        exit(-1);
    }

    int ex_mask_width_y = ((r_y*2+1)+3)&~3;
    ASSERT(ex_mask_width_y<=gt_ex_width_y);
    if (ex_mask_width_y>gt_ex_width_y)
    {
        int o=0;
        o=o/o;
        exit(-1);
    }


    int fwidth = (width+3)&~3;
    int fstride = (fwidth + ex_mask_width_x)*sizeof(float);
    int fheight = (height+3)&~3;
    int fstride_ver = (fheight+ex_mask_width_y)*sizeof(float);

    PUINT8 buff_base = reinterpret_cast<PUINT8>(xy_malloc(height*fstride + (fwidth + ex_mask_width_x)*fstride_ver));
    
    float *hor_buff_base = reinterpret_cast<float*>(buff_base);
    float *hor_buff = hor_buff_base + ex_mask_width_x;    

    // byte to float
    ASSERT( ((width+15)&~15)<=stride );
    xy_byte_2_float_sse(hor_buff, fwidth, fstride, src, width, height, stride);

    // horizontal pass
    xy_filter_sse(hor_buff, fwidth, height, fstride, gt_x, ex_mask_width_x);


    // transpose
    float *ver_buff_base = reinterpret_cast<float*>(buff_base + height*fstride);
    float *ver_buff = ver_buff_base + ex_mask_width_y;

    int true_width = width+r_x*2;
    xy_float_2_float_transpose_sse(ver_buff, fheight, fstride_ver, hor_buff-r_x*2, true_width, height, fstride);

    // vertical pass
    xy_filter_sse(ver_buff, fheight, true_width, fstride_ver, gt_y, ex_mask_width_y);
    
    // transpose
    int true_height = height + 2*r_y;
    xy_float_2_byte_transpose_sse(dst, true_width, dst_stride, ver_buff-r_y*2, true_height, true_width, fstride_ver);
    
    xy_free(buff_base);
#ifndef _WIN64
    // TODOX64 : fixme!
    _mm_empty();
#endif
}


enum RoundingPolicy
{
    ROUND_DOWN
    , ROUND_HALF_DOWN
    , ROUND_HALF_UP
    , ROUND_HALF_TO_EVEN
    , COUNT_ROUND_POLICY
};

template<int ROUNDING_POLICY, int precision>
struct XyRounding
{
    __forceinline void init_sse();
    __forceinline __m128i round(__m128i in);
    __forceinline int round(int in);
};

template<int precision>
struct XyRounding<ROUND_DOWN, precision>
{
    __forceinline void init_sse()
    {

    }
    __forceinline __m128i round(__m128i in)
    {
        return in;
    }

    __forceinline int round(unsigned in)
    {
        return in;
    }
};


template<int precision>
struct XyRounding<ROUND_HALF_DOWN, precision>
{
    __forceinline void init_sse()
    {
        m_rounding_patch = _mm_set1_epi16( (1<<(precision-1))-1 );
    }
    __forceinline __m128i round(__m128i in)
    {
        return _mm_adds_epu16(in, m_rounding_patch);
    }

    __forceinline int round(unsigned in)
    {
        return in + ((1<<(precision-1))-1);
    }
    __m128i m_rounding_patch;
};


template<int precision>
struct XyRounding<ROUND_HALF_UP, precision>
{
    __forceinline void init_sse()
    {
        m_rounding_patch = _mm_set1_epi16( 1<<(precision-1) );
    }
    __forceinline __m128i round(__m128i in)
    {
        return _mm_adds_epu16(in, m_rounding_patch);
    }

    __forceinline int round(unsigned in)
    {
        return in + (1<<(precision-1));
    }
    __m128i m_rounding_patch;
};


template<int precision>
struct XyRounding<ROUND_HALF_TO_EVEN, precision>
{
    __forceinline void init_sse()
    {
        m_rounding_patch = _mm_set1_epi16( 1<<(precision-1) );
    }
    __forceinline __m128i round(__m128i in)
    {
        in = _mm_adds_epu16(in, m_rounding_patch);
        __m128i tmp = _mm_slli_epi16(in, 15-precision);
        tmp = _mm_srli_epi16(tmp, 15);
        return _mm_adds_epu16(in, tmp);
    }

    __forceinline int round(unsigned in)
    {
        return in + (1<<(precision-1)) + ((in>>precision)&1);
    }
    __m128i m_rounding_patch;
};

/****
 * filter with [1,2,1]
 * 1. It is a in-place horizontal filter
 * 2. Boundary Pixels are filtered by padding 0. E.g. 
 *      dst[0] = (0*1 + dst[0]*2 + dst[1]*1)/4;
 **/
template<int ROUNDING_POLICY>
void xy_be_filter_c(PUINT8 dst, int width, int height, int stride)
{
    ASSERT(width>=1);
    if (width<=0)
    {
        return;
    }
    PUINT8 dst2 = NULL;
    XyRounding<ROUNDING_POLICY, 2> xy_rounding;
    for (int y=0;y<height;y++)
    {
        dst2 = dst + y*stride;
        int old_sum = dst2[0];
        int tmp = 0;
        int x=0;
        for (x=0;x<width-1;x++)
        {
            int new_sum = dst2[x]+dst2[x+1];
            tmp = old_sum + new_sum;//old_sum == src2[x-1]+src2[x];
            dst2[x] = (xy_rounding.round(tmp)>>2);
            old_sum = new_sum;
        }
        tmp = old_sum + dst2[x];
        dst2[x] = (xy_rounding.round(tmp)>>2);
    }
}

/****
 * 1. It is a in-place symmetric 3-tag horizontal filter
 * 2. Boundary Pixels are filtered by padding 0. E.g. 
 *      dst[0] = (0*1 + dst[0]*2 + dst[1]*1)/4;
 * 3. sum(filter) == 256
 **/
template<int ROUNDING_POLICY>
void xy_be_filter2_c(PUINT8 dst, int width, int height, int stride, PCUINT filter)
{
    ASSERT(width>=1);
    if (width<=0)
    {
        return;
    }

    const int VOLUME_BITS = 8;
    const int VOLUME = (1<<VOLUME_BITS);
    if (filter[0]==0)
    {
        return;//nothing to do;
    }
    else if (filter[0]== (VOLUME>>2))
    {
        return xy_be_filter_c<ROUNDING_POLICY>(dst, width, height, stride);
    }

    PUINT8 dst2 = NULL;
    XyRounding<ROUNDING_POLICY, VOLUME_BITS> xy_rounding;
    for (int y=0;y<height;y++)
    {
        dst2 = dst + y*stride;
        int old_pix = 0;
        int tmp = 0;
        int x=0;
        for (x=0;x<width-1;x++)
        {
            tmp = (old_pix + dst2[x+1]) * filter[0] + dst2[x] * filter[1];
            old_pix = dst2[x];
            dst2[x] = (xy_rounding.round(tmp)>>VOLUME_BITS);
        }
        tmp = old_pix * filter[0] + dst2[x] * filter[1];
        dst2[x] = (xy_rounding.round(tmp)>>VOLUME_BITS);
    }
}

/****
 * See @xy_be_filter_c
 * No alignment requirement.
 **/
template<int ROUNDING_POLICY>
void xy_be_filter_sse(PUINT8 dst, int width, int height, int stride)
{
    ASSERT(width>=1);
    if (width<=0)
    {
        return;
    }
    int width_mod8 = ((width-1)&~7);
    XyRounding<ROUNDING_POLICY, 2> xy_rounding;
    xy_rounding.init_sse();
    for (int y = 0; y < height; y++) {
        PUINT8 dst2=dst+y*stride;

        __m128i old_pix_128 = _mm_cvtsi32_si128(dst2[0]);
        __m128i old_sum_128 = old_pix_128;

        int x = 0;
        for (; x < width_mod8; x+=8) {
            __m128i new_pix = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(dst2+x+1));
            new_pix = _mm_unpacklo_epi8(new_pix, _mm_setzero_si128());
            __m128i temp = _mm_slli_si128(new_pix,2);
            temp = _mm_add_epi16(temp, old_pix_128);
            temp = _mm_add_epi16(temp, new_pix);
            old_pix_128 = _mm_srli_si128(new_pix,14);

            new_pix = _mm_slli_si128(temp,2);
            new_pix = _mm_add_epi16(new_pix, old_sum_128);
            new_pix = _mm_add_epi16(new_pix, temp);
            old_sum_128 = _mm_srli_si128(temp, 14);

            new_pix = xy_rounding.round(new_pix);

            new_pix = _mm_srli_epi16(new_pix, 2);
            new_pix = _mm_packus_epi16(new_pix, new_pix);

            _mm_storel_epi64( reinterpret_cast<__m128i*>(dst2+x), new_pix );
        }
        int old_sum = _mm_cvtsi128_si32(old_sum_128);
        old_sum &= 0xffff;
        int tmp = 0;
        for ( ; x < width-1; x++) {
            int new_sum = dst2[x] + dst2[x+1];
            tmp = old_sum + new_sum;
            dst2[x] = (xy_rounding.round(tmp)>>2);
            old_sum = new_sum;
        }
        tmp = old_sum + dst2[x];
        dst2[x] = (xy_rounding.round(tmp)>>2);
    }
}

/****
 * See @xy_be_filter2_c
 * No alignment requirement.
 **/
template<int ROUNDING_POLICY>
void xy_be_filter2_sse(PUINT8 dst, int width, int height, int stride, PCUINT filter)
{
    const int VOLUME_BITS = 8;
    const int VOLUME = (1<<VOLUME_BITS);
    ASSERT(filter[0]==filter[2]);
    ASSERT(filter[0]+filter[1]+filter[2]==VOLUME);
    ASSERT(width>=1);
    if (width<=0)
    {
        return;
    }
    
    XyRounding<ROUNDING_POLICY, VOLUME_BITS> xy_rounding;
    xy_rounding.init_sse();
    __m128i f3_1 = _mm_set1_epi16(filter[0]);
    __m128i f3_2 = _mm_set1_epi16(filter[1]);

    int width_mod8 = ((width-1)&~7);
    //__m128i round = _mm_set1_epi16(8);
    
    PUINT8 dst_byte = reinterpret_cast<PUINT8>(dst);
    PUINT8 end = dst_byte + height*stride;
    for( ; dst_byte<end; dst_byte+=stride )
    {
        PUINT8 dst2 = dst_byte;

        PUINT8 dst_end0 = dst_byte + width - 4;

        __m128i old_pix1_128 = _mm_setzero_si128();
        __m128i old_pix2_128 = _mm_cvtsi32_si128(dst2[0]);

        int x = 0;
        for (; x < width_mod8; x+=8) {
            __m128i pix2 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(dst2+x+1));
            pix2 = _mm_unpacklo_epi8(pix2, _mm_setzero_si128());
            __m128i pix1 =  _mm_slli_si128(pix2,2);
            pix1 = _mm_add_epi8(pix1, old_pix2_128);
            __m128i pix0 =  _mm_slli_si128(pix1,2);
            pix0 = _mm_add_epi8(pix0, old_pix1_128);
            old_pix1_128 = _mm_srli_si128(pix1,14);
            old_pix2_128 = _mm_srli_si128(pix2,14);

            pix0 = _mm_add_epi16(pix0, pix2);
            pix0 = _mm_mullo_epi16(pix0, f3_1);
            pix1 = _mm_mullo_epi16(pix1, f3_2);
            
            pix1 = _mm_adds_epu16(pix1, pix0);

            pix1 = xy_rounding.round(pix1);

            pix1 = _mm_srli_epi16(pix1, VOLUME_BITS);
            pix1 = _mm_packus_epi16(pix1, pix1);

            _mm_storel_epi64( reinterpret_cast<__m128i*>(dst2+x), pix1 );
        }
        int old_pix1 = _mm_cvtsi128_si32(old_pix1_128);
        old_pix1 &= 0xff;
        int tmp = 0;
        for ( ; x < width-1; x++) {
            tmp = (old_pix1 + dst2[x+1]) * filter[0] + dst2[x] * filter[1];
            old_pix1 = dst2[x];

            dst2[x] = (xy_rounding.round(tmp)>>VOLUME_BITS);
        }
        tmp = old_pix1*filter[0] + dst2[x]*filter[1];
        dst2[x] = (xy_rounding.round(tmp)>>VOLUME_BITS);
    }
}

/****
 * See @xy_be_blur
 * Construct the filter used in the final horizontal/vertical pass of @xy_be_blur when @pass is NOT a integer.
 * This filter is constructed to satisfy:
 *   If @p is a pixel in the middle of the image, pixels @(p-1) and @(p+1) lie just at the left and the right
 *   of @p respectively. The value of @p after all horizontal filtering equals to
 *      a*value_old(@(p-1)) + b*value_old(@p) + a*value_old(@(p+1)) + other pixels' weighted sum,
 *   then
 *      a/b = @pass/(@pass+1).
 * It makes sense because the above property also holds when @pass is a integer.
 *
 * @return
 *   Let n = floor(pass);
 *   filter = [ (pass-n)(n+2) / (2*(1+3pass-n)), 1-(pass-n)(n+2)/(1+3pass-n), (pass-n)(n+2)/ (2*(1+3pass-n)) ]
 **/
void xy_calculate_filter(float pass, PUINT filter)
{
    const int VOLUME = (1<<8);
    int n = (int)pass;
    if (n==0)
    {
        filter[0] = VOLUME * pass/(1+3*pass);
    }
    else if (n==1)
    {
        filter[0] = VOLUME * (pass-1)/(2*pass);
    }
    else
    {
        filter[0] = VOLUME * (pass-n)*(n+2)/ (2*(1+3*pass-n));
    }    
    filter[1] = VOLUME - 2*filter[0];
    filter[2] = filter[0];

    if (2*filter[0]>filter[1])
    {
        //this should not happen
        ASSERT(0);
        filter[0] = VOLUME/4;
        filter[1] = VOLUME - 2*filter[0];
        filter[2] = filter[0];
    }
}

/****
 * See @xy_float_2_byte_transpose_c
 **/
void xy_byte_2_byte_transpose_c(UINT8 *dst, int dst_width, int dst_stride, 
    PCUINT8 src, int width, int height, int src_stride)
{
    ASSERT(dst_width >= height);
    PUINT8 dst_byte = reinterpret_cast<PUINT8>(dst);
    PCUINT8 src_end = src + width;
    PCUINT8 src2_end = reinterpret_cast<PCUINT8>(src) + height*src_stride;
    for( ; src<src_end; src++, dst_byte+=dst_stride )
    {
        PCUINT8 src2 = reinterpret_cast<PCUINT8>(src);

        UINT8 *dst2 = reinterpret_cast<UINT8*>(dst_byte);
        for (;src2<src2_end;src2+=src_stride,dst2++)
        {
            *dst2 = *src2;
        }
        UINT8 *dst2_end = reinterpret_cast<UINT8*>(dst_byte) + dst_width;
        for (;dst2<dst2_end;dst2++)
        {
            *dst2 = 0;
        }
    }
}

/****
 * Repeat filter [1,2,1] @pass_x times in horizontal and @pass_y times in vertical
 * Boundary Pixels are filtered by padding 0, see @xy_be_filter_c.
 *
 * @pass_x:
 *   When @pass_x is not a integer, horizontal filter [1,2,1] is repeated (int)@pass_x times,
 *   and a 3-tag symmetric filter which generated according to @pass_x is applied.
 *   The final 3-tag symmetric filter is constructed to satisfy the following property:
 *     If @pass_xx > @pass_x, the filtered result of @pass_x should NOT be more blur than
 *     the result of @pass_xx. More specially, the filtered result of @pass_x should NOT be more 
 *     blur than (int)@pass_x+1 and should NOT be less blur than (int)@pass_x;
 *     
 * Rounding:
 *   Original VSFilter \be effect uses a simple a round down, which has an bias of -7.5/16 per pass.
 *   That rounding error is really huge with big @pass value. It has become one part of the \be effect.
 *   We can simulate VSFilter's rounding bias by combining different rounding methods. However a simple
 *   test shows that result still has some visual difference from VSFilter's.
 *   
 * Know issue:
 *   It seems that this separated filter implementation is more sensitive to precision in comparison to 
 *   VSFilter's simple implementation. Vertical blocky artifact can be observe with big pass value 
 *   (and @pass_x==@pass_y). So it is only used for filtering of fractional part of \be strength.
 **/
void xy_be_blur(PUINT8 src, int width, int height, int stride, float pass_x, float pass_y)
{
    //ASSERT(pass_x>0 && pass_y>0);

    typedef void (*XyBeFilter)(PUINT8 src, int width, int height, int stride);
    typedef void (*XyFilter2)(PUINT8 src, int width, int height, int stride, PCUINT filter);

    XyBeFilter filter = xy_be_filter_sse<ROUND_HALF_TO_EVEN>;
    XyFilter2 filter2 = xy_be_filter2_sse<ROUND_HALF_TO_EVEN>;

    int stride_ver = height;
    PUINT8 tmp = reinterpret_cast<PUINT8>(xy_malloc(width*height));
    ASSERT(tmp);
    // horizontal pass
    int pass_x_int = static_cast<int>(pass_x);
    for (int i=0; i<pass_x_int; i++)
    {
        filter(src, width, height, stride);
    }
    if (pass_x-pass_x_int>0)
    {
        UINT f[3] = {0};
        xy_calculate_filter(pass_x, f);
        filter2(src, width, height, stride, f);
    }

    // transpose
    xy_byte_2_byte_transpose_c(tmp, height, stride_ver, src, width, height, stride);

    // vertical pass
    int pass_y_int = static_cast<int>(pass_y);
    for (int i=0;i<pass_y_int;i++)
    {
        filter(tmp, height, width, stride_ver);
    }
    if (pass_y-pass_y_int>0)
    {
        UINT f[3] = {0};
        xy_calculate_filter(pass_y, f);
        filter2(tmp, height, width, stride_ver, f);
    }

    // transpose
    xy_byte_2_byte_transpose_c(src, width, stride, tmp, height, width, stride_ver);

    xy_free(tmp);
    return;
}
