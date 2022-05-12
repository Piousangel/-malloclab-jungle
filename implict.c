#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "jungel 4th",
    /* First member's full name */
    "Piousangel",
    /* First member's email address */
    "yyh7654@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

// #define은 컴파일 직전에 처리되므로 전처리기 과정을 거치면 매크로가 정의됨
// #define -> 선언, #define 새 매크로 이름 기존 매크로 이름 -> 매크로 이름 재정의 #undef 매크로 해제

/* single word (4) or double word (8) alignment */



// word size -> 4bytes
#define WSIZE 4
// double word size -> 8bytes
#define DSIZE 8
// 초기가용블록과 힙 확장을 위해 chunksize 지정
#define CHUNKSIZE (1 << 12) //4096

#define MAX(x, y) ((x) > (y) ? (x) : (y))

//블록의 size와 할당여부를 알려주는 alloc bit를 합쳐 header와 footer에 담을 수 있도록 반환
#define PACK(size, alloc)   ((size) | (alloc))

// pointer p를 역참조하여 값을 가져옴
// p는 대부분 void( *)일 것이고 void형 pointer는 직접적으로 역참조가 안되므로 형변환을 함
#define GET(p)  (*(unsigned int *)(p))

// pointer p를 역참조하여 val로 값을 바꿈
#define PUT(p, val) (*(unsigned int *)(p) = (val))

// pointer p에 접근하여 블록의 size를 반환
#define GET_SIZE(p) (GET(p) & ~(0x7))

// pointer p에 접근하여 블록의 할당 bit를 반환
#define GET_ALLOC(p)    (GET(p) & 0x1)

// block pointer p를 주면 해당 block의 header를 가리키는 포인터를 반환
#define HDRP(bp)    ((char *)(bp) - WSIZE)

// block pointer p를 주면 해당 block의 footer를 가리키는 포인터를 반환
#define FTRP(bp)    ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

// block pointer p의 다음 블록의 위치를 가리키는 pointer를 반환
#define NEXT_BLKP(bp)   ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
// block pointer p의 이전 블록의 위치를 가리키는 pointer를 반환
#define PREV_BLKP(bp)   ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

#define FIRST_FITkkkkk
#define NEXT_FIT


//points to first byte of heap
// static char *mem_start_brk;

// //points to last byte of heap
// static char *mem_brk;

// //largest legal heap address
// static char *mem_max_addr;
static void *coalesce(void *bp);
static void *extend_heap(size_t words);
// static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);

static void *next_fit(size_t asize);
static void *heap_listp = NULL;  
static void *last_bp; //next_fit
/* 
 * mm_init - initialize the malloc package.
 */
// void mem_init(void)
// {
//     /*
//     allocate the sorage we will use to medel the avaiilable VM
//     heap의 최대 크기(MAX_HEAP)을 할당했을 때, 그 시작주소가 NULL이면 에러처리
//     */
//    if ((mem_start_brk = (char *)malloc(MAX_HEAP)) == NULL){
//        fprintf(stderr, "mem_init_vm : malloc error\n");
//        exit(1);
//    }
//    //heap의 마지막을 가리키는 mem_max_addr
//     mem_brk = (char *)mem_start_brk;                 //heap is empty initially
//     mem_max_addr = (char *)mem_start_brk + MAX_HEAP; //max legal heap address 
// }

// void *mem_sbrk(int incr){

//     //return을 위해 이전 mem_brk를 저장
//     char *old_brk = mem_brk;

//     if ((incr <0) || ((mem_brk + incr) > mem_max_addr)){      //줄이는 기능은 구현 안되있기 때문에 incr < 0 이면 error
//         errno = ENOMEM;
//         fprintf(stderr, "ERROR : mem_sbrk failed. Ran out of memory...\n");
//         return (void *)-1;
//     }
//     mem_brk += incr;
//     return (void *)old_brk;     //늘리기 전의 mem_brk를 반환
// }

/*
    mm_init함수는 메모리 시스템에서 4word를 가져와서 비어있는 가용 리스트를 만들고
    extend_heap()함수를 통해 CHUNKSIZE 바이트로 확장하고 초기 가용 블록을 생성해주는 역할
*/
int mm_init(void){

    // void *heap_listp;
    //4워드가 필요하므로 heap 전체 영역이 4워드 미만이면 안됨
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;
    
    //패딩 할당해주기
    PUT(heap_listp, 0);
    // prologue header, 1워드 떨어진 지점
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));
    // prologue footer, 2워드 떨어진 지점
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));
    // epliogue header, 3워드 떨어진 지점
    PUT(heap_listp + (3*WSIZE), PACK(0,1));
    
    heap_listp += (2*WSIZE); //header블록 앞쪽으로 bp를 잡아주려고(padding+prologue header가 2WSIZE)

    if(extend_heap(CHUNKSIZE/WSIZE) == NULL){
        return -1;
    }
    last_bp = heap_listp;   //처음 last_bp를 초기화한 header블록 앞쪽으로 잡아줌
    return 0;
}

