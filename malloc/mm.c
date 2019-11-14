/*
 * Simple, 32-bit and 64-bit clean allocator based on implicit free
 * lists, first-fit placement, and boundary tag coalescing, as described
 * in the CS:APP3e text. Blocks must be aligned to doubleword (8 byte)
 * boundaries. Minimum block size is 16 bytes.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "back_row_boiz",
    /* First member's full name */
    "Jack Bernstein",
    /* First member's email address */
    "john.bernstein@pomona.edu",
    /* Second member's full name */
    "Adam Lininger-White",
    /* Second member's email address */
    "adla2017@mymail.pomona.edu"
};

/* $begin mallocmacros */
/* Basic constants and macros */
#define WSIZE    sizeof(void *)     /* Word and header/footer size (bytes) */ //line:vm:mm:beginconst
#define DSIZE     (2 * WSIZE)       /* Double word size (bytes) */
#define OVERHEAD    24      /* Minimum block size (8 for header/footer + 16 for 2 pointer) */
#define CHUNKSIZE  (1<<12)  /* Extend heap by this amount (bytes) */  //line:vm:mm:endconst

#define MAX(x, y) ((x) > (y)? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc)) //line:vm:mm:pack

/* Read and write a word at address p */
#define GET(p)       (*(unsigned int *)(p))            //line:vm:mm:get
#define PUT(p, val)  (*(unsigned int *)(p) = (val))    //line:vm:mm:put

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7)                   //line:vm:mm:getsize
#define GET_ALLOC(p) (GET(p) & 0x1)                    //line:vm:mm:getalloc

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)       ((char *)(bp) - WSIZE)                      //line:vm:mm:hdrp
#define FTRP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) //line:vm:mm:ftrp

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE))) //line:vm:mm:nextblkp
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE))) //line:vm:mm:prevblkp

/* Given ptr in free list, get next and previous ptr in the list */
/* bp is address of the free block. Since minimum Block size is 16 bytes,
   we utilize to store the address of previous block pointer and next block pointer.
*/
#define FWD_PTR(bp)  (*(char **)(bp + WSIZE))
#define BACK_PTR(bp)  (*(char **)(bp))

/* Puts pointers in the next and previous elements of free list */
#define SET_FWD_PTR(bp, qp) (FWD_PTR(bp) = qp)
#define SET_BACK_PTR(bp, qp) (BACK_PTR(bp) = qp)

/* Global variables */
static char *heap_listp = 0;  /* Pointer to first block */
static char *flist_head = 0;
static char *test = 0;

static void *extend_heap(size_t words);
static void add_flist(void *bp);
static void place(void *bp, size_t size);

/*************************************************
*       CORE FUNCTIONS
*************************************************/
/*
 * mm_init - Initialize the memory manager
 */
/* $begin mminit */
int mm_init(void)
{
    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1) //line:vm:mm:begininit
        return -1;
    PUT(heap_listp, 0);                          /* Alignment padding */
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); /* Prologue header */
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); /* Prologue footer */
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));     /* Epilogue header */

    flist_head = heap_listp + (2*WSIZE);



    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;
}
/* $end mminit */

void *mm_malloc(size_t size) {

  size_t asize;      /* Adjusted block size */
  size_t extendsize; /* Amount to extend heap if no fit */
  void *bp;

  /* Ignore spurious requests. */
  if (size == 0)
    return (NULL);

    /* Adjust block size to include overhead and alignment reqs. */
  if (size <= DSIZE)                                          //line:vm:mm:sizeadjust1
      asize = 2*DSIZE;                                        //line:vm:mm:sizeadjust2
  else
      asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE); //line:vm:mm:sizeadjust3


    for (bp = flist_head; GET_ALLOC(HDRP(bp)) == 0; bp = FWD_PTR(bp) ){
      if (asize <= (size_t)GET_SIZE(HDRP(bp)) ) {
        place(bp, asize);
        return bp;
      }
    }

  /* No fit found. Get more memory and place the block */
   extendsize = MAX(asize,CHUNKSIZE);                 //line:vm:mm:growheap1
   if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
       return NULL;                        //line:vm:mm:growheap2
   place(bp, asize);                            //line:vm:mm:growheap3
   return bp;
}

void mm_free(void *bp) {
  size_t current = GET_SIZE(HDRP(bp));
  PUT(HDRP(bp), PACK(current, 0));
  PUT(FTRP(bp), PACK(current, 0));
  add_flist(bp);
}

void *mm_realloc(void *ptr, size_t size) {
  return ptr;
}

static void *extend_heap(size_t words) {

    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;

    size = MAX(size, OVERHEAD);

    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(NEXT_BLKP(bp), PACK(0,1));

    add_flist(bp);

    return bp;
}

static void add_flist(void *bp) {
      SET_FWD_PTR(bp, flist_head);
      SET_BACK_PTR(flist_head, bp);
      SET_BACK_PTR(bp, NULL);
      flist_head = bp;
}

static void remove_flist(void *bp) {

  if(BACK_PTR(bp)) {
    SET_FWD_PTR(BACK_PTR(bp), FWD_PTR(bp));
  }
  else
    flist_head = FWD_PTR(bp);
  SET_BACK_PTR(FWD_PTR(bp), BACK_PTR(bp));
}

static void place(void *bp, size_t size) {

  size_t current = GET_SIZE(HDRP(bp));
  PUT(HDRP(bp), PACK(current, 1));
  PUT(FTRP(bp), PACK(current, 1));
  remove_flist(bp);
}
