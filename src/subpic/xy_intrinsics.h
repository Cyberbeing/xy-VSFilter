#ifndef __XY_INTRINSICS_D66EF42F_67BC_47F4_A70D_40F1AB80F376_H__
#define __XY_INTRINSICS_D66EF42F_67BC_47F4_A70D_40F1AB80F376_H__

#ifdef LINUX
#include <pmmintrin.h>
#else
#include<intrin.h>
#endif

#include <WTypes.h>

//out: m128_1 = avg(m128_1.u8[0],m128_1.u8[1],m128_2.u8[0],m128_2.u8[1]) 
//              0 
//              avg(...) 
//              0 
//              ...
#define AVERAGE_4_PIX_INTRINSICS(m128_1, m128_2) \
    m128_1 = _mm_avg_epu8(m128_1, m128_2); \
    m128_2 = _mm_slli_epi16(m128_1, 8); \
    m128_1 = _mm_srli_epi16(m128_1, 8); \
    m128_2 = _mm_srli_epi16(m128_2, 8); \
    m128_1 = _mm_avg_epu8(m128_1, m128_2);

//out: m128_1 = avg(m128_1.u8[0],m128_1.u8[1],m128_2.u8[0],m128_2.u8[1]) 
//              avg(m128_1.u8[0],m128_1.u8[1],m128_2.u8[0],m128_2.u8[1]) 
//              avg(...) 
//              avg(...)
//              ...
#define AVERAGE_4_PIX_INTRINSICS_2(m128_1, m128_2) \
    {\
    m128_1 = _mm_avg_epu8(m128_1, m128_2); \
    m128_2 = _mm_slli_epi16(m128_1, 8); \
    __m128i m128_3 = _mm_srli_epi16(m128_1, 8); \
    m128_2 = _mm_or_si128(m128_2, m128_3);\
    m128_1 = _mm_avg_epu8(m128_1, m128_2);\
    }

//in : m128_1 = whatever, m128_last = u8 U_last 0 0 0 ... 0
//out: m128_1 = avg(U_last, u8[0], u8[1]) 
//              0 
//              avg(u8[1], u8[2], u8[3]) 
//              0
//              ...
//     m128_last = m128_1.u8[14] m128_1.u8[15] 0 0 0 ... 0
#define AVERAGE_4_PIX_INTRINSICS_3(m128_1, m128_last) \
    {\
    __m128i m128_2 = _mm_slli_si128(m128_1,2);\
    m128_2 = _mm_or_si128(m128_2, m128_last);\
    m128_2 = _mm_avg_epu8(m128_2, m128_1);\
    m128_last = _mm_srli_si128(m128_1,14);\
    m128_1 = _mm_slli_epi16(m128_1, 8);\
    m128_1 = _mm_avg_epu8(m128_1, m128_2);\
    m128_1 = _mm_srli_epi16(m128_1, 8);\
    }

static void average_4_pix_intrinsics_3_c(__m128i& m128i_1, __m128i& m128i_last) 
{
    int last=m128i_last.m128i_u8[1];
    m128i_last.m128i_u8[0] = m128i_1.m128i_u8[14];
    m128i_last.m128i_u8[1] = m128i_1.m128i_u8[15];
    for (int i=2;i<16;i++)
    {
        m128i_last.m128i_u8[i] = 0;
    }
    for (int i=0;i<8;i++)
    {
        int u0 = m128i_1.m128i_u8[2*i];
        int u1 = m128i_1.m128i_u8[2*i+1];
        last = (last + u1 + 1)/2;
        u0 = (last + u0 + 1)/2;
        last = u1;
        m128i_1.m128i_u8[2*i] = u0;
        m128i_1.m128i_u8[2*i+1] = 0;
    }
}

//in : m128_1 = whatever, m128_last = u8 U_last 0 0 0 ... 0
//out: m128_1 = 0
//              avg(U_last, u8[0], u8[1]) 
//              0
//              avg(u8[1], u8[2], u8[3]) 
//              ...
//     m128_last = m128_1.u8[14] m128_1.u8[15] 0 0 0 ... 0
#define AVERAGE_4_PIX_INTRINSICS_4(m128_1, m128_last) \
    {\
    __m128i m128_2 = _mm_slli_si128(m128_1,2);\
    m128_2 = _mm_or_si128(m128_2, m128_last);\
    m128_2 = _mm_avg_epu8(m128_2, m128_1);\
    m128_last = _mm_srli_si128(m128_1,14);\
    m128_2 = _mm_srli_epi16(m128_2, 8);\
    m128_1 = _mm_avg_epu8(m128_1, m128_2);\
    m128_1 = _mm_slli_epi16(m128_1, 8);\
    }

