/*
 * main_server.c
 * adapted from chat_server_full.c
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <ctype.h>

#define PORT_NUM 1030
#define DATA_SIZE 1024
#define NUM_ROOMS 5
#define MAX_PPL 7

int room_list[NUM_ROOMS][MAX_PPL]; // list of rooms + num of ppl in them
int current_rooms; // # of rooms open

// error handling
void error(const char *msg)
{
	perror(msg);
	exit(1);
}

// struct to help create a user
typedef struct client_user {
	int clisockfd; // socket file descriptor
	int room;
    char* username; //  user's chosen name
    char* color; // what color user has
	struct client_user* next; // used in linked list of users
} client_user;

// used for linked list
client_user *head = NULL;
client_user *tail = NULL;

char* unique_color() {
	/* color codes from https://gist.github.com/JBlond/2fea43a3049b38287e5e9cefc87b2124:
		Black, Red, Green, Yellow,  Blue, Purple, Cyan, White
	*/
	char* color = "black";
	char* options[MAX_PPL] = {"\x1B[30m", "\x1B[31m", "\x1B[32m", "\x1B[33m", "\x1B[34m", "\x1B[35m", "\x1B[36m"}; 

	int n = rand() % 7;
	int is_unique = 0;
	char* current_color;
	client_user* current = head;

	// randomly generate between 0 and MAX_PPL for unique colorrs
	while (is_unique = 0) {		
		int n = rand() % MAX_PPL; 	
		current_color = current->color;

		while(current != NULL) {		
			if (options[n] == current_color) {
				is_unique = 1;
			}
		}
	}
	return options[n];
}

// prints the users currently connected
void print_clients() {
    client_user* current = head;
    printf("The following users are connected: \n");
	if(head == NULL) {
		printf("No users are connected \n");
	} else {
		while(current != NULL) {
			printf("User: [%s] - [1]\n", current->username);
			current = current->next;
		}
	}
    return;
}

// add new user, goes to end of linked list of users
void add_user(int newclisockfd, char* name_new, char* color_new, int roomparam) 
{
	if (head == NULL) 
    {
		head = (client_user*) malloc(sizeof(client_user));
		head->clisockfd = newclisockfd;

        head->username = (char*)malloc(strlen(name_new)* sizeof(char)); //malloc bc string
        strcpy(head->username, name_new);

        head->color = (char*)malloc(strlen(color_new)* sizeof(char)); //malloc bc string
        strcpy(head->color, color_new);
		head->room = roomparam;

		head->next = NULL;
		tail = head;

	} else {
		tail->next = (client_user*) malloc(sizeof(client_user));
		tail->next->clisockfd = newclisockfd;

        tail->next->username = (char*)malloc(strlen(name_new)* sizeof(char)); //malloc bc string
        strcpy(tail->next->username, name_new);

        tail->next->color = (char*)malloc(strlen(color_new)* sizeof(char)); //malloc bc string
        strcpy(tail->next->color, color_new);
		tail->room = roomparam;

		tail->next->next = NULL;
		tail = tail->next;
	}
		for(int y = 0; y < MAX_PPL;y++){
			if(room_list[roomparam][y]==-1){
				room_list[roomparam][y]=newclisockfd;
				break;
			}
		}
}

void write_file(int sockfd,char *filename){
	int n;
	FILE *fp;
	char filebuffer[DATA_SIZE];
	fp = fopen(filename,"w");
	if(fp==NULL){
		perror("Error in creating file");
		exit(1);
	}

	while (1){
		n = recv(sockfd,filebuffer,DATA_SIZE,0);
		if(n<=0){
			break;
			return;
		}
		fprintf(fp,"%s",filebuffer);
		bzero(filebuffer,DATA_SIZE);
	}
	return;
}

