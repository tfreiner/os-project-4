/**
 * Author: Taylor Freiner
 * Date: October 21st, 2017
 * Log: Adding to scheduler
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
int front0, back0, front1, back1, front2, back2 = -1;

union semun{
	int val;
};

void insert(int q, int pid){
	if((q == 0 && front0 == 18) || (q == 1 && front1 == 18) || (q == 2 && front2 == 18)){
		fprintf(stderr, "Queue is full.\n");
		return;
	}
	else{
		switch (q){
			case 0:
				if(q == 0 && front0 == -1)
					front0 = 0;
				back0++;
				queue0[back0] = pid;
				break;
			case 1:
				if(q == 1 && front1 == -1)
					front1 = 0;
				back1++;
				queue1[back1] = pid;
				break;
			case 2:
				if(q == 2 && front2 == -1)
					front2 = 0;
				back2++;
				queue2[back2] = pid;
				break;
		}
	}
}

void delete(int q, int pid){
	if((q == 0 && (front0 == -1 || front0 > back0)) || (q == 1 && (front1 == -1 || front1 > back1)) || (q == 2 && (front2 == -1 || front2 > back2))){
		fprintf(stderr, "Queue is empty.\n");
		return;
	}
	else{
		switch(q){
			case 0:
				front0++;
				break;
			case 1:
				front1++;
				break;
			case 2:
				front2++;
				break;
		}
	}
}

int schedule(int, controlBlockStruct*, int*);

bool isReady(int, int, controlBlockStruct*);
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

int main(int argc, char* argv[])  {

	union semun arg;
	arg.val = 1;
	int i;
	int bitArray[1] = { 0 };
	bool tableFull = 0;
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
		perror("Error\n");
	}
	sharedmem[0] = memid;
	sharedmem[1] = memid2;
	sharedmem[2] = semid;
	int *clock = (int *)shmat(memid, NULL, 0);
	controlBlock = (controlBlockStruct *)shmat(memid2, NULL, 0);
	if(*clock == -1 || (int*)controlBlock == (int*)-1){
		printf("%s: ", argv[0]);
		perror("Error\n");
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
	while(!tableFull){
		incrementTime = rand() % 1000;
		printf("lastForkTime: %d\n", lastForkTime[0] + lastForkTime[1]);
		printf("forkTime: %d\n", forkTime);
		printf("incrementTime: %d\n", incrementTime);
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
				printf("TABLE NOT FULL\n");
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
					insert(0, childpid);
					int num = schedule(childpid, controlBlock, clock);
				}
				if(errno){
					fprintf(stderr, "%s\n", strerror(errno));
					exit(1);
				}
				//if(num == 1)
				//	printf("END PROCESS\n\n\n");
				//else{
				//	processIds[i] = childpid;
				//	processCount++;
				//}
			}
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

int schedule(int pid, controlBlockStruct* controlBlock, int *clock){
	printf("SCHEDULE\n");
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
			return 1;
			break;
		case 1:
			controlBlock[processNum].quantum[0] = quantum[0];
			controlBlock[processNum].quantum[1] = quantum[1];
			controlBlock[processNum].task = 1;
			break;
		case 2:
			waitTime[0] = r;
			waitTime[1] = s;
			controlBlock[processNum].waitTime[0] = waitTime[0];
			controlBlock[processNum].waitTime[1] = waitTime[1];
			controlBlock[processNum].task = 2;
			isReady(pid, 0, controlBlock);
			break;
		case 3:
			controlBlock[processNum].p = p;
			controlBlock[processNum].quantum[0] = quantum[0];
			controlBlock[processNum].quantum[1] = quantum[1];
			break;
	}

	return 0;
}

bool isReady(int pid, int q, controlBlockStruct* controlBlock){
	printf("IS READY: PID: %d\n", pid);	
	int i, j;
	int averageWaitTime = 0;
	int totalTime = 0;
	int processCount = 0;
	int processIds[19];
	int processNum = -1;
	
	for(i = 0; i < 19; i++){
		if(controlBlock[i].pid == pid){
			processNum = i;
			printf("PROCESS NUM----------: %d", processNum);
			break;
		}
	}
	if(processNum == -1){
		fprintf(stderr, "isReady: PROCESS NOT FOUND IN CONTROL BLOCK\n");
		clean(1);
		exit(1);
	}

	switch(q){
		case 0:
			for(i = 0; i < 19; i++){
				if(queue0[i] != -1){
					processIds[i] = queue0[i];
					printf("ID IN Q: %d\n", queue0[i]);
					for(j = 0; j < 19; j++){
						if(controlBlock[j].pid == processIds[i]){
							totalTime = totalTime + controlBlock[processNum].waitTime[0] + controlBlock[processNum].waitTime[1];
							processCount++;
							break;
						}
					}
				}else{
					break;
				}
			}
			printf("TOTAL TIME: %d\n", totalTime);
			printf("PROCESS COUNT: %d\n", processCount);
			averageWaitTime = totalTime/processCount;
			return 0;
			if((controlBlock[processNum].waitTime[0] + controlBlock[processNum].waitTime[1]) > A * averageWaitTime)
				return true;
			else
				return false;
		break;
	
		case 1:
			for(i = 0; i < 19; i++){
				if(queue1[i] != -1){
					processIds[i] = queue1[i];
					for(j = 0; j < 19; j++){
						if(controlBlock[j].pid == processIds[i]){
							totalTime = totalTime + controlBlock[processNum].waitTime[0] + controlBlock[processNum].waitTime[1];
							processCount++;
							break;
						}
					}
				}else{
					break;
				}
			}
			averageWaitTime = totalTime/processCount;
			if((controlBlock[processNum].waitTime[0] + controlBlock[processNum].waitTime[1]) > B * averageWaitTime)
				return true;
			else
				return false;
		break;
	}

	return 1;
}