static void average_4_pix_intrinsics_4_c(__m128i& m128i_1, __m128i& m128i_last) 
{
    int last=m128i_last.m128i_u8[1];
    m128i_last.m128i_u8[0] = m128i_1.m128i_u8[14];
    m128i_last.m128i_u8[1] = m128i_1.m128i_u8[15];
    for (int i=2;i<16;i++)
    {
        m128i_last.m128i_u8[i] = 0;
    }
    for (int i=0;i<8;i++)
    {
        int u0 = m128i_1.m128i_u8[2*i];
        int u1 = m128i_1.m128i_u8[2*i+1];
        last = (last + u1 + 1)/2;
        u0 = (last + u0 + 1)/2;
        last = u1;
        m128i_1.m128i_u8[2*i+1] = u0;
        m128i_1.m128i_u8[2*i] = 0;
    }
}
//in : m128_1 = whatever, m128_last = u8 U_last 0 0 0 ... 0
//out: m128_1 = avg(U_last, u8[0], u8[1]) 
//              avg(U_last, u8[0], u8[1]) 
//              avg(u8[1], u8[2], u8[3]) 
//              avg(u8[1], u8[2], u8[3])
//              ...
//     m128_last = m128_1.u8[14] m128_1.u8[15] 0 0 0 ... 0
#define AVERAGE_4_PIX_INTRINSICS_5(m128_1, m128_last) \
    {\
    __m128i m128_2 = _mm_slli_si128(m128_1,2);\
    m128_2 = _mm_or_si128(m128_2, m128_last);\
    m128_2 = _mm_avg_epu8(m128_2, m128_1);\
    m128_last = _mm_srli_si128(m128_1,14);\
    m128_2 = _mm_srli_epi16(m128_2, 8);\
    m128_1 = _mm_avg_epu8(m128_1, m128_2);\
    m128_1 = _mm_slli_epi16(m128_1, 8);\
    m128_2 = _mm_srli_epi16(m128_1, 8);\
    m128_1 = _mm_or_si128(m128_1, m128_2);\
    }

static void average_4_pix_intrinsics_5_c(__m128i& m128i_1, __m128i& m128i_last) 
{
    int last=m128i_last.m128i_u8[1];
    m128i_last.m128i_u8[0] = m128i_1.m128i_u8[14];
    m128i_last.m128i_u8[1] = m128i_1.m128i_u8[15];
    for (int i=2;i<16;i++)
    {
        m128i_last.m128i_u8[i] = 0;
    }
    for (int i=0;i<8;i++)
    {
        int u0 = m128i_1.m128i_u8[2*i];
        int u1 = m128i_1.m128i_u8[2*i+1];
        last = (last + u1 + 1)/2;
        u0 = (last + u0 + 1)/2;
        last = u1;
        m128i_1.m128i_u8[2*i+1] = u0;
        m128i_1.m128i_u8[2*i] = u0;
    }
}

static void subsample_and_interlace_2_line_c(BYTE* dst, const BYTE* u, const BYTE* v, int w, int pitch)
{
    const BYTE* end = u + w;
    for (;u<end;dst+=2,u+=2,v+=2)
    {
        dst[0] = (u[0] + u[0+pitch] + 1)/2;
        int tmp1 = (u[1] + u[1+pitch] + 1)/2;
        dst[0] = (dst[0] + tmp1 + 1)/2;                
        dst[1] = (v[0] + v[0+pitch] + 1)/2;
        tmp1 = (v[1] + v[1+pitch] + 1)/2;
        dst[1] = (dst[1] + tmp1 + 1)/2;
    }
}

static __forceinline void subsample_and_interlace_2_line_sse2(BYTE* dst, const BYTE* u, const BYTE* v, int w, int pitch)
{
    const BYTE* end = u + w;
    for (;u<end;dst+=16,u+=16,v+=16)
    {
        __m128i u_1 = _mm_load_si128( reinterpret_cast<const __m128i*>(u) );    
        __m128i u_2 = _mm_load_si128( reinterpret_cast<const __m128i*>(u+pitch) );
        __m128i v_1 = _mm_load_si128( reinterpret_cast<const __m128i*>(v) );    
        __m128i v_2 = _mm_load_si128( reinterpret_cast<const __m128i*>(v+pitch) );
        AVERAGE_4_PIX_INTRINSICS(u_1, u_2);
        AVERAGE_4_PIX_INTRINSICS(v_1, v_2);
        u_1 = _mm_packus_epi16(u_1, u_1);
        v_1 = _mm_packus_epi16(v_1, v_1);
        u_1 = _mm_unpacklo_epi8(u_1, v_1);

        _mm_store_si128( reinterpret_cast<__m128i*>(dst), u_1 );
    }
}

static __forceinline void pix_alpha_blend_yv12_luma_sse2(byte* dst, const byte* alpha, const byte* sub)
{
    __m128i dst128 = _mm_load_si128( reinterpret_cast<const __m128i*>(dst) );
    __m128i alpha128 = _mm_load_si128( reinterpret_cast<const __m128i*>(alpha) );
    __m128i sub128 = _mm_load_si128( reinterpret_cast<const __m128i*>(sub) );
    __m128i zero = _mm_setzero_si128();

    __m128i ones;
#ifdef _DEBUG
    ones = _mm_setzero_si128();//disable warning C4700
#endif
    ones = _mm_cmpeq_epi32(ones,ones);
    ones = _mm_cmpeq_epi8(ones,alpha128);

    __m128i dst_lo128 = _mm_unpacklo_epi8(dst128, zero);
    __m128i alpha_lo128 = _mm_unpacklo_epi8(alpha128, zero);

    __m128i ones2 = _mm_unpacklo_epi8(ones, zero);    

    dst_lo128 = _mm_mullo_epi16(dst_lo128, alpha_lo128);
    dst_lo128 = _mm_adds_epu16(dst_lo128, ones2);
    dst_lo128 = _mm_srli_epi16(dst_lo128, 8);

    dst128 = _mm_unpackhi_epi8(dst128, zero);
    alpha128 = _mm_unpackhi_epi8(alpha128, zero);

    ones2 = _mm_unpackhi_epi8(ones, zero);

    dst128 = _mm_mullo_epi16(dst128, alpha128);
    dst128 = _mm_adds_epu16(dst128, ones2);
    dst128 = _mm_srli_epi16(dst128, 8);
    dst_lo128 = _mm_packus_epi16(dst_lo128, dst128);

    dst_lo128 = _mm_adds_epu8(dst_lo128, sub128);
    _mm_store_si128( reinterpret_cast<__m128i*>(dst), dst_lo128 );
}

/***
 * output not exactly identical to pix_alpha_blend_yv12_chroma
 */
