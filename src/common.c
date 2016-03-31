#include "common.h"

#define buffer_size 2048 //размер буффера для чтения из потока вывода ребенка
pthread_mutex_t child_dead = PTHREAD_MUTEX_INITIALIZER;
volatile int child_dead_var = 0;
char reading_buf[buffer_size]; //буффер
char writing_buf[buffer_size]; //буффер
int bytes = 0; //смещение в буфере чтения
int byteswr = 0; //смещение в буфере записи
int RSTDOUT[2]; //1-запись, 0 - чтение
int RSTDIN[2]; //1-запись, 0 - чтение
int RSTDERR[2]; //1-запись, 0 - чтение
time_t lastio; //таймауты для select()
int logfileno = 2;
pid_t child;

void sign_handler(int signo, siginfo_t *siginf, void *ptr)
{
	switch(signo)
	{
		case SIGCHLD:
		{
			printf("%d TERMINATED WITH EXIT CODE: %d\n", siginf->si_pid, siginf->si_status);
			//exit(EXIT_SUCCESS);
			child_dead_var =1;
			break;
		};
		case SIGPIPE:
		{
			printf("Listner is dead..\n");
			exit(EXIT_SUCCESS);
			break;
		};
		default:
		{
			break;
		};
	}
	return;
}

void SIGNIO_handler(int signo, siginfo_t *siginf, void *ptr)
{
	lastio = time(NULL);
	if(siginf->si_fd == RSTDOUT[PIPE_READ])
	{
		//gettimeofday(&lastio,NULL);
		handle_output(RSTDOUT[PIPE_READ]);
	}
	if(siginf->si_fd == RSTDERR[PIPE_READ])
	{
		//gettimeofday(&lastio,NULL);
		handle_output(RSTDOUT[PIPE_READ]);
	}
	if(siginf->si_fd == STDIN_FILENO)
	{
		//gettimeofday(&lastio,NULL);
		handle_input(RSTDIN[PIPE_WRITE]);
	}
	return;
}

/*
 * Возвращает дату и время в формате DD.MM.YYYY/HH:MM:SS
 */
char *get_datetime()
{
	char *datetime = (char*)malloc(30);
	time_t rawtime;
	struct tm *timeinfo;
	time (&rawtime);
	timeinfo = localtime(&rawtime);
	sprintf (datetime,"%d.%d.%d/%d:%d:%d",timeinfo->tm_mday,timeinfo->tm_mon+1, timeinfo->tm_year+1900,
												timeinfo->tm_hour,timeinfo->tm_min, timeinfo->tm_sec);
	return datetime;
}

const char delims[] = {'\t','\n',' '};
int delims_count = sizeof(delims)/sizeof(delims[0]);
int is_delimetr(char symbol) //return 1 if symbol is delimeter or 0
{
	for(int i=0;i<delims_count;i++)
	{
		if(delims[i]==symbol) return 1;
	}
	return 0;
}


char **string_to_argv(char *string, int *argc)
{
	int exec_len = strlen(string);
	int begin =0;
	int end =0;
	int argc_local = 0;
	char **argv_local;
	
	while(begin<exec_len)
	{
		while(begin<exec_len && is_delimetr(string[begin])) begin++;
		end = begin;
		while(end<exec_len && !is_delimetr(string[end])) end++;
		begin = end;
		argc_local++;
	}
	
	//printf("argc %d\n",argc_local);
	argv_local = (char**)malloc(argc_local+1);
	int i = 0;
	end = 0;
	begin = 0;
	
	while(begin<exec_len)
	{
		while(begin<exec_len && is_delimetr(string[begin])) begin++;
		end = begin;
		while(end<exec_len && !is_delimetr(string[end])) end++;
		
		int arr_size = end-begin;
		argv_local[i] = malloc(arr_size+2);
		memcpy(argv_local[i],string+begin,arr_size);
		argv_local[i][arr_size] = '\0';
		//printf("argv %s\n",argv_local[i]);
		
		begin = end;
		i++;
	}
	argv_local[argc_local] = NULL;
	
	*argc = argc_local;
	return argv_local;
}

