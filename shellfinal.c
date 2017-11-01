# include<stdio.h>
# include<stdlib.h>
# include<unistd.h>
# include<sys/wait.h>
# include<sys/types.h>
#include<termios.h>
# include<fcntl.h>
# include<signal.h>
# include<string.h>
# include<errno.h>
# include<pwd.h>
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#define PATH_MAX 1024
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
extern char** environ;
int status= 0; // for echo $?
struct termios curterm;
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void print_error(char* prog,char* cmd){
	printf("shell: %s: %s: %s\n",prog,cmd,strerror(errno));
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void print_launch_error(char* prog){
	printf("shell: %s: %s\n",prog,strerror(errno));
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void prompt(){
	struct passwd *info;
	char* pwd= NULL;
	int len= 0;
	uid_t uid= getuid();
	info= getpwuid(uid);
	pwd= getcwd(pwd,PATH_MAX);
	len= strlen(info->pw_dir);
	while(len-->0)
		pwd++;
	printf("\x1b[36m%s\x1b[0m:\x1b[36m~%s\x1b[0m$ ",info->pw_name,pwd);
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void change_term_mode()
{
	struct termios new_term;
 	tcgetattr(STDIN_FILENO,&new_term);
	new_term.c_lflag &= ~(ICANON | ECHO);	// local mode flags: setting to non-canonical mode and disabling echo...
	new_term.c_lflag |= ISIG;
	new_term.c_oflag &= ~OLCUC;
	new_term.c_oflag |= ONLRET;
	new_term.c_cc[VMIN]= 1;
	new_term.c_cc[VTIME]= 0;
	tcsetattr(STDIN_FILENO,TCSANOW,&new_term);
}
void restore_term_mode()
{
	tcsetattr(STDIN_FILENO,TCSANOW,&curterm);
}
void save_term_mode()
{
	tcgetattr(STDIN_FILENO,&curterm);
	atexit(restore_term_mode);
}
//~~~~~~~~~FUNCTION TO CHANGE ENV TO THEIR VALUES IN A STRING~~~~~~~~~~~~~~~~~~~~
char *replace_envs(char* line){
	int size= 1024;
	char *str= NULL,*token= NULL,*temp= NULL,*back= NULL,*newstr= NULL;
	if(strlen(line)>1&&(temp= index(line,'$')) != NULL){		// checking for environment variables to be replaced
		newstr= (char*)malloc(sizeof(char)*size);
		str= line;
		token= strtok_r(str,"$",&back);
		if(temp==line){
			temp= getenv(token);			
			if(temp!=NULL)
				newstr= strcat(newstr,temp);
		}
		else
			newstr= strcat(newstr,token);
		for (str= NULL;;str= NULL){
			token= strtok_r(str,"$",&back);
		    if(token==NULL)
		       	break;
			temp= getenv(token);
			newstr= strcat(newstr,temp);
		}
		return newstr;
	}
	return line;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 
char** break_line(char* line,char* delim){							// To break the line into arguments or words seperated by spaces
	int size= 10,i= 0;
	char *token= NULL, *backup= NULL, **command= (char**)malloc(sizeof(char*)*size);
	command[0]= NULL;
	if(line==NULL||line[0]=='\0')
		return command;
	for (line;;line= NULL){
		token= strtok_r(line,delim,&backup);
		
		if (token==NULL){
			command[i++]= NULL;
			break;
		}
		token= replace_envs(token);
		command[i++]= token;
		if(i==size){
			size*= 2;
			command= (char**)realloc(command,sizeof(char*)*size);
		}
	}
	if(i<size)
		command= (char**)realloc(command,sizeof(char*)*i);
	return command;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ To read a line till user presses enter button
char* read_line(){
	int i= 0,j= 0,buffLen= 100, fd[2]= {0};// fd is used for pipe as well as fd for other files
	pid_t pid= -1;
	FILE *tabfile;
	char c= -1, *line= (char*)malloc(sizeof(char)*buffLen),*tab= (char*)malloc(sizeof(char)*10),special[2]= {0}, *temp= NULL;
	tab[0]= '^';
	prompt();
	while(c!='\n'){
		c= getc(stdin);
		switch(c){
			case -1:
				continue;//loop again
			case 9: //tab
				pid= fork();					
				if(pid==0)
				{
					pipe(fd);
					if((pid=fork())==0)
					{
						dup2(fd[1],STDOUT_FILENO);
						close(fd[1]);
						close(fd[0]);
						execlp("ls","ls",NULL);
						exit(0);
					}
					waitpid(pid,&status,WUNTRACED);
					if((pid=fork())==0)
					{
						dup2(fd[0],STDIN_FILENO);
						close(fd[0]);
						close(fd[1]);
						fd[1]= open("tab.txt",O_WRONLY | O_CREAT | O_TRUNC,
							S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
						tab= strcat(tab,line);
						dup2(fd[1],STDOUT_FILENO);
						close(fd[1]);
						execlp("grep","grep",tab,NULL);
						exit(0);
					}
					close(fd[1]);
					close(fd[0]);
					wait(NULL);					
					_exit(0);
				}
				waitpid(pid,&status,WUNTRACED);
				tabfile= fopen("tab.txt","r");
				j= getline(&tab,(size_t*)&j,tabfile);
				if(j>0){
					while(i-->0)
						printf("\b \b");
					line= strcpy(line,tab);
					i= strlen(line);
					line[i-1]= 0;
					printf("%s",line);
				}
				continue;
			case 10: //enter
				printf("\n");
				continue;//loop again
			case 12: //clear screen (ctrl+l)
				pid= fork();
				if(pid==0)
				{
					execlp("clear","clear",NULL);
					exit(0);
				}
				wait(NULL);
				prompt();
				continue;//loop again
			case 27: //arrow keys
				read(STDIN_FILENO,special,2);
				continue;
			case 127: //backspace
				if(i>0){
					printf("\b \b");
					line[i--]= 0;
				}
				continue;
			default:
				printf("%c",c);
				break;
		}
		line[i++]= c;
		if(i==buffLen){
			buffLen+= buffLen;
			line= (char*)realloc(line,sizeof(char)*buffLen);
		}
	}
	if(i>0)
		line[i]= '\0';
	return line;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void handler(int signal){						// signal handler for shell
	pid_t pid= -1;
	switch(signal){
		case SIGINT:
		case SIGTSTP:
			printf("\n");
			break;
		case SIGCHLD:
			if(WIFEXITED(status))
				status= WEXITSTATUS(status);
			return;
	}
	prompt();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void exe_echo(char** args){							// execution of ECHO command
	char *str= NULL,*token= NULL,*temp= NULL,*back= NULL;
	if(args[1]==NULL)
		printf("\n");
	else{
		int i= 1;
		while(args[i]!=NULL)
			printf("%s ",args[i++]);
		printf("\n");
	}
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/*void executeScript(char * argv[])
{
	char * linePtr= NULL, ** argsList= NULL;
	FILE *stream;
	char *line = NULL;
	size_t len = 0;
	ssize_t read;
	char ** args= NULL;

	stream = fopen(argv[1], "r");
	if (stream == NULL)
		exit(EXIT_FAILURE);
	else
		printf("FILE OPENED\n");		
	if ((read = getline(&line, &len, stream)) != -1) {
		line[read-1]= 0;
		if(read > 2)
		{
			if(!(line[0]=='#' && line[1]=='!')){
				printf("in: %s\n",line);
				executePipes(line);
			}
			else
			{
				args= parseInput(&line[2]," ");
				printf("gawar: %s\n",args[0]);
				if(strcmp(args[0],argv[0])!=0)
					execvp(args[0],argv);
			}
		}	
				
	}
	while ((read = getline(&line, &len, stream)) != -1) {		
		if(line[read-1]=='\n')
			line[read-1]= 0;		
		executePipes(line);
	}

	free(line);
	fclose(stream);
	exit(EXIT_SUCCESS);
}*/
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
int launch_command(char** command,int readfd,int writefd){
	pid_t pid= -1,wpid= -1;
	sigset_t block, old;
	int fd= -1,i= 0,len= 0;
	struct sigaction newact;
	char** tempcmd= command;
	if(strcmp(command[0],"exit")==0)
		return 0;
	pid= fork();
	switch(pid){
		case 0:
			if(readfd!=STDIN_FILENO){
				dup2(readfd,STDIN_FILENO);
				close(readfd);
			}
			if(writefd!=STDOUT_FILENO){
				dup2(writefd,STDOUT_FILENO);
				close(writefd);
			}
			newact.sa_handler= SIG_DFL;
			sigaction(SIGINT,&newact,NULL);
			sigaction(SIGTSTP,&newact,NULL);
			while(command[i]){
				if(strcmp(command[i],">")==0){
					fd= open(command[i+1],O_WRONLY | O_CREAT | O_TRUNC,
							S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
					dup2(fd,STDOUT_FILENO);
					close(fd);
					command[i]= NULL;
					break;
				}
				else if(strcmp(command[i],">>")==0){
					fd= open(command[i+1],O_WRONLY | O_CREAT | O_APPEND,
							S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
					dup2(fd,STDOUT_FILENO);
					close(fd);
					command[i]= NULL;
					break;
				}
				else if(strcmp(command[i],"<")==0){
					fd= open(command[i+1],O_RDONLY);
					if(fd==-1){
						print_launch_error(command[i+1]);
						exit(EXIT_FAILURE);
					}
					dup2(fd,STDIN_FILENO);
					close(fd);
					command[i]= NULL;
					break;
				}
				i++;
			}
			if(strcmp(command[0],"echo")==0){
				exe_echo(command);
				exit(EXIT_SUCCESS);
			}
			else{
				if(strcmp(command[0],"ls")==0||strcmp(command[0],"grep")==0){
					while(command[len]!=NULL){
						if(strstr(command[len++],"--color"))
							break;
					}
					if(command[len]==NULL){
						command= (char**)realloc(command,sizeof(char*)*len+1);
						command[len]= "--color=auto";
						command[len+1]= NULL;
					}
				}
				execvp(command[0],command);
			}
			print_launch_error(command[0]);			//exec failed
			exit(EXIT_FAILURE);
			break;
		case -1:
			printf("Getting -1\n");
			print_launch_error(command[0]);
			break;
		default:
			close(readfd);
			wpid= waitpid(pid,&status,WUNTRACED);
			break;
	}
	return 1;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
int execute_command(char **cmd){						// executing a shell command
	int i= 0, readfd= STDIN_FILENO;
	uid_t uid= -1;
	struct passwd* info;
	int pipefd[2],parent[2];
	char** command= NULL;
	struct sigaction intold, stpold, newact;
	if(cmd[0]==NULL)
		return 1;
	if(strstr(cmd[0],"cd")){
		command= break_line(cmd[0]," ");
		if(strcmp(command[0],"cd")==0){
			if(!command[1]){
				uid= getuid();
				info= getpwuid(uid);
				chdir(info->pw_dir);
			}
			else if(chdir(command[1])==-1)
				print_error(command[0],command[1]);
			return 1;
		}
	}
	if(pipe(parent)==-1){
		print_launch_error(cmd[0]);
		return 1;
	}
	newact.sa_handler= SIG_IGN;
	sigaction(SIGINT,&newact,&intold);
	sigaction(SIGTSTP,&newact,&stpold);
	if(fork()==0){
		restore_term_mode();
		while(cmd[i+1]){
			if(pipe(pipefd)==-1){
				print_launch_error(cmd[0]);
				return 1;
			}
			command= break_line(cmd[i]," ");
			launch_command(command,readfd,pipefd[1]);
			readfd= pipefd[0];
			close(pipefd[1]);
			i++;
		}
		command= break_line(cmd[i]," ");
		i= launch_command(command,readfd,STDOUT_FILENO);
		close(parent[0]);
		write(parent[1],&i,sizeof(int));
		close(pipefd[0]);
		exit(EXIT_SUCCESS);
	}
	else{
		wait(NULL);
		close(parent[1]);
		read(parent[0],&i,sizeof(i));
	}
	sigaction(SIGINT,&intold,NULL);
	sigaction(SIGTSTP,&stpold,NULL);
	return(i);
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
int main(int argc, char* argv[], char* envp){
	char* line= NULL;
	char** command= NULL;
	int status= 1,i=0;
	struct sigaction sa;
	sa.sa_handler= handler;
	if(sigaction(SIGINT,&sa,NULL)==-1||sigaction(SIGTSTP,&sa,NULL)==-1){
		printf("%s: failed to register handler.\n",argv[0]);
		exit(EXIT_FAILURE);
	}
	sa.sa_flags= SA_NOCLDWAIT;
	if(sigaction(SIGCHLD,&sa,NULL)==-1){
		printf("%s: failed to register handler.\n",argv[0]);
		exit(EXIT_FAILURE);
	}
	save_term_mode();
	while(status){
		change_term_mode();
		line= read_line();
		command= break_line(line,"|");	
//		while(command[i])
//			printf("%s\n",command[i++]);
		status= execute_command(command);
		free(line);
		free(command);
		i= 0;
	}
	exit(EXIT_SUCCESS);
}
