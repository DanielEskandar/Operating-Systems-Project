#include "headers.h"

// forward declarations
void produceItem(int item, int *p_buffer, int *p_add);

int main()
{
	// attach to buffer shared memory
	int bufferID = shmget(BUFFER_KEY, (BUFFER_SIZE * sizeof(int)), IPC_CREAT | 0644);
	int *p_buffer = shmat(bufferID, (void *) 0, 0);
	
	// attch to add shared memory
	int *p_add;
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
	int mutex = semget(MUTEX_KEY, 1, IPC_CREAT | IPC_EXCL | 0644);
	int full = semget(FULL_KEY, 1, IPC_CREAT | IPC_EXCL | 0644);
	int empty = semget(EMPTY_KEY, 1, IPC_CREAT | IPC_EXCL | 0644);
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
	
	
	for (int i = 0; i < TOTAL_ITEMS; i++)
	{
		down(empty);
		down(mutex);
		produceItem(i, p_buffer, p_add);
		up(mutex);
		up(full);	
	}
	
	printf("Producer quitting\n");		

	return 0;
}

void produceItem(int item, int *p_buffer, int *p_add)
{
	int location = *p_add;
	p_buffer[*p_add] = item;
	*p_add = (*p_add + 1) % BUFFER_SIZE;
	printf("Producer: inserted item %d in location %d\n", item, location);
}

