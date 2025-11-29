#include <stdio.h>
#include <stdlib.h>
#include "list.h"
#include "string.h"

#define LIST_DEFAULT_LENGTH 12

List *list_init(List *list, size_t element_size) {
    list->elements = malloc(LIST_DEFAULT_LENGTH * element_size);
    if (list->elements == NULL) {
        return NULL;
    }
    list->length = 0;
    list->capacity = LIST_DEFAULT_LENGTH;
    list->element_size = element_size;
    return list;
}

void list_add(List *list, void *ref) {
    if (list->capacity < list->length + 1) {
        int new_capacity = list->capacity * 2;
		if (new_capacity < list->capacity) {
			fprintf(stderr, "List too long. Capacity overflowed.");
			exit(1);
		}
        void *new_elements = realloc(list->elements, new_capacity * list->element_size);
        if (new_elements == NULL) {
			fprintf(stderr, "Unable to reallocate list.");
			exit(1);
        }
        list->elements = new_elements;
        list->capacity = new_capacity;
    }
    memcpy(list->elements + (list->length * list->element_size), ref, list->element_size);
    list->length++;
}
