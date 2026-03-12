#ifndef QUEUE_H
#define QUEUE_H

// This queue "abstraction" is a copy
// from the coresponding lecture's slides

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <errno.h>

#define ERRORQ_KEY (key_t) 108 // For errors in worker.c
#define WORKERQ_KEY (key_t) 109 // For enqueued workers in fss_manager.c

#define QPERM 0666 // I didn't know what permissions to give so I gave 0666
#define MAXOBN 511
#define MAXPRIOR 10

// Message Queue entry
typedef struct q_entry{
    long mtype;
    char mtext[MAXOBN + 1];
} Q_Entry;

// Message Queue functions
int init_queue(int q_key);
int enqueue(int qid, char *message, int priority);
char *dequeue(int qid);
void empty_out_queue(int qid);

#endif