// send message from one user's client to another user's client 
void send_message(int fromfd, char* message) {
	struct sockaddr_in cliaddr;
	socklen_t clen = sizeof(cliaddr);
	if (getpeername(fromfd, (struct sockaddr*)&cliaddr, &clen) < 0) error("ERROR Unknown sender!");

    // check for username & color
    char* username;
    char* color;

	// iterate thru all connected clients to check for which user sent message
	client_user* current = head;
    while (current != NULL) {
		// check if current user is who sent messsge
		if (current->clisockfd == fromfd) {
			username = current->username;
            color = current->color;
			break;
		}
		current = current->next;
	}
	current = head;

	/* iterate thru all connected clients to check for which user sent message
	*  and check if current is not the one who sent message + see if in the same room
	*  For chat rooms, make a list of fds in a chatroom
	*/
	int roomNum;
	for(int i = 0; i < NUM_ROOMS;i++){
		for(int j = 0; j < MAX_PPL; j++){
			if (room_list[i][j] == fromfd){
				roomNum=i;
			}
		}
	}
	while (current != NULL) {
		int inlist = 0;
		if (current->clisockfd != fromfd) {
			for(int y = 0; y < MAX_PPL;y++){
				if(current->clisockfd==room_list[roomNum][y]){
					inlist=1;
				}
			}
			if (inlist==1){
			char buffer[512];
			memset(buffer, 0, 512);
			// prepare message
			
			sprintf(buffer, "\x1B %s %s [%s]: %s   \x1B[0m", color, username, inet_ntoa(cliaddr.sin_addr), message);
			int nmsg = strlen(buffer);
			// send message
			int nsen = send(current->clisockfd, buffer, nmsg, 0);
			if (nsen != nmsg) error("ERROR send() failed");
		}
}
		current = current->next;
	}
}

typedef struct _ThreadArgs {
	int clisockfd;
} ThreadArgs;

void* thread_main(void* args)
{  
	// make sure thread resources are deallocated upon return
	pthread_detach(pthread_self());
	char discbuffer[512];
	// get socket descriptor from argument
	int clisockfd = ((ThreadArgs*) args)->clisockfd;
	free(args);

	char buffer[256];
	int nsen, nrcv;
	nrcv = recv(clisockfd, buffer, 255, 0);
	client_user* current = head;
	client_user* prev = head;
	int temp_fd;

	// find out which user is at current position in linked list 
	while (current != NULL) {
		temp_fd = current->clisockfd;
		if (temp_fd == clisockfd) { // found current user
			break; 
		}
		if (current != head) {
			prev = prev->next;
		}
		current = current->next;
	}
	if((strcmp(buffer,"\0")==0)||(strlen(buffer)==1)){
			nrcv=-1;
		}
	// first message
	if (nrcv < 0) {		
		printf("ERROR: recv() failed");
	} else if (nrcv == 0) {	// 
		// display when user disconnects
		printf("User [%s] DISCONNECTED\n", current->username); 
		memset(discbuffer, 0, 512);
		sprintf(discbuffer,"%s has left the chatroom.\n",current->username);
		send_message(current->clisockfd,discbuffer);
		memset(discbuffer, 0, 512);
		for(int x = 0; x < MAX_PPL;x++){
			if(room_list[current->room][x]==clisockfd){		
				room_list[current->room][x]=-1;
			}
		}
		// remove user from list
		if (current == head) {
			head = current->next;
			free(current);
		} else if(current == tail){
			tail = prev;
			free(current);
		} 
		else {
			prev->next = current->next;
			free(current);
		}
		close(clisockfd);
		return NULL;
	}

	// every message after first
	while (nrcv > 0) {

		if (nrcv < 0) {		
			error("ERROR: recv() failed");
		} else {
			send_message(clisockfd, buffer);
		}
		memset(buffer, 0, sizeof(buffer));
		nrcv = recv(clisockfd, buffer, 255, 0);
		if((strcmp(buffer,"\0")==0)||(strlen(buffer)==1)){
			nrcv=-1;
		}
		if(strcmp(buffer,"SEND")==0){
			//write_file(clisockfd,"test.txt");
		}
	}
	// disconnecting after sending messages
	printf("User [%s] DISCONNECTED FROM THE CHAT ROOM!\n", current->username);
	memset(discbuffer, 0, 512);
	sprintf(discbuffer,"%s has left the chatroom.\n",current->username);
	send_message(current->clisockfd,discbuffer);
	for(int x = 0; x < MAX_PPL;x++){
			if(room_list[current->room][x]==clisockfd){	
				room_list[current->room][x]=-1;
			}
		}
	if (current == head) {
		head = current->next;
		free(current);
	}else if(current == tail){
			tail = prev;
			free(current);
		} 
	else {
		prev->next = current->next;
		free(current);
	}
	close(clisockfd);
	return NULL;
}


