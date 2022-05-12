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
    "team 7",
    /* First member's full name */
    "Piousangel",
    /* First member's email address */
    "yyh7654@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};



/* single word(4) or double word (8) alignment */
/* 아래 ALIGH(size) 함수에서 할당할 크기인 size를 8의 배수로 맞춰서 할당하기 위한 매크로 */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
/* 할당할 크기인 size를 보고 8의 배수 크기로 할당하기 위해 size를 다시 align하는 작업을 한다. 만약 size가 4이면 (4+8-1) = 11 = 0000 1011 이고
이를 ~0x7 = 1111 1000과 AND 연한하면 0000 1000 = 8이 되어 적당한 8의 배수 크기로 align할 수 있다.*/
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

/* 메모리 할당 시 기본적으로 header와 footer를 위해 필요한 더블워드만큼의 메모리 크기.
    long형인 size_t의 크기만큼 8을 나타내는 매크로.
 */
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))


#define WSIZE 4 /* word와 header/footer 사이즈 */
#define DSIZE 8 
#define MINIMUM 16 /* initial prologue 블록 사이즈, header, footer, PREC, SUCC 포인터 */
#define CHUNKSIZE (1<<12) /* 힙을 1<<12만큼 연장 => 4096byte */

#define MAX(x, y) ((x) > (y) ? (x) : (y)) 

/* header 및 footer 값(size + allocated) 리턴 */
#define PACK(size, alloc)   ((size) | (alloc))

/* header 및 footer 값 (size + allocated) 리턴 */
#define GET(p)          (*(unsigned int*)(p))

/* 주소 p에서의 word를 읽어오거나 쓰는 함수 */
#define GET(p)          (*(unsigned int*)(p))
#define PUT(p, val)     (*(unsigned int*)(p) = (val)) // 주소 p에 val을 넣어준다.

/* header or footer에서 블록의 size, allocated field를 읽어온다 */
#define GET_SIZE(p) (GET(p) & ~0x7) /* GET(p)로 읽어오는 주소는 8의 배수 & 이를 7을 뒤집은 1111 1000과 AND 연산하면 정확히 블록 크기(뒷 세자리는 제외)만 읽어오기 가능*/
#define GET_ALLOC(p) (GET(p) & 0x1) /* 0000 0001과 AND 연산 => 끝자리가 1이면 할당/0이면 해제 */

/* 블록 포인터 bp를 인자로 받아 블록의 header와 footer의 주소를 반환 */
#define HDRP(bp) ((char*)(bp)-WSIZE) /* 헤더 포인터: bp의 주소를 받아서 한 워드 사이즈 앞으로 가면 헤더가 있음.*/
#define FTRP(bp)    ((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
/*footer 포인터: 현재 블록 포인터 주소에서 전체 사이즈만큼 더해주고(전체 사이즈 알려면 HDRP(bp)로 전체 사이즈 알아내야) 맨앞 패딩 + header만큼 빼줘야 footer를 가리킨다. */

/* 블록 포인터 bp를 인자로 받아 이후, 이전 블록의 주소를 리턴한다 */
#define NEXT_BLKP(bp)   ((char*)(bp) + GET_SIZE(((char*)(bp) - WSIZE))) // (char*)(bp) + GET_SIZE(지금 블록의 헤더값)
#define PREV_BLKP(bp)   ((char*)(bp) - GET_SIZE(((char*)(bp) - DSIZE))) // (char*)(bp) - GET_SIZE(이전 블록의 풋터값)

/* Free List 상에서의 이전, 이후 블록 포인터를 리턴 */
/* Free list에서는 bp가 PREC을 가리키고 있고 SUCC는 그 다음 블록을 가리키고 있는 상태 */
#define PREC_FREEP(bp) (*(void**)(bp)) // 이전 블록의 bp에 들어있는 주소값을 리턴
#define SUCC_FREEP(bp) (*(void**)(bp + WSIZE)) // 이후 블록의 bp

/* define searching method for find suitable free blocks to allocate */

// #define NEXT_FIT -> define 하면 next_fit, 안하면 first_fit으로 탐색

/*global variable & functions */

static char* heap_listp = NULL; // 항상 prologue block(힙의 맨앞 시작점)을 가리키는 정적 전역 변수 설정
static char* free_listp = NULL; // free list의 맨 첫 블록을 가리키는 포인터

