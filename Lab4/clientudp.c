/*****************************
 * COEN 146, UDP, client
 *****************************/

#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include "./tfv1.h"

char checksum(PACKET p){
	p.header.checksum = 0;
	char checksum = 0;
	int i;
	char * pp = &p;
	//printf("sizeof(p) = %d\n",sizeof(p));
	for (i = 0; i < sizeof(p); i++)
		checksum = checksum ^ *(pp+i);
	return checksum;
}

void reliable_send (int sock, PACKET p, struct sockaddr_in serverAddr, socklen_t addr_size){
	//setup select
	fd_set readfs;
	struct timeval tv;
	int rv;

	//setup select
	FD_ZERO(&readfs);
	FD_SET(sock,&readfs);
	//set timer
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	
	//send
	p.header.checksum = checksum(p);
	if (rand()%100 >= 20)//dont send 20%
		sendto (sock, &p, sizeof(p), 0, (struct sockaddr *)&serverAddr, addr_size);
	//start time and track received data
	rv = select(sock+1,&readfs,NULL,NULL,&tv);

	PACKET ack;

	//wait for the ack, resend for timeouts or duplicate ACK
	while (1){
		char cks;
		if (rv ==1){
			//receive the ACK
			recvfrom (sock, &ack, sizeof(ack), 0, (struct sockaddr *)&serverAddr, &addr_size);

			if (rand()%100 < 20)//ACK checksum invalid 20%
				ack.header.checksum = 0;

			//if ack seq# and checksum are correct, break, otherwise continue and resend the packet
			cks = ack.header.checksum;
			if (ack.header.seq == p.header.seq && cks == ack.header.checksum) break;
		}
		if (rv == 0)
			printf("Timer expired. Resending packet (seq#%d)...\n",p.header.seq);
		else if (cks != ack.header.checksum)
			printf("Checksum error (%d!=%d). Resending packet (seq#%d)...\n",cks,ack.header.checksum,p.header.seq);
		else if (ack.header.seq != p.header.seq)
			printf("Wrong ACK (%d!=%d). Resending packet...\n",p.header.seq,ack.header.seq);
		else
			printf("Unknown Error. Attempting to resend the packet\n");
		
		//setup select
		FD_ZERO(&readfs);
		FD_SET(sock,&readfs);
		tv.tv_sec = 0;
		tv.tv_usec = 500;
		//call select, send filename to server
		if (rand()%100 >= 20)//dont send 20%
			sendto (sock, &p, sizeof(p), 0, (struct sockaddr *)&serverAddr, addr_size);
		rv = select(sock+1,&readfs,NULL,NULL,&tv);
	}

	return;
}

/***********
 *  main
 ***********/
int main (int argc, char *argv[])
{
	//seed rand with time
	srand(time(0));
	
	int sock, portNum, nBytes;
	char buffer[25];
	struct sockaddr_in serverAddr;
	
	socklen_t addr_size;

	if (argc != 5)
	{
		printf ("Usage: %s in.txt out.txt <server> <port>\n", argv[0]);
		return 1;
	}

	// configure address
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons (atoi (argv[4]));
	inet_pton (AF_INET, argv[3], &serverAddr.sin_addr.s_addr);
	memset (serverAddr.sin_zero, '\0', sizeof (serverAddr.sin_zero));  
	addr_size = sizeof serverAddr;

	/*Create UDP socket*/
	sock = socket (PF_INET, SOCK_DGRAM, 0);

	FILE *in= fopen(argv[1],"r");//open src file
	nBytes = strlen (argv[2]) + 1;//get size of output file name
	
	
	//setup filename
	PACKET p;
	int i;
	for (i = 0; i < strlen(argv[2])+1; i++) p.data[i] = argv[2][i];

	//setup sequence
	int seq = 0;
	p.header.seq = seq;
	//printf( "Checksum: %d\n",p.header.checksum);
	
	//send output filename
	printf ("Outfile: %s\n", p.data);
	//send filename
	reliable_send(sock,p,serverAddr,addr_size);

	//read from file and send to server
	while ( (nBytes = fread(buffer,1,10,in)) > 0 )
	{
		//swap the sequence numbers between 0 and 1
		seq = (seq+1)%2;
		p.header.seq = seq;
		strcpy(p.data,buffer);
		//send file line to server
		reliable_send(sock,p,serverAddr,addr_size);
		//printf("%d,%s\n",seq,buffer);
	}

	//send fin packet
	PACKET fin;
	fin.header.fin = 1;
	seq = (seq+1)%2;
	fin.header.seq = seq;
	reliable_send(sock,fin,serverAddr,addr_size);
	printf("Received ACK for FIN\n");
	//sendto (sock, &fin, sizeof(fin), 0, (struct sockaddr *)&serverAddr, addr_size);

	fclose(in);

	printf("\nFile transmitted!\n");
	return 0;
}
