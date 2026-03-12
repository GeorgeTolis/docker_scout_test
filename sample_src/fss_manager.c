#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/inotify.h>
#include <sys/select.h>
#include <signal.h>
#include <time.h>
#include "utils.h"
#include "list.h"
#include "queue.h"

int logfile_fd = -1, config_fd = -1, worker_limit = 5, active_worker_count = 0; // Store flag values + active worker count
int fss_in_fd = -1, fss_out_fd = -1; // FIFO file descriptors
List *sync_info_mem_store; // List that store syncing pairs
Worker_List *workers; // Active worker list
int worker_qid; // Message Queue ID for queued commands
int inotify_fd = -1;
volatile sig_atomic_t check_children_flag = 0; // When this flag is set to true, main loop will check if a child finished and make the right actions
bool shutdown_flag = false; // If shutdown message is sent, then this flag will turn true

// Cleans up used resources (also doubles as a signal handler)
void terminate_manager(int signo) {
    // Free directory watching list
    destroy_list(&sync_info_mem_store, inotify_fd);
    close(inotify_fd);
    
    // Close used file descriptors
    close(logfile_fd);
    close(config_fd);
    close(fss_in_fd);
    close(fss_out_fd);

    // Destroy worker queue and list
    empty_out_queue(worker_qid);
    destroy_worker_list(&workers);
    
    // Delete named pipes (no longer needed)
    fifo_delete("fss_in");
    fifo_delete("fss_out");

    if (signo == SIGINT){
        fprintf(stderr, "\n");
        exit(1);
    }
}

// Writes a message to stdout and/or logfile and/or pipe if the coresponding flags are set
void write_log_message(char *main_mes, char *src_dir, char *trgt_dir, bool write_in_stdout, bool write_in_log, bool write_in_pipe){
    char cur_time[TIME_SIZE], message[512];
    format_time(&cur_time, time(NULL));

    // Format message based on the input
    if (src_dir == NULL){
        snprintf(message, sizeof(message), "[%s] %s\n", cur_time, main_mes);
    } else if (trgt_dir == NULL){
        snprintf(message, sizeof(message), "[%s] %s %s\n", cur_time, main_mes, src_dir);
    } else {
        snprintf(message, sizeof(message), "[%s] %s %s -> %s\n", cur_time, main_mes, src_dir, trgt_dir);
    }

    // Write message
    if (write_in_stdout){
        printf("%s", message);
    }
    if (write_in_log){
        write(logfile_fd, message, strlen(message));
    }
    if (write_in_pipe){
        write(fss_out_fd, message, strlen(message) + 1);
    }
}

// Makes exec report entry in logfile. Returns error count in exec report
int write_exec_report(worker_info_t *wk, filepair_t *pair, time_t t, char *report){
    int exec_errors = 0; // This variable will count the number of erros in the exec report
    char cur_time[TIME_SIZE];
    format_time(&cur_time, t);

    // Get result (FULL/PARTIAL/ERROR) and details from worker report
    char *temp, *result, *details;
    temp = strtok(report, "\n");
    temp = strtok(NULL, " ");
    result = strtok(NULL, "\n");
    temp = strtok(NULL, " ");
    details = strtok(NULL, "\n");

    // if the worker return with errors and operation was not full, 
    // we report the file that had the error to the logfile details
    if (strcmp(result, "ERROR") == 0 && strcmp(wk->operation, "FULL") != 0){
        temp = strtok(NULL, "\n");
        temp = strtok(NULL, " ");
        details = strtok(NULL, "\n");
        exec_errors = 1;
    }

    // If operation was FULL, errors will be the number of files skipped
    if (strcmp(wk->operation, "FULL") == 0){
        char *temp, *files_skipped;
        copy_string(&temp, details);
        files_skipped = strtok(temp, ",");
        files_skipped = strtok(NULL, " ");
        exec_errors = atoi(files_skipped);
        free(temp);
    }

    // Format string that will go in the logfile 
    char log_output[512];
    snprintf(log_output, sizeof(log_output), "[%s] [%s] [%s] [%d] [%s] [%s] [%s]\n", cur_time, wk->source_dir, pair->target_dir, wk->pid, wk->operation, result, details);
    if (write(logfile_fd, log_output, strlen(log_output)) == -1){
        perror("write");
    }

    return exec_errors;
}

