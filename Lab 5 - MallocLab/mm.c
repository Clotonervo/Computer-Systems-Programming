/*
 * mm-implicit.c -  Simple allocator based on implicit free lists,
 *                  first fit placement, and boundary tag coalescing.
 *
 * Each block has header and footer of the form:
 *
 *      31                     3  2  1  0
 *      -----------------------------------
 *     | s  s  s  s  ... s  s  s  0  0  a/f
 *      -----------------------------------
 *
 * where s are the meaningful size bits and a/f is set
 * iff the block is allocated. The list has the following form:
 *
 * begin                                                          end
 * heap                                                           heap
 *  -----------------------------------------------------------------
 * |  pad   | hdr(8:a) | ftr(8:a) | zero or more usr blks | hdr(8:a) |
 *  -----------------------------------------------------------------
 *          |       prologue      |                       | epilogue |
 *          |         block       |                       | block    |
 *
 * The allocated prologue and epilogue blocks are overhead that
 * eliminate edge conditions during coalescing.
 */
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "mm.h"
#include "memlib.h"

/*
 * If NEXT_FIT defined use next fit search, else use first fit search
 */
#define NEXT_FITx

/* Team structure */
team_t team = {
#ifdef NEXT_FIT
    "implicit next fit",
#else
    "explicit first fit",
#endif
    "Sam Hopkins", "h0pkins3",
    "Annie Larkin", "avl7949"
};

/* $begin mallocmacros */
/* Basic constants and macros */
#define WSIZE       4       /* word size (bytes) */
#define DSIZE       8       /* doubleword size (bytes) */
#define CHUNKSIZE  (1<<14)  /* initial heap size (bytes) */
#define OVERHEAD    8       /* overhead of header and footer (bytes) */

#define MAX(x, y) ((x) > (y)? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)       (*(size_t *)(p))
#define PUT(p, val)  (*(size_t *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)       ((char *)(bp) - WSIZE)
#define FTRP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
#define NEXT_PTR(bp)       ((char *)(bp + WSIZE))
#define PREV_PTR(bp)       ((char *)(bp))


/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))
/* $end mallocmacros */

/* Global variables */
static char *heap_listp;  /* pointer to first block */
#ifdef NEXT_FIT
static char *rover;       /* next fit rover */
#endif


/* function prototypes for internal helper routines */
static void *extend_heap(size_t words);
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);
static void *coalesce(void *bp);
static void printblock(void *bp);
static void checkblock(void *bp);

/* My Global variables */
char *root = NULL;
char *end;
int free_list_size = 0;


/*
 * mm_init - Initialize the memory manager
 */
/* $begin mminit */
int mm_init(void)                     //DONE
{
    /* create the initial empty heap */
    if ((heap_listp = mem_sbrk(16)) == NULL)
	return -1;
    PUT(heap_listp, 0);                        /* alignment padding */
    PUT(heap_listp+WSIZE, PACK(OVERHEAD, 1));  /* prologue header */
    PUT(heap_listp+DSIZE, PACK(OVERHEAD, 1));  /* prologue footer */
    PUT(heap_listp+WSIZE+DSIZE, PACK(0, 1));   /* epilogue header */
    heap_listp += DSIZE;
    end = heap_listp + WSIZE + DSIZE;       //initialize end, which points to the epilogue block
    root = NULL;
    free_list_size = 0;

#ifdef NEXT_FIT
    rover = heap_listp;
#endif

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
	return -1;
    return 0;
}
/* $end mminit */

/* root
 * mm_malloc - Allocate a block with at least size bytes of payload
 */
/* $begin mmmalloc */
void *mm_malloc(size_t size)          //DONE
{
  //printf("In malloc\n");

    size_t asize;      /* adjusted block size */
    char *bp;

    // mm_checkheap(1);
    /* Ignore spurious requests */
    if (size <= 0)
	     return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE){
	    asize = DSIZE + OVERHEAD;
    }
    else {
	    asize = DSIZE * ((size + (OVERHEAD) + (DSIZE-1)) / DSIZE);
    }

    if ((bp = find_fit(asize)) != NULL) {
	    place(bp, asize);
	    return bp;
    }
    /* No fit found. Get more memory and place the block  */
    if ((bp = extend_heap(CHUNKSIZE)) == NULL){
	    return NULL;
    }
    place(bp, asize);
    return bp;
}
/* $end mmmalloc */