int main(int argc, char *argv[])
{
	//initialize the 2d array that holds chatrooms and occupants
	for(int x = 0; x < NUM_ROOMS; x++){
		for(int y = 0; y < MAX_PPL; y++){
			room_list[x][y]=-1;
		}
	}
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) error("ERROR opening socket");
	//set up socket and node
	struct sockaddr_in serv_addr;
	socklen_t slen = sizeof(serv_addr);
	memset((char*) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;	
	//serv_addr.sin_addr.s_addr = inet_addr("192.168.1.171");	
	serv_addr.sin_port = htons(PORT_NUM);

	int status = bind(sockfd, 
			(struct sockaddr*) &serv_addr, slen);
	if (status < 0) error("ERROR on binding");

	listen(sockfd, 5); // maximum number of rooms = 5
	char buffer[512];
	memset(buffer, 0, 512);
	while(1) {
		//set up the node
		int room_input_int;
		struct sockaddr_in cli_addr;
		socklen_t clen = sizeof(cli_addr);
		int newsockfd = accept(sockfd, 
			(struct sockaddr *) &cli_addr, &clen);
		if (newsockfd < 0) error("ERROR on accept");

        printf("New Client Connecting...\n");

        // choose color
		int nrcv;
        char* user_color = unique_color();
		char room_input[256];
		memset(room_input,0,256);
		nrcv = recv(newsockfd,room_input,255,0);
		//see if the user wants a new room
		if(strcmp(room_input,"new")==0){
			int foundroom = 0;
			//find an empty room
			for(int x = 0; x < NUM_ROOMS; x++){
				int roomisempty = 1;
				for(int y = 0; y < MAX_PPL; y++){
					if(room_list[x][y]!=-1){
						room_input_int=x;
						roomisempty = 0;
						
					}
				}
			if(roomisempty==1){
				foundroom = 1;
				room_input_int=x;
				sprintf(buffer,"You were assigned room %i\n",x);
				send(newsockfd,buffer,strlen(buffer),0);
				memset(buffer,0,256);
				break;
			}
		}
		if(foundroom==0){
			sprintf(buffer,"%s","There was no room, the number of chatrooms is capped at 5\n Terminating your client\n");
			send(newsockfd,buffer,strlen(buffer),0);
			memset(buffer,0,255);
			close(newsockfd);
			
		}
		}else if(strcmp(room_input,"x")==0){
			//situation where the user leaves the space blank next to IP address
			int personcount[NUM_ROOMS];
			for(int x = 0; x < NUM_ROOMS; x++){
				int cnt = 0;
				for(int y = 0; y < MAX_PPL; y++){
					if(room_list[x][y]!=-1){
						cnt = cnt+1;
					}
				}
				personcount[x]=cnt;
			}
			
			sprintf(buffer,"room 0 has %i people\n room 1 has %i people\n room 2 has %i people\n room 3 has %i people\n room 4 has %i people\n",
			personcount[0],personcount[1],personcount[2],personcount[3],personcount[4]);
			send(newsockfd,buffer,strlen(buffer),0);
			nrcv = recv(newsockfd, room_input, 255, 0);
			room_input_int = atoi(room_input);

		}
		else{
			//if they just entered a number use that as the room
			room_input_int = atoi(room_input);
		}
		memset(buffer, 0, 512);
        // get username from client input
        char username[256];
        memset(username, 0, 256); //setting memory
        nrcv = recv(newsockfd, username, 255, 0);
        if(nrcv < 0) {
            error("Error recv() failed");
        }	
		// add new user to the list
		add_user(newsockfd, username, user_color,room_input_int); 
		//prints out the notification that the user has entered the room
		sprintf(buffer,"%s has joined the chatroom",username);
		send_message(newsockfd,buffer);
		memset(buffer, 0, 512);
		// prepare ThreadArgs structure to pass client socket
		ThreadArgs* args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
		if (args == NULL) error("ERROR creating thread argument");

 		// print out list of currently connected users
        print_clients();
		//check file descriptor to fix bug
		args->clisockfd = newsockfd;
		pthread_t tid;
		if (pthread_create(&tid, NULL, thread_main, (void*) args) != 0) error("ERROR creating a new thread");
	}

	return 0; 
}

//---TODO tell the user which room they entered if they type new
//---notification of leaving
//empty string (unresolved)
//---can leave and rejoin

//Known bugs:
//	Entering usernames out of order (I don't really know how to fix this)
//  Adding an eighth person to a room
//  Not having a free room