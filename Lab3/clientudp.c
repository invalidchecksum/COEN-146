/*****************************
 * COEN 146, UDP, client
 *****************************/

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include "./tfv1.h"

/***********
 *  main
 ***********/
int main (int argc, char *argv[])
{
	int sock, portNum, nBytes;
	char buffer[10];
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

	PACKET p,ack;
	int i;
	for (i = 0; i < strlen(argv[2])+1; i++) p.data[i] = argv[2][i];
	p.header.seq_ack = 0;
	//printf("%d,%s\n",p.header,p.data);

	// send output filename
	printf ("Sending file name: %s\n", p.data);
	sendto (sock, &p, sizeof(p), 0, (struct sockaddr *)&serverAddr, addr_size);

	//wait for the ack
	recvfrom (sock, &ack, sizeof(ack), 0, (struct sockaddr *)&serverAddr, &addr_size);
	
	//check the ack
	if (ack.header.seq_ack == p.header.seq_ack){//ack confirmed
		//printf("Received ACK %d\n", ack.header.seq_ack);
	}
	else{//ack has wrong sequence number
		printf("Packet ACK out of sequence!\n");
		return 1;
	}

	while ( (nBytes = fread(buffer,1,10,in)) > 0 )
	{
		printf(buffer);
		//swap the sequence numbers between 0 and 1
		p.header.seq_ack = (p.header.seq_ack+1) % 2;
		int i;
		//convert char * to char[]
		for (i = 0; i < strlen(buffer)+1; i++)
			p.data[i] = buffer[i];

		//send file line to server
		sendto (sock, &p, sizeof(p), 0, (struct sockaddr *)&serverAddr, addr_size);

		//wait for ack
		recvfrom (sock, &ack, sizeof(ack), 0, (struct sockaddr *)&serverAddr, &addr_size);
		
		//check the ack
		if (ack.header.seq_ack == p.header.seq_ack){//ack confirmed
			//printf("Received ACK %d\n", ack.header.seq_ack);
			continue;
		}
		else{//ack has wrong sequence number
			printf("Packet ACK out of sequence!\n");
			return 1;
		}	
		//printf("%d,%s\n",p.header,p.data);
	}
	
	fclose(in);
	return 0;
}
