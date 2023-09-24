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
  * provide your information in the following struct.
  ********************************************************/
team_t team = {
    /* Your student ID */
    "20191626",
    /* Your full name*/
    "Yonguk Lee",
    /* Your email address */
    "dldyddnr11@sogang.ac.kr",
};
// Basic constants and macors
#define WSIZE       4           // Word and header/footer size(bytes)
#define DSIZE       8           // Double word size (btyes)
#define CHUNKSIZE   (1 << 12)   // Extend heap by this amount (bytes) : 초기 가용 블록과 힙 확장을 위한 기본 크기

#define MAX(x, y)   ((x) > (y) ? (x) : (y))    

#define PACK(size, alloc)   ((size) | (alloc))

#define GET(p)  (*(unsigned int *)(p))
#define PUT(p, val)  (*(unsigned int *)(p) = (val))

// Read the size and allocated field from address p
#define GET_SIZE(p)    (GET(p) & ~0x7)  // size of the header or footer
#define GET_ALLOC(p)   (GET(p) & 0x1) 

// position of current block
#define HDRP(bp)    ((char *)(bp) - WSIZE)
#define FTRP(bp)    ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

// prev and next block pointer
#define NEXT_BLKP(bp)   (((char *)(bp) + GET_SIZE((char *)(bp) - WSIZE)))    
#define PREV_BLKP(bp)   (((char *)(bp) - GET_SIZE((char *)(bp) - DSIZE)))    


/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

// Declaration
static char *heap_listp;
static char *temp_bp;
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    // Create the initial empty heap
    if ((heap_listp = mem_sbrk(4*WSIZE)) == NULL) {  
        return -1;
    }

    PUT(heap_listp, 0);                              /*Alignment padding*/
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));     /*Prologue header*/
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));     /*Prologue footer*/
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));         /*Epilogue header*/
    heap_listp += (2*WSIZE);

    // Extend the empty heap with a free block of CHUNKSIZE bytes
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL) {
        return -1;
    }
    //temp_bp = (char *)heap_listp;   
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 * Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size) {
    char *bp;
    size_t asize;       // adjusted block szie
    size_t e_size;         // Amount to extend heap if no fit
    

    if(heap_listp ==0)
    {
        mm_init();
    }
    // Ignore spurious requests
    if (size == 0) {
        return NULL;
    }

    
    if (size <= DSIZE) asize = 2*DSIZE;
    else asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);

    // Search the free list for a fit
    if ((bp = find_fit(asize))) {   // proper block search
        place(bp, asize);                   // fragment excess and new bp return
        return bp;
    }

    // fit is not found. 
    // Need more memory and place the block

    e_size = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(e_size/WSIZE)) == NULL)
        return NULL;
    
    place(bp, asize);
    return bp;
}


/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    if(bp ==0)
    {
        return;
    }

    if(heap_listp ==0)
    {
        mm_init();
    }
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));

    coalesce(bp);
}

/*
   realloc
*/
void *mm_realloc(void *bp, size_t size) {
    size_t oldsize;
    size_t newsize;   

    if(bp==NULL)
    {
        return mm_malloc(size);
    }

    if(heap_listp ==0)
    {
        mm_init();
    }

    
    oldsize = GET_SIZE(HDRP(bp));
    newsize = size + (2 * WSIZE);

    
    // if newsize is latger than oldsize, size changed
    if(newsize>oldsize)
    {
        size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
        size_t csize = oldsize + GET_SIZE(HDRP(NEXT_BLKP(bp)));

        // next block is available and old + next >= new size , then use it
        if (!next_alloc) {
            if(csize >= newsize){
                PUT(HDRP(bp), PACK(csize, 1));
                PUT(FTRP(bp), PACK(csize, 1));
                return bp;
            }
        }
        // or make new block and move it
        else {
            void *new_bp = mm_malloc(newsize);
            place(new_bp, newsize);
            memcpy(new_bp, bp, newsize);  
            mm_free(bp);
            return new_bp;
        }
    }
    else
    {
        return bp;
    }

    return NULL;
}

/*
  extend_heap
  -heap initialized
  -no proper fit 
*/
static void *extend_heap(size_t words) {
    char *bp;
    size_t size;

    // Allocate an even number of words  to maintain alignment
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE; 
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    // initialize free block header/footer and the epilogue header
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    // coalesce if the previous block was free
    return coalesce(bp);   
}

/*
   integrate if it is possible
*/
static void *coalesce(void *bp) {

    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    // case1: prev && next
    if (prev_alloc && next_alloc) {
        temp_bp = bp;
        return bp;
    }

    // case2: prev && !next
    else if (prev_alloc && !next_alloc) {
        size = size + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    // case3: !prev && next
    else if (!prev_alloc && next_alloc) {
        size = size + GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    // case4: !prev && !next
    else {
        size = size + GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    temp_bp = bp;
    return bp;
} 
//
//
/*
  capacity place
*/
static void place(void *bp, size_t asize) {
    size_t csize = GET_SIZE(HDRP(bp));

    if ((csize - asize) >= (2 * DSIZE)) {
        
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        
        bp = NEXT_BLKP(bp);
        // header , footer for remainder
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
    }
    else {
        // use total blocks if csize - asize is less than 16bytes
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }

}



/* 
   By using next fit, find proper block

*/
static void *find_fit(size_t asize) {
    char *bp;

    bp = temp_bp;
    
    for (bp = NEXT_BLKP(bp); GET_SIZE(HDRP(bp)) != 0; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp))) {
            if(GET_SIZE(HDRP(bp)) > asize)
            {
                return bp;
            }
        }
    }

    bp = heap_listp;

    for(;bp<temp_bp;)
    {
        bp = NEXT_BLKP(bp);
        // = 유무에 따라 성능 차이
        if (!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= asize) {
            return bp;
        }
    }

    return NULL;
}