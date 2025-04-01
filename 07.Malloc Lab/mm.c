/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 *
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

 /*********************************************************
  * NOTE TO STUDENTS: Before you do anything else, please
  * provide your team information in the following struct.
  ********************************************************/
team_t team = {
    /* Team name */
    "jayjay team",
    /* First member's full name */
    "jay",
    /* First member's email address */
    "1974617416@qq.com",
    /* Second member's full name (leave blank if none) */
    "jay",
    /* Second member's email address (leave blank if none) */
    "it's also me 🤣"
};

//#define DEBUG_MODE

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

#define SIZE_MASK ~0x7

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & SIZE_MASK)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WORD_SIZE 4
#define DOUBLE_WORD_SIZE 8

// pointer to first block in heap(after prologue block)
static void* heap_start;

static inline void* get_next_block_address(void* address);

static inline void put_word(void* address, unsigned int word)
{
    *(unsigned int*)address = word;
}

static inline unsigned int get_word(void* address)
{
    return *(unsigned int*)address;
}

// return [index] of word address starts from [address]
static inline void* get_word_address(void* address, int index)
{
    return (void*)((char*)address + (WORD_SIZE * index));
}

static inline void* get_header_address(void* address)
{
    return get_word_address(address, -1);
}

static inline void* get_footer_address(void* address)
{
    return get_next_block_address(address) - DOUBLE_WORD_SIZE;
}

static inline unsigned int get_header(void* address)
{
    return get_word(get_header_address(address));
}

static inline unsigned int get_footer(void* address)
{
    return get_word(get_footer_address(address));
}

/*
size: aligned with DOUBLE_WORD_SIZE
address: point to head of payload of a block
*/
static inline void set_block_size(void* address, size_t size)
{
    unsigned int word = (get_header(address) & 0x7) | size;
    put_word(get_header_address(address), word);
    put_word(get_footer_address(address), word);
}

/*
address: point to head of payload of a block
*/
static inline size_t get_block_size(void* address)
{
    return get_header(address) & (SIZE_MASK);
}

static inline void allocate_block(void* address)
{
    unsigned int word = get_header(address) | 0x1;
    put_word(get_header_address(address), word);
    put_word(get_footer_address(address), word);
}

static inline void deallocate_block(void* address)
{
    unsigned int word = get_header(address) & (~0x1);
    put_word(get_header_address(address), word);
    put_word(get_footer_address(address), word);
}

static inline void* get_next_block_address(void* address)
{
    return (void*)((char*)address + get_block_size(address));
}

static inline void* get_previous_block_address(void* address)
{
    return (void*)((char*)address - (get_word(get_word_address(address, -2)) & (SIZE_MASK)));
}

/*
address: point to head of payload of a block
*/
static inline int is_allocated(void* address)
{
    return get_header(address) & 0x1;
}

/*
address: point to head of payload of a block
*/
static inline int reach_epilogue_block(void* address)
{
    return get_block_size(address) == 0;
}

/*
size: aligned with DOUBLE_WORD_SIZE
start_address: point to head of payload of a block
*/
static void* find_first_fit_block(size_t size, void* start_address)
{
    void* ptr = start_address;
    // from start_address to epilogue block
    for (; !reach_epilogue_block(ptr); ptr = get_next_block_address(ptr))
    {
        if (get_block_size(ptr) >= size && !is_allocated(ptr))
        {
            return ptr;
        }
    }
    // from heap_start to start_address
    for (ptr = heap_start; ptr != start_address; ptr = get_next_block_address(ptr))
    {
        if (get_block_size(ptr) >= size && !is_allocated(ptr))
        {
            return ptr;
        }
    }
    // no fit return NULL
    return NULL;
}

static void print_block_list()
{
    printf("----------\n");
    for (void* ptr = heap_start; !reach_epilogue_block(ptr); ptr = get_next_block_address(ptr))
    {
        printf("address: %p\tsize:%d\tallocated:%d\n", ptr, get_block_size(ptr), is_allocated(ptr));
    }
    printf("----------\n");
}