static void* extend_heap(size_t words); // 주어진 워드 사이즈만큼 힙을 늘린다.
static void* coalesce(void* bp); // 현재 블록 포인터를 인자로 받아 합체
static void* find_fit(size_t asize); // 블록이 들어갈 사이즈 맞는 애를 찾아
static void place(void* bp, size_t newsize); // 새로운 블록을 배치

int mm_init(void);
void *mm_malloc(size_t size);
void mm_free(void *bp);
void *mm_realloc(void *ptr, size_t size);

/* mm_init: 말록 패키지를 초기화 */

int mm_init(void)
{

    if ((heap_listp = mem_sbrk(6*WSIZE)) == (void*)-1) {   //여기서 heap_listp를 메모리의 처음으로 세팅 mem_brk위치 1차확인
       return -1;
    }

    PUT(heap_listp, 0);  // Alignment padding. 더블 워드 경계로 정렬된 미사용 패딩.
    PUT(heap_listp + (1*WSIZE), PACK(MINIMUM, 1));  // prologue header
    PUT(heap_listp + (2*WSIZE), NULL);  // prologue block안의 PREC 포인터 NULL로 초기화
    PUT(heap_listp + (3*WSIZE), NULL);  // prologue block안의 SUCC 포인터 NULL로 초기화
    PUT(heap_listp + (4*WSIZE), PACK(MINIMUM, 1));  // prologue footer
    PUT(heap_listp + (5*WSIZE), PACK(0, 1));      // epliogue header

    free_listp = heap_listp + 2*WSIZE; //free_listp위치는 지금 프롤로그 헤더바로뒤

    /* 그 후 CHUNKSIZE만큼 힙을 확장해 초기 가용 블록을 생성 하면서 bp위치가 옮겨지는 자리를 잘봐야해 */
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) 
    { // 실패하면 -1 리턴
        return -1;
    }
    return 0;
}

/*
    coalesce(bp): 해당 가용 블록을 앞뒤 가용블록과 연결하고 연결된 가용 블록의 주소를 리턴
*/

static void* coalesce(void* bp){
    /*직전 블록의 footer, 직후 블록의 header를 보고 가용 여부를 확인.
      초기화 때의 coalesce그림을 그려 주고 싶지만 .. 이것도 bp위치 때문에 고생했어요
      초기화시 bp의 프리브 블럭의 bp도 1로, bp의 넥스트 블럭의 헤더도 1이기 때문에 1번케이스에 해당 됨*/
    
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); // 직전 블록 가용 여부 확인
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); // 직후 블록 가용 여부 확인
    size_t size = GET_SIZE(HDRP(bp));


    /* case 1: 직전, 직후 블록이 모두 할당 -> 해당 블록만 free list에 넣어준다. 초기화 과정에서도 생각해보면
    6칸에 extend_heap으로 들어가서 CHUNKSIZE/WSIZE 만큼의 칸을 할당받아 헤더 풋터 에필로그 헤더까지 잡아주고
    옮겨진 bp의 관점에서 생각해보면 넥스트 blkp도 1, prev_blkp도 1이기 때문에 1번 케이스에 해당된다.
    */

    // case 2: 직전 블록 할당, 직후 블록 가용

    if (prev_alloc && !next_alloc) {
        removeBlock(NEXT_BLKP(bp)); // free 상태였던 직후 블록을 free list에서 제거
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    // case 3: 직전 블록 가용, 직후 블록 할당
    else if (!prev_alloc && next_alloc){ // alloc이 0이니까  !prev_alloc == True.
        removeBlock(PREV_BLKP(bp)); // 직전 블록을 free list에서 제거
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        bp = PREV_BLKP(bp);
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    // case 4: 직전, 직후 블록 모두 가용
    else if (!prev_alloc && !next_alloc) {
        removeBlock(PREV_BLKP(bp));
        removeBlock(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp))); // 앞뒤 free된 블록 전체 사이즈 계산
        bp = PREV_BLKP(bp); // 현재 bp를 가용인 앞쪽 블록으로 이동시켜 => 거기가 전체 free 블록의 앞쪽이 되어야 함.
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    // 연결된 새 가용 블록을 free list에 추가
    putFreeBlock(bp);

    return bp;
}

/*
mm_malloc - Allocate a block by incrementing the brk pointer
    Always allocate a block whose size is a multiple of the alignment.

    처음 초기화를 하고 mm_malloc을 하는 과정만 알면 그 뒤론 다 똑같으니 
    처음 할당받을 사이즈를 매개변수로 받아 그 사이즈를 사용 가능한 사이즈로 만들어 준 뒤에
    find_fit과정을 진행하면서 bp의 위치를 옮겨준다 그리고 가용사이즈가 부족하면 늘려준다
    초기화 후 제공할 충분한 공간이 있다고 가정하면 place함수로 갈 수 있다
*/