static __forceinline void pix_alpha_blend_yv12_chroma_sse2(byte* dst, const byte* src, const byte* alpha, int src_pitch)
{
    __m128i zero = _mm_setzero_si128();
    __m128i alpha128_1 = _mm_load_si128( reinterpret_cast<const __m128i*>(alpha) );
    __m128i alpha128_2 = _mm_load_si128( reinterpret_cast<const __m128i*>(alpha+src_pitch) );
    __m128i dst128 = _mm_loadl_epi64( reinterpret_cast<const __m128i*>(dst) );

    __m128i sub128_1 = _mm_load_si128( reinterpret_cast<const __m128i*>(src) );
    __m128i sub128_2 = _mm_load_si128( reinterpret_cast<const __m128i*>(src+src_pitch) );

    AVERAGE_4_PIX_INTRINSICS(alpha128_1, alpha128_2);

    __m128i ones;
#ifdef _DEBUG
    ones = _mm_setzero_si128();//disable warning C4700
#endif
    ones = _mm_cmpeq_epi32(ones,ones);
    ones = _mm_cmpeq_epi8(ones, alpha128_1);
    
    dst128 = _mm_unpacklo_epi8(dst128, zero);
    __m128i dst128_2 = _mm_and_si128(dst128, ones);
    
    dst128 = _mm_mullo_epi16(dst128, alpha128_1);
    dst128 = _mm_adds_epu16(dst128, dst128_2);
    
    dst128 = _mm_srli_epi16(dst128, 8);
        
    AVERAGE_4_PIX_INTRINSICS(sub128_1, sub128_2);

    dst128 = _mm_adds_epi16(dst128, sub128_1);    
    dst128 = _mm_packus_epi16(dst128, dst128);
    
    _mm_storel_epi64( reinterpret_cast<__m128i*>(dst), dst128 );
}

static __forceinline void mix_16_y_p010_sse2(BYTE* dst, const BYTE* src, const BYTE* src_alpha)
{
    //important!
    __m128i alpha = _mm_load_si128( reinterpret_cast<const __m128i*>(src_alpha) );
    __m128i src_y = _mm_load_si128( reinterpret_cast<const __m128i*>(src) );
    __m128i dst_y = _mm_load_si128( reinterpret_cast<const __m128i*>(dst) );                        

    __m128i alpha_ff;
#ifdef _DEBUG
    alpha_ff = _mm_setzero_si128();//disable warning C4700
#endif
    alpha_ff = _mm_cmpeq_epi32(alpha_ff,alpha_ff);

    alpha_ff = _mm_cmpeq_epi8(alpha_ff, alpha);                                           

    __m128i lo = _mm_unpacklo_epi8(alpha_ff, alpha);//(alpha<<8)+0x100 will overflow
    //so we do it another way
    //first, (alpha<<8)+0xff
    __m128i ones = _mm_setzero_si128();
    ones = _mm_cmpeq_epi16(dst_y, ones);

    __m128i ones2;
#ifdef _DEBUG
    ones2 = _mm_setzero_si128();//disable warning C4700
#endif
    ones2 = _mm_cmpeq_epi32(ones2,ones2);

    ones = _mm_xor_si128(ones, ones2);                            
    ones = _mm_srli_epi16(ones, 15);
    ones = _mm_and_si128(ones, lo);

    dst_y = _mm_mulhi_epu16(dst_y, lo);
    dst_y = _mm_adds_epu16(dst_y, ones);//then add one if necessary

    lo = _mm_setzero_si128();
    lo = _mm_unpacklo_epi8(lo, src_y);
    dst_y = _mm_adds_epu16(dst_y, lo);                        
    _mm_store_si128( reinterpret_cast<__m128i*>(dst), dst_y );

    dst += 16;
    dst_y = _mm_load_si128( reinterpret_cast<const __m128i*>(dst) );

    lo = _mm_unpackhi_epi8(alpha_ff, alpha);

    ones = _mm_setzero_si128();
    ones = _mm_cmpeq_epi16(dst_y, ones);
    ones = _mm_xor_si128(ones, ones2);  
    ones = _mm_srli_epi16(ones, 15);
    ones = _mm_and_si128(ones, lo);    

    dst_y = _mm_mulhi_epu16(dst_y, lo); 
    dst_y = _mm_adds_epu16(dst_y, ones);

    lo = _mm_setzero_si128();
    lo = _mm_unpackhi_epi8(lo, src_y);
    dst_y = _mm_adds_epu16(dst_y, lo);
    _mm_store_si128( reinterpret_cast<__m128i*>(dst), dst_y );
}

//for test only
static void mix_16_y_p010_c(BYTE* dst, const BYTE* src, const BYTE* src_alpha)
{
    WORD* dst_word = reinterpret_cast<WORD*>(dst);
    for (int i=0;i<16;i++)
    {
        if (src_alpha[i]!=0xff)
        {
            dst_word[i] = ((dst_word[i] *src_alpha[i])>>8) + (src[i]<<8);
        }
    }
}

