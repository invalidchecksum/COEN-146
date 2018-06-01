
/********************
 * COEN 146, UDP example, server
 ********************/

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
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

/********************
 * main
 ********************/
int main (int argc, char *argv[])
{
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
		
	PACKET p,ack;
	//wait for data
	nBytes = recvfrom (sock, &p, sizeof(p), 0, (struct sockaddr *)&serverStorage, &addr_size);
	
	//check the checksum
	char cks = p.header.checksum;
	while (cks == checksum(p)){
		printf("Dope!!! %d = %d\n", p.header.checksum,checksum(p));
		sendto (sock, &p, sizeof(p), 0, (struct sockaddr *)&serverStorage, addr_size);
		nBytes = recvfrom (sock, &p, sizeof(p), 0, (struct sockaddr *)&serverStorage, &addr_size);
	}
	// receive filename and copy into packet
	char filename[0];
	strcpy(filename, p.data);

	printf("Outfile: %s\n", filename);

	//open
	FILE *out = fopen(filename,"w");
	if (out == NULL){
		printf("File cannot be opened\n");
		return 0;
	}
	//send ack for filename received	
	sendto (sock, &p, sizeof(p), 0, (struct sockaddr *)&serverStorage, addr_size);
	
	return 0;
	while (nBytes = recvfrom (sock, &p, sizeof(p), 0, (struct sockaddr *)&serverStorage, &addr_size))
	{	
		//if file transfer done, send ACK and break
		if (p.header.fin == 1){
			sendto (sock, &p, sizeof(p), 0, (struct sockaddr *)&serverStorage, addr_size);
			break;
		}

		//printf("%d,%s\n",p.header,p.data);
		printf("%s", p.data);
		
		fwrite(p.data,1,10,out);//write 10 bytes to file

		//send the same packet back as an ack after clearing the data
		sendto (sock, &p, sizeof(p), 0, (struct sockaddr *)&serverStorage, addr_size);	
	}

	fclose(out);
	printf("\nFile transmitted!\n");
	
	return 0;
}