void *mm_malloc(size_t size) {
    size_t asize; // Adjusted block size
    size_t extendsize; // Amount for extend heap if there is no fit
    char* bp;

    if (size == 0) {
        return NULL;
    }
    // 요청 사이즈에 header와 footer를 위한 double word 공간(SIZE_T_SIZE)을 추가한 후 align해준다.
    asize = ALIGN(size + SIZE_T_SIZE);

    // 할당할 가용 리스트를 찾아 필요하다면 분할해서 할당한다.
    if ((bp = find_fit(asize)) != NULL) { // first fit으로 추적.
        place(bp, asize); // 필요하다면 분할하여 할당
        return bp;
    }

    // 만약 맞는 크기의 가용 블록이 없다면 새로 힙을 늘려서 새 힙에 메모리를 할당. max를 사용하는 이유는 기본적으로 Chunksize를 받는데 asize가 chunksize보다 더 큰게 들어오면
    // 그걸로 받기 위함 일 것 같다.
    extendsize = MAX(asize, CHUNKSIZE); // 둘 중 더 큰 값으로 사이즈를 정한다.
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL) {
        return NULL;
    }
    place(bp, asize);
    return bp;
}

/*
    extend_heap: 워드 단위 메모리를 인자로 받아 힙을 늘려준다.
    
    1. 제일 처음 초기화 당시 size를 8의 배수를 맞춰놓고
    mem_sbrk를 통해 bp의 위치를 재설정한다 (bp 위치 때문에 꽤나 고생함) 총 초기화시 mem_sbrk가 2번 실행되므로~
    그리고 옮겨진 bp의 헤더 푸터를 0, 거기다 에필로그 헤더까지 박아준다.
    그리고 코얼레시스 초기화로 들어감

*/

static void* extend_heap(size_t words) {
    // 워드 단위로 받는다.
    char* bp;
    size_t size;

    /* 더블 워드 정렬에 따라 메모리를 mem_sbrk 함수를 이용해 할당받음. mem_sbrk는 힙 용량을 늘려줌.*/ 
    size = (words % 2) ? (words + 1) * WSIZE : (words) * WSIZE; // size를 짝수 word && byte 형태로 만든다.
    if ((long)(bp = mem_sbrk(size)) == -1) {// 새 메모리의 첫 부분을 bp로 둔다. 주소값은 int로는 못 받으니 long으로 type casting
        return NULL;
    }
    /* 새 가용 블록의 header와 footer를 정해주고 epliogue block을 가용 블록 맨 끝으로 옮긴다. */
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); // 새 에필로그 헤더

    /* 만약 이전 블록이 가용 블록이라면 연결 */
    return coalesce(bp);
}

/*
first_fit: 힙의 맨 처음부터 탐색해서 요구하는 메모리 공간보다 큰 가용 블록의 주소를 반환.
*/

static void* find_fit(size_t asize) {

    void* bp;

    //free list에서 유일하게 할당된 블록. 제일 처음 초기화 했던 그 1로 초기화 했던 거기(free list의 마지막)까지 포문을 돌게끔
    for (bp = free_listp; GET_ALLOC(HDRP(bp))!= 1; bp = SUCC_FREEP(bp)){
        if (asize <= GET_SIZE(HDRP(bp))) {
            return bp;
        }
    }

    // 못 찾으면 NULL을 리턴한다.
    return NULL;
}

/*
place(bp, size): 요구 메모리를 할당할 수 있는 가용 블록을 할당. 이때, 분할이 가능하면 분할

초기화 후 mm_malloc을 하면서 place로 넘어왔을 때 처음 remove블록함수를 통해 
freelist_p의 위치를 바꿔주는 작업을 한다.
이 함수에서 csize가 충분히 크고 asize가 충분히 작아서 그 차이가 4words사이즈보다 클 경우

*/

static void place(void* bp, size_t asize) {
    // 현재 할당할 수 있는 가용 블록 주소

    size_t csize = GET_SIZE(HDRP(bp));

    //할당될 블록이니 free list에서 없애준다.
    removeBlock(bp);

    // 분할 가능한 경우
    if ((csize - asize) >= (2*DSIZE)) {
        // 앞 블록은 할당 블록으로
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        // 뒤 블록은 가용 블록으로 분할
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize, 0));
        PUT(FTRP(bp), PACK(csize-asize, 0));

        // free list 첫번째에 분할된 블럭을 넣는다.

        putFreeBlock(bp);
    }
    else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

