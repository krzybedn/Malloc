/*
	Krzysztof Bednarek
	292974
	Systemy operacyjne (zaawansowane)
	pracownia 3
*/

#include "malloc.h"

pthread_mutex_t main_mutex;

#define MALLOC_DEBUG 0

#if MALLOC_DEBUG == 1
	mem_chunk_t* last_chunk = NULL;
#endif

#define CHUNK_SIZE ((int64_t)sizeof(mem_chunk_t))
#define BLOCK_BEGIN_SIZE ((int64_t)sizeof(mem_block_t))
#define BLOCK_END_SIZE ((int64_t)sizeof(int64_t))
#define BLOCK_META_SIZE (BLOCK_BEGIN_SIZE + BLOCK_END_SIZE)
#define PAGE_SIZE getpagesize()
#define MAX_FREE_CHUNKS 1


#define powerof2(x)	((((x)-1)&(x))==0)


int free_chunks = 0;


/*******************************
	POINTERS MOVE
*******************************/

mem_block_t* move_block(mem_block_t* block, size_t step)
{
	if(block == NULL)
		//Niewazny wskaznik po przesunieciu powinien pozostac niewazny
		return NULL;

	return (mem_block_t*)((void*)block + step);
}

void* block_data_address(mem_block_t* block)
	{return move_block(block, BLOCK_BEGIN_SIZE);}

int64_t* block_end_meta_address(mem_block_t* block)
	{return (int64_t*)move_block(block, BLOCK_BEGIN_SIZE + abs(block->mb_size));}

mem_block_t* block_address_from_end(int64_t* end)
	{return (mem_block_t*)((void*)end- abs(~(*end)) - BLOCK_BEGIN_SIZE);}

mem_block_t* next_block_address(mem_block_t* block)
	{return move_block(block, abs(block->mb_size) + BLOCK_META_SIZE);}

mem_block_t* prev_block_address(mem_block_t* block)
	{return block_address_from_end((int64_t*)move_block(block, -BLOCK_END_SIZE));}

mem_chunk_t* move_chunk(mem_chunk_t* chunk, size_t step)
	{return (mem_chunk_t*)((void*)chunk + step);}

void* chunk_data_address(mem_chunk_t* chunk)
	{return move_chunk(chunk, CHUNK_SIZE);}

int64_t fix_size(int64_t size)//nasze meta dane skladaja sie ze struktury majcacej rozmiar wielokrotnosci slowa maszynowego na poczatku
								// oraz z 32 bitowego inta na końcu. Zatem chcemy aby wlasciwe dane mialy rozmiar bedacy wielokrotnoscia
								// rozmiaru slowa maszynowego + 32 bity				
	{return size + (8 - size%8)%8;}


void* address_end_chunk(mem_chunk_t* chunk)
	{return move_chunk(chunk, chunk->mc_size + CHUNK_SIZE);}

mem_block_t* block_meta_address_from_data(void* ptr)
{
	if(ptr == NULL)
		return NULL;
	return (mem_block_t*)(ptr - BLOCK_BEGIN_SIZE);//wiemy, ze bezposrednio przed danymi znajduje sie wskaznik na poczatek bloku
}


/*******************************
	DEBUG
*******************************/


void debbug_chunk_write(mem_chunk_t* chunk)//Wypisujemy zawartosc pojedynczego chunka
{//Przy wypisywaniu wypisujemy rozmiar zawartosci, bez romiaru meatadanych
	char msg[100];
	snprintf(msg, 100, ">>%p - %lu \t", chunk, chunk->mc_size);
	write(STDERR_FILENO, msg, strlen(msg));

	mem_block_t* block = (mem_block_t*)move_chunk(chunk, CHUNK_SIZE);
	block = next_block_address(block);
	do
	{
		snprintf(msg, 100, "| %p - %lu\t", block, block->mb_size);
		write(STDERR_FILENO, msg, strlen(msg));
		block = next_block_address(block);
	}
	while(block->mb_size != 0);
	write(STDERR_FILENO, "\n\n", 2);
}

void mdump()
{
	mem_chunk_t* chunk = LIST_FIRST(&chunk_list);
	while(chunk != NULL)
	{
		debbug_chunk_write(chunk);
		chunk = LIST_NEXT(chunk, cl_node);
	}	
}


