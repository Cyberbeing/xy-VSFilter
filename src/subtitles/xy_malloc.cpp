/************************************************************************/
/* author: xy                                                           */
/* date: 20110514                                                       */
/************************************************************************/
#include "xy_malloc.h"
#include <stdint.h>
#include <malloc.h>
#include <string.h>

//gathered from x264
/****************************************************************************
 * xy_malloc:
 ****************************************************************************/
void *xy_malloc( int i_size )
{
#ifdef SYS_MACOSX
    /* Mac OS X always returns 16 bytes aligned memory */
    return malloc( i_size );
#elif defined( HAVE_MALLOC_H )
    return memalign( 16, i_size );
#else
    uint8_t * buf;
    uint8_t * align_buf;
    buf = (uint8_t *) malloc( i_size + 15 + sizeof( void ** ) +
              sizeof( int ) );
    align_buf = buf + 15 + sizeof( void ** ) + sizeof( int );
    align_buf -= (intptr_t) align_buf & 15;
    *( (void **) ( align_buf - sizeof( void ** ) ) ) = buf;
    *( (int *) ( align_buf - sizeof( void ** ) - sizeof( int ) ) ) = i_size;
    return align_buf;
#endif
}

/****************************************************************************
 * xy_free:
 ****************************************************************************/
void xy_free( void *p )
{
    if( p )
    {
#if defined( HAVE_MALLOC_H ) || defined( SYS_MACOSX )
        free( p );
#else
        free( *( ( ( void **) p ) - 1 ) );
#endif
    }
}

/****************************************************************************
 * xy_realloc:
 ****************************************************************************/
void *xy_realloc( void *p, int i_size )
{
#ifdef HAVE_MALLOC_H
    return realloc( p, i_size );
#else
    int       i_old_size = 0;
    uint8_t * p_new;
    if( p )
    {
        i_old_size = *( (int*) ( (uint8_t*) p - sizeof( void ** ) -
                         sizeof( int ) ) );
    }
    p_new = (uint8_t*)xy_malloc( i_size );
    if( i_old_size > 0 && i_size > 0 )
    {
        memcpy( p_new, p, ( i_old_size < i_size ) ? i_old_size : i_size );
    }
    xy_free( p );
    return p_new;
#endif
}


