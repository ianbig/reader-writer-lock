#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <dirent.h>
#define gettid() syscall(SYS_gettid)

#include "rwlock.h"

/* the number of times the read loop */
// #define LOOPS 10
#define r_num 3
#define w_num 9

typedef enum{true, false} bool;


// status rstatus[r_num];
// status wstatus[w_num];

/* declare a read/write lock */
rwl * rwlock;

/* a linked list node */
typedef struct node_t {
    int value;
    struct node_t* next;
}node;

typedef struct rarg_t{
    int id;
    int tid;
} rargs;

/* writer arguments */
typedef struct warg_t{
    int id;
    int tid;
    // int seq;
    int priority;
} wargs;


int r_id[r_num];
int w_id[w_num];
pthread_mutex_t rlock[r_num];
pthread_mutex_t wlock[w_num];
pthread_cond_t  rcond[r_num];
pthread_cond_t  wcond[w_num];
rargs rinput[r_num];
wargs winput[w_num];
pthread_t r_th[r_num];
pthread_t w_th[w_num];
FILE * wfp[w_num];
FILE * rfp[r_num];
char wthread_name[w_num][100];
char rthread_name[r_num][100];
int w_state[w_num];
int r_state[r_num];

int w_end[w_num];
int r_end[w_num];

int w_prior[] = {0,0,0,1,1,1,2,2,2};
/* our list is global */

char * sstate = "State:	S (sleeping)\n";
char * rstate = "State:	R (running)\n";
char * status;
size_t len = 100;
/* writer function which inserts elements */


/*
priorityrw tests seq:
Writer 1 arrives
Writer 1 acquires the lock
Writer 0 arrives
Writer 3 arrives
Writer 7 arrives
Reader 0 arrives
Writer 1 releases the lock
Writer 0 acquires the lock
Writer 0 releases the lock
Writer 3 acquires the lock
Reader 1 arrives
Writer 3 releases the lock
Writer 7 acquires the lock
Writer 8 arrives
Writer 7 releases the lock
Writer 8 acquires the lock
Writer 8 releases the lock
Reader 0 & 1 acquires the lock
Writer 6 arrives
Reader 2 arrives
Writer 4 arrives
Writer 5 arrives
Reader 1 releases the lock
Reader 0 releases the lock
Writer 4/5 acquires the lock
Writer 2 arrives
Reader 1 arrives 
Writer 4/5 releases the lock
Writer 2 acquires the lock
Writer 2 releases the lock
Writer 5/4 acquires the lock
Writer 5/4 releases the lock
Writer 6 acquires the lock
Writer 6 releases the lock
Reader 1 & 2 acquires the lock
Reader 2 releases the lock
Reader 1 releases the lock
*/

void read_wstatus(int id){
    do{
        wfp[id] = fopen(wthread_name[id], "r");
        for(int i = 0; i < 3; i++){
            getline(&status, &len, wfp[id]);
        }    
    }while(strcmp(status, sstate));
    fclose(wfp[id]);
}

void read_rstatus(int id){
    do{
        rfp[id] = fopen(rthread_name[id], "r");
        for(int i = 0; i < 3; i++){
            getline(&status, &len, rfp[id]);
        }    
    }while(strcmp(status, sstate));
    fclose(rfp[id]);
}

