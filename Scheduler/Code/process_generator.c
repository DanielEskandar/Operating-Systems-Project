#include "headers.h"

// definitions
#define CLK_PROCESS "./clk.out"
#define SCHEDULER_PROCESS "./scheduler.out"

// forward declarations
void clearResources(int);
int createProcess(char *file);

int main(int argc, char * argv[])
{
    signal(SIGINT, clearResources);
    // TODO Initialization
    // 1. Read the input files.
    // 2. Ask the user for the chosen scheduling algorithm and its parameters, if there are any.
    // 3. Initiate and create the scheduler and clock processes.
    int scheduler_pid = createProcess(SCHEDULER_PROCESS);
    int clk_pid = createProcess(CLK_PROCESS);
    
    // 4. Use this function after creating the clock process to initialize clock
    initClk();
    // To get time use this
    int x = getClk();
    printf("current time is %d\n", x);
    // TODO Generation Main Loop
    // 5. Create a data structure for processes and provide it with its parameters.
    // 6. Send the information to the scheduler at the appropriate time.
    // 7. Clear clock resources
    destroyClk(true);
}

void clearResources(int signum)
{
    //TODO Clears all resources in case of interruption
    
    // clear clk shared memory
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

