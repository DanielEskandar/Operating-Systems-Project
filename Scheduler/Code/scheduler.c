#include "headers.h"
#include "scheduler_utilities.h"

void cleanup(int signum);

int main(int argc, char * argv[])
{
	signal(SIGINT, cleanup);

	initClk();

	//TODO implement the scheduler :)
	//upon termination release the clock resources.

	while(1) ;
}

void cleanup(int signum)
{
	//TODO Clears all resources in case of interruption

	// clear clk shared memory
	destroyClk(false);

	//terminate
	exit(0);
}