static __forceinline void pix_alpha_blend_yv12_chroma(byte* dst, const byte* src, const byte* alpha, int src_pitch)
{
    unsigned int ia = (alpha[0]+alpha[1]+
        alpha[0+src_pitch]+alpha[1+src_pitch])>>2;
    if(ia!=0xff)
    {
        *dst= (((*dst)*ia)>>8) + ((src[0]        +src[1]+
            src[src_pitch]+src[1+src_pitch] )>>2);
    }    
}
static __forceinline void mix_16_uv_p010_sse2(BYTE* dst, const BYTE* src, const BYTE* src_alpha, int pitch)
{
    //important!
    __m128i alpha = _mm_load_si128( reinterpret_cast<const __m128i*>(src_alpha) );
    __m128i alpha2 = _mm_load_si128( reinterpret_cast<const __m128i*>(src_alpha+pitch) );

    __m128i src_y = _mm_load_si128( reinterpret_cast<const __m128i*>(src) );
    __m128i dst_y = _mm_load_si128( reinterpret_cast<const __m128i*>(dst) );                        

    AVERAGE_4_PIX_INTRINSICS_2(alpha, alpha2);

    __m128i alpha_ff;
#ifdef _DEBUG
    alpha_ff = _mm_setzero_si128();//disable warning C4700
#endif
    alpha_ff = _mm_cmpeq_epi32(alpha_ff,alpha_ff);

    alpha_ff = _mm_cmpeq_epi8(alpha_ff, alpha);                                           

    __m128i lo = _mm_unpacklo_epi8(alpha_ff, alpha);//(alpha<<8)+0x100 will overflow
    //so we do it another way
    //first, (alpha<<8)+0xff
    __m128i ones = _mm_setzero_si128();
    ones = _mm_cmpeq_epi16(dst_y, ones);

    __m128i ones2;
#ifdef _DEBUG
    ones2 = _mm_setzero_si128();//disable warning C4700
#endif
    ones2 = _mm_cmpeq_epi32(ones2,ones2);
    ones = _mm_xor_si128(ones, ones2);                            
    ones = _mm_srli_epi16(ones, 15);
    ones = _mm_and_si128(ones, lo);

    dst_y = _mm_mulhi_epu16(dst_y, lo);
    dst_y = _mm_adds_epu16(dst_y, ones);//then add one if necessary

    lo = _mm_setzero_si128();
    lo = _mm_unpacklo_epi8(lo, src_y);
    dst_y = _mm_adds_epu16(dst_y, lo);                        
    _mm_store_si128( reinterpret_cast<__m128i*>(dst), dst_y );

    dst += 16;
    dst_y = _mm_load_si128( reinterpret_cast<const __m128i*>(dst) );

    lo = _mm_unpackhi_epi8(alpha_ff, alpha);

    ones = _mm_setzero_si128();
    ones = _mm_cmpeq_epi16(dst_y, ones);
    ones = _mm_xor_si128(ones, ones2);  
    ones = _mm_srli_epi16(ones, 15);
    ones = _mm_and_si128(ones, lo);    

    dst_y = _mm_mulhi_epu16(dst_y, lo); 
    dst_y = _mm_adds_epu16(dst_y, ones);

    lo = _mm_setzero_si128();
    lo = _mm_unpackhi_epi8(lo, src_y);
    dst_y = _mm_adds_epu16(dst_y, lo);
    _mm_store_si128( reinterpret_cast<__m128i*>(dst), dst_y );
}

static void mix_16_uv_p010_c(BYTE* dst, const BYTE* src, const BYTE* src_alpha, int pitch)
{
    WORD* dst_word = reinterpret_cast<WORD*>(dst);
    for (int i=0;i<8;i++, src_alpha+=2, src+=2, dst_word+=2)
    {
        unsigned int ia = ( 
            (src_alpha[0]+src_alpha[0+pitch]+1)/2+
            (src_alpha[1]+src_alpha[1+pitch]+1)/2+1)/2;
        if( ia!=0xFF )
        {
            int tmp = (((dst_word[0])*ia)>>8) + (src[0]<<8);
            if(tmp>0xffff) tmp = 0xffff;
            dst_word[0] = tmp;
            tmp = (((dst_word[1])*ia)>>8) + (src[1]<<8);
            if(tmp>0xffff) tmp = 0xffff;
            dst_word[1] = tmp;
        }
    }
}

static __forceinline void mix_16_uv_nvxx_sse2(BYTE* dst, const BYTE* src, const BYTE* src_alpha, int pitch)
{
    __m128i dst128 = _mm_load_si128( reinterpret_cast<const __m128i*>(dst) );
    __m128i alpha128_1 = _mm_load_si128( reinterpret_cast<const __m128i*>(src_alpha) );
    __m128i alpha128_2 = _mm_load_si128( reinterpret_cast<const __m128i*>(src_alpha+pitch) );
    __m128i sub128 = _mm_load_si128( reinterpret_cast<const __m128i*>(src) );

    AVERAGE_4_PIX_INTRINSICS_2(alpha128_1, alpha128_2);
    __m128i zero = _mm_setzero_si128();

    __m128i ones;
#ifdef _DEBUG
    ones = _mm_setzero_si128();//disable warning C4700
#endif
    ones = _mm_cmpeq_epi32(ones,ones);
    ones = _mm_cmpeq_epi8(ones,alpha128_1);

    __m128i dst_lo128 = _mm_unpacklo_epi8(dst128, zero);
    alpha128_2 = _mm_unpacklo_epi8(alpha128_1, zero);

    __m128i ones2 = _mm_unpacklo_epi8(ones, zero);    

    dst_lo128 = _mm_mullo_epi16(dst_lo128, alpha128_2);
    dst_lo128 = _mm_adds_epu16(dst_lo128, ones2);
    dst_lo128 = _mm_srli_epi16(dst_lo128, 8);

    dst128 = _mm_unpackhi_epi8(dst128, zero);
    alpha128_1 = _mm_unpackhi_epi8(alpha128_1, zero);

    ones2 = _mm_unpackhi_epi8(ones, zero);

    dst128 = _mm_mullo_epi16(dst128, alpha128_1);
    dst128 = _mm_adds_epu16(dst128, ones2);
    dst128 = _mm_srli_epi16(dst128, 8);
    dst_lo128 = _mm_packus_epi16(dst_lo128, dst128);

    dst_lo128 = _mm_adds_epu8(dst_lo128, sub128);
    _mm_store_si128( reinterpret_cast<__m128i*>(dst), dst_lo128 );
}

