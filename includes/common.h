#ifndef common
#define common

#define _GNU_SOURCE
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>
#include <sys/time.h>

#ifndef max
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#endif

#define PIPE_READ 0
#define PIPE_WRITE 1

typedef struct
{
	int logfile_descr;
	int multiplex;
	char *execute;
}params;

void multiplexer_signal();
void multiplexer_select();
void proc_manager(params *cmd);
int handle_output(int fd);
int handle_input(int fd);
void LOG(char *buf, int size,int stream);

#endif
