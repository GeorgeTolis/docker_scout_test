#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <utime.h>
#include "utils.h"
#include "queue.h"

char *source_dir, *target_dir, *filename, *operation; // Flags
int files_copied = 0, files_skipped = 0, error_count = 0; // Report variables
int error_qid; // Error queue (will include all errors generated while operating)

// Initialize flag values
void get_flag_values(char *argv[]){ // argv always has 5 values    
    copy_string(&source_dir, argv[1]);
    copy_string(&target_dir, argv[2]);
    copy_string(&filename, argv[3]);
    copy_string(&operation, argv[4]);
}

// Clean up resources
void free_memory(){
    free(source_dir);
    free(target_dir);
    free(filename);
    free(operation);
    empty_out_queue(error_qid);
}

// Make a path = dir/filename
char *make_path(char *dir, char *filename){
    char *path;

    copy_string(&path, dir);
    if (dir[strlen(dir)] == '/'){
        concat_string(&path, filename);
    } else {
        concat_string(&path, "/");
        concat_string(&path, filename);
    }

    return path;
}

// Creates an error message for exec report based on other_error variable or errno 
void make_error_message(char **error_mes, char *file_path, char *other_error){
    copy_string(error_mes, "- File: ");
    concat_string(error_mes, file_path);
    concat_string(error_mes, " - ");
    if (other_error != NULL){
        concat_string(error_mes, other_error);
    } else {
        concat_string(error_mes, strerror(errno));
    }
    concat_string(error_mes, "\n");
}

// Copies a file from source path to target path
int copy_file(char *src_path, char *trgt_path){
    int src_fd, trgt_fd;
    char *error_mes;

    // if there is no source path, make an error
    if ((src_fd = open(src_path, O_RDONLY)) == -1){
        make_error_message(&error_mes, src_path, NULL);
        enqueue(error_qid, error_mes, 1);
        free(error_mes);
        error_count++;
        return -1;
    }

    // if there is no target path, make an error
    if ((trgt_fd = open(trgt_path, O_WRONLY)) == -1){
        make_error_message(&error_mes, trgt_path, NULL);
        enqueue(error_qid, error_mes, 1);
        free(error_mes);
        error_count++;
        return -1;
    }

    // Copy file 4096 bytes at a time
    char buffer[4096];
    ssize_t bytes_read, bytes_written;
    while ((bytes_read = read(src_fd, buffer, sizeof(buffer))) > 0){
        bytes_written = write(trgt_fd, buffer, bytes_read);
        
        if (bytes_written != bytes_read){
            make_error_message(&error_mes, trgt_path, "Write error");
            enqueue(error_qid, error_mes, 1);
            free(error_mes);
            close(src_fd);
            close(trgt_fd);
            return -1;
        }
    }

    // File descriptors not requiered anymore
    close(src_fd);
    close(trgt_fd);

    // We couldn't read from source file, make an error
    if (bytes_read == -1){
        make_error_message(&error_mes, src_path, NULL);
        enqueue(error_qid, error_mes, 1);
        free(error_mes);
        error_count++;
        return -1;
    }
    
    // Change target file's permissions
    struct stat st;
    lstat(src_path, &st);
    if (chmod(trgt_path, st.st_mode) == -1){
        make_error_message(&error_mes, trgt_path, NULL);
        enqueue(error_qid, error_mes, 1);
        free(error_mes);
        error_count++;
        return -1;
    }

    return 0;
}

// Creates or modifies a file (ADDED and/or MODIFIED operation)
int add_or_modify_file(char *filename, bool create_file){
    char *src_path, *trgt_path;

    // Make coresponding file paths
    src_path = make_path(source_dir, filename);
    trgt_path = make_path(target_dir, filename);

    // If we are in ADDED operation, crreate the file
    if (create_file) creat(trgt_path, 0644);
    int cp_code = copy_file(src_path, trgt_path); // user copy_file to do the rest 

    free(src_path);
    free(trgt_path);
    return cp_code; // Return copy_file's return code (0: success, -1: failure)
}

