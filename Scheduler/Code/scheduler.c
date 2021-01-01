#include "headers.h"
#include "scheduler_utilities.h"

// definitions
#define PROCESS "./process.out"
#define DEBUGGING

// forward declarations
void cleanup(int signum);
void writeLog(FILE *pFile, int currentTime, struct PCB *p_scheduledPCB, int logType);
void schedulerHPF(struct readyQueue *p_readyQueue, struct process **p_scheduledProcess, struct PCB **p_scheduledPCB, int currentTime, int *processTable, int PCB_sem, float *weightedTurnaroundTimeArr, int *waitingTimeArr, int *processesFinished, FILE *pFile);

// global variables
int PCB_sem;

int main(int argc, char * argv[])
{
	signal(SIGINT, cleanup);

	initClk();
	
	// create shared memory between scheduler and generator to hold the simulation size
	int simSize_shmid = shmget(SIM_SIZE_SHM_KEY, sizeof(int), IPC_CREAT | 0644);
	int *p_simSize = shmat(simSize_shmid, (void *)0, 0);
	int N = *p_simSize; // Total number of processes in simulation
	#ifdef DEBUGGING
		printf("simSize = %d\n", N);
	#endif
		
	// create shared memory between scheduler and generator
	int scheduler_shmid = shmget(SCHEDULER_SHM_KEY, (sizeof(struct schedulerInfo) + sizeof(struct readyQueue) + (N * sizeof(struct process))), IPC_CREAT | 0644); 
	struct schedulerInfo *p_schedulerInfo = (struct schedulerInfo *) shmat(scheduler_shmid, (void *)0, 0);
	struct readyQueue *p_readyQueue = (struct readyQueue *) (p_schedulerInfo + 1);
	#ifdef DEBUGGING
		printf("scheduler_shmid = %d\n", scheduler_shmid);
	#endif
	
	// create sempahore between scheduler and generator
	int scheduler_sem = semget(SCHEDULER_SEM_KEY, 1, IPC_CREAT | 0644);
	#ifdef DEBUGGING
		printf("scheduler_sem = %d\n", scheduler_sem);
	#endif
	
	// create semaphore between scheduler and process (initialized with 0)
	PCB_sem = semget(PCB_SEM_KEY, 1, IPC_CREAT | 0644);
	union Semun semun;
	semun.val = 0;
	if (semctl(PCB_sem, 0, SETVAL, semun) == -1)
	{
		perror("Error in semctl\n");
		exit(-1);
	}
	#ifdef DEBUGGING
		printf("PCB_sem = %d\n", PCB_sem);
	#endif
	
	
	// The process table contains the pid of each process. The pid in the process table is used as an
	// id to get the address of the shared memory where the PCB of the corresponding process is stored.
	// The process table is initialized with -1 and overwritten during forking.
	int *processTable = (int *) malloc(N * sizeof(int));
	for (int i = 0; i < N; i++)
	{
		processTable[i] = -1;
	}
	
	// performance arrays
	float *weightedTurnaroundTimeArr = (float *) malloc(N * sizeof(float));
	int *waitingTimeArr = (int *) malloc(N * sizeof(int));
		
	// open scheduler.log
	FILE *pFile;
	pFile = fopen(LOG, "w");
	#ifdef DEBUGGING
		printf("log file opened\n");
	#endif
	
	// scheduler main loop
	int processesFinished = 0;
	struct process *p_scheduledProcess = NULL;
	struct PCB *p_scheduledPCB = NULL;
	int currentTime = getClk();
	while(processesFinished != N)
	{
		// wait for generator to finished processing on the ready queue
		down(scheduler_sem);
		
		switch (p_schedulerInfo->schedulerType)
		{
			case HPF:
				schedulerHPF(p_readyQueue, &p_scheduledProcess, &p_scheduledPCB, currentTime, processTable, PCB_sem, weightedTurnaroundTimeArr, waitingTimeArr, 					&processesFinished, pFile);								
				break;
			
			case SRTN:
				// schedulerSRTN();
				break;
			case RR:
				// schedulerRR();
				break;
		}
		
		// wait until clk changes
		while (currentTime == getClk());
		currentTime = getClk();
	}
	
	fclose(pFile);

	// upon termination release the clock resources
	raise(SIGINT);
}