//for test only
static void mix_16_uv_nvxx_c(BYTE* dst, const BYTE* src, const BYTE* src_alpha, int pitch)
{
    for (int i=0;i<8;i++, src_alpha+=2, src+=2, dst+=2)
    {
        unsigned int ia = ( 
            (src_alpha[0]+src_alpha[0+pitch]+1)/2+
            (src_alpha[1]+src_alpha[1+pitch]+1)/2+1)/2;
        if( ia!=0xFF )
        {
            dst[0] = (((dst[0])*ia)>>8) + src[0];            
            dst[1] = (((dst[1])*ia)>>8) + src[1];
        }
    }
}

/******
 * hleft_vmid: 
 *   chroma placement(x=Y, o=U,V):
 *     x  x  x  x ...
 *     o     o    ...
 *     x  x  x  x ...
 *     o     o    ...
 *     x  x  x  x ...
 ******/
static __forceinline void hleft_vmid_subsample_and_interlace_2_line_c(BYTE* dst, const BYTE* u, const BYTE* v, int w, int pitch, int last_src_id=0)
{
    const BYTE* end = u + w;
    BYTE last_u = (u[last_src_id]+u[last_src_id+pitch]+1)/2;
    BYTE last_v = (v[last_src_id]+v[last_src_id+pitch]+1)/2;
    for (;u<end;dst+=2,u+=2,v+=2)
    {
        dst[0] = (u[0] + u[0+pitch] + 1)/2;
        int tmp1 = (u[1] + u[1+pitch] + 1)/2;
        last_u = (tmp1+last_u+1)/2;
        dst[0] = (dst[0] + last_u + 1)/2;
        last_u = tmp1;

        dst[1] = (v[0] + v[0+pitch] + 1)/2;
        tmp1 = (v[1] + v[1+pitch] + 1)/2;
        last_v = (tmp1+last_v+1)/2;
        dst[1] = (last_v + dst[1] + 1)/2;
        last_v = tmp1;
    }
}

// @w : w % 16 must == 0!
static __forceinline void hleft_vmid_subsample_and_interlace_2_line_sse2(BYTE* dst, const BYTE* u, const BYTE* v, int w, int pitch, int last_src_id=0)
{
    const BYTE* end_mod16 = u + (w&~15);

    __m128i u_last = _mm_cvtsi32_si128( (u[last_src_id]+u[pitch+last_src_id]+1)<<7 );
    __m128i v_last = _mm_cvtsi32_si128( (v[last_src_id]+v[pitch+last_src_id]+1)<<7 );
    for (;u<end_mod16;dst+=16,u+=16,v+=16)
    {
        __m128i u_1 = _mm_load_si128( reinterpret_cast<const __m128i*>(u) );
        __m128i u_2 = _mm_load_si128( reinterpret_cast<const __m128i*>(u+pitch) );
        __m128i v_1 = _mm_load_si128( reinterpret_cast<const __m128i*>(v) );    
        __m128i v_2 = _mm_load_si128( reinterpret_cast<const __m128i*>(v+pitch) );
        u_1 = _mm_avg_epu8(u_1, u_2);
        AVERAGE_4_PIX_INTRINSICS_3(u_1, u_last);
        v_1 = _mm_avg_epu8(v_1, v_2);
        AVERAGE_4_PIX_INTRINSICS_4(v_1, v_last);
        u_1 = _mm_or_si128(u_1, v_1);
        _mm_store_si128( reinterpret_cast<__m128i*>(dst), u_1 );
    }
    //The following fails if dst==u
    //hleft_vmid_subsample_and_interlace_2_line_c(dst, u, v, w&15, pitch, w>15?-1:0);
}

static __forceinline void hleft_vmid_mix_uv_yv12_c(byte* dst, int w, const byte* src, const byte* am, int src_pitch, int last_src_id=0)
{
    int last_alpha = (am[last_src_id]+am[last_src_id+src_pitch]+1)/2;
    int last_sub = (src[last_src_id]+src[last_src_id+src_pitch]+1)/2;
    const BYTE* end = src + w;
    for(; src < end; src += 2, am += 2, dst++)
    {
        int ia = (am[0]+am[0+src_pitch]+1)/2;
        int tmp1 = (am[1]+am[1+src_pitch]+1)/2;
        last_alpha = (last_alpha + tmp1 + 1)/2;
        ia = (ia + last_alpha + 1)/2;
        last_alpha = tmp1;

        if(ia!=0xff)
        {
            tmp1 = (src[0]+src[0+src_pitch]+1)/2;
            int tmp2 = (src[1]+src[1+src_pitch]+1)/2;
            last_sub = (last_sub+tmp2+1)/2;
            tmp1 = (tmp1+last_sub+1)/2;
            last_sub = tmp2;

            *dst= (((*dst)*ia)>>8) + tmp1;
        }
        else
        {
            last_sub = (src[1]+src[1+src_pitch]+1)/2;
        }
    }
}

