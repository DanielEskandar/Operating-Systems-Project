// scheduling algorithms
#define HPF 0
#define SRTN 1
#define RR 2

// PCB states
#define RUNNING 1
#define WAITING 0

// log states
#define STARTED 0
#define STOPPED 1
#define FINISHED 2
#define RESUMED 3

// output files
#define LOG "scheduler.log"
#define PERFORMANCE "scheduler.perf"

// keys
#define SIM_SIZE_SHM_KEY 400
#define SCHEDULER_SHM_KEY 500
#define SCHEDULER_SEM_KEY 600
#define PCB_SEM_KEY 700

struct PCB
{
	int id;
	int state;
	int arrivalTime;
	int executionTime;
	int remainingTime;
	int waitingTime;
	int priority;
};

struct process
{
	int id;	
	int arrivalTime;
	int runningTime;
	int priority;
	int remainingTime;
	int next;
	int prev;
};

struct schedulerInfo
{
	int schedulerType;
	int quantum;
	bool generationFinished;
	int scheduledProcessIndex;
};

struct readyQueue
{
	int head;
	int tail;
	bool processArrival;
};

void enqueue(struct readyQueue *p_readyQueue, struct process *p_processBufferStart, struct process *p_process, int processIndex, int scheduledProcessIndex, int schedulerType)
{	
	// corner case: empty queue
	if (p_readyQueue->head == -1)
	{
		p_readyQueue->head = processIndex;
		p_readyQueue->tail = processIndex;
		return;
	}
	
	struct process *p_nextProcess = NULL;
	struct process *p_prevProcess = NULL;
	struct process *p_scheduledProcess = NULL;
	
	struct process *p_currentProcess = p_processBufferStart + p_readyQueue->head;
	switch (schedulerType)
	{
		case HPF:
			// corner case: process has highest priority
			if (p_process->priority < p_currentProcess->priority)
			{
				p_process->next = p_readyQueue->head;
				p_currentProcess->prev = processIndex;
				p_readyQueue->head = processIndex;
				return;
			}
			
			while (p_currentProcess->next != -1)
			{
				p_nextProcess = p_processBufferStart + p_currentProcess->next;
				if (p_process->priority < p_nextProcess->priority)
				{
					p_process->next = p_currentProcess->next;
					p_process->prev = p_nextProcess->prev;
					p_currentProcess->next = processIndex;
					p_nextProcess->prev = processIndex;
					return;
				}
				p_currentProcess =  p_processBufferStart + p_currentProcess->next;
			}
			
			// corner case: process has lowest priority
			p_currentProcess->next = processIndex;
			p_process->prev = p_readyQueue->tail;
			p_readyQueue->tail = processIndex;
			return;
		
		case SRTN:
			// corner case: process has lowest remaining time
			if (p_process->remainingTime < p_currentProcess->remainingTime)
			{
				p_process->next = p_readyQueue->head;
				p_currentProcess->prev = processIndex;
				p_readyQueue->head = processIndex;
				return;
			}
			
			while (p_currentProcess->next != -1)
			{
				p_nextProcess = p_processBufferStart + p_currentProcess->next;
				if (p_process->remainingTime < p_nextProcess->remainingTime)
				{
					p_process->next = p_currentProcess->next;
					p_process->prev = p_nextProcess->prev;
					p_currentProcess->next = processIndex;
					p_nextProcess->prev = processIndex;
					return;
				}
				p_currentProcess =  p_processBufferStart + p_currentProcess->next;
			}
			
			// corner case: process has highest remaining time
			p_currentProcess->next = processIndex;
			p_process->prev = p_readyQueue->tail;
			p_readyQueue->tail = processIndex;
			return;
			
		case RR:
			p_scheduledProcess = p_processBufferStart + scheduledProcessIndex;
			
			// corner case: scheduled process is head
			if (p_scheduledProcess->prev == -1)
			{
				printf("scheduled process: %d prev: %d\n", p_scheduledProcess->id, p_scheduledProcess->prev);
				
				#ifdef DEBUGGING
					struct process *p_current = p_processBufferStart + p_readyQueue->head;
					printf("head: %d ", p_current->id);
					while (p_current->next != -1)
					{
						p_current = p_processBufferStart + p_current->next;
						printf("%d ", p_current->id);
					}
					printf(":tail\n");
				#endif
				
				p_scheduledProcess->prev = processIndex;
				p_process->next = p_readyQueue->head;
				p_readyQueue->head = processIndex;
				
				printf("scheduled process: %d prev: %d\n", p_scheduledProcess->id, p_scheduledProcess->prev);
				
				#ifdef DEBUGGING
					p_current = p_processBufferStart + p_readyQueue->head;
					printf("head: %d ", p_current->id);
					while (p_current->next != -1)
					{
						p_current = p_processBufferStart + p_current->next;
						printf("%d ", p_current->id);
					}
					printf(":tail\n");
				#endif
				
				return;
			}
			
			printf("scheduled process: %d prev: %d\n", p_scheduledProcess->id, p_scheduledProcess->prev);
			
			#ifdef DEBUGGING
				struct process *p_current = p_processBufferStart + p_readyQueue->head;
				printf("head: %d ", p_current->id);
				while (p_current->next != -1)
				{
					p_current = p_processBufferStart + p_current->next;
					printf("%d ", p_current->id);
				}
				printf(":tail\n");
			#endif
			
			p_prevProcess = p_processBufferStart + p_scheduledProcess->prev;
			p_process->next = p_prevProcess->next;
			p_process->prev = p_scheduledProcess->prev;
			p_prevProcess->next = processIndex;
			p_scheduledProcess->prev = processIndex;
			
			printf("scheduled process: %d prev: %d\n", p_scheduledProcess->id, p_scheduledProcess->prev);
			
			#ifdef DEBUGGING
				p_current = p_processBufferStart + p_readyQueue->head;
				printf("head: %d ", p_current->id);
				while (p_current->next != -1)
				{
					p_current = p_processBufferStart + p_current->next;
					printf("%d ", p_current->id);
				}
				printf(":tail\n");
			#endif
			
			return;
	}
}


void dequeue(struct readyQueue *p_readyQueue, struct process *p_processBufferStart, struct process *p_process)
{
	struct process *p_nextProcess = NULL;
	struct process *p_prevProcess = NULL;
	struct process *p_scheduledProcess = NULL;
	
	// corner case: queue size is 1
	if ((p_process->prev == -1) && (p_process->next == -1))
	{
		p_readyQueue->head = -1;
		p_readyQueue->tail = -1;
		return;
	}

	// corner case: dequeue head
	if (p_process->prev == -1)
	{
		p_readyQueue->head = p_process->next;
		p_nextProcess = p_processBufferStart + p_process->next;
		p_nextProcess->prev = -1;
		return;
	}
	
	// corner case: dequeue tail
	if (p_process->next == -1)
	{
		p_readyQueue->tail = p_process->prev;
		p_prevProcess = p_processBufferStart + p_process->prev;
		p_prevProcess->next = -1;
		return;
	}
	
	// normal case
	p_nextProcess = p_processBufferStart + p_process->next;
	p_prevProcess = p_processBufferStart + p_process->prev;	
	p_nextProcess->prev = p_process->prev;
	p_prevProcess->next = p_process->next;
	return;
}

