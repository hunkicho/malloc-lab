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
    "team_01",
    /* First member's full name */
    "Jiyoung Jang",
    /* First member's email address */
    "98jjyoung@naver.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* **************BASIC CONSTANTS AND MACROS******************/

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8
#define WSIZE 4         /* word and header/footer size (bytes) */
#define DSIZE 8         /* Double word size (bytes) */
#define CHUNKSIZE 1<<12 /* Extend heap by this amount(bytes) */

#define MAX(x, y) ((x) > (y)? (x) : (y))

/* Pack a size and allocation defining bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* macros to use void pointer P */
#define GET(p)  (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/* seperate "size" and "allocation bit" from val in address p */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p)    (GET(p) & 0x1)

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* Give block ptr bp, compute address of it's header and footer */
#define HDRP(bp)    ((char *)(bp) - WSIZE)
#define FTRP(bp)    ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Give block ptr bp, get address of next & previous blk ptr */
#define NEXT_BLKP(bp)   ((char *)(bp) + GET_SIZE((char *)(bp) - WSIZE))
#define PREV_BLKP(bp)   ((char *)(bp) - GET_SIZE((char *)(bp) - DSIZE))

/*************************************************************/

/* private global variables */
static char *heap_listp;        // ptr that always points to prologue blk ftr
char *search_bp;                // ptr for next fit 

/* function declaration */
static void *extend_heap(size_t);
static void *coalesce(void *);
static void *find_fit(size_t);
int place(void *, size_t);



/* 
 * mm_init              RETURN : success 0 / fail -1
 * heap initialization 
 * heap_listp : ptr addressed to prologue blk     
 * set prologue & epilogue blks 
 * after init empty heap, then calls extend_heap() 
 */
int mm_init(void) {
    /* initialize heap_listp ptr brk ptr & Failure Check */
    /* heap_listp is now at start of heap */
    /* alignment padding+prologue(hdr & ftr)+epilogue(hdr) = 4*WSIZE */
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1) {
        return -1;
    } 

    /* init for NEXT FIT allocator */
    search_bp = heap_listp;

    /* set additional blks for convenience */
    PUT(heap_listp, 0);                         // Alignment padding
    PUT(heap_listp + WSIZE, PACK(DSIZE, 1));    // Prologue Header
    PUT(heap_listp + 2*WSIZE, PACK(DSIZE, 1));  // Prologue Footer
    PUT(heap_listp + 3*WSIZE, PACK(DSIZE, 1));  // Epilogue Header

    /* heap_listp will always point to prologue ftr */
    heap_listp += 2*WSIZE;

    /* Extend empty heap with free blk & Fail Check */
    if(extend_heap(CHUNKSIZE) == NULL) {
        return -1;
    }

    return 0;
}


/*
 * extend_heap          RETURN : new free blk bp / fail -1
 * gets size of heap block and rounds it up to match alignment plan
 * sets brk ptr to end of extend heap 
 * sets blk ptr(bp) to start of newly extended heap
 * initializes new free blk hdr/ftr
 * sends new free blk to coalesce() for merging 
 * 
 */
static void *extend_heap(size_t size) {
    char *bp;                       // block ptr for new free blk
    size_t newSize = ALIGN(size);   //rounded up to match our alignment plan

    /* set brk ptr & bp */
    if((long)(bp = mem_sbrk(newSize)) == -1) {
        return -1;
    } 

    /* initialize new free blk */
    PUT(HDRP(bp), PACK(newSize, 0));        // Free blk header
    PUT(FTRP(bp), PACK(newSize, 0));        // Free blk footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));   // New epilogue Header

    /* Send to Coalesce */
    return coalesce(bp);
}


/*
 * mm_free          RETURN : void 
 * failure check is NULL pointer? 
 * failure check is allocated blk?
 * gets blk ptr and frees block. 
 * sends free blk ptr to coalesce to merge adjacent blks.
 * 
 */
void mm_free(void *bp) {
    /* Failure Checking */
    assert(bp != NULL);            // is not NULL pointer?
    assert(GET_ALLOC(HDRP(bp)));    // is allocated blk?

    size_t size = GET_SIZE(HDRP(bp));

    /* adjust header & footer */
    PUT(HDRP(bp), PACK(size, 0));   
    PUT(FTRP(bp), PACK(size, 0));   

    /* send new free blk ptr to coalesce */
    coalesce(bp);
}


/*
 * coalesce         RETURN : new free blk ptr
 * failure check : is free blk?
 * gets blk ptr of a free blk 
 * scans adjacent blks (using footer, time complexity O(1))
 * if prev or behind is free, then merge 
 * coalesce never gets a NULL pointer, since it's called by free() and extend_heap()
 * 
 */
