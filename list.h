#ifndef _LIST_H
#define _LIST_H

typedef struct {
	int capacity;
	int length;
	void **items;
} list_t;

list_t *create_list(void);
void list_free(list_t *list);
void list_add(list_t *list, void *item);
void list_insert(list_t *list, int index, void *item);
void list_del(list_t *list, int index);
int list_seq_find(list_t *list, int compare(const void *item, const void *cmp_to), const void *cmp_to);
int list_find(list_t *list, const void *item);

#endif
