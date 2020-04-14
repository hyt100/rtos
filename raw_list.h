/**************************************************************************************************
  Filename:   raw_list.h
  Author:      HYT
  Created:    2015-11-20
  Revised:
  Description:
  Notes:

  Copyright 2015 Autel. All rights reserved.
**************************************************************************************************/
#ifndef _RAW_LIST_H_
#define _RAW_LIST_H_
#include <stdint.h>

/*
 * Doubly-link list
 */
typedef struct _LIST {
	struct _LIST	*next;
	struct _LIST	*previous;
} LIST;


#define list_entry(node, type, member)    ((type *)((uint8_t *)(node) - (uint32_t)(&((type *)0)->member)))


/*
 * List initialization
 */
#define list_init(list_head)	\
					do {\
						(list_head)->next = (list_head);\
						(list_head)->previous = (list_head);\
					 } while (0)


/*
 * return TRUE if the list is empty
 */
#define is_list_empty(list)   ((list)->next == (list))


					

/*
 * add element to list
 * add element before head.
 */
#define list_insert(head, element)    \
					do {\
						(element)->previous = (head)->previous;\
						(element)->next = (head);\
						(head)->previous->next = (element);\
						(head)->previous = (element);\
					 } while (0)


/*
 * delete element from list
 */					
#define list_delete(element)          \
				do {\
					(element)->previous->next = (element)->next;\
					(element)->next->previous = (element)->previous;\
				} while (0)


#endif

