#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

// buffer size
#define BUFFER_SIZE 3

// number of items produced
#define TOTAL_ITEMS 20

// keys
#define BUFFER_KEY 100
#define ADD_KEY 200
#define REM_KEY 300
#define MUTEX_KEY 400
#define FULL_KEY 500
#define EMPTY_KEY 600
#define PROCESS_COUNT_KEY 700

// struct to count running processes
struct processCount
{
	int producerCount;
	int consumerCount;
};

// arg for semctl system calls
union Semun
{
    int val;               //value for SETVAL
    struct semid_ds *buf;  // buffer for IPC_STAT & IPC_SET
    ushort *array;         // array for GETALL & SETALL
    struct seminfo *__buf; // buffer for IPC_INFO
    void *__pad;
};

void down(int sem)
{
    struct sembuf p_op;

    p_op.sem_num = 0;
    p_op.sem_op = -1;
    p_op.sem_flg = !IPC_NOWAIT;

    if (semop(sem, &p_op, 1) == -1)
    {
        perror("Error in down()");
        exit(-1);
    }
}

void up(int sem)
{
    struct sembuf v_op;

    v_op.sem_num = 0;
    v_op.sem_op = 1;
    v_op.sem_flg = !IPC_NOWAIT;

    if (semop(sem, &v_op, 1) == -1)
    {
        perror("Error in up()");
        exit(-1);
    }
}

