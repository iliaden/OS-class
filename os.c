#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
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
char *parse_path(char*curr,char * path, char* to);
char *trim(char *str);


char curr_path[MAXPATHLEN];
char ** dir_stack;
char * path_buf;
int used_dir_stack =0;
int ii;
char host[256];
FILE *history;


int split_str( char * string, char ** to, int * redirections )
{

    char quote='\0';
    char currword[MAXCMDLEN];
    int offset=0, position=0;
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

    *redirections=-1;
    return wordcount;
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
    
    char * hist_path = getenv("HOME");
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
/*	else
	{
	    int ii;
	    for (ii=0; ii< words_count;ii++)
		printf("%d\t[%s]\n",ii,words[ii]);
	    printf("redirections:\n");
	    for (ii=0; redirections[ii]>=0;ii++)
		printf("%d\t[%d]\n",ii,redirections[ii]);
	} 	
*/
	if ( cptr != NULL)
	{
	    if (strncmp("exit", buf, 4) ==0)
		break;
	    else if (strncmp("echo", buf, 4) ==0)
	    {
		//TODO: add formatting?
		tmp[0]='\0';
		strncpy (tmp, buf+5, sizeof(buf)-5);
		printf("%s\n",tmp); 
	    }
	    else if (strncmp("history", buf, 7) ==0)
	    {//only print
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
	    }
	    else if (strncmp("pwd", buf, 3) ==0)
		printf("%s\n", curr_path);
	    else if (strncmp("pushd", buf, 5) ==0)
	    {
		if ( used_dir_stack == 99)
		    printf("Cannot push directory onto stack: out of stack space");
		else
		{   
		    char temp[MAXCMDLEN];
		    parse_path(curr_path, words[1], temp);
		    dir_stack[used_dir_stack] = temp;
		    used_dir_stack ++;
		}
	    }
	    else if (strncmp("popd", buf, 4) ==0)
	    {
		if ( used_dir_stack == 0)
                    printf("Nothing to pop.");
                else
                {
		    used_dir_stack--;
		    chdir(dir_stack[used_dir_stack]);
		    dir_stack[used_dir_stack] = NULL;
		    if (!GetCurrDir(curr_path, sizeof(curr_path) ))
			return errno;
//		    curr_path[sizeof(curr_path)-1] = '\0'; //to be sure
		}
	    }
	    else if (strncmp("cd", buf, 2) ==0)
	    {
		int result;
		tmp[0]='\0';
                strcpy (tmp, buf+3 );
		trim(tmp);
		char to[MAXPATHLEN];
	        parse_path(curr_path,tmp, to);
//		char * new_path = parse_path(curr_path,tmp);
		if ((result = chdir(to)) != 0)
		    printf("error occured changing into %s : %d", tmp, result);
		
		else 
		{
		    if (!GetCurrDir(curr_path, sizeof(curr_path) ))
			return errno;
		    curr_path[sizeof(curr_path)-1] = '\0'; //to be sure
		}
		
	    }
	    else
	    {
		printf("bypassing....");
		int result = system(buf);
	    }
	    
	    //execute it (system for now)	
	}
	else
	    ;
    }

}

void prompt()
{
    printf("[%s %s]$ ",host, curr_path);
}

char* parse_path(char * curr, char * path, char* to)
{
    char buffer[2*MAXPATHLEN];
    trim(path);
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
