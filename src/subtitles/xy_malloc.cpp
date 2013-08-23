/************************************************************************/
/* author: xy                                                           */
/* date: 20110514                                                       */
/************************************************************************/
#include "xy_malloc.h"
#include <stdint.h>
#include <malloc.h>
#include <string.h>

extern std::size_t g_xy_malloc_used_size = 0;

//gathered from x264
/****************************************************************************
 * xy_malloc:
 ****************************************************************************/
void *xy_malloc( std::size_t i_size, int align_shift )
{
    const int ALIGNMENT = 16;
    const int MASK = ALIGNMENT - 1;
    uint8_t * buf;
    uint8_t * align_buf;
    align_shift &= MASK;
    buf = (uint8_t *) malloc( i_size + MASK + sizeof( void ** ) +
              sizeof( i_size ) );
    if (!buf)
    {
        return NULL;
    }
    align_buf = buf + MASK + sizeof( void ** ) + sizeof( i_size );
    align_buf -= (ALIGNMENT + ((intptr_t) align_buf & MASK) - align_shift) & MASK;
    *( (void **) ( align_buf - sizeof( void ** ) ) ) = buf;
    *( (std::size_t *) ( align_buf - sizeof( void ** ) - sizeof( i_size ) ) ) = i_size;
    g_xy_malloc_used_size += i_size;
    return align_buf;
}

/****************************************************************************
 * xy_free:
 ****************************************************************************/
void xy_free( void *p )
{
    if( p )
    {
        g_xy_malloc_used_size -= *( (std::size_t*) ( (uint8_t*) p - sizeof( void ** ) -
            sizeof( std::size_t ) ) );
        free( *( ( ( void **) p ) - 1 ) );
    }
}

/****************************************************************************
 * xy_realloc:
 ****************************************************************************/
void *xy_realloc( void *p, std::size_t i_size, int align_shift )
{
    std::size_t i_old_size = 0;
    uint8_t    *p_new = NULL;
    p_new = (uint8_t*)xy_malloc( i_size, align_shift );
    if (p_new && p)
    {
        i_old_size = *( (std::size_t*) ( (uint8_t*) p - sizeof( void ** ) -
            sizeof( i_size ) ) );
        memcpy( p_new, p, ( i_old_size < i_size ) ? i_old_size : i_size );
        xy_free(p);
    }
    return p_new;
}


