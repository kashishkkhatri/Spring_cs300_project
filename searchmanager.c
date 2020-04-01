#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <semaphore.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>
#include "longest_word_search.h"
#include "queue_ids.h"

//Global Variables
char **ArrayPrefix; // Stores all prefixes
sem_t *completedPassages; //Semaphore Array to Track Progress
int numPrefix; 
int numPassages;



size_t strlcpy(char *dst, const char *src, size_t size){
    size_t srclen;
    size--;
    srclen = strlen(src);
    if(srclen > size)
        srclen = size;

    memcpy(dst, src, srclen);
    dst[srclen] = '\0';

    return (srclen);
}

void sig_handler(int signo)
{
  signal(SIGINT, sig_handler); 
  if (signo == SIGINT){
      int *check = (int*) malloc(sizeof(int));
      for(int i = 0; i < numPrefix; i++){
            printf(" %s - ", ArrayPrefix[i]);
            sem_getvalue(&completedPassages[i], check);
            
            if(*check == 0){
                printf("pending\n");
            } else if(*check == numPassages){
                printf("done\n");
            } else {
                printf("%d of %d\n",*check,numPassages);
            }
        }
    }
}

response_buf recieveLastMessage(){

    int msqid;
    int msgflg = IPC_CREAT | 0666;
    key_t key;
    prefix_buf sbuf;
    response_buf rbuf;
    size_t buf_length;
    
    key = ftok(CRIMSON_ID,QUEUE_NUMBER);
        if ((msqid = msgget(key, msgflg)) < 0) {
            int errnum = errno;
            fprintf(stderr, "Value of errno: %d\n", errno);
            perror("(msgget)");
            fprintf(stderr, "Error msgget: %s\n", strerror( errnum ));
        }

        // msgrcv to receive message
        int ret;
        do {
            ret = msgrcv(msqid, &rbuf, sizeof(response_buf), 2, 0);//receive type 2 message
            int errnum = errno;
            if (ret < 0 && errno !=EINTR){
                fprintf(stderr, "Value of errno: %d\n", errno);
                perror("Error printed by perror");
                fprintf(stderr, "Error receiving msg: %s\n", strerror( errnum ));
            }
        } while ((ret < 0 ) && (errno == 4));

        if(rbuf.index == 0){
            printf("Exiting ...\n");
        }
}
void allocateSpace(){
    ArrayPrefix = (char **)malloc(numPrefix * sizeof(char *));
    for(int i = 0; i < numPrefix; i++){
        ArrayPrefix[i] = (char *)malloc(20 * sizeof(char));
    }
    completedPassages = (sem_t*)malloc(numPrefix *  sizeof(sem_t)); 
}

