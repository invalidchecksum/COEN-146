


#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>

#define SIZE    20


typedef struct
{
    int	seq_ack;

} HEADER;

typedef struct
{
    HEADER	header;
    char	data[SIZE];
} PACKET;


