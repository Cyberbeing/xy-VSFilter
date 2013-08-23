/************************************************************************/
/* author: xy                                                           */
/* date: 20110514                                                       */
/************************************************************************/
#ifndef __XY_MALLOC_H_22FA5F7D_A5C3_4D8D_B4E6_5FB954770019__
#define __XY_MALLOC_H_22FA5F7D_A5C3_4D8D_B4E6_5FB954770019__

#include <cstddef>

/* xy_malloc : will do or emulate a memalign
 * you have to use xy_free for buffers allocated with xy_malloc */
void *xy_malloc( std::size_t size, int align_shift=0);
void *xy_realloc( void *p, std::size_t i_size, int align_shift=0 );
void  xy_free( void * );

extern std::size_t g_xy_malloc_used_size;

#endif // end of __XY_MALLOC_H_22FA5F7D_A5C3_4D8D_B4E6_5FB954770019__

