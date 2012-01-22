#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#ifdef WINDOWS
    #include <direct.h>
    #define GetCurrDir _getcwd
#else
    #include <unistd.h>
    #define GetCurrDir getcwd
#endif

#define MAXPATHLEN 1000
#define MAXCMDLEN 2000
#define MAXWORDS 128


void prompt();
char *parse_path(char*curr,const char * path, char* to);
char *trim(char *str);
int cd(const char * path);
int echo_func(const char *name, const char * args[]);
int pwd_func(const char *name, const char * args[]);
int cd_func(const char *name, const char * args[]);
int popd_func(const char *name, const char * args[]);
int pushd_func(const char *name, const char * args[]);
int env_func(const char *name, const char * args[]);
int set_func(const char *name, const char * args[]);
int unset_func(const char *name, const char * args[]);
int exit_func(const char *name, const char * args[]);
int history_func(const char *name, const char * args[]);
int split_str( char * string, char ** to, int * redirections );



char curr_path[MAXPATHLEN];
char ** dir_stack;
char * path_buf, *hist_path;
int used_dir_stack =0;
int last_code=0;
char host[256];
FILE *history;

int cd(const char * path)
{
    int result;
    if ((result = chdir(path)) != 0)
    {
	printf("error occured changing into %s : %d", path, result);
	return -1;
    }

    else
    {
	if (!GetCurrDir(curr_path, sizeof(curr_path) ))
            return errno;
        curr_path[sizeof(curr_path)-1] = '\0'; //to be sure
    }
    return 0;
}

int pwd_func(const char *name, const char * args[])
{
    printf("%s\n", curr_path);
}

int echo_func(const char *name, const char * args[])
{
    int options =0;
    int escape=0;
    int newline =1;
    while ( args[options] != NULL)
    {
	if (strcmp(args[options], "-E" ) == 0)
	    escape = 0;
	else if (strcmp(args[options], "-e" ) == 0)
	    escape = 1;
	else if (strcmp(args[options], "-n" ) == 0)
	    newline = 0;

        else if ( !escape){
	    printf(">%s ",args[options]);
	}    
	else
	{
	    //we need to escape the characters...
	    printf(">%s ",args[options]); //FIXME: escape...
	}

	options++;
    }
    if ( newline)
	printf("\n");
    return 0;
}
int cd_func(const char *name, const char * args[])
{
    return cd(args[0]);
}
int popd_func(const char *name, const char * args[])
{
    if ( used_dir_stack == 0)
        printf("Nothing to pop.");
    else
    {
        cd (dir_stack[--used_dir_stack]);
        dir_stack[used_dir_stack] = NULL;
    }
    
}
int pushd_func(const char *name, const char * args[])
{
    if ( used_dir_stack == 99)
        printf("Cannot push directory onto stack: out of stack space");
    else
    {   
        char temp[MAXCMDLEN];
        parse_path(curr_path, args[0], temp);
        dir_stack[used_dir_stack++] = strdup(curr_path);
	cd (temp);
    }
    return 0;
}
int env_func(const char *name, const char * args[])
{
    return 0;
}
int set_func(const char *name, const char * args[])
{
    return 0;
}
int unset_func(const char *name, const char * args[])
{
    return 0;
}
int exit_func(const char *name, const char * args[])
{
    exit(0);
    return 0;
}
int history_func(const char *name, const char * args[])
{
    FILE * hist = fopen(hist_path, "r");
    if ( hist != NULL )
    {
        char line [ 1000 ]; 
        int line_count=0;
        while ( fgets ( line, sizeof line, hist ) != NULL )
        {
	   line_count++;
	    fprintf (stdout, " %d  %s",line_count,line );
	}
        fclose ( hist );
    }
    return 0;
}

typedef int (*FUNCTION)(const char * arg0, const char * args[]);
struct {
    const char * name;
    FUNCTION function;
}commands[] = {
    {"echo", echo_func},
    {"pwd", pwd_func},
    {"cd", cd_func},
    {"popd", popd_func},
    {"pushd", pushd_func},
    {"env", env_func},
    {"set", set_func},
    {"unset", unset_func},
    {"history", history_func},
    {"exit", exit_func},
    {NULL, NULL}
};

