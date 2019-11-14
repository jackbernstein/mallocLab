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
#include <stdbool.h>

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



/*********************************************************
 * MACROS & CONSTANTS (from book)
 ********************************************************/

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE       4       /* Word and header/footer size (bytes) */
#define DSIZE       8       /* Double word size (bytes) */
#define CHUNKSIZE  (1<<12)  /* Extend heap by this amount (bytes) */
#define OVERHEAD    24      /* Minimum block size */


#define MAX(x, y) ((x) > (y)? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)       (*(unsigned int *)(p))
#define PUT(p, val)  (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

#define GET_ALIGN(p) (GET(p) & 0x7)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)       ((char *)(bp) - WSIZE)
#define FTRP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))


/* MACROS FOR EXPLICIT LIST IMPLEMENTATION */
/* Access this block's pointers. Argument bp is a pointer to the
* first byte of payload in this block. Pointer to previous free
* block is located at bp, and the forward pointer immediately follows */
#define BACK_PTR(bp) (*(char**)(bp))
#define FWD_PTR(bp) (*(char **)(bp + WSIZE))
/* Set the pointers for this block */
#define SET_BACK_PTR(bp, ptr) (BACK_PTR(bp) = ptr)
#define SET_FWD_PTR(bp, ptr) (FWD_PTR(bp) = ptr)

/* Global variables */
static char *heap_listp = 0;  /* Pointer to first block */
static char *start_flist = 0; /* Pointer to start of our explicit free list */

/* Function prototypes for internal helper routines */
static void *extend_heap(size_t words);
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);
static void *coalesce(void *bp);
static void flist_remove(void *bp);
static void flist_add(void *bp);

static void printblock(void *bp);
static void checkblock(void *bp);


/*********************************************************
 * CORE FUNCTIONS
 ********************************************************/

/*
 * mm_init - initialize the malloc package.
 *
 * Borrowed from textbook
 */
 int mm_init(void) {

  /* Create the initial empty heap. */
  if ((heap_listp = mem_sbrk(4*WSIZE)) == NULL)
    return -1;

  PUT(heap_listp, 0);                            /* Alignment padding */
  PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); /* Prologue header */
  PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); /* Prologue footer */
  PUT(heap_listp + (3 * WSIZE), PACK(0, 1));     /* Epilogue header */
  start_flist = heap_listp + 2*WSIZE;

  /* Extend the empty heap with a free block of minimum possible block size */
  if (extend_heap(4) == NULL){
    return -1;
  }
  return 0;
}


/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 *
 * No changes to mm_malloc needed for explicit list implementation, all
 * substantive changes will happen in helper functions
 */
void *mm_malloc(size_t size)
{
    printf("START\n");
    size_t asize;      /* Adjusted block size */
    size_t extendsize; /* Amount to extend heap if no fit */
    char *bp;

    if (heap_listp == 0){
        mm_init();
    }
    printf("INITIALIZED\n");

    /* Ignore spurious requests */
    if (size == 0)
        return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE)
        asize = 2*DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {
        printf("FIT FOUND\n");
        place(bp, asize);
        printf("PLACED1\n");
        return bp;
    }


    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize,CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    printf("HEAP EXTENDED\n");
    place(bp, asize);
    printf("PLACED 2\n");
    printf("GOODBYE\n");

    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{

    if(bp == NULL) return;

    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));

    coalesce(bp);

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



/*********************************************************
 * HELPER FUNCTIONS
 ********************************************************/

/*
* Extend the heap with a free block, coallesce with neighbors, and
* give us a pointer to the free block.
*
* This helper function also borrowed from the textbook
*/
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;

    size = MAX(size, 2 * DSIZE);

    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));         /* Free block header */
    PUT(FTRP(bp), PACK(size, 0));         /* Free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */


    flist_add(bp);


    return bp;

    /* Coalesce if the previous block was free */
    //return coalesce(bp);
}


/*
* Coalesce a newly freed block with its negbors and return a pointer
* to the start of this new block.
*/
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    // if (prev_alloc && next_alloc) {            /* Case 1 */
        flist_add(bp);
        return bp;
    // }
    //
    // else if (prev_alloc && !next_alloc) {      /* Case 2 */
    //     size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
    //     PUT(HDRP(bp), PACK(size, 0));
    //     PUT(FTRP(bp), PACK(size,0));
    //     /* Set next block's forward and back pointers to point to
    //     the new, larger block */
    //     SET_FWD_PTR(BACK_PTR(NEXT_BLKP(bp)), bp);
    //     SET_BACK_PTR(FWD_PTR(NEXT_BLKP(bp)), bp);
    //     /* Set the new, bigger block to point to the same forward
    //     and back blocks as before */
    //     SET_FWD_PTR(bp, FWD_PTR(NEXT_BLKP(bp)));
    //     SET_BACK_PTR(bp, BACK_PTR(NEXT_BLKP(bp)));
    // }
    //
    // else if (!prev_alloc && next_alloc) {      /* Case 3 */
    //     size += GET_SIZE(HDRP(PREV_BLKP(bp)));
    //     PUT(FTRP(bp), PACK(size, 0));
    //     PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
    //     bp = PREV_BLKP(bp);
    //     /* Don't need to mess with pointers. Incoming and outgoing pointers from
    //     previous block should do the trick */
    // }
    //
    // else {                                     /* Case 4 */
    //     size += GET_SIZE(HDRP(PREV_BLKP(bp))) +
    //         GET_SIZE(FTRP(NEXT_BLKP(bp)));
    //     PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
    //     PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
    //     bp = PREV_BLKP(bp);
    //     // First, set the first block's forward pointer to the
    //     // last blocks forward pointer
    //     SET_FWD_PTR(PREV_BLKP(bp), FWD_PTR(NEXT_BLKP(bp)));
    //     SET_BACK_PTR(FWD_PTR(NEXT_BLKP(bp)), PREV_BLKP(bp));
    // }

    // return bp;
}

