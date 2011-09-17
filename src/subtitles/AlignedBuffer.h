#ifndef _ALIGNED_BUFFER_H
#define _ALIGNED_BUFFER_H

void * newAlignedBuffer(int size, int alignment);
void deleteAlignedBuffer(void* ptr);

#endif