void cleanup(int signum)
{
	// clear sempahore between scheduler and process
	semctl(PCB_sem, IPC_RMID, 0, (struct semid_ds *) 0);

	// clear clk shared memory
	destroyClk(false);
	
	// interrupt process generator
	kill(getppid(), SIGINT);

	//terminate
	exit(0);
}

void writeLog(FILE *pFile, int currentTime, struct PCB *p_scheduledPCB, int logType)
{
	switch (logType)
	{
		case STARTED:
			fprintf(pFile, "At time %d ", currentTime);
			fprintf(pFile, "process %d started ", p_scheduledPCB->id);
			fprintf(pFile, "arr %d ", p_scheduledPCB->arrivalTime);
			fprintf(pFile, "total %d ", p_scheduledPCB->executionTime);
			fprintf(pFile, "remain %d ", p_scheduledPCB->remainingTime);
			fprintf(pFile, "wait %d\n", p_scheduledPCB->waitingTime);
			return;
		
		case STOPPED:
			fprintf(pFile, "At time %d ", currentTime);
			fprintf(pFile, "process %d stopped ", p_scheduledPCB->id);
			fprintf(pFile, "arr %d ", p_scheduledPCB->arrivalTime);
			fprintf(pFile, "total %d ", p_scheduledPCB->executionTime);
			fprintf(pFile, "remain %d ", p_scheduledPCB->remainingTime);
			fprintf(pFile, "wait %d\n", p_scheduledPCB->waitingTime);
			return;
		
		case FINISHED:
			fprintf(pFile, "At time %d ", currentTime);
			fprintf(pFile, "process %d finished ", p_scheduledPCB->id);
			fprintf(pFile, "arr %d ", p_scheduledPCB->arrivalTime);
			fprintf(pFile, "total %d ", p_scheduledPCB->executionTime);
			fprintf(pFile, "remain %d ", p_scheduledPCB->remainingTime);
			fprintf(pFile, "wait %d ", p_scheduledPCB->waitingTime);
			fprintf(pFile, " TA %d ", (currentTime - p_scheduledPCB->arrivalTime));
			fprintf(pFile, "WTA %.2f\n", ((currentTime - p_scheduledPCB->arrivalTime) / (float) p_scheduledPCB->executionTime));
			return;
		
		case RESUMED:
			fprintf(pFile, "At time %d ", currentTime);
			fprintf(pFile, "process %d resumed ", p_scheduledPCB->id);
			fprintf(pFile, "arr %d ", p_scheduledPCB->arrivalTime);
			fprintf(pFile, "total %d ", p_scheduledPCB->executionTime);
			fprintf(pFile, "remain %d ", p_scheduledPCB->remainingTime);
			fprintf(pFile, "wait %d\n", p_scheduledPCB->waitingTime);
			return;
	}
}

