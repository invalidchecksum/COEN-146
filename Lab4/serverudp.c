
/********************
 * COEN 146, UDP example, server
 ********************/

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <string.h>
#include <stdlib.h>
#include "./tfv1.h"

char checksum (PACKET p){
	p.header.checksum = 0;
	char checksum = 0;
	int i;
	char * pp = &p;
	//printf("sizeof(p) = %d\n",sizeof(p));
	for (i = 0; i < sizeof(p); i++)
		checksum = checksum ^ *(pp+i);
	return checksum;

}

void sendack (PACKET ack, int sock, struct sockaddr_storage serverStorage, socklen_t addr_size){
	sendto (sock, &ack, sizeof(ack), 0, (struct sockaddr *)&serverStorage, addr_size);
	
	return;
}

/********************
 * main
 ********************/
int main (int argc, char *argv[])
{
	srand(time(0));
	int sock, nBytes;
	char buffer[1024];
	struct sockaddr_in serverAddr, clientAddr;
	struct sockaddr_storage serverStorage;
	socklen_t addr_size, client_addr_size;
	int i;

    if (argc != 2)
    {
        printf ("need the port number\n");
        return 1;
    }

	// init 
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons ((short)atoi (argv[1]));
	serverAddr.sin_addr.s_addr = htonl (INADDR_ANY);
	memset ((char *)serverAddr.sin_zero, '\0', sizeof (serverAddr.sin_zero));  
	addr_size = sizeof (serverStorage);

	// create socket
	if ((sock = socket (AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		printf ("socket error\n");
		return 1;
	}

	// bind
	if (bind (sock, (struct sockaddr *)&serverAddr, sizeof (serverAddr)) != 0)
	{
		printf ("bind error\n");
		return 1;
	}
		
	PACKET p,lastp,ack;
	//wait for data
	nBytes = recvfrom (sock, &p, sizeof(p), 0, (struct sockaddr *)&serverStorage, &addr_size);
	
	//check the checksum
	char cks = p.header.checksum;
	while (cks != checksum(p)){
		printf("Invalid Checksum for filename. Waiting for client retransmit.\n");
		nBytes = recvfrom (sock, &p, sizeof(p), 0, (struct sockaddr *)&serverStorage, &addr_size);
	}
	printf("Checksum: %d = %d\n",cks,checksum(p));
	//calculate ACK checksum
	p.header.checksum = checksum(p);
	//send ACK for filename
	if (rand()%100 >= 20)//dont send ACK 20%
		sendto (sock, &p, sizeof(p), 0, (struct sockaddr *)&serverStorage, addr_size);

	// receive filename and copy from the packet data
	char filename[0];
	strcpy(filename, p.data);
	
	//make sure the filename will not overwrite source code
	if (filename == "serverudp.c" || filename == "clientudp.c"){
		printf("Someone is trying to overwrite your source code!\n");
		return 0;
	}

	printf("Outfile: %s\n",filename);

	//open the file
	FILE *out = fopen(filename,"w");
	if (out == NULL){
		printf("File cannot be opened\n");
		return 0;
	}

	//set next expected seq #
	int expected_seq = (p.header.seq + 1) %2;

	//track last packet in case a duplicate ACK must be sent
	lastp = p;
	lastp.header.checksum = checksum(lastp);
	
	while (nBytes = recvfrom (sock, &p, sizeof(p), 0, (struct sockaddr *)&serverStorage, &addr_size))
	{
		if (rand()%100 < 20){//packect checksum invalid 20%
			//printf("Bad");
			p.header.checksum = 0;
		}

		//check checksum
		char cks = p.header.checksum;
		if (cks != checksum(p)){
			printf("Invalid checksum (%d,%d)\n",cks,checksum(p));
			if (rand()%100 >= 20)//dont send ACK 20%
				sendto (sock, &lastp, sizeof(lastp), 0, (struct sockaddr *)&serverStorage, addr_size);
			continue;
		}
		//out of order seq #, send last valid packet received
		if (p.header.seq != expected_seq){
			printf("Seq out of order (%d!=%d)\n",p.header.seq,expected_seq);
			if (rand()%100 >= 20)//dont send ACK 20%
				sendto (sock, &lastp, sizeof(lastp), 0, (struct sockaddr *)&serverStorage, addr_size);
			continue;
		}
		//if file transfer done, send ACK and break
		if (p.header.fin == 1){
			printf("Received fin packet. Terminating loop!\n");
			lastp = p;
			break;
		}

		//send the same packet back as an ack after clearing the data
		lastp = p;
		lastp.header.checksum = checksum(lastp);
		
		if (rand()%100 >= 20)//dont send ACK 20%
			sendto (sock, &lastp, sizeof(lastp), 0, (struct sockaddr *)&serverStorage, addr_size);

		//write to file
		//printf("%s",p.data);
		fwrite(p.data,1,10,out);//write 10 bytes to file
		//update expected sequence number
		expected_seq = (expected_seq + 1)%2;
		//printf("%d,%s\n",p.header.seq, p.data);
	}
	fclose(out);

	printf("Entering 3 seconds timed_wait!\n");
	//timed wait to make sure client gets last ack
	fd_set readfs;
	struct timeval tv;
	int rv;
	PACKET temp;
	
	tv.tv_sec = 3;
	tv.tv_usec = 0;
	FD_ZERO(&readfs);
	FD_SET(sock,&readfs);
	rv = select(sock+1,&readfs,NULL,NULL,&tv);

	//while data is received, send back ACK on fin packet
	while (1){
		if (rv == 1)
			recvfrom (sock, &temp, sizeof(temp), 0, (struct sockaddr *)&serverAddr, &addr_size);
		else
			break;
		tv.tv_sec = 3;
		tv.tv_usec = 0;
		FD_ZERO(&readfs);
		FD_SET(sock,&readfs);
		if (rand()%100 >= 20)//send ACK 20%
			sendto (sock, &p, sizeof(p), 0, (struct sockaddr *)&serverAddr, addr_size);
		rv = select(sock+1,&readfs,NULL,NULL,&tv);
	}

	printf("\nFile transmitted!\n");
	
	return 0;
}
