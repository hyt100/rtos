#include "os.h"

#define SEM_EMPTY   0
#define SEM_FULL      0xffffffff


#define SEM_OPT_PRI       (1<<0)
#define SEM_OPT_FIFO     (1<<1)
#define SEM_OPT_BIN       (1<<2)
#define SEM_OPT_CNT      (1<<3)
#define SEM_OPT_MUTEX    (1<<4)




Status_t os_sem_create(OS_Sem *sem, U32 opt, U32 val)
{
	if (!sem) {
		return RET_FAILURE;
	}
	if ((((opt & SEM_OPT_PRI) != SEM_OPT_PRI) && ((opt & SEM_OPT_FIFO) != SEM_OPT_FIFO)) ||
		(((opt & SEM_OPT_BIN) != SEM_OPT_BIN) && ((opt & SEM_OPT_CNT) != SEM_OPT_CNT) 
		                                                                  && ((opt & SEM_OPT_MUTEX) != SEM_OPT_MUTEX))) 
	{
		return RET_FAILURE;
	}
	if ((opt & SEM_OPT_BIN) == SEM_OPT_BIN) {
		if ((val != SEM_EMPTY) && (val != SEM_FULL)) {
			return RET_FAILURE;
		}
	}
	if ((opt & SEM_OPT_MUTEX) == SEM_OPT_MUTEX) {
		if (val != SEM_FULL) {
			return RET_FAILURE;
		}
	}
	
	os_sched_table_init(&sem->tab);
	sem->counter = val;
	sem->opt = opt;
	sem->tcb = NULL;
	return RET_SUCCESS;
}

Status_t os_sem_take(OS_Sem *sem, U32 timeout)
{
	if (!sem) {
		return RET_FAILURE;
	}

	os_int_lock();
	if (timeout == OS_DELAYTIME_NOWAIT) {
		/****** bin ******/
		if ((sem->opt & SEM_OPT_BIN) == SEM_OPT_BIN) {
			if (sem->counter == SEM_FULL) {
				sem->counter = SEM_EMPTY;
				os_int_unlock();
				return RET_SUCCESS;
			}
			else {
				os_int_unlock();
				return RET_FAILURE;
			}
		}
		/****** cnt ******/
		else if ((sem->opt & SEM_OPT_CNT) == SEM_OPT_CNT){
			if (sem->counter != SEM_EMPTY) {
				sem->counter--;
				os_int_unlock();
				return RET_SUCCESS;
			}
			else {
				os_int_unlock();
				return RET_FAILURE;
			}
		}
		/****** mutex ******/
		else {
			if (sem->counter == SEM_FULL) {
				sem->counter--;
				sem->tcb = cur_tcb;
				os_int_unlock();
				return RET_SUCCESS;
			}
			
			if (sem->tcb == cur_tcb) {
				if (sem->counter == SEM_EMPTY) {
					os_int_unlock();
					return RET_FAILURE;
				} 
				else {
					sem->counter--;
					os_int_unlock();
					return RET_SUCCESS;
				}
			}
			else {  
				os_int_unlock();
				return RET_FAILURE;
			}
		}
	}
	else {
		/****** bin ******/
		if ((sem->opt & SEM_OPT_BIN) == SEM_OPT_BIN) {
			if (sem->counter == SEM_FULL) {
				sem->counter = SEM_EMPTY;
				os_int_unlock();
				return RET_SUCCESS;
			}
			else {
				if (os_task_pend(sem, timeout) == RET_FAILURE) {
					os_int_unlock();
					return RET_FAILURE;
				}

				os_int_unlock();
				os_swi_trigger();
				return cur_tcb->opt.delay_tick;
			}
		} 
		/****** cnt ******/
		else if ((sem->opt & SEM_OPT_CNT) == SEM_OPT_CNT){
			if (sem->counter != SEM_EMPTY) {
				sem->counter--;
				os_int_unlock();
				return RET_SUCCESS;
			}
			else {
				if (os_task_pend(sem, timeout) == RET_FAILURE) {
					os_int_unlock();
					return RET_FAILURE;
				}

				os_int_unlock();
				os_swi_trigger();
				return cur_tcb->opt.delay_tick;
			}
		}
		/****** mutex ******/
		else {
			if (sem->counter == SEM_FULL) {
				sem->counter--;
				sem->tcb = cur_tcb;
				os_int_unlock();
				return RET_SUCCESS;
			}
			
			if (sem->tcb == cur_tcb) {
				if (sem->counter == SEM_EMPTY) {
					os_int_unlock();
					return RET_FAILURE;
				} 
				else {
					sem->counter--;
					os_int_unlock();
					return RET_SUCCESS;
				}
			}
			else {  //block
				if (os_task_pend(sem, timeout) == RET_FAILURE) {
					os_int_unlock();
					return RET_FAILURE;
				}

				os_int_unlock();
				os_swi_trigger();
				return cur_tcb->opt.delay_tick;
			}
		}
		/***************/
	}
}

