#include "os.h"

OS_TCB* tcb1;
U8 sp1[STACK_SIZE];

OS_TCB* tcb2;
U8 sp2[STACK_SIZE];

#ifdef OS_TASK_HOOK_SUPPORT
OS_TaskHook myhook = {
	task_switch_print,
	task_create_print,
	task_delete_print
};
#endif

void user_boot(void)
{
#ifdef OS_TASK_HOOK_SUPPORT
	os_task_hook_setup(&myhook);
#endif

	tcb1 = os_task_create("task1", (U8*) &sp1, STACK_SIZE, user_task1, 2, NULL);
	tcb2 = os_task_create("task2", (U8*) &sp2, STACK_SIZE, user_task2, 3, NULL);

	os_task_selfdelete();
}

void user_idle(void)
{
	while (1) {

	}
}

void user_task1(void)
{
	while (1) {

	}
}

void user_task2(void)
{
	while (1) {

	}
}


#ifdef OS_TASK_HOOK_SUPPORT
void task_create_print(OS_TCB* tcb)
{
	printf("Task %s is created! Tick is: %d \r\n", tcb->name, os_tick_get());
}

void task_switch_print(OS_TCB *old_tcb, OS_TCB *new_tcb)
{
	printf("Task %s ---> Task %s! Tick is: %d \r\n", old_tcb->name, new_tcb->name, os_tick_get());
}

void task_delete_print(OS_TCB* tcb)
{
    	printf("Task %s is deleted! Tick is: %d \r\n",  tcb->name, os_tick_get());
}
#endif