/*
 * mm_free - Free a block
 */
/* $begin mmfree */
void mm_free(void *bp)                  //DONE
{
  //printf("In free \n");

    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));     //set the header and footer to 0

    coalesce(bp);       //check if needs coalescing
}

/* $end mmfree */

/*
 * mm_realloc - naive implementation of mm_realloc
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *newp;
    size_t copySize;
   // //printf("In REALLOC \n");

    if ((newp = mm_malloc(size)) == NULL) {
	printf("ERROR: mm_malloc failed in mm_realloc\n");
	exit(1);
    }
    copySize = GET_SIZE(HDRP(ptr));
    if (size < copySize)
      copySize = size;
    memcpy(newp, ptr, copySize);
    mm_free(ptr);
    return newp;
}

/*
 * mm_checkheap - Check the heap for consistency
 */
void mm_checkheap(int verbose)
{
    char *bp = heap_listp;

    if (verbose){
	   printf("Heap (%p):\n", heap_listp);
    }

    if ((GET_SIZE(HDRP(heap_listp)) != DSIZE) || !GET_ALLOC(HDRP(heap_listp))){ //Checks header and footer of the heap list
	   printf("Bad prologue header\n");
     checkblock(heap_listp);
    }

    for (bp = heap_listp; bp != end; bp = NEXT_BLKP(bp)) { //Outputs information concerning the current malloc
	     if (verbose){
	        printblock(bp);
	        checkblock(bp);
        }
    }

    char* list_checker = root;                //list checker iterates through the free list
    for (int i = 0; i < free_list_size; i++){
      if (GET_ALLOC(HDRP(list_checker))){
        printf("ERROR: Allocated header block in free list\n");   //if header of pointer is allocated
      }
      else if (GET_ALLOC(FTRP(list_checker))){
        printf("ERROR: Allocated footer block in free list\n");   //if footer of pointer is allocated
      }
      else if (!GET_ALLOC(HDRP(NEXT_BLKP(list_checker)))){
        printf("ERROR: Coalesce failure, next block is also free");   // if the next block is free, it is not coalesced
      }
      else if (GET_ALLOC(HDRP(PREV_BLKP(list_checker)))){
        printf("ERROR: Coalesce failure, previous block is also free\n");   //if the previous block is free, it is not coalesced
      }
      list_checker = GET(NEXT_PTR(list_checker));
    }

    if (free_list_size != 0){
      for (bp = heap_listp; bp != end; bp = NEXT_BLKP(bp)){ // this double for loop checks to make sure that
        list_checker = root;
        int valid = 0;                                      // the free list points to all valid pointers on the heap
          for (int i = 0; i < free_list_size; i++){
            if (list_checker == bp){
              valid = 1;
            }
            list_checker = GET(NEXT_PTR(list_checker));
          }
          if (!valid){
            printf("ERROR: Invalid block in free list");        //free list contains an invalid block
          }
      }
  }

    if (verbose){
	    printblock(bp);
      print_free_list(root);
    }

    if ((GET_SIZE(HDRP(bp)) != 0) || !(GET_ALLOC(HDRP(bp)))){     //bad epilogue header
	     printf("Bad epilogue header\n");
     }
}



/* The remaining routines are internal helper routines */

/*
 * extend_heap - Extend heap with free block and return its block pointer
 */
/* $begin mmextendheap */
static void *extend_heap(size_t words)    //DONE
{
    char *bp;
    size_t size;
    //printf("In extend heap\n");

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ((bp = mem_sbrk(size)) == (void *)-1)
	   return NULL;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));         /* free block header */
    PUT(FTRP(bp), PACK(size, 0));         /* free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* new epilogue header */

    return coalesce(bp);
}


/* $end mmextendheap */

/*
 * place - Place block of asize bytes at start of free block bp
 *         and split if remainder would be at least minimum block size
 */