// Forks a child and begins a worker
void begin_worker(char *src_dir, char *trgt_dir, char *filename, char *operation, bool report_to_log){
    // Open communication pipe
    int pipe_fd[2];
    if (pipe(pipe_fd) == -1){
        perror("pipe");
        terminate_manager(0);
        exit(1);
    }

    // Fork a child
    int child_pid = fork();
    switch (child_pid){
    case -1: // Error while forking
        perror("fork");
        terminate_manager(0);
        exit(1);
    case 0: // Child process
        close(pipe_fd[0]); // Close read from pipe
        close(1);
        dup2(pipe_fd[1], 1); // Dup pipe to stdout
        
        // Exec new worker
        if (execlp("./worker", "worker", src_dir, trgt_dir, filename, operation, NULL) == -1){
            delete_worker_from_list(&workers, getpid());
            active_worker_count--;
            exit(1);
        }
    default: // Parrent process
        close(pipe_fd[1]); // Close write from pipe
        worker_info_t *wk = make_worker_info(child_pid, src_dir, operation, pipe_fd[0], report_to_log);       
        push_to_worker_list(&workers, wk); // Insert new worker into active worker list
        active_worker_count++;
        return;
    }
}

// Enqueues a command for a worker
void enqueue_worker(char *src_dir, char *trgt_dir, char *filename, char *operation, bool report_to_log){
    // If we haven't reached the worker limit begin new worker
    if (active_worker_count < worker_limit){
        begin_worker(src_dir, trgt_dir, filename, operation, report_to_log);
    } else { // Else we enqueue command
        // Make a string that holds the coresponding command
        // This is done cause queue is a message queue IPC
        char *cmd = NULL;
        copy_string(&cmd, src_dir);
        concat_string(&cmd, " ");
        concat_string(&cmd, trgt_dir);
        concat_string(&cmd, " ");
        concat_string(&cmd, filename);
        concat_string(&cmd, " ");
        concat_string(&cmd, operation);
        concat_string(&cmd, " ");
        concat_string(&cmd, (report_to_log)? "1" : "0");

        // Enqueue command
        enqueue(worker_qid, cmd, 1);
        free(cmd);
    }
}

// Initializes flag values
void get_flag_values(int argc, char *argv[]){
    int opt;
    while ((opt = getopt(argc, argv, "l:c:n:")) != -1) {
        switch (opt) {
            case 'l': // Logfile flag
                if ((logfile_fd = open(optarg , O_WRONLY | O_CREAT | O_TRUNC , 0644)) == -1){
                    perror("open");
                    exit(1);
                }
                break;
            case 'c': // Config file flag
                if ((config_fd = open(optarg, O_RDONLY, 0644)) == -1){
                    perror("open");
                    exit(1);
                }
                break;
            case 'n': // Number of workers flag
                worker_limit = atoi(optarg);
                break;
            default: // If another flag is given, error
                fprintf(stderr, "Unknown option: %c\n", optopt);
                exit(1);
        }
    }
}

// Creates and connects to named pipes
void make_comunication_pipes(){
    // In case something went wrong and the fifos still exist
    unlink("fss_in");
    unlink("fss_out");

    // Make fifos for comunication with console
    fifo_create("fss_in", 0666);
    fifo_create("fss_out", 0666);

    // Open fifos
    fss_in_fd = fifo_open("fss_in", O_RDONLY | O_NONBLOCK);
    fss_out_fd = fifo_open("fss_out", O_RDWR | O_NONBLOCK);
}

// Reads config and initializes watched directories
void initialize_sync_info(){
    // Create filepair list
    create_list(&sync_info_mem_store);

    // Open config file
    FILE *config_file;
    if ((config_file = fdopen(config_fd, "r")) == NULL){
        perror("fdopen");
        exit(1);
    }

    int split_index, end_index, wd;
    char *src_dir, *trgt_dir;
    filepair_t *pair;
    time_t ls_time;

    // Read each line and initialize a filepair for each pair of directories
    char *line = NULL;
    size_t length = 0;
    while (getline(&line, &length, config_file) > 0){
        // Split the line in two
        split_index = strcspn(line, " ");
        end_index = strcspn(line, "\n");
        line[split_index] = '\0';
        line[end_index] = '\0';

        // Get source and target directories
        copy_string(&src_dir, line);
        copy_string(&trgt_dir, line + split_index + 1);

        // If one of them is already in the list, then don't do anything
        if (list_search(sync_info_mem_store, src_dir) || list_search(sync_info_mem_store, trgt_dir) || 
            list_search_trgt(sync_info_mem_store, trgt_dir) || list_search_trgt(sync_info_mem_store, src_dir)){
            
            free(src_dir);
            free(trgt_dir);
            continue;
        }

        // Get dummy time. This time will change when initial sync is finished
        // and process goes into the signal handler
        time(&ls_time);
        if ((wd = inotify_add_watch(inotify_fd, src_dir, IN_CREATE | IN_MODIFY | IN_DELETE)) == -1){
            fprintf(stderr, "Could not watch %s\n", src_dir);
            continue;
        }
        pair = make_pair(src_dir, trgt_dir, 0, ls_time, true, 0, wd); // Make filepair
        push_to_list(&sync_info_mem_store, pair); // Add filepair to list

        // Report to logfile that source - target pair was added
        write_log_message("Added directory:", src_dir, trgt_dir, false, true, false);
        write_log_message("Monitoring started for", src_dir, NULL, false, true, false);

        // Begin initial full sync
        enqueue_worker(pair->source_dir, pair->target_dir, "ALL", "FULL", false);
    }

    free(line);
    fclose(config_file);
}

