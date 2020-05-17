#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <semaphore.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

//TERMINAL COLORS
#define RED   "\x1B[31m"
#define GRN   "\x1B[32m"
#define YEL   "\x1B[33m"
#define BLU   "\x1B[34m"
#define MAG   "\x1B[35m"
#define CYN   "\x1B[36m"
#define WHT   "\x1B[37m"
#define RESET "\x1B[0m"

/*
Code Logic: This piece of code reads 3 files for a garage implementation which has (Requests,Repairs,Resources).
			For each car entering the garage there's an array of repairs to be made, to each of the array's repairs there's a corresponding thread.
			For each repair there's an array of resources needed for it. so for each resource there's a thread which fetches that resource using a semaphore holding #avialable resources.
			the logic can be seen as a tree.
			the code is built like the tree below, from top to lower leafs

						2594010(carID)                (request level)
						/      |      \  <--------thread level 1>
				(oil-change)   (tire)  (brake repair)
					 1         3        5               (repair level)
					/ \      / | \     /  \ <------------threads level 2>
				  10  13    13 35 17  13  35           (resources level)
				  ||||||||||||||||||||||||||
		-all the threads on this level check semaphores-


*/
typedef struct Requests { // holds an instance of a car request from file

	int serialNumber, numOfRequests;
	int time;
	int* requests;

}requests;


typedef struct Repairs {// holds an instance of a repair for a car request

	int ID, hoursNeeded, numOfRes;
	char* name;
	int* resources;

}repairs;

typedef struct Resources { // holds an instance of a resource for a repair

	int resID;
	char* name;
	int garageQuantity;
	sem_t thisResourceSem; // each resource has a semaphore with its quantity

}resources;

typedef struct CarInGarage {
	int serialNum;
	int currentRequest;

}CarInGarage;


requests* readReq(char*);
resources* readRes(char*);
repairs* readRep(char*);
void printError(char*);
char* retrieveData(int);
void* timerF();
void* garage(void*);
void* repairPlatform(void*);
void* waitTillAvailable(void*);
void releaseResources(int*, int);
int readLines(char*);
void freeMemory();

int timer = 0, amountOfCars, amountOfResources = 0, amountOfRepairs = 0, Day = 1;
sem_t* servicesOfGarage;
pthread_mutex_t lock;
requests* req;
repairs* rep;
resources* res;

int main(int argc, char** argv) {
	int i;

	if (argc != 4) { printError("Please enter 3 files by this order: requests.txt,repairs.txt,resources.txt"); }

	req = readReq(argv[1]); // reads the requests file
	rep = readRep(argv[2]); // reads the repairs file
	res = readRes(argv[3]); // reads the resources file
	pthread_t requestsThreads[amountOfCars], timeThread; // level 1 threads + timer thread

	pthread_mutex_init(&lock, NULL);
	pthread_create(&timeThread, NULL, timerF, NULL); // starts the timer with thread

	for (i = 0; i < amountOfResources; i++) { // initializes the semaphores in the resources to their quantity
		sem_init(&res[i].thisResourceSem, 0, res[i].garageQuantity);
	}

	for (i = 0; i < amountOfCars; i++) { // level 1 threads sending into next job
		if (pthread_create(&requestsThreads[i], NULL, garage, &req[i])) { printError("thread-creation"); }
	}

	for (i = 0; i < amountOfCars; i++) { // waiting for all cars to finish their repairs
		pthread_join(requestsThreads[i], NULL);
	}

	freeMemory();

	printf(RED"\n\n**Program finished simulation**\n\n"RESET);
	exit(0);
	pthread_join(timeThread, NULL);

	return 0;
}

