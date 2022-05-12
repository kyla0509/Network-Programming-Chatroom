/*
 * main_client.c
 * adapted from chat_client_full.c
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h> 
#include <pthread.h>

#define PORT_NUM 1030
#define DATA_SIZE 1024
#define NUM_ROOMS 5
#define MAX_PPL 32

int current_rooms = 0;

void error(const char *msg)
{
	perror(msg);
	exit(0);
}

typedef struct _ThreadArgs {
	int clisockfd;
} ThreadArgs;


void* thread_main_recv(void* args)
{
	pthread_detach(pthread_self());

	int sockfd = ((ThreadArgs*) args)->clisockfd;
	free(args);

	// keep receiving and displaying message from server
	char buffer[512];
	int n;
	memset(buffer, 0, 512);
	n = recv(sockfd, buffer, 512, 0);
	while (n > 0) {
		printf("%s\n", buffer);
		memset(buffer, 0, 512);
		
		n = recv(sockfd, buffer, 512, 0);
		if (n < 0) error("ERROR recv() failed");
		
	}

	return NULL;
}

void send_file(FILE *fp, int sockfd){
	char data[DATA_SIZE]= {0};
	while(fgets(data,DATA_SIZE,fp)!=NULL){
		if(send(sockfd, data, sizeof(data),0)==-1){
			perror("Error in sending data");
			exit(1);
		}
		bzero(data,DATA_SIZE);
	}
}

void* thread_main_send(void* args)
{
	pthread_detach(pthread_self());

	int sockfd = ((ThreadArgs*) args)->clisockfd;
	free(args);

	// keep sending messages to the server
	char buffer[256]; //max characters per msg
	int n;
	int username_set = 0; // check if username is set or not

	while (1) {
		// You will need a bit of control on your terminal
		// console or GUI to have a nice input window.

		if(username_set==0)
		{
			printf("\nChoose a username of less than 16 characters\n");
		}
		memset(buffer, 0, 256);
		fgets(buffer, 255, stdin);
		if (strlen(buffer) == 1) {
			sprintf(buffer,"%s","\0");
		}
		if (strlen(buffer)==0){
			sprintf(buffer,"%s","\0");
			 buffer[1] = '\0';
		}
		if (buffer[strlen(buffer)-1] == 10) // gets rid of \n
		{
			buffer[strlen(buffer)-1] = '\0';
		}

		if(!username_set && strlen(buffer) > 12) {
			continue;
		} else {
			username_set = 1; // username is set
		}
		if(strcmp(buffer,"SEND")==0){
			//char* filename="test.txt";
			//FILE *fp = fopen(filename,"r");
			//send_file(fp,sockfd);
		}else{
		n = send(sockfd, buffer, strlen(buffer), 0);
		}
		if (n < 0) error("ERROR writing to socket");

		if (n == 0) break; // break after empty string
	}
	return NULL;
}

int main(int argc, char *argv[])
{
	if (argc < 2) error("Please specify hostname");

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) error("ERROR opening socket");
	char* room_client_input = "1";
	struct sockaddr_in serv_addr;
	socklen_t slen = sizeof(serv_addr);
	memset((char*) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
	serv_addr.sin_port = htons(PORT_NUM);
	
	if(argc>2){
		room_client_input = argv[2];
	}else{
		room_client_input = "x";
	}
	

	printf("Try connecting to %s...\n", inet_ntoa(serv_addr.sin_addr));

	int status = connect(sockfd, 
			(struct sockaddr *) &serv_addr, slen);
	if (status < 0) error("ERROR connecting");

	char buffer[256];
	char roombuf[256];
	
	sprintf(roombuf,"%s",room_client_input);
	if(strcmp(room_client_input,"x")!=0){
		send(sockfd,roombuf,strlen(roombuf),0);
		
	}
	// clear buffer
	memset(buffer, 0, 256);
	if(strcmp(room_client_input,"x")==0){
		send(sockfd,roombuf,strlen(roombuf),0);
		int nrcv = recv(sockfd, buffer, 255, 0);
		printf("%s \nPlease select the room\n",buffer);
		memset(buffer,0,256);
		memset(roombuf,0,256);
		fgets(roombuf, 255, stdin);
		send(sockfd, roombuf, strlen(roombuf), 0);
		memset(roombuf,0,256);
	}
	
	pthread_t tid1;
	pthread_t tid2;

	ThreadArgs* args;
	
	args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
	args->clisockfd = sockfd;
	pthread_create(&tid1, NULL, thread_main_send, (void*) args);

	args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
	args->clisockfd = sockfd;
	pthread_create(&tid2, NULL, thread_main_recv, (void*) args);

	// parent will wait for sender to finish (= user stop sending message and disconnect from server)
	pthread_join(tid1, NULL);
	pthread_join(tid2, NULL);

	close(sockfd);

	return 0;
}