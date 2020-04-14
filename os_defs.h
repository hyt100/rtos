#ifndef OS_DEFS_H
#define OS_DEFS_H



typedef uint8_t U8;
typedef int8_t   S8;
typedef uint16_t U16;
typedef int16_t   S16;
typedef uint32_t U32;
typedef int32_t   S32;
typedef uint8_t   BOOL;


#define TRUE      1
#define FALSE     0

typedef U32 Status_t;
#define RET_SUCCESS    0
#define RET_FAILURE     1
#define RET_DELAY_BREAK    2
#define RET_DELAY_TIMEUP   3
#define RET_SEM_TIMEUP    4
#define RET_SEM_DEL           5


#define NULL    ((void *)0)

typedef void (*TASK_FUNC)(void);


#endif
