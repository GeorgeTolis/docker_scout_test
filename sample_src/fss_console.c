#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h>
#include "utils.h"

int logfile_fd = -1; // Store flag value
int fss_in_fd = -1, fss_out_fd = -1; // FIFO file descriptors
bool shutdown_flag = false; // Shutdown flag, will be set to true if user wants shutdown
int times_to_read; // Will tell read_respose() how many times to read from pipe

// Initialize flag's value
void get_flag_value(int argc, char *argv[]){
    int opt;
    while ((opt = getopt(argc, argv, "l:")) != -1) {
        switch (opt) {
            case 'l': // Logfile flag
                if ((logfile_fd = open(optarg , O_WRONLY | O_CREAT | O_APPEND, 0644)) == -1){
                    perror ("open") ;
                    exit (1) ;
                }
                break;
            default:
                fprintf(stderr, "Unknown option: %c\n", optopt);
                exit(1);
        }
    }
}

// Connect to fss_in and fss_out pipes created by the manager
void connect_to_communication_pipes(){
    // Open fifos
    fss_in_fd = fifo_open("fss_in", O_WRONLY | O_NONBLOCK);
    fss_out_fd = fifo_open("fss_out", O_RDONLY);
}

// Checks if given command is valid
bool is_valid_command(char *user_cmd){
    times_to_read = 0;
    char *cmd, *src_dir, *trgt_dir;
    
    cmd = strtok(user_cmd, " \n");
    if (cmd == NULL){
        return false;
    }

    if (strcmp(cmd, "add") == 0){
        times_to_read = 2; // We read 2 times when we want to add

        // add command needs 2 arguments
        src_dir = strtok(NULL, " ");
        trgt_dir = strtok(NULL, " \n");

        if (src_dir == NULL || trgt_dir == NULL){
            return false;
        }
    } else if (strcmp(cmd, "cancel") == 0 || strcmp(cmd, "status") == 0
                || strcmp(cmd, "sync") == 0){
        
        times_to_read = (strcmp(cmd, "cancel") == 0)? 1 : 2; // We only read once whe we have cancel, otherwise 2 times
        
        // cancel, status and sync commands have only one argument
        src_dir = strtok(NULL, " \n");
        
        if (src_dir == NULL){
            return false;
        }

        // If there is a second argument it's still an invalid command
        trgt_dir = strtok(NULL, " \n");
        if (trgt_dir != NULL){
            return false;
        }
    } else if (strcmp(cmd, "shutdown") == 0){
        times_to_read = 4; // We read 4 messages when manager shuts down
        src_dir = strtok(NULL, " \n");

        // Shutdown doesn't have arguments
        if (src_dir != NULL){
            return false;
        }
        shutdown_flag = true; // Set shutdown flag to true, cause shutdown is requested
    } else {
        return false; // not valid command
    }

    return true; // valid command and command syntax
}

// Send command to manager
void send_command(char *user_cmd){
    ssize_t bytes_written;
    if ((bytes_written = write(fss_in_fd, user_cmd, strlen(user_cmd) + 1)) == -1){
        perror("write");
        exit(1);
    }
    sleep(0.001); // Added this because I want manager to read one command at a time
}

// Report user command to log
void report_log(char *user_cmd){
    char cur_time[TIME_SIZE], message[512];
    format_time(&cur_time, time(NULL));

    // Parse command
    char *cmd, *src_dir, *trgt_dir;
    cmd = strtok(user_cmd, " ");
    src_dir = strtok(NULL, " ");
    trgt_dir = strtok(NULL, " ");

    // Format report in the way the exercise tells us
    if (src_dir == NULL){
        snprintf(message, sizeof(message), "[%s] Command %s\n", cur_time, cmd);
    } else if (trgt_dir == NULL){
        snprintf(message, sizeof(message), "[%s] Command %s %s\n", cur_time, cmd, src_dir);
    } else {
        snprintf(message, sizeof(message), "[%s] Command %s %s -> %s\n", cur_time, cmd, src_dir, trgt_dir);
    }

    // Write log report
    write(logfile_fd, message, strlen(message));
}

// Read response from manager
void read_response(int times){
    char buf[1024];
    ssize_t bytes_read;

    // Read as many times as the variable times says, exept for some exeptions
    for (int i = 0; i < times; i++){
        if ((bytes_read = read(fss_out_fd, buf, 1024)) > 0){
            write(1, buf, bytes_read);
            write(logfile_fd, buf, strlen(buf));

            // Check for exeptions
            char *temp;
            temp = strtok(buf, "]");
            temp = strtok(NULL, " ");

            // Received strange report
            if (temp == NULL){
                break;
            }

            // Received error report (ex. Directory not monitored) or report from another command sent
            if (strcmp(temp, "Directory") == 0 || strcmp(temp, "Already") == 0 || strcmp(temp, "Monitoring") == 0){
                break;
            }
        }
    }
}

// Terminate console and clean up resources (doubles as singal handler)
void terminate_console(int signo) {
    // Close file descriptors
    if (logfile_fd != -1) close(logfile_fd);
    if (fss_in_fd != -1) close(fss_in_fd);
    if (fss_out_fd != -1) close(fss_out_fd);

    if (signo == SIGINT){
        fprintf(stderr, "\n");
        exit(0);
    }
}

// Initializes SIGINT and SIGCHLD signal handlers
void init_signal_handler() {
    struct sigaction sa;

    // SIGINT handler
    sa.sa_handler = terminate_console;
    sigemptyset(&(sa.sa_mask));
    sigaddset(&(sa.sa_mask), SIGINT);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction(SIGINT)");
        exit(1);
    }
}


int main(int argc, char *argv[]){
    if (argc != 3){
        fprintf(stderr, "Usage: ./fss_console -l <console-logfile>\n");
        exit(1);
    }

    // Initialize console
    init_signal_handler();
    get_flag_value(argc, argv);
    connect_to_communication_pipes();

    // Main loop
    char *temp, *user_cmd = NULL;
    size_t length = 0;
    ssize_t bytes;
    while (!shutdown_flag){
        printf("> ");
        fflush(stdout);

        // Get command from user
        getline(&user_cmd, &length, stdin);
        user_cmd[strcspn(user_cmd, "\n")] = '\0'; // Null terminate command

        // Check if command is valid
        copy_string(&temp, user_cmd);
        if (!is_valid_command(temp)){
            fprintf(stderr, "Not valid command\n");
            free(temp);
            continue;
        }
        free(temp);

        // Report command to log
        copy_string(&temp, user_cmd);
        report_log(temp);
        free(temp);

        send_command(user_cmd); // Send command to manager
        read_response(times_to_read); // Read manager respones
        printf("\n"); fflush(stdout);
    }
    
    // Terminate console
    free(user_cmd);
    terminate_console(0);
    exit(0);
}