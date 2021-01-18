#include "headers.h"

// global variables
// IDs
int processCountID;
int bufferID;
int remID;
int mutex;
int full;
int empty;
// addresses
struct processCount *p_processCount;
int *p_buffer;
int *p_rem;

// forward declarations
void consumeItem(int *p_buffer, int *p_add);
void cleanup(int signum);

int main()
{
	// bind handler
	signal(SIGINT, cleanup);
	
	// attach to process count struct	
	processCountID = shmget(PROCESS_COUNT_KEY, sizeof(struct processCount), IPC_CREAT | IPC_EXCL | 0644);
	if (processCountID == -1)
	{
		processCountID = shmget(PROCESS_COUNT_KEY, sizeof(struct processCount), IPC_CREAT | 0644);
		p_processCount = shmat(processCountID, (void *) 0, 0);
		p_processCount->consumerCount++;		
	}
	else
	{
		p_processCount = shmat(processCountID, (void *) 0, 0);
		p_processCount->producerCount = 0;
		p_processCount->consumerCount = 1;
	}

	// attach to buffer shared memory
	bufferID = shmget(BUFFER_KEY, (BUFFER_SIZE * sizeof(int)), IPC_CREAT | 0644);
	p_buffer = shmat(bufferID, (void *) 0, 0);
	
	// attch to add shared memory
	remID = shmget(REM_KEY, sizeof(int), IPC_CREAT | IPC_EXCL | 0644);
	if (remID == -1)
	{
		remID = shmget(REM_KEY, sizeof(int), IPC_CREAT | 0644);
		p_rem = shmat(remID, (void *) 0, 0);
	}
	else
	{
		// initialize add
		p_rem = shmat(remID, (void *) 0, 0);
		*p_rem = 0;
	}
	
	// attach to semaphores (mutex, full, empy)
	mutex = semget(MUTEX_KEY, 1, IPC_CREAT | IPC_EXCL | 0644);
	full = semget(FULL_KEY, 1, IPC_CREAT | IPC_EXCL | 0644);
	empty = semget(EMPTY_KEY, 1, IPC_CREAT | IPC_EXCL | 0644);
	if ((mutex == -1) || (full == -1) || (empty == -1))
	{
		mutex = semget(MUTEX_KEY, 1, IPC_CREAT | 0644);
		full = semget(FULL_KEY, 1, IPC_CREAT | 0644);
		empty = semget(EMPTY_KEY, 1, IPC_CREAT | 0644);
	}
	else
	{
		union Semun semun;
		// initialize mutex to 1
		semun.val = 1;
		semctl(mutex, 0, SETVAL, semun);
		// initialize full to 0
		semun.val = 0;
		semctl(full, 0, SETVAL, semun);
		// initialize empty to BUFFER_SIZE
		semun.val = BUFFER_SIZE;
		semctl(empty, 0, SETVAL, semun);
	}
	
	// consumer main loop
	while (1)
	{
		down(full);
		down(mutex);
		consumeItem(p_buffer, p_rem);
		up(mutex);
		up(empty);	
	}		

	return 0;
}

void consumeItem(int *p_buffer, int *p_rem)
{
	printf("Consumer: removed item %d from location %d\n", p_buffer[*p_rem], *p_rem);
	*p_rem = (*p_rem + 1) % BUFFER_SIZE;
}

void cleanup(int signum)
{
	printf("Consumer quitting\n");

	p_processCount->consumerCount--;
	if ((p_processCount->producerCount == 0) && (p_processCount->consumerCount == 0))
	{
		int addID = shmget(ADD_KEY, sizeof(int), IPC_CREAT | 0644);
		shmctl(processCountID, IPC_RMID, (struct shmid_ds *) 0);
		shmctl(bufferID, IPC_RMID, (struct shmid_ds *) 0);
		shmctl(addID, IPC_RMID, (struct shmid_ds *) 0);
		shmctl(remID, IPC_RMID, (struct shmid_ds *) 0);
		semctl(mutex, IPC_RMID, 0, (struct semid_ds *) 0);
		semctl(empty, IPC_RMID, 0, (struct semid_ds *) 0);
		semctl(full, IPC_RMID, 0, (struct semid_ds *) 0);
	}
	else
	{
		shmdt(p_processCount);
		shmdt(p_buffer);
		shmdt(p_rem);
	}
	
	exit(0);
}