//0<=w15<=15 && w15%2==0
static __forceinline void hleft_vmid_mix_uv_yv12_c2(byte* dst, int w15, const byte* src, const byte* am, int src_pitch, int last_src_id=0)
{
    ASSERT(w15>=0 && w15<=15 && (w15&1)==0 );

    int last_alpha = (am[last_src_id]+am[last_src_id+src_pitch]+1)/2;
    int last_sub = (src[last_src_id]+src[last_src_id+src_pitch]+1)/2;
    const BYTE* end = src + w15;

    switch(w15)
    {
    case 14:
#define _hleft_vmid_mix_uv_yv12_c2_mix_2 \
    int ia = (am[0]+am[0+src_pitch]+1)/2;\
    int tmp1 = (am[1]+am[1+src_pitch]+1)/2;\
    last_alpha = (last_alpha + tmp1 + 1)/2;\
    ia = (ia + last_alpha + 1)/2;\
    last_alpha = tmp1;\
    \
    if(ia!=0xff)\
    {\
        tmp1 = (src[0]+src[0+src_pitch]+1)/2;\
        int tmp2 = (src[1]+src[1+src_pitch]+1)/2;\
        last_sub = (last_sub+tmp2+1)/2;\
        tmp1 = (tmp1+last_sub+1)/2;\
        last_sub = tmp2;\
        \
        *dst= (((*dst)*ia)>>8) + tmp1;\
    }\
    else\
    {\
        last_sub = (src[1]+src[1+src_pitch]+1)/2;\
    }src += 2, am += 2, dst++

        { _hleft_vmid_mix_uv_yv12_c2_mix_2; }
    case 12:
        { _hleft_vmid_mix_uv_yv12_c2_mix_2; }
    case 10:
        { _hleft_vmid_mix_uv_yv12_c2_mix_2; }
    case 8:
        { _hleft_vmid_mix_uv_yv12_c2_mix_2 ; }
    case 6:
        { _hleft_vmid_mix_uv_yv12_c2_mix_2 ; }
    case 4:
        { _hleft_vmid_mix_uv_yv12_c2_mix_2; }
    case 2:
        { _hleft_vmid_mix_uv_yv12_c2_mix_2; }
    }
}

// am[last_src_id] valid && w&15=0
static __forceinline void hleft_vmid_mix_uv_yv12_sse2(byte* dst, int w00, const byte* src, const byte* am, int src_pitch, int last_src_id=0)
{
    ASSERT( (( (2*(intptr_t)dst) | w00 | (intptr_t)src | (intptr_t)am | src_pitch)&15)==0 );

    __m128i last_src = _mm_cvtsi32_si128( (src[last_src_id]+src[src_pitch+last_src_id]+1)<<7 );
    __m128i last_alpha = _mm_cvtsi32_si128( (am[last_src_id]+am[src_pitch+last_src_id]+1)<<7 );
    const BYTE* end_mod16 = src + (w00&~15);
    for(; src < end_mod16; src += 16, am += 16, dst+=8)
    {
        __m128i zero = _mm_setzero_si128();

        __m128i alpha128_1 = _mm_load_si128( reinterpret_cast<const __m128i*>(am) );
        __m128i tmp = _mm_load_si128( reinterpret_cast<const __m128i*>(am+src_pitch) );
        alpha128_1 = _mm_avg_epu8(alpha128_1, tmp);
        AVERAGE_4_PIX_INTRINSICS_3(alpha128_1, last_alpha);

        __m128i dst128 = _mm_loadl_epi64( reinterpret_cast<const __m128i*>(dst) );

        __m128i sub128_1 = _mm_load_si128( reinterpret_cast<const __m128i*>(src) );
        tmp = _mm_load_si128( reinterpret_cast<const __m128i*>(src+src_pitch) );
        sub128_1 = _mm_avg_epu8(sub128_1, tmp);
        AVERAGE_4_PIX_INTRINSICS_3(sub128_1, last_src);

        __m128i ones;
#ifdef _DEBUG
        ones = _mm_setzero_si128();//disable warning C4700
#endif
        ones = _mm_cmpeq_epi32(ones,ones);
        ones = _mm_cmpeq_epi8(ones, alpha128_1);

        dst128 = _mm_unpacklo_epi8(dst128, zero);
        __m128i dst128_2 = _mm_and_si128(dst128, ones);

        dst128 = _mm_mullo_epi16(dst128, alpha128_1);
        dst128 = _mm_adds_epu16(dst128, dst128_2);

        dst128 = _mm_srli_epi16(dst128, 8);

        dst128 = _mm_adds_epi16(dst128, sub128_1);    
        dst128 = _mm_packus_epi16(dst128, dst128);

        _mm_storel_epi64( reinterpret_cast<__m128i*>(dst), dst128 );
    }
}

static __forceinline void hleft_vmid_mix_uv_p010_c(BYTE* dst, int w, const BYTE* src, const BYTE* am, int src_pitch, int last_src_id=0)
{
    int last_alpha = (am[last_src_id]+am[src_pitch+last_src_id]+1)/2;
    const BYTE* end = src + w;
    WORD* dst_word = reinterpret_cast<WORD*>(dst); 
    for(; src < end; src+=2, am+=2, dst_word+=2)
    {
        int ia = (am[0]+am[0+src_pitch]+1)/2;
        int tmp2 = (am[1]+am[1+src_pitch]+1)/2;
        last_alpha = (last_alpha + tmp2 + 1)/2;
        ia = (ia + last_alpha + 1)/2;
        last_alpha = tmp2;

        if( ia!=0xFF )
        {
            int tmp = (((dst_word[0])*ia)>>8) + (src[0]<<8);
#ifdef XY_UNIT_TEST
            tmp ^= (tmp^0xffff)&((0xffff-tmp)>>31);//if(tmp>0xffff) tmp = 0xffff;
#endif
            dst_word[0] = tmp;
            tmp = (((dst_word[1])*ia)>>8) + (src[1]<<8);
#ifdef XY_UNIT_TEST
            tmp ^= (tmp^0xffff)&((0xffff-tmp)>>31);//if(tmp>0xffff) tmp = 0xffff;
#endif
            dst_word[1] = tmp;
        }

    }
}

