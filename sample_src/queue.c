#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "queue.h"
#include "utils.h"

// Initialize message queue
int init_queue(int q_key){
    int queue_id;

    if ((queue_id = msgget(q_key ,IPC_CREAT | QPERM)) == -1){
        perror("msgget failed");
    }
    return queue_id;
}

// Enqueue a message in the message queue
int enqueue(int qid, char *message, int priority){
    int len;
    Q_Entry s_entry;

    // If message/priority is invalid return
    if ((len = strlen(message)) > MAXOBN){
        return -1;
    }
    if (priority > MAXPRIOR || priority < 0){
        return -1;
    }

    // Enqueue message
    s_entry.mtype = (long)priority;
    strncpy(s_entry.mtext, message, MAXOBN);

    if (msgsnd(qid, &s_entry, len, 0) == -1){
        return -1;
    } else {
        return 0;
    }
}

// Returns the first message in the queue 
char *dequeue(int qid){
    int mlen;
    Q_Entry r_entry;

    if ((mlen = msgrcv(qid, &r_entry, sizeof(r_entry.mtext), 0, IPC_NOWAIT | MSG_NOERROR)) == -1){
        return NULL;
    } else {
        r_entry.mtext[mlen] = '\0';

        char *message = malloc(sizeof(char) * (mlen + 1));
        malloc_check(message);
        strcpy(message, r_entry.mtext);

        return message;
    }
}

// Clean up queue
void empty_out_queue(int qid){
    msgctl(qid, IPC_RMID, NULL);
}