/*
* A method that uses first-fit search to find the first slot where we can
* place a block of size asize
*/
static void *find_fit(size_t asize)
{
  /* First-fit search */
  void* bp;

  for(bp = start_flist; GET_ALLOC(HDRP(bp)) == 0; bp = FWD_PTR(bp)) {
    if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
      return bp;
    }
  }
  return NULL; /* No fit */
}

/*
* Allocate a block of size asize into the slot pointed to by bp
*/
static void place(void *bp, size_t asize)
{
  size_t csize = GET_SIZE(HDRP(bp));

  // Case 1: There is enough leftover space to split the block
  // if ((csize - asize) >= (2*DSIZE)) {

      // // Step 1: update size and status of bp
      // PUT(HDRP(bp), PACK(asize, 1));
      // PUT(FTRP(bp), PACK(asize, 1));
      //
      // flist_remove(bp);
      //
      // // A pointer to the leftover section
      // void* temp = NEXT_BLKP(bp);
      //
      // // Step 2: update size and status of bp
      // PUT(HDRP(bp), PACK(csize-asize, 0));
      // PUT(FTRP(bp), PACK(csize-asize, 0));
      //
      // // Step 3: Set bp's back ptr's fwd point to leftover
      // SET_FWD_PTR(BACK_PTR(bp), temp);
      //
      // // Step 4: Set leftover's back pointer to bp's back pointer
      // SET_BACK_PTR(temp, BACK_PTR(bp));
      //
      // // Step 5: Set bp's forward pointer's back pointer to leftover
      // SET_BACK_PTR(FWD_PTR(bp), temp);
      //
      // // Step 6: Set leftover's forward pointer to bp's forward point
      // SET_FWD_PTR(temp, FWD_PTR(bp));

  // Case 2: we don't have extra space, so allocate the whole block
  // else {
      PUT(HDRP(bp), PACK(csize, 1));
      PUT(FTRP(bp), PACK(csize, 1));
      // Step 1: Set back pointer's forward pointer to bp's forward pointer
      // SET_FWD_PTR(BACK_PTR(bp), FWD_PTR(bp));
      // Step 2: Set forward pointer's back pointer to bp's back pointer
      // SET_BACK_PTR(FWD_PTR(bp), BACK_PTR(bp));
      // printf("ABOUT TO REMOVE\n");
      flist_remove(bp);
      // printf("REMOVED\n");
  // }
}


/*
* Remove the block pointed to by bp from the free list
*/
static void flist_remove(void* bp) {

  // Case 1: Block we're allocating isn't the first item in our free list
  if (BACK_PTR(bp) != 0) {
    printf("previous\n");
    SET_FWD_PTR(BACK_PTR(bp), FWD_PTR(bp));
  } else { // Case 2: The block we're allocating is the first item in our free list
    printf("no previous\n");
    start_flist = FWD_PTR(bp);
  }
  // We always need to set the forward item's back pointer
  printf("not working\n");
  SET_BACK_PTR(FWD_PTR(bp), BACK_PTR(bp));

}

/*
* A method to add the free block at bp to our list
*/
static void flist_add(void*bp) {
  printf("flistadd\n");
  SET_FWD_PTR(bp, start_flist);
  SET_BACK_PTR(start_flist, bp);
  SET_BACK_PTR(bp, NULL);
  start_flist = bp;
}


/*********************************************************
 * HEAP CHECKING FUNCTIONS
 ********************************************************/

static void printblock(void *bp)
{
    size_t hsize, halloc, fsize, falloc;

    // checkheap(0);
    hsize = GET_SIZE(HDRP(bp));
    halloc = GET_ALLOC(HDRP(bp));
    fsize = GET_SIZE(FTRP(bp));
    falloc = GET_ALLOC(FTRP(bp));

    if (hsize == 0) {
        printf("%p: EOL\n", bp);
        return;
    }

    printf("%p: header: [%ld:%c] footer: [%ld:%c]\n", bp,
           hsize, (halloc ? 'a' : 'f'),
           fsize, (falloc ? 'a' : 'f'));
}

static void checkblock(void *bp)
{
    if ((size_t)bp % 8)
        printf("Error: %p is not doubleword aligned\n", bp);
    if (GET(HDRP(bp)) != GET(FTRP(bp)))
        printf("Error: header does not match footer\n");
}

/*
 * checkheap - Minimal check of the heap for consistency
 */
void checkheap(int verbose)
{
    char *bp = heap_listp;

    if (verbose)
        printf("Heap (%p):\n", heap_listp);

    if ((GET_SIZE(HDRP(heap_listp)) != DSIZE) || !GET_ALLOC(HDRP(heap_listp)))
        printf("Bad prologue header\n");
    checkblock(heap_listp);

    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (verbose)
            printblock(bp);
        checkblock(bp);
    }

    if (verbose)
        printblock(bp);
    if ((GET_SIZE(HDRP(bp)) != 0) || !(GET_ALLOC(HDRP(bp))))
        printf("Bad epilogue header\n");
}
