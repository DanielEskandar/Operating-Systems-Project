#include "headers.h"
#include "scheduler_utilities.h"
#include <math.h>

// definitions
#define PROCESS "./process.out"

// forward declarations
void cleanup(int signum);
void writeLog(FILE *pFile, int currentTime, struct PCB *p_scheduledPCB, int logType);
void schedulerHPF(struct readyQueue *p_readyQueue, struct process *p_processBufferStart, struct process **p_scheduledProcess, struct PCB **p_scheduledPCB, int currentTime, int *processTable, int PCB_sem, float *weightedTurnaroundTimeArr, int *waitingTimeArr, int *processesFinished, FILE *pFile, struct memUnit *memory, FILE *pMemFile);
void schedulerSRTN(struct readyQueue *p_readyQueue, struct process *p_processBufferStart, struct process **p_scheduledProcess, struct PCB **p_scheduledPCB, int currentTime, int *processTable, int PCB_sem, float *weightedTurnaroundTimeArr, int *waitingTimeArr, int *processesFinished, FILE *pFile, struct memUnit *memory, FILE *pMemFile);
void schedulerRR(struct readyQueue *p_readyQueue, struct process *p_processBufferStart, struct process **p_scheduledProcess, struct PCB **p_scheduledPCB, int currentTime, int *processTable, int PCB_sem, float *weightedTurnaroundTimeArr, int *waitingTimeArr, int *processesFinished, int quantum, int *processQuantum, FILE *pFile, struct memUnit *memory, FILE *pMemFile);
void writeMemLog(FILE *pFile, int currentTime, struct process* p_process, int logType);
void manageMemory(FILE *pFile, int currentTime, struct memUnit *memory, struct process* p_process);

// global variables
int PCB_sem;
float *weightedTurnaroundTimeArr;
int *waitingTimeArr;
struct memUnit *memory;

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
	weightedTurnaroundTimeArr = (float *) malloc(N * sizeof(float));
	waitingTimeArr = (int *) malloc(N * sizeof(int));
	
	// memory
	memory = (struct memUnit *) malloc(sizeof(struct memUnit));
	memory->id = -1;
	memory->size = 1024;
	memory->start = 0;
	memory->parent = NULL;
	memory->left = NULL;
	memory->right = NULL;
		
	// open scheduler.log
	FILE *pFile;
	pFile = fopen(LOG, "w");
	fprintf(pFile, "# At time x process y state arr w total z remain y wait k\n");
	
	// open memory.log
	FILE *pMemFile;
	pMemFile = fopen(MEMORY, "w");
	fprintf(pMemFile, "# At time x allocated y bytes for process z from i to j\n");
	
	// scheduler main loop
	int wastedTime = 0;
	int processesFinished = 0;
	int processQuantum = 0;
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
				schedulerHPF(p_readyQueue, p_processBufferStart, &p_scheduledProcess, &p_scheduledPCB, currentTime, processTable, PCB_sem, weightedTurnaroundTimeArr, waitingTimeArr, &processesFinished, pFile, memory, pMemFile);								
				break;
			
			case SRTN:
				schedulerSRTN(p_readyQueue, p_processBufferStart, &p_scheduledProcess, &p_scheduledPCB, currentTime, processTable, PCB_sem, weightedTurnaroundTimeArr, waitingTimeArr, &processesFinished, pFile, memory, pMemFile);
				break;
			case RR:
				schedulerRR(p_readyQueue, p_processBufferStart, &p_scheduledProcess, &p_scheduledPCB, currentTime, processTable, PCB_sem, weightedTurnaroundTimeArr, waitingTimeArr, &processesFinished, p_schedulerInfo->quantum, &processQuantum, pFile, memory, pMemFile);
				break;
		}
		
		// wait until clk changes
		while (currentTime == getClk());
		if (processesFinished != N)
		{
			currentTime = getClk();
			if (p_scheduledProcess == NULL)
			{
				wastedTime++;
			}
		}		
	}
	#ifdef PRINTING
		printf("\033[0;32m"); // set color to green
		printf("===================\n");
		printf("SIMULATION FINISHED\n");
		printf("===================\n");
		printf("\033[0;37m"); // set color to white
	#endif	
	
	// close log files
	fclose(pFile);
	fclose(pMemFile);
	
	// performance log
	pFile = fopen(PERFORMANCE, "w");
	fprintf(pFile, "CPU utilization = %.2f%%\n", ((currentTime - wastedTime) / (float) currentTime) * 100); // CPU utilization
	float avgWTA = 0;
	float avgWT = 0;
	for (int i = 0; i < N; i++)
	{
		avgWTA += weightedTurnaroundTimeArr[i];
		avgWT += waitingTimeArr[i];
	}
	avgWTA /= N;
	avgWT /= N;
	fprintf(pFile, "Avg WTA = %.2f\n", avgWTA); // average weighted turnaround time
	fprintf(pFile, "Avg Waiting = %.2f\n", avgWT); // average waiting time
	float std = 0;
	for (int i = 0; i < N; i++)
	{
		std += powf((weightedTurnaroundTimeArr[i] - avgWTA), 2);
	}
	fprintf(pFile, "Std WTA = %.2f\n", sqrtf(std / N));
	fclose(pFile);

	// upon termination release the clock resources
	raise(SIGINT);
}