int handle_output(int fd)
{
	while((read(fd, reading_buf+bytes, 1)) > 0)
	{
		bytes++;
		if(reading_buf[bytes-1]=='\n' || bytes==buffer_size)
		{
			int stream = 1;
			if(fd==RSTDERR[PIPE_READ]) stream = 2;
			else stream = 1;
			LOG(reading_buf,bytes,stream);
			bytes=0;
		}
	}
	
	return 0;
}

void LOG(char *buf, int size,int stream)
{
	char *dtm = get_datetime();
	if(stream>=0)
	{
		char buffer[80];
		if(stream==0)
			sprintf(buffer,"%d >0 ",child);
		else
			sprintf(buffer,"%d <%d ",child,stream);
		write(STDOUT_FILENO,buffer,strlen(buffer));
		write(STDOUT_FILENO,buf,size);
		//-----
		if(stream==0)
			sprintf(buffer," >0 ");
		else
			sprintf(buffer," <%d ",stream);
		write(logfileno,dtm,strlen(dtm));
		write(logfileno,buffer,strlen(buffer));
		write(logfileno,buf,size);
	}
	else
	{
		char buffer[70];
		strcpy(buffer,dtm);
		strcat(buffer," NOIO\n");
		write(logfileno,buffer,strlen(buffer));
	}
	free(dtm);
}

int handle_input(int fd)
{
	while((read(STDIN_FILENO,writing_buf+byteswr,1))>0)
	{
		byteswr++;
	}
	
	if(byteswr>0)
	{
		write(fd,writing_buf,byteswr);
		writing_buf[byteswr-1] = '\0';
		if(strcmp(writing_buf,"exit")==0)
		{
			printf("[I]: EXIT\n");			
			exit(EXIT_SUCCESS);
		}
		writing_buf[byteswr-1] = '\n';
		LOG(writing_buf,byteswr,0);
		byteswr =0;
	}
	
	return 0;
}

void add_flags(int fd, int flags)
{
	int old_flags = fcntl(fd, F_GETFL, 0);
	fcntl(fd,F_SETFL,old_flags|flags);
	return;
}

void proc_manager(params *cmd)
{
	logfileno = cmd->logfile_descr;
	lastio = time(NULL);
	struct sigaction sa;
	sigemptyset(&(sa.sa_mask));
	sigfillset(&(sa.sa_mask));
	
	sa.sa_flags = SA_SIGINFO|SA_NOCLDWAIT; //use siginfo
	sa.sa_sigaction = sign_handler; //set signals handler
	if(sigaction(SIGPIPE, &sa, 0)==-1 || sigaction(SIGCHLD, &sa, 0)==-1)
	{
		perror(NULL);
		exit(EXIT_FAILURE);
	}
	
	if(pipe(RSTDOUT)==-1 || pipe(RSTDIN)==-1 || pipe(RSTDERR)==-1)
	{
		perror(NULL);
		exit(EXIT_FAILURE);
	}
	
	add_flags(RSTDOUT[PIPE_READ],O_NONBLOCK);
	add_flags(RSTDOUT[PIPE_WRITE],O_NONBLOCK);
	
	child = fork();			
	switch(child)
	{
		case -1:
		{
			perror(NULL);
			exit(EXIT_FAILURE);
			break;
		};
		case 0: //fork
		{			
			//перенаправить stdin
			if (dup2(RSTDIN[PIPE_READ], STDIN_FILENO) == -1)
			{
				perror("redirecting stdin");
				exit(EXIT_FAILURE);
			}
			// перенаправить stdout
			if (dup2(RSTDOUT[PIPE_WRITE], STDOUT_FILENO) == -1)
			{
				perror("redirecting stdout");
				exit(EXIT_FAILURE);
			}
			// перенаправить stderr
			if (dup2(RSTDERR[PIPE_WRITE], STDERR_FILENO) == -1)
			{
				perror("redirecting stderr");
				exit(EXIT_FAILURE);
			}
			fflush(stdout);
			
			//setbuf(stdout, NULL); //отключить буферизацию stdout
			close(RSTDIN[PIPE_READ]);
			close(RSTDIN[PIPE_WRITE]);
			close(RSTDOUT[PIPE_READ]);
			close(RSTDOUT[PIPE_WRITE]);
			close(RSTDERR[PIPE_READ]);
			close(RSTDERR[PIPE_WRITE]);
			
			char **argv;
			int argc=0;
			argv = string_to_argv(cmd->execute,&argc);
			
			execvp(argv[0], argv);
			perror("Call exec() failed!");
			exit(EXIT_FAILURE);
		};
		default: //батька
		{
			close(RSTDIN[PIPE_READ]);
			close(RSTDOUT[PIPE_WRITE]);
			close(RSTDERR[PIPE_WRITE]);
			
			add_flags(STDIN_FILENO,O_NONBLOCK);
			
			if(cmd->multiplex==1) multiplexer_select(cmd);
			else if(cmd->multiplex==0) multiplexer_signal(cmd);
			
			break;
		};
	}
}