/* $begin mmplace */
/* $begin mmplace-proto */
static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));

    if ((csize - asize) >= (DSIZE + OVERHEAD)) {
                         //remove the found block from free list
                /*If bp is the only pointer in the list, set list to null*/
             if(GET(PREV_PTR(bp)) == NULL && GET(NEXT_PTR(bp)) == NULL){
                 root = NULL;
             }
             /*If bp is root, make its next the new root*/
             else if (GET(PREV_PTR(bp)) == NULL && GET(NEXT_PTR(bp)) != NULL){
               root = GET(NEXT_PTR(bp));
               PUT(GET(NEXT_PTR(bp)), NULL);
             }
             /*If bp is at the end of the list, just remove it from the end*/
             else if (GET(PREV_PTR(bp)) != NULL && GET(NEXT_PTR(bp)) == NULL){
                 PUT((GET(PREV_PTR(bp)) + WSIZE), NULL);
             }
             /*If bp is in the middle of the list, remove it and fix its next and previous pointers*/
             else {
                 PUT((GET(PREV_PTR(bp)) + WSIZE), GET(NEXT_PTR(bp)));
                 PUT((GET(NEXT_PTR(bp))), GET(PREV_PTR(bp)));
             }
             free_list_size--;

	    PUT(HDRP(bp), PACK(asize, 1));
      PUT(FTRP(bp), PACK(asize, 1));        //set block as allocated

      void* bp_next = NEXT_BLKP(bp);              //go to next block
	    PUT(HDRP(bp_next), PACK(csize-asize, 0));
      PUT(FTRP(bp_next), PACK(csize-asize, 0));       // set the next block to free

      	/*Check if list is empty*/
   	if(free_list_size == 0){
        	free_list_size++;
        	root = bp_next;
        	PUT(PREV_PTR(bp_next), 0);
        	PUT(NEXT_PTR(bp_next), 0);
    	}
    	else{       /*Point bp's next to the old root and make bp the new root*/
        	free_list_size++;
        	PUT(root,PREV_PTR(bp_next));
        	PUT(PREV_PTR(bp_next), 0);
        	PUT(NEXT_PTR(bp_next), root);
        	root = bp_next;
    	}
	                   //add to free list
    }
    else {
	     PUT(HDRP(bp), PACK(csize, 1));
	     PUT(FTRP(bp), PACK(csize, 1));
       		/*If bp is the only pointer in the list, set list to null*/
    	     if(GET(PREV_PTR(bp)) == NULL && GET(NEXT_PTR(bp)) == NULL){
                 root = NULL;
             }
             /*If bp is root, make its next the new root*/
             else if (GET(PREV_PTR(bp)) == NULL && GET(NEXT_PTR(bp)) != NULL){
               root = GET(NEXT_PTR(bp));
               PUT(GET(NEXT_PTR(bp)), NULL);
             }
             /*If bp is at the end of the list, just remove it from the end*/
             else if (GET(PREV_PTR(bp)) != NULL && GET(NEXT_PTR(bp)) == NULL){
                 PUT((GET(PREV_PTR(bp)) + WSIZE), NULL);
             }
             /*If bp is in the middle of the list, remove it and fix its next and previous nodes*/
             else {
                 PUT((GET(PREV_PTR(bp)) + WSIZE), GET(NEXT_PTR(bp)));
                 PUT((GET(NEXT_PTR(bp))), GET(PREV_PTR(bp)));
             }
             free_list_size--;                // becuase there is no other free block, simply remove from free list because there are not other free blocks
    }

}
/* $end mmplace */

/*
 * find_fit - Find a fit for a block with asize bytes
 */
static void *find_fit(size_t asize)
{
#ifdef NEXT_FIT
    /* next fit search */
    char *oldrover = rover;-

    /* search from the rover to the end of list */
    for ( ; GET_SIZE(HDRP(rover)) > 0; rover = NEXT_BLKP(rover))
	if (!GET_ALLOC(HDRP(rover)) && (asize <= GET_SIZE(HDRP(rover))))
	    return rover;

    /* search from start of list to old rover */
    for (rover = heap_listp; rover < oldrover; rover = NEXT_BLKP(rover))
	     if (!GET_ALLOC(HDRP(rover)) && (asize <= GET_SIZE(HDRP(rover))))
	    return rover;

    return NULL;  /* no fit found */
#else
    /* first fit search */
    void *bp = root;
      /*Traverse through free list using next pointers*/
      for (int i = 0; i < free_list_size; i++){
        if (!GET_ALLOC(HDRP(bp)) && asize <= GET_SIZE(HDRP(bp))){
          return bp;
        }
        bp = GET(NEXT_PTR(bp));
      }

    return NULL; /* no fit */
#endif
}


