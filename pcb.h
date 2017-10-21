/**
 * Author: Taylor Freiner
 * Date: October 21st, 2017
 * Log: Moving stucture to header file
 */

typedef struct controlBlockStruct{
	int pid;
	int cpuTime;
	int systemTime;
	int lastBurstTime;
	int processPriority;
	int waitTime[2];
	int q;
	int p;
	int task;
	int quantum[2];
}controlBlockStruct;
