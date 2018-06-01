//Nick Aiello - SCU COEN 146 Lab 5 Spring 2018

#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <limits.h>

void sendupdate();//send user inputs to peers
void readfiles(FILE *c, FILE *m);//parse file data
void userinput();//every 10 seconds, get changes from keyboard, thread 1
void *listener();//thread 2
void shortestpath();//thread 3
void build_peer_addressing();
void printCosts();
void *linkstate();
void printPath();
void printPeers();
typedef struct{
	int routerID;
	int neighborID;
	int newCost;
}message;

//holds info on each peer router
typedef struct{
	char name[50];//superfluous :)
	char ip[50];
	int port;
	struct sockaddr_in addr;
	struct sockaddr_storage storage;
	socklen_t addr_size;
}peer;

//this router's socket
int mysock;
struct sockaddr_in myaddr;
struct sockaddr_storage mystorage;
socklen_t myaddr_size;

int myid;//ID of this router, updated in main from args
int N;//# of routers in fabric, updated in main from args
int costs[50][50];//cost array, managed by N, max size is 50 peers
int path[50][50];//contains the shortest path distance for each element
peer peers[50];//peer info, managed by N, max size is 50 peers
pthread_mutex_t lock;//memory locking variable

int ITERATIONS = 3;//number of times to update from keyboard

int main(int argc, char *argv[]){
	//check arg count, terminate if wrong
	if (argc != 5){
		printf("Usage: <RouterID> <#ofNodes> <CostsFile> <MachinesFile>\n");
		return 1;
	}
	//printf("RouterID: %s\n# of Nodes: %s\nCostsFile: %s\nMachinesFile: %s\n",argv[1],argv[2],argv[3],argv[4]);

	//get args and gather info from files
	N = atoi(argv[2]);
	myid = atoi(argv[1]);
	FILE *cFile = fopen(argv[3],"r");
	FILE *mFile = fopen(argv[4],"r");	
	readfiles(cFile,mFile);//parse files
	printPeers();
	fclose(cFile);
	fclose(mFile);
	
	srand(time(0));

	printf("My Info: ID: %d\nIP: %s\nPort: %d\n\n",myid,peers[myid].ip,peers[myid].port);//declare myself to screen

	//propagate socket info to myself based on myid and file info
	myaddr.sin_family = AF_INET;
	myaddr.sin_port = htons ((short)peers[myid].port);
	myaddr.sin_addr.s_addr = htonl (INADDR_ANY);
	memset ((char *)myaddr.sin_zero,'\0',sizeof(myaddr.sin_zero));
	myaddr_size = sizeof(mystorage);

	//create socket	
	if ((mysock = socket(AF_INET,SOCK_DGRAM,0)) < 0){
		printf("Socket Error!\n");
		return 1;
	}
	//bind socket
	if(bind(mysock,(struct sockaddr *)&myaddr,sizeof(myaddr))!=0){
		printf("Bind Error!\n");
		return 1;
	}

	//fill out peer address information for each router
	build_peer_addressing();

	printCosts();

	//spawn threads: listener & shoretest path
	printf("Starting threads\n");
	pthread_t listener_t, linkstate_t;
	pthread_create(&listener_t,NULL,listener,NULL);
	pthread_create(&linkstate_t,NULL,linkstate,NULL);	
	
	//thread 1, loop 3 times getting user input then sleep for 10 seconds
	int i = 0;
	while(i<ITERATIONS){
		userinput();//get user inputs and send to peers
		//printCosts();//print the current costs table
		sleep(10);
		i++;
	}
	return 0;
}

void *listener(){
	//printf("Listener started!\n");
	//int a[3],nBytes;
	message x;
	//while (nBytes = recvfrom(mysock,&a,sizeof(a),0,(struct sockaddr *)&mystorage,&myaddr_size)){
	while (recvfrom(mysock,&x,sizeof(x),0,NULL,NULL)){
		int a[3];
		a[0] = x.neighborID;
		a[1] = x.routerID;
		a[2] = x.newCost;
		printf("Receiving update: <%d,%d,%d>!\n",a[0],a[1],a[2]);
		pthread_mutex_lock(&lock);
		costs[a[0]][a[1]] = a[2];
		costs[a[1]][a[0]] = a[2];
		pthread_mutex_unlock(&lock);
		printCosts();
	}
}

void *linkstate(){
	while(1){
		int i;
		for(i=0;i<N;i++){
			//printf("\n%d: ",i);
			pthread_mutex_lock(&lock);
			shortestpath(i);
			pthread_mutex_unlock(&lock);
		}
		printPath();
		sleep(10);
	}
	return;
}

int minDist(short int visited[], int dist[]){
	int i;
	int min=INT_MAX,minID;
	//check each node for closest peer
	for(i=0;i<N;i++)
		if(visited[i]==0 && min>=dist[i]){
			minID = i,min=dist[i];
		}
	return minID;
}

