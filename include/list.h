#ifndef __LIST_H__
#define __LIST_H__

#include <stdint.h>

#define container_of(ptr, type, member) \
  ((type *)((uintptr_t)(ptr) - offsetof(type, member)))

struct list_head {
    struct list_head *next;
    struct list_head *prev;
};

typedef struct list_head list_head_t;

static inline void list_init(list_head_t *list)
{
    list->next = list;
    list->prev = list;
}

static inline int list_is_empty(list_head_t *list)
{
	return list->next == list;
}


static inline void list_add(list_head_t *list, list_head_t *item)
{
    list->next->prev = item;
	item->next = list->next;
	item->prev = list;
	list->next = item;
}

static inline void list_del(list_head_t *item)
{
    item->next->prev = item->prev;
	item->prev->next = item->next;
	item->next = NULL;
	item->prev = NULL;
}


static inline int list_is_head(list_head_t *list, list_head_t *head)
{
	return list == head;
}

#define list_entry(ptr, type, member) container_of(ptr, type, member)


#define list_for_each(pos, head) \
	for (pos = (head)->next; !list_is_head(pos, (head)); pos = pos->next)

static inline uint64_t list_count_nodes(list_head_t *list)
{
	list_head_t *pos;
	uint64_t count = 0;

	list_for_each(pos, list)
		count++;

	return count;
}

#endif /* __LIST_H__ */