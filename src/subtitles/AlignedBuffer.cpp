#include "AlignedBuffer.h"

#include <hash_map>

using namespace std;

typedef hash_map<void*, void*> BUFFER_PTR_MAP;
static BUFFER_PTR_MAP alloced_buffers;

void* newAlignedBuffer(int size, int alignment)
{
	if(alignment<=0)
		return NULL;
	void * origin = malloc(size+alignment);
	int temp = (int)origin + alignment;
	void * result = (void*)(temp - temp % alignment);
	alloced_buffers.insert(BUFFER_PTR_MAP::value_type(result, origin));
	return result;
}

void deleteAlignedBuffer(void* ptr)
{
	BUFFER_PTR_MAP::iterator iter = alloced_buffers.find(ptr);
	if(iter!=alloced_buffers.end())
	{
		void* origin = iter->first;
		free(origin);
		alloced_buffers.erase(iter);
	}
}