bool run_tests(){
    pid_t pid = getpid();
    // printf("%d\n", pid);
    DIR * dir = opendir("/proc/");
    if(dir){
        closedir(dir);
    }
    else{
        printf("You are not in Linux system, please run tests in docker.\n");
        return false;
    }
    for(int i = 0 ; i < w_num; i++){
        while(wfp[i] == NULL){
        sprintf(wthread_name[i], "/proc/%d/task/%d/status", pid, winput[i].tid);
            wfp[i] = fopen(wthread_name[i], "r");
        }
    }
    for(int i = 0 ; i < r_num; i++){
        while(rfp[i] == NULL){
            sprintf(rthread_name[i], "/proc/%d/task/%d/status", pid, rinput[i].tid);
            rfp[i] = fopen(rthread_name[i], "r");
        }
    }
    
    read_wstatus(1);
    // Writer 1 arrives
    pthread_cond_signal(&wcond[1]);
    read_wstatus(1);
    if(w_state[1] != 1){
        printf("writer 1 fails to acquire the lock!\n");
        return false;
    }
    // Writer 1 acquires the lock
    read_wstatus(0);
    // Writer 0 arrives
    pthread_cond_signal(&wcond[0]);
    read_wstatus(3);
    // Writer 3 arrives
    pthread_cond_signal(&wcond[3]);
    read_rstatus(1);
    // Reader 1 arrives
    pthread_cond_signal(&rcond[1]);
    read_wstatus(7);
    // Writer 7 arrives
    pthread_cond_signal(&wcond[7]);
    read_wstatus(0);
    if(w_state[0] == 1){
        printf("writer 0 wrongly acquires the lock!\n");
        return false;
    }
    read_wstatus(3);
    if(w_state[3] == 1){
        printf("writer 3 wrongly acquires the lock!\n");
        return false;
    }
    read_rstatus(1);
    if(r_state[1] == 1){
        printf("reader 1 wrongly acquires the lock!\n");
        return false;
    }
    read_wstatus(7);
    if(w_state[7] == 1){
        printf("writer 7 wrongly acquires the lock!\n");
        return false;
    }
    pthread_cond_signal(&wcond[1]);
    read_wstatus(1);
    if(w_state[1] != 0){
        printf("writer 1 fails to release the lock!\n");
        return false;
    }
    // Writer 1 releases the lock
    read_wstatus(0);
    if(w_state[0] != 1){
        printf("writer 0 fails to acquire the lock!\n");
        return false;
    }
    read_wstatus(3);
    if(w_state[3] == 1){
        printf("writer 3 wrongly acquires the lock!\n");
        return false;
    }
    read_rstatus(1);
    if(r_state[1] == 1){
        printf("reader 1 wrongly acquires the lock!\n");
        return false;
    }
    read_wstatus(7);
    if(w_state[7] == 1){
        printf("writer 7 wrongly acquires the lock!\n");
        return false;
    }
    // Writer 0 acquires the lock
    pthread_cond_signal(&wcond[0]);
    read_wstatus(0);
    if(w_state[0] != 0){
        printf("writer 0 fails to release the lock!\n");
        return false;
    }
    // Writer 0 releases the lock
    read_wstatus(3);
    if(w_state[3] != 1){
        printf("writer 3 fails to acquire the lock!\n");
        return false;
    }
    read_rstatus(1);
    if(r_state[1] == 1){
        printf("reader 1 wrongly acquires the lock!\n");
        return false;
    }
    read_wstatus(7);
    if(w_state[7] == 1){
        printf("writer 7 wrongly acquires the lock!\n");
        return false;
    }
    // Writer 3 acquires the lock
    read_rstatus(0);
    pthread_cond_signal(&rcond[0]);
    read_rstatus(0);
    if(r_state[0] == 1){
        printf("reader 0 wrongly acquires the lock!\n");
        return false;
    }
    // Reader 1 arrives
    pthread_cond_signal(&wcond[3]);
    read_wstatus(3);
    if(w_state[3] != 0){
        printf("writer 3 fails to release the lock!\n");
        return false;
    }
    // Writer 3 releases the lock
    read_wstatus(7);
    if(w_state[7] != 1){
        printf("writer 7 fails to acquire the lock!\n");
        return false;
    }
    read_rstatus(1);
    if(r_state[1] == 1){
        printf("reader 1 wrongly acquires the lock!\n");
        return false;
    }
    read_rstatus(0);
    if(r_state[0] == 1){
        printf("reader 0 wrongly acquires the lock!\n");
        return false;
    }
    // Writer 7 acquires the lock
    read_wstatus(8);
    pthread_cond_signal(&wcond[8]);
    read_wstatus(8);
    if(w_state[8] == 1){
        printf("writer 8 wrongly acquires the lock!\n");
        return false;
    }
    // Writer 8 arrives
    pthread_cond_signal(&wcond[7]);
    read_wstatus(7);
    if(w_state[7] != 0){
        printf("writer 7 fails to release the lock!\n");
        return false;
    }
    // Writer 7 releases the lock
    read_wstatus(8);
    if(w_state[8] != 1){
        printf("writer 8 fails to acquire the lock!\n");
        return false;
    }
    read_rstatus(1);
    if(r_state[1] == 1){
        printf("reader 1 wrongly acquires the lock!\n");
        return false;
    }
    read_rstatus(0);
    if(r_state[0] == 1){
        printf("reader 0 wrongly acquires the lock!\n");
        return false;
    }
    // Writer 8 acquires the lock
    pthread_cond_signal(&wcond[8]);
    read_wstatus(8);
    if(w_state[8] != 0){
        printf("writer 8 fails to release the lock!\n");
        return false;
    }
    // Writer 8 releases the lock
    read_rstatus(0);
    if(r_state[0] != 1){
        printf("reader 0 fails to acquire the lock!\n");
        return false;
    }
    read_rstatus(1);
    if(r_state[1] != 1){
        printf("reader 1 fails to acquire the lock!\n");
        return false;
    }
    // Reader 0 & 1 acquires the lock
    read_wstatus(6);
    pthread_cond_signal(&wcond[6]);
    read_wstatus(6);
    if(w_state[6] == 1){
        printf("writer 6 wrongly acquires the lock!\n");
        return false;
    }
    // Writer 6 arrives
    read_rstatus(2);
    pthread_cond_signal(&rcond[2]);
    read_rstatus(2);
    if(r_state[2] == 1){
        printf("reader 2 wrongly acquires the lock!\n");
        return false;
    }
    // Reader 2 arrives
    read_wstatus(4);
    pthread_cond_signal(&wcond[4]);
    read_wstatus(4);
    if(w_state[4] == 1){
        printf("writer 4 wrongly acquires the lock!\n");
        return false;
    }
    // Writer 4 arrives
    read_wstatus(5);
    pthread_cond_signal(&wcond[5]);
    read_wstatus(5);
    if(w_state[5] == 1){
        printf("writer 5 wrongly acquires the lock!\n");
        return false;
    }
    // Writer 5 arrives
    pthread_cond_signal(&rcond[1]);
    read_rstatus(1);
    if(r_state[1] != 0){
        printf("reader 1 fails to release the lock!\n");
        return false;
    }
    read_wstatus(4);
    if(w_state[4] == 1){
        printf("writer 4 wrongly acquires the lock!\n");
        return false;
    }
    read_wstatus(5);
    if(w_state[5] == 1){
        printf("writer 5 wrongly acquires the lock!\n");
        return false;
    }
    read_wstatus(6);
    if(w_state[6] == 1){
        printf("writer 6 wrongly acquires the lock!\n");
        return false;
    }
    read_rstatus(2);
    if(r_state[2] == 1){
        printf("reader 2 wrongly acquires the lock!\n");
        return false;
    }
    // Reader 1 releases the lock
    pthread_cond_signal(&rcond[0]);
    read_rstatus(0);
    if(r_state[0] != 0){
        printf("reader 0 fails to release the lock!\n");
        return false;
    }
    // Reader 0 releases the lock
    int curr;
    read_wstatus(4);
    read_wstatus(5);
    if(w_state[4] == 1){
        if(w_state[5] == 1){
            printf("writer 4 and 5 wrongly acquires the lock at the same time!\n");
            return false;
        }
        curr = 4;
    }
    else if(w_state[5] == 1){
        curr = 5;
    }
    else{
        printf("writer 4 or 5 fails to acquire the lock!\n");
        return false;
    }
    read_wstatus(6);
    if(w_state[6] == 1){
        printf("writer 6 wrongly acquires the lock!\n");
        return false;
    }
    read_rstatus(2);
    if(r_state[2] == 1){
        printf("reader 2 wrongly acquires the lock!\n");
        return false;
    }
    // Writer 4/5 acquires the lock
    pthread_cond_signal(&wcond[2]);
    read_wstatus(2);
    if(w_state[2] == 1){
        printf("writer 2 wrongly acquires the lock!\n");
        return false;
    }
    // Writer 2 arrives
    pthread_cond_signal(&rcond[1]);
    read_rstatus(1);
    if(r_state[1] == 1){
        printf("reader 1 wrongly acquires the lock!\n");
        return false;
    }
    // Reader 1 arrives 
    pthread_cond_signal(&wcond[curr]);
    read_wstatus(curr);
    if(w_state[curr] != 0){
        printf("writer %d fails to release the lock!\n", curr);
        return false;
    }
    if(curr == 4){
        curr = 5;
    }
    else curr = 4;
    
    // Writer 4/5 releases the lock
    read_wstatus(curr);
    if(w_state[curr] == 1){
        printf("writer %d wrongly acquires the lock!\n", curr);
        return false;
    }
    read_wstatus(6);
    if(w_state[6] == 1){
        printf("writer 6 wrongly acquires the lock!\n");
        return false;
    }
    read_rstatus(2);
    if(r_state[2] == 1){
        printf("reader 2 wrongly acquires the lock!\n");
        return false;
    }
    read_rstatus(1);
    if(r_state[1] == 1){
        printf("reader 1 wrongly acquires the lock!\n");
        return false;
    }
    read_wstatus(2);
    if(w_state[2] != 1){
        printf("writer 2 fails to acquire the lock!\n");
        return false;
    }
    // Writer 2 acquires the lock
    pthread_cond_signal(&wcond[2]);
    read_wstatus(2);
    if(w_state[2] != 0){
        printf("writer 2 fails to release the lock!\n");
        return false;
    }
    // Writer 2 releases the lock
    read_wstatus(6);
    if(w_state[6] == 1){
        printf("writer 6 wrongly acquires the lock!\n");
        return false;
    }
    read_rstatus(2);
    if(r_state[2] == 1){
        printf("reader 2 wrongly acquires the lock!\n");
        return false;
    }
    read_rstatus(1);
    if(r_state[1] == 1){
        printf("reader 1 wrongly acquires the lock!\n");
        return false;
    }
    read_wstatus(curr);
    if(w_state[curr] != 1){
        printf("writer %d fails to acquire the lock!\n", curr);
        return false;
    }
    // Writer 5/4 acquires the lock
    pthread_cond_signal(&wcond[curr]);
    read_wstatus(curr);
    if(w_state[curr] != 0){
        printf("writer %d fails to release the lock!\n", curr);
        return false;
    }
    // Writer 5/4 releases the lock
    read_rstatus(2);
    if(r_state[2] == 1){
        printf("reader 2 wrongly acquires the lock!\n");
        return false;
    }
    read_rstatus(1);
    if(r_state[1] == 1){
        printf("reader 1 wrongly acquires the lock!\n");
        return false;
    }
    read_wstatus(6);
    if(w_state[6] != 1){
        printf("writer 6 fails to acquire the lock!\n");
        return false;
    }
    // Writer 6 acquires the lock
    pthread_cond_signal(&wcond[6]);
    read_wstatus(6);
    if(w_state[6] != 0){
        printf("writer 6 fails to release the lock!\n");
        return false;
    }
    // Writer 6 releases the lock
    read_rstatus(2);
    if(r_state[2] != 1){
        printf("reader 2 fails to acquire the lock!\n");
        return false;
    }
    read_rstatus(1);
    if(r_state[1] != 1){
        printf("reader 1 fails to acquire the lock!\n");
        return false;
    }
    // Reader 1 & 2 acquires the lock
    pthread_cond_signal(&rcond[2]);
    sched_yield();
    read_rstatus(2);
    if(r_state[2] != 0){
        printf("reader 2 fails to release the lock!\n");
        return false;
    }
    // Reader 2 releases the lock
    pthread_cond_signal(&rcond[1]);
    read_rstatus(1);
    if(r_state[1] != 0){
        printf("reader 1 fails to release the lock!\n");
        return false;
    }
    // Reader 1 releases the lock
    for(int i = 0 ; i < w_num; i++){
        w_end[i] = 1;
        pthread_cond_signal(&wcond[i]);
    }
    for(int i = 0 ; i < r_num; i++){
        r_end[i] = 1;
        pthread_cond_signal(&rcond[i]);
    }
    return true;
}