void cleanup(int signum)
{
	// free dynamically allocated memory
	free(weightedTurnaroundTimeArr);
	free(waitingTimeArr);
	free(memory);

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

void schedulerHPF(struct readyQueue *p_readyQueue, struct process *p_processBufferStart, struct process **p_scheduledProcess, struct PCB **p_scheduledPCB, int currentTime, int *processTable, int PCB_sem, float *weightedTurnaroundTimeArr, int *waitingTimeArr, int *processesFinished, FILE *pFile, struct memUnit *memory, FILE *pMemFile)
{
	// reset processArrival bool
	p_readyQueue->processArrival = false;
	
	if ((*p_scheduledProcess) != NULL) // if a process is running
	{
		// update process and PCB data
		(*p_scheduledProcess)->remainingTime--;
		(*p_scheduledPCB)->remainingTime--;
		(*p_scheduledPCB)->waitingTime = (currentTime - (*p_scheduledPCB)->arrivalTime) - ((*p_scheduledPCB)->executionTime - (*p_scheduledPCB)->remainingTime);
		
		if ((*p_scheduledProcess)->remainingTime == 0) // if the process finished execution
		{			
			#ifdef PRINTING
				printf("Scheduler: Process %d finished\n", (*p_scheduledPCB)->id);
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
			
			// write mem log and deallocate process
			if ((*p_scheduledProcess)->allocatedMemUnit != NULL)
			{
				writeMemLog(pMemFile, currentTime, (*p_scheduledProcess), FREED);
				#ifdef PRINTING
					printf("Memory Manager: Process %d freed from %d to %d\n", (*p_scheduledProcess)->id, (*p_scheduledProcess)->allocatedMemUnit->start, (*p_scheduledProcess)->allocatedMemUnit->start + (*p_scheduledProcess)->allocationSize - 1);
				#endif
				deallocate((*p_scheduledProcess));
			}
			
			// dequeue process
			dequeue(p_readyQueue, p_processBufferStart, (*p_scheduledProcess));
			
			if (p_readyQueue->head != -1) // if  ready queue is not empty
			{
				// schedule next process with highest priority
				(*p_scheduledProcess) = p_processBufferStart + p_readyQueue->head;
				#ifdef PRINTING
					printf("Scheduler: Process %d is scheduled, remaining time = %d\n", (*p_scheduledProcess)->id, (*p_scheduledProcess)->remainingTime);
				#endif
				
				// allocate memory for scheduled process and write log
				manageMemory(pMemFile, currentTime, memory, (*p_scheduledProcess));
				
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
				p_scheduledProcess == NULL;
				#ifdef PRINTING
					printf("Scheduler: No process is scheduled\n");
				#endif
			}
		}
		else
		{
			#ifdef PRINTING
				printf("Scheduler: Process %d is running, remaining time = %d\n", (*p_scheduledProcess)->id, (*p_scheduledProcess)->remainingTime);
			#endif
		}
	}
	else // no process running
	{		
		if (p_readyQueue->head != -1) // if  ready queue is not empty
		{			
			// schedule next process with highest priority
			(*p_scheduledProcess) = p_processBufferStart + p_readyQueue->head;	
			#ifdef PRINTING
				printf("Scheduler: Process %d is scheduled, remaining time = %d\n", (*p_scheduledProcess)->id, (*p_scheduledProcess)->remainingTime);
			#endif
			
			// allocate memory for scheduled process and write log
			manageMemory(pMemFile, currentTime, memory, (*p_scheduledProcess));

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
			(*p_scheduledPCB)->waitingTime = (currentTime - (*p_scheduledPCB)->arrivalTime) - ((*p_scheduledPCB)->executionTime - (*p_scheduledPCB)->remainingTime);
			(*p_scheduledPCB)->priority = (*p_scheduledProcess)->priority;
			
			// enable process to read PCB
			up(PCB_sem);
			
			// write log
			writeLog(pFile, currentTime, (*p_scheduledPCB), STARTED);
		}
		else
		{
			#ifdef PRINTING
				printf("Scheduler: No process is scheduled\n");
			#endif
		}
	}
}

void schedulerSRTN(struct readyQueue *p_readyQueue, struct process *p_processBufferStart, struct process **p_scheduledProcess, struct PCB **p_scheduledPCB, int currentTime, int *processTable, int PCB_sem, float *weightedTurnaroundTimeArr, int *waitingTimeArr, int *processesFinished, FILE *pFile, struct memUnit *memory, FILE *pMemFile)
{
	if ((*p_scheduledProcess) != NULL) // if a process is running
	{
		// update process and PCB data
		(*p_scheduledProcess)->remainingTime--;
		(*p_scheduledPCB)->remainingTime--;
		(*p_scheduledPCB)->waitingTime = (currentTime - (*p_scheduledPCB)->arrivalTime) - ((*p_scheduledPCB)->executionTime - (*p_scheduledPCB)->remainingTime);
		
		if ((*p_scheduledProcess)->remainingTime == 0) // if the process finished execution
		{
			// reset processArrival bool
			p_readyQueue->processArrival = false;
		
			#ifdef PRINTING
				printf("Scheduler: Process %d finished\n", (*p_scheduledPCB)->id);
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
			
			// write mem log and deallocate process
			if ((*p_scheduledProcess)->allocatedMemUnit != NULL)
			{
				writeMemLog(pMemFile, currentTime, (*p_scheduledProcess), FREED);
				#ifdef PRINTING
					printf("Memory Manager: Process %d freed from %d to %d\n", (*p_scheduledProcess)->id, (*p_scheduledProcess)->allocatedMemUnit->start, (*p_scheduledProcess)->allocatedMemUnit->start + (*p_scheduledProcess)->allocationSize - 1);
				#endif
				deallocate((*p_scheduledProcess));
			}
			
			// dequeue process
			dequeue(p_readyQueue, p_processBufferStart, (*p_scheduledProcess));
			
			if (p_readyQueue->head != -1) // if  ready queue is not empty
			{
				// schedule next process with the lowest remaining time
				(*p_scheduledProcess) = p_processBufferStart + p_readyQueue->head;
								
				if ((*p_scheduledProcess)->remainingTime == (*p_scheduledProcess)->runningTime) // if process is scheduled for the first time
				{
					#ifdef PRINTING
						printf("Scheduler: Process %d is scheduled, remaining time = %d\n", (*p_scheduledProcess)->id, (*p_scheduledProcess)->remainingTime);
					#endif
					
					// allocate memory for scheduled process and write log
					manageMemory(pMemFile, currentTime, memory, (*p_scheduledProcess));
					
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
					(*p_scheduledPCB)->waitingTime = (currentTime - (*p_scheduledPCB)->arrivalTime) - ((*p_scheduledPCB)->executionTime - (*p_scheduledPCB)->remainingTime);
					(*p_scheduledPCB)->priority = (*p_scheduledProcess)->priority;
					
					// enable process to read PCB
					up(PCB_sem);
					
					// write log
					writeLog(pFile, currentTime, (*p_scheduledPCB), STARTED);
				}
				else
				{
					// select PCB
					int PCB_shmid = shmget(processTable[(*p_scheduledProcess)->id - 1], sizeof(struct PCB), IPC_CREAT | 0644);
					(*p_scheduledPCB) = shmat(PCB_shmid, (void *)0, 0);
					(*p_scheduledPCB)->state = RUNNING;
					(*p_scheduledPCB)->waitingTime = (currentTime - (*p_scheduledPCB)->arrivalTime) - ((*p_scheduledPCB)->executionTime - (*p_scheduledPCB)->remainingTime);
					
					#ifdef PRINTING
						printf("Scheduler: Process %d is resumed, remaining time = %d\n", (*p_scheduledProcess)->id, (*p_scheduledProcess)->remainingTime);
					#endif
					// write log
					writeLog(pFile, currentTime, (*p_scheduledPCB), RESUMED);
				}
			}
			else // process finished and ready queue is empty
			{
				p_scheduledProcess == NULL;
				#ifdef PRINTING
					printf("Scheduler: No process is scheduled\n");
				#endif
			}
		}
		else if (p_readyQueue->processArrival)
		{
			// reset processArrival bool
			p_readyQueue->processArrival = false;
		
			if ((*p_scheduledProcess)->prev != -1) // if there is a new head
			{
				#ifdef PRINTING
					printf("Scheduler: Process %d is preempted\n", (*p_scheduledProcess)->id);
				#endif
				// write log
				writeLog(pFile, currentTime, (*p_scheduledPCB), STOPPED);
				
				// schedule next process with the lowest remaining time
				(*p_scheduledPCB)->state = WAITING;
				(*p_scheduledProcess) = p_processBufferStart + p_readyQueue->head;
					
				#ifdef PRINTING
					printf("Scheduler: Process %d is scheduled, remaining time = %d\n", (*p_scheduledProcess)->id, (*p_scheduledProcess)->remainingTime);
				#endif
				
				// allocate memory for scheduled process and write log
				manageMemory(pMemFile, currentTime, memory, (*p_scheduledProcess));

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
				(*p_scheduledPCB)->waitingTime = (currentTime - (*p_scheduledPCB)->arrivalTime) - ((*p_scheduledPCB)->executionTime - (*p_scheduledPCB)->remainingTime);
				(*p_scheduledPCB)->priority = (*p_scheduledProcess)->priority;
				
				// enable process to read PCB
				up(PCB_sem);
				
				// write log
				writeLog(pFile, currentTime, (*p_scheduledPCB), STARTED);
			}
			else
			{
				#ifdef PRINTING
					printf("Scheduler: Process %d is running, remaining time = %d\n", (*p_scheduledProcess)->id, (*p_scheduledProcess)->remainingTime);
				#endif
			}
		}
		else
		{
			#ifdef PRINTING
				printf("Scheduler: Process %d is running, remaining time = %d\n", (*p_scheduledProcess)->id, (*p_scheduledProcess)->remainingTime);
			#endif
		}
	}
	else
	{
		if (p_readyQueue->head != -1) // if  ready queue is not empty
		{
			// reset processArrival bool
			p_readyQueue->processArrival = false;
		
			// schedule next process with the lowest remaining time
			(*p_scheduledProcess) = p_processBufferStart + p_readyQueue->head;
				
			#ifdef PRINTING
				printf("Scheduler: Process %d is scheduled, remaining time = %d\n", (*p_scheduledProcess)->id, (*p_scheduledProcess)->remainingTime);
			#endif
			
			// allocate memory for scheduled process and write log
			manageMemory(pMemFile, currentTime, memory, (*p_scheduledProcess));

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
			(*p_scheduledPCB)->waitingTime = (currentTime - (*p_scheduledPCB)->arrivalTime) - ((*p_scheduledPCB)->executionTime - (*p_scheduledPCB)->remainingTime);
			(*p_scheduledPCB)->priority = (*p_scheduledProcess)->priority;
			
			// enable process to read PCB
			up(PCB_sem);
			
			// write log
			writeLog(pFile, currentTime, (*p_scheduledPCB), STARTED);
		}
		else
		{
			#ifdef PRINTING
				printf("Scheduler: No process is scheduled\n");
			#endif
		}
	}
}

void schedulerRR(struct readyQueue *p_readyQueue, struct process *p_processBufferStart, struct process **p_scheduledProcess, struct PCB **p_scheduledPCB, int currentTime, int *processTable, int PCB_sem, float *weightedTurnaroundTimeArr, int *waitingTimeArr, int *processesFinished, int quantum, int *processQuantum, FILE *pFile, struct memUnit *memory, FILE *pMemFile)
{	
	// reset processArrival bool
	p_readyQueue->processArrival = false;

	if ((*p_scheduledProcess) != NULL) // if a process is running
	{
		// update process and PCB data
		(*p_scheduledProcess)->remainingTime--;
		(*p_scheduledPCB)->remainingTime--;
		(*p_scheduledPCB)->waitingTime = (currentTime - (*p_scheduledPCB)->arrivalTime) - ((*p_scheduledPCB)->executionTime - (*p_scheduledPCB)->remainingTime);
		(*processQuantum)++;
		
		if ((*p_scheduledProcess)->remainingTime == 0) // if the process finished execution
		{		
			#ifdef PRINTING
				printf("Scheduler: Process %d finished\n", (*p_scheduledPCB)->id);
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
			
			// write mem log and deallocate process
			if ((*p_scheduledProcess)->allocatedMemUnit != NULL)
			{
				writeMemLog(pMemFile, currentTime, (*p_scheduledProcess), FREED);
				#ifdef PRINTING
					printf("Memory Manager: Process %d freed from %d to %d\n", (*p_scheduledProcess)->id, (*p_scheduledProcess)->allocatedMemUnit->start, (*p_scheduledProcess)->allocatedMemUnit->start + (*p_scheduledProcess)->allocationSize - 1);
				#endif
				deallocate((*p_scheduledProcess));
			}
			
			if (p_readyQueue->head != -1) // if  ready queue is not empty
			{
				// reset processQuantum
				(*processQuantum) = 0;
				
				// schedule and dequeue next process
				(*p_scheduledProcess) = p_processBufferStart + p_readyQueue->head;
				dequeue(p_readyQueue, p_processBufferStart, (*p_scheduledProcess));
								
				if ((*p_scheduledProcess)->remainingTime == (*p_scheduledProcess)->runningTime) // if process is scheduled for the first time
				{
					#ifdef PRINTING
						printf("Scheduler: Process %d is scheduled, remaining time = %d\n", (*p_scheduledProcess)->id, (*p_scheduledProcess)->remainingTime);
					#endif
					
					// allocate memory for scheduled process and write log
					manageMemory(pMemFile, currentTime, memory, (*p_scheduledProcess));
					
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
					(*p_scheduledPCB)->waitingTime = (currentTime - (*p_scheduledPCB)->arrivalTime) - ((*p_scheduledPCB)->executionTime - (*p_scheduledPCB)->remainingTime);
					(*p_scheduledPCB)->priority = (*p_scheduledProcess)->priority;
					
					// enable process to read PCB
					up(PCB_sem);
					
					// write log
					writeLog(pFile, currentTime, (*p_scheduledPCB), STARTED);
				}
				else
				{
					// select PCB
					int PCB_shmid = shmget(processTable[(*p_scheduledProcess)->id - 1], sizeof(struct PCB), IPC_CREAT | 0644);
					(*p_scheduledPCB) = shmat(PCB_shmid, (void *)0, 0);
					(*p_scheduledPCB)->state = RUNNING;
					(*p_scheduledPCB)->waitingTime = (currentTime - (*p_scheduledPCB)->arrivalTime) - ((*p_scheduledPCB)->executionTime - (*p_scheduledPCB)->remainingTime);
					
					#ifdef PRINTING
						printf("Scheduler: Process %d is resumed, remaining time = %d\n", (*p_scheduledProcess)->id, (*p_scheduledProcess)->remainingTime);
					#endif
					// write log
					writeLog(pFile, currentTime, (*p_scheduledPCB), RESUMED);
				}
			}
			else // process finished and ready queue is empty
			{
				p_scheduledProcess == NULL;
				#ifdef PRINTING
					printf("Scheduler: No process is scheduled\n");
				#endif
			}
		}
		else if ((*processQuantum) == quantum)
		{
			// reset processQuantum
			(*processQuantum) = 0;
			
			#ifdef PRINTING
				printf("Scheduler: Process %d is preempted\n", (*p_scheduledProcess)->id);
			#endif
			// write log
			writeLog(pFile, currentTime, (*p_scheduledPCB), STOPPED);
			
			// enqueue preempted process
			enqueue(p_readyQueue, p_processBufferStart, (*p_scheduledProcess), ((*p_scheduledProcess)->id - 1), RR);
			
			// schedule and dequeue next process
			(*p_scheduledProcess) = p_processBufferStart + p_readyQueue->head;
			dequeue(p_readyQueue, p_processBufferStart, (*p_scheduledProcess));
			
			if ((*p_scheduledProcess)->remainingTime == (*p_scheduledProcess)->runningTime) // if process is scheduled for the first time
			{
				#ifdef PRINTING
					printf("Scheduler: Process %d is scheduled, remaining time = %d\n", (*p_scheduledProcess)->id, (*p_scheduledProcess)->remainingTime);
				#endif
				
				// allocate memory for scheduled process and write log
				manageMemory(pMemFile, currentTime, memory, (*p_scheduledProcess));
				
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
				(*p_scheduledPCB)->waitingTime = (currentTime - (*p_scheduledPCB)->arrivalTime) - ((*p_scheduledPCB)->executionTime - (*p_scheduledPCB)->remainingTime);
				(*p_scheduledPCB)->priority = (*p_scheduledProcess)->priority;
				
				// enable process to read PCB
				up(PCB_sem);
				
				// write log
				writeLog(pFile, currentTime, (*p_scheduledPCB), STARTED);
			}
			else
			{
				// select PCB
				int PCB_shmid = shmget(processTable[(*p_scheduledProcess)->id - 1], sizeof(struct PCB), IPC_CREAT | 0644);
				(*p_scheduledPCB) = shmat(PCB_shmid, (void *)0, 0);
				(*p_scheduledPCB)->state = RUNNING;
				(*p_scheduledPCB)->waitingTime = (currentTime - (*p_scheduledPCB)->arrivalTime) - ((*p_scheduledPCB)->executionTime - (*p_scheduledPCB)->remainingTime);
				
				#ifdef PRINTING
					printf("Scheduler: Process %d is resumed, remaining time = %d\n", (*p_scheduledProcess)->id, (*p_scheduledProcess)->remainingTime);
				#endif
				// write log
				writeLog(pFile, currentTime, (*p_scheduledPCB), RESUMED);
			}
		}
		else
		{
			#ifdef PRINTING
				printf("Scheduler: Process %d is running, remaining time = %d\n", (*p_scheduledProcess)->id, (*p_scheduledProcess)->remainingTime);
			#endif
		}
	}
	else
	{
		if (p_readyQueue->head != -1) // if  ready queue is not empty
		{
			// reset processQuantum
			(*processQuantum) = 0;
		
			// schedule and dequeue head
			(*p_scheduledProcess) = p_processBufferStart + p_readyQueue->head;
			dequeue(p_readyQueue, p_processBufferStart, (*p_scheduledProcess));	
			#ifdef PRINTING
				printf("Scheduler: Process %d is scheduled, remaining time = %d\n", (*p_scheduledProcess)->id, (*p_scheduledProcess)->remainingTime);
			#endif
			
			// allocate memory for scheduled process and write log
			manageMemory(pMemFile, currentTime, memory, (*p_scheduledProcess));

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
			(*p_scheduledPCB)->waitingTime = (currentTime - (*p_scheduledPCB)->arrivalTime) - ((*p_scheduledPCB)->executionTime - (*p_scheduledPCB)->remainingTime);
			(*p_scheduledPCB)->priority = (*p_scheduledProcess)->priority;
			
			// enable process to read PCB
			up(PCB_sem);
			
			// write log
			writeLog(pFile, currentTime, (*p_scheduledPCB), STARTED);
		}
		else
		{
			#ifdef PRINTING
				printf("Scheduler: No process is scheduled\n");
			#endif
		}
	}
}

void writeMemLog(FILE *pFile, int currentTime, struct process* p_process, int logType)
{
	switch (logType)
	{
		case ALLOCATED:
			fprintf(pFile, "At time %d allocated %d bytes for process %d from %d to %d\n", currentTime, p_process->memSize, p_process->id, p_process->allocatedMemUnit->start, p_process->allocatedMemUnit->start + p_process->allocationSize - 1);
			break;
			
		case FREED:
			fprintf(pFile, "At time %d freed %d bytes from process %d from %d to %d\n", currentTime, p_process->memSize, p_process->id, p_process->allocatedMemUnit->start, p_process->allocatedMemUnit->start + p_process->allocationSize - 1);
			break;
	}
}

void manageMemory(FILE *pFile, int currentTime, struct memUnit *memory, struct process* p_process)
{
	bool processAllocated = allocate(memory, p_process);
		
	if (processAllocated)
	{
		writeMemLog(pFile, currentTime, p_process, ALLOCATED);
		#ifdef PRINTING
			printf("Memory Manager: Process %d allocated from %d to %d\n", p_process->id, p_process->allocatedMemUnit->start, p_process->allocatedMemUnit->start + p_process->allocationSize - 1);
		#endif
		return;
	}
	else
	{
		if (p_process->allocatedMemUnit == NULL)
		{
			#ifdef PRINTING
				printf("Memory Manager: Could do not allocate memory for process %d\n", p_process->id);
			#endif
			return;
		}
		else
		{
			splitAllocate(p_process);
			writeMemLog(pFile, currentTime, p_process, ALLOCATED);
			#ifdef PRINTING
				printf("Memory Manager: Process %d allocated from %d to %d\n", p_process->id, p_process->allocatedMemUnit->start, p_process->allocatedMemUnit->start + p_process->allocationSize - 1);
			#endif
		}
	}
}