void* garage(void* request) { // function recieving the level 1 threads, and send them to the platforms for the repairs
	int i;
	CarInGarage info; // holds neccessery information to pass with threads
	requests* car;
	car = (requests*)request;
	pthread_t repairsThreads[car->numOfRequests]; // initializes repairs threads

	while (1) {
		if (car->time == timer) break; // waits for the car's arrival time.
	}

	printf(YEL "[%d:00] [Day:%d]"BLU"{%d}" RESET" car has " CYN "ENTERED" RESET " the GARAGE\n", timer, Day, car->serialNumber);


	info.serialNum = car->serialNumber;
	for (i = 0; i < car->numOfRequests; i++) { //sends the request by its order from the file
		info.currentRequest = car->requests[i];
		pthread_create(&repairsThreads[i], NULL, repairPlatform, &info);
		pthread_join(repairsThreads[i], NULL);  // doesn't go to the next repair untill first one is finished

	}

	printf(YEL "[%d:00] [Day:%d]"BLU" {%d} car has "RED "FINISHED"RESET" ALL the repairs and left the GARAGE\n", timer, Day, car->serialNumber);

	return NULL;
}

void* repairPlatform(void* info) { // function recieving level 2 threads, and perform their repair if resources are available;
	int serviceID, serialNum, i, j;
	int* resourcesToRelease;
	unsigned int timeForRep;
	pthread_t* resourcesThreads;
	char* nameOfRepair;

	serviceID = ((CarInGarage*)info)->currentRequest;
	serialNum = ((CarInGarage*)info)->serialNum;

	for (i = 0; i < amountOfRepairs; i++) { // searches for the specific repair information
		if (rep[i].ID == serviceID) {
			nameOfRepair = rep[i].name;
			timeForRep = (unsigned int)rep[i].hoursNeeded;
			break;
		}
	}

	resourcesThreads = (pthread_t*)malloc(sizeof(pthread_t) * rep[i].numOfRes); // allocates size for amount of resources needed
	resourcesToRelease = (int*)malloc(sizeof(int) * rep[i].numOfRes);			 // allocates array of resources to release later

	pthread_mutex_lock(&lock); // mutex lock is used to keep threads taking resources of other threads while the other thread didn't finish fetching all its resources on time, this causes a deadlock

	for (j = 0; j < rep[i].numOfRes; j++) { 						// creates threads to check if resources are available on the last level + saves the resources for release
		pthread_create(&resourcesThreads[j], NULL, waitTillAvailable, &rep[i].resources[j]);
		resourcesToRelease[j] = rep[i].resources[j];
	}


	for (j = 0; j < rep[i].numOfRes; j++) {
		pthread_join(resourcesThreads[j], NULL); // this keeps the repair from performing before ALL resources are fetched
	}

	pthread_mutex_unlock(&lock); // unlocks so other threads can go fetch resources

	printf(YEL "[%d:00] [Day:%d]"BLU" {%d} car has " GRN "STARTED " RESET " repair:-%s-\n", timer, Day, serialNum, nameOfRepair);
	sleep(timeForRep);			// sleep for the time needed for the repair
	releaseResources(resourcesToRelease, rep[i].numOfRes); // releases all resources after finishing the repair
	printf(YEL "[%d:00] [Day:%d]"BLU " {%d} car has "MAG"FINISHED "RESET"repair:-%s-\n", timer, Day, serialNum, nameOfRepair);

	free(resourcesThreads);
	free(resourcesToRelease);

	return NULL;

}

void* waitTillAvailable(void* resourceID) { // threads fetch here their resources or wait for them if not avilable
	int resID, i;
	resID = *(int*)resourceID;

	for (i = 0; i < amountOfResources; i++) {
		if (res[i].resID == resID) {
			sem_wait(&res[i].thisResourceSem);
			break;
		}
	}
	return NULL;
}

void releaseResources(int* resourcesToPost, int size) { // threads releasing resources here after repair is finished
	int i, j;

	for (j = 0; j < size; j++) {
		for (i = 0; i < amountOfResources; i++) {
			if (res[i].resID == resourcesToPost[j]) {
				sem_post(&res[i].thisResourceSem);
			}
		}
	}
}