void * writer(void* args) {
    
    
    wargs* input = (wargs *) args;
    int id = input->id;
    // int seq = input->seq;
    input->tid = gettid();
    int priority = input->priority;
    pthread_mutex_lock(&wlock[id]);
    while(1){
        pthread_cond_wait(&wcond[id],&wlock[id]);
        if(w_end[id] == 1){
            break;
        }
        printf("Writer %d tries to acquire the lock \n", id);
        rwl_wlock(rwlock, input->priority);
        w_state[id] = 1;
        printf("Writer %d acquires the lock \n", id);
        pthread_cond_wait(&wcond[id],&wlock[id]);
        rwl_wunlock(rwlock, priority);
        w_state[id] = 0;
        printf("Writer %d releases the lock \n", id);
    }
    pthread_mutex_unlock(&wlock[id]);
    pthread_exit(NULL);
}

/* reader function which gets the latest value in the list */
void * reader(void* args) {
    /* continually print the list */
    rargs* input = (rargs *) args;
    input->tid = gettid();
    int id = input->id;
    pthread_mutex_lock(&rlock[id]);
    while(1){
        pthread_cond_wait(&rcond[id],&rlock[id]);
        if(r_end[id] == 1){
            break;
        }
        printf("Reader %d tries to acquire the lock \n", id);
        rwl_rlock(rwlock);
        r_state[id] = 1;
        printf("Reader %d acquires the lock \n", id);
        pthread_cond_wait(&rcond[id],&rlock[id]);
        rwl_runlock(rwlock);
        r_state[id] = 0;
        printf("Reader %d releases the lock \n", id);
    }
    pthread_mutex_unlock(&rlock[id]);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {

    printf("priority read/write test:\n");
    status = (char *) malloc(100);
    rwlock = (rwl *)malloc(sizeof(rwl));
    /* initialize the lock */
    rwl_init(rwlock);
    /* spawn all threads */
    for (int i = 0; i < w_num; i++) {
        // wargs * input = (wargs *) malloc(sizeof(wargs));  
        pthread_mutex_init(&wlock[i],NULL);
        pthread_cond_init(&wcond[i], NULL);
        w_state[i] = 0;
        w_end[i] = 0;
        winput[i].priority = w_prior[i];
        winput[i].id = i;
        winput[i].tid = 0;
        int r = pthread_create(&w_th[i], NULL, &writer, (void *) &winput[i]);
        if(r != 0){
            printf("Failed to create threads!\n");
            return 0;
        }
        
    }
    for (int i = 0; i < r_num; i++) {
        // rargs * input = (rargs *) malloc(sizeof(rargs));
        pthread_mutex_init(&rlock[i],NULL);
        pthread_cond_init(&rcond[i], NULL);
        r_state[i] = 0;
        r_end[i] = 0;
        rinput[i].id = i;
        rinput[i].tid = 0;
        int r = pthread_create(&r_th[i], NULL, &reader, (void *) &rinput[i]);
        if(r != 0){
            printf("Failed to create threads!\n");
            return 0;
        }
    }
    
    bool result = run_tests();
    // for (int i = 0; i < w_num; i++) {
    //     pthread_join(w_th[i], NULL);
    // }

    // for (int i = 0; i < r_num; i++) {
    //     pthread_join(r_th[i], NULL);
    // }
    if(result == true){
        printf("Test Passed!\n");
    }else{
        printf("Test Failed!\n");
    }
    


    return 0;
}