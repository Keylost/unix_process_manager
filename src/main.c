#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include "common.h"

void usage();

int main(int argc, char **argv)
{	
	params cmd;
	
	const char* short_options = "l:he:m:";
	char *logfile=NULL;
	int logfile_descr = 2; //stderr по умолчанию
	char *execute=NULL;
	int multiplex=1;

	const struct option long_options[] =
	{
		{"help",no_argument,NULL,'h'},
		{"logfile",optional_argument,NULL,'l'},
		{"execute",optional_argument,NULL,'e'},
		{"multiplex",optional_argument,NULL,'m'},
		{NULL,0,NULL,0}
	};

	int rez;
	int option_index;

	while ((rez=getopt_long(argc,argv,short_options,
		long_options,&option_index))!=-1){

		switch(rez){
			case 'h': 
			{
				usage();
				break;
			};
			case 'l':
			{
				logfile = malloc(strlen(optarg)+1);
				strcpy(logfile,optarg);
				break;
			};
	
			case 'e':
			{
				execute = malloc(strlen(optarg)+1);
				strcpy(execute,optarg);
				break;
			};
			case 'm':
			{
				multiplex=atoi(optarg);
				if(multiplex!=0 && multiplex!=1)
				{
					printf("[E]: multiplex must be 0 or 1\n");
					usage();
				}
				break;
			};			
			default:
			{
				fprintf(stderr,"[E]: Found unknown option\n");
				usage();
				break;
			};
		};
	};
	
	if(execute == NULL)
	{
		printf("[E]: You should set execute parameter\n");
		usage();
	}
	if(logfile != NULL)
	{
		logfile_descr = open(logfile, O_RDWR|O_CREAT);
	}
	
	cmd.logfile_descr = logfile_descr;
	cmd.execute = execute;
	cmd.multiplex = multiplex;
	
	if(multiplex!=0 && multiplex!=1)
	{
		printf("Unknown multiplexer\n");
		exit(EXIT_FAILURE);
	}
	
	proc_manager(&cmd);
	
	return 0;
}

void usage()
{
	printf(" -h or --help show this message and exit.\n");
	printf(" -m or --multiplex set IO handler type. multiplex must be 0 or 1. 1 by default\n");
	printf(" -l or --logfile file for logging\n");
	printf(" -e or --execute command for execute\n");
	exit(EXIT_SUCCESS);
};
