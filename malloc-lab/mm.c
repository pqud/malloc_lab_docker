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

/** macros */
// basic ocnstants and macros
#define WSIZE 4 // word and header/footer size(bytes)
#define DSIZE 8 // double word size (bytes)
#define CHUNKSIZE (1<<12) // extend heap by this amount (bytes)

#define MAX(x, y) ((x) > (y)? (x) : (y))

// pack a size and allocated bit into a word
#define PACK(size, alloc) ((size) | (alloc))

// read and write a word at address p
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p)=(val))

// read the size and allocate fields from address p
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

// Given block ptr bp, compute address of its header and footer
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

// Given block ptr bp, compute address of next and previous blocks
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* payload 영역에 실제 포인터를 저장/읽기 위한 매크로 */
#define GET_PTR(p)      (*(void **)(p))
#define PUT_PTR(p, val) (*(void **)(p) = (void *)(val)) 

/* explicit free list 전용 */
#define GET_PRED(bp)    GET(bp)
#define GET_SUCC(bp)    GET_PTR((char *)(bp) + WSIZE)
#define PUT_PRED(bp,val) PUT_PTR(bp, val)
#define PUT_SUCC(bp,val) PUT_PTR((char *)(bp) + WSIZE, val)

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
	// create the initial empty heap
	if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
		return -1;
	PUT(heap_listp, 0); // alignment padding
	PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); // prologue header
	PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); // prologue footer
	PUT(heap_listp + (3*WSIZE), PACK(0, 1)); // Epliogue header
	heap_listp += 2*WSIZE;
	free_listp = NULL;
	node_num = 0;
	if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
		return -1;
	assert((size_t)(GET_SUCC(free_listp))%DSIZE == 0 || GET_SUCC(free_listp) == NULL); 
	assert(heap_listp + DSIZE == free_listp);  // 프롤로그 헤더와 중간의 프리 블럭 헤더 공간 = DSIZE
	assert(((size_t)(GET_PRED(free_listp))%DSIZE == 0) || GET_PRED(free_listp) == NULL);
	return 0;
}
	
static void *extend_heap(size_t words)
{
	char *bp;
	size_t size;
	
	// allocate an even number of words to maintain alignment
	size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
	assert(size >= 2*DSIZE);
	if ((long)(bp = mem_sbrk(size)) == -1)
		return NULL;
	
	// initialize free block header / footer and the epilogue header
	PUT(HDRP(bp), PACK(size, 0)); // free block header
	PUT(FTRP(bp), PACK(size, 0)); // free block footer
	PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); // new epilogue header
	
	// coalesce if the previous block was free
	return coalesce(bp);
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
	size_t asize; // adjusted block size
	size_t extendsize; // amount to extend heap if no fit
    char *bp;
	if (free_listp != NULL)  assert(GET_PRED(free_listp) == NULL);
	
	// ignore spurious requests
	if (size == 0)
		return NULL;
	
	// adjust block size to include overhead and alignment reqs.
	if (size < DSIZE) //최소 로직
		asize = 2*DSIZE;
	else // 반올림 로직
		asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);
	assert(asize > 2*DSIZE); // asize가 충분히 큰가. (pred, succ, hdr, ftr 각각 합쳐서 16바이트는 돼야 함.)
	// search the free list for a fit
	if ((bp = find_fit(asize)) != NULL) {
		place(bp, asize);
		return bp;
	}
	
	// no fit found. get more memory and place the block
	extendsize = MAX(asize, CHUNKSIZE);
	if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
		return NULL;
	place(bp, asize);
	return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
	size_t size = GET_SIZE(HDRP(bp));
	
	PUT(HDRP(bp), PACK(size, 0));
	PUT(FTRP(bp), PACK(size, 0));
	coalesce(bp);
	// assert((size_t)(GET_SUCC(bp))%4 == 0 || GET_SUCC(bp) == NULL); 
	// assert(((size_t)(GET_PRED(bp))%DSIZE == 0) || GET_PRED(bp) == NULL); 
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

