#define HPF 0
#define SRTN 1
#define RR 2
#define SCHEDULER_SHM_KEY 500
#define SCHEDULER_SEM_KEY 600

struct process
{
	int id;	
	int arrivalTime;
	int runningTime;
	int priority;
	int remainingTime;
	struct process* next;
	struct process* prev;
};

struct schedulerInfo
{
	int schedulerType;
	int quantum;
	bool generationFinished;
};

struct readyQueue
{
	struct process *head;
	struct process *tail;
	int size;
	bool processArrival;
};

void enqueue(struct readyQueue *p_readyQueue, struct process *p_process, int schedulerType)
{	
	// corner case: empty queue
	if (p_readyQueue->size == 0)
	{
		p_readyQueue->head = p_process;
		p_readyQueue->tail = p_process;
		p_readyQueue->size++;
		return;
	}
	
	struct process *current = p_readyQueue->head;
	switch (schedulerType)
	{
		case HPF:
			// corner case: process has highest priority
			if (p_process->priority < p_readyQueue->head->priority)
			{
				p_process->next = p_readyQueue->head;
				p_readyQueue->head->prev = p_process;
				p_readyQueue->head = p_process;
				p_readyQueue->size++;
				return;
			}
			
			while (current->next != NULL)
			{
				if (p_process->priority < current->next->priority)
				{
					p_process->next = current->next;
					p_process->prev = current;
					current->next->prev = p_process;
					current->next = p_process;
					p_readyQueue->size++;
					return;
				}
				current = current->next;
			}
			
			// corner case: process has lowest priority
			current->next = p_process;
			p_process->prev = current;
			p_readyQueue->tail = p_process;
			p_readyQueue->size++;
			return;
		
		case SRTN:
			// corner case: process has lowest remaining time
			if (p_process->remainingTime < p_readyQueue->head->remainingTime)
			{
				p_process->next = p_readyQueue->head;
				p_readyQueue->head->prev = p_process;
				p_readyQueue->head = p_process;
				p_readyQueue->size++;
				return;
			}
			
			while (current->next != NULL)
			{
				if (p_process->remainingTime < current->next->remainingTime)
				{
					p_process->next = current->next;
					p_process->prev = current;
					current->next->prev = p_process;
					current->next = p_process;
					p_readyQueue->size++;
					return;
				}
				current = current->next;
			}
			
			// corner case: process has highest remaining time
			current->next = p_process;
			p_process->prev = current;
			p_readyQueue->tail = p_process;
			p_readyQueue->size++;
			return;
			
		case RR:
			while (current->next != NULL)
			{
				current = current->next;
			}
			current->next = p_process;
			p_process->prev = current;
			p_readyQueue->tail = p_process;
			p_readyQueue->size++;
			return;
	}
}
