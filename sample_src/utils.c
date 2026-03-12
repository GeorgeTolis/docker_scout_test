#include "utils.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

// Checks if a ptr is null (used after allocating a pointer)
void malloc_check(void *ptr){
    if (!ptr){
        perror("malloc");
        exit(1);
    }
}

// Allocates a filepair struct
filepair_t *make_pair(char *src, char *trgt, short int status, time_t ls_time, bool active, int error_count, int wd){
    filepair_t *pair = malloc(sizeof(filepair_t));
    malloc_check(pair);

    pair->source_dir = src;
    pair->target_dir = trgt;
    pair->status = status;
    pair->last_sync_time = ls_time;
    pair->active = active;
    pair->error_count = error_count;
    pair->watch_descriptor = wd;
}

// Equality check based on source and target directories
bool pair_eq(char *src, char *trgt, filepair_t *pr){
    return (strcmp(src, pr->source_dir) == 0 && strcmp(trgt, pr->target_dir) == 0);
}

// Equality check based on source directory
bool pair_eq_src(char *src, filepair_t *pr){
    return (strcmp(src, pr->source_dir) == 0);
}

// Equality check based on target directory
bool pair_eq_trgt(char *trgt, filepair_t *pr){
    return (strcmp(trgt, pr->target_dir) == 0);
}

// Equality check based on watch descriptor
bool pair_eq_wd(int wd, filepair_t *pr){
    return (wd == pr->watch_descriptor);
}

// Frees memory that coresponds to a filepairs
void free_pair(filepair_t *pr){
    free(pr->source_dir);
    free(pr->target_dir);
    free(pr);
}

// Allocates a worker info struct
worker_info_t *make_worker_info(pid_t pid, char *src, char *operation, int read_fd, bool report_to_log){
    worker_info_t *worker = malloc(sizeof(worker_info_t));
    malloc_check(worker);

    worker->pid = pid;
    copy_string(&(worker->source_dir), src);
    copy_string(&(worker->operation), operation);
    worker->read_fd = read_fd;
    worker->report_to_log = report_to_log;
}

// Equality check based on process id
bool worker_info_eq(pid_t pid, worker_info_t *wk){
    return (pid == wk->pid);
}

// Equality check based on source directory
bool worker_info_eq_src(char *src_dir, worker_info_t *wk){
    return (strcmp(src_dir, wk->source_dir) == 0);
}

// Frees up memory that corresponds to worker info struct
void free_worker_info(worker_info_t *wk){
    free(wk->source_dir);
    free(wk->operation);
    free(wk);
}

// Fills array formated_time with the "t" variables time, but formated in the exercise's way
void format_time(char (*formated_time)[], time_t t){
	struct tm *tmp;
	tmp = localtime(&t);
	
	strftime(*formated_time, TIME_SIZE, "%Y-%m-%d %H:%M:%S", tmp); // Found this on thie internet
}

// Creates a fifo with the given name
void fifo_create(char *path, int perms){
    if (mkfifo(path , perms) == -1) {
        if (errno != EEXIST) {
            perror("receiver : mkfifo") ;
            exit(1);
        }
    }
}

// Opens a fifo and returns it's file descriptor
int fifo_open(char *path, int flag){
    int fd;
    if ((fd = open(path, flag)) == -1){
        perror ("open") ;
        exit (1) ;
    }
    return fd;
}

// Deletes a fifo from the filesystem
void fifo_delete(char *path){
    struct stat st;
    if (stat(path, &st) != 0) return; // Only delete fifo if it exist
    
    if(unlink(path) == -1){
        perror("unlink");
        exit(1);
    }
}

// "Duplicate" a string with allocation
void copy_string(char **target, char *original){
    *target = malloc(sizeof(char) * (strlen(original) + 1));
    malloc_check(*target);
    strcpy(*target, original);
}

// Concat a string with reallocation
void concat_string(char **target, char *original){
    *target = realloc(*target, (strlen(*target) + strlen(original) + 1));
    malloc_check(*target);
    strcat(*target, original);
}