/*******************************
	CREATING AND EDITING DATA
*******************************/


mem_block_t* search_block_in_chunk(mem_block_t* block, size_t size)
{
	while(block != NULL)
	{
		if(block->mb_size >= (int64_t)size + BLOCK_META_SIZE)
			return block;
		block = LIST_NEXT(block, bl_node);
	}
	return NULL;
}

mem_block_t* search_block(size_t size)
{
	mem_chunk_t* chunk = LIST_FIRST(&chunk_list);
	while(chunk != NULL)
	{
		mem_block_t* candidate = search_block_in_chunk(LIST_FIRST(&chunk->blocks), size);
		if(candidate != NULL)
		{
			return candidate;
		}
		chunk = LIST_NEXT(chunk, cl_node);
	}
	return NULL;
}

void set_block_size(mem_block_t* block, int64_t size)
{
	block->mb_size = size;
	int64_t* end = block_end_meta_address(block);
	*end = ~(block->mb_size);
}


short is_block_in_chunk(mem_block_t* block, mem_chunk_t* chunk)
{
	return ((void*)chunk <= (void*)block) && (address_end_chunk(chunk) > (void*)block);
}

mem_chunk_t* find_containging_chunk(mem_block_t* block)
{
	mem_chunk_t* chunk = LIST_FIRST(&chunk_list);
	while(chunk != NULL)
	{
		if(is_block_in_chunk(block, chunk))
			return chunk;
		chunk = LIST_NEXT(chunk, cl_node);
	}
	return NULL;
}


void find_and_insert(mem_block_t* block)
{
	mem_chunk_t* chunk = find_containging_chunk(block);
	mem_block_t* actual = LIST_FIRST(&chunk->blocks);
	mem_block_t* prev = NULL;
	while(actual != NULL)
	{
		if(actual > block)
		{
			LIST_INSERT_BEFORE(actual, block, bl_node);
			return;
		}
		prev = actual;
		actual = LIST_NEXT(actual, bl_node);
	}
	if(prev == NULL)
		LIST_INSERT_HEAD(&chunk->blocks, block, bl_node);
	else
		LIST_INSERT_AFTER(prev, block, bl_node);
}

void insert(mem_block_t* block)
{
	mem_block_t* prev_block = prev_block_address(block);
	mem_block_t* next_block = next_block_address(block);

	if(prev_block->mb_size > 0)
		LIST_INSERT_AFTER(prev_block, block, bl_node);
	else if(next_block->mb_size > 0)
		LIST_INSERT_BEFORE(next_block, block, bl_node);
	else
		find_and_insert(block);
}

mem_block_t* create_new_block(void* place, size_t size, short should_insert)
{
	mem_block_t* block = place;
	set_block_size(block, size);

	if(should_insert)
		insert(block);
	else
		block->data = block_data_address(block);
	return block;
}

mem_chunk_t* create_new_chunk(size_t size)
{
	mem_chunk_t* chunk;

	if((int64_t)size + CHUNK_SIZE + 3*BLOCK_META_SIZE > PAGE_SIZE)//Jedna strona nie wystarczy
	{
		size = size + CHUNK_SIZE + 3*BLOCK_META_SIZE;

		//Wyrownujemy rozmiar do pelnych stron
		size = PAGE_SIZE * (size/PAGE_SIZE) + ((size % PAGE_SIZE != 0) ? PAGE_SIZE : 0);
	}
	else
	{
		size = PAGE_SIZE;
	}
	chunk = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, 0, 0);
	if(chunk == MAP_FAILED)
		return NULL;
	chunk->mc_size = size - CHUNK_SIZE;
	LIST_INSERT_HEAD(&chunk_list, chunk, cl_node);
	LIST_INIT(&chunk->blocks);

	//Konce chunka oznaczamy blokami o rozmiarze 0
	//Ulatwi to sprawdzanie, czy nie wyskoczylismy poza jego granice
	create_new_block(move_chunk(chunk, CHUNK_SIZE), 0, 0);
	create_new_block(move_chunk(chunk, size - BLOCK_META_SIZE), 0, 0);

	create_new_block(move_chunk(chunk, CHUNK_SIZE + BLOCK_META_SIZE), size-CHUNK_SIZE-3*BLOCK_META_SIZE, 1);

	return chunk;

}

