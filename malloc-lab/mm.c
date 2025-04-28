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
#include <errno.h>
#include <stdint.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "Team6",
    /* First member's full name */
    "Hyeonho Cho",
    /* First member's email address */
    "joho0504@gmail.com",
    /* Second member's full name (leave blank if none) */
    "Harin Lee",
    /* Second member's email address (leave blank if none) */
    "gbs1823@gmail.com"};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE sizeof(void *) //워드와 헤더풋터 사이즈(바이트), 32비트는 4, 64비트에서는 8바이트 
#define DSIZE (2*WSIZE) //더블 워드 사이즈(바이트)
#define CHUNKSIZE (1<<12) //초기 가용 블록과 힙 확장을 위한 크기(바이트)

#define MAX(x,y) ((x)>(y) ? (x):(y)) 

//크기와 할당 비트를 통합해서 헤더와 풋터에 저장할 수 있는 값을 리턴함.
#define PACK(size, alloc) ((size) | (alloc)) 

//인자 p가 참조하는 워드를 읽고 씀
#define GET(p) (*(uintptr_t *)(p))
#define PUT(p, val) (*(uintptr_t *)(p)=(val))

//주소 p에 있는 헤더/풋터의 size를 리턴함.
#define GET_SIZE(p) (GET(p) & ~(DSIZE-1))
//주소 p에 있는 헤더/풋터의 할당 비트를 리턴함.
#define GET_ALLOC(p) (GET(p) & 0x1)

//블록의 포인터 bp를 받고, 블록 헤더를 가리키는 포인터를 리턴함.
#define HDRP(bp) ((char *) (bp) - WSIZE)
//블록의 포인터 bp를 받고, 블록 풋터를 가리키는 포인터를 리턴함.
#define FTRP(bp) ((char *) (bp) + GET_SIZE(HDRP(bp))-DSIZE)

//블록의 포인터 bp를 받고, 다음 블록의 포인터 리턴.
#define NEXT_BLKP(bp)  ((void *)(bp) + GET_SIZE(HDRP(bp))) 
//블록의 포인터 bp를 받고, 이전 블록의 포인터 리턴.
#define PREV_BLKP(bp) ((void *)(bp) - GET_SIZE((void *)(bp) - DSIZE))

//블록의 포인터 bp를 받고, Free list의 이전블록의 포인터를 리턴 
#define PREC_FREEP(bp) (*(void**)(bp))
//블록의 포인터 bp를 받고, Free list의 이후블록의 포인터를 리턴
#define SUCC_FREEP(bp) (*(void **)((char *)(bp) + WSIZE))

static void *heap_listp=NULL;
static void *free_listp=NULL;
static void *last_listp=NULL;

static int test=0;


static void heap_debugger(){
    fprintf(stderr, "HEAP_DEBUGGER\n");
    
    void* bp;
    int check=0;
    
    fprintf(stderr, "heap_list\n");
    for(bp=heap_listp; GET_SIZE(HDRP(bp))>0; bp=NEXT_BLKP(bp)){
        fprintf(stderr, "bp: %p, GET_SIZE(HDRP(bp)): %ld  GET_ALLOC(HDRP(bp)): %ld\n", bp, GET_SIZE(HDRP(bp)), GET_ALLOC(HDRP(bp)));    
    }

    fprintf(stderr, "free_list\n");
    for (bp = free_listp; bp != NULL; bp = SUCC_FREEP(bp)) {
        fprintf(stderr, "bp: %p, GET_SIZE(HDRP(bp)): %ld  GET_ALLOC(HDRP(bp)): %ld\n", bp, GET_SIZE(HDRP(bp)), GET_ALLOC(HDRP(bp)));
    }

    fprintf(stderr, "debugger END\n");
    
}

static void heap_debugger2(){
    
    void* bp;

    fprintf(stderr, "heap_debugger2, free_listp: %p\n", free_listp);

    for (bp = free_listp; bp != NULL; bp = SUCC_FREEP(bp)) {
        fprintf(stderr, "bp: %p, GET_SIZE(HDRP(bp)): %ld  GET_ALLOC(HDRP(bp)): %ld SUCC: %p\n", bp, GET_SIZE(HDRP(bp)), GET_ALLOC(HDRP(bp)), SUCC_FREEP(bp));
    }

    fprintf(stderr, "debugger END\n");
    
}

//가용 블록을 free_list의 앞부분에 넣음
void PUTfree(void*bp){
    // fprintf(stderr, "bp address: %p\n", bp);
    // fprintf(stderr, "bp status: %ld\n", GET_ALLOC(HDRP(bp)));
    SUCC_FREEP(bp) = free_listp;//원래 맨앞의 요소
    PREC_FREEP(bp) = NULL;//리스트의 앞이기 떄문에 전임자는 없음

    if (free_listp!= NULL) {//맨 처음을 제외하고
        PREC_FREEP(free_listp) = bp;
    }

    free_listp = bp;
}