static void *coalesce(void *bp)
{
	size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
	size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
	size_t size = GET_SIZE(HDRP(bp));
	if (prev_alloc && next_alloc) { //  case 1. 전부 할당된 경우.
		insert_node(bp); 
		assert((size_t)(GET_SUCC(bp))%DSIZE == 0 || GET_SUCC(bp) == NULL); 
		assert(((size_t)(GET_PRED(bp))%DSIZE == 0) || GET_PRED(bp) == NULL); 
		return bp;
	}
	
	else if (prev_alloc && !next_alloc) { // case 2. 뒤에 친구가 할당 안 된 경우
		assert((size_t)(GET_SUCC(NEXT_BLKP(bp)))%DSIZE == 0 || GET_SUCC(NEXT_BLKP(bp)) == NULL);  
		assert(((size_t)(GET_PRED(NEXT_BLKP(bp)))%DSIZE == 0) || GET_PRED(NEXT_BLKP(bp)) == NULL); 
		remove_node(NEXT_BLKP(bp));
		size += GET_SIZE(HDRP(NEXT_BLKP(bp))); // 뒤에 친구만큼 사이즈 키워주고
		PUT(HDRP(bp), PACK(size, 0)); // 업데이트 된 사이즈를 토대로 헤더와 푸터 업데이트.
		PUT(FTRP(bp), PACK(size, 0));
		insert_node(bp);
	}
	else if(!prev_alloc && next_alloc) { // case 3.
		assert((size_t)(GET_SUCC(PREV_BLKP(bp)))%DSIZE == 0 || GET_SUCC(PREV_BLKP(bp)) == NULL);  
		assert(((size_t)(GET_PRED(PREV_BLKP(bp)))%DSIZE == 0) || GET_PRED(PREV_BLKP(bp)) == NULL); 
		remove_node(PREV_BLKP(bp));
		size += GET_SIZE(HDRP(PREV_BLKP(bp)));
		PUT(FTRP(bp), PACK(size, 0));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
		bp = PREV_BLKP(bp);
		insert_node(bp);
	}
	
	else {
		
		assert((size_t)(GET_SUCC(PREV_BLKP(bp)))%DSIZE == 0 || GET_SUCC(PREV_BLKP(bp)) == NULL);  
		assert(((size_t)(GET_PRED(PREV_BLKP(bp)))%DSIZE == 0) || GET_PRED(PREV_BLKP(bp)) == NULL); 
		remove_node(PREV_BLKP(bp));
		
		assert((size_t)(GET_SUCC(NEXT_BLKP(bp)))%DSIZE == 0 || GET_SUCC(NEXT_BLKP(bp)) == NULL);  
		assert(((size_t)(GET_PRED(NEXT_BLKP(bp)))%DSIZE == 0) || GET_PRED(NEXT_BLKP(bp)) == NULL);
		remove_node(NEXT_BLKP(bp));

		size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
		// size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp))); 이렇게 해더로 뒤에 친구 사이즈를 알 수도 있지 않나?
		PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
		PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
		bp = PREV_BLKP(bp);
		insert_node(bp);
	}
	assert((size_t)(GET_SUCC(bp))%DSIZE == 0 || GET_SUCC(bp) == NULL); 
	assert(((size_t)(GET_PRED(bp))%DSIZE == 0) || GET_PRED(bp) == NULL); 
	assert(free_listp == bp);
	return bp;
}

static void *find_fit(size_t asize)
{
    // first fit search
    void *bp;

    for (bp = free_listp; bp != NULL; bp = GET_SUCC(bp))
    {
		assert(bp != NULL);
        if (GET_SIZE(HDRP(bp)) >= asize)
        {
			assert(free_listp != NULL); //리스트가 빈 값이어서는 안 됨
			return bp;
        }
    }
    return  NULL;
}

/**
 * feature: place the node at certain available block pointed by bp. 
 * split the block if it's dividable.
 * 
 * invariatns
 */
static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));
	assert((size_t)(GET_SUCC(bp))%DSIZE == 0 || GET_SUCC(bp) == NULL); 
	assert(((size_t)(GET_PRED(bp))%DSIZE == 0) || GET_PRED(bp) == NULL);
	remove_node(bp);
    if ((csize - asize) >= (2*DSIZE))
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize, 0));
        PUT(FTRP(bp), PACK(csize-asize, 0));
		insert_node(bp);
		assert((size_t)(GET_SUCC(bp))%DSIZE == 0 || GET_SUCC(bp) == NULL); 
		assert(((size_t)(GET_PRED(bp))%DSIZE == 0) || GET_PRED(bp) == NULL);
    }
    else
    {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}