void merge_and_insert(mem_block_t* block)
{//Laczymy blok z sasiadami, a nastepnie wlaczamy go na liste wolnych
	mem_block_t* prev_block = prev_block_address(block);
	mem_block_t* next_block = next_block_address(block);
	if(prev_block->mb_size > 0)
	{//Poprzedni blok jest wolny, wiec mozemy je scalic
		set_block_size(prev_block, block->mb_size + prev_block->mb_size + BLOCK_META_SIZE);

		if(next_block->mb_size > 0)
		{//Nastepny blok jest wolny, wiec mozemy je scalic
			LIST_REMOVE(next_block, bl_node);
			set_block_size(prev_block, prev_block->mb_size + next_block->mb_size + BLOCK_META_SIZE);
		}
	}
	else if(next_block->mb_size > 0)
	{//Nastepny blok jest wolny, wiec mozemy je scalic
		set_block_size(block, block->mb_size + next_block->mb_size + BLOCK_META_SIZE);
		LIST_INSERT_BEFORE(next_block, block, bl_node);
		LIST_REMOVE(next_block, bl_node);
	}
	else
	{
		find_and_insert(block);
	}
}


/*******************************
	MAIN FUNCTIONS
*******************************/


void* my_malloc(size_t size)
{
	if(LIST_EMPTY(&chunk_list))
	{
		if(pthread_mutex_init(&main_mutex, NULL) != 0)
			return NULL;
		LIST_INIT(&chunk_list);
	}

#if MALLOC_DEBUG == 1
	pthread_mutex_lock(&main_mutex);
	char msg[100];
	
	snprintf(msg, 100, "malloc %lu\n",size);
	write(STDERR_FILENO, msg, strlen(msg));
	
	if(last_chunk != NULL)
	{
		debbug_chunk_write(last_chunk);
		last_chunk = NULL;
	}
	pthread_mutex_unlock(&main_mutex);
#endif

	pthread_mutex_lock(&main_mutex);

	size = fix_size(size);

	mem_block_t* block = search_block(size);

#if MALLOC_DEBUG == 1
	last_chunk = find_containging_chunk(block);
#endif

	if(block == NULL)//Nie ma wolnych blokow wystarczajcych rozmiarow, wiec tworzymy nowe miejsce
	{
		mem_chunk_t* chunk = create_new_chunk(size);

#if MALLOC_DEBUG == 1
		last_chunk = chunk;
#endif
		if(chunk == NULL)
		{
			pthread_mutex_unlock(&main_mutex);
			return NULL;
		}
		block = LIST_FIRST(&chunk->blocks);
	}
	if(prev_block_address(block)->mb_size == 0 && next_block_address(block)->mb_size == 0)//jedyny blok w chunku
		free_chunks--;

	if(block->mb_size <= (int64_t)size + BLOCK_META_SIZE)//Po stworzeniu nowego bloku zostanie za malo miesca na strukture.
	{
		LIST_REMOVE(block, bl_node);
		set_block_size(block, -(block->mb_size));
		block->data = block_data_address(block);
		
		pthread_mutex_unlock(&main_mutex);
		return block->data;
	}
	else
	{
		void* next_place = move_block(block, block->mb_size - size);
		set_block_size(block, block->mb_size - (BLOCK_META_SIZE + size));

		mem_block_t* new_block = create_new_block(next_place, size, 0);
		set_block_size(new_block, -(new_block->mb_size));
		
		pthread_mutex_unlock(&main_mutex);
		return new_block->data;
	}
}

void *my_calloc(size_t count, size_t size)
{
#if MALLOC_DEBUG == 1
	pthread_mutex_lock(&main_mutex);
	char msg[100];
	
	snprintf(msg, 100, "calloc %lu %lu\n", count, size);
	write(STDERR_FILENO, msg, strlen(msg));
	
	if(last_chunk != NULL)
	{
		debbug_chunk_write(last_chunk);
		last_chunk = NULL;
	}
	pthread_mutex_unlock(&main_mutex);
#endif

	if(count == 0 || size == 0)
		return NULL;
	void* res = my_malloc(size * count);
	if(res == NULL)
		return NULL;

	memset(res, 0, size * count);
	return res;
}

