#include "os.h"

#define STACK_SIZE     512


OS_TCB* cur_tcb= NULL;
U32 tick = 0;

OS_StackReg *cur_task_sp_addr= NULL;
OS_StackReg *next_task_sp_addr= NULL;


OS_TaskHook  hook = {NULL, NULL, NULL};


OS_ScheduleTab   sched;
LIST delay_que_head;

OS_TCB* root = NULL;
U8 root_stack[STACK_SIZE];
OS_TCB* idle = NULL;
U8 idle_stack[STACK_SIZE];

static U8 const UnMapTab[256] = {
    0, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    7, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
};


int main(void)
{
	os_init();

	root = os_task_create("root", (U8*) &root_stack, STACK_SIZE, user_boot, OS_PRI_HIGHEST, NULL);
	idle = os_task_create("idle", (U8*) &idle_stack, STACK_SIZE, user_idle, OS_PRI_LOWEST, NULL);

	os_start();
	return 0;
}

void MDS_TickIsr(void)
{
	U16 pri;
	LIST* n;
	OS_TCB* tcb;

	tick++;
	delay_table_sched();
	list_delete(&cur_tcb->tcb_node);
	list_insert(&sched.list[cur_tcb->priority], &cur_tcb->tcb_node);
	ready_table_sched();
}

void MDS_SwiIsr(void)
{
	ready_table_sched();
}

void ready_table_sched(void)
{
	U16 pri;
	LIST* n;
	OS_TCB* tcb;
		
	pri = os_pri_query_highest(&sched.flag);
	n = sched.list[pri].next;
	tcb = list_entry(n, OS_TCB, tcb_node);
	task_switch(tcb);
}

void delay_table_sched(void)
{
	LIST* node;
	OS_TCB* tcb;

	node = delay_que_head.next;
	while (node != &delay_que_head) {
		tcb = list_entry(node, OS_TCB, delay_node);
		if (tcb->still_tick == tick) {
			node = node->next;
			list_delete(node);
			tcb->task_flag &= ~OS_TASKFLAG_IN_DELAY_TABLE;
			if (tcb->opt.task_sta == OS_TASKSTA_DELAY) {
				tcb->opt.delay_tick = RET_DELAY_TIMEUP;
			} 
			else if (tcb->opt.task_sta == OS_TASKSTA_PEND) {
				tcb->opt.delay_tick = RET_SEM_TIMEUP;
			}
			else {
				tcb->opt.delay_tick = RET_SUCCESS;
			}
			tcb->opt.task_sta = OS_TASKSTA_READY;
			schedule_add_task(&sched, tcb);
		}
		else {
			//node = node->next;
			break;
		}
	}
}


void task_switch(OS_TCB* tcb)
{
#ifdef OS_TASK_HOOK_SUPPORT
	if (hook.sw_func) {
		if (cur_tcb != tcb) {
			hook.sw_func(cur_tcb, tcb);
		}
	}
#endif
	cur_task_sp_addr = &cur_tcb->regs;
	next_task_sp_addr = &tcb->regs;
	cur_tcb = tcb;
}

void os_start(void)
{
	next_task_sp_addr = &root->regs;
	cur_tcb = root;
	MDS_SwitchToTask();
}

void os_init(void)
{	
	os_sched_table_init(&sched);
	
	list_init(&delay_que_head);
}

void os_sched_table_init(OS_ScheduleTab *tab)
{
	U16 i;
	
	for (i=0; i<OS_PRI_NUM; ++i) {
		list_init(&tab->list[i]);
	}
	os_pri_init(&tab->flag);
}

U16 os_pri_query_highest(OS_PriFlag* pFlag)
{
	U16 highest;
#if (OS_PRI_NUM > OS_PRI_64)  //128,256
	U8 a, b, c;
	a = UnMapTab[pFlag->grp3];
	b = UnMapTab[pFlag->grp2[a]];
	c = UnMapTab[pFlag->grp1[a*8+b]];
	highest = ((U16)a*8 + b)*8 + c;
#elif (OS_PRI_NUM > OS_PRI_8) //16,32,64
	U8 a, b;
	a = UnMapTab[pFlag->grp2];
	b = UnMapTab[pFlag->grp1[a]];
	highest = ((U16)a*8 + b)*8 + c;
#else  //8
	highest = (U16)UnMapTab[pFlag->grp1];
#endif
	return highest;
}

void os_pri_set(OS_PriFlag* pFlag, U8 pri)
{
#if (OS_PRI_NUM > OS_PRI_64)  //128,256
	pFlag->grp1[pri/8]    |= 1<<(pri%8);
	pFlag->grp2[pri/8/8] |= 1<<(pri/8%8);
	pFlag->grp3              |= 1<<(pri/8/8);
#elif (OS_PRI_NUM > OS_PRI_8) //16,32,64
	pFlag->grp1[pri/8]    |= 1<<(pri%8);
	pFlag->grp2             |=  1<<(pri/8)
#else  //8
	pFlag->grp1 |= (1<<pri);
#endif
}

