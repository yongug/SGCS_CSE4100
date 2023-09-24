/* $begin shellmain */
#include "csapp.h"
#include<errno.h>

#define MAXARGS   128

/* Function prototypes */
void eval(char *cmdline);
int parseline(char *buf, char **argv);
int builtin_command(char **argv); 

/*variables for execv & directory function*/
char path[MAXLINE];
char bin[MAXLINE] = "/bin/";
char usr_path[MAXLINE] = "/usr/bin/";

/*variables for history & ! command*/
char* buffer;
int buffer_size;
int buffer_count;

/*variables for File*/
FILE* fp; // main pointer
FILE* fp2; // read pointer
FILE* fp3;

int line = 0;
int line_2 = 0; // garbage line_value
char last_command[MAXLINE];

int main() 
{
    char cmdline[MAXLINE]; /* Command line */
    char c;

    fp = fopen("historylog.txt","a+");
    
    while(fscanf(fp,"%c",&c)!=EOF)
    {
        if(c=='\n')
        {
            line++;
        }
    }
    //printf("line: %d\n",line);
    //fclose(fp2);
    fseek(fp,0,SEEK_SET);

    //fp3 = fopen("historylog.txt","a+");
    
    for(int i=0;i<line;i++)
    {
        fscanf(fp,"%d %[^\n]",&line_2,&last_command);
    }
    strcat(last_command,"\n");

    //printf("last command: %s",last_command);
    

    while (1) {
	/* Read */
	printf("CSE4100-MP-P1> ");                   
	fgets(cmdline, MAXLINE, stdin);
    
    //command insert into historylog.txt
    /*
        line++;
        fprintf(fp,"%d %s",line,cmdline);
    */

	if (feof(stdin))
    {
        fclose(fp);
        exit(0);

    }
        
	/* Evaluate */
	eval(cmdline);
    }
    
}
/* $end shellmain */
  