static void print_status()
{
    printf("heap start:%p\n", heap_start);
    print_block_list();
}

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    void* address = mem_sbrk(WORD_SIZE * 4);
    if (address == NULL)
    {
        return -1;
    }
    if (address == (void*)ALIGN((int)address))
    {
        address = get_word_address(address, 2);
    }
    // set prologue block
    set_block_size(address, DOUBLE_WORD_SIZE);
    allocate_block(address);
    // set epilogue block
    put_word(get_header_address(get_next_block_address(address)), 0x1);
    // set heap_start to first block
    heap_start = get_next_block_address(address);
    return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void* mm_malloc(size_t size)
{
#ifdef DEBUG_MODE
    printf("before allocate for size: %d\n", size);
    print_status();
#endif

    // align and plus header, footer
    int newsize = ALIGN(size) + DOUBLE_WORD_SIZE;
    void* address = find_first_fit_block(newsize, heap_start);
    // no fit block, try to increment brk pointer
    if (address == NULL)
    {
        address = mem_sbrk(newsize);
        if (address == NULL)
        {
            return address;
        }
        // insert block
        set_block_size(address, newsize);
        allocate_block(address);
        // set epilogue block
        put_word(get_header_address(get_next_block_address(address)), 0x1);
    }
    // find fit block
    // && have enough space to generate a new block
    else if (get_block_size(address) - newsize >= 2 * DOUBLE_WORD_SIZE)
    {
        size_t left_size = get_block_size(address) - newsize;
        // set block
        set_block_size(address, newsize);
        allocate_block(address);
        // generate a new free block
        void* new_block_address = get_next_block_address(address);
        set_block_size(new_block_address, left_size);
        deallocate_block(new_block_address);
    }
    else
    {
        allocate_block(address);
    }
#ifdef DEBUG_MODE
    printf("after allocate: \n");
    print_status();
#endif
    return address;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void* ptr)
{
#ifdef DEBUG_MODE
    printf("before free %p\n", ptr);
    print_status();
#endif

    void* head_address = get_header_address(ptr),
        * tail_address = get_footer_address(ptr) + WORD_SIZE,
        * previous_block_address = get_previous_block_address(ptr),
        * next_block_address = get_next_block_address(ptr);
    if (!is_allocated(previous_block_address))
    {
        head_address = get_header_address(previous_block_address);
    }
    if (!is_allocated(next_block_address))
    {
        tail_address = get_footer_address(next_block_address) + WORD_SIZE;
    }
    void* new_block_address = head_address + WORD_SIZE;
    set_block_size(new_block_address, tail_address - head_address);
    deallocate_block(new_block_address);

#ifdef DEBUG_MODE
    printf("after free %p\n", ptr);
    print_status();
#endif
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void* mm_realloc(void* ptr, size_t size)
{
#ifdef DEBUG_MODE
    printf("before reallocate:%p\tnew size:%d\n", ptr, size);
    print_status();
#endif
    void* result = NULL;
    if (ptr == NULL)
    {
        result = mm_malloc(size);
    }
    else if (size == 0)
    {
        mm_free(ptr);
        result = NULL;
    }
    else
    {
        size_t newsize = ALIGN(size) + DOUBLE_WORD_SIZE;
        // find a new place
        if (newsize > get_block_size(ptr))
        {
            void* new_address = mm_malloc(newsize);
            memcpy(new_address, ptr, get_block_size(ptr));
            mm_free(ptr);
            result = new_address;
        }
        // shrink current block
        else if (get_block_size(ptr) - newsize >= 2 * DOUBLE_WORD_SIZE)
        {
            size_t left_size = get_block_size(ptr) - newsize;
            // set block
            set_block_size(ptr, newsize);
            allocate_block(ptr);
            // generate a new free block
            void* new_block_address = get_next_block_address(ptr);
            set_block_size(new_block_address, left_size);
            deallocate_block(new_block_address);
            result = ptr;
        }
        else
        {
            result = ptr;
        }
    }

#ifdef DEBUG_MODE
    printf("after reallocate:%p\tnew size:%d\n", ptr, size);
    print_status();
#endif

    return result;
}