Status_t os_sem_give(OS_Sem *sem)
{
	OS_TCB *tcb;
	
	if (!sem) {
		return RET_FAILURE;
	}

	os_int_lock();
	/****** bin ******/
	if ((sem->opt & SEM_OPT_BIN) == SEM_OPT_BIN) {
		if (sem->counter == SEM_FULL) {
			os_int_unlock();
			return RET_SUCCESS;
		}
		else {
			tcb = os_sem_table_get_active_task(sem);
			if (tcb != NULL) {
				os_sem_table_remove(sem, tcb);
				if (tcb->task_flag & OS_TASKFLAG_IN_DELAY_TABLE) {
					list_delete(tcb);
					tcb->task_flag &= ~OS_TASKFLAG_IN_DELAY_TABLE;
				}
				schedule_add_task(&sched, tcb);
				tcb->opt.task_sta = OS_TASKSTA_READY;
				tcb->opt.delay_tick = RET_SUCCESS;
				os_int_unlock();
				os_swi_trigger();
				return RET_SUCCESS;
			}
			else {
				sem->counter = SEM_FULL;
			}
		}
	}
	/****** cnt ******/
	else if ((sem->opt & SEM_OPT_CNT) == SEM_OPT_CNT){
		if (sem->counter == SEM_FULL) {
			os_int_unlock();
			return RET_FAILURE;
		}
		else if (sem->counter == SEM_EMPTY){
			tcb = os_sem_table_get_active_task(sem);
			if (tcb != NULL) {
				os_sem_table_remove(sem, tcb);
				if (tcb->task_flag & OS_TASKFLAG_IN_DELAY_TABLE) {
					list_delete(tcb);
					tcb->task_flag &= ~OS_TASKFLAG_IN_DELAY_TABLE;
				}
				schedule_add_task(&sched, tcb);
				tcb->opt.task_sta = OS_TASKSTA_READY;
				tcb->opt.delay_tick = RET_SUCCESS;
				os_int_unlock();
				os_swi_trigger();
				return RET_SUCCESS;
			}
			else {
				sem->counter++;
			}
		}
		else {
			sem->counter++;
		}
	}
	/****** mutex ******/
	else {
		if (sem->tcb != cur_tcb) {
			os_int_unlock();
			return RET_FAILURE;
		}
		if (sem->counter == SEM_FULL) {
			os_int_unlock();
			return RET_FAILURE;
		}
		sem->counter++;
		if (sem->counter == SEM_FULL) {
			tcb = os_sem_table_get_active_task(sem);
			if (tcb != NULL) {
				sem->counter--;  //dec 1
				sem->tcb = tcb;
					
				os_sem_table_remove(sem, tcb);
				if (tcb->task_flag & OS_TASKFLAG_IN_DELAY_TABLE) {
					list_delete(tcb);
					tcb->task_flag &= ~OS_TASKFLAG_IN_DELAY_TABLE;
				}
				schedule_add_task(&sched, tcb);
				tcb->opt.task_sta = OS_TASKSTA_READY;
				tcb->opt.delay_tick = RET_SUCCESS;
				os_int_unlock();
				os_swi_trigger();
				return RET_SUCCESS;
			}
			else {
				sem->tcb = NULL;
			}
		}
		/***************/
	}

	os_int_unlock();
	return RET_SUCCESS;
}

Status_t os_sem_flush_value(OS_Sem *sem, U32 ret_value)
{
	OS_TCB *tcb;
	
	if (!sem) {
		return RET_FAILURE;
	}

	os_int_lock();
	if (sem->counter == SEM_FULL) {
		os_int_unlock();
		return RET_SUCCESS;
	}
	else {
		tcb = os_sem_table_get_active_task(sem);
		while (tcb != NULL) {
			os_sem_table_remove(sem, tcb);
			if (tcb->task_flag & OS_TASKFLAG_IN_DELAY_TABLE) {
				list_delete(tcb);
				tcb->task_flag &= ~OS_TASKFLAG_IN_DELAY_TABLE;
			}
			schedule_add_task(&sched, tcb);
			tcb->opt.task_sta = OS_TASKSTA_READY;
			tcb->opt.delay_tick = ret_value;

			//next tcb
			tcb = os_sem_table_get_active_task(sem);
		}
		sem->counter = SEM_FULL;
	}
	os_int_unlock();

	os_swi_trigger();
	return RET_SUCCESS;
}

Status_t os_sem_flush(OS_Sem *sem)
{
	return os_sem_flush_value(sem, RET_SUCCESS);
}

Status_t os_sem_delete(OS_Sem *sem)
{
	if (!sem) {
		return RET_FAILURE;
	}

	return os_sem_flush_value(sem, RET_SEM_DEL);
}

