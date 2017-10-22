/**
 * Author: Taylor Freiner
 * Date: October 21st, 2017
 * Log: Setting up shared memory 
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/shm.h>
#include "pcb.h"

int main(int argc, char* argv[]){
	return 0;
	srand(time(NULL));
	int quantumUse = rand() % 2;
	int pid = atoi(argv[1]);
	int i;
	int processIndex = -1;
	controlBlockStruct* controlBlock;

	key_t key2 = ftok("keygen2", 1);
	int memid2 = shmget(key2, sizeof(struct controlBlockStruct) * 19, 0);
	controlBlock = (controlBlockStruct *)shmat (memid2, NULL, 0);
	
	if(quantumUse == 1){
		for(i = 0; i < 19; i++){
			if(controlBlock[i].pid == getpid()){
				processIndex = i;
				break;
			}
		}
		if(processIndex == -1){
			fprintf(stderr, "user: PROCESS NOT FOUND\n");
			exit(1);
		}
		int quantumLength = rand() % (controlBlock[processIndex].quantum[0] * 1000000000 + controlBlock[processIndex].quantum[1] + 1);
	}

	return 0;
}
