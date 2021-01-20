# Operating System Project

A simulation of an operating system with scheduler, a memory manager, and a synchronizer.

* The scheduler can perform 3 scheduling algorithms:
	- Highest Priority First (HPF)
	- Shortest Remaining Time Next (SRTN)
	- Round Robin (RR)
* The memory manager using the Buddy alocation system to allocated processes in a 1024 Bytes memory.
* The synchronizer solves the famous producer-consumer problem.

---

## Set Up

The following bash commands are used to set up and run the project.

To compile the project:

> make all

To generate a test case:

> ./test_generator.out

To run the project on the generated test case:

> make run

---