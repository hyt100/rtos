#include "os.h"

#define QUE_OPT_PRI     (1<<0)
#define QUE_OPT_FIFO     (1<<1)

Status_t os_que_create(OS_Que *q, U32 opt)
{
	U32 sem_opt;
	if (q == NULL) {
		return RET_FAILURE;
	}
	if (opt & QUE_OPT_PRI) {
		sem_opt = (SEM_OPT_CNT | SEM_OPT_PRI);
	}
	else if (opt & QUE_OPT_FIFO) {
		sem_opt = (SEM_OPT_CNT | SEM_OPT_FIFO);
	}
	else {
		return RET_FAILURE;
	}

	list_init(&q->head);
	return os_sem_create(&q->sem, sem_opt, SEM_EMPTY);
}

Status_t os_que_put(OS_Que *q, LIST *node)
{
	if (!q || !node) {
		return RET_FAILURE;
	}

	os_int_lock();
	list_insert(&q->head, node);
	os_int_unlock();
	
	return os_sem_give(&q->sem);
}

Status_t os_que_get(OS_Que *q, LIST **out, U32 timeout)
{
	if (!q || !out) {
		return RET_FAILURE;
	}

	if (os_sem_take(&q->sem, timeout) == RET_SUCCESS) {
		os_int_lock();
		if (!is_list_empty(&q->head)) {
			*out = q->head.next;
			list_delete(&q->head.next);
		}
		else {
			*out = NULL;
		}
		os_int_unlock();
		return RET_SUCCESS;
	}
	else {
		*out = NULL;
		return RET_FAILURE;
	}
}

Status_t os_que_delete(OS_Que *q)
{
	if (q == NULL) {
		return RET_FAILURE;
	}
	os_sem_delete(&q->sem);
	return RET_SUCCESS;
}


