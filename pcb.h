/**
 * Author: Taylor Freiner
 * Date: October 23rd, 2017
 * Log: Adding start time array
 */

#include <stdbool.h>

typedef struct controlBlockStruct{
	int pid;
	int cpuTime;
	int systemTime;
	int lastBurstTime;
	int processPriority;
	int startTime[2];
	int waitTime[2];
	int q;
	int p;
	int r;
	int s;
	int task;
	int quantum[2];
	bool ready;
}controlBlockStruct;