void os_pri_clr(OS_PriFlag* pFlag, U8 pri)
{
#if (OS_PRI_NUM > OS_PRI_64)  //128,256
	pFlag->grp1[pri/8]    &= ~(1<<(pri%8));
	if (pFlag->grp1[pri/8] == 0) {
		pFlag->grp2[pri/8/8] &= ~(1<<(pri/8%8));
		if (pFlag->grp2[pri/8/8] == 0) {
			pFlag->grp3              &= ~(1<<(pri/8/8));
		}
	}
#elif (OS_PRI_NUM > OS_PRI_8) //16,32,64
	pFlag->grp1[pri/8]    &= ~(1<<(pri%8));
	if (pFlag->grp1[pri/8]  == 0) {
		pFlag->grp2             &=  ~(1<<(pri/8));
	}
#else  //8
	pFlag->grp1 &= ~(1<<pri);
#endif
}

void os_pri_init(OS_PriFlag* pFlag)
{
#if (OS_PRI_NUM > OS_PRI_64)  //128,256
	U16 i;
	for (i=0; i<OS_PRIFLAG_GRP1_SIZE; ++i) {
		pFlag->grp1[i] = 0;
	}
	for (i=0; i<OS_PRIFLAG_GRP2_SIZE; ++i) {
		pFlag->grp2[i] = 0;
	}
	pFlag->grp3 = 0;
#elif (OS_PRI_NUM > OS_PRI_8) //16,32,64
	U16 i;
	for (i=0; i<OS_PRIFLAG_GRP1_SIZE; ++i) {
		pFlag->grp1[i] = 0;
	}
	pFlag->flag.grp2 = 0;
#else  //8
	pFlag->flag.grp1 = 0;
#endif
}

void schedule_add_task(OS_ScheduleTab* st, OS_TCB* tcb)
{
	LIST* head = (LIST*)&st->list[tcb->priority];

	list_insert(head, &tcb->tcb_node);
	os_pri_set(&st->flag, tcb->priority);
}

void schedule_remove_task(OS_ScheduleTab* st, OS_TCB* tcb)
{
	if (!tcb)
		return;	
	
	LIST* head = (LIST*)&st->list[tcb->priority];

	list_delete(&tcb->tcb_node);
	if (is_list_empty(head)) {
		os_pri_clr(&st->flag, tcb->priority);
	}
}

U32 os_tick_get(void)
{
	return tick;
}

void os_delay_table_add(OS_TCB *tcb)
{
	LIST *temp_node;
	OS_TCB *temp_tcb;
	U32 temp_tick;
	U32 new_tick = tcb->still_tick;
	U32 cur_tick = tick;
	
	temp_node = delay_que_head.next;
	while (temp_node != &delay_que_head) {
		temp_tcb = list_entry(temp_node, OS_TCB, delay_node);
		temp_tick = temp_tcb->still_tick;

		if (temp_tick >= cur_tick) {
			if ((new_tick >= cur_tick) && (new_tick <= temp_tick))
				break;
		}
		else {
			if (new_tick > cur_tick)
				break;
			if (new_tick <= temp_tick)
				break;
		}

		temp_node = temp_node->next;
	}
	list_insert(temp_node, &tcb->delay_node);
}

void os_task_hook_setup(OS_TaskHook *h)
{
	hook.create_func = h->create_func;
	hook.sw_func = h->sw_func;
	hook.del_func = h->del_func;
}

void os_sem_table_add(OS_Sem *sem, OS_TCB *tcb)
{
	U16 pri;
	LIST* head;
	
	if (sem->opt & SEM_OPT_PRI) {
		pri = tcb->priority;
		os_pri_set(&sem->tab.flag, tcb->priority);
	}
	else {
		pri = OS_PRI_LOWEST;
	}

	head = (LIST*)&sem->tab.list[pri];
	list_insert(head, &tcb->tcb_node);
}

void os_sem_table_remove(OS_Sem *sem, OS_TCB *tcb)
{
	U16 pri;
	LIST* head;

	if (sem->opt & SEM_OPT_PRI) {
		pri = tcb->priority;
		head = (LIST*)&sem->tab.list[pri];
		list_delete(tcb);
		if (is_list_empty(head)) {
			os_pri_clr(&sem->tab.flag, pri);
		}
	}
	else {
		list_delete(tcb);
	}
}

 OS_TCB *os_sem_table_get_active_task(OS_Sem *sem)
{
	U16 pri;
	LIST* head;

	if (sem->opt & SEM_OPT_PRI) {
		pri = os_pri_query_highest();
	}
	else {
		pri = OS_PRI_LOWEST;
	}

	head = (LIST*)&sem->tab.list[pri];
	if (is_list_empty(head)) {
		return NULL;
	}
	else {
		return head->next;
	}
}

 
