//Nick Aiello - COEN 146L Lab 1 - 04/04/2018

#include <stdio.h>
#include <string.h>

//argc = # of args
//argv contains strings of elements starting at argv[1]
//argv[0] contains the running from files name, lab1.c
int main(int argc, char *argv[]){
	if (argc < 2){//Need 2 arguments
		printf ("Use only 2 arguments!\n");
		return 1;
	}

	//char src[] = "src.txt";
	//char dest[] = "dest.txt";
	printf("Reading from: %s\nWriting to: %s\n",argv[1],argv[2]);

	FILE *in,*out;
	in = fopen(argv[1],"r");	//open src file for reading
	out = fopen(argv[2],"w+");	//open dest file for reading and writing

	char buffer[10];	//buffer size, 10 bytes
	size_t data;		//output of fread

	//continuously read 10 bytes at a time
	while ( (data = fread(buffer,1,10,in)) > 0 ){
		fwrite(buffer,1,data,out);//write the 10 bytes to file
	}

	//close files
	fclose(in);
	fclose(out);
	
	return 0;
}
