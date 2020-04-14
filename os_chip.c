#include "os.h"


#define OS_INT_ENABLE()
#define OS_INT_DISABLE()

static U16 int_cnt = 0;


#define VAL_XPSR    0
 
void os_stackreg_init(OS_StackReg *pStack, TASK_FUNC func)
{
	pStack->XPSR = VAL_XPSR;
	pStack->R0 = 0;
	pStack->R1 = 0;
	pStack->R2 = 0;
	pStack->R3 = 0;
	pStack->R4 = 0;
	pStack->R5 = 0;
	pStack->R6 = 0;
	pStack->R7 = 0;
	pStack->R8 = 0;
	pStack->R9 = 0;
	pStack->R10 = 0;
	pStack->R11 = 0;
	pStack->R12 = 0;
	pStack->R13 = (U32)pStack;
	pStack->R14 = (U32)os_task_selfdelete;
	pStack->R15 = (U32)func;
}

Status_t os_int_lock(void)
{
	if (NULL == cur_tcb)
		return RET_SUCCESS;
	
	if (RET_SUCCESS == MDS_RunInInt())
		return RET_SUCCESS;
	
	if (int_cnt == 0) {
		OS_INT_DISABLE();
		++int_cnt;
		return RET_SUCCESS;
	}
	else if (int_cnt < 0xffff){
		++int_cnt;
		return RET_SUCCESS;
	}
	else {
		return RET_FAILURE;
	}
}

Status_t os_int_unlock(void)
{
	if (NULL == cur_tcb)
		return RET_SUCCESS;
	
	if (RET_SUCCESS == MDS_RunInInt())
		return RET_SUCCESS;
	
	if (int_cnt > 0) {
		--int_cnt;
		if (int_cnt == 0) {
			OS_INT_ENABLE();
		}
		return RET_SUCCESS;
	}
	else {
		return RET_FAILURE;
	}
}

void os_swi_trigger(void)
{
	MDS_TaskOccurSwi();
}