resources* readRes(char* fileName) { // function reading the resources file using linux system calls
	int fd, i = 0;
	char tempBuffer[256];
	resources* temp;
	int size = readLines(fileName);

	if ((fd = open(fileName, O_RDONLY)) == -1) { printError("read error"); }
	temp = (resources*)malloc(sizeof(resources) * size);

	while (i < size) {

		temp[i].resID = atoi(retrieveData(fd));
		strcpy(tempBuffer, retrieveData(fd));
		temp[i].name = (char*)malloc(sizeof(tempBuffer) + 1);
		strcpy(temp[i].name, tempBuffer);
		temp[i].garageQuantity = atoi(retrieveData(fd));
		amountOfResources++;
		i++;

	}
	return temp;

}

requests* readReq(char* fileName) { // function reading the requests file using linux system call
	int fd, i = 0, k;
	requests* temp;
	amountOfCars = readLines(fileName);

	if ((fd = open(fileName, O_RDONLY)) == -1) { printError("read error"); }
	temp = (requests*)malloc(sizeof(requests) * amountOfCars);

	while (i < amountOfCars) {

		temp[i].serialNumber = atoi(retrieveData(fd));
		temp[i].time = atoi(retrieveData(fd));
		temp[i].numOfRequests = atoi(retrieveData(fd));
		temp[i].requests = (int*)malloc(sizeof(int) * temp[i].numOfRequests);
		for (k = 0; k < temp[i].numOfRequests; k++) {
			temp[i].requests[k] = atoi(retrieveData(fd));
		}
		i++;
	}
	return temp;
}

repairs* readRep(char* fileName) { // function reading the repairs file using linux system call
	int fd, i = 0, k;
	repairs* temp;
	int size = readLines(fileName);
	char tempBuffer[256];
	if ((fd = open(fileName, O_RDONLY)) == -1) { printError("read error"); }
	temp = (repairs*)malloc(sizeof(repairs) * size);

	while (i < size) {

		temp[i].ID = atoi(retrieveData(fd));
		strcpy(tempBuffer, retrieveData(fd));
		temp[i].name = (char*)malloc(sizeof(tempBuffer) + 1);
		strcpy(temp[i].name, tempBuffer);
		temp[i].hoursNeeded = atoi(retrieveData(fd));
		temp[i].numOfRes = atoi(retrieveData(fd));

		temp[i].resources = (int*)malloc(sizeof(int) * temp[i].numOfRes);
		for (k = 0; k < temp[i].numOfRes; k++) {
			temp[i].resources[k] = atoi(retrieveData(fd));
		}
		amountOfRepairs++;
		i++;
	}
	return temp;
}

void printError(char* msg) {
	perror(msg);
	exit(1);
}

void* timerF() {
	while (1) {
		if (timer == 24) {
			timer = 0;
			Day++;
		}
		sleep(1);
		timer++;
	}
}

char* retrieveData(int fd) { // fetches data from files and generates a string
	char byteData[2];
	char* buffer = (char*)malloc(255);
	strcpy(buffer, "");

	read(fd, byteData, 1);

	do {
		if (byteData[0] != '\n' && byteData[0] != '\t') { strncat(buffer, byteData, 1); }
		read(fd, byteData, 1);
	} while (byteData[0] != '\t' && byteData[0] != '\n');

	return buffer;
}

int readLines(char* file) { // reads the amount of lines(inputs) from a file
	int fd, lines = 0;
	char buffer;
	if ((fd = open(file, O_RDONLY)) == -1) { printError(file); }

	while (read(fd, &buffer, 1)) {
		if (buffer == '\n')lines++;
	}
	return lines - 1;
}

void freeMemory() {
	int i;
	for (i = 0; i < amountOfCars; i++) {
		free(req[i].requests);
	}
	free(req);
	for (i = 0; i < amountOfRepairs; i++) {
		free(rep[i].name);
		free(rep[i].resources);
	}
	free(rep);
	for (i = 0; i < amountOfResources; i++) {
		free(res[i].name);
	}
	free(res);
}