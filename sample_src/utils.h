#ifndef UTILS_H
#define UTILS_H

#include <time.h>
#include <stdbool.h>

#define TIME_SIZE 20 // Size of time formated in the way we want for logfile entries

// Filepair type is the main type for directory control
typedef struct {
    char *source_dir;
    char *target_dir;
    short int status; // The exercise said to use status as a field, but in my code is useless
    time_t last_sync_time;
    bool active;
    int error_count;
    int watch_descriptor; // Added watch descriptor field so I know which pair an inotify event is refering to
} filepair_t;

// filepair functions
filepair_t *make_pair(char *src, char *trgt, short int status, time_t ls_time, bool active, int error_count, int wd);
bool pair_eq(char *src, char *trgt, filepair_t *dt);
bool pair_eq_src(char *src, filepair_t *dt);
bool pair_eq_trgt(char *trgt, filepair_t *dt);
bool pair_eq_wd(int wd, filepair_t *dt);
void free_pair(filepair_t *pr);


// Worker info type is used for active worker's information
typedef struct {
    pid_t pid;
    char *source_dir;
    char *operation;
    int read_fd; // Pipe's read file descriptor
    bool report_to_log; // If this is set to true and operation is FULL, the program reports back to console
} worker_info_t;

// worker info type functions
worker_info_t *make_worker_info(pid_t pid, char *src, char *operation, int read_fd, bool report_to_log);
bool worker_info_eq(pid_t pid, worker_info_t *wk);
bool worker_info_eq_src(char *src_dir, worker_info_t *wk);
void free_worker_info(worker_info_t *wk);


// For formating time in given format
void format_time(char (*formated_time)[], time_t t);

// For easier allocation checking
void malloc_check(void *);

// For easier fifo management
void fifo_create(char *path, int perms);
int fifo_open(char *path, int flag);
void fifo_delete(char *path);

// For easier string management
void copy_string(char **target, char *original);
void concat_string(char **target, char *original);

#endif 