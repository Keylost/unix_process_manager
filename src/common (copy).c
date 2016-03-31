#include "common.h"

#define buffer_size 2048 //размер буффера для чтения из потока вывода ребенка
pthread_mutex_t child_dead = PTHREAD_MUTEX_INITIALIZER;
volatile int child_dead_var = 0;
char reading_buf[buffer_size]; //буффер
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
	bytes =0;
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
			write(1, buf, bytes+len+1); // 1 -> stdout
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
		write(1, buf, bytes+len+1); // 1 -> stdout
		free(buf);
		bytes=0;
	}
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
	
	//int RSTDOUT[2]; //1-запись, 0 - чтение
	int RSTDIN[2]; //1-запись, 0 - чтение
	//int RSTDERR[2]; //1-запись, 0 - чтение
	if(pipe(RSTDIN)==-1)
	//if(pipe(RSTDOUT)==-1 || pipe(RSTDIN)==-1 || pipe(RSTDERR)==-1)
	{
		perror(NULL);
		exit(EXIT_FAILURE);
	}
	
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
			//close(RSTDOUT[0]); //ребенок не читает
			//close(RSTDERR[0]); //
			close(RSTDIN[1]); //ребенок не пишет
			//dup2(RSTDOUT[1], 1); //перенаправить stdout в pipe
			//dup2(RSTDERR[1], 2); //перенаправить stderr в pipe
			dup2(RSTDIN[0], 0); //перенаправить stdin в pipe
			close(STDIN_FILENO);
			char **argv;
			int argc=0;
			argv = string_to_argv(cmd->execute,&argc);
			
			execvp(argv[0], argv);
			perror("Call exec() failed!");
			exit(EXIT_FAILURE);
		};
		default: //батька
		{
			close(RSTDOUT[1]); //батька не пишет))
			close(RSTDERR[1]); //батька не пишет
			close(RSTDIN[0]); //батька не читает
			int is_dead = 0; //локальная копия флага смерти ребенка))
			int timeout = 1; //таймаут для select()
			fd_set rfds; //набор дескрипторов потоков с которых нужно ожидать чтение
			fd_set wfds; //на запись
			struct timeval tv; //таймауты для select()
			int retval;
			FD_ZERO(&rfds); //очистить набор
			FD_ZERO(&wfds); //очистить набор
			FD_SET(RSTDOUT[0], &rfds); //добавить дескриптор в набор
			FD_SET(RSTDERR[0], &rfds); //добавить дескриптор в набор
			FD_SET(RSTDIN[1], &wfds); //добавить дескриптор в набор
			tv.tv_sec = timeout; //ждать максимум n секунд(после выхода из select()
			tv.tv_usec = 0; //значение таймаута будет сброшено)
			int maxfd = max(RSTDOUT[0],max(RSTDERR[0],RSTDIN[0]));
			
			while(1)
			{
				struct timeval tvcopy = tv;
				retval = select(maxfd+1, &rfds, &wfds, NULL, &tvcopy);
				
				char *dtm = get_datetime();
				printf("%d\n",retval);
				if(retval<0)
				{
					if(errno==EINTR) continue; //если выход из селекта из-за сигнала
					perror("Call select() failed");
					exit(EXIT_FAILURE);
				}
				else if(retval>0)
				{
					if(FD_ISSET(RSTDIN[1], &wfds))
					{
						printf("jj\n");
						//handle_output(RSTDOUT[0],dtm);
					}
					if(FD_ISSET(RSTDOUT[0], &rfds))
					{
						printf("hh\n");
						handle_output(RSTDOUT[0],dtm);
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
			close(RSTDOUT[0]);
			close(RSTDERR[0]);
			close(RSTDIN[1]);
			exit(EXIT_SUCCESS);
			break;
		};
	}
}
