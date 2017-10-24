/**
 * Author: Taylor Freiner
 * Date: October 23rd, 2017
 * Log: Adding loop to check for update
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include <sys/shm.h>
#include "pcb.h"

int main(int argc, char* argv[]){
	srand(time(NULL));
	int quantumUse = rand() % 2;
	int pid = atoi(argv[1]);
	int i;
	int processIndex = -1;
	controlBlockStruct* controlBlock;
	bool ready = false;

	key_t key2 = ftok("keygen2", 1);
	int memid2 = shmget(key2, sizeof(struct controlBlockStruct) * 19, 0);
	controlBlock = (controlBlockStruct *)shmat (memid2, NULL, 0);

	while(!ready){
		for(i = 0; i < 19; i++){
			if(controlBlock[i].pid == pid && controlBlock[i].ready == true)
				ready = true;
		}
	}

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