void schedulerHPF(struct readyQueue *p_readyQueue, struct process **p_scheduledProcess, struct PCB **p_scheduledPCB, int currentTime, int *processTable, int PCB_sem, float *weightedTurnaroundTimeArr, int *waitingTimeArr, int *processesFinished, FILE *pFile)
{
	#ifdef DEBUGGING
		printf("entered scheduler at %d\n", currentTime);
	#endif
	if (p_scheduledProcess[0] != NULL) // if a process is running
	{
		#ifdef DEBUGGING
			printf("process %d is running\n", p_scheduledProcess[0]->id);
		#endif
		// update process and PCB data
		p_scheduledProcess[0]->remainingTime--;
		p_scheduledPCB[0]->remainingTime--;
		p_scheduledPCB[0]->waitingTime = (currentTime - p_scheduledPCB[0]->arrivalTime) - (p_scheduledPCB[0]->executionTime - p_scheduledPCB[0]->remainingTime);
		p_scheduledPCB[0]->state = RUNNING;
		
		if (p_scheduledProcess[0]->remainingTime == 0) // if the process finished execution
		{
			#ifdef DEBUGGING
				printf("process %d finished\n", p_scheduledPCB[0]->id);
			#endif
			// increment number of finished processes
			*processesFinished++;
			
			// fill performance arrays
			weightedTurnaroundTimeArr[p_scheduledPCB[0]->id - 1] = (currentTime - p_scheduledPCB[0]->arrivalTime) / (float) p_scheduledPCB[0]->executionTime;
			waitingTimeArr[p_scheduledPCB[0]->id - 1] = p_scheduledPCB[0]->waitingTime;
			
			// write log
			writeLog(pFile, currentTime, p_scheduledPCB[0], FINISHED);
			
			// delete PCB
			int PCB_shmid = shmget(processTable[p_scheduledPCB[0]->id - 1], sizeof(struct PCB), IPC_CREAT | 0644);
			shmctl(PCB_shmid, IPC_RMID, (struct shmid_ds *) 0);
			
			// dequeue process
			dequeue(p_readyQueue, p_scheduledProcess[0]);
			#ifdef DEBUGGING
				printf("process %d dequeued\n", p_scheduledProcess[0]->id);
			#endif
			
			if (p_readyQueue->size != 0) // if  ready queue is not empty
			{
				// schedule next process with highest priority
				p_scheduledProcess[0] = p_readyQueue->head;
				#ifdef DEBUGGING
					printf("process %d scheduled\n", p_scheduledProcess[0]->id);
				#endif
				
				// start process and store its pid in the process table
				processTable[p_scheduledProcess[0]->id - 1] = createProcess(PROCESS);
				
				// create PCB to be share with the process
				PCB_shmid = shmget(processTable[p_scheduledProcess[0]->id - 1], sizeof(struct PCB), IPC_CREAT | 0644);
				p_scheduledPCB[0] = shmat(PCB_shmid, (void *)0, 0);
				
				// initialize PCB
				p_scheduledPCB[0]->id = p_scheduledProcess[0]->id;
				p_scheduledPCB[0]->state = RUNNING;
				p_scheduledPCB[0]->arrivalTime = p_scheduledProcess[0]->arrivalTime;
				p_scheduledPCB[0]->executionTime = p_scheduledProcess[0]->runningTime;
				p_scheduledPCB[0]->remainingTime = p_scheduledProcess[0]->remainingTime;
				p_scheduledPCB[0]->waitingTime = 0;
				p_scheduledPCB[0]->priority = p_scheduledProcess[0]->priority;
				#ifdef DEBUGGING
					printf("PCB created for process %d\n", p_scheduledPCB[0]->id);
				#endif
				
				// enable process to read PCB
				up(PCB_sem);
				
				// write log
				writeLog(pFile, currentTime, p_scheduledPCB[0], STARTED);			
			}
			else // process finished and ready queue is empty
			{
				p_scheduledProcess[0] = NULL;
				#ifdef DEBUGGING
					printf("no process scheduled\n");
				#endif
			}
		}
	}
	else // no process running
	{
		#ifdef DEBUGGING
			printf("no process is running\n");
		#endif
		if (p_readyQueue->size != 0) // if  ready queue is not empty
		{
			#ifdef DEBUGGING
				printf("queue is not empty\n");
				printf("head: %p, scheduled: %p\n", p_readyQueue->head, p_scheduledProcess[0]);
			#endif
			// schedule next process with highest priority
			p_scheduledProcess[0] = p_readyQueue->head;
			#ifdef DEBUGGING
				printf("scheduled: %p \n", p_scheduledProcess[0]);
				printf("process %d scheduled\n", p_scheduledProcess[0]->id);
			#endif
			
			// start process and store its pid in the process table
			int process_id = createProcess(PROCESS);
			processTable[p_scheduledProcess[0]->id - 1] = process_id;
			#ifdef DEBUGGING
				printf("process forked\n");
			#endif
						
			// create PCB to be share with the process
			int PCB_shmid = shmget(processTable[p_scheduledProcess[0]->id - 1], sizeof(struct PCB), IPC_CREAT | 0644);
			p_scheduledPCB[0] = shmat(PCB_shmid, (void *)0, 0);
			
			// initialize PCB
			p_scheduledPCB[0]->id = p_scheduledProcess[0]->id;
			p_scheduledPCB[0]->state = RUNNING;
			p_scheduledPCB[0]->executionTime = p_scheduledProcess[0]->runningTime;
			p_scheduledPCB[0]->remainingTime = p_scheduledProcess[0]->remainingTime;
			p_scheduledPCB[0]->waitingTime = 0;
			p_scheduledPCB[0]->priority = p_scheduledProcess[0]->priority;
			#ifdef DEBUGGING
				printf("PCB created for process %d\n", p_scheduledPCB[0]->id);
			#endif
			
			// enable process to read PCB
			up(PCB_sem);
			
			// write log
			writeLog(pFile, currentTime, p_scheduledPCB[0], STARTED);
			
		}
		else
		{
			#ifdef DEBUGGING
				printf("queue is empty\n");
			#endif
		}
	}
}

