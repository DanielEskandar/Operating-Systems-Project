#include "headers.h"
#include "scheduler_utilities.h"

// definitions
#define CLK_PROCESS "./clk.out"
#define SCHEDULER_PROCESS "./scheduler.out"

// forward declarations
void clearResources(int signum);

// global variables
int simSize_shmid;
int scheduler_shmid;
int scheduler_sem;
struct process *processArray;

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

	// count number of processes in simulation
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

	// read input file and create an array of processes
	pFile = fopen(argv[1], "r");
	processArray = (struct process *) malloc(N * sizeof(struct process));
	fscanf(pFile, "%*[^\n]\n");
	for (int i = 0; i < N; i++)
	{
		fscanf(pFile, "%d\t%d\t%d\t%d\t%d\n", &processArray[i].id, &processArray[i].arrivalTime, &processArray[i].runningTime, &processArray[i].priority, &processArray[i].memSize);
		processArray[i].remainingTime = processArray[i].runningTime;
		if (processArray[i].memSize > SMALLEST_ALLOCATION_UNIT)
		{
			processArray[i].allocationSize = pow(2, ceil(log2f((float) processArray[i].memSize)));
		}
		else
		{
			processArray[i].allocationSize = SMALLEST_ALLOCATION_UNIT;
		}
		processArray[i].allocatedMemUnit = NULL;
		processArray[i].next = -1;
		processArray[i].prev = -1;
	}
	fclose(pFile);

	// create shared memory between scheduler and generator to hold the simulation size
	simSize_shmid = shmget(SIM_SIZE_SHM_KEY, sizeof(int), IPC_CREAT | 0644);
	int *p_simSize = shmat(simSize_shmid, (void *)0, 0);
	*p_simSize = N; // Total number of processes in simulation

	// create shared memory between scheduler and generator
	scheduler_shmid = shmget(SCHEDULER_SHM_KEY, (sizeof(struct schedulerInfo) + sizeof(struct readyQueue) + (N * sizeof(struct process))), IPC_CREAT | 0644); 
	struct schedulerInfo *p_schedulerInfo = (struct schedulerInfo *) shmat(scheduler_shmid, (void *) 0, 0);
	struct readyQueue *p_readyQueue = (struct readyQueue *) (p_schedulerInfo + 1);
	struct process *p_processBufferStart = (struct processs *) (p_readyQueue + 1);
	
	// initialize shared memory
	p_schedulerInfo->generationFinished = false;
	p_schedulerInfo->quantum = 0;
	p_readyQueue->head = -1;
	p_readyQueue->tail = -1;
	p_readyQueue->processArrival = false;
	
	// create sempahore between scheduler and generator
	scheduler_sem = semget(SCHEDULER_SEM_KEY, 1, IPC_CREAT | 0644);
	union Semun semun;
	semun.val = 0;
	if (semctl(scheduler_sem, 0, SETVAL, semun) == -1)
	{
		perror("Error in semctl\n");
		exit(-1);
	}
	
	#ifdef PRINTING
		int simTime = processArray[0].arrivalTime;
		for (int i = 0; i < N; i++)
		{
			simTime += processArray[i].runningTime;
		}
	#endif
	
	// ask the user for the chosen algorithm
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
	while (1)
	{
		#ifdef PRINTING
			if (currentTime <= simTime)
			{
				printf("============\n");
				printf("TIME STEP %d\n", currentTime);
				printf("============\n");
			}
		#endif	
		if (!p_schedulerInfo->generationFinished)
		{
			// send the information to the scheduler in its appropriate time
			while (processArray[processIndex].arrivalTime == currentTime)
			{			 
				*p_process = processArray[processIndex]; // physical allocation
				enqueue(p_readyQueue, p_processBufferStart, p_process, processIndex, p_schedulerInfo->schedulerType);
				p_readyQueue->processArrival = true;
				#ifdef PRINTING
					printf("Process Generator: Process %d arrived\n", p_process->id);
				#endif	
				
				if ((processIndex + 1) == N)
				{
					p_schedulerInfo->generationFinished = true;
					break;
				}
				p_process += 1;
				processIndex++;
			}
		}
					
		// enable scheduler to operate on the ready queue
		up(scheduler_sem);
		
		// wait until clk changes
		while (currentTime == getClk());
		currentTime = getClk();
	}
}

void clearResources(int signum)
{
	// free dynamically allocated data
	free(processArray);
	
	// clear resources
	shmctl(simSize_shmid, IPC_RMID, (struct shmid_ds *) 0);
	shmctl(scheduler_shmid, IPC_RMID, (struct shmid_ds *) 0);
	semctl(scheduler_sem, IPC_RMID, 0, (struct semid_ds *) 0);

	// clear clk resources
	destroyClk(true);

	exit(0);
}