int main(int argc, char**argv){

    if (signal(SIGINT, sig_handler) == SIG_ERR)
        printf("\ncan't catch SIGINT\n");

    int msqid;
    int msgflg = IPC_CREAT | 0666;
    key_t key;
    prefix_buf sbuf;
    response_buf rbuf;
    size_t buf_length;

    if(argc <= 1){
        printf("Error: please provide prefix of at least two characters for search\n");
        printf("Usage: %s <prefix>\n", argv[0]);
        exit(-1);
    }

    numPrefix = 0;
    for(int  i = 2; i < argc; i++){
        if(strlen(argv[i]) < 3 || strlen(argv[i]) > 20){
            printf("Error: please provide prefix of at least three characters for search\n");
            ("Usage: %s <prefix>\n", argv[0]);
            argv[i] = "";
            numPrefix --;
        } 
        numPrefix++;
    }


    allocateSpace();
    for(int i = 2; i < argc; i++){
        sem_init(&completedPassages[i-2], 0, 0);
        if(argv[i] == ""){
            ArrayPrefix[i - 2] = argv[i+1];
            i++;
        } else {
            ArrayPrefix[i - 2] = argv[i];
        }
    }
    
    key = ftok(CRIMSON_ID, QUEUE_NUMBER);
    if ((msqid = msgget(key, msgflg)) < 0) {
        int errnum = errno;
        fprintf(stderr, "Value of errno: %d\n", errno);
        perror("(msgget)");
        fprintf(stderr, "Error msgget: %s\n", strerror( errnum ));
    }
    else {
        for(int i = 0; i < numPrefix; i++){     
            sbuf.mtype = 1;
            strlcpy(sbuf.prefix, ArrayPrefix[i], WORD_LENGTH);
            sbuf.id = i+1;

            buf_length = strlen(sbuf.prefix) + sizeof(int) + 1;

            if((msgsnd(msqid, &sbuf, buf_length, IPC_NOWAIT)) < 0) {
                int errnum = errno;
                fprintf(stderr,"%d, %ld, %s, %d\n", msqid, sbuf.mtype, sbuf.prefix, (int)buf_length);
                perror("(msgsnd)");
                fprintf(stderr, "Error sending msg: %s\n", strerror( errnum ));
                exit(1);
            }
            else
                fprintf(stderr,"Message(%d): \"%s\" Sent (%d bytes)\n", sbuf.id, sbuf.prefix,(int)buf_length);
            sleep(atoi(argv[1]));             
            int numResponses = 0;
            key = ftok(CRIMSON_ID,QUEUE_NUMBER);
            if ((msqid = msgget(key, msgflg)) < 0) {
                int errnum = errno;
                fprintf(stderr, "Value of errno: %d\n", errno);
                perror("(msgget)");
                fprintf(stderr, "Error msgget: %s\n", strerror( errnum ));
            }

            int ret;
            do {
                ret = msgrcv(msqid, &rbuf, sizeof(response_buf), 2, 0);//receive type 2 message
                int errnum = errno;
                if (ret < 0 && errno !=EINTR){
                    fprintf(stderr, "Value of errno: %d\n", errno);
                    perror("Error printed by perror");
                    fprintf(stderr, "Error receiving msg: %s\n", strerror( errnum ));
                }
            } while ((ret < 0 ) && (errno == 4));
            
            numPassages = rbuf.count;
            response_buf arrayResults[numPassages] ;
            sem_post(&completedPassages[i]);
            arrayResults[rbuf.index] = rbuf;
            numResponses++;    
            
            while(numResponses != numPassages){
                key = ftok(CRIMSON_ID,QUEUE_NUMBER);
                if ((msqid = msgget(key, msgflg)) < 0) {
                    int errnum = errno;
                    fprintf(stderr, "Value of errno: %d\n", errno);
                    perror("(msgget)");
                    fprintf(stderr, "Error msgget: %s\n", strerror( errnum ));
                }

                int ret;
                do {
                    ret = msgrcv(msqid, &rbuf, sizeof(response_buf), 2, 0);//receive type 2 message
                    int errnum = errno;
                    if (ret < 0 && errno !=EINTR){
                        fprintf(stderr, "Value of errno: %d\n", errno);
                        perror("Error printed by perror");
                        fprintf(stderr, "Error receiving msg: %s\n", strerror( errnum ));
                    }
                } while ((ret < 0 ) && (errno == 4));
                    
                sem_post(&completedPassages[i]);
                arrayResults[rbuf.index] = rbuf;     
                numResponses++;
            }

            for(int i = 0; i < numPassages; i++){
                if(arrayResults[i].present == 1){
                    printf("Passage %d - %s - %s\n", i, arrayResults[i].location_description, arrayResults[i].longest_word);
                }
                else
                    printf("Passage %d - %s - no word found\n",i, arrayResults[i].location_description);      
            }
            
        }

        sbuf.mtype = 1;
        strlcpy(sbuf.prefix, "   ", WORD_LENGTH);
        sbuf.id = 0;
        buf_length = strlen(sbuf.prefix) + sizeof(int) + 1;
        if((msgsnd(msqid, &sbuf, buf_length, IPC_NOWAIT)) < 0) {
            int errnum = errno;
            fprintf(stderr,"%d, %ld, %s, %d\n", msqid, sbuf.mtype, sbuf.prefix, (int)buf_length);
            perror("(msgsnd)");
            fprintf(stderr, "Error sending msg: %s\n", strerror( errnum ));
            exit(1);
        }
        else
            fprintf(stderr,"Message(%d): \"%s\" Sent (%d bytes)\n", sbuf.id, sbuf.prefix,(int)buf_length);

        response_buf response; 
        response = recieveLastMessage();
    }
}