//free_list에서 할당상태 된 블록 빼기 
void Popfree(void*bp){
    void *pred = PREC_FREEP(bp);
    void *succ = SUCC_FREEP(bp);
    
    if(last_listp==bp)
        last_listp=NULL;

    if (pred != NULL) {
        SUCC_FREEP(pred) = succ;
    } else {
        // pred가 NULL이면 bp가 head임
        free_listp = succ;
    }

    if (succ != NULL) {//후임자가 있으면
        PREC_FREEP(succ) = pred;
    }

    // heap_debugger2();
}

static void *coalesce(void *bp){
    // fprintf(stderr, "cur block Alloc status: %ld\n", GET_ALLOC(HDRP(bp)));
    size_t prev_alloc = GET_ALLOC(HDRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));
    void* next_bp=NEXT_BLKP(bp);
    
    if(prev_alloc!=0 && prev_alloc !=1){
        //이전 블록이 할당 상태면 풋터가 존재하지 않으므로 prec_alloc에 쓰레기값 담기게 된다.
        prev_alloc=1;
    }
    
    //case 1. 전후 블록 모두 할당상태
    if(prev_alloc && next_alloc){
        // fprintf(stderr, "case 1\n");
        PUTfree(bp);
        return bp;
    }
    
    //case 2.다음 블록 가용상태 
    else if(prev_alloc&&!next_alloc){
        // fprintf(stderr, "case 2\n");
        Popfree(next_bp);
        size+=GET_SIZE(HDRP(next_bp)); //다음 블록 크기만큼 증가
        PUT(HDRP(bp), PACK(size,0));
        PUT(FTRP(bp), PACK(size,0)); 
    }
    
    //case 3. 이전 블록 가용상태 
    else if(!prev_alloc && next_alloc){
        // fprintf(stderr, "case 3\n");
        void *prev_bp=PREV_BLKP(bp);
        Popfree(prev_bp);
        size+=GET_SIZE(HDRP(PREV_BLKP(bp)));
        //헤더랑 풋터 업뎃
        PUT(FTRP(bp), PACK(size,0));
        PUT(HDRP(prev_bp), PACK(size,0));
        bp=prev_bp; //bp는 이전 블록의 bp로
        
    }
    
    //case 4. 전후 블록 가용상태
    else{
        // fprintf(stderr, "case 4\n");
        void *prev_bp=PREV_BLKP(bp);
        Popfree(prev_bp);
        Popfree(next_bp);
        size+=GET_SIZE(HDRP(next_bp)) + GET_SIZE(HDRP(prev_bp));
        PUT(HDRP(prev_bp), PACK(size,0));
        PUT(FTRP(next_bp), PACK(size,0));
        bp=prev_bp;
        
    }

    // fprintf(stderr, "after coal bp Alloc status: %ld\n", GET_ALLOC(HDRP(bp)));
    PUTfree(bp);//병합된 블록 삽입

    // heap_debugger();

    return bp;

}

static void *extend_heap(size_t words){
    char *bp;
    size_t size;

    // fprintf(stderr, "size: %d\n", words);
    size=(words%2) ? (words+1) *WSIZE : words*WSIZE;
    
    if((void*)(bp = mem_sbrk(size))==(void*)-1){
        return NULL;
    }
    
    //가용 블록의 헤더풋터와 에필로그 블록의 헤더를 초기화.
    PUT(HDRP(bp), PACK(size,0)); //가용 블록 헤더 초기화
    PUT(FTRP(bp), PACK(size,0)); //가용 블록 풋터 초기화
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1)); //에필로그 블록 헤더 초기화
    // fprintf(stderr, "checkpoint 2\n");

    // heap_debugger();
    //이전 블록이 가용 상태면 병합 
    return coalesce(bp);
}

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    //패딩, 헤더(프롤로그), 풋터(프롤로그), PREC, SUCC, 헤더(에필로그) 
    heap_listp = mem_sbrk(8*WSIZE);

    if(heap_listp==(void*)-1){
        // perror("BP 1");
        return -1;
    }

    PUT(heap_listp,0);
    PUT(heap_listp + (1*WSIZE),PACK(DSIZE,1));
    PUT(heap_listp + (2*WSIZE),PACK(DSIZE,1));
    PUT(heap_listp + (3*WSIZE),PACK(2*DSIZE,0));//맨처음에 적장한 블럭을 하나 만들고 시작할것!!!
    PUT(heap_listp + (4*WSIZE),NULL);
    PUT(heap_listp + (5*WSIZE),NULL);
    PUT(heap_listp + (6*WSIZE),PACK(2*DSIZE,0));
    PUT(heap_listp + (7*WSIZE),PACK(0,1));
    heap_listp += (4*WSIZE);//가용블록의 블록 포인터
    // heap_listp +=DSIZE;

    // heap_debugger();
    if(extend_heap(CHUNKSIZE/WSIZE)==NULL){
        return -1;
    }

    return 0;
}


