#include "headers.h"

// global variables
// IDs
int processCountID;
int bufferID;
int addID;
int mutex;
int full;
int empty;
// addresses
struct processCount *p_processCount;
int *p_buffer;
int *p_add;

// forward declarations
void produceItem(int item, int *p_buffer, int *p_add);
void cleanup(int signum);

int main()
{
	// bind handler
	signal(SIGINT, cleanup);
	
	// attach to process count struct
	int producerNumber;	
	processCountID = shmget(PROCESS_COUNT_KEY, sizeof(struct processCount), IPC_CREAT | IPC_EXCL | 0644);
	if (processCountID == -1)
	{
		processCountID = shmget(PROCESS_COUNT_KEY, sizeof(struct processCount), IPC_CREAT | 0644);
		p_processCount = shmat(processCountID, (void *) 0, 0);
		producerNumber = p_processCount->producerCount;
		p_processCount->producerCount++;		
	}
	else
	{
		p_processCount = shmat(processCountID, (void *) 0, 0);
		p_processCount->producerCount = 1;
		p_processCount->consumerCount = 0;
		producerNumber = 0;
	}

	// attach to buffer shared memory
	bufferID = shmget(BUFFER_KEY, (BUFFER_SIZE * sizeof(int)), IPC_CREAT | 0644);
	p_buffer = shmat(bufferID, (void *) 0, 0);
	
	// attch to add shared memory
	int addID = shmget(ADD_KEY, sizeof(int), IPC_CREAT | IPC_EXCL | 0644);
	if (addID == -1)
	{
		addID = shmget(ADD_KEY, sizeof(int), IPC_CREAT | 0644);
		p_add = shmat(addID, (void *) 0, 0);
	}
	else
	{
		// initialize add
		p_add = shmat(addID, (void *) 0, 0);
		*p_add = 0;
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
	
	// producer main loop
	int item;
	for (int i = 0; i < TOTAL_ITEMS; i++)
	{
		sleep(1);
		down(empty);
		down(mutex);
		item = i + (producerNumber * TOTAL_ITEMS);
		produceItem(item, p_buffer, p_add);
		up(mutex);
		up(full);	
	}
	
	printf("Producer quitting\n");
	
	raise(SIGINT);		

	return 0;
}

void produceItem(int item, int *p_buffer, int *p_add)
{
	int location = *p_add;
	p_buffer[*p_add] = item;
	*p_add = (*p_add + 1) % BUFFER_SIZE;
	printf("Producer: inserted item %d in location %d\n", item, location);
}

void cleanup(int signum)
{
	p_processCount->producerCount--;
	if ((p_processCount->producerCount == 0) && (p_processCount->consumerCount == 0))
	{
		int remID = shmget(REM_KEY, sizeof(int), IPC_CREAT | 0644);
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
		shmdt(p_add);
	}
	
	exit(0);
}

