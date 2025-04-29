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
    "gbs1823@gmail.com",
	"Eunchae Park",
	"ghkqh09@gmail.com"
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE sizeof(void *) //워드와 헤더풋터 사이즈(바이트), 32비트는 4, 64비트에서는 8바이트 
#define DSIZE (2*WSIZE) //더블 워드 사이즈(바이트)
#define MINBLOCKSIZE (4*WSIZE)
#define CHUNKSIZE (1<<12) //초기 가용 블록과 힙 확장을 위한 크기(바이트)

#define MAX(x,y) ((x)>(y) ? (x):(y)) 

//크기와 할당 비트를 통합해서 헤더와 풋터에 저장할 수 있는 값을 리턴함.
#define PACK(size, alloc) ((size) | (alloc & 0x1)) 

//인자 p가 참조하는 워드를 읽고 씀
#define GET(p) (*(uintptr_t *)(p))


//주소 p에 있는 헤더/풋터의 size를 리턴함.
#define GET_SIZE(p)     (GET(p) & ~0x7) 
//주소 p에 있는 헤더/풋터의 할당 비트를 리턴함.
#define GET_ALLOC(p) (GET(p) & 0x1)


#define PUT(p, val) (*(uintptr_t *)(p)=(val))
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

#define PRED_PTR(bp) (void*)((char *)(bp))
#define SUCC_PTR(bp) (void*)((char*)(bp) + WSIZE)
#define SET_PTR(p, bp) (*(uintptr_t *)(p) = (uintptr_t)(bp))

#define LISTLIMIT 20 // 총 20개의 크기 구간


static void *heap_listp=NULL;
static void *free_list[LISTLIMIT];

static int test=0;

static int find_list_index(size_t size);
static int find_list_index(size_t size);
void PUTfree(void*bp, size_t size);
void Popfree(void*bp);
static void *coalesce(void *bp);
static void *extend_heap(size_t words);
int mm_init(void);
void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
void *mm_malloc(size_t size);
void mm_free(void *ptr);
void *mm_realloc(void *ptr, size_t size);



static void heap_debugger(){
    fprintf(stderr, "=====HEAP_DEBUGGER=====\n");
    
    void* bp;
    int check=0;
    
    fprintf(stderr, "heap_list\n");
    for(bp=heap_listp; GET_SIZE(HDRP(bp))>0; bp=NEXT_BLKP(bp)){
        fprintf(stderr, "bp: %p, GET_SIZE(HDRP(bp)): %ld  GET_ALLOC(HDRP(bp)): %ld\n", bp, GET_SIZE(HDRP(bp)), GET_ALLOC(HDRP(bp)));    
    }

    fprintf(stderr, "=====debugger END=====\n");
    
}

static void heap_debugger2() {
    void *bp;
    
    fprintf(stderr, "===== heap_debugger2 =====\n");
    
    for (int i = 0; i < LISTLIMIT; i++) {
        fprintf(stderr, "[List %d] start: %p\n", i, free_list[i]);
        
        for (bp = free_list[i]; bp != NULL; bp = SUCC_FREEP(bp)) {
            size_t size = GET_SIZE(HDRP(bp));
            int correct_idx = find_list_index(size);

            // 올바른 리스트에 들어있는지 검사
            if (i != correct_idx) {
                fprintf(stderr, "ERROR: bp %p (size %zu) in list %d, but should be in list %d\n",
                        bp, size, i, correct_idx);
                exit(1);
            }

            fprintf(stderr, "  -> bp: %p, GET_SIZE(HDRP(bp)): %zu, GET_ALLOC(HDRP(bp)): %ld, SUCC: %p\n", 
                bp, size, GET_ALLOC(HDRP(bp)), SUCC_FREEP(bp));
        }
    }

    fprintf(stderr, "===== debugger END =====\n");
}


// 블록 사이즈에 따라 몇 번째 리스트에 들어갈지 결정
static int find_list_index(size_t size) {
    int idx = 0;
    size_t temp = size;
    while (idx < LISTLIMIT - 1 && temp > 1) {
        temp >>= 1; // 2로 계속 나누기
        idx++;
    }
    return idx;
}

