#define HPF 0
#define SRTN 1
#define RR 2
#define SCHEDULER_KEY 500

struct process
{
	int id;
	int arrivalTime;
	int runningTime;
	int priority;
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

