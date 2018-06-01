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

/***********
 *  main
 ***********/
int main (int argc, char *argv[])
{
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
	
	//setup select
	fd_set readfs;
	struct timeval tv;
	int rv;

	//setup filename
	PACKET p,ack;
	int i;
	for (i = 0; i < strlen(argv[2])+1; i++) p.data[i] = argv[2][i];
	p.header.seq = 0;

	// send output filename
	printf ("Sending file name: %s\n", p.data);
	p.header.checksum = checksum(p);
	printf( "Checksum: %d",p.header.checksum);
	//setup select
	FD_ZERO(&readfs);
	FD_SET(sock,&readfs);
	//set timer
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	
	//send
	sendto (sock, &p, sizeof(p), 0, (struct sockaddr *)&serverAddr, addr_size);
	
	//call select, send filename to server
	rv = select(sock+1,&readfs,NULL,NULL,&tv);

	//wait for the ack, resend for timeouts
	while (1){
		if (rv == 1){
			//receive the ACK
			recvfrom (sock, &ack, sizeof(ack), 0, (struct sockaddr *)&serverAddr, &addr_size);
			//if ack seq # correct, break, otherwise continue and resend the packet
			if (ack.header.seq == p.header.seq) break;
		}

		printf("timer expired\n");
		//setup select
		FD_ZERO(&readfs);
		FD_SET(sock,&readfs);
		//set timer
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		//call select, send filename to server
		sendto (sock, &p, sizeof(p), 0, (struct sockaddr *)&serverAddr, addr_size);
		rv = select(sock+1,&readfs,NULL,NULL,&tv);
	}
	printf ("ACK received\n");

	return 0;
	//read from file and send to server
	while ( (nBytes = fread(buffer,1,10,in)) > 0 )
	{
		printf(buffer);
		//swap the sequence numbers between 0 and 1
		p.header.seq = (p.header.seq+1) % 2;
		int i;
		//convert char * to char[]
		for (i = 0; i < strlen(buffer)+1; i++)
			p.data[i] = buffer[i];

		//send file line to server
		sendto (sock, &p, sizeof(p), 0, (struct sockaddr *)&serverAddr, addr_size);

		//wait for ack
		recvfrom (sock, &ack, sizeof(ack), 0, (struct sockaddr *)&serverAddr, &addr_size);
		
		//check the ack
		if (ack.header.seq == p.header.seq){//ack confirmed
			//printf("Received ACK %d\n", ack.header.seq_ack);
			continue;
		}
		else{//ack has wrong sequence number
			printf("Packet ACK out of sequence!\n");
			return 1;
		}	
	}

	//send fin packet
	PACKET fin;
	fin.header.fin = 1;
	sendto (sock, &fin, sizeof(fin), 0, (struct sockaddr *)&serverAddr, addr_size);

	fclose(in);
	return 0;
}