/*
putFreeBlock(bp): 새로 반환되거나 생성된 가용 블록을 free list 첫 부분에 넣는다.

초기화 시에 이게 제일 처음 초기화한 6개가 맨 뒤에 나오고 이 것들은 예외적으로 가용(0)이 아니라
(1)일 때도 succ가 가르킬 수 있다 이것도 초기화만 예외적으로 발생하는 과정
그렇기 때문에 bp의 succ이 free_listp(제일 처음 프롤로그 헤더 뒤에 나온 녀석)을 가리킬 수 있고
초기화시 extend_heap에 한번들어가 추가된 것 밖에 없으니 그곳을 가리키는 bp가 마지막이니
bp의 prec은 null, free_list의 prec도 bp가 되면서 이쁘게 떨어진다
*/

void putFreeBlock(void* bp) {
    SUCC_FREEP(bp) = free_listp;
    PREC_FREEP(bp) = NULL;
    PREC_FREEP(free_listp) = bp;
    free_listp = bp;
}

void removeBlock(void *bp) {
    // Free list의 첫번째 블록 없앨 때
    if (bp == free_listp) {
        PREC_FREEP(SUCC_FREEP(bp)) = NULL;
        free_listp = SUCC_FREEP(bp);
    }
    // free list 안에서 없앨 때
    else {
        SUCC_FREEP(PREC_FREEP(bp)) = SUCC_FREEP(bp);
        PREC_FREEP(SUCC_FREEP(bp)) = PREC_FREEP(bp);
    }
}

/*
mm_free - Freeing a block does nothing.
*/

void mm_free(void *bp) {
    // 해당 블록의 size를 알아내 header와 footer의 정보를 수정.
    size_t size = GET_SIZE(HDRP(bp));

    // header와 footer를 설정

    PUT(HDRP(bp), PACK(size, 0)); // size/0을 헤더에 입력
    PUT(FTRP(bp), PACK(size, 0)); // size/0을 footer에 입력

    // 만약 앞뒤 블록이 가용 상태면 연결
    coalesce(bp);
}

/*
mm_realloc: implimented simply in terms of mm_malloc and mm_free
*/

void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr; // 크기를 조절하고 싶은 힙의 시작 포인터
    void *newptr; // 크기 조절 뒤의 새 힙 시작 포인터
    size_t copySize; // 복사할 힙 크기

    newptr = mm_malloc(size);
    if (newptr == NULL) {
        return NULL;
    }

    // copysize: *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    copySize = GET_SIZE(HDRP(oldptr));

    // 원래 메모리 크기보다 적은 크기를 realloc하면 크기에 맞는 메모리만 할당되고 나머지는 안 도니다.

    if (size < copySize) {
        copySize = size;
    }

    memcpy(newptr, oldptr, copySize);  //newptr에 oldptr를 시작으로 copysize만큼의 메모리값복사
    mm_free(oldptr);
    return newptr;
}