/*
 * coalesce - boundary tag coalescing. Return ptr to coalesced block
 */
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(HDRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));
    size_t allocated_blocks = prev_alloc + next_alloc;
//    printf("before prev and next declarations \n");

    void *bp_prev = PREV_BLKP(bp);
    void *bp_next = NEXT_BLKP(bp);
  //  printf("both = %d \n", allocated_blocks);
//  printf("after prev and next declarations \n");

    switch (allocated_blocks) {
      case 2:
    //    printf("case 2 \n");
        break;
      case 1:
    //  printf("case 1 \n");

        if (prev_alloc){
      //    printf("case 1 prev_alloc is allocated \n");

                void *next_ptr = GET(NEXT_PTR(bp_next));
                void *prev_ptr = GET(PREV_PTR(bp_next));
              //  printf("next_ptr = %p \n", next_ptr);
              //  printf("prev_ptr = %p \n", prev_ptr);

                        /*If bp is the only pointer in the list, set list to null*/
                     if(prev_ptr == NULL && next_ptr == NULL){
                         root = NULL;
                     }
                     /*If bp is root, make its next the new root*/
                     else if (prev_ptr == NULL && next_ptr != NULL){
                       root = GET(NEXT_PTR(bp_next));
                       PUT(GET(NEXT_PTR(bp_next)), NULL);
                     }
                     /*If bp is at the end of the list, just remove it from the end*/
                     else if (prev_ptr != NULL && next_ptr == NULL){
                         PUT((GET(PREV_PTR(bp_next)) + WSIZE), NULL);
                     }
                     /*If bp is in the middle of the list, remove it and fix its next and previous pointer*/
                     else {
                         PUT((GET(PREV_PTR(bp_next)) + WSIZE), GET(NEXT_PTR(bp_next)));
                         PUT((GET(NEXT_PTR(bp_next))), GET(PREV_PTR(bp_next)));
                     }
                     free_list_size--;

              size += GET_SIZE(HDRP(NEXT_BLKP(bp)));

              PUT(HDRP(bp), PACK(size, 0));
              PUT(FTRP(bp), PACK(size,0));
        }
        else {    //next block is allocated
        //  printf("case 1 next_alloc is allocated \n");

          void *next_ptr = GET(NEXT_PTR(bp_prev));
          void *prev_ptr = GET(PREV_PTR(bp_prev));
        //  printf("pointers declared \n");

           	     /*If bp is the only pointer in the list, set list to null*/
              if(prev_ptr == NULL && next_ptr == NULL){
                  root = NULL;
              }
              /*If bp is root, make its next the new root*/
              else if (prev_ptr == NULL && next_ptr != NULL){
                root = GET(NEXT_PTR(bp_prev));
                PUT(GET(NEXT_PTR(bp_prev)), NULL);
              }
              /*If bp is at the end of the list, just remove it from the end*/
              else if (prev_ptr != NULL && next_ptr == NULL){
                  PUT((GET(PREV_PTR(bp_prev)) + WSIZE), NULL);
              }
              /*If bp is in the middle of the list, remove it and fix its next and previous pointer*/
              else {
                  PUT((GET(PREV_PTR(bp_prev)) + WSIZE), GET(NEXT_PTR(bp_prev)));
                  PUT((GET(NEXT_PTR(bp_prev))), GET(PREV_PTR(bp_prev)));
              }
              free_list_size--;
          //    printf("after big if else chunk \n");
          //    printf("next_ptr = %p \n", next_ptr);
          //    printf("prev_ptr = %p \n", prev_ptr);

        	    size += GET_SIZE(HDRP(PREV_BLKP(bp)));
            //  printf("here 1");

              PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
            //  printf("here 2");

        	    PUT(FTRP(bp), PACK(size, 0));
              //printf("here 3");

              bp = PREV_BLKP(bp);
            //  printf("done with case");

        }
        break;
      default:
    //  printf("default \n");

           /*If bp is the only pointer in the list, set list to null*/
           if(GET(PREV_PTR(bp_prev)) == NULL && GET(NEXT_PTR(bp_prev)) == NULL){
               root = NULL;
           }
           /*If bp is root, make its next the new root*/
           else if (GET(PREV_PTR(bp_prev)) == NULL && GET(NEXT_PTR(bp_prev)) != NULL){
             root = GET(NEXT_PTR(bp_prev));
             PUT(GET(NEXT_PTR(bp_prev)), NULL);
           }
           /*If bp is at the end of the list, just remove it from the end*/
           else if (GET(PREV_PTR(bp_prev)) != NULL && GET(NEXT_PTR(bp_prev)) == NULL){
               PUT((GET(PREV_PTR(bp_prev)) + WSIZE), NULL);
           }
           /*If bp is in the middle of the list, remove it and fix its next and previous pointer*/
           else {
               PUT((GET(PREV_PTR(bp_prev)) + WSIZE), GET(NEXT_PTR(bp_prev)));
               PUT((GET(NEXT_PTR(bp_prev))), GET(PREV_PTR(bp_prev)));
           }

              /*If bp is the only pointer in the list, set list to null*/
           if(GET(PREV_PTR(bp_next)) == NULL && GET(NEXT_PTR(bp_next)) == NULL){
               root = NULL;
           }
           /*If bp is root, make its next the new root*/
           else if (GET(PREV_PTR(bp_next)) == NULL && GET(NEXT_PTR(bp_next)) != NULL){
             root = GET(NEXT_PTR(bp_next));
             PUT(GET(NEXT_PTR(bp_next)), NULL);
           }
           /*If bp is at the end of the list, just remove it from the end*/
           else if (GET(PREV_PTR(bp_next)) != NULL && GET(NEXT_PTR(bp_next)) == NULL){
               PUT((GET(PREV_PTR(bp_next)) + WSIZE), NULL);
           }
           /*If bp is in the middle of the list, remove it and fix its next and previous pointer*/
           else {
               PUT((GET(PREV_PTR(bp_next)) + WSIZE), GET(NEXT_PTR(bp_next)));
               PUT((GET(NEXT_PTR(bp_next))), GET(PREV_PTR(bp_next)));
           }
           free_list_size = free_list_size - 2;

    size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));

    PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
    PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
    bp = PREV_BLKP(bp);
        break;
    }


