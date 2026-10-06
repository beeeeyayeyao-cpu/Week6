/*
  mm-naive.c - The fastest, least memory-efficient malloc package.
 
  In this naive approach, a block is allocated by simply incrementing
  the brk pointer.  A block is pure payload. There are no headers or
  footers.  Blocks are never coalesced or reused. Realloc is
  implemented directly using mm_malloc and mm_free.
 
  NOTE TO STUDENTS: Replace this header comment with your own header
  comment that gives a high level description of your solution.
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
    "ateam",
    /* First member's full name */
    "choi dong ho",
    /* First member's email address */
    "choi1234@gmail.com",
    /* Second member's full name (leave blank if none) */
    "kim meong jun",
    /* Second member's email address (leave blank if none) */
    "kim1234@gmail.com"};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))
void* heap_listp;
void* free_list;

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
static void remove_block(void* bp);
static void insert_block(void* bp);

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void*)-1) return -1;

    PUT(heap_listp, 0);
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (3*WSIZE), PACK(0,1));
    heap_listp += (2*WSIZE);
    free_list = NULL;

    if (extend_heap(CHUNKSIZE/WSIZE)==NULL) return -1;                        
    return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
static void *find_fit(size_t asize)
{
    void *bp;

    for (bp = free_list; bp != NULL; bp = GET_NEX(bp)) {
        if (asize <= GET_SIZE(HDRP(bp))) {
            return bp;
        }
    }

    return NULL;
}

void place(void * bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));
 
    remove_block(bp);

    if ((csize - asize) >= MIN_BLOCK_SIZE) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize, 0));
        PUT(FTRP(bp), PACK(csize-asize, 0));
        insert_block(bp);
    }
    else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char *bp;

    if (size ==0) return NULL;

    if (size <= DSIZE) asize = 2*DSIZE;
    else asize = DSIZE * ((size + (DSIZE) + (DSIZE-1))/DSIZE); //헤더랑 푸터가 있어야 되기 때문

    if ((bp = find_fit(asize)) != NULL)
    {
        place(bp, asize);
        return bp;
    }

    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL) return NULL;
    place(bp,asize);
    return bp;
}

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1) return NULL;

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1));

    return coalesce(bp);
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
}

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    /* Case 1: 앞뒤 모두 할당되어 병합 없음 -> 그대로 리스트에 삽입 */
    if (prev_alloc && next_alloc) {
        insert_block(bp);
        return bp;
    }

    /* Case 2: 뒤 블록만 가용 블록 -> 뒤 블록을 리스트에서 빼고 합침 */
    else if (prev_alloc && !next_alloc) {
        remove_block(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    /* Case 3: 앞 블록만 가용 블록 -> 앞 블록을 리스트에서 빼고 합침 */
    else if (!prev_alloc && next_alloc) {
        remove_block(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    /* Case 4: 앞뒤 모두 가용 블록 -> 둘 다 리스트에서 빼고 합침 */
    else {
        remove_block(PREV_BLKP(bp));
        remove_block(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    /* 합쳐진 새 덩어리를 free_list 맨 앞에 삽입 */
    insert_block(bp);
    return bp;
}
void remove_block(void * bp)
{
    void * pre = GET_PRE(bp);
    void * nex = GET_NEX(bp);
    if (pre == NULL) 
    {
        free_list = nex;
    }
    else
    {
        PUT_PTR(NEX_PTR(pre), nex);
    }
    if (nex != NULL)
    {
        PUT_PTR(PRE_PTR(nex), pre);
    }
}

void insert_block(void * bp)
{
    /*
    void * firstL = free_list;
    free_list = bp;
    PUT_PTR(GET_PRE(bp), NULL);
    PUT_PTR(GET_NEX(bp), firstL);
    */

    PUT_PTR(NEX_PTR(bp), free_list);
    PUT_PTR(PRE_PTR(bp), NULL);

    if (free_list != NULL) {
        PUT_PTR(PRE_PTR(free_list), bp);
    }

    free_list = bp;
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    newptr = mm_malloc(size);
    if (newptr == NULL)
        return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
        copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}