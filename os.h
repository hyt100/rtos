#ifndef OS_H
#define OS_H
#include "os_defs.h"
#include "raw_list.h"

//config
#define OS_TASK_HOOK_SUPPORT


#define OS_PRI_8       8
#define OS_PRI_16     16
#define OS_PRI_32     32
#define OS_PRI_64     64
#define OS_PRI_128   128
#define OS_PRI_256   256


#define OS_PRI_NUM        OS_PRI_8
#define OS_PRI_HIGHEST        0
#define OS_PRI_LOWEST        (OS_PRI_NUM-1)


#define OS_DELAYTIME_NOWAIT               0
#define OS_DELAYTIME_FOREVER     (0xffffffff)


#define OS_TASKSTA_READY          (1<<0)
#define OS_TASKSTA_DELAY          (1<<1)
#define OS_TASKSTA_PEND           (1<<2)
#define OS_TASKSTA_SUSPEND     (1<<3)

#define OS_TASKFLAG_IN_DELAY_TABLE      (1<<0)
#define OS_TASKFLAG_SAFE                         (1<<1)



#if (OS_PRI_NUM > OS_PRI_64)  //128,256
	#define OS_PRIFLAG_GRP1_SIZE   (OS_PRI_NUM / 8)
	#define OS_PRIFLAG_GRP2_SIZE   (OS_PRI_NUM / 64)
#elif (OS_PRI_NUM > OS_PRI_8) //16,32,64
	#define OS_PRIFLAG_GRP1_SIZE   (OS_PRI_NUM / 8)
	#define OS_PRIFLAG_GRP2_SIZE   1
#else  //8
	#define OS_PRIFLAG_GRP1_SIZE   1
	#define OS_PRIFLAG_GRP2_SIZE   1
#endif


typedef struct {
	U32 XPSR;
	U32 R0;
	U32 R1;
	U32 R2;
	U32 R3;
	U32 R4;
	U32 R5;
	U32 R6;
	U32 R7;
	U32 R8;
	U32 R9;
	U32 R10;
	U32 R11;
	U32 R12;
	U32 R13;
	U32 R14;
	U32 R15;
}OS_StackReg;

typedef struct {
#if (OS_PRI_NUM > OS_PRI_64)  //128,256
	U8 grp3;
	U8 grp2[OS_PRIFLAG_GRP2_SIZE];
	U8 grp1[OS_PRIFLAG_GRP1_SIZE];
#elif (OS_PRI_NUM > OS_PRI_8) //16,32,64
	U8 grp2;
	U8 grp1[OS_PRIFLAG_GRP1_SIZE];
#else  //8
	U8 grp1;
#endif
}OS_PriFlag;



typedef struct {
	U32 task_sta;
	U32 delay_tick;
}OS_TaskOpt;

typedef struct {
	OS_StackReg regs;
	LIST  tcb_node;
	LIST  delay_node;
	OS_Sem* sem;
	U8*   name;
	U32   task_flag;
	U16   priority;
	OS_TaskOpt opt;
	U32   still_tick;
	U32   safe_cnt;
}OS_TCB;

typedef struct {
	LIST list[OS_PRI_NUM];
	OS_PriFlag    flag;
}OS_ScheduleTab;

/////////////////////// Hook  /////////////////////////////


typedef void (*TaskSwHookFunc)(OS_TCB* old_tcb, OS_TCB* new_tcb);
typedef void (*TaskHookFunc)(OS_TCB*);

typedef struct {
	TaskSwHookFunc  sw_func;
	TaskHookFunc create_func;
	TaskHookFunc del_func;
}OS_TaskHook;


/////////////////////// Semaphore /////////////////////////

typedef struct {
	OS_ScheduleTab tab;
	U32 counter;
	U32 opt;
	OS_TCB *tcb;
}OS_Sem;

/////////////////////// Queue      /////////////////////////
typedef struct {
	LIST head;
	OS_Sem  sem;
}OS_Que;

#endif
