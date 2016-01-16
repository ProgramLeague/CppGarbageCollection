#pragma once
#include "MemoryChunk.h"
/** @ StaticMemory.h
 * 定义实现内存池
 * 采用固定大小策略进行内存管理与分配
 * 减少因大量小内存分配导致的内存碎片增加
 */
truct HeapHeader
{
	size_t size;
};
struct MemoryHeap
{
	HeapHeader	header;
	char		pBuffer;
};


class StaticMemory
{
public:
	typedef enum { MAX_SIZE = 1024, MIN_SIZE = sizeof(MemoryChunk*) };
	StaticMemory()
	{
		chunkcount = 0;
		for ( size_t size = MIN_SIZE; size <= MAX_SIZE; size *= 2 )
			++chunkcount;
/* pChunkList=(MemoryChunk**)malloc(sizeof(MemoryChunk*)*chunkcount); */
		pChunkList = new MemoryChunk*[chunkcount];
		int index = 0;
		for ( size_t size = MIN_SIZE; size <= MAX_SIZE; size *= 2 )
		{
			pChunkList[index++] = new MemoryChunk( size, 1000 );
		}
	}


	~StaticMemory()
	{
		for ( int index = 0; index < chunkcount; ++index )
		{
			delete pChunkList[index];
		}
/* free(pChunkList); */
		delete[] pChunkList;
	}


	void* Malloc( size_t size )
	{
		if ( size > MAX_SIZE )
		{
			return(malloc( size ) );
		}
		int index = 0;
		for ( size_t tsize = MIN_SIZE; tsize <= MAX_SIZE; tsize *= 2 )
		{
			if ( tsize >= size )
				break;
			++index;
		}
		return(pChunkList[index]->malloc() );
	}


	void Free( void* pMem )
	{
		if ( !free( pMem ) )
			MemoryChunk::free( pMem );
	}


protected:
	void* malloc( size_t size )
	{
		MemoryHeap* pHeap = (MemoryHeap *) ::malloc( sizeof(HeapHeader) + size );
		if ( pHeap )
		{
			pHeap->header.size = size;
			return(&pHeap->pBuffer);
		}
		return(NULL);
	}


	bool free( void* pMem )
	{
		MemoryHeap* pHeap = (MemoryHeap *) ( (char *) pMem - sizeof(HeapHeader) );
		if ( pHeap->header.size > MAX_SIZE )
		{
			::free( pHeap );
			return(true);
		}
		return(false);
	}


private:
	MemoryChunk	** pChunkList;
	int		chunkcount;
};
#endif
