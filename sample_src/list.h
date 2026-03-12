#ifndef LIST_H
#define LIST_H

#include <time.h>
#include "utils.h"

/* List for filepair info */
typedef struct listnode List;

void create_list(List **l);
void destroy_list(List **l, int inotify_fd);
void push_to_list(List **l, filepair_t *pr);
void print_list(List *l);
filepair_t *list_search(List *l, char *src); // search by source directory
filepair_t *list_search_trgt(List *l, char *trgt); // search by target directory
filepair_t *list_search_wd(List *l, int wd); // search by watch descriptor

/* List for worker info */
typedef struct worker_listnode Worker_List;

void create_worker_list(Worker_List **l);
void destroy_worker_list(Worker_List **l);
void push_to_worker_list(Worker_List **l, worker_info_t *pr);
void print_worker_list(Worker_List *l);
void delete_worker_from_list(Worker_List **l, pid_t pid);
worker_info_t *worker_list_search(Worker_List *l, pid_t pid); // search by pid
worker_info_t *worker_list_search_src(Worker_List *l, char *src_dir); // search by source directory

#endif 