void *my_realloc(void *ptr, size_t size)
{
	mem_block_t* block = block_meta_address_from_data(ptr);

#if MALLOC_DEBUG == 1
	pthread_mutex_lock(&main_mutex);

	char msg[100];
	
	snprintf(msg, 100, "realloc %p %lu\n", block, size);
	write(STDERR_FILENO, msg, strlen(msg));
	
	if(last_chunk != NULL)
	{
		debbug_chunk_write(last_chunk);
		last_chunk = NULL;
	}
	pthread_mutex_unlock(&main_mutex);
#endif

	if(block == NULL)
	{
		void* new_ptr = my_malloc(size);
		return new_ptr;
	}
	if(size == 0)
	{
		my_free(ptr);
		return NULL;
	}

	pthread_mutex_lock(&main_mutex);


#if MALLOC_DEBUG == 1
	last_chunk = find_containging_chunk(block);;
#endif

	size = fix_size(size);
	int64_t prev_size = -(block->mb_size);

	if(prev_size >= (int64_t)size)//Nowy rozmiar jest mniejszy
	{
		if(prev_size <= (int64_t)size + BLOCK_META_SIZE)//Po stworzeniu nowego bloku zostanie za malo miesca na strukture.
		{
			pthread_mutex_unlock(&main_mutex);
			return ptr;
		}
		set_block_size(block, -size);
		create_new_block(move_block(block, size + BLOCK_META_SIZE), prev_size - size - BLOCK_META_SIZE, 1);

		pthread_mutex_unlock(&main_mutex);
		return ptr;
	}
	else//Nowy rozmiar jest większy od poprzedniego
	{
		mem_block_t* next_block = next_block_address(block);
		if(next_block->mb_size > 0  && next_block->mb_size + prev_size + BLOCK_META_SIZE >= (int64_t)size)
		{//W nastepnym bloku jest wystarczajaco miejsca zeby powiekszyc stary blok 
			if(next_block->mb_size + prev_size > (int64_t)size)
			{//Po przesuniecie czesci pamieci pozostnie nam miejsce na struktury


				LIST_REMOVE(next_block, bl_node);

				int64_t new_size = prev_size + next_block->mb_size - size;
				next_block = move_block(next_block, next_block->mb_size - new_size);
				set_block_size(next_block, new_size);
				set_block_size(block, -size);
				insert(next_block);
				block->data = block_data_address(block);
				
				pthread_mutex_unlock(&main_mutex);
				return block->data;
			}
			else
			{//Po zmianie granic nie bedzie wstarczajaco miejsca na metadane dla drugiego bloku

				LIST_REMOVE(next_block, bl_node);
				set_block_size(block, -(prev_size + BLOCK_META_SIZE + next_block->mb_size));
				block->data = block_data_address(block);
			
				pthread_mutex_unlock(&main_mutex);
				return block->data;
			}
		}
		else
		{//Musimy przeniesc wczystko do nowego bloku
			pthread_mutex_unlock(&main_mutex);

			void* new_ptr = my_malloc(size);

			pthread_mutex_lock(&main_mutex);

			if(new_ptr == NULL)
			{
				pthread_mutex_unlock(&main_mutex);
				return NULL;
			}

			memcpy(new_ptr, block_data_address(block), prev_size);

			pthread_mutex_unlock(&main_mutex);

			my_free(ptr);
			return new_ptr;
		}
	}
		
}

