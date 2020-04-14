#include "os.h"

#define TASK_SAFE_CNT_EMPTY    0
#define TASK_SAFE_CNT_FULL     0xffffffff


OS_TCB* os_tcb_init(U8* pName, U8* pStack, U32 stack_size, TASK_FUNC task_func, U8 pri, OS_TaskOpt* pOpt)
{
	OS_TCB* tcb;
	U8* byte4;

	byte4 = ((U32)pStack + stack_size) & 0xFFFFFFFC;
	tcb = (OS_TCB*)(((U32)byte4 - sizeof(OS_TCB)) & 0xFFFFFFF8);	

	os_stackreg_init(&tcb->regs, task_func);
	tcb->name = pName;
	tcb->priority = pri;
	tcb->task_flag = 0;
	tcb->safe_cnt = TASK_SAFE_CNT_EMPTY;

	if (pOpt) {
		tcb->opt.task_sta = pOpt->task_sta;
		tcb->opt.delay_tick = pOpt->delay_tick;
	}
	else {
		tcb->opt.task_sta = OS_TASKSTA_READY;
	}

	os_int_lock();
	
	if (OS_TASKSTA_READY == tcb->opt.task_sta) {
		schedule_add_task(&sched, tcb);
	}
	else if (OS_TASKSTA_DELAY == tcb->opt.task_sta) {
		if (tcb->opt.delay_tick != OS_DELAYTIME_FOREVER) {
			tcb->still_tick = os_tick_get() + tcb->opt.delay_tick;
			tcb->task_flag |= OS_TASKFLAG_IN_DELAY_TABLE;
			os_delay_table_add(tcb);
		}
	}
		
	os_int_unlock();
	
	return tcb;
}

OS_TCB* os_task_create(U8* pName, U8* pStack, U32 stack_size, TASK_FUNC task_func, U16 pri, OS_TaskOpt* pOpt)
{
	if ((!pStack) || (stack_size < sizeof(OS_TCB))
		return NULL;
	if (!task_func)
		return NULL;
	if (task_func == user_boot) {
		if (root != NULL) {
			return NULL;
		}
	}
	if (task_func == user_idle) {
		if (idle != NULL) {
			return NULL;
		}
	}
	if (pri > OS_PRI_LOWEST)
		return NULL;	
	if ((pri == OS_PRI_HIGHEST) || (pri == OS_PRI_LOWEST)) {
		if ( (task_func != user_boot) && (task_func != user_idle)) {
			return NULL;
		}
	}
	
	if (pOpt) {
		if ((pOpt->task_sta != OS_TASKSTA_READY) && (pOpt->task_sta != OS_TASKSTA_DELAY)) {
			return NULL;
		}
	}

	OS_TCB* tcb = os_tcb_init(pName, pStack, stack_size, task_func, pri, pOpt);

	if ( (task_func != user_boot) && (task_func != user_idle)) {
#ifdef OS_TASK_HOOK_SUPPORT
		if (hook.create_func) {
			hook.create_func(tcb);
		}
#endif
		os_swi_trigger();
	}
	
	return tcb;
}

Status_t os_task_delete(OS_TCB *tcb)
{
	if (!tcb) {
		return RET_FAILURE;
	}

	if (tcb == idle) {
		return RET_FAILURE;
	}

	os_int_lock();
	if (tcb->task_flag & OS_TASKFLAG_SAFE) {
		os_int_unlock();
		return RET_FAILURE;
	}
	if (tcb->opt.task_sta == OS_TASKSTA_READY) {
		schedule_remove_task(&sched, tcb);
	}
	if (tcb->opt.task_sta == OS_TASKSTA_PEND) {
		os_sem_table_remove(tcb->sem, tcb);
	}
	if (tcb->task_flag & OS_TASKFLAG_IN_DELAY_TABLE) {
		list_delete(&tcb->delay_node);
	}
	os_int_unlock();

#ifdef OS_TASK_HOOK_SUPPORT
	if (hook.del_func) {
		hook.del_func(tcb);
	}
#endif

	os_swi_trigger();
	return RET_SUCCESS;
}

void os_task_selfdelete(void)
{
	(void)os_task_delete(cur_tcb);
}

Status_t os_task_delay(U32   delay_tick)
{
	OS_TCB *tcb = cur_tcb;

	if (tcb == idle) {
		return RET_FAILURE;
	}

	if (delay_tick == OS_DELAYTIME_NOWAIT) {
		tcb->opt.delay_tick = RET_SUCCESS;
	}
	else {
		os_int_lock();
		
		schedule_remove_task(&sched, tcb);
		if (delay_tick != OS_DELAYTIME_FOREVER) {
			tcb->still_tick = os_tick_get() + delay_tick;
			tcb->task_flag |= OS_TASKFLAG_IN_DELAY_TABLE;
			os_delay_table_add(tcb);
		}
		tcb->opt.task_sta = OS_TASKSTA_DELAY;

		os_int_unlock();
	}
	
	os_swi_trigger();
	return tcb->opt.delay_tick;
}

Status_t os_task_wakeup(OS_TCB *tcb)
{
	if ((!tcb) || (tcb == idle)) {
		return RET_FAILURE;
	}

	os_int_lock();
	
	if (tcb->opt.task_sta != OS_TASKSTA_DELAY) {
		os_int_unlock();
		return RET_FAILURE;
	}
	
	if (tcb->task_flag & OS_TASKFLAG_IN_DELAY_TABLE) {
		tcb->task_flag &= ~OS_TASKFLAG_IN_DELAY_TABLE;
		list_delete(&tcb->delay_node);
	}
	tcb->opt.task_sta = OS_TASKSTA_READY;
	tcb->opt.delay_tick = RET_DELAY_BREAK;
	schedule_add_task(&sched, tcb);
	
	os_int_unlock();
	os_swi_trigger();
	return RET_SUCCESS;
}

Status_t os_task_pend(OS_Sem *sem, U32 timeout)
{
	OS_TCB *tcb = cur_tcb;

	if (tcb == idle) {
		return RET_FAILURE;
	}

	tcb->sem = sem;
	schedule_remove_task(&sched, tcb);
	os_sem_table_add(sem, tcb);
	if (timeout != OS_DELAYTIME_FOREVER) {
		tcb->still_tick = os_tick_get() + timeout;
		tcb->task_flag |= OS_TASKFLAG_IN_DELAY_TABLE;
		os_delay_table_add(tcb);
	}
	tcb->opt.task_sta = OS_TASKSTA_PEND;
	
	return RET_SUCCESS;
}

Status_t os_task_safe(void)
{
	os_int_lock();
	if (cur_tcb->safe_cnt == TASK_SAFE_CNT_EMPTY) {
		cur_tcb->task_flag |= OS_TASKFLAG_SAFE;
		cur_tcb->safe_cnt++;
		os_int_unlock();
		return RET_SUCCESS;
	}
	else if (cur_tcb->safe_cnt == TASK_SAFE_CNT_FULL) {
		os_int_unlock();
		return RET_FAILURE;
	}
	else {
		cur_tcb->safe_cnt++;
		os_int_unlock();
		return RET_SUCCESS;
	}
}

Status_t os_task_unsafe(void)
{
	os_int_lock();
	if (cur_tcb->safe_cnt == TASK_SAFE_CNT_EMPTY) {
		os_int_unlock();
		return RET_FAILURE;
	}
	cur_tcb->safe_cnt--;
	if (cur_tcb->safe_cnt == TASK_SAFE_CNT_EMPTY) {
		cur_tcb->task_flag &= ~OS_TASKFLAG_SAFE;
	}
	os_int_unlock();
	return RET_SUCCESS;
}