void PUTfree(void*bp, size_t size){

    int idx = find_list_index(size);
    void *head = free_list[idx];

    SUCC_FREEP(bp) = head;
    PREC_FREEP(bp) = NULL;
    if (head != NULL)
        PREC_FREEP(head) = bp;
    free_list[idx] = bp;

    // int idx = 0;
    // void *search_ptr = bp;
    // void *insert_ptr = NULL;
    
    // idx=find_list_index(size);

    // search_ptr = free_list[idx];
    // while ((search_ptr != NULL) && (GET_SIZE(HDRP(search_ptr)) < size)) {
    //     insert_ptr = search_ptr;
    //     search_ptr = SUCC_FREEP(search_ptr); 
    // }

    
    // // Set predecessor and successor
    // if (search_ptr != NULL) {
    //     if (insert_ptr != NULL) {
    //         SET_PTR(PRED_PTR(bp), search_ptr);
    //         SET_PTR(SUCC_PTR(search_ptr), bp);
    //         SET_PTR(SUCC_PTR(bp), insert_ptr);
    //         SET_PTR(PRED_PTR(insert_ptr), bp);
    //     } else {
    //         SET_PTR(PRED_PTR(bp), search_ptr);
    //         SET_PTR(SUCC_PTR(search_ptr), bp);
    //         SET_PTR(SUCC_PTR(bp), NULL);
            
    //         /* Add block to appropriate idx */
    //         free_list[idx] = bp;
    //     }
    // } else {
    //     if (insert_ptr != NULL) {
    //         SET_PTR(PRED_PTR(bp), NULL);
    //         SET_PTR(SUCC_PTR(bp), insert_ptr);
    //         SET_PTR(PRED_PTR(insert_ptr), bp);
    //     } else {
    //         SET_PTR(PRED_PTR(bp), NULL);
    //         SET_PTR(SUCC_PTR(bp), NULL);
            
    //         /* Add block to appropriate idx */
    //         free_list[idx] = bp;
    //     }
    // }
    
    // return;
}



//free_list에서 할당상태 된 블록 빼기 
void Popfree(void*bp){
    size_t size=GET_SIZE(HDRP(bp));
    int idx = find_list_index(size);
    void *prev = PREC_FREEP(bp);
    void *next = SUCC_FREEP(bp);

    if (prev != NULL)
        SUCC_FREEP(prev) = next;
    else
        free_list[idx] = next;
    if (next != NULL)
        PREC_FREEP(next) = prev;


    // int idx = 0;
    // size_t size = GET_SIZE(HDRP(bp));
    // // fprintf(stderr, "cur block %p size status: %ld\n", bp, size);
    
    // /* Select segregated list */
    // idx=find_list_index(size);
    // if (PREC_FREEP(bp) != NULL) {
    //     if (SUCC_FREEP(bp) != NULL) {
    //         SET_PTR(SUCC_PTR(PREC_FREEP(bp)), SUCC_FREEP(bp));
    //         SET_PTR(PRED_PTR(SUCC_FREEP(bp)), PREC_FREEP(bp));
    //     } else {
    //         SET_PTR(SUCC_PTR(PREC_FREEP(bp)), NULL);
    //         free_list[idx] = PREC_FREEP(bp);
    //     }
    // } else {
    //     if (SUCC_FREEP(bp) != NULL) {
    //         SET_PTR(PRED_PTR(SUCC_FREEP(bp)), NULL);
    //     } else {
    //         free_list[idx] = NULL;
    //     }
    // }
    
    // return;
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
        return bp;
    }
    
    //case 2.다음 블록 가용상태 
    else if(prev_alloc&&!next_alloc){
        Popfree(next_bp);
        Popfree(bp);
        size+=GET_SIZE(HDRP(next_bp)); //다음 블록 크기만큼 증가
        PUT(HDRP(bp), PACK(size,0));
        PUT(FTRP(bp), PACK(size,0)); 
    }
    
    //case 3. 이전 블록 가용상태 
    else if(!prev_alloc && next_alloc){
        void *prev_bp=PREV_BLKP(bp);
        Popfree(bp);
        Popfree(prev_bp);
        size+=GET_SIZE(HDRP(prev_bp));

        //헤더랑 풋터 업뎃
        PUT(HDRP(prev_bp), PACK(size,0));
        PUT(FTRP(bp), PACK(size,0));
        bp=prev_bp; //bp는 이전 블록의 bp로
        
    }
    
    //case 4. 전후 블록 가용상태
    else{
        void *prev_bp=PREV_BLKP(bp);
        Popfree(bp);
        Popfree(prev_bp);
        Popfree(next_bp);
        size+=(GET_SIZE(HDRP(next_bp)) + GET_SIZE(HDRP(prev_bp)));
        PUT(HDRP(prev_bp), PACK(size,0));
        PUT(FTRP(next_bp), PACK(size,0));
        bp=prev_bp;
        
    }
    PUTfree(bp,size);//병합된 블록 삽입

    return bp;

}