#ifdef NEXT_FIT
    /* Make sure the rover isn't pointing into the free block */
    /* that we just coalesced */
    if ((rover > (char *)bp) && (rover < NEXT_BLKP(bp)))
	rover = bp;
#endif

         /*Check if list is empty*/
        if(free_list_size == 0){
             free_list_size++;
             root = bp;
             PUT(PREV_PTR(bp), 0);
             PUT(NEXT_PTR(bp), 0);
        }
        else{       /*Point bp's next to the old root and make bp the new root*/
             free_list_size++;
             PUT(root,PREV_PTR(bp));
             PUT(PREV_PTR(bp), 0);
             PUT(NEXT_PTR(bp), root);
             root = bp;
        }
    return bp;
}


static void printblock(void *bp)
{
    size_t hsize, halloc, fsize, falloc;

    hsize = GET_SIZE(HDRP(bp));
    halloc = GET_ALLOC(HDRP(bp));
    fsize = GET_SIZE(FTRP(bp));
    falloc = GET_ALLOC(FTRP(bp));

    if (hsize == 0) {
	printf("%p: EOL\n", bp);
	return;
    }

    printf("%p: header: [%d:%c] footer: [%d:%c]\n", bp,
	   hsize, (halloc ? 'a' : 'f'),
	   fsize, (falloc ? 'a' : 'f'));
}

/* Traverse through free list and print each node's previous and next pointers */
void print_free_list(void* root){
  void* bp = root;
  printf("free_list_size = %d \n", free_list_size);
  for (int i = 0; i < free_list_size; i++){
    printf("bp = %p \n", bp);
    printf("bp->next = %p \n", GET(bp));
    printf("bp->prev = %p \n", GET(bp));

    bp = GET(NEXT_PTR(bp));
    }
}


static void checkblock(void *bp)
{
    if ((size_t)bp % 8)
	printf("Error: %p is not doubleword aligned\n", bp);
    if (GET(HDRP(bp)) != GET(FTRP(bp)))
	printf("Error: header does not match footer\n");
}
