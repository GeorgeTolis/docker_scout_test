#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <sys/inotify.h>
#include "list.h"

struct listnode{
    filepair_t *pr;
    struct listnode *next;
};

struct worker_listnode{
    worker_info_t *worker;
    struct worker_listnode *next;
};

/* --- DIRECTORY INFO LIST --- */

// Create an epmty list
void create_list(List **l){
    *l = NULL;
}

// Add element to the start of the list
void push_to_list(List **l, filepair_t *pr){
    List *node = malloc(sizeof(List));
    malloc_check(node);
    node->pr = pr;

    List *cur_head = *l;
    List *new_head = node;
    new_head->next = cur_head;
    *l = new_head;
}

// Searches for a source directory
filepair_t *list_search(List *l, char *src){
    List *cur = l;
    while(cur != NULL){
        if (pair_eq_src(src, cur->pr)){
            return cur->pr;
        }
        cur = cur->next;
    }
    return NULL;
}

// Sarches for a target directory
filepair_t *list_search_trgt(List *l, char *trgt){
    List *cur = l;
    while(cur != NULL){
        if (pair_eq_trgt(trgt, cur->pr)){
            return cur->pr;
        }
        cur = cur->next;
    }
    return NULL;
}

// Sarches for a watch descriptor
filepair_t *list_search_wd(List *l, int wd){
    List *cur = l;
    while(cur != NULL){
        if (pair_eq_wd(wd, cur->pr)){
            return cur->pr;
        }
        cur = cur->next;
    }
    return NULL;
}

void print_list(List *l){
    List *cur = l;
    while (cur){
        printf("(src: %s, trg: %s) -> ", cur->pr->source_dir, cur->pr->target_dir);
        cur = cur->next;
    }
    printf("NULL\n");
}

// De-allocates a list
void destroy_list(List **l, int inotify_fd){
    List *cur = NULL;
    while (*l != NULL) {
        cur = (*l)->next;
        inotify_rm_watch(inotify_fd, (*l)->pr->watch_descriptor);
        free_pair((*l)->pr);
        free(*l);
        *l = cur;
    }
}

/* --- WORKER LIST --- */

// Create an epmty list
void create_worker_list(Worker_List **l){
    *l = NULL;
}

// Add element to the start of the list
void push_to_worker_list(Worker_List **l, worker_info_t *wk){
    Worker_List *node = malloc(sizeof(Worker_List));
    malloc_check(node);
    node->worker = wk;

    Worker_List *cur_head = *l;
    Worker_List *new_head = node;
    new_head->next = cur_head;
    *l = new_head;
}

void print_worker_list(Worker_List *l){
    Worker_List *cur = l;
    while (cur){
        printf("(src: %s, pid: %d) -> ", cur->worker->source_dir, cur->worker->pid);
        cur = cur->next;
    }
    printf("NULL\n");
}

// Deletes a worker with the given pid
void delete_worker_from_list(Worker_List **l, pid_t pid){
    Worker_List *cur = *l;
    Worker_List *prev = NULL;
    while(cur != NULL){
        if (worker_info_eq(pid, cur->worker)){
            if (prev == NULL){
                *l = cur->next;
                free_worker_info(cur->worker);
                free(cur);
                return;
            } else {
                prev->next = cur->next;
                free_worker_info(cur->worker);
                free(cur);
                return;
            }
        }
        prev = cur;
        cur = cur->next;
    }
}

// Search for a pid in the list
worker_info_t *worker_list_search(Worker_List *l, pid_t pid){
    Worker_List *cur = l;
    while(cur != NULL){
        if (worker_info_eq(pid, cur->worker)){
            return cur->worker;
        }
        cur = cur->next;
    }
    return NULL;
}

// Search for a source file in the list
worker_info_t *worker_list_search_src(Worker_List *l, char *src_dir){
    Worker_List *cur = l;
    while(cur != NULL){
        if (worker_info_eq_src(src_dir, cur->worker)){
            return cur->worker;
        }
        cur = cur->next;
    }
    return NULL;
}

// De-allocates the worker list
void destroy_worker_list(Worker_List **l){
    Worker_List *cur = NULL;
    while (*l != NULL) {
        cur = (*l)->next;
        free_worker_info((*l)->worker);
        free(*l);
        *l = cur;
    }
}