int split_str( char * string, char ** to, int * redirections )
{

    char quote='\0';
    char currword[MAXCMDLEN];
    int position=0;
    int wordlen=0, wordcount=0;
    
    for(position=0; string[position] != '\0' ; position ++)
    {
	switch ( string[position])
	{
	case ' ':
	case '\t':
	    if ( wordlen > 0)
	    {
		currword[wordlen]='\0';
	        to[wordcount++] = strdup(currword);
		wordlen=0;
	    } 
	    break;

	case '|':
	case '>':
	case '<':
	    if ( wordlen > 0)
            {
                currword[wordlen]='\0';
                to[wordcount++] = strdup(currword);
                wordlen=0;
            }
	    currword[0]=string[position];currword[1]='\0';
	    to[wordcount] = strdup(currword);
	    *redirections++=wordcount++;

	    //if previous word is also a pipe, return an error
	    if ( (wordcount>1 && (redirections[-1] == redirections[-2]+1 )) 
		 || wordcount ==1)
		return -1;
	    break;
	    
/*	case '\\':
	    if (string[position+1] =='\0')
		return -1;
	    currword[wordlen++] = string[++position];
	    break;
*/
	case '"':
	case '\'':
	    quote=string[position];
	    while ( string[position+1] != quote && string[position +1 ] != '\0')
	    {
		currword[wordlen++] = string[++position];
	    }
	    if (string[position +1 ] == '\0')
	    {	//quotation marks mismatch
		return -1;
	    }
	    position++;
	    break;

	default:
	    currword[wordlen++] = string[position];
	    break;

	}
	
    }
    currword[wordlen]='\0';
    to[wordcount++] = strdup(currword);
    to[wordcount] = NULL;
    *redirections=-1;
    return wordcount;
}

FUNCTION find_local_func(const char *word)
{
    int ii=0;
    for (ii=0; commands[ii].name != NULL; ii++)
    {
	if ( strcmp(commands[ii].name, word) == 0)
	    return commands[ii].function;
    }
    return NULL;
}

int exec_command( char ** words, const int *redirections)
{
    if ( strcmp(words[0], "exit") ==0) exit(0);
    FUNCTION funcptr = find_local_func(words[0]);
    int pc[2],cp[2];
    if( pipe(pc) < 0)
    {
	perror("Can't make pipe");
	exit(1);
    }
    if( pipe(cp) < 0)
    {
	perror("Can't make pipe");
	exit(1);
    }	

    int pid = fork();
    if (pid == -1){
	printf ("Fork Failed\n");
	return (-1);
    }
    else if (pid == 0)
    {
	//attach parent out to self in
	if (*redirections >=0 )
	{
	    switch (* (words[*redirections]) )
	    {
		case '<':
		{
		    fprintf(stderr, "encountered <\n");
		    char *filepath = words[*redirections + 1];
		    FILE *file = fopen(filepath, "r");
		    close(pc[0]);
		    close(0);
		    dup(fileno(file));
		    //TODO: can we close the file here
		}
		    break;
		case '>':
		{
		    fprintf(stderr, "encountered >\n");
		    char *filepath = words[*redirections + 1];
		    FILE *file = fopen(filepath, "w");
		    close(pc[1]);
		    close(1);
		    dup(fileno(file));
		    //TODO: can we close the file here
		}
		    break;
		case '|':
		    break;
		default:
		    break;
		    //crap
	    }
	}
	words[*redirections] = NULL;
	if (funcptr != NULL ) 
	{
	    exit( funcptr(words[0], words+1));
	}
	execvp(words[0], words);
	
    }
    else
    {
	int status;
	waitpid(pid, &status, 0);
	return WEXITSTATUS(status);
//	wait for child to die.
    }
    
/*    if (funcptr == 0 || redirections[0] >= 0 )//TODO: verify equlity
    { //external command

	//external
	if( pipe(pc) < 0)
	{
	    perror("Can't make pipe");
	    exit(1);
	}
	if( pipe(cp) < 0)
	{
	    perror("Can't make pipe");
	    exit(1);
	}	
	int pid = fork();
	if ( pid == 0)
	{ //child
	    if (*redirections >=0)
	    {
		return exec_command(word+*redirections, redirections+1);
	    }
	}
	else
	{   //parent

	}
    }
    else return funcptr(words[0], words+1);*/
}


