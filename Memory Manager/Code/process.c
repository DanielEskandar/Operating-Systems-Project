#include "headers.h"
#include "scheduler_utilities.h"

int main(int agrc, char * argv[])
{
	// initialize clock
	initClk();
	
	// get PCB address
	int PCB_shmid = shmget(getpid(), sizeof(struct PCB), IPC_CREAT | 0644);
	struct PCB *p_PCB = shmat(PCB_shmid, (void *)0, 0);
	
	// get semaphore between scheduler and process
	int PCB_sem = semget(PCB_SEM_KEY, 1, IPC_CREAT | 0644);
	
	// wait for scheduler to initialize PCB
	down(PCB_sem);

	int remainingtime = p_PCB->remainingTime;
	while (remainingtime > 0)
	{
		remainingtime = p_PCB->remainingTime;
	}

	destroyClk(false);

	return 0;
}
