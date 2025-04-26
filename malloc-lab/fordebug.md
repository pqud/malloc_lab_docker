디버깅 기록용.


mdriver를 확인했을 때, mm_malloc은 항상 실행된다.

Checking mm_malloc for correctness, ERROR [trace 0, line 5]: mm_malloc failed.

이 에러는 malloc의 반환값의 사이즈가 0일때 발생했음. 
실제로 mm_malloc retrun 100; 하니까 안뜨네.
근데 이러면 init에서 에러가 뜸. extend_heap 에서 malloc이랑 연관되서 그런듯 

그럼 일단 순서는 mm_malloc -> init 순서인듯 
엥?
아


```C
switch (trace->ops[i].type)
	{
	case ALLOC: /* mm_malloc */

				/* Call the student's malloc */
				if ((p = mm_malloc(size)) == NULL)
				{
					malloc_error(tracenum, i, "mm_malloc failed.");
					return 0;
				}
		...
	}
``` 
mdriver.c 646줄 


디버깅하려고 mdriver.c 파일을 수정했는데 적용이 안된다. 바뀌면 make 했을 때 변경점을 인식하는것 같긴 한데 적용이안댐;
일단 mm_malloc으로 넘겨주는게 int형임.


case 2:
여기서 에러 발생
```C
 //인접 2워드 배수로 반올림.
    size = ALIGN(words * WSIZE);
    if((long)(bp = mem_sbrk(size))==-1){
        perror("BP 6");
        return NULL;
    }

```

malloc->init->extend->coalesce
이럼 여기서 생기는 문제가 아닌데
테케 한번에 malloc을 여러번 시도하는?


찾았다.
place에서 블록 분할이 불가능할 때 세그먼트 오류 발생함.
아니
못찾았는데 왜 place로 넘어감? 못찾으면 확장해야되는디 

아니야
find_fit에서 찾지 못했을 때, bp를 반환한 다음 이 bp에 접근하려고 하면 에러가 뜨는거 같은데? 맞네 

엥 find_fit에서 뜨는게 아니네? ㅋㅋ; extend_heap에서 뜨나벼...
ㅇㄴ coalesce문제였다니 충격

이전 블록이 가용상태일 때는 풋터가 존재하지 않음. 

병합할 때, 현재 블록을 free할 거고, 전 or 후 블록 중 가용상태인게 있으면 합쳐주는게 목적인디...
1이고 앞뒤가 0인 경우가 하나도 없는데 어찌된일;
뭔데 이상한데? free의 문제인가? 

병합하는데 이전블록 꼬라지가 왜이래 
아니 그리고 다음 블록이 4096이면 현재 블록이 16인데 얘 앞블록은 1이잖아 
ㅇㄴ 싸우자는거냐 



----------
해결했음 

#define PREV_BLKP(bp) ((char *) (bp) - GET_SIZE((char *)(bp) -DSIZE))

여기에 문제가 있었어
----------------
새로운 문제가 있음
payload가 다른 payload를 덮어쓰는 문제임.
버퍼 오버플로우 문제. 
계속계속 heap을 extend 해서 그른가?
--------------
다시 segmentation fault뜬다.
어디가 문제지 
일단 find_fit 문제는 아닌것같은데 
분할 가능했고 

아젠장
역시 분할문제였어 
젠장또분할이야 

왜 또 
fprintf(stderr, "FTRP(PREV_BLKP(bp)): %p\n", (void *)FTRP(PREV_BLKP(bp)));

여기서 오류나냐 
아 그치 풋터 없을 수 있지...

int prev_alloc = GET_ALLOC(HDRP(PREV_BLKP(bp)));  
    fprintf(stderr, "prev_alloc: %d\n", prev_alloc);

이게 왜 에러가 나냐 

일반적으로, 올바른 블록 경계와 포인터(bp)가 보장된다면 이전 블록의 헤더는 항상 존재해야 합니다.

