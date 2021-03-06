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
#define MEMORY "memory.log"

// keys
#define SIM_SIZE_SHM_KEY 400
#define SCHEDULER_SHM_KEY 500
#define SCHEDULER_SEM_KEY 600
#define PCB_SEM_KEY 700

// memory log states
#define ALLOCATED 0
#define FREED 1

// smallest allocation unit
#define SMALLEST_ALLOCATION_UNIT 8

// memory unit states
#define EMPTY -1
#define NOT_EMPTY 0

struct waitingProcess
{
	struct process *p_process;
	struct waitingProcess *next;
	struct waitingProcess *prev;
};

struct waitingQueue
{
	struct waitingProcess *head;
	struct waitingProcess *tail;
};

struct memUnit
{
	int id;
	int size;
	int start;
	struct memUnit *parent;
	struct memUnit *left;
	struct memUnit *right;
};

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
	int memSize;
	int allocationSize;
	struct memUnit *allocatedMemUnit;
	int next;
	int prev;	
};

struct schedulerInfo
{
	int schedulerType;
	int quantum;
	bool generationFinished;
};

struct readyQueue
{
	int head;
	int tail;
	bool processArrival;
};

void enqueue(struct readyQueue *p_readyQueue, struct process *p_processBufferStart, struct process *p_process, int processIndex, int schedulerType)
{	
	// corner case: empty queue
	if (p_readyQueue->head == -1)
	{
		p_readyQueue->head = processIndex;
		p_readyQueue->tail = processIndex;
		p_process->next = -1;
		p_process->prev = -1;
		return;
	}
	
	struct process *p_currentProcess = p_processBufferStart + p_readyQueue->head;
	struct process *p_nextProcess = NULL;
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
			p_currentProcess = p_processBufferStart + p_readyQueue->tail;
			p_currentProcess->next = processIndex;
			p_process->prev = p_readyQueue->tail;
			p_readyQueue->tail = processIndex;
			p_process->next = -1;
			return;
	}
}


void dequeue(struct readyQueue *p_readyQueue, struct process *p_processBufferStart, struct process *p_process)
{
	struct process *p_nextProcess = NULL;
	struct process *p_prevProcess = NULL;

	// corner case: dequeue head
	if (p_process->prev == -1)
	{
		p_readyQueue->head = p_process->next;
		if (p_process->next != -1)
		{
			p_nextProcess = p_processBufferStart + p_process->next;
			p_nextProcess->prev = -1;
		}
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

bool allocate(struct memUnit *memory, struct process *p_process)
{
	// base condition
	if ((memory->left == NULL) && (memory->right == NULL))
	{
		if (memory->id != EMPTY)	// memory unit is not free
		{
			return false;
		}
		
		if (memory->size == p_process->allocationSize) // allocation possible
		{
			memory->id = p_process->id;
			p_process->allocatedMemUnit = memory;
			return true;
		}
		
		if (memory->size < p_process->allocationSize) // allocation not possible
		{
			return false;
		}
		
		if (memory->size > p_process->allocationSize) // allocation that needs splitting
		{
			// check size to get smallest memory unit greater than allocation size
		
			if (p_process->allocatedMemUnit == NULL)
			{
				p_process->allocatedMemUnit = memory;
			}
			else if (memory->size < p_process->allocatedMemUnit->size)
			{
				p_process->allocatedMemUnit = memory;
			}
			
			return false;
		}
		
	}
	
	//search in memory (left first then right)
	
	if (allocate(memory->left, p_process))
	{
		return true;
	}
	else 
	{
		return allocate(memory->right, p_process);
	}
}

void splitAllocate(struct process *p_process)
{
	// get memory unit
	struct memUnit *p_memUnit = p_process->allocatedMemUnit;
	
	while (p_memUnit->size != p_process->allocationSize)
	{
		// create left memory unit
		p_memUnit->left = (struct memUnit *) malloc(sizeof(struct memUnit));
		p_memUnit->left->id = EMPTY;
		p_memUnit->left->size = 0.5 * p_memUnit->size;
		p_memUnit->left->start = p_memUnit->start;
		p_memUnit->left->parent = p_memUnit;
		p_memUnit->left->left = NULL;
		p_memUnit->left->right = NULL;
		
		// create right memory unit
		p_memUnit->right = (struct memUnit *) malloc(sizeof(struct memUnit));
		p_memUnit->right->id = EMPTY;
		p_memUnit->right->size = 0.5 * p_memUnit->size;
		p_memUnit->right->start = p_memUnit->start + p_memUnit->right->size;
		p_memUnit->right->parent = p_memUnit;
		p_memUnit->right->left = NULL;
		p_memUnit->right->right = NULL;
		
		// mark the memory unit as not empty
		p_memUnit->id = NOT_EMPTY;
		
		// go to left memUnit
		p_memUnit = p_memUnit->left;
	}
	
	// allocate process
	p_memUnit->id = p_process->id;
	p_process->allocatedMemUnit = p_memUnit;
}

void deallocate(struct process *p_process)
{
	// get memory unit
	struct memUnit *p_memUnit = p_process->allocatedMemUnit;
	
	// deallocate memory unit
	p_memUnit->id = EMPTY;
	p_process->allocatedMemUnit = NULL;
	
	while (p_memUnit->parent != NULL)
	{
		// go to parent
		p_memUnit = p_memUnit->parent;
		
		if ((p_memUnit->left->id == -1) && (p_memUnit->right->id == -1)) // merge
		{
			// mark parent as empty
			p_memUnit->id = EMPTY;
		
			// delete left child
			free(p_memUnit->left);
			p_memUnit->left = NULL;
			
			// delete right child
			free(p_memUnit->right);
			p_memUnit->right = NULL;
		}
		else
		{
			return;
		}
	}
}

void addToWaitingList(struct waitingQueue *waitingList, struct process *p_process)
{
	struct waitingProcess *newWaitingProcess = (struct waitingProcess *) malloc(sizeof(struct waitingProcess));
	newWaitingProcess->p_process = p_process;
	
	// corner case: waiting list is empty
	if (waitingList->head == NULL)
	{		
		newWaitingProcess->prev = NULL;
		newWaitingProcess->next = NULL;
		waitingList->head = newWaitingProcess;
		waitingList->tail = newWaitingProcess;
		return;
	}
	
	// insert in the tail of the waiting list
	waitingList->tail->next = newWaitingProcess;
	newWaitingProcess->prev = waitingList->tail;
	newWaitingProcess->next = NULL;
	waitingList->tail = newWaitingProcess;
}

void removeFromWaitingList(struct waitingQueue *waitingList, struct waitingProcess *p_waitingProcess)
{
	struct waitingProcess *p_prevWaitingProcess = p_waitingProcess->prev;
	struct waitingProcess *p_nextWaitingProcess = p_waitingProcess->next;
	
	// corner case: remove head
	if (p_prevWaitingProcess == NULL)
	{
		waitingList->head = p_nextWaitingProcess;
		if (p_nextWaitingProcess != NULL)
		{
			p_nextWaitingProcess->prev = NULL;
		}
		free(p_waitingProcess);
		return;
	}
	
	// corner case: remove tail
	if (p_nextWaitingProcess == NULL)
	{
		waitingList->tail = p_prevWaitingProcess;
		p_prevWaitingProcess->next = NULL;
		free(p_waitingProcess);
		return;
	}
	
	// normal case:	
	p_prevWaitingProcess->next = p_waitingProcess->next;
	p_nextWaitingProcess->prev = p_waitingProcess->prev;
	free(p_waitingProcess);
}
