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

#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a > b ? a : b)

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

#define SIZE_MASK ~0x7

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & SIZE_MASK)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WORD_SIZE 4
#define DOUBLE_WORD_SIZE (2 * WORD_SIZE)
#define MIN_BLOCK_SIZE (2 * DOUBLE_WORD_SIZE)

// pointer to first block in heap(after prologue block)
static void* heap_start;

/*
free block list array
0:1 1:2-3 2:4-7 3:8-15 4:16-31 5:32-63 6:64-127 7:128-255
8:256-511 9:512-1023 10:1024-2047 11:2047-4095 12:4096-...
*/
#define FREE_BLOCK_LIST_ARRAY_SIZE 13
static void* free_block_list_array[FREE_BLOCK_LIST_ARRAY_SIZE];

static inline void* get_next_block_address(void* address);

static inline unsigned int log2int(unsigned int n)
{
    int result = 0;
    if (n == 0)
    {
        return result;
    }
    if ((n & 0xFFFF0000) != 0)
    {
        result += 16;
        n >>= 16;
    }
    if ((n & 0xFF00) != 0)
    {
        result += 8;
        n >>= 8;
    }
    if ((n & 0xF0) != 0)
    {
        result += 4;
        n >>= 4;
    }
    if ((n & 0xC) != 0)
    {
        result += 2;
        n >>= 2;
    }
    if ((n & 0x2) != 0)
    {
        result += 1;
        n >>= 1;
    }
    return result;
}

static inline size_t get_free_block_list_array_index(size_t size)
{
    size_t result = log2int(size);
    if (result >= FREE_BLOCK_LIST_ARRAY_SIZE)
    {
        result = FREE_BLOCK_LIST_ARRAY_SIZE - 1;
    }
    return result;
}

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

static inline void* get_epilogue_block_address()
{
    return mem_heap_hi() + 1;
}