// Initialize worker datastructs
void initialize_workers(){
    create_worker_list(&workers);
    worker_qid = init_queue(WORKERQ_KEY);
}

// Reset file descriptor set with the 2 values we need
void reset_fd_set(fd_set *fds){
    FD_ZERO(fds);
    FD_SET(inotify_fd, fds);
    FD_SET(fss_in_fd, fds);
}

// Do right action if we have an inotify event
void sync_notified_dir(){
    // Read inotify events from file descriptor
    ssize_t bytes_read;
    char buf[4096];
    if ((bytes_read = read(inotify_fd, buf, 4096)) == -1){
        perror("read");
        terminate_manager(0);
        exit(1);
    }

    // For every inotify event read (I found this kind of loop on the internet)
    char *p;
    struct inotify_event *event;
    for (p = buf; p < buf + bytes_read; p += sizeof(struct inotify_event) + event->len){
        event = (struct inotify_event *)p; // get event

        // If watch descriptor not in the list continue
        filepair_t *pair;
        if ((pair = list_search_wd(sync_info_mem_store, event->wd)) == NULL){
            continue;
        }

        // I saw this condition on the internet and my code didn't
        // run correctly without it, so I kept it
        if (event->len == 0) continue;

        // Get operation needed
        char *operation = NULL;
        if (event->mask & IN_CREATE){
            operation = "ADDED";
        } else if (event->mask & IN_MODIFY){
            operation = "MODIFIED";
        } else if (event->mask & IN_DELETE){
            operation = "DELETED";
        }

        // Enqueue worker for the sync needed
        enqueue_worker(pair->source_dir, pair->target_dir, event->name, operation, false);
    }
}