static void *extend_heap(size_t words){
    void *bp;
    size_t size;

    size=(words%2) ? (words+1) *WSIZE : words*WSIZE;
    size=ALIGN(size);
    if((void*)(bp = mem_sbrk(size))==(void*)-1){
        return NULL;
    }
    
    //가용 블록의 헤더풋터와 에필로그 블록의 헤더를 초기화.
    PUT(HDRP(bp), PACK(size,0)); //가용 블록 헤더 초기화
    PUT(FTRP(bp), PACK(size,0)); //가용 블록 풋터 초기화
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1)); //에필로그 블록 헤더 초기화

    PUTfree(bp, size);
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
    int idx;

    for(idx=0; idx<LISTLIMIT; idx++){
        free_list[idx]=NULL;
    }

    heap_listp = mem_sbrk(8*WSIZE);

    if(heap_listp==(void*)-1){
        // perror("BP 1");
        return -1;
    }

    PUT(heap_listp,0);
    PUT(heap_listp + (1*WSIZE),PACK(DSIZE,1));
    PUT(heap_listp + (2*WSIZE),PACK(DSIZE,1));
    PUT(heap_listp + (3*WSIZE),PACK(MINBLOCKSIZE,0));//맨처음에 적장한 블럭을 하나 만들고 시작할것!!!
    PUT(heap_listp + (4*WSIZE),NULL);
    PUT(heap_listp + (5*WSIZE),NULL);
    PUT(heap_listp + (6*WSIZE),PACK(MINBLOCKSIZE,0));
    PUT(heap_listp + (7*WSIZE),PACK(0,1));
    heap_listp = NEXT_BLKP(heap_listp); //가용블록의 블록 포인터

    if(extend_heap(CHUNKSIZE/WSIZE)==NULL){
        return -1;
    }

    return 0;
}


void *find_fit(size_t asize) {

    int idx = find_list_index(asize);
    void *best_bp = NULL;
    size_t best_size = (size_t)-1; // 매우 큰 값으로 초기화

    // idx부터 LISTLIMIT-1까지 모든 리스트에서 best-fit 탐색
    for (; idx < LISTLIMIT; idx++) {
        void *bp = free_list[idx];
        for (; bp != NULL; bp = SUCC_FREEP(bp)) {
            size_t bsize = GET_SIZE(HDRP(bp));
            if (bsize >= asize) {
                if (bsize == asize) {
                    // 완벽하게 맞는 블록 발견 (early return)
                    return bp;
                }
                if (bsize < best_size) {
                    best_size = bsize;
                    best_bp = bp;
                }
            }
        }
        // best-fit은 모든 리스트를 다 탐색해야 하므로 break 없음
    }
    return best_bp; // 없으면 NULL 반환




    // int idx = find_list_index(asize);
    // void *best_bp = NULL;
    // size_t best_size = (size_t)-1; // 매우 큰 값으로 초기화

    // // idx부터 LISTLIMIT-1까지 모든 리스트에서 best-fit 탐색
    // for (; idx < LISTLIMIT; idx++) {
    //     void *bp = free_list[idx];
    //     for (; bp != NULL; bp = SUCC_FREEP(bp)) {
    //         size_t bsize = GET_SIZE(HDRP(bp));
    //         if (bsize >= asize) {
    //             if (bsize == asize) {
    //                 // 완벽하게 맞는 블록 발견 (early return)
    //                 return bp;
    //             }
    //             if (bsize < best_size) {
    //                 best_size = bsize;
    //                 best_bp = bp;
    //             }
    //         }
    //     }
    //     // best-fit은 모든 리스트를 다 탐색해야 하므로 break 없음
    // }
    // return best_bp; // 없으면 NULL 반환
}