/**
 * feature: insert current block pointed by bp into the free list
 * 
 * invariants
 * 1. 기존 free_listp의 predecessor가 bp가 돼야 함. => assert(GET_PRED(tmp) == bp);
 */
static void insert_node(void *bp)
{
	if(free_listp != NULL)
	{
		// PUT_PRED(free_listp, bp); 
		GET_PRED(free_listp) = (void *) bp;
		assert(GET_PRED(free_listp) != NULL);
	}
	PUT_SUCC(bp, free_listp); // 여기서도 다 검사하자.
	// PUT_PRED(bp, NULL);
	GET_PRED(bp) = (void *)NULL;
	if(free_listp != NULL) assert(GET_SUCC(bp) != NULL);
	free_listp = bp;
	assert(free_listp == bp);
	*node_num++;
	// bp를 기준으로 bp의 전후가 null혹은 align값인지 확인 -> 여긴 또 잘 패스한다.
	// 아무튼 이 불변식 괜찮네.
	assert((size_t)(GET_SUCC(bp))%DSIZE == 0 || GET_SUCC(bp) == NULL); 
	assert(((size_t)(GET_PRED(bp))%DSIZE == 0) || GET_PRED(bp) == NULL); 
	
	// print_list();
}


/**
 * feature: removes the node pointed by bp
 * 
 * invariants 이 친구를 좀 빡세게 봐야겠다.
 * 1. final p_bp and s_bp should connected  => GET_SUCC(p_bp) == s_bp & GET_PRED(s_bp) == p_bp
 * 2. ㅇㅇ
 */
static void remove_node(void *bp)
{
	assert(((size_t)bp % DSIZE) == 0);
	void *p_bp = GET_PRED(bp); // 이 값들이 coalesce에서 호출된 거면 안에 있는 값들은 절대 유효하지 않음. 갓 프리된 애들이니까. 이 로직이 잘못 됐네.
	void *s_bp = GET_SUCC(bp);
	assert((size_t)(GET_SUCC(bp))%DSIZE == 0 || GET_SUCC(bp) == NULL); 
	assert(((size_t)(GET_PRED(bp))%DSIZE == 0) || GET_PRED(bp) == NULL); 

	if (p_bp != NULL){ 
		PUT_SUCC(p_bp, s_bp);  //아 여기서 sbp가 이상한 값인데도 들어가서 그렇구나
	} 
	else{
		free_listp = s_bp;
	}
	if(s_bp != NULL){
		//여기서 assert 뭘 하면 좋을까?

		assert((void*)0xffff00000000UL != s_bp);
		PUT_PRED(s_bp, p_bp);
	}
	// 만약 p_bp, s_bp가 각각 존재한다면 서로 잘 이어졌는지 확인
	if(p_bp != NULL) assert(GET_SUCC(p_bp) == s_bp); //어느 시점에 PUT_PRED(sbp, p_bp)에서 잘린 값이 들어감
	//null이라면? p_bp가 없다는 말은 뭐지? bp의 전임자가 처음부터 없다는 뜻->삭제하던 블럭이 마지막이었다. 
	// 따라서, s_bp와 free_listp가 일치하는지 봐야 한다. 왜? bp의 후임자가 리스트 최상단이니까. 
	else assert(s_bp == free_listp);

	if(s_bp != NULL) assert(GET_PRED(s_bp) == p_bp);
	// 후임자가 처음부터 없었다는 말은 리스트가 완전히 비었다는 것. 따라서 free_listp가 null인지 체크해야 함.
	else assert(free_listp == NULL);
	// print_list();
	*node_num--;
}

static void print_list(void){
	void *bp;
	for(bp = free_listp; bp != NULL; bp = GET_SUCC(bp)){
		fprintf("%p %u %u %p -> ", GET_PRED(bp),  GET_SIZE(HDRP(bp)), GET_ALLOC(HDRP(bp)) ,GET_SUCC(bp));
	}
	fprintf("end\n", NULL);

}

