/**
 * Author: Taylor Freiner
 * Date: October 24th, 2017
 * Log: Fixing semaphore issues 
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include <semaphore.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include "pcb.h"

int main(int argc, char* argv[]){
	struct sembuf sb;
	srand(time(NULL));
	int quantumUse = rand() % 2;
	int quantumLength[2];
	int index = atoi(argv[1]);
	int i;
	int processIndex = -1;
	controlBlockStruct* controlBlock;
	bool ready = false;

	key_t key = ftok("keygen", 1);
	key_t key2 = ftok("keygen2", 1);
	key_t key3 = ftok("keygen3", 1);

	int memid = shmget(key, sizeof(int*)*2, IPC_CREAT | 0644);
	int memid2 = shmget(key2, sizeof(struct controlBlockStruct) * 19, 0);
	int semid = semget(key3, 1, 0);

	int *clock = (int *)shmat(memid, NULL, 0);	
	controlBlock = (controlBlockStruct *)shmat (memid2, NULL, 0);

	while(!ready){
		if(controlBlock[index].ready){
			ready = true;
		}
	}
	if(quantumUse == 1){
		quantumLength[0] = rand() % controlBlock[processIndex].quantum[0] + 1;
		quantumLength[1] = rand() % controlBlock[processIndex].quantum[1] + 1;
	} else {
		quantumLength[0] = controlBlock[processIndex].quantum[0];
		quantumLength[1] = controlBlock[processIndex].quantum[1];
	}
	controlBlock[index].ready = false;

	sb.sem_op = 1;
	sb.sem_num = 0;
	sb.sem_flg = 0;
	semop(semid, &sb, 1);

	return 0;
}
