/**
 * Author: Taylor Freiner
 * Date: October 24th, 2017
 * Log: Fixing semaphore issues
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
#include <math.h>
#include "pcb.h"

#define BIT_COUNT 32
#define PROCESS_MAX 19
#define THRESHOLD 5000000000
#define A 2
#define B 4

struct sembuf sb;
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

void dispatch(controlBlockStruct*, int*, FILE*, int);

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
	return -1;
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
	if(sig != 1)
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
	sb.sem_op = 1;
	sb.sem_num = 0;
	sb.sem_flg = 0;
	semop(semid, &sb, 1);
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
	int processIndex = 0;
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
		if(processCount > 0){
			update(processCount, clock, controlBlock, file);
			dispatch(controlBlock, clock, file, semid);		
		}
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
					sprintf(arg, "%d", processIndex);
					execl("./user", "user", arg, NULL);
				} else {
					controlBlock[processIndex].pid = childpid;
					controlBlock[processIndex].q = 0;
					controlBlock[processIndex].startTime[0] = clock[0];
					controlBlock[i].startTime[1] = clock[1];
					schedule(childpid, controlBlock, file);
					processIds[processIndex] = childpid;
					processCount++;
					processIndex++;
				}
				if(errno){
					fprintf(stderr, "%s\n", strerror(errno));
					exit(1);
				}
			}
		}

		if(clock[0] > 100){
			clean(1);
			exit(1);
		}
	}
	sleep(10);
	
	fclose(file);

	clean(1);
	
	return 0;
}

void schedule(int pid, controlBlockStruct* controlBlock, FILE* file){
	int i;
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
			controlBlock[processNum].task = 3;
			break;
	}

	return;
}

void update(int pCount, int *clock, controlBlockStruct* controlBlock, FILE *file){
	int i, j, q;
	int averageWaitTime[2][2];
	int totalTime[3][2];
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
	
		controlBlock[i].waitTime[0] = clock[0] - controlBlock[i].startTime[0];
		controlBlock[i].waitTime[1] = clock[1] - controlBlock[i].startTime[1];
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
							totalTime[0][0] = totalTime[0][0] + controlBlock[j].waitTime[0];
							if((totalTime[0][1] + controlBlock[j].waitTime[1]) >= 1000000000){
								totalTime[0][1] = (totalTime[0][1] + controlBlock[j].waitTime[1] % 1000000000);
								totalTime[0][0]++;
							}
							else
								totalTime[0][1] = (totalTime[0][1] + controlBlock[j].waitTime[1]);
							processCount++;
						}
					}
				}else{
					break;
				}
				if(processCount > 0){
					averageWaitTime[0][0] = totalTime[0][0]/processCount;
					averageWaitTime[0][1] = totalTime[0][1]/processCount;
				}
				else{
					averageWaitTime[0][0] = 0;
					averageWaitTime[0][1] = 0;
				}
				if(((controlBlock[processNum].waitTime[0] > A * averageWaitTime[0][0]) || (controlBlock[processNum].waitTime[0] == A * averageWaitTime[0][0] &&
						controlBlock[processNum].waitTime[1] > averageWaitTime[0][1])) && (controlBlock[processNum].waitTime[0] >= 5)){
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
							totalTime[1][0] = totalTime[1][0] + controlBlock[j].waitTime[0];
							if((totalTime[1][1] + controlBlock[j].waitTime[1]) >= 1000000000){
								totalTime[1][1] = (totalTime[1][1] + controlBlock[j].waitTime[1] % 1000000000);
								totalTime[1][0]++;
							}
							else
								totalTime[1][1] = (totalTime[1][1] + controlBlock[j].waitTime[1]);
							processCount++;
						}
					}
				}else{
					break;
				}
				if(processCount > 0){
					averageWaitTime[1][0] = totalTime[1][0]/processCount;
					averageWaitTime[1][1] = totalTime[1][1]/processCount;
				}else{
					averageWaitTime[1][0] = 0;
					averageWaitTime[1][1] = 0;
				}
				if(((controlBlock[processNum].waitTime[0] > B * averageWaitTime[1][0]) || (controlBlock[processNum].waitTime[0] == B * averageWaitTime[1][0] &&
						controlBlock[processNum].waitTime[1] > averageWaitTime[0][1])) && (controlBlock[processNum].waitTime[0] >= 5)){
					fprintf(file, "OSS: Moving process with PID %d from queue 1 to queue 2 at time %d:%d\n", peek(0), clock[0], clock[1]);
					controlBlock[processNum].q = 2;
					delete(1);
					insert(2, controlBlock[processNum].pid);
				}
				break;
		}
	}
}

void dispatch(controlBlockStruct* controlBlock, int *clock, FILE* file, int semid){
	int i, j;
	bool dispatch = true;
	bool done = false;

	if(peek(0) != -1){
		for(i = 0; i < 19; i++){
			if(controlBlock[i].pid == peek(0)){
				for(j = 0; j < 19; j++){
					if(controlBlock[j].ready == true){
						dispatch = false;
						break;
					}
				}
				if(dispatch){
					controlBlock[i].ready = true;
					fprintf(file, "OSS: Dispatching process with PID %d from queue 0 at time %d:%d\n", controlBlock[i].pid, clock[0], clock[1]);
					delete(0);
					controlBlock[i].q = -1;
					sb.sem_op = -1;
					sb.sem_num = 0;
					sb.sem_flg = 0;
					semop(semid, &sb, 1);
					fprintf(file, "OSS: Receiving that process with PID %d ran for %d.%d seconds.\n", controlBlock[i].pid, 5, 4);
					break;
				}
				break;
			}
		}
	}else if(peek(1) != -1){
		for(i = 0; i < 19; i++){
			if(controlBlock[i].pid == peek(1)){
				for(j = 0; j < 19; j++){
					if(controlBlock[j].ready == true){
						dispatch = false;
						break;
					}
				}
				if(dispatch){
					controlBlock[i].ready = true;
					fprintf(file, "OSS: Dispatching process with PID %d from queue 1 at time %d:%d\n", peek(1), clock[0], clock[1]);
					delete(1);
					controlBlock[i].q = -1;
					break;
				}
				break;
			}
		}
	}else if(peek(2) != -1){
		for(i = 0; i < 19; i++){
			if(controlBlock[i].pid == peek(2)){
				for(j = 0; j < 19; j++){
					if(controlBlock[j].ready == true){
						dispatch = false;
						break;
					}
				}
				if(dispatch){
					controlBlock[i].ready = true;
					fprintf(file, "OSS: Dispatching process with PID %d from queue 2 at time %d:%d\n", peek(2), clock[0], clock[1]);
					delete(2);
					controlBlock[i].q = -1;
					break;
				}
				break;
			}
		}
	}
}