void multiplexer_select()
{
	//setbuf(stdin, NULL);
	
	fd_set rfds,wfds; //набор дескрипторов потоков с которых нужно ожидать чтение
	fd_set rfdscopy = rfds,wfdscopy = wfds; //select портит fd_set и таймауты
	struct timeval tv; //таймауты для select()
	struct timeval tvcopy = tv;
	int retval;
	FD_ZERO(&rfds); //очистить набор
	FD_SET(STDIN_FILENO, &rfds); //добавить дескриптор в набор
	FD_SET(RSTDOUT[PIPE_READ], &rfds); //добавить дескриптор в набор
	FD_SET(RSTDERR[PIPE_READ], &rfds); //добавить дескриптор в набор
	tv.tv_sec = 1; //ждать максимум n секунд(после выхода из select()
	tv.tv_usec = 0; //значение таймаута будет сброшено)
	int maxfd = max(STDIN_FILENO,max(RSTDOUT[PIPE_READ],RSTDERR[PIPE_READ]));
	
	while(1)
	{
		tvcopy = tv;
		rfdscopy = rfds; wfdscopy = wfds;
		retval = select(maxfd+1, &rfdscopy, NULL, NULL, &tvcopy);
		
		if(retval<0)
		{
			if(errno==EINTR) continue; //если выход из селекта из-за сигнала
			perror("Call select() failed");
			exit(EXIT_FAILURE);
		}
		else if(retval>0)
		{
			if(FD_ISSET(STDIN_FILENO, &rfdscopy))
			{
				handle_input(RSTDIN[PIPE_WRITE]);
			}
			if(FD_ISSET(RSTDOUT[PIPE_READ], &rfdscopy))
			{
				handle_output(RSTDOUT[PIPE_READ]);
			}
			if(FD_ISSET(RSTDERR[PIPE_READ], &rfdscopy))
			{
				handle_output(RSTDERR[PIPE_READ]);
			}
		}
		else
		{
			LOG(NULL,0,-1);
		}
	}
	close(RSTDIN[1]);
	exit(EXIT_SUCCESS);	
	
}

void multiplexer_signal()
{
	struct sigaction sa2;
	sigemptyset(&(sa2.sa_mask));
	sigfillset(&(sa2.sa_mask));	
	sa2.sa_flags = SA_SIGINFO;
	sa2.sa_sigaction = SIGNIO_handler;
	
	if(sigaction(SIGIO, &sa2, 0)==-1)
	{
		perror(NULL);
		exit(EXIT_FAILURE);		
	}	
	
	add_flags(RSTDOUT[PIPE_READ],O_NONBLOCK|FASYNC);
	add_flags(RSTDERR[PIPE_READ],O_NONBLOCK|FASYNC);
	add_flags(STDIN_FILENO,O_NONBLOCK|O_ASYNC);
	add_flags(STDOUT_FILENO,O_NONBLOCK);
	
	fcntl(RSTDOUT[PIPE_READ], F_SETSIG, SIGIO);
	fcntl(RSTDOUT[PIPE_READ], F_SETOWN, getpid());
	fcntl(RSTDERR[PIPE_READ], F_SETSIG, SIGIO);
	fcntl(RSTDERR[PIPE_READ], F_SETOWN, getpid());
	fcntl(STDIN_FILENO, F_SETSIG, SIGIO);
	fcntl(STDIN_FILENO, F_SETOWN, getpid());
	
	while(1) 
	{		
		sleep(1);
		if(time(NULL)>lastio)
		{
			LOG(NULL,0,-1);
			lastio = time(NULL);
		}
		if(child_dead_var==1)
		{
			exit(EXIT_SUCCESS);
		}
	}
}