그런데 왜 안되냐고젠장~~~` 

```C
#define PREV_BLKP(bp) ((char *) (bp) - GET_SIZE(((char *)(bp) -DSIZE)))
```

여기서 
((char *)(bp) -DSIZE)
는 현재 블록의 풋터 위치임.

GET_SIZE(((char *)(bp) -DSIZE))
는 풋터에 저장된 "이전 블록의 크기"를 읽어옴.



이전 블록자체는 읽어올 수 있는데,
이전 블록의 할당 비트는 읽을 수가 없다고??? 말이되냐 이게 

방어 코드 추가함.
------------------

이제 segementaion fault는 발생안하는데
모든 테케에서 overlaps가 발생함.

가능성
1. 동일한 주소에 두 번 이상 할당
2. 블록 경계 계산 오류
	크기 계산, 해더/풋터 처리, 정렬 처리 등에서 payload의 시작/끝 주소 계산이 잘못됨 
	* 블록 분할(place)시 남은 블록 주소 계산 오류
	* 블록 크기(헤더+payload+풋터) 계산에서 실수
	* 할당/해제 시 bp이동 잘못됨
3. 버퍼 오버플로우
	한 블록의 payload에 할당 크기보다 더 많은 데이터를 써서
4. 중복 free 

------------
추가로 coalesce에서 free하고 연결 끊는건 명시적 연결 리스트 방식이어서 그랬던 모양.
지금은 묵시적 연결 리스트 방식도 못하고있으니까...
일단 이것부터 해내고 나서 해보자...


extend도 되고, coal도 되긴하는데 일단 마지막에
bp: 0x7efd5e159f90, GET_SIZE(HDRP(bp)): 120  GET_ALLOC(HDRP(bp)): 1
bp: 0x7efd5e15a008, GET_SIZE(HDRP(bp)): 24  GET_ALLOC(HDRP(bp)): 1
bp: 0x7efd5e15a020, GET_SIZE(HDRP(bp)): 4096  GET_ALLOC(HDRP(bp)): 0
NEXT_BLKP(bp): 0x7efd5e15b020
PREV_BLKP(bp): 0x7efd5e15a008
원래 디버그 이렇게 끝났음. 한 번 보자

p: 0x7f35ac387f78, GET_SIZE(HDRP(bp)): 24  GET_ALLOC(HDRP(bp)): 1
bp: 0x7f35ac387f90, GET_SIZE(HDRP(bp)): 120  GET_ALLOC(HDRP(bp)): 1
bp: 0x7f35ac388008, GET_SIZE(HDRP(bp)): 4120  GET_ALLOC(HDRP(bp)): 0

엥?
extend=>malloc=>free=>col 이런식이겠지만... 
malloc한 이후에 마지막 주소는 0x7efd5e15a020 여기였고
여기서 120만큼 넣었음.
그러면 주소가 왜.... 4120임? 왜 24만큼 늘어났지

흐름 그대로 따라가보면,

120할당하려고 find_fit 했지만 자리가 없어서 NULL 반환 . 
=> extend heap
bp: 0x7f35ac388020, GET_SIZE(HDRP(bp)): 4096  GET_ALLOC(HDRP(bp)): 0

=>확장하고 place함. 블록은 분할 가능하니까, 
=>이후에 col함. 
bp: 0x7f35ac387f90, GET_SIZE(HDRP(bp)): 120  GET_ALLOC(HDRP(bp)): 1
bp: 0x7f35ac388008, GET_SIZE(HDRP(bp)): 4120  GET_ALLOC(HDRP(bp)): 0

분할 이후 블록 남은게 이상한데? 
이전 블록은 분명히 가용상태가 아닌데, 왜 가용상태라고 그러냐 

디버깅
----------------------

bp: 0x7fe8dad5c2f8, GET_SIZE(HDRP(bp)): 4080  GET_ALLOC(HDRP(bp)): 1
bp: 0x7fe8dad5d2e8, GET_SIZE(HDRP(bp)): 4080  GET_ALLOC(HDRP(bp)): 1
bp: 0x7fe8dad5e2d8, GET_SIZE(HDRP(bp)): 4080  GET_ALLOC(HDRP(bp)): 1

free(0x7fe8dad5d2e8)
=>case 1

bp: 0x7fe8dad5c2f8, GET_SIZE(HDRP(bp)): 4080  GET_ALLOC(HDRP(bp)): 1
bp: 0x7fe8dad5d2e8, GET_SIZE(HDRP(bp)): 4080  GET_ALLOC(HDRP(bp)): 0
bp: 0x7fe8dad5e2d8, GET_SIZE(HDRP(bp)): 4080  GET_ALLOC(HDRP(bp)): 1

그 다음

bp: 0x7fe8dad59280, GET_SIZE(HDRP(bp)): 168  GET_ALLOC(HDRP(bp)): 1
bp: 0x7fe8dad59328, GET_SIZE(HDRP(bp)): 4080  GET_ALLOC(HDRP(bp)): 1
bp: 0x7fe8dad5a318, GET_SIZE(HDRP(bp)): 4080  GET_ALLOC(HDRP(bp)): 1

free(0x7fe8dad59328)

이거 왜 case 3임?




realloc
--------------

sbrk는 커널의 brk 포인터에 incr을 더해서 힙을 늘리거나 줄인다.

case 1은 incr이 음수인 경우. 이 경우는 구현하지 않아서 안됨.

case 2는 incr을 더했을 때 최대 mem_max_addr을 벗어나는 경우임.
이게 떴다는건 용량 초과라는 건데... free를 제대로 못하고있는지?

```oldptr 0x7fd4f182af88
newptr 0x7fd4f182ea48
copySize 15040
size 15032
```

마지막에 뜨는게 이건데, 이거 이후에 malloc 시도하다가 에러뜨는거란 말이지 

아니 이거 realloc만 디버깅 못하냐?
아니
ㅋㅋㅋㅋㅋㅋㅋㅋㅋㅋ
또 col 너야? 
제발 좀 합치라고
!!!!!!!!!!!!!!!!!!!!!!

대체

```
bp: 0x7fa9f9701018, GET_SIZE(HDRP(bp)): 8  GET_ALLOC(HDRP(bp)): 1
bp: 0x7fa9f9701020, GET_SIZE(HDRP(bp)): 24  GET_ALLOC(HDRP(bp)): 1
bp: 0x7fa9f9701038, GET_SIZE(HDRP(bp)): 4072  GET_ALLOC(HDRP(bp)): 0
bp: 0x7fa9f9702020, GET_SIZE(HDRP(bp)): 4104  GET_ALLOC(HDRP(bp)): 0
bp: 0x7fa9f9703028, GET_SIZE(HDRP(bp)): 4112  GET_ALLOC(HDRP(bp)): 0
```

이게 무슨 개같은꼬라지야 ?????????

아니 봐바
oldptr 0x7fb5639d0f88
newptr 0x7fb5639d4a48
copySize 15040
size 15032
free start 0x7fb5639d0f88
coalesce 1
free start 0x7fb5625db038
coalesce 2
HEAP_DEBUGGER
bp: 0x7fb5625db038
bp: 0x7fb5625db018, GET_SIZE(HDRP(bp)): 8  GET_ALLOC(HDRP(bp)): 1
bp: 0x7fb5625db020, GET_SIZE(HDRP(bp)): 24  GET_ALLOC(HDRP(bp)): 1
bp: 0x7fb5625db038, GET_SIZE(HDRP(bp)): 4072  GET_ALLOC(HDRP(bp)): 0
bp: 0x7fb5625dc020, GET_SIZE(HDRP(bp)): 4104  GET_ALLOC(HDRP(bp)): 0

이게 말이안됨
0x7fb5639d0f88를 먼저 free 했는데 왜 0x7fb5625db038가 먼저 coal에 들어가느냐고 
그리고 내가 원래 의도했던것도 88이 맞음. 38은 뭔데?
??????????
0x7fb5625db038 이거 뭐임?
왜 free를 호출하면 계속계속계속 얘만 free되냐
정작 내가 호출한 oldptr은 free되지 않고


아 그치? bp는 이후에 바뀌니까
아니 뭐 그건 그렇다고 쳐
그런데 왜 coal 작업 이후인 bp의 값이 모두 같은데?
그리고 그래서 왜 free가 두번 호출되냐고 ㅋ; 
아 아닌가?
두번째 free가 무시되는건가 지금 0x7fb5625db038이게 무시되는거같기도하고....
아니 근데 그래요 그래서 왜 bp가 계속동일한데요 ㅋ;


발견한 특이점
1. coalesce는 1 또는 2만 출력됨. 
	전후 블록이 모두 할당상태거나, 다음 블록이 가용상태라는 의미. 
	다시 말해 이전 블록은 무조건 할당상태임. 
	malloc을 뒷쪽 빈 공간에서 하니까 당연할지도
	이래서 bp는 계속 동일한 값이구나
	coal내부에서 bp값이 변하는 경우는 case 3 또는 4임.

	그래서 bp값이 안바뀌나보다
	oldptr을 만들고 바로 지우고 있잖아
	A뒤에 malloc 해서 만든 블럭을 newptr이라고 하자.
	oldptr은 직전의 newptr임.
	newptr의 앞 블럭은 A고.
	그러면 A의 주소인 0x7fb5625db038가 계속 bp로 남게되는거지 
	오케이 coal 이후 heap_debugger에서 0x7fb5625db038가 계속 나오는 이유는 납득함. 
	
	잠깐 다시 이해가 안가는데
	글면 왜 coal 1이 나올 수 있는거냐? coal2만 나와야하는거 아닌가
	내가 대체 무엇을 free하고 있는거지 

	엥?? 아니지 newptr 주소는 계속 다르잖아
	뭐야 아닌데
	 

	=>이러니까 coal이 잘 되고 잇는거 같기도 하고 아닌것 같기도 하고...

2. free가 두 번 출력되는데 이 값은 coal 이후 bp값이다.
	그리고 free로 지정되었는데도 불구하고 힙에는 계속 남아있음. 매번. 
	* 대체 이 free는 어디서 출력되는걸까
	realloc->free->realloc이라고 생각했는데 출력 순서보니까 그것도 아님. 이 순서대로였다면 heap_debugger가 출력된 다음 free 0x7fb5625db038 이랬어야 정상인데 그게 아니잖아

3. 계속 같은 패턴이 반복됨. 진짜 이상하네
	oldptr
	newptr
	copysize
	size
	free oldptr
	coal 1
	free ? <- 얘는 반복되는 값은 아님.. 근데 그래서 누구시냐구요 
	coal 1
	coal 1 <=그리고 여기서는 왜 heap_debugger안뜸? ㅋㅋㅋ; 환장~~~  
	------약간 거울 
	oldptr
	newptr
	copysize
	size
	free oldptr
	coal 1
	free 0x7fb5625db038 <-여기서 부터 달라지네 
	coal 2
	heap_debug

malloc이 계속 새로운걸 만드는건 아니지.
malloc하고 find_fit으로 찾으면 달라질테니까........



아
지금보니까 coal 1 경우엔 heap debugger가 안뜨는게 정상임
그러니까
realloc->free->realloc 으로 되어 있엇던거네 

여태 문제가 되었던 이유가
copysize가 잘못된 크기를 가리키고 있어서인것 같은데? 그래서 계속 분할된듯...???


free할 때 이상한데 
bp: 0x7f7337e10018, GET_SIZE(HDRP(bp)): 8  GET_ALLOC(HDRP(bp)): 1
bp: 0x7f7337e10020, GET_SIZE(HDRP(bp)): 520  GET_ALLOC(HDRP(bp)): 0
bp: 0x7f7337e10228, GET_SIZE(HDRP(bp)): 136  GET_ALLOC(HDRP(bp)): 1
bp: 0x7f7337e102b0, GET_SIZE(HDRP(bp)): 648  GET_ALLOC(HDRP(bp)): 1
bp: 0x7f7337e10538, GET_SIZE(HDRP(bp)): 2792  GET_ALLOC(HDRP(bp)): 0

여기서 0x7f7337e10228를 프리하면
이전 블록이 합쳐져야

아 합쳐진거냐? 아래에서 보니까 크기가 다르긴 하넴... 
아니근데 뭔지랄을했기래 사이즈가 이따구인것임; 



bp: 0x7f6ef0a47018, GET_SIZE(HDRP(bp)): 8  GET_ALLOC(HDRP(bp)): 1
bp: 0x7f6ef0a47020, GET_SIZE(HDRP(bp)): 136  GET_ALLOC(HDRP(bp)): 1
bp: 0x7f6ef0a470a8, GET_SIZE(HDRP(bp)): 384  GET_ALLOC(HDRP(bp)): 0
bp: 0x7f6ef0a47228, GET_SIZE(HDRP(bp)): 136  GET_ALLOC(HDRP(bp)): 1
bp: 0x7f6ef0a472b0, GET_SIZE(HDRP(bp)): 648  GET_ALLOC(HDRP(bp)): 1
bp: 0x7f6ef0a47538, GET_SIZE(HDRP(bp)): 2792  GET_ALLOC(HDRP(bp)): 0


여기서 0x7f6ef0a47228의 이전 블록 0x7f6ef0a470a8이 가용 블록으로 취급하여 두 블록을 병합해야하는데 병합하지 않고 있음. 

아놔 조건을 ||로 걸어놨네 미치겠다 ㅋㅋㅋ


----------------------
왜 realloc 고치니까 malloc이 안됨?
실화냐고 


또 니가 문제야 coal 이전 블록 접근

bp: 0x7fba9a8cb328, GET_SIZE(HDRP(bp)): 4080  GET_ALLOC(HDRP(bp)): 0
bp: 0x7fba9a8cc318, GET_SIZE(HDRP(bp)): 4080  GET_ALLOC(HDRP(bp)): 1
bp: 0x7fba9a8cd308, GET_SIZE(HDRP(bp)): 12240  GET_ALLOC(HDRP(bp)): 0

여기서 0x7fb379776308를 free하고 싶어 
다음 블록은? 오케이 떠 
이전 블록 꼬라지가 왜이럼 
장난하냐? 검색해도 안나오네 어디서오신누구신데요 ㅠㅠㅠㅠㅠㅠㅠ


bp: 0x7fa3b00d36f0, GET_SIZE(HDRP(bp)): 4096  GET_ALLOC(HDRP(bp)): 0
bp: 0x7fa3b00d46f0, GET_SIZE(HDRP(bp)): 4096  GET_ALLOC(HDRP(bp)): 1
bp: 0x7fa3b00d56f0, GET_SIZE(HDRP(bp)): 12288  GET_ALLOC(HDRP(bp)): 0

0x7fa3b00d46f0 
다음블록:0x7fa3b00d56f0 맞고
전블록:0xebec6b8f9bf922e0 딱봐도 아닌데 
ㅋ;



점수올리기기
----------------------


trace  valid  util     ops      secs  Kops
 0       yes   99%    5694  0.012693   449
 1       yes   99%    5848  0.010087   580
 2       yes   99%    6648  0.016460   404
 3       yes   99%    5380  0.013232   407
 4       yes   98%   14400  0.000195 73960
 5       yes   92%    4800  0.013204   364
 6       yes   91%    4800  0.012356   388
 7       yes   53%   12000  0.170567    70
 8       yes   47%   24000  0.430069    56
 9       yes   25%   14401  0.025700   560
10       yes   45%   14401  0.001085 13269
Total          77%  112372  0.705647   159

Perf index = 46 (util) + 11 (thru) = 57/100


조건문 바꾸고 나서-> 점수 오히려 떨어짐


Results for mm malloc:
trace  valid  util     ops      secs  Kops
 0       yes   99%    5694  0.010095   564
 1       yes   99%    5848  0.010542   555
 2       yes   99%    6648  0.023018   289
 3       yes   99%    5380  0.012389   434
 4       yes   98%   14400  0.000285 50491
 5       yes   92%    4800  0.013529   355
 6       yes   91%    4800  0.013866   346
 7       yes   53%   12000  0.118379   101
 8       yes   47%   24000  0.646553    37
 9       yes   25%   14401  0.057850   249
10       yes   45%   14401  0.003890  3702
Total          77%  112372  0.910396   123

Perf index = 46 (util) + 8 (thru) = 54/100


MINBLOCK 설정해줘서? 갑자기 점수 좀 올랐음

trace  valid  util     ops      secs  Kops
 0       yes   99%    5694  0.004117  1383
 1       yes   99%    5848  0.004132  1415
 2       yes   99%    6648  0.005896  1128
 3       yes   99%    5380  0.004448  1210
 4       yes   98%   14400  0.000116123818
 5       yes   92%    4800  0.006205   774
 6       yes   91%    4800  0.003519  1364
 7       yes   53%   12000  0.065477   183
 8       yes   47%   24000  0.236260   102
 9       yes   25%   14401  0.024294   593
10       yes   45%   14401  0.000951 15138
Total          77%  112372  0.355414   316

Perf index = 46 (util) + 21 (thru) = 67/100



```C
 if(heap_listp==0){
        mm_init();
    }
```
이거 추가하고 1점 오름 ㅎㅎㅋㅋ;
Results for mm malloc:
trace  valid  util     ops      secs  Kops
 0       yes   99%    5694  0.004170  1365
 1       yes   99%    5848  0.004743  1233
 2       yes   99%    6648  0.006103  1089
 3       yes   99%    5380  0.004324  1244
 4       yes   98%   14400  0.000122117936
 5       yes   92%    4800  0.004079  1177
 6       yes   91%    4800  0.003686  1302
 7       yes   53%   12000  0.064717   185
 8       yes   47%   24000  0.234919   102
 9       yes   25%   14401  0.022730   634
10       yes   45%   14401  0.000828 17401
Total          77%  112372  0.350421   321

Perf index = 46 (util) + 21 (thru) = 68/100