// Do right action if we have a message from fss_console
void receive_and_handle_console_cmd(){
    char buf[1024];
    ssize_t bytes_read = 1;
    // Read message
    if ((bytes_read = read(fss_in_fd, buf, sizeof(buf))) <= 0){
        return; // if invalid read, return
    }
    buf[bytes_read] = '\0'; // Null terminate cause it causes problems with string functions if not

    // Parse message and do the coresponding task
    char *cmd = strtok(buf, " \n");
    if (strcmp(cmd, "add") == 0){ // Add a new pair in the watch list
        char *temp_src_dir = strtok(NULL, " ");
        char *temp_trgt_dir = strtok(NULL, " \n");
        if (temp_src_dir == NULL || temp_trgt_dir == NULL){
            return;
        }

        // Duplicate strings. I added this because I had double free
        // errors when freeing the list in terminate manager
        char *src_dir, *trgt_dir;
        copy_string(&src_dir, temp_src_dir);
        copy_string(&trgt_dir, temp_trgt_dir);

        filepair_t *pr;
        if ((pr = list_search(sync_info_mem_store, src_dir)) || list_search(sync_info_mem_store, trgt_dir) || 
            list_search_trgt(sync_info_mem_store, trgt_dir) || list_search_trgt(sync_info_mem_store, src_dir)){
        
            // If directory already monitored, write message to console and return    
            if (pr == NULL){
                write_log_message("Already in queue:", src_dir, NULL, true, false, true);
                free(src_dir);
                free(trgt_dir);
                return;
            }
            if (pr->active == true){
                write_log_message("Already in queue:", src_dir, NULL, true, false, true);
                free(src_dir);
                free(trgt_dir);
                return;
            }

            // If directory was cancelled (active = false) and target isn't the original target, report to console and return
            if (strcmp(pr->target_dir, trgt_dir) != 0){
                write_log_message("Already in queue:", src_dir, NULL, true, false, true);
                free(src_dir);
                free(trgt_dir);
                return;
            }
        }

        // Else we create a new pair and add it to the list
        int wd;
        time_t ls_time;
        time(&ls_time);
        if ((wd = inotify_add_watch(inotify_fd, src_dir, IN_CREATE | IN_MODIFY | IN_DELETE)) == -1){
            fprintf(stderr, "Could not watch %s\n", src_dir);
        }

        filepair_t *pair;
        if (pr == NULL){
            pair = make_pair(src_dir, trgt_dir, 0, ls_time, true, 0, wd);
            push_to_list(&sync_info_mem_store, pair);
        } else {
            pair = pr;
            pr->watch_descriptor = wd;
            pair->active = true;
        }
        write_log_message("Added directory:", src_dir, trgt_dir, true, true, true); // Report to logfile and console

        sleep(0.001); // Added this because console would block while reading response
        write_log_message("Monitoring started for", src_dir, NULL, true, true, true); // Report to logfile and console

        // Enqueue worker for initial full sync of filepair
        enqueue_worker(src_dir, trgt_dir, "ALL", "FULL", false); 

    } else if (strcmp(cmd, "cancel") == 0){ // Cancel watch of a source directory
        char *src_dir = strtok(NULL, " \n");
        if (src_dir == NULL){
            return;
        }

        // Search for source directory
        filepair_t *pair = list_search(sync_info_mem_store, src_dir);
        if (pair == NULL){
            // If it's not on the list, report back to console and return
            write_log_message("Directory not monitored:", src_dir, NULL, true, false, true);
            return;
        }

        // If pair is inactive (not monitored), report back to console
        if (pair->active == false){
            write_log_message("Directory not monitored:", src_dir, NULL, true, false, true);
            return;
        }
        
        pair->active = false; // Cancel watch
        inotify_rm_watch(inotify_fd, pair->watch_descriptor);

        // Report back to console
        write_log_message("Monitoring stopped for", src_dir, NULL, true, true, true);

    } else if (strcmp(cmd, "status") == 0) { // Give status for a source directory
        char *src_dir = strtok(NULL, " \n");
        if (src_dir == NULL){
            return;
        }

        // Search for source directory
        filepair_t *pair = list_search(sync_info_mem_store, src_dir);
        if (pair == NULL){
            // If it's not on the list, report back to console and return
            write_log_message("Directory not monitored:", src_dir, NULL, true, false, true);
            return;
        }

        // Report status request to console
        write_log_message("Status requested for", src_dir, NULL, true, false, true);

        // Format status
        char status_report[512];
        char ls_time[TIME_SIZE];
        format_time(&ls_time, pair->last_sync_time);
        snprintf(status_report, sizeof(status_report), "Directory: %s\nTarget: %s\nLast Sync: %s\nErrors: %d\nStatus: %s\n",
                pair->source_dir, pair->target_dir, ls_time, pair->error_count, (pair->active)? "Active" : "Inactive");
                
        // Report status
        printf("%s", status_report);
        sleep(0.001); // Added this because console would block while reading response
        write(fss_out_fd, status_report, strlen(status_report) + 1); // Report back to console

    } else if (strcmp(cmd, "sync") == 0){ // Sync filepair
        char *src_dir = strtok(NULL, " \n");
        if (src_dir == NULL){
            return;
        }

        // Search for source directory
        filepair_t *pair = list_search(sync_info_mem_store, src_dir);
        if (pair == NULL){
            // If it's not on the list, return 
            // I didn't add a report cause the exercise didn't say anything
            return;
        }

        // If a worker is already syncing the filepair, dont do anything
        worker_info_t *wk = worker_list_search_src(workers, pair->source_dir);
        if (wk != NULL){
            write_log_message(" Sync already in progress", src_dir, NULL, true, false, true);
        }

        // Report to console and enqueue worker
        write_log_message("Syncing directory:", pair->source_dir, pair->target_dir, true, true, true);
        enqueue_worker(pair->source_dir, pair->target_dir, "ALL", "FULL", true);

    } else if (strcmp(cmd, "shutdown") == 0){ // Shutdown manager
        shutdown_flag = true;
        write_log_message("Shutting down manager...", NULL, NULL, true, false, true);
        return;
    }
}

// SIGCHLD signal handler
void child_finished(int signo){
    check_children_flag = 1;
}

