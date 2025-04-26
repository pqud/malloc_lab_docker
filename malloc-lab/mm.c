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

#define SIZE_T_SIZE (ALIGN(sizeof(int)))

#define WSIZE sizeof(void *) //워드와 헤더풋터 사이즈(바이트), 32비트는 4, 64비트에서는 8바이트 
#define DSIZE (2*WSIZE) //더블 워드 사이즈(바이트)
#define MINBLOCKSIZE (2*DSIZE) //헤더+풋터+payload 최소 블록 크기. place 비교용 
#define CHUNKSIZE (1<<7) //초기 가용 블록과 힙 확장을 위한 크기(바이트)

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

//블록의 포인터 bp를 받고, Free list의 이전블록의 포인터를 이전
#define PREC_FREEP(bp) (*(void**)(bp))
//블록의 포인터 bp를 받고, Free list의 이후블록의 포인터를 이전
#define SUCC_FREEP(bp) (*(void**)(bp+WSIZE))

static void *heap_listp=NULL;
static void *free_listp=NULL;

static int test=0;


static void heap_debugger(){
    fprintf(stderr, "HEAP_DEBUGGER\n");
    
    void* bp;
    int check=0;

    //블록 단위 순회라서 패딩은 순회에 포함 한됨.
    //프롤로그 블록은 헤더+풋터 해서 8바이트고 프롤로그의 payload 부터 순회함.
    //에필로그 블록은 크기가 0이라서 순회 조건에 안걸림. 
    for(bp=heap_listp; GET_SIZE(HDRP(bp))>0; bp=NEXT_BLKP(bp)){
        fprintf(stderr, "bp: %p, GET_SIZE(HDRP(bp)): %ld  GET_ALLOC(HDRP(bp)): %ld\n", bp, GET_SIZE(HDRP(bp)), GET_ALLOC(HDRP(bp)));    
        // check++;
        // if(check>100){
        //     return;
        // }
    }
}

//가용 블록을 free_list의 앞부분에 넣음
void PUTfree(void*bp){
    // fprintf(stderr, "in put Free 1\n");
    SUCC_FREEP(bp)= free_listp;
    PREC_FREEP(bp)=NULL;
    PREC_FREEP(free_listp)=bp;
    // fprintf(stderr, "in put Free 2\n");
    free_listp=bp;
}

//free_list에서 할당상태 된 블록 빼기 
void Popfree(void*bp){
    if(bp==free_listp){
        PREC_FREEP(SUCC_FREEP(bp))=NULL;
        free_listp=SUCC_FREEP(bp);
    }
    else{
        SUCC_FREEP(PREC_FREEP(bp))=SUCC_FREEP(bp);
        PREC_FREEP(SUCC_FREEP(bp))=PREC_FREEP(bp);
    }
}

static void *coalesce(void *bp){
    
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
        PUT(FTRP(next_bp), PACK(size,0)); 
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

    PUTfree(bp);


    return bp;

}

static void *extend_heap(size_t words){
    char *bp;
    size_t size;

    // fprintf(stderr, "checkpoint 1\n");
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
    heap_listp = mem_sbrk(6*WSIZE);

    if(heap_listp==(void*)-1){
        // perror("BP 1");
        return -1;
    }

    PUT(heap_listp, 0); //패딩
    PUT(heap_listp + (1*WSIZE), PACK(MINBLOCKSIZE,1)); //프롤로그 블록 헤더 (8|1)
    PUT(heap_listp + (2*WSIZE), NULL); //prec
    PUT(heap_listp + (3*WSIZE), NULL); //succ
    PUT(heap_listp + (4*WSIZE), PACK(MINBLOCKSIZE,1)); //에필로그 블록 헤더 4바이트 (0|1 )
    PUT(heap_listp + (5*WSIZE), PACK(0,1)); //에필로그 블록 헤더 4바이트 (0|1 )
    // heap_listp +=DSIZE;

    free_listp=heap_listp+2*WSIZE;

    if(extend_heap(CHUNKSIZE/WSIZE)==NULL){
        return -1;
    }

    return 0;
}



static void *find_fit(int asize){
    void *bp;
    // fprintf(stderr, "start find_fit\n");

    for(bp=free_listp; GET_ALLOC(HDRP(bp))!=1; bp=SUCC_FREEP(bp)){
        if(asize<=GET_SIZE(HDRP(bp))){
            // fprintf(stderr, "find\n");
            return bp;
        }
    }
    // fprintf(stderr, "no fit\n");
    return NULL;
}

static void place(void *bp, size_t asize){
    
    size_t csize= GET_SIZE(HDRP(bp));

    //할당 될 블록 free_list에서 제거
    Popfree(bp);

    if((csize-asize) >=MINBLOCKSIZE){
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

    // fprintf(stderr, "start malloc \n");

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
        newsize=DSIZE*((size+(DSIZE) + (DSIZE-1))/DSIZE);

    bp=find_fit(newsize);
    //적합한 가용 블록 탐색
    if(bp!=NULL){
        place(bp, newsize);
        return bp;
    }
    
    // fprintf(stderr, "bp is NULL!!\n");
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