static void *coalesce(void *bp) {
    assert(bp != NULL);                 // is not NULL ptr?
    assert(!GET_ALLOC(HDRP(bp)));       // is free block?

    /* get alloc info of adjacent blocks */
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));

    size_t size = GET_SIZE(HDRP(bp));       // size of current free blk

    /* coalesce by case */
    if (prev_alloc && next_alloc) {         // prev & next allocated
        return bp;

    } else if (prev_alloc && !next_alloc) { // only next is free
        /* merging next blk & current blk */
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));

        PUT(HDRP(bp), PACK(size, 0));       // use header of current blk
        PUT(FTRP(bp), PACK(size, 0));

    } else if (!prev_alloc && next_alloc) { // only prev is free
        /* merging prev blk & current blk */
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));

        PUT(FTRP(bp), PACK(size, 0));       // use footer of current blk
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);

    } else {                                // prev & next is free
        /* merging prev blk & current blk & next blk */
        size += GET_SIZE(HDRP(NEXT_BLKP(bp))) + GET_SIZE(HDRP(PREV_BLKP(bp)));

        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);

    } 
    return bp;
}



/* 
 * mm_malloc            RETURN : bp for new allocated blk / fail NULL
 * gets size byte 
 * fail check : is size a number?
 * arrange size to a multiple of the alignment. (= newSize)
 * newSize must have room for header and footer. 
 * Allocate a block by incrementing the brk pointer.
 * 
 * search free list for free blks that are big enough. 
 * call place() to (possibly)split and place blk
 * returns block pointer of newly allocated blk
 * 
 * if no possible free blks, then call extend_heap()
 * call place() to place blk on new heap
 * then return
 */
void *mm_malloc(size_t size) {
    /* adjust block size to include header&footer + 2WORD requirment */
    size_t newSize = ALIGN(size) + DSIZE;   
    size_t extendSize;          // amount to extend heap if no fit
    char *bp;                   // block pointer to return 

    /* Fail check */
    if (size == 0) {        
        return NULL;
    }

    /* Search free list for a fit */
    if ((bp = find_fit(newSize)) != NULL) {
        place(bp, newSize);
        return bp;
    }

    /* there is no fit */
    extendSize = MAX(newSize, CHUNKSIZE);
    /* send bp to new free blk & fail check */
    if ((bp = extend_heap(extendSize)) == NULL) {   
        return NULL;
    }
    place(bp, newSize);
    return bp;
}




/* 
 * mm_realloc           RETURN : newly allocated bp / fail NULL
 * gets old ptr and new size 
 * malloc with new size & fail check
 * memcpy(dest, source, copy size(byte)) 
 * if old size > new size : cut 
 * free old ptr
 */
void *mm_realloc(void *ptr, size_t size) {
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL) {
        return NULL;
    }
    copySize = GET_SIZE(HDRP(oldptr)) - DSIZE;
    if (ALIGN(size) < copySize) {
        copySize = ALIGN(size);
    }

    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}







/* 
 * find_fit        RETURN : block pointer of possible free blk / fail NULL
 * gets size, search heap list for free blk bigger than size  
 * 
 * FIRST FIT : 
 * returns first free blk that is possible
 * searches from the start of list
 * 
 * NEXT FIT : 
 * returns first free blk that is possible
 * BUT searches from the tail of previous placement 
 * 
 * BEST FIT : 
 * searches whole list every time for smallest possible blk
 * 
 */
static void *find_fit(size_t size) {
    void *bp;
    
    /************** FIRST FIT Score : 58****************/
    // for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
    //     if (!GET_ALLOC(HDRP(bp)) && (size <= GET_SIZE(HDRP(bp)))) {
    //         return bp;
    //     }
    // }
    // return NULL;    // no fit 

    /************** NEXT FIT Segmentation fault*****************/
    // for (search_bp; GET_SIZE(HDRP(search_bp)) > 0; 
    // search_bp = NEXT_BLKP(search_bp)) {
    //     if (!GET_ALLOC(HDRP(search_bp)) && (size <= GET_SIZE(HDRP(search_bp)))) {
    //         return search_bp;
    //     }
    // }
    // return NULL;    // no fit, search_bp is at epilogue header

    /************** BEST FIT score : 57*****************/
    size_t min = 4294967295;        // TODO : 추후 수정할것 
    void *temp = NULL;              // return NULL if no fit
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && (size <= GET_SIZE(HDRP(bp))) 
        && (min > GET_SIZE(HDRP(bp)))) {
            min = GET_SIZE(HDRP(bp));
            temp = bp;
        }
    }
    return temp;    
}


/* TODO
 * place                RETURN : success 0 / fail -1
 * fail check : is bp not NULL?
 * fail check : is size < fsize
 * fail check : is bp free?
 * 
 * header & footer alloc
 */
int place(void *bp, size_t size) {
    size_t fsize = GET_SIZE(HDRP(bp));

    /* fail checking */
    assert(bp != NULL);
    assert(size <= fsize);
    assert(!GET_ALLOC(HDRP(bp)));

    if (size == fsize) {
        PUT(HDRP(bp), PACK(size, 1));
        PUT(FTRP(bp), PACK(size, 1));
        return 0;
    } else {    // fsize is always bigger than size 
        PUT(FTRP(bp), PACK(fsize - size, 0));           // change free footer 
        PUT(bp + size - WSIZE, PACK(fsize - size, 0));  // set free header
        PUT(HDRP(bp), PACK(size, 1));                   // change alloc header
        PUT(FTRP(bp), PACK(size, 1));                   // set alloc footer
        return 0; 
    }

    return -1;
}