/* $begin eval */
/* eval - Evaluate a command line */
void eval(char *cmdline) 
{
    char *argv[MAXARGS]; /* Argument list execve() */
    char buf[MAXLINE];   /* Holds modified command line */
    int bg;              /* Should the job run in bg or fg? */
    pid_t pid;           /* Process id */
    
    int builtin_flag;
    
    int temp_number;

    if(cmdline[0]=='!')
    {
        char history_argv[MAXLINE];
        char temp_cmd[MAXLINE];

        if(cmdline[1]=='!')
        {
            //fp2 = fopen("historylog.txt","a+");
            fseek(fp,0,SEEK_SET);
            for(int i=0;i<line;i++)
            {
                fscanf(fp,"%d %[^\n]\n",&temp_number,&history_argv);
            }
            //fclose(fp);
            
            if(temp_number>line)
            {
                printf("bash: !%d: event not found\n",temp_number);
                return;
            }

            char *ptr;

            if(ptr=strstr(cmdline," "))
            {
                ptr++;
                strcpy(temp_cmd,ptr);
                strncpy(cmdline,history_argv,sizeof(history_argv));
                strcat(cmdline," ");
                strcat(cmdline,temp_cmd);
            }
            else{
                strncpy(cmdline,history_argv,sizeof(history_argv));
                strcat(cmdline,"\n");
            }
            
            
        }
        else
        {
            int line_number = atoi(&cmdline[1]);
            
            //fp2 = fopen("historylog.txt","a+");
            fseek(fp,0,SEEK_SET);
            for(int i=0;i<line_number;i++)
            {
                fscanf(fp,"%d %[^\n]\n",&temp_number,&history_argv);
            }
            //fclose(fp);
        
            if(line_number>line)
            {
                printf("bash: !%d: event not found\n",line_number);
                return;
            }
            //printf("%s ", history_argv);
            char* ptr;


            if(ptr=strstr(cmdline," "))
            {
                ptr++;
                strcpy(temp_cmd,ptr);
                strncpy(cmdline,history_argv,sizeof(history_argv));
                //printf("tc:%s",temp_cmd);
                strcat(cmdline," ");
                strcat(cmdline,temp_cmd);
            }
            else{
                strncpy(cmdline,history_argv,sizeof(history_argv));
                //printf("cmdline:%s",cmdline);
                strcat(cmdline,"\n");
            }

        }
        printf("%s",cmdline);
    }
    if(!strncmp(cmdline,"history\n",8))
    {
        //fp2 = fopen("historylog.txt","a+");
        if(strcmp(cmdline,last_command))
        {
            //fp = fopen("historylog.txt","a+");
            fseek(fp,0,SEEK_END);
            line++;
            fprintf(fp,"%d %s",line,cmdline);
            strcpy(last_command,cmdline);
            //fclose(fp);
        }
        
        fseek(fp,0,SEEK_END);
        buffer_size = ftell(fp);

        buffer = malloc(sizeof(char)*(buffer_size+1));
        memset(buffer,0,buffer_size+1);

        fseek(fp,0,SEEK_SET);
        buffer_count = fread(buffer,1,buffer_size,fp);
        printf("%s",buffer);
        free(buffer);
        
        //fclose(fp);
        

        return;
    }
    
    /* compare and record the history*/
    if(strcmp(cmdline,last_command))
    {

        //fp = fopen("historylog.txt","a+");
        fseek(fp,0,SEEK_END);
        line++;
        fprintf(fp,"%d %s",line,cmdline);
        strcpy(last_command,cmdline);
        //fclose(fp);
    }
    

    strcpy(buf, cmdline);
    strcpy(last_command,cmdline);
    bg = parseline(buf, argv); 
    if (argv[0] == NULL)  
	return;   /* Ignore empty lines */

    builtin_flag = builtin_command(argv);


    if (!builtin_flag) { //quit -> exit(0), & -> ignore, other -> run
        if((pid=Fork())==0)
        {
            strcpy(&bin[5],argv[0]);
            if(argv[0][0] =='.' && argv[0][1] == '/')
            {
                getcwd(path,MAXLINE);
                if(execve(strcat(path,&argv[0][1]),argv,environ)<0)
                {
                    fprintf("%s : Command not found.\n",argv[0]);
                    //fprintf(stderr,"error : %s - %s\n",&argv[0][2], strerror(errno));
                    exit(0);
                }
            }
            else if(execve(bin,argv,environ)<0)
            {
                //strcpy(&usr_path[9],argv[0]);
                if(execve(strcat(usr_path,argv[0]),argv,environ)<0)
                {
                    fprintf("%s : Command not found.\n",argv[0]);
                }
                exit(0);
            }
            
        }

       
	/* Parent waits for foreground job to terminate */
	if (!bg){ 
        // pdf added
	    int status;
        if(waitpid(pid,&status,0)<0) unix_error("waitfg: waitpid error");
	}
	else//when there is backgrount process!
	    printf("%d %s", pid, cmdline);
    
    }

    
    return;
}

/* If first arg is a builtin command, run it and return true */
int builtin_command(char **argv) 
{
    if (!strcmp(argv[0], "quit"))
    {
        fclose(fp);
        exit(0);
    } /* quit command */
	
    //temporary exit process
    if(!strcmp(argv[0],"exit"))
    {
        fclose(fp);
        exit(0);
    }
    if (!strcmp(argv[0], "&"))    /* Ignore singleton & */
	return 1;
    if(!strcmp(argv[0],"cd")) {
        if(chdir(argv[1])<0)
        {
            Sio_puts("Changing directory is failed\n");
           
        }
        return 1;
     }

    return 0;                     /* Not a builtin command */
}
/* $end eval */

/* $begin parseline */
/* parseline - Parse the command line and build the argv array */
int parseline(char *buf, char **argv) 
{
    char *delim;         /* Points to first space delimiter */
    int argc;            /* Number of args */
    int bg;              /* Background job? */

    buf[strlen(buf)-1] = ' ';  /* Replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* Ignore leading spaces */
	buf++;

    /* Build the argv list */
    argc = 0;
    while ((delim = strchr(buf, ' '))) {
	argv[argc++] = buf;
	*delim = '\0';
	buf = delim + 1;
	while (*buf && (*buf == ' ')) /* Ignore spaces */
            buf++;
    }
    argv[argc] = NULL;
    
    if (argc == 0)  /* Ignore blank line */
	return 1;

    /* Should the job run in the background? */
    if ((bg = (*argv[argc-1] == '&')) != 0)
	argv[--argc] = NULL;

    return bg;
}
/* $end parseline */