static inline void* set_epilogue_block(int is_previous_allocated)
{
    is_previous_allocated = (is_previous_allocated == 0 ? 0x0 : 0x2);
    put_word(get_header_address(get_epilogue_block_address()), is_previous_allocated | 0x1);
    return get_epilogue_block_address();
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
size: aligned with DOUBLE_WORD_SIZE
address: point to head of payload of a block
*/
static inline void set_allocated_block_size(void* address, size_t size)
{
    unsigned int word = (get_header(address) & 0x7) | size;
    put_word(get_header_address(address), word);
}

/*
address: point to head of payload of a block
*/
static inline size_t get_block_size(void* address)
{
    return get_header(address) & (SIZE_MASK);
}

static inline void set_previous_allocated_bit(void* address)
{
    unsigned int word = get_header(address) | 0x2;
    put_word(get_header_address(address), word);
}

static inline void clear_previous_allocated_bit(void* address)
{
    unsigned int word = get_header(address) & (~0x2);
    put_word(get_header_address(address), word);
}

static inline void allocate_block(void* address)
{
    unsigned int word = get_header(address) | 0x1;
    put_word(get_header_address(address), word);
    // set allocated bit in next block's header
    set_previous_allocated_bit(get_next_block_address(address));
}

static inline void deallocate_block(void* address)
{
    unsigned int word = get_header(address) & (~0x1);
    put_word(get_header_address(address), word);
    put_word(get_footer_address(address), word);
    // clear alloacted bit in next block's header to 0
    clear_previous_allocated_bit(get_next_block_address(address));
}

static inline void* get_next_block_address(void* address)
{
    return (void*)((char*)address + get_block_size(address));
}

static inline void* get_previous_block_address(void* address)
{
    return (void*)((char*)address - (get_word(get_word_address(address, -2)) & (SIZE_MASK)));
}

static inline void* get_previous_free_block_address(void* address)
{
    return (void*)get_word(get_word_address(address, 0));
}

static inline void* get_next_free_block_address(void* address)
{
    return (void*)get_word(get_word_address(address, 1));
}

static inline void set_free_block_previous_address(void* address, void* previous_address)
{
    put_word(get_word_address(address, 0), (unsigned int)previous_address);
}

static inline void set_free_block_next_address(void* address, void* next_address)
{
    put_word(get_word_address(address, 1), (unsigned int)next_address);
}

static void remove_free_block(void* address)
{
    void* previous = get_previous_free_block_address(address),
        * next = get_next_free_block_address(address);
    if (previous != NULL)
    {
        set_free_block_next_address(previous, next);
    }
    if (next != NULL)
    {
        set_free_block_previous_address(next, previous);
    }
    // if it is head of free block list, then set head ptr to next
    if (previous == NULL)
    {
        free_block_list_array[get_free_block_list_array_index(get_block_size(address))] = next;
    }
}

/*
function: insert free block between two block, guarantee correct behaviour when previous is head
*/
static void insert_free_block_between(void* address, void* previous, void* next)
{
    // set previous and next free block
    if (previous != NULL)
    {
        set_free_block_next_address(previous, address);
    }
    if (next != NULL)
    {
        set_free_block_previous_address(next, address);
    }
    set_free_block_previous_address(address, previous);
    set_free_block_next_address(address, next);
    // if it is head of free block list, then set head ptr to next
    if (previous == NULL)
    {
        free_block_list_array[get_free_block_list_array_index(get_block_size(address))] = address;
    }
}

// insert in order of address
static void insert_free_block(void* address)
{
    void* ptr1 = NULL,
        * ptr2 = free_block_list_array[get_free_block_list_array_index(get_block_size(address))];
    if (ptr2 != NULL)
    {
        for (;ptr2 != NULL && ptr2 < address;
            ptr1 = ptr2, ptr2 = get_next_free_block_address(ptr2));
    }
    insert_free_block_between(address, ptr1, ptr2);
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
static inline int is_previous_allocated(void* address)
{
    return (get_header(address) & 0x2) != 0;
}

/*
address: point to head of payload of a block
*/
static inline int is_epilogue_block(void* address)
{
    return get_block_size(address) == 0;
}

/*
size: aligned with DOUBLE_WORD_SIZE
start_address: point to head of payload of a block
*/
static void* find_fit_block(size_t size)
{
    for (size_t index = get_free_block_list_array_index(size);
        index != FREE_BLOCK_LIST_ARRAY_SIZE; ++index)
    {

        for (void* address = free_block_list_array[index];address != NULL;
            address = get_next_free_block_address(address))
        {
            if (get_block_size(address) >= size)
            {
                return address;
            }
        }
    }
    return get_epilogue_block_address();
}

static void print_block_list()
{
    for (void* ptr = heap_start; !is_epilogue_block(ptr); ptr = get_next_block_address(ptr))
    {
        printf("address: %p\tsize:%d\tallocated:%d\n", ptr, get_block_size(ptr), is_allocated(ptr));
    }
    void* ptr = get_epilogue_block_address();
    printf("address: %p\tsize:%d\tallocated:%d\n", ptr, get_block_size(ptr), is_allocated(ptr));
}

static void print_free_block_list(size_t index)
{
    printf("index[%d]:", index);
    for (void* address = free_block_list_array[index]; address != NULL; address = get_next_free_block_address(address))
    {
        printf("[%p](%d)->", address, get_block_size(address));
    }
    printf("NULL\n");
}

static void print_free_block_list_array()
{
    printf("free block list:\n");
    for (size_t i = 0; i != FREE_BLOCK_LIST_ARRAY_SIZE; ++i)
    {
        print_free_block_list(i);
    }
}

static void print_status()
{
    printf("----------\n");
    printf("heap start:%p\n", heap_start);
    print_block_list();
    //print_free_block_list_array();
    printf("----------\n");
    fflush(stdout);
}

static void compare_block_content(void* lhs, void* rhs, size_t size)
{
    for (size_t offset = 0; offset != size; offset += WORD_SIZE)
    {
        if (get_word(lhs + offset) != get_word(rhs + offset))
        {
            printf("first different index: %d\n", offset / WORD_SIZE);
            printf("lhs: %x\t rhs: %x\n", get_word(lhs + offset), get_word(rhs + offset));
            return;
        }
    }
    printf("consistent content\n");
}

static int is_every_free_block_in_correct_free_list()
{
    int result = 1;
    for (void* ptr = heap_start; !is_epilogue_block(ptr); ptr = get_next_block_address(ptr))
    {
        if (is_allocated(ptr))
        {
            continue;
        }
        int found = 0;
        for (void* free_block_ptr = free_block_list_array[get_free_block_list_array_index(get_block_size(ptr))];
            free_block_ptr != NULL; free_block_ptr = get_next_free_block_address(free_block_ptr))
        {
            if (free_block_ptr == ptr)
            {
                found = 1;
                break;
            }
        }
        if (!found)
        {
            result = 0;
            printf("Didn't found %p[free] in free block list array[%d]\n", ptr, get_free_block_list_array_index(get_block_size(ptr)));
        }
    }
    return result;
}

static int is_every_block_in_free_block_list_is_marked_as_free()
{
    int result = 1;
    for (size_t index = 0; index != FREE_BLOCK_LIST_ARRAY_SIZE; ++index)
    {
        for (void* ptr = free_block_list_array[index]; ptr != NULL;
            ptr = get_next_free_block_address(ptr))
        {
            if (is_allocated(ptr))
            {
                result = 0;
                printf("%p[allocated] is found in free block list\n", ptr);
            }
        }
    }
    return result;
}

// return nonzero if and only if pass the check
static int mm_check(void)
{
    int result = 1;
    result = MIN(result, is_every_free_block_in_correct_free_list());
    result = MIN(result, is_every_block_in_free_block_list_is_marked_as_free());

    return result;
}

// /*
// function: clear fragment
// return: pointer to first free block
// */
// static void* clear_fragment()
// {
// #ifdef DEBUG_MODE
//     printf("before clear fragment\n");
//     print_status();
// #endif

//     void* available_start = heap_start,
//         * current_allocated_block_address = heap_start,
//         * next_allocated_block_address;
//     // find first allocated block
//     while (!is_allocated(current_allocated_block_address))
//     {
//         current_allocated_block_address = get_next_block_address(current_allocated_block_address);
//     }
//     for (next_allocated_block_address = get_next_allocated_block_address(current_allocated_block_address);
//         !is_epilogue_block(current_allocated_block_address);
//         available_start = get_next_block_address(available_start),
//         current_allocated_block_address = next_allocated_block_address,
//         next_allocated_block_address = get_next_allocated_block_address(next_allocated_block_address))
//     {
//         memmove(get_header_address(available_start),
//             get_header_address(current_allocated_block_address),
//             get_block_size(current_allocated_block_address));
//     }
//     if (!is_epilogue_block(available_start))
//     {
//         set_block_size(available_start, current_allocated_block_address - available_start);
//         deallocate_block(available_start);
//     }

// #ifdef DEBUG_MODE
//     printf("after clear fragment\n");
//     print_status();
// #endif

//     return available_start;
// }

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    for (size_t i = 0; i != FREE_BLOCK_LIST_ARRAY_SIZE; ++i)
    {
        free_block_list_array[i] = NULL;
    }
    void* address = mem_sbrk(WORD_SIZE * 4);
    if (address == NULL)
    {
        return -1;
    }
    if (address == (void*)ALIGN((int)address))
    {
        address = get_word_address(address, 2);
    }
    else
    {
        address = get_word_address(address, 1);
    }
    // set prologue block
    set_block_size(address, DOUBLE_WORD_SIZE);
    allocate_block(address);
    set_epilogue_block(1);
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

    // plus header, then align
    int newsize = ALIGN(size + WORD_SIZE);
    void* address = find_fit_block(newsize);
    // no fit block
    if (is_epilogue_block(address))
    {
        //address = clear_fragment();
        // if last block is free block
        if (!is_previous_allocated(address))
        {
            address = get_previous_block_address(address);
            remove_free_block(address);
        }
        long int exceed_size = newsize - get_block_size(address);
        if (mem_sbrk(exceed_size) == NULL)
        {
            return address;
        }
        // allocate block
        set_allocated_block_size(address, newsize);
        allocate_block(address);
        set_epilogue_block(1);
    }
    // find fit block
    // && have enough space to generate a new block
    else if (get_block_size(address) - newsize >= MIN_BLOCK_SIZE)
    {
        size_t left_size = get_block_size(address) - newsize;
        // allocate block
        void* previous = get_previous_free_block_address(address),
            * next = get_next_free_block_address(address);
        size_t old_size = get_block_size(address);
        remove_free_block(address);
        set_allocated_block_size(address, newsize);
        allocate_block(address);
        // generate a new free block
        void* new_block_address = get_next_block_address(address);
        set_block_size(new_block_address, left_size);
        deallocate_block(new_block_address);
        // if can't be in same list after split, then remove from origin list
        if (get_free_block_list_array_index(old_size)
            != get_free_block_list_array_index(left_size))
        {
            insert_free_block(new_block_address);
        }
        else
        {
            insert_free_block_between(new_block_address, previous, next);
        }
    }
    else
    {
        remove_free_block(address);
        allocate_block(address);
    }
#ifdef DEBUG_MODE
    printf("after allocate: \n");
    print_status();
    mm_check();
#endif
    return address;
}

void* mm_malloc_with_source(size_t size, void* source)
{
#ifdef DEBUG_MODE
    printf("before allocate for size: %d\n", size);
    print_status();
#endif

    // plus header, then align
    int newsize = ALIGN(size + WORD_SIZE);
    void* address = find_fit_block(newsize);
    // no fit block
    if (is_epilogue_block(address))
    {
        //address = clear_fragment();
        // if last block is free block
        if (!is_previous_allocated(address))
        {
            address = get_previous_block_address(address);
            remove_free_block(address);
        }
        long int exceed_size = newsize - get_block_size(address);
        if (mem_sbrk(exceed_size) == NULL)
        {
            return address;
        }
        // allocate block
        set_allocated_block_size(address, newsize);
        allocate_block(address);
        memmove(address, source, size);
        set_epilogue_block(1);
    }
    // find fit block
    // && have enough space to generate a new block
    else if (get_block_size(address) - newsize >= MIN_BLOCK_SIZE)
    {
        size_t left_size = get_block_size(address) - newsize;
        // allocate block
        void* previous = get_previous_free_block_address(address),
            * next = get_next_free_block_address(address);
        size_t old_size = get_block_size(address);
        remove_free_block(address);
        memmove(address, source, size);
        set_allocated_block_size(address, newsize);
        allocate_block(address);
        // generate a new free block
        void* new_block_address = get_next_block_address(address);
        set_block_size(new_block_address, left_size);
        deallocate_block(new_block_address);
        // if can't be in same list after split, then remove from origin list
        if (get_free_block_list_array_index(old_size)
            != get_free_block_list_array_index(left_size))
        {
            insert_free_block(new_block_address);
        }
        else
        {
            insert_free_block_between(new_block_address, previous, next);
        }
    }
    else
    {
        remove_free_block(address);
        allocate_block(address);
        memmove(address, source, size);
    }
#ifdef DEBUG_MODE
    printf("after allocate: \n");
    print_status();
    mm_check();
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
        * tail_address = get_next_block_address(ptr) - WORD_SIZE,
        * next_block_address = get_next_block_address(ptr);
    if (!is_previous_allocated(ptr))
    {
        head_address = get_header_address(get_previous_block_address(ptr));
        remove_free_block(get_previous_block_address(ptr));
    }
    if (!is_allocated(next_block_address))
    {
        tail_address = get_footer_address(next_block_address) + WORD_SIZE;
        remove_free_block(get_next_block_address(ptr));
    }
    void* new_block_address = head_address + WORD_SIZE;
    set_block_size(new_block_address, tail_address - head_address);
    deallocate_block(new_block_address);
    insert_free_block(new_block_address);

#ifdef DEBUG_MODE
    printf("after free %p\n", ptr);
    print_status();
    if (!mm_check())
    {
        exit(0);
    }
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
    size_t compare_size = MIN(get_block_size(ptr) - WORD_SIZE, size);
    unsigned int* old_data_copy = malloc(compare_size);
    memcpy(old_data_copy, ptr, compare_size);
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
        // first and second word might be replaced by previous and next pointer in free block
        // last word might be replaced by footer
        // save first, second, last word in local variable
        size_t data_size = MIN(get_block_size(ptr) - WORD_SIZE, size) - WORD_SIZE;
        unsigned int first_word = get_word(get_word_address(ptr, 0)),
            second_word = get_word(get_word_address(ptr, 1)),
            last_word = get_word(ptr + data_size);
        mm_free(ptr);
        result = mm_malloc_with_source(size, ptr);
        put_word(get_word_address(result, 0), first_word);
        put_word(get_word_address(result, 1), second_word);
        put_word(result + data_size, last_word);
    }

#ifdef DEBUG_MODE
    printf("after reallocate:%p\tnew size:%d\n", ptr, size);
    print_status();
    mm_check();
    compare_block_content(old_data_copy, result, compare_size);
    free(old_data_copy);
#endif

    return result;
}














