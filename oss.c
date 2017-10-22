/**
 * Author: Taylor Freiner
 * Date: October 22nd, 2017
 * Log: Fixing queues
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <ctype.h>
#include <signal.h>
#include <unistd.h>
#include <semaphore.h>
#include <stdbool.h>
#include <time.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/types.h>
#include "pcb.h"

#define BIT_COUNT 32
#define PROCESS_MAX 19
#define A 2
#define B 4

int sharedmem[3];
int processCount = 0;
int processIds[100];
int queue0[PROCESS_MAX];
int queue1[PROCESS_MAX];
int queue2[PROCESS_MAX];
int front0 = 0;
int front1 = 0;
int front2 = 0;
int queue0Count = 0;
int queue1Count = 0;
int queue2Count = 0;
int back0 = -1;
int back1 = -1;
int back2 = -1;

union semun{
	int val;
};

void schedule(int, controlBlockStruct*, FILE*);

void update(int, int*, controlBlockStruct*, FILE*);

void dispatch(controlBlockStruct*, int*, FILE*);

bool isQueue0Full(){
	return queue0Count == 19;
}

bool isQueue0Empty(){
	return queue0Count == 0;
}

bool isQueue1Full(){
	return queue1Count == 19;
}

bool isQueue1Empty(){
	return queue1Count == 0;
}

bool isQueue2Full(){
	return queue2Count == 19;
}

bool isQueue2Empty(){
	return queue2Count == 0;
}

int peek(int q){
	switch(q){
		case 0:
			return queue0[front0];
			break;
		case 1:
			return queue1[front1];
			break;
		case 2:
			return queue2[front2];
			break;
	}
}

void insert(int q, int pid){
	switch (q){
		case 0:
			if(!isQueue0Full()){
				if(back0 == 18)
					back0 = -1;
				queue0[++back0] = pid;
				queue0Count++;
			}
			break;
		case 1:
			if(!isQueue1Full()){
				if(back1 == 18)
					back1 = -1;
				queue1[++back1] = pid;
				queue1Count++;
			}
			break;
		case 2:
			if(!isQueue2Full()){
				if(back2 == 18)
					back2 = -1;
				queue2[++back2] = pid;
				queue2Count++;
			}
			break;
	}
}

void delete(int q){
	switch(q){
		case 0:
			if(!isQueue0Empty()){
				queue0[front0++] = -1;	
				if(front0 == 19)
					front0 = 0;
				queue0Count--;
			}
			break;
		case 1:
			if(!isQueue1Empty()){
				queue1[front1++] = -1;
				if(front1 == 19)
					front1 = 0;
				queue1Count--;
			}
			break;
		case 2:
			if(!isQueue2Empty()){
				queue2[front2++] = -1;
				if(front2 == 19)
					front2 = 0;
				queue2Count--;
			}
			break;
	}
}

void setBit(int bitArray[], int i){
	bitArray[i/BIT_COUNT] |= 1 << (i % BIT_COUNT);
}

void unsetBit(int bitArray[], int i){
	bitArray[i/BIT_COUNT] &= ~(1 << (i % BIT_COUNT));
}

bool checkBit(int bitArray[], int i){
	return ((bitArray[i/BIT_COUNT] & (1 << (i % BIT_COUNT))) != 0);
}

void clean(int sig){
	fprintf(stderr, "Interrupt signaled. Removing shared memory and killing processes.\n");
	int i;
	shmctl(sharedmem[0], IPC_RMID, NULL);
	shmctl(sharedmem[1], IPC_RMID, NULL);
	semctl(sharedmem[2], 0, IPC_RMID);
	for(i = 0; i < processCount; i++){
		kill(processIds[i], SIGKILL);
	}
	exit(1);
}

int main(int argc, char* argv[]){
	union semun arg;
	arg.val = 1;
	int i;
	int bitArray[1] = { 0 };
	bool tableFull = 0;
	int processCount = 0;
	controlBlockStruct* controlBlock;

	//SIGNAL HANDLING
	signal(SIGINT, clean);

	//FILE MANAGEMENT
	FILE *file = fopen("log.txt", "w");
	
	if(file == NULL){
		printf("%s: ", argv[0]);
		perror("Error: \n");
		return 1;
	}

	//SHARED MEMORY
	key_t key = ftok("keygen", 1);
	key_t key2 = ftok("keygen2", 1);
	key_t key3 = ftok("keygen3", 1);
	int memid = shmget(key, sizeof(int*)*2, IPC_CREAT | 0644);
	int memid2 = shmget(key2, sizeof(struct controlBlockStruct) * 19, IPC_CREAT | 0644);
	int semid = semget(key3, 1, IPC_CREAT | 0644);
	if(memid == -1 || memid2 == -1){
		printf("%s: ", argv[0]);
		perror("Error: \n");
	}
	sharedmem[0] = memid;
	sharedmem[1] = memid2;
	sharedmem[2] = semid;
	int *clock = (int *)shmat(memid, NULL, 0);
	controlBlock = (struct controlBlockStruct *)shmat(memid2, NULL, 0);
	if(*clock == -1 || (int*)controlBlock == (int*)-1){
		printf("%s: ", argv[0]);
		perror("Error: \n");
	}
	int clockVal = 0;
	for(i = 0; i < 2; i++){
		memcpy(&clock[i], &clockVal, 4);
	}
	
	semctl(semid, 0, SETVAL, 1, arg);
	if(errno){
		fprintf(stderr, "%s\n", strerror(errno));
		exit(1);
	}

	//INITIALIZING QUEUES
	for(i = 0; i < 19; i++){
		queue0[i] = -1;
		queue1[i] = -1;
		queue2[i] = -1;
	}

	//CREATING PROCESSES
	int forkTime;
	int incrementTime;
	int lastForkTime[2];
	lastForkTime[0] = clock[0];
	lastForkTime[1] = clock[1];
	pid_t childpid;
	srand(time(NULL));
	forkTime = rand() % 3;
	while(1){
		incrementTime = rand() % 1000;
		clock[0] += 1;
		if(clock[1] + incrementTime >= 1000000000){
			clock[1] = (clock[1] + incrementTime) % 1000000000;
			clock[0]++;
		}
		else
			clock[1] += incrementTime;
		if(((clock[0] * 1000000000 + clock[1]) - (lastForkTime[0] * 1000000000 + lastForkTime[1]) > (forkTime * 1000000000))){
			lastForkTime[0] = clock[0];
			lastForkTime[1] = clock[1];
			forkTime = rand() % 3;
			for(i = 0; i < 19; i++){
				if(checkBit(bitArray, i) == 0){
					tableFull = 0;
					setBit(bitArray, i);
					break;
				}
				tableFull = 1;
			}
			if(!tableFull){
				childpid = fork();
				if(errno){
					fprintf(stderr, "%s", strerror(errno));
					exit(1);
				}

				fprintf(file, "OSS: Generating process with PID %d and putting it in queue 0 at time %d:%d\n", childpid, clock[0], clock[1]);
		
				if(childpid == 0){
					char arg[12];
					sprintf(arg, "%d", childpid);
					execl("./user", "user", arg, NULL);
				} else {
					controlBlock[i].pid = childpid;
					controlBlock[i].q = 0;
					controlBlock[i].waitTime[0] = clock[0];
					controlBlock[i].waitTime[1] = clock[1];
					schedule(childpid, controlBlock, file);
					processIds[i] = childpid;
					processCount++;
				}
				if(errno){
					fprintf(stderr, "%s\n", strerror(errno));
					exit(1);
				}
			}
		}
		update(processCount, clock, controlBlock, file);
		dispatch(controlBlock, clock, file);

		if(clock[0] > 100){
			clean(1);
			exit(1);
		}
	}
	sleep(10);
	fclose(file);
	shmctl(memid, IPC_RMID, NULL);
	shmctl(memid2, IPC_RMID, NULL);
	semctl(semid, 0, IPC_RMID);



	for(i = 0; i < processCount; i++)
		kill(processIds[i], SIGKILL);

	return 0;
}

void schedule(int pid, controlBlockStruct* controlBlock, FILE* file){
	int i;
	int waitTime[2];
	int quantum[2];
	quantum[0] = 2;
	quantum[1] = 5000;
	insert(0, pid);
	int randNum = rand() % 4;
	int r = rand() % 6;
	int s = rand() % 1001;
	int p = rand() % 99 + 1;
	int processNum = -1;
	for(i = 0; i < 19; i++){
		if(controlBlock[i].pid == pid){
			processNum = i;
			break;
		}
	}
	if(processNum == -1){
		fprintf(stderr, "schedule: PROCESS NOT FOUND IN CONTROL BLOCK\n");
		clean(1);
		exit(1);
	}

	switch(randNum){
		case 0:
			controlBlock[processNum].task = 0;
			return;
			break;
		case 1:
			controlBlock[processNum].quantum[0] = quantum[0];
			controlBlock[processNum].quantum[1] = quantum[1];
			controlBlock[processNum].task = 1;
			break;
		case 2:
			controlBlock[processNum].r = r;
			controlBlock[processNum].s = s;
			controlBlock[processNum].task = 2;
			break;
		case 3:
			controlBlock[processNum].p = p;
			controlBlock[processNum].quantum[0] = quantum[0];
			controlBlock[processNum].quantum[1] = quantum[1];
			break;
	}

	return;
}

void update(int pCount, int *clock, controlBlockStruct* controlBlock, FILE *file){
	int i, j, q;
	int averageWaitTime = 0;
	int totalTime = 0;
	int processIds[19];
	int processCount = 0;
	int processNum = -1;
	
//	for(i = 0; i < 19; i++){
//		if(controlBlock[i].pid == pid){
//			processNum = i;
//			break;
//		}
//	}
//	if(processNum == -1){
//		fprintf(stderr, "isReady: PROCESS NOT FOUND IN CONTROL BLOCK\n");
//		clean(1);
//		exit(1);
//	}

	for(i = 0; i < pCount; i++){
		q = controlBlock[i].q;
		controlBlock[i].waitTime[0] = clock[0] - controlBlock[i].waitTime[0];
		controlBlock[i].waitTime[1] = clock[1] - controlBlock[i].waitTime[1];
		processNum = i;
		switch(q){
			case 0:
				if(peek(0) != -1){
					processIds[i] = peek(0);
					for(j = 0; j < pCount; j++){
						//if(controlBlock[j].pid == processIds[i]){
						//	totalTime = totalTime + controlBlock[processNum].waitTime[0] + controlBlock[processNum].waitTime[1];
						//	processCount++;
						//	break;
						//}
						if(controlBlock[j].q == 1){
							printf("\n\n\nHERE\n\n\n\n");
							totalTime = totalTime + controlBlock[j].waitTime[0] + controlBlock[j].waitTime[1];
							processCount++;
						}
						else
							break;
					}
				}else{
					break;
				}
				if(processCount > 0)
					averageWaitTime = totalTime/processCount;
				else
					averageWaitTime = 0;
				printf("WAIT TIME: %d.%d\n\n\n\n", controlBlock[processNum].waitTime[0], controlBlock[processNum].waitTime[1]);
				if((controlBlock[processNum].waitTime[0] + controlBlock[processNum].waitTime[1]) > A * averageWaitTime){
					fprintf(file, "OSS: Moving process with PID %d from queue 0 to queue 1 at time %d:%d\n", peek(0), clock[0], clock[1]);	
					controlBlock[processNum].q = 1;
					delete(0);
					insert(1, controlBlock[processNum].pid);
				}
			break;
	
			case 1:
				if(peek(1) != -1){
					processIds[i] = peek(1);
					for(j = 0; j < pCount; j++){
						//if(controlBlock[j].pid == processIds[i]){
						//	totalTime = totalTime + controlBlock[processNum].waitTime[0] + controlBlock[processNum].waitTime[1];
						//	processCount++;
						//	break;
						//}
						if(controlBlock[j].q == 2){
							totalTime = totalTime + controlBlock[j].waitTime[0] + controlBlock[j].waitTime[1];
							processCount++;
						}
						else
							break;
					}
				}else{
					break;
				}
				if(processCount > 0)
					averageWaitTime = totalTime/processCount;
				else
					averageWaitTime = 0;
				if((controlBlock[processNum].waitTime[0] + controlBlock[processNum].waitTime[1]) > B * averageWaitTime){
					controlBlock[processNum].q = 2;
					delete(1);
					insert(2, controlBlock[processNum].pid);
				}
			break;
		}
		return;
	}
}

void dispatch(controlBlockStruct* controlBlock, int *clock, FILE* file){
	int i;
	if(peek(0) != -1){
		for(i = 0; i < 19; i++){
			if(controlBlock[i].pid == peek(0)){
				controlBlock[i].ready = true;
				fprintf(file, "OSS: Dispatching process with PID %d from queue 0 at time %d:%d\n", peek(0), clock[0], clock[1]);
				delete(0);
				controlBlock[i].q = -1;
				break;
			}
		}
	}else if(peek(1) != -1){
		for(i = 0; i < 19; i++){
			if(controlBlock[i].pid == peek(1)){
				controlBlock[i].ready = true;
				fprintf(file, "OSS: Dispatching process with PID %d from queue 1 at time %d:%d\n", peek(1), clock[0], clock[1]);
				delete(1);
				controlBlock[i].q = -1;
				break;
			}
		}
	}else if(peek(2) != -1){
		for(i = 0; i < 19; i++){
			if(controlBlock[i].pid == peek(2)){
				controlBlock[i].ready = true;
				fprintf(file, "OSS: Dispatching process with PID %d from queue 2 at time %d:%d\n", peek(2), clock[0], clock[1]);
				delete(2);
				controlBlock[i].q = -1;
				break;
			}
		}
	}
}
