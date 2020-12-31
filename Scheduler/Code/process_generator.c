#include "headers.h"
#include "scheduler_utilities.h"

// definitions
#define CLK_PROCESS "./clk.out"
#define SCHEDULER_PROCESS "./scheduler.out"

// forward declarations
void clearResources(int signum);
int createProcess(char *file);

int main(int argc, char * argv[])
{
	signal(SIGINT, clearResources);

	// open file
	FILE *pFile = fopen(argv[1], "r");    
	if (pFile == NULL)
	{
		printf("Could not open file %s\n", argv[1]);
		return -1;
	}

	// count number of processes N
	int N = -1;
	char c;
	for (c = getc(pFile); c != EOF; c = getc(pFile))
	{
		if (c == '\n')
		{
			N += 1;
		}
	}   
	fclose(pFile);
	printf("N = %d\n", N);

	// read input file and create an array of processes
	pFile = fopen(argv[1], "r");
	struct process *processArray = (struct process *) malloc(N * sizeof(struct process));
	fscanf(pFile, "%*[^\n]\n"); 
	for (int i = 0; i < N; i++)
	{
		fscanf(pFile, "%d\t%d\t%d\t%d\n", &processArray[i].id, &processArray[i].arrivalTime, &processArray[i].runningTime, &processArray[i].priority);
		processArray[i].remainingTime = processArray[i].runningTime;
		processArray[i].next = NULL;
		processArray[i].prev = NULL;
	}
	fclose(pFile);
	
	// create sempahore between scheduler and generator
	int scheduler_sem = semget(SCHEDULER_SEM_KEY, 1, IPC_CREAT | 0644);
	union Semun semun;
	semun.val = 0;
	if (semctl(scheduler_sem, 0, SETVAL, semun) == -1)
	{
		perror("Error in semctl\n");
		exit(-1);
	}
	
	// create shared memory between scheduler and generator
	int scheduler_shmid = shmget(SCHEDULER_SHM_KEY, (sizeof(struct schedulerInfo) + sizeof(struct readyQueue) + (N * sizeof(struct process))), IPC_CREAT | 0644);
	printf("Shared memory size = %ld\n", (sizeof(struct schedulerInfo) + sizeof(struct readyQueue) + (N * sizeof(struct process)))); 
	struct schedulerInfo *p_schedulerInfo = (struct schedulerInfo *) shmat(scheduler_shmid, (void *)0, 0);
	struct readyQueue *p_readyQueue = (struct readyQueue *) (p_schedulerInfo + 1);
	// initialize shared memory
	p_schedulerInfo->generationFinished = false;
	p_schedulerInfo->quantum = 0;
	p_readyQueue->head = NULL;
	p_readyQueue->tail = NULL;
	p_readyQueue->size = 0;
	p_readyQueue->processArrival = false;
	
	// ask the user for the chose algorithm
	int type;
	printf("Choose a scheduling algorithm (0:HPF 1:SRTN 2:RR): ");
	scanf("%d", &type);
	p_schedulerInfo->schedulerType = type;
	if (p_schedulerInfo->schedulerType == RR)
	{
		int q;
		printf("Determine a quantum value: ");
		scanf("%d", &q);
		p_schedulerInfo->quantum = q;
	}

	// initiate and create the scheduler and clock processes.
	int scheduler_pid = createProcess(SCHEDULER_PROCESS);
	int clk_pid = createProcess(CLK_PROCESS);

	// initialize clock
	initClk();
	int currentTime = getClk();
	printf("Current time is %d\n", currentTime);
	
	// generation main loop
	int processIndex = 0;
	struct process *p_process = (struct process *) (p_readyQueue + 1);
	while (!p_schedulerInfo->generationFinished)
	{
		// send the information to the scheduler in its appropriate time
		while (processArray[processIndex].arrivalTime == currentTime)
		{			 
			*p_process = processArray[processIndex]; // physical allocation
			enqueue(p_readyQueue, p_process, p_schedulerInfo->schedulerType);
			
			if ((processIndex + 1) == N)
			{
				p_schedulerInfo->generationFinished = true;
				break;
			}
			p_process += 1;
			processIndex++;
		}
					
		// enable scheduler to operate on the ready queue
		// up(scheduler_sem);
		
		// wait until clk changes
		while (currentTime == getClk());
		currentTime = getClk();
	}
	
	struct process *current = p_readyQueue->head;
	while (current != NULL)
	{
		printf("%d\t%d\t%d\t%d\n", current->id, current->arrivalTime, current->runningTime, current->priority);
		current = current->next;
	}
	
	// Wait until scheduler finishes
	while (1);
}

void clearResources(int signum)
{
	//TODO Clears all resources in case of interruption
	
	// free	processArray
	// clear shared memroy between generator and scheduler
	// clear semaphore between generator and scheduler

	// clear clk resources
	destroyClk(true);

	exit(0);
}

int createProcess(char *file)
{
	int pid = fork();
	if (pid == -1)
	{
		perror("Error in fork\n");
		clearResources(SIGINT);
	}
	else if (pid == 0)
	{
		char *args[] = {file, NULL}; 
		if (execvp(args[0], args) == -1)
		{
			printf("Error in executing %s\n", file);
			exit(0);
		}
	}
	else
	{
		return pid;
	}
}