// 초기화 과정
// 처음 mm_init 함수를 보면 mem_sbrk을 호출하면서 메모리 사이즈를 블록 6개로
// 잡아주면서 heap_listp의 위치까지 잡아줘요 
// 그리고 6개의 블록에 패딩, 프롤로그 헤더(1), 앞 bp를 가리키는 블록(NULL), 뒤 bp를 가리키는 블록(NULL),
// 프롤로그 푸터(1), 에필로그 푸터순으로 설정해줘요 그리고 free_listp(가용블록을 시작을 알리는 bp)를 
// heap_listp의 2워드 사이즈 뒤인 프롤로그 헤더 바로 뒤로 위치해줬어요
// 그렇게 free_listp의 위치를 잡아주고 extend_heap함수로 들어갑니다 여기서 bp(block pointer)의
// 위치가 바뀌는데 그걸 인지하지 못하고 헛손질을 몇시간 했어요...
// extend_heap함수에서 초기 할당받은 사이즈를 짝수로 맞춰주고 mem_sbrk를 한번더 호출해서 bp를
// size를 늘려주기 전 그 이전의 6블록이 끝나는 점으로!!!! 옮겨줍니다. 왜냐 거기가 brk지점이니까 
// memlib.c들어가서 mem_sbrk 즉, old_brk위치를 잘 생각해 주세요
// 그리고 새로 만들어진 블록의 헤더, 푸터에 사이즈와 할당가능하다고 0을 박아주고
// 에필로그 헤더에 1을 박아주고 coalesce함수로 bp와 함께 가요
// coalesce함수에서 직전 블록의 가용 여부, 직후 블록의 가용 여부를 확인 할 수 있는데, 앞 bp의 블록은
// 저희가 초기화 할때 1, 1로 초기화 했고 뒷 bp(즉, 에필로그 헤더)가 1이기 때문에 둘다 할당 되있기 때문에 1번
// case니까 바로 마법의 putfreeBlock함수로 bp와 함께 가요 그리고 지금 있는 에필로그 헤더 지점의 bp의 succ를
// 제일 초기화했던 그 블록의 free_listp위치로 이어주고, 당연히 bp의 prec는 NULL이고 frree_listp의 prec를 그 extend_heap
// 에서 만들었던 거기 bp로 연결시켜주고 free_listp의 위치를 현재 bp위치로 옮겨 줍니다. 이러면 기본 초기화 과정이 진행되었어요
// 그리고 mm_malloc을 통해 할당해 볼게요 매개변수로 필요한 사이즈를 받아와 사이즈를 확인해요
// 그리고 header, footer를 위한 double word 공간을 추가한 후 find_fit함수를 통해 asize가 들어갈 공간을 찾아요
// 저희는 처음 extend_heap을 통해 엄청 큰 하나의 블록을 생성했죠? 초기화 블럭 말고 그 하나의 블럭 거기에 free_listp
// 가 있고 거기로 bp가 이동되면서 bp가 리턴되요 그리고 place함수에 bp와 size를 들고 갑니다
// place함수에 들어가면 바로 removeBlock을 만나게 되는데 첫 mm_malloc을 호출할 때는 
// find_fit 함수에서 bp가 free_listp를 바로 만나기 때문에 removeBlock에서 첫번째 if문에 걸리게 되고
// 제일 먼저 초기화해줬던 6개 블록의 bp의 prec를 NULL 처리해서 끊어 주고, free_listp의 위치를 다시 6개의 블록쪽으로
// 옮겨줘요 처음엔 이 때 왜 free_listp가 옮겨지는지 저도 이해 하지 못했는데 좀있다 기가막힌 이유가 나옵니다.
// 그렇게 옮겨주고 다시 place함수로 가봅시다 그럼 csize는 대략 chunksize/wsize - 6개 블록(24byte)
// 만큼 있고 asize를 한 16byte(4블록) 잡으면 무조건 2*dsize보다 크기 때문에 bp위치에서 asize만큼 잡고 헤더 푸터에 
// 1, 1넣고 애필로그 헤더로 bp를 옮겨 그 애필로그 헤더를 헤더로 바꿔끼고 그곳 부터 끝까지 헤더 푸터를 잡고 0,0
// 으로 박아줘요 그리고 대망의 풋프리블럭을 만납니다. 현재 bp와 free_listp의 위치를 알고 계셔야 해요
// bp는 현재 어떻게 보면 큰 3개의 블럭중 마지막 블럭에 위치하고 있고 free_listp는 3개 블럭중 첫번째블럭에 위치하고 있어요
// 그리고 bp의 위치의 블럭이 지금 free_list의 첫번째가 되어야 하죠?
// 그렇기 때문에 아까 remove_block에서 free_listp의 위치가 앞으로 간게 이해가 되나요?
// 여기서 bp의 succ을 free_listp로 설정해 주고, 아까말했듯이 총 3개의 블럭중 마지막 블럭에 bp가 위치하고 있기 때문에
// bp의 prec을 NULL로 잡아 줘요 free_listp의 prec을 bp로 잡아주면서 2번째 블럭(할당 블럭)과의 
// 연결을 끊어 버리는 거죠 그리고 당연히 이제 free_list는 3번째 위치를 가리켜야 하기 때문에 bp의 위치로 옮겨옵니다
// 이렇게 mm_malloc을 통해 하나의 동적할당 하는 과정을 알아봤어요 코드 보면서 진짜 계속 너무 신기해서 놀랐어요

// 그리고 만약 mm_malloc과정이 몇번이 있고 할당 해제하는 과정에서 4가지 case가 있는데 이것도 하면서
// putfreeBlock의 함수가 case4개를 다 커버치면서 free_listp가 자기 위치를 찾아가는 과정이 너무 신기했는데
// 꼭 이 과정을 직접 그림을 그려가면서 이해하시길 바랄게요 전 여기 까지 하고 가보겠씁니다