//0<=w15<=15
static __forceinline void hleft_vmid_mix_uv_p010_c2(BYTE* dst, int w15, const BYTE* src, const BYTE* am, int src_pitch, int last_src_id=0)
{
    ASSERT(w15>=0 && w15<=15 && (w15&1)==0 );
    int last_alpha = (am[last_src_id]+am[src_pitch+last_src_id]+1)/2;
    WORD* dst_word = reinterpret_cast<WORD*>(dst); 

#ifdef XY_UNIT_TEST 
#  define  _hleft_vmid_mix_uv_p010_c2_CLIP(tmp) tmp ^= (tmp^0xffff)&((0xffff-tmp)>>31);/*if(tmp>0xffff) tmp = 0xffff;*/
#else
#  define  _hleft_vmid_mix_uv_p010_c2_CLIP(tmp) 
#endif

    switch(w15)
    {
    case 14:
#define _hleft_vmid_mix_uv_p010_c2_mix_2 \
    int ia = (am[0]+am[0+src_pitch]+1)/2;\
    int tmp2 = (am[1]+am[1+src_pitch]+1)/2;\
    last_alpha = (last_alpha + tmp2 + 1)/2;\
    ia = (ia + last_alpha + 1)/2;\
    last_alpha = tmp2;\
    \
    if( ia!=0xFF )\
    {\
        int tmp = (((dst_word[0])*ia)>>8) + (src[0]<<8);\
        _hleft_vmid_mix_uv_p010_c2_CLIP(tmp);\
        dst_word[0] = tmp;\
        tmp = (((dst_word[1])*ia)>>8) + (src[1]<<8);\
        _hleft_vmid_mix_uv_p010_c2_CLIP(tmp);\
        dst_word[1] = tmp;\
    } src+=2, am+=2, dst_word+=2

        { _hleft_vmid_mix_uv_p010_c2_mix_2; }
    case 12:
        { _hleft_vmid_mix_uv_p010_c2_mix_2; }
    case 10:
        { _hleft_vmid_mix_uv_p010_c2_mix_2; }
    case 8:
        { _hleft_vmid_mix_uv_p010_c2_mix_2; }
    case 6:
        { _hleft_vmid_mix_uv_p010_c2_mix_2; }
    case 4:
        { _hleft_vmid_mix_uv_p010_c2_mix_2; }
    case 2:
        { _hleft_vmid_mix_uv_p010_c2_mix_2; }
    }
}

// am[last_src_id] valid && w&15=0
static __forceinline void hleft_vmid_mix_uv_p010_sse2(BYTE* dst, int w00, const BYTE* src, const BYTE* am, int src_pitch, int last_src_id=0)
{
    ASSERT( (((intptr_t)dst | w00 | (intptr_t)src | (intptr_t)am | src_pitch)&15)==0 );
    __m128i last_alpha = _mm_cvtsi32_si128( (am[last_src_id]+am[src_pitch+last_src_id]+1)<<7 );
    const BYTE* end_mod16 = src + w00;
    for(; src < end_mod16; src+=16, am+=16, dst+=32)
    {
        //important!
        __m128i alpha = _mm_load_si128( reinterpret_cast<const __m128i*>(am) );
        __m128i alpha2 = _mm_load_si128( reinterpret_cast<const __m128i*>(am+src_pitch) );

        __m128i src_y = _mm_load_si128( reinterpret_cast<const __m128i*>(src) );
        __m128i dst_y = _mm_load_si128( reinterpret_cast<const __m128i*>(dst) );                        

        alpha = _mm_avg_epu8(alpha, alpha2);
        AVERAGE_4_PIX_INTRINSICS_5(alpha, last_alpha);

        __m128i alpha_ff;
#ifdef _DEBUG
        alpha_ff = _mm_setzero_si128();//disable warning C4700
#endif
        alpha_ff = _mm_cmpeq_epi32(alpha_ff,alpha_ff);

        alpha_ff = _mm_cmpeq_epi8(alpha_ff, alpha);                                           

        __m128i lo = _mm_unpacklo_epi8(alpha_ff, alpha);//(alpha<<8)+0x100 will overflow
        //so we do it another way
        //first, (alpha<<8)+0xff
        __m128i ones = _mm_setzero_si128();
        ones = _mm_cmpeq_epi16(dst_y, ones);

        __m128i ones2;
#ifdef _DEBUG
        ones2 = _mm_setzero_si128();//disable warning C4700
#endif
        ones2 = _mm_cmpeq_epi32(ones2,ones2);
        ones = _mm_xor_si128(ones, ones2);                            
        ones = _mm_srli_epi16(ones, 15);
        ones = _mm_and_si128(ones, lo);

        dst_y = _mm_mulhi_epu16(dst_y, lo);
        dst_y = _mm_adds_epu16(dst_y, ones);//then add one if necessary

        lo = _mm_setzero_si128();
        lo = _mm_unpacklo_epi8(lo, src_y);
        dst_y = _mm_adds_epu16(dst_y, lo);                        
        _mm_store_si128( reinterpret_cast<__m128i*>(dst), dst_y );

        dst_y = _mm_load_si128( reinterpret_cast<const __m128i*>(dst+16) );

        lo = _mm_unpackhi_epi8(alpha_ff, alpha);

        ones = _mm_setzero_si128();
        ones = _mm_cmpeq_epi16(dst_y, ones);
        ones = _mm_xor_si128(ones, ones2);  
        ones = _mm_srli_epi16(ones, 15);
        ones = _mm_and_si128(ones, lo);    

        dst_y = _mm_mulhi_epu16(dst_y, lo); 
        dst_y = _mm_adds_epu16(dst_y, ones);

        lo = _mm_setzero_si128();
        lo = _mm_unpackhi_epi8(lo, src_y);
        dst_y = _mm_adds_epu16(dst_y, lo);
        _mm_store_si128( reinterpret_cast<__m128i*>(dst+16), dst_y );
    }
}

static __forceinline void hleft_vmid_mix_uv_nv12_c(BYTE* dst, int w, const BYTE* src, const BYTE* am, int src_pitch, int last_src_id=0)
{
    int last_alpha = (am[last_src_id]+am[src_pitch+last_src_id]+1)/2;
    const BYTE* end = src + w;
    for(; src < end; src+=2, am+=2, dst+=2)
    {
        int ia = (am[0]+am[0+src_pitch]+1)/2;
        int tmp2 = (am[1]+am[1+src_pitch]+1)/2;
        last_alpha = (last_alpha + tmp2 + 1)/2;
        ia = (ia + last_alpha + 1)/2;
        last_alpha = tmp2;
        if ( ia!=0xFF )
        {
            dst[0] = (((dst[0])*ia)>>8) + src[0];            
            dst[1] = (((dst[1])*ia)>>8) + src[1];
        }
    }
}

//0<=w15<=15
static __forceinline void hleft_vmid_mix_uv_nv12_c2(BYTE* dst, int w15, const BYTE* src, const BYTE* am, int src_pitch, int last_src_id=0)
{
    ASSERT(w15>=0 && w15<=15 && (w15&1)==0 );
    int last_alpha = (am[last_src_id]+am[src_pitch+last_src_id]+1)/2;

    switch(w15)
    {
    case 14:
#define _hleft_vmid_mix_uv_nv12_c2_mix_2 \
    int ia = (am[0]+am[0+src_pitch]+1)/2;\
    int tmp2 = (am[1]+am[1+src_pitch]+1)/2;\
    last_alpha = (last_alpha + tmp2 + 1)/2;\
    ia = (ia + last_alpha + 1)/2;\
    last_alpha = tmp2;\
    if ( ia!=0xFF )\
    {\
        dst[0] = (((dst[0])*ia)>>8) + src[0];\
        dst[1] = (((dst[1])*ia)>>8) + src[1];\
    }\
    src+=2, am+=2, dst+=2

        { _hleft_vmid_mix_uv_nv12_c2_mix_2; }
    case 12:
        { _hleft_vmid_mix_uv_nv12_c2_mix_2; }
    case 10:
        { _hleft_vmid_mix_uv_nv12_c2_mix_2; }
    case 8:
        { _hleft_vmid_mix_uv_nv12_c2_mix_2; }
    case 6:
        { _hleft_vmid_mix_uv_nv12_c2_mix_2; }
    case 4:
        { _hleft_vmid_mix_uv_nv12_c2_mix_2; }
    case 2:
        { _hleft_vmid_mix_uv_nv12_c2_mix_2; }
    }
}

// am[last_src_id] valid && w&15=0
static __forceinline void hleft_vmid_mix_uv_nv12_sse2(BYTE* dst, int w00, const BYTE* src, const BYTE* am, int src_pitch, int last_src_id=0)
{
    ASSERT( (((intptr_t)dst | w00 | (intptr_t)src | (intptr_t)am | src_pitch)&15)==0 );
    __m128i last_alpha = _mm_cvtsi32_si128( (am[last_src_id]+am[src_pitch+last_src_id]+1)<<7 );
    const BYTE* end_mod16 = src + w00;
    for(; src < end_mod16; src+=16, am+=16, dst+=16)
    {
        __m128i dst128 = _mm_load_si128( reinterpret_cast<const __m128i*>(dst) );
        __m128i alpha128_1 = _mm_load_si128( reinterpret_cast<const __m128i*>(am) );
        __m128i alpha128_2 = _mm_load_si128( reinterpret_cast<const __m128i*>(am+src_pitch) );
        __m128i sub128 = _mm_load_si128( reinterpret_cast<const __m128i*>(src) );

        alpha128_1 = _mm_avg_epu8(alpha128_1, alpha128_2);
        AVERAGE_4_PIX_INTRINSICS_5(alpha128_1, last_alpha);

        __m128i zero = _mm_setzero_si128();

        __m128i ones;
#ifdef _DEBUG
        ones = _mm_setzero_si128();//disable warning C4700
#endif
        ones = _mm_cmpeq_epi32(ones,ones);
        ones = _mm_cmpeq_epi8(ones,alpha128_1);

        __m128i dst_lo128 = _mm_unpacklo_epi8(dst128, zero);
        alpha128_2 = _mm_unpacklo_epi8(alpha128_1, zero);

        __m128i ones2 = _mm_unpacklo_epi8(ones, zero);    

        dst_lo128 = _mm_mullo_epi16(dst_lo128, alpha128_2);
        dst_lo128 = _mm_adds_epu16(dst_lo128, ones2);
        dst_lo128 = _mm_srli_epi16(dst_lo128, 8);

        dst128 = _mm_unpackhi_epi8(dst128, zero);
        alpha128_1 = _mm_unpackhi_epi8(alpha128_1, zero);

        ones2 = _mm_unpackhi_epi8(ones, zero);

        dst128 = _mm_mullo_epi16(dst128, alpha128_1);
        dst128 = _mm_adds_epu16(dst128, ones2);
        dst128 = _mm_srli_epi16(dst128, 8);
        dst_lo128 = _mm_packus_epi16(dst_lo128, dst128);

        dst_lo128 = _mm_adds_epu8(dst_lo128, sub128);
        _mm_store_si128( reinterpret_cast<__m128i*>(dst), dst_lo128 );
    }
}

#endif // __XY_INTRINSICS_D66EF42F_67BC_47F4_A70D_40F1AB80F376_H__