void my_free(void* ptr)
{
	pthread_mutex_lock(&main_mutex);

	mem_block_t* block = block_meta_address_from_data(ptr);

#if MALLOC_DEBUG == 1
	char msg[100];

	snprintf(msg, 100, "free %p %p\n", ptr, block);
	write(STDERR_FILENO, msg, strlen(msg));
	
	if(last_chunk != NULL)
	{
		debbug_chunk_write(last_chunk);
		last_chunk = NULL;
	}
#endif
	if(block == NULL)
	{
		pthread_mutex_unlock(&main_mutex);
		return;
	}

#if MALLOC_DEBUG == 1
	last_chunk = find_containging_chunk(block);
#endif
	

	set_block_size(block, -(block->mb_size));
	merge_and_insert(block);

	if(prev_block_address(block)->mb_size == 0 && next_block_address(block)->mb_size == 0)//Zwrocony blok stanowi calosc danego chunku
		free_chunks++;

	if(free_chunks > MAX_FREE_CHUNKS)//mamy duzo wolnych chunkow, mozemy czesc z nich zwolnic
	{
		mem_chunk_t* chunk = find_containging_chunk(block);
		LIST_REMOVE(chunk, cl_node);
		munmap(chunk, chunk->mc_size);
	}

	pthread_mutex_unlock(&main_mutex);

}


int my_posix_memalign(void **memptr, size_t alignment, size_t size)
{
#if MALLOC_DEBUG == 1
	pthread_mutex_lock(&main_mutex);
	char msg[100];

	snprintf(msg, 100, "posix_memalign %lu %lu\n", alignment, size);
	write(STDERR_FILENO, msg, strlen(msg));
	
	if(last_chunk != NULL)
	{
		debbug_chunk_write(last_chunk);
		last_chunk = NULL;
	}
	pthread_mutex_unlock(&main_mutex);
#endif

	if(size == 0)
	{
		*memptr = NULL;
		return 0;
	}
	if(!powerof2(alignment))
		return EINVAL;

	if(alignment <= sizeof(size_t))//Takie wyrowananie jest zapewniane przez malloca
	{
		void* ptr = malloc(fix_size(size));
		if(ptr == NULL)
			return ENOMEM;

		*memptr = ptr;

		return 0;
	}

	size_t fixed_size = fix_size(size + BLOCK_META_SIZE + alignment);
	void* ptr = malloc(fixed_size);//alokujemy wiecej niz potrzebujemy, nadmiar zwrocimy

	if(ptr == NULL)
		return ENOMEM;

	pthread_mutex_lock(&main_mutex);

	mem_block_t* block = block_meta_address_from_data(ptr);

	fixed_size = -block->mb_size;//moze okazac sie, ze malloc dal nam wiecej, niz chcielismy
	size_t diff = (alignment - (int64_t)ptr%alignment) % alignment;//o ile musimy przesunac blok, aby jego adres byl wyrownany do alignment
	size_t new_size = fixed_size - diff;

#if MALLOC_DEBUG == 1
	if(last_chunk != NULL)
		debbug_chunk_write(last_chunk);
	last_chunk = find_containging_chunk(block);
#endif

	mem_block_t* prev_block = prev_block_address(block);
	if(prev_block->mb_size > 0)
	{//poprzedni blok jest poprawny i wolny, wiec mozemy mu "podrzucic" troche dodatkowej pamieci
		block = move_block(block, diff);
		set_block_size(block, -new_size);
		block->data = block_data_address(block);

		set_block_size(prev_block, prev_block->mb_size + diff);

		pthread_mutex_unlock(&main_mutex);

		*memptr = block_data_address(block);
		return 0;
	}
	else
	{//musimy stworzyc nowy blok, aby pozbyc sie nadmiaru zaalokowanej pamieci
		while(diff < BLOCK_META_SIZE)//potrzebujemy miec miejsce na metadane
		{//alokowalismy pamiec w taki sposob, aby miec pewnosc, ze w tym mies=jscu je nam nie zabraknie
			diff += alignment;
			new_size -= alignment;
		}

		block = move_block(block, diff);
		set_block_size(block, -new_size);

		create_new_block(block_meta_address_from_data(ptr), diff - BLOCK_META_SIZE, 1);

		pthread_mutex_unlock(&main_mutex);

		*memptr = block_data_address(block);
		return 0;
	} 


}






void* malloc (size_t) __attribute__ ((alias("my_malloc")));
void* calloc (size_t, size_t) __attribute__ ((alias("my_calloc")));
void* realloc (void*, size_t) __attribute__ ((alias("my_realloc")));
void free (void*) __attribute__ ((alias("my_free")));
int posix_memalign (void**, size_t, size_t) __attribute__ ((alias("my_posix_memalign")));