int main(int argc, char *argv[], char *envp[])
{
    dir_stack = (char **)malloc(sizeof(char *)*100);
    char * tmp  = (char *) malloc(sizeof(char) * MAXPATHLEN);
    path_buf = (char*) malloc(sizeof(char)*MAXPATHLEN);
    host[0]='\0';
    char * host_tmp = getenv("USER");
    strcat(host,host_tmp);
/*    host[strlen(host)]='@'; host[strlen(host)]='\0';
    FILE* rcConf = fopen("/etc/rc.conf","r");
    char line[512];
    while ( fgets ( line, sizeof line, rcConf ) != NULL )
	if ( strncmp(trim(line),"HOSTNAME", 8) == 0)
	    strcat(host,trim(line)+10);*/
//    host[strlen(host)-1]='\0';
//    fclose ( rcConf );
    
    hist_path = getenv("HOME");
    strcat(hist_path, "/.my_shell_hist");
//    for (ii=0;ii<100;ii++)
//	dir_stack[ii]=(char*)malloc(sizeof(char)*MAXPATHLEN);

    if (!GetCurrDir(curr_path, sizeof(curr_path) ))
	return errno;
    curr_path[sizeof(curr_path)-1] = '\0'; //to be sure
    
    //loop code
    while(1)
    {
	prompt();
	//get command
	char *cptr;
	char buf[MAXCMDLEN];
	
	cptr = fgets(buf, MAXCMDLEN, stdin);
	if (cptr == NULL) break;
	trim(buf);
	//add it to history
	history = fopen( hist_path, "a");
	if (buf[0]!='\0' && buf[1] !='\0')
	    fprintf(history, "%s\n",buf);
	fclose(history);

	char *words[MAXWORDS];
	int redirections[MAXWORDS];
	int words_count = split_str(buf, words, redirections);
	if (words_count<0) 
	    printf("Invalid syntax: [%s].\n",buf);
	
	last_code=exec_command(words, redirections);
    }

    return 0;
}

void prompt()
{
    printf("[%s %s]$ ",host, curr_path);
}

char* parse_path(char * curr, const char * path, char* to)
{
    char buffer[2*MAXPATHLEN];
    if (path[0]=='/')
    {
	strcpy(to, path);
	return to;
    }
    strcpy(buffer,curr);
    strcat(buffer,"/");
    strcat(buffer,path);
    char *dirs[MAXPATHLEN];
    char *dirp;
    int dirind=0;
    for(dirp=strtok( buffer, "/"); dirp != NULL; dirp = strtok(NULL, "/")) 
    {
	dirs[dirind++] = dirp;
    }
    dirs[dirind]=NULL;
    char *target[MAXPATHLEN];
    int targetind=0;
    for (dirind=0;dirs[dirind]!=NULL;dirind++)
    {
	if (strcmp(dirs[dirind], ".") ==0 )
	    ; //do nothing
	else if (strcmp(dirs[dirind],"..") == 0)
	{
	    if (targetind > 0)
		//remove previous entry in target
		target[targetind--] = NULL;
	}
	else
	    target[targetind++] = dirs[dirind];
    }
    target[targetind]=NULL;

    *to='\0'; 
    for (dirind=0;target[dirind]!=NULL;dirind++)
    {
	strcat(to,"/");
	strcat(to,target[dirind]);
    }
    return to;
}

char *trim(char *str)
{
    char *end;
    // Trim leading space
    while(isspace(*str)) str++;
    if(*str == 0)  // All spaces?
	return str;
    // Trim trailing space
    end = str + strlen(str) - 1;
    while(end > str && isspace(*end)) end--;
	*(end+1) = 0;
    return str;
}
