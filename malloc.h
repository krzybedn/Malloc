/*
	Krzysztof Bednarek
	292974
	Systemy operacyjne (zaawansowane)
	pracownia 3
*/

#ifndef MALLOC_INCLUDED
#define MALLOC_INCLUDED 1

#include <string.h>
#include <unistd.h>
#include <sys/queue.h> 
#include <sys/mman.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdio.h>
#include <pthread.h>
#include <errno.h>


typedef struct mem_block
{
	int64_t mb_size;
	union
	{
		LIST_ENTRY(mem_block) bl_node; // node on free block list, valid if block is free 	
		void* data;	
	};
} mem_block_t;

typedef struct mem_chunk
{
	LIST_ENTRY(mem_chunk) cl_node; /* node on list of all chunks */
	LIST_HEAD(block_l, mem_block) blocks; /* list of all free blocks in the chunk */
	int64_t mc_size;
} mem_chunk_t;

LIST_HEAD(chunk_l, mem_chunk) chunk_list; /* list of all chunks */





void *my_malloc(size_t size);
void *my_calloc(size_t count, size_t size);
void *my_realloc(void *ptr, size_t size);
int my_posix_memalign(void **memptr, size_t alignment, size_t size);
void my_free(void *ptr);

void mdump();



#endif