static void *find_fit(int asize){

    void *bp;
    void *best_bp = NULL;
    size_t best_size = (size_t)-1;

    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp))) {
            size_t bsize = GET_SIZE(HDRP(bp));
            if (asize <= bsize) {
                if (bsize == asize) {
                    return bp; // Perfect fit, early exit!
                }
                if (bsize < best_size) {
                    best_size = bsize;
                    best_bp = bp;
                }
            }
        }
    }
    return best_bp;

    // void *bp;



    // if(last_listp==NULL)
    //     last_listp=free_listp;   
        
    // if(last_listp==NULL)
    //     return NULL;

    // for (bp = last_listp; bp != NULL; bp = SUCC_FREEP(bp)) 
    // {
    //     if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
    //     {
    //         last_listp=bp;
    //         return bp;
    //     }
    // }

    // for (bp = heap_listp; bp != NULL && last_listp > bp; bp = NEXT_BLKP(bp))
    // {
    //     if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
    //     {
    //         return bp;
    //     }
    // }

    // return NULL;

}


static void place(void *bp, size_t asize){
    
    size_t csize= GET_SIZE(HDRP(bp));
    Popfree(bp);

    //할당 될 블록 free_list에서 제거
    if((csize-asize) >=2*DSIZE){
        //블록 분할 가능 
        PUT(HDRP(bp), PACK(asize ,1)); //헤더에 크기와 할당 상태 1 기록
        PUT(FTRP(bp), PACK(asize ,1)); //풋터에 크기와 할당 상태 1 기록
        bp=NEXT_BLKP(bp); //다음 블록으로
        PUT(HDRP(bp), PACK(csize-asize,0)); //헤더에 남은 크기 (csize-asize)와 가용 상태 0 기록
        PUT(FTRP(bp), PACK(csize-asize,0)); //풋터에  남은 크기 (csize-asize)와 가용 상태 0 기록
        PUTfree(bp);
    }else{
        //블록 분할 불가능 
        PUT(HDRP(bp), PACK(csize,1)); //헤더에 전체 크기와 할당 상태 1 기록
        PUT(FTRP(bp), PACK(csize,1)); //풋터에 전체 크기와 할당 상태 1 기록
    }
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t newsize;
    size_t extendsize;
    char *bp;

    
    if(heap_listp==0){
        mm_init();
    }
    
    if(size==0) {
        return NULL;
    }
    
    if(size<=DSIZE)
    newsize=2*DSIZE;
    else
    newsize=DSIZE*((size + DSIZE + DSIZE-1)/DSIZE);
    
    if(size==(int)176) fprintf(stderr, "HERE!");

    bp=find_fit(newsize);
    
    //적합한 가용 블록 탐색
    if(bp!=NULL){
        place(bp, newsize);
        return bp;
    }
    
    //가용 블록 못찾으면 확장 
    extendsize = MAX(newsize, CHUNKSIZE);
    if((bp=extend_heap(extendsize/WSIZE))==NULL){
        // perror("BP 5");
        return NULL;
    }
    
    place(bp, newsize);

    
    return bp;
    
    
}

/*
* mm_free - Freeing a block does nothing.
*/
void mm_free(void *ptr)
{
    // fprintf(stderr, "free start %p\n", ptr);
    if(ptr==NULL) return;
    
    size_t size=GET_SIZE(HDRP(ptr));
    
    PUT(HDRP(ptr), PACK(size,0));
    PUT(FTRP(ptr), PACK(size,0));
    coalesce(ptr);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */

void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t oldsize;
    
    if((int)size<0) return NULL;

    if(size==0){
        mm_free(ptr);
        return NULL;
    }

    if(ptr==NULL)
        return(mm_malloc(size));

    newptr = mm_malloc(size);
    if (newptr == NULL)
        return NULL;


    oldsize = GET_SIZE(HDRP(oldptr));    
    if (size < oldsize) //원래 size가 요청사이즈보다 크면, 원래 사이즈를 요청사이즈로 
        oldsize = size;
    memcpy(newptr, oldptr, oldsize); //oldptr중 copySize만큼을 newptr에 복사. 
    mm_free(oldptr); //oldptr을 반환.

    return newptr;

}