// Handles exec report parsing and begining queued workers
void check_children_and_make_report(){
    int status;
    pid_t pid;

    // While instead of if, because there might be 2 or more child
    // processes that conclude together
    while ((pid = waitpid(-1, &status, WNOHANG)) != -1){
        // Search for coresponding worker
        worker_info_t *wk = worker_list_search(workers, pid);
        
        // If worker is not on the list or pid is 0, return
        // I added this cause my program would segfault and if I did it with continue, it would block at waitpid 
        if (!wk || !pid) return;

        // Read exec report
        char buf[1024];
        ssize_t bytes = read(wk->read_fd, buf, 1024);
        buf[bytes] = '\0';

        // Find filepair and update it's last sync time
        // Here no check is needed, because wk->source_dir is surely on the list
        filepair_t *pair = list_search(sync_info_mem_store, wk->source_dir);
        time_t ls_time;
        time(&ls_time);

        // If operation was full, update last sync time
        if (strcmp(wk->operation, "FULL") == 0){
            // If worker did a sync requested from console, report back  
            if (wk->report_to_log){
                write_log_message(" Sync completed", pair->source_dir, pair->target_dir, true, true, true);
            }
            pair->last_sync_time = ls_time;            
        }

        // Add exec report to logfile and update filepair's error count
        int exec_errors = write_exec_report(wk, pair, ls_time, buf);
        pair->error_count += exec_errors;

        // close pipe's read_fd and delete worker from active workers
        close(wk->read_fd);
        delete_worker_from_list(&workers, pid);
        active_worker_count--;

        // If we can begin a new worker, try
        if (active_worker_count < worker_limit){
            // If there is no enqueued task continue with next finished child
            char *command = dequeue(worker_qid);
            if (command == NULL){
                continue;
            }

            // Else parse command
            char *src_dir = strtok(command, " ");
            char *trgt_dir = strtok(NULL, " ");
            char *filename = strtok(NULL, " ");
            char *operation = strtok(NULL, " ");
            char *rtl = strtok(NULL, " \n");
            bool report_to_log = atoi(rtl);

            // Begin worker for coresponding task
            begin_worker(src_dir, trgt_dir, filename, operation, report_to_log);
            free(command);
        }
    }

    check_children_flag = false;
}

// Waits for already running workers before shutdown
void wait_for_running_workers(){
    // Ignore SIGCHLD and wait for the workers to finish 
    // I added this so that the log messages match the examples given
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sigemptyset(&(sa.sa_mask));
    sigaddset(&(sa.sa_mask), SIGCHLD);
    sa.sa_flags = 0;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction(SIGCHLD)");
        exit(1);
    }
    
    // Wait for already running workers
    write_log_message("Waiting for all active workers to finish.", NULL, NULL, true, false, true);
    while(active_worker_count > 0){
        // Wait for child
        pid_t pid = wait(NULL);
        if (pid == -1) break; // No more children 

        // Find coresponding worker
        worker_info_t *wk = worker_list_search(workers, pid);
        if (!wk || !pid) continue; // Same as SIGCHLD signal handler

        // Read exec report
        char buf[1024];
        ssize_t bytes = read(wk->read_fd, buf, 1023);
        buf[bytes] = '\0';

        // Find filepair and update it's last sync time
        filepair_t *pair = list_search(sync_info_mem_store, wk->source_dir);
        time_t ls_time;
        time(&ls_time);
        pair->last_sync_time = ls_time;

        // Add exec report to logfile and update filepair's error count
        int exec_errors = write_exec_report(wk, pair, ls_time, buf);
        pair->error_count += exec_errors;

        // Close pipe's read and delete worker
        close(wk->read_fd);
        delete_worker_from_list(&workers, pid);
        active_worker_count--;
    }
}

