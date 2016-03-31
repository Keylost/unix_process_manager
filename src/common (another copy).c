#include "common.h"

#define buffer_size 2048 //размер буффера для чтения из потока вывода ребенка
pthread_mutex_t child_dead = PTHREAD_MUTEX_INITIALIZER;
volatile int child_dead_var = 0;
char reading_buf[buffer_size]; //буффер
char writing_buf[buffer_size]; //буффер
int bytes = 0; //смещение в буфере

void sign_handler(int signo, siginfo_t *siginf, void *ptr)
{
	switch(signo)
	{		
		case SIGCHLD:
		{
			printf("%d TERMINATED WITH EXIT CODE: %d\n", siginf->si_pid, siginf->si_status);
			pthread_mutex_lock(&(child_dead));
			child_dead_var = 1;
			pthread_mutex_unlock(&(child_dead));
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
	
	printf("argc %d\n",argc_local);
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
		printf("argv %s\n",argv_local[i]);
		
		begin = end;
		i++;
	}
	argv_local[argc_local] = NULL;
	
	*argc = argc_local;
	return argv_local;
}

int handle_output(int fd, char *dtm)
{
	//bytes =0;
	while((read(fd, reading_buf+bytes, 1)) > 0)
	{
		bytes++;
		if(reading_buf[bytes-1]=='\n' || bytes==buffer_size)
		{
			char *buf = (char*)malloc(bytes+50);
			strcpy(buf,dtm);
			strcat(buf," output(1)");
			int len = strlen(buf);
			buf[len] = ' ';
			memcpy(buf+len+1,reading_buf,bytes);
			write(STDOUT_FILENO, buf, bytes+len+1); // 1 -> stdout
			free(buf);
			bytes=0;
		}
	}
	if(bytes>0)
	{
		char *buf = (char*)malloc(bytes+50);
		strcpy(buf,dtm);
		strcat(buf," output(1)");
		int len = strlen(buf);
		buf[len] = ' ';
		memcpy(buf+len+1,reading_buf,bytes);
		write(STDOUT_FILENO, buf, bytes+len+1); // 1 -> stdout
		free(buf);
		bytes=0;
	}
	return 0;
}

int handle_input(int fd, char *dtm)
{
	/*
	strcpy(writing_buf,dtm);
	strcat(writing_buf," input(0) ");
	int start = strlen(writing_buf);
	writing_buf[start] = ' ';
	start++;
	scanf("%s",writing_buf+start);
	int end = strlen(writing_buf);
	int bytes =0;
	int wb = 0;
	int sz = end - start -1;
	while((wb=write(fd,writing_buf+start+bytes,sz))>=0 && (bytes+=wb)<sz);
	* */
	//sleep(6);
	char buf = 'k';
	//printf("kjk\n");
	while(read(STDIN_FILENO,&buf,1)>0)
	{
		printf("kk\n");
		write(fd,&buf,1);
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
	pid_t child;
	
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
	
	int RSTDOUT[2]; //1-запись, 0 - чтение
	int RSTDIN[2]; //1-запись, 0 - чтение
	int RSTDERR[2]; //1-запись, 0 - чтение
	if(pipe(RSTDOUT)==-1 || pipe(RSTDIN)==-1 || pipe(RSTDERR)==-1)
	{
		perror(NULL);
		exit(EXIT_FAILURE);
	}
	
	add_flags(RSTDOUT[0],O_NONBLOCK|O_DIRECT);
	add_flags(RSTDOUT[1],O_NONBLOCK|O_DIRECT);
	
	add_flags(RSTDIN[0],O_DIRECT);
	add_flags(RSTDIN[1],O_DIRECT);
	
	add_flags(STDIN_FILENO,O_NONBLOCK);
	add_flags(STDOUT_FILENO,O_NONBLOCK);
	
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

			//all these are for use by parent only
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
			// close unused file descriptors, these are for child only
			close(RSTDIN[PIPE_READ]);
			close(RSTDOUT[PIPE_WRITE]);
			close(RSTDERR[PIPE_WRITE]);
			
			int is_dead = 0; //локальная копия флага смерти ребенка))
			int timeout = 1; //таймаут для select()
			fd_set rfds; //набор дескрипторов потоков с которых нужно ожидать чтение
			fd_set wfds; //на запись
			struct timeval tv; //таймауты для select()
			int retval;
			FD_ZERO(&rfds); //очистить набор
			//FD_ZERO(&wfds); //очистить набор
			//FD_SET(RSTDIN[PIPE_WRITE], &wfds); //добавить дескриптор в набор
			FD_SET(RSTDOUT[PIPE_READ], &rfds); //добавить дескриптор в набор
			tv.tv_sec = timeout; //ждать максимум n секунд(после выхода из select()
			tv.tv_usec = 0; //значение таймаута будет сброшено)
			//int maxfd = max(RSTDIN[PIPE_WRITE],RSTDOUT[PIPE_READ]);
			int maxfd= RSTDOUT[PIPE_READ];
			
			while(1)
			{
				struct timeval tvcopy = tv;
				FD_ZERO(&rfds); //очистить набор
				//FD_ZERO(&wfds); //очистить набор
				//FD_SET(RSTDIN[PIPE_WRITE], &wfds); //добавить дескриптор в набор
				FD_SET(RSTDOUT[PIPE_READ], &rfds); //добавить дескриптор в набор
				
				retval = select(maxfd+1, &rfds, NULL, NULL, &tvcopy);
				
				char *dtm = get_datetime();
				if(retval<0)
				{
					if(errno==EINTR) continue; //если выход из селекта из-за сигнала
					perror("Call select() failed");
					exit(EXIT_FAILURE);
				}
				else if(retval>0)
				{
					//if(FD_ISSET(RSTDOUT[PIPE_READ], &rfds))
					{
						//printf("outp\n");
						handle_output(RSTDOUT[PIPE_READ],dtm);
					}
					//if(FD_ISSET(RSTDIN[PIPE_WRITE], &wfds))
					{
						handle_input(RSTDIN[PIPE_WRITE],dtm);
					}
					
				}
				else
				{
					char buf[70];
					strcpy(buf,dtm);
					strcat(buf," NOIO\n");
					write(cmd->logfile_descr,buf,strlen(buf));
				}				
				
				free(dtm);
				pthread_mutex_lock(&(child_dead));
				int is_dead = child_dead_var;
				pthread_mutex_unlock(&(child_dead));
				if(child_dead_var == 1) break;
			}
			close(RSTDIN[1]);
			exit(EXIT_SUCCESS);
			break;
		};
	}
}
