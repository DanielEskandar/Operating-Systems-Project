#include "headers.h"
#include "scheduler_utilities.h"

// definitions
#define PROCESS "./process.out"

// forward declarations
void cleanup(int signum);
void writeLog(FILE *pFile, int currentTime, struct PCB *p_scheduledPCB, int logType);
void schedulerHPF(struct readyQueue *p_readyQueue, struct process *p_processBufferStart, struct process **p_scheduledProcess, struct PCB **p_scheduledPCB, int currentTime, int *processTable, int PCB_sem, float *weightedTurnaroundTimeArr, int *waitingTimeArr, int *processesFinished, FILE *pFile);

// global variables
int PCB_sem;

int main(int argc, char * argv[])
{
	signal(SIGINT, cleanup);

	// initialize clock
	initClk();
	
	// create shared memory between scheduler and generator to hold the simulation size
	int simSize_shmid = shmget(SIM_SIZE_SHM_KEY, sizeof(int), IPC_CREAT | 0644);
	int *p_simSize = shmat(simSize_shmid, (void *)0, 0);
	int N = *p_simSize; // Total number of processes in simulation
		
	// create shared memory between scheduler and generator
	int scheduler_shmid = shmget(SCHEDULER_SHM_KEY, (sizeof(struct schedulerInfo) + sizeof(struct readyQueue) + (N * sizeof(struct process))), IPC_CREAT | 0644); 
	struct schedulerInfo *p_schedulerInfo = (struct schedulerInfo *) shmat(scheduler_shmid, (void *)0, 0);
	struct readyQueue *p_readyQueue = (struct readyQueue *) (p_schedulerInfo + 1);
	struct process *p_processBufferStart = (struct processs *) (p_readyQueue + 1);
	
	// create sempahore between scheduler and generator
	int scheduler_sem = semget(SCHEDULER_SEM_KEY, 1, IPC_CREAT | 0644);
	
	// create semaphore between scheduler and process (initialized with 0)
	PCB_sem = semget(PCB_SEM_KEY, 1, IPC_CREAT | 0644);
	union Semun semun;
	semun.val = 0;
	if (semctl(PCB_sem, 0, SETVAL, semun) == -1)
	{
		perror("Error in semctl\n");
		exit(-1);
	}	
	
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
	fprintf(pFile, "# At time x process y state arr w total z remain y wait k\n");
	
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
				schedulerHPF(p_readyQueue, p_processBufferStart, &p_scheduledProcess, &p_scheduledPCB, currentTime, processTable, PCB_sem, weightedTurnaroundTimeArr, waitingTimeArr, &processesFinished, pFile);								
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
	#ifdef PRINTING
		printf("====================\n");
		printf("SIMULATION FINISHED\n");
		printf("====================\n");
	#endif	
	
	// close log file
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
			fprintf(pFile, "TA %d ", (currentTime - p_scheduledPCB->arrivalTime));
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

void schedulerHPF(struct readyQueue *p_readyQueue, struct process *p_processBufferStart, struct process **p_scheduledProcess, struct PCB **p_scheduledPCB, int currentTime, int *processTable, int PCB_sem, float *weightedTurnaroundTimeArr, int *waitingTimeArr, int *processesFinished, FILE *pFile)
{
	#ifdef PRINTING
		printf("============\n");
		printf("TIME STEP %d\n", currentTime);
		printf("============\n");
	#endif	
	if ((*p_scheduledProcess) != NULL) // if a process is running
	{
		// update process and PCB data
		(*p_scheduledProcess)->remainingTime--;
		(*p_scheduledPCB)->remainingTime--;
		(*p_scheduledPCB)->waitingTime = (currentTime - (*p_scheduledPCB)->arrivalTime) - ((*p_scheduledPCB)->executionTime - (*p_scheduledPCB)->remainingTime);
		
		if ((*p_scheduledProcess)->remainingTime == 0) // if the process finished execution
		{
			#ifdef PRINTING
				printf("Process %d has finished\n", (*p_scheduledPCB)->id);
			#endif			
			// increment number of finished processes
			(*processesFinished)++;
			
			// fill performance arrays
			weightedTurnaroundTimeArr[(*p_scheduledPCB)->id - 1] = (currentTime - (*p_scheduledPCB)->arrivalTime) / (float) (*p_scheduledPCB)->executionTime;
			waitingTimeArr[(*p_scheduledPCB)->id - 1] = (*p_scheduledPCB)->waitingTime;
			
			// write log
			writeLog(pFile, currentTime, (*p_scheduledPCB), FINISHED);
			
			// delete PCB
			int PCB_shmid = shmget(processTable[(*p_scheduledPCB)->id - 1], sizeof(struct PCB), IPC_CREAT | 0644);
			shmctl(PCB_shmid, IPC_RMID, (struct shmid_ds *) 0);
			
			// dequeue process
			dequeue(p_readyQueue, p_processBufferStart, (*p_scheduledProcess));
			
			if (p_readyQueue->head != -1) // if  ready queue is not empty
			{
				// schedule next process with highest priority
				(*p_scheduledProcess) = p_processBufferStart + p_readyQueue->head;
				#ifdef PRINTING
					printf("Process %d is scheduled\n", (*p_scheduledProcess)->id);
				#endif
				
				// start process and store its pid in the process table
				processTable[(*p_scheduledProcess)->id - 1] = createProcess(PROCESS);
				
				// create PCB to be share with the process
				PCB_shmid = shmget(processTable[(*p_scheduledProcess)->id - 1], sizeof(struct PCB), IPC_CREAT | 0644);
				(*p_scheduledPCB) = shmat(PCB_shmid, (void *)0, 0);
				
				// initialize PCB
				(*p_scheduledPCB)->id = (*p_scheduledProcess)->id;
				(*p_scheduledPCB)->state = RUNNING;
				(*p_scheduledPCB)->arrivalTime = (*p_scheduledProcess)->arrivalTime;
				(*p_scheduledPCB)->executionTime = (*p_scheduledProcess)->runningTime;
				(*p_scheduledPCB)->remainingTime = (*p_scheduledProcess)->remainingTime;
				(*p_scheduledPCB)->waitingTime = (currentTime - (*p_scheduledPCB)->arrivalTime) - ((*p_scheduledPCB)->executionTime - (*p_scheduledPCB)->remainingTime);
				(*p_scheduledPCB)->priority = (*p_scheduledProcess)->priority;
				
				// enable process to read PCB
				up(PCB_sem);
				
				// write log
				writeLog(pFile, currentTime, (*p_scheduledPCB), STARTED);			
			}
			else // process finished and ready queue is empty
			{
				#ifdef PRINTING
					printf("No process is scheduled\n");
				#endif
				(*p_scheduledProcess) = NULL;
			}
		}
		else
		{
			#ifdef PRINTING
				printf("Process %d is running, remaining time = %d\n", (*p_scheduledProcess)->id, (*p_scheduledProcess)->remainingTime);
			#endif
		}
	}
	else // no process running
	{
		#ifdef PRINTING
			printf("No process is running\n");
		#endif		
		if (p_readyQueue->head != -1) // if  ready queue is not empty
		{
			// schedule next process with highest priority
			(*p_scheduledProcess) = p_processBufferStart + p_readyQueue->head;		
			#ifdef PRINTING
				printf("Process %d is scheduled\n", (*p_scheduledProcess)->id);
			#endif

			// start process and store its pid in the process table
			processTable[(*p_scheduledProcess)->id - 1] = createProcess(PROCESS);

			// create PCB to be share with the process
			int PCB_shmid = shmget(processTable[(*p_scheduledProcess)->id - 1], sizeof(struct PCB), IPC_CREAT | 0644);
			(*p_scheduledPCB) = shmat(PCB_shmid, (void *)0, 0);

			// initialize PCB
			(*p_scheduledPCB)->id = (*p_scheduledProcess)->id;
			(*p_scheduledPCB)->state = RUNNING;
			(*p_scheduledPCB)->arrivalTime = (*p_scheduledProcess)->arrivalTime;
			(*p_scheduledPCB)->executionTime = (*p_scheduledProcess)->runningTime;
			(*p_scheduledPCB)->remainingTime = (*p_scheduledProcess)->remainingTime;
			(*p_scheduledPCB)->waitingTime = 0;
			(*p_scheduledPCB)->priority = (*p_scheduledProcess)->priority;
			
			// enable process to read PCB
			up(PCB_sem);
			
			// write log
			writeLog(pFile, currentTime, (*p_scheduledPCB), STARTED);
			
		}
	}
}