static void *extend_heap(size_t words){

    size_t size;
    char *bp;

    // 가용 리스트의 크기를 8의 배수로 조절하기 위한 작업 -> 1 : TRUE , 0 : FALSE
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    
    if ((long)(bp = mem_sbrk(size)) == -1){
        return NULL;
    }

    // header와 footer 업데이트
    // header에 size와 할당비트 업데이트
    PUT(HDRP(bp), PACK(size, 0)); 
    // footer에 size와 할당비트 업데이트
    PUT(FTRP(bp), PACK(size, 0));
    // 새로운 epilogue header 생성 -> epilogue header는 가상의 헤더이며 다음 블록의 헤더를 가리키는 것이 아니다(끝을 알리기위함)
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1));

    return coalesce(bp);
}

static void place(void *bp, size_t asize){
    size_t csize = GET_SIZE(HDRP(bp));
    if((csize - asize) >= 2 * DSIZE){    //할당되고 남은게 4워드보다 크면
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
    }
    else{
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 * 
 * 인자로 size를 받으면 그 size에 적합한 메모리를 할당하주는 역할을 하는 함수
 * 현재 free된 메모리 영역에 들어가지 못하는 size를 인자로 넣었다면 메모리 시스템에게 extend_heap()을
 * 요청한다 free된 메모리 영역에 적합한 사이즈라면 2가지 경우가 나뉜다
 * 
 * 첫 번째는 8바이트보다 작은 size로 인자가 들어오면 header와 footer 8바이트를 포함하여 총 16바이트를
 * 할당해주면 된다
 * 두 번째는 8바이트보다 큰 size가 들어온다면 alignment constraints를 마주기 위해 하나의 수식을 사용
 * 
 * DSIZE : header & footer 8bytes
 * DSIZE - 1 을 더해도 8을 넘지 않으니 +1 이상의 몫을 나타낼 수 없다 so, 7을 더해줘서 최대 +1을 맞춰준다
 */


/*
    mm_malloc에서 할당이 가능한 size라고 판단이 내려지면 실제로 메모리를 할당받고 값을 업데이트 시키는 역할을 한다
*/

void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char *bp;

    if(size == 0){
        return 0;
    }
    if(size <= DSIZE){
        asize = 2 *DSIZE;
    }
    else{
        // DSIZE - 1 을 더해도 8을 넘지 않으니 +1 이상의 몫을 나타낼 수 없다 so, 7을 더해줘서 최대 +1을 맞춰준다(+1을한 몫을 구하는 과정)
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
    }
    if((bp = next_fit(asize)) != NULL){   //(bp = next_fit(asize)  (bp = find_fit(asize)  
        place(bp, asize);
        last_bp = bp;
        return bp;
    }
    extendsize = MAX(asize, CHUNKSIZE);  // why max?
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL){
        return NULL;
    }
    place(bp, asize);
    return bp;
    
    
    // int newsize = ALIGN(size + SIZE_T_SIZE);
    // void *p = mem_sbrk(newsize);
    // if (p == (void *)-1)
	// return NULL;
    // else {
    //     *(size_t *)p = size;
    //     return (void *)((char *)p + SIZE_T_SIZE);
    // }
}

// first fit으로 가용한 부분(free 영역)을 찾는 과정
#if defined(FIRST_FIT)
static void *find_fit(size_t asize){

    void *bp;
    for(bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)){
        if( !GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
            return bp;
        }
    }
    return NULL;
}

#else
//next fit
static void *next_fit(size_t asize){

    char * bp = last_bp;

    for(bp = NEXT_BLKP(bp); GET_SIZE(HDRP(bp)) != 0; bp = NEXT_BLKP(bp)){

        if(GET_ALLOC(HDRP(bp)) ==  0 && GET_SIZE(HDRP(bp)) >= asize){
            last_bp = bp;
            return bp;
        }
    }

    //끝까지 갔는데 할당가능한 free block이 없으면 다시 처음부터 last_bp전까지 탐색
    bp = heap_listp; // heap_listp가 
    while(bp < last_bp){

        bp = NEXT_BLKP(bp);

        if(GET_ALLOC(HDRP(bp)) == 0 && GET_SIZE(HDRP(bp)) >= asize){
            last_bp = bp;
            return bp;
        }
    }
    return NULL;
}

#endif

// free된 영역을 합쳐주는 역할을 하는 함수
static void *coalesce(void *bp){

    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    // 앞 뒤 블록 모두 할당이 되어있는 경우
    if(prev_alloc && next_alloc){
        last_bp = bp;
        return bp;
    }

    // 앞의 블록은 할당, 뒤의 블록만 free인 경우
    else if( prev_alloc && !next_alloc ){
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    // 앞의 블록은 free, 뒤의 블록만 할당된 경우
    else if( !prev_alloc && next_alloc){
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    // 앞의 블록 free, 뒤의 블록도 free인 경우
    else{
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    return bp;
}

/*
     mm_free - Freeing a block does nothing.
     할당을 받았던 block을 free시켜주는 역할을 하는 함수
 */
void mm_free(void *bp)
{
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
    copySize = GET_SIZE(HDRP(oldptr));
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}