void shortestpath(int src){
	int dist[N];
	short int visited[N];
	int i;
	//initialize distance to large int, and all nodes not visited
	for(i=0;i<N;++i){
		visited[i]=0,dist[i]=INT_MAX;
	}
	dist[src] = 0;//0 cost to self
	
	//loop thru N-1 nodes since ignore self
	int c,j;
	for(c=0;c<N-1;c++){
		int minID = minDist(visited,dist);//get next closest node
		visited[minID] = 1;
		for(j=0;j<N;++j){
			//not visited && cost exists && distance is less && shorter distance than before
			if(visited[j] == 0 && costs[minID][j] && dist[minID] != INT_MAX 
					   && dist[j] > dist[minID]+costs[minID][j])
				dist[j] = dist[minID]+costs[minID][j];//update shortest path cost to this node
		}
	}
	//print results
	//for (i=0;i<N;i++) printf("%d,",dist[i]);
	for (i=0;i<N;i++)
		path[src][i]=dist[i];
	
	return;
}

void printPath(){
	//print out table of least cost from each node to each node
	int i,j;
	printf("\nPath Table:\n");
	for(i=0;i<N;i++){
		for(j=0;j<N;j++)
			printf("%d ",path[i][j]);
		printf("\n");
	}
	return;
}

void build_peer_addressing(){
	int sock;
	struct sockaddr_in addrs[N];
	socklen_t addr_size[N];

	//create peer addressing information
	//loop through all nodes and update their address info in peer struct
	int i;
	for (i=0;i<N;i++){
		//if (i==myid) continue;
		peers[i].addr.sin_family = AF_INET;
		peers[i].addr.sin_port = htons(peers[i].port);
		inet_pton(AF_INET, peers[i].ip, &(peers[i].addr.sin_addr.s_addr));
		memset (peers[i].addr.sin_zero,'\0',sizeof(peers[i].addr.sin_zero));
		peers[i].addr_size = sizeof(peers[i].addr);
	}

	return;
}

void sendupdate(int p, int c){
	//send update to all peers of the user input change
	int i;
	//int outdata[3] = {myid,p,c};
	message outdata;
	outdata.neighborID = myid;
	outdata.routerID = p;
	outdata.newCost = c;

	for (i=0;i<N;i++){
		//if (i==myid) continue;
		//printf("Sending: <%s,%d>\n",peers[i].ip,peers[i].port);
		printf("Sending to: %s\n",peers[i].ip);
		sendto(mysock,&outdata,sizeof(outdata),0,(struct sockaddr *)&peers[i].addr,peers[i].addr_size);
	}

	return;
}

void readfiles(FILE *c, FILE *m){
	//parse file data
	
	int i,j;
	char *line = NULL;
	size_t len = 0;
	while (getline(&line,&len,c)!= -1){
		//printf("%s",line);
		char *token = strtok(line,"\t");
		j=0;
		while (token != NULL && token != ""){
			//printf("%s ",token);
			//printf("<%d,%d>",i,j);
			costs[i][j] = atoi(token);
			if (++j == N) break;
			token = strtok(NULL,"\t");
		}
		//printf("a\n");
		if (++i == N) break;
	}
	i=0;
	/*
	char *stuff[4];
	char *token;
	while (getline(&line,&len,m)!=-1){
		//printf("%s",line);
		token = strtok(line,"\t");
		//printf("1: %s\n",token);
		peers[i].name = token;
		token = strtok(NULL,"\t");
		//printf("2: %s\n",token);
		peers[i].ip = token;
		//stuff[i]=token;
		token = strtok(NULL,"\t");
		peers[i].port = atoi(token);
		//printf("3: %s\n",token);
		printf("%d:%s\n",i,peers[i].ip);
		i++;
	}
	for (i=0;i<4;i++) printf("%d:%s\n",i,peers[i].ip);
	*/
//	for (i=0;i<N;i++)fscanf(m,"%s%s%d",peers[i].name,peers[i].ip,&peers[i].port);
	
	//printPeers();
	return;
}

void userinput(){
	int p=-1,c=-1;
	//sanitize inputs, 0<p<N && c>=0 to continue
	while ((p < 0 || p > N-1 || p == myid) || c<0){
		printf("Input new link cost: ");
		scanf("%d %d",&p,&c);
	}
	
	pthread_mutex_lock(&lock);
	costs[myid][p] = c;
	costs[p][myid] = c;
	//printf("Sending update...");
	sendupdate(p,c);
	//printf("Sent!\n");
	pthread_mutex_unlock(&lock);

	return;
}

void printCosts(){
	//prints a the cost array
	//printf("Updated Cost Table:\n");
	int i,j;
	for (i=0;i < N;++i){
		for (j=0;j < N;++j){
			printf("%d ",costs[i][j]);
		}
		printf("\n");
	}

	return;
}

void printPeers(){
	//prints list of peers from peers array (peer struct data type)
	int i=0;
	for(;i<N;++i){
		printf("Name: %s\nIP: %s\nPort: %d\n",peers[i].name,peers[i].ip,peers[i].port);
	}

	return;
}