static void place(void *bp, size_t asize){
    
    size_t csize = GET_SIZE(HDRP(bp));
    size_t remainder = csize - asize;
    
    Popfree(bp);

    if (remainder >= MINBLOCKSIZE) {
        // 블록 분할 가능: 앞쪽에 할당, 뒤쪽에 가용 블록
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        void *next_bp = NEXT_BLKP(bp);
        PUT(HDRP(next_bp), PACK(remainder, 0));
        PUT(FTRP(next_bp), PACK(remainder, 0));
        PUTfree(next_bp, remainder);
    } else {
        // 블록 분할 불가: 
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
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
    void *bp;
    
    if(heap_listp==NULL){
        mm_init();
    }
    
    if(size==0) {
        return NULL;
    }
    
    newsize = MAX(ALIGN(size + WSIZE * 2), MINBLOCKSIZE);
    
    bp=find_fit(newsize);
    //적합한 가용 블록 탐색
    if(bp!=NULL){
        place(bp, newsize);
        return bp;
    }
    
    //가용 블록 못찾으면 확장 
    extendsize = MAX(newsize, CHUNKSIZE);
    if((bp=extend_heap(extendsize/WSIZE))==NULL){
        return NULL;
    }

    place(bp, newsize);
    
    // heap_debugger();
    // heap_debugger2();
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

    PUTfree(ptr, size);
    coalesce(ptr);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */



void *mm_realloc(void *ptr, size_t size)
{

    void *oldptr = ptr;
    void *newptr;
    size_t oldsize= GET_SIZE(HDRP(oldptr));
    
    if(size==0){
        mm_free(ptr);
        return NULL;
    }

    if(ptr==NULL)
        return(mm_malloc(size));

    if((int)size<0) return NULL;    

    // 요청 사이즈를 블록 최소 크기 단위로 정렬
    size_t asize = MAX(DSIZE*((size + DSIZE + DSIZE-1)/DSIZE), MINBLOCKSIZE);

    //분할 가능하면 분할 
    if(asize<=oldsize){
        size_t remainder = oldsize - asize;
        if(oldsize-asize>=MINBLOCKSIZE){
            // 블록 분할
            fprintf(stderr, "case 1\n");
            PUT(HDRP(oldptr), PACK(asize, 1));
            PUT(FTRP(oldptr), PACK(asize, 1));
            void *split_bp = NEXT_BLKP(oldptr);
            PUT(HDRP(split_bp), PACK(remainder, 0));
            PUT(FTRP(split_bp), PACK(remainder, 0));
            coalesce(split_bp);
            fprintf(stderr, "case 1 END \n");

        }
        return ptr;
    }

    //다음 블록이 free이고 합쳐서 충분하면 in-place 확장
    void *next = NEXT_BLKP(oldptr);
    if (!GET_ALLOC(HDRP(next))) {
        size_t combined_size = oldsize + GET_SIZE(HDRP(next));
        if (combined_size >= asize) {
            Popfree(next);
            size_t remainder = combined_size - asize;
            if (remainder >= MINBLOCKSIZE) {
                PUT(HDRP(oldptr), PACK(asize, 1));
                PUT(FTRP(oldptr), PACK(asize, 1));
                void *split_bp = NEXT_BLKP(oldptr);
                PUT(HDRP(split_bp), PACK(remainder, 0));
                PUT(FTRP(split_bp), PACK(remainder, 0));
                PUTfree(split_bp, remainder);
                coalesce(split_bp);
            } else {
                PUT(HDRP(oldptr), PACK(combined_size, 1));
                PUT(FTRP(oldptr), PACK(combined_size, 1));
            }
            return oldptr;
        }
    }
    

    //그 외
    newptr = mm_malloc(size);
    if (newptr == NULL) return NULL;
    if (size < oldsize) oldsize = size; //원래 size가 요청사이즈보다 크면, 원래 사이즈를 요청사이즈로 
    memcpy(newptr, oldptr, oldsize); //oldptr중 copySize만큼을 newptr에 복사. 
    mm_free(oldptr); //oldptr을 반환.

    return newptr;

}