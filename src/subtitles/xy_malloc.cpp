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
void *xy_malloc( int i_size, int align_shift )
{
    const int ALIGNMENT = 16;
    const int MASK = ALIGNMENT - 1;
    uint8_t * buf;
    uint8_t * align_buf;
    align_shift &= MASK;
    buf = (uint8_t *) malloc( i_size + MASK + sizeof( void ** ) +
              sizeof( int ) );
    align_buf = buf + MASK + sizeof( void ** ) + sizeof( int );
    align_buf -= (ALIGNMENT + ((intptr_t) align_buf & MASK) - align_shift) & MASK;
    *( (void **) ( align_buf - sizeof( void ** ) ) ) = buf;
    *( (int *) ( align_buf - sizeof( void ** ) - sizeof( int ) ) ) = i_size;
    return align_buf;
}

/****************************************************************************
 * xy_free:
 ****************************************************************************/
void xy_free( void *p )
{
    if( p )
    {
        free( *( ( ( void **) p ) - 1 ) );
    }
}

/****************************************************************************
 * xy_realloc:
 ****************************************************************************/
void *xy_realloc( void *p, int i_size, int align_shift )
{
    int       i_old_size = 0;
    uint8_t * p_new;
    if( p )
    {
        i_old_size = *( (int*) ( (uint8_t*) p - sizeof( void ** ) -
                         sizeof( int ) ) );
    }
    p_new = (uint8_t*)xy_malloc( i_size, align_shift );
    if( i_old_size > 0 && i_size > 0 )
    {
        memcpy( p_new, p, ( i_old_size < i_size ) ? i_old_size : i_size );
    }
    xy_free( p );
    return p_new;
}


