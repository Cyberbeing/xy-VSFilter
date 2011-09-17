/************************************************************************/
/* author: xy                                                           */
/* date: 20110514                                                       */
/************************************************************************/
#ifndef __XY_MALLOC_H_22FA5F7D_A5C3_4D8D_B4E6_5FB954770019__
#define __XY_MALLOC_H_22FA5F7D_A5C3_4D8D_B4E6_5FB954770019__

/* xy_malloc : will do or emulate a memalign
 * you have to use xy_free for buffers allocated with xy_malloc */
void *xy_malloc( int );
void *xy_realloc( void *p, int i_size );
void  xy_free( void * );

#endif // end of __XY_MALLOC_H_22FA5F7D_A5C3_4D8D_B4E6_5FB954770019__