// Begin rest of enqueued workers before shutdown
void complete_enqueued_workers(){
    write_log_message("Processing remaining queued tasks.", NULL, NULL, true, false, true);
    // Enqueue remaining workers if necessary
    while (active_worker_count < worker_limit){
        char *command = dequeue(worker_qid);
        if (command == NULL){
            break;
        }

        // Parse command
        char *src_dir = strtok(command, " ");
        char *trgt_dir = strtok(NULL, " ");
        char *filename = strtok(NULL, " ");
        char *operation = strtok(NULL, " ");
        char *rtl = strtok(NULL, " \n");
        bool report_to_log = atoi(rtl);

        // Begin worker for coresponding task
        begin_worker(src_dir, trgt_dir, filename, operation, report_to_log);
        free(command);
    }

    // Wait for working processes and enqueue more if necessary
    // same logic as check_children_and_make_report() but wait without NOHANG
    int status;
    pid_t pid;
    while ((pid = wait(&status)) != -1){
        // Search for coresponding worker
        worker_info_t *wk = worker_list_search(workers, pid);
        
        // If worker is not on the list or pid is 0, something went wrong
        if (!wk || !pid) continue;

        // Read exec report
        char buf[1024];
        ssize_t bytes = read(wk->read_fd, buf, 1024);
        buf[bytes] = '\0';

        // Find filepair and update it's last sync time
        // Here no check is needed, because wk->source_dir is surely on the list
        filepair_t *pair = list_search(sync_info_mem_store, wk->source_dir);
        time_t ls_time;
        time(&ls_time);

        // If operation was full, update last sync time
        if (strcmp(wk->operation, "FULL") == 0){  
            pair->last_sync_time = ls_time;            
        }

        // Add exec report to logfile and update filepair's error count
        int exec_errors = write_exec_report(wk, pair, ls_time, buf);
        pair->error_count += exec_errors;

        // close pipe's read_fd and delete worker from active workers
        close(wk->read_fd);
        delete_worker_from_list(&workers, pid);
        active_worker_count--;

        // If we can begin a new worker, try
        if (active_worker_count < worker_limit){
            // If there is no enqueued task continue with next finished child
            char *command = dequeue(worker_qid);
            if (command == NULL){
                continue;
            }

            // Else parse command
            char *src_dir = strtok(command, " ");
            char *trgt_dir = strtok(NULL, " ");
            char *filename = strtok(NULL, " ");
            char *operation = strtok(NULL, " ");
            char *rtl = strtok(NULL, " \n");
            bool report_to_log = atoi(rtl);

            // Begin worker for coresponding task
            begin_worker(src_dir, trgt_dir, filename, operation, report_to_log);
            free(command);
        }
    }
}

// Initializes SIGINT and SIGCHLD signal handlers
void init_signal_handlers() {
    struct sigaction sa;
    
    // SIGCHLD handler
    sa.sa_handler = child_finished;
    sigemptyset(&(sa.sa_mask));
    sigaddset(&(sa.sa_mask), SIGCHLD);
    sa.sa_flags = 0;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction(SIGCHLD)");
        exit(1);
    }

    // SIGINT handler
    sa.sa_handler = terminate_manager;
    sigemptyset(&(sa.sa_mask));
    sigaddset(&(sa.sa_mask), SIGINT);
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction(SIGINT)");
        exit(1);
    }
}


int main(int argc, char *argv[]){
    if (argc != 7 && argc != 5){
        fprintf(stderr, "Usage: ./fss_manager -l <manager_logfile> -c <config_file> -n <worker_limit>\n");
        exit(1);
    }

    init_signal_handlers();

    // Get inotify file descriptor
    if ((inotify_fd = inotify_init()) == -1){
        perror("inotify_init");
        exit(1);
    }

    // Initialize manager
    get_flag_values(argc, argv);
    initialize_workers();
    initialize_sync_info();
    make_comunication_pipes();

    // Main loop of manager
    fd_set fds;
    int activity, max_fd = ((inotify_fd > fss_in_fd)? inotify_fd : fss_in_fd);
    while(!shutdown_flag){
        reset_fd_set(&fds); // Reset fd set because select changes it
        // Block until console input of an inotify event arrives 
        if ((activity = select(max_fd + 1, &fds, NULL, NULL, NULL)) == -1){
            if (errno == EINTR){ 
                // If select was interupted by a signal, we check if it was SIGCHLD, so we make logfile logs 
                if (check_children_flag){
                    check_children_and_make_report();
                }
                continue;
            } else {
                perror("select");
                terminate_manager(0);
                exit(1);
            }
        }

        // Do coresponding action
        if (FD_ISSET(inotify_fd, &fds)){
            sync_notified_dir();    
        } else if (FD_ISSET(fss_in_fd, &fds)){
            receive_and_handle_console_cmd();
        }

        // If a child has finished, get it's report, update logfile and start new children if necessary 
        if (check_children_flag){
            check_children_and_make_report();
        }
    }

    // If shutdown was requested from console, do waiting and dequeueing.
    // I added this as an "if" because there were problems if I didn't put it in an if statement
    if (shutdown_flag){
        wait_for_running_workers();
        complete_enqueued_workers();
        write_log_message("Manager shutdown complete.", NULL, NULL, true, false, true);
    }
    
    // Cleanup resources
    terminate_manager(0);
    exit(0);
}