// Deletes a file from target directory (DELETED operation)
int delete_file(char *filename){
    char *trgt_path, *error_mes;
    trgt_path = make_path(target_dir, filename); // make coresponding path

    // If file can't be deleted, make an error
    if (unlink(trgt_path) == -1){
        make_error_message(&error_mes, trgt_path, NULL);
        enqueue(error_qid, error_mes, 1);
        free(error_mes);
        error_count++;
        free(trgt_path);
        return -1;
    }

    free(trgt_path);
    return 0; // return code (0: success, -1: failure)
}

// Sync source and target directories (FULL operation)
int sync_directories(){
    char *error_mes;
    
    // Open target dir, and if you can't make an error
    DIR *trgt_ptr;
    if ((trgt_ptr = opendir(target_dir)) == NULL){
        make_error_message(&error_mes, target_dir, NULL);
        enqueue(error_qid, error_mes, 1);
        free(error_mes);
        error_count++;
        return -1;
    }

    // Delete everything from target directory
    char *fn;
    struct dirent *direntp;
    while((direntp = readdir(trgt_ptr)) != NULL){
        fn = direntp->d_name;
        if (strcmp(fn, ".") == 0 || strcmp(fn, "..") == 0){
            continue;
        }

        delete_file(fn);
    }
    closedir(trgt_ptr);

    // If you can't open source directory, make an error
    DIR *src_ptr;
    if ((src_ptr = opendir(source_dir)) == NULL){
        make_error_message(&error_mes, source_dir, NULL);
        enqueue(error_qid, error_mes, 1);
        free(error_mes);
        error_count++;
        return -1;
    }

    // Copy every file in the source to the target
    while((direntp = readdir(src_ptr)) != NULL){
        fn = direntp->d_name;
        if (strcmp(fn, ".") == 0 || strcmp(fn, "..") == 0){
            continue;
        }

        if (add_or_modify_file(fn, true) == -1){
            files_skipped++; // failed to copy
        } else {
            files_copied++; // copied successfully
        }
    }
    closedir(src_ptr);
}

// Generates exec report
char *make_exec_report(){
    // Status depends on the operation and the files copied
    char *status;
    if (error_count == 0){
        status = "SUCCESS";
    } else {
        if (strcmp(operation, "FULL") == 0 && files_copied > 0){
            status = "PARTIAL";
        } else {
            status = "ERROR";
        }
    } 

    // format exec report details
    char details[256], *report;
    if (strcmp(operation, "FULL") == 0){
        snprintf(details ,sizeof(details), "EXEC_REPORT_START\nSTATUS: %s\nDETAILS: %d files copied, %d skipped\n", status, files_copied, files_skipped);
    } else {
        snprintf(details ,sizeof(details), "EXEC_REPORT_START\nSTATUS: %s\nDETAILS: File: %s\n", status, filename);
    }
    
    copy_string(&report, details);

    // If there were errors, create an error section
    if (error_count > 0){
        concat_string(&report, "ERRORS:\n");
        char *error_mes;
        while ((error_mes = dequeue(error_qid)) != NULL){
            concat_string(&report, error_mes);
            free(error_mes);
        }
    }

    // End report
    concat_string(&report, "EXEC_REPORT_END\n");
    return report;
}

int main(int argc, char *argv[]){   
    if (argc != 5){
        exit(1);
    }
    
    // Initialize worker dependencies
    get_flag_values(argv);
    error_qid = init_queue(ERRORQ_KEY);

    // Do the right operation
    if (strcmp(operation, "FULL") == 0){
        sync_directories();
    } else if (strcmp(operation, "ADDED") == 0){
        if (add_or_modify_file(filename, true) == -1){
            files_skipped++;
        } else {
            files_copied++;
        }
    } else if (strcmp(operation, "MODIFIED") == 0){
        if (add_or_modify_file(filename, false) == -1){
            files_skipped++;
        } else {
            files_copied++;
        }
    } else if (strcmp(operation, "DELETED") == 0){
        if (delete_file(filename) == -1){
            files_skipped++;
        } else {
            files_copied++;
        }
    } else {
        free_memory();
        exit(1);
    }

    // generate report
    char *report = make_exec_report();
    
    // print reports
    printf("%s", report);

    // Cleanup
    free(report);
    free_memory();
    exit(0);
}