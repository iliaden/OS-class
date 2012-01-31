#include <stdio.h>
#include <sys/wait.h>
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

typedef int ( *FUNCTION ) ( const char * args[] );

void prompt();
char *parse_path ( char*curr, const char * path, char* to );
char *trim ( char *str );
int cd ( const char * path );
int echo_func ( const char * args[] );
int pwd_func ( const char * args[] );
int cd_func ( const char * args[] );
int popd_func ( const char * args[] );
int pushd_func ( const char * args[] );
int env_func ( const char * args[] );
int set_func ( const char * args[] );
int unset_func ( const char * args[] );
int exit_func ( const char * args[] );
int history_func ( const char * args[] );
int split_str ( char * string, char ** to, int * redirections );
int contains_equal( char ** words );
FUNCTION find_local_func ( const char *word );
FUNCTION find_env_func ( const char *word );
int detect_pipes ( char ** words, const int * redirections );
int file_redirects ( char ** words, const int * redirections);
int exec_command ( char ** words, const int *redirections );

char ** dir_stack;
char ** environment;
char * hist_path;
int used_dir_stack = 0;
int last_code = 0;
char * host;

struct {
    const char * name;
    FUNCTION function;
} 
    commands[] = {
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
    },
    env_commands[] = {
        {"cd", cd_func},
        {"set", set_func},
        {"unset", unset_func},
        {"popd", popd_func},
        {"pushd", pushd_func},
        {"exit", exit_func},
        {NULL, NULL}
    };

int cd ( const char * path )
{
    if ( chdir ( path ) != 0 )
    {
        fprintf ( stderr, "error occured changing into [%s]\n", path );
        return -1;
    }
    else
    {
	char currpath[MAXPATHLEN];
        if ( !GetCurrDir ( currpath, sizeof ( currpath ) ) )
            return errno;
	setenv("PWD",currpath,1);
    }
    return 0;
}

int pwd_func ( const char * args[] )
{
    return fprintf ( stdout, "%s\n", getenv("PWD"));
}

int echo_func ( const char * args[] )
{
    int options = 0;
    int escape = 0;
    int newline = 1;
    int first=1;
    while ( args[options] != NULL )
    {
        if ( strcmp ( args[options], "-E" ) == 0 )
            escape = 0;
        else if ( strcmp ( args[options], "-e" ) == 0 )
            escape = 1;
        else if ( strcmp ( args[options], "-n" ) == 0 )
            newline = 0;
	else if ( strcmp ( args[options], "|" ) == 0 
		||strcmp ( args[options], ">" ) == 0
		||strcmp ( args[options], "<" ) == 0 )
	    break;
        else if ( !escape ) {
	    if (first)
	    {
		first=0;
		printf ( "%s", args[options] );
	    }
	    else
		printf ( " %s", args[options] );
        }
        else
        {
            //we need to escape the characters...
	    if (first)
	    {
		first=0;
		printf ( "%s", args[options] );
	    }
	    else
		printf ( " %s", args[options] );
        }
        options++;
    }
    if ( newline )
        printf ( "\n" );
    return 0;
}
int cd_func ( const char * args[] )
{
    return cd ( args[0] );
}
int popd_func ( const char * args[] )
{
    if ( used_dir_stack == 0 )
    {
        fprintf ( stderr, "Nothing to pop.\n" );
        return -1;
    }
    else
    {
        int retcode = cd ( dir_stack[--used_dir_stack] );
        dir_stack[used_dir_stack] = NULL;
        return retcode;
    }
    
}
int pushd_func ( const char * args[] )
{
    if ( used_dir_stack == 99 )
        fprintf ( stderr, "Cannot push directory onto stack: out of stack space" );
    else
    {
        char temp[MAXCMDLEN];
        parse_path ( getenv("PWD"), args[0], temp );
        dir_stack[used_dir_stack++] = strdup ( getenv("PWD") );
        cd ( temp );
    }
    return 0;
}
int env_func ( const char * args[] )
{ //FIXME: update unvironment if possible
    char ** env;
    for ( env = environment; *env != 0 ;env++)
    {
	char * this_env = *env;
	printf ("%s\n", this_env);
    }
    return 0;
}
int set_func ( const char * args[] )
{
    return putenv(strdup(args[0]));
}
int unset_func ( const char * args[] )
{
    return unsetenv( args[0] );
}
int exit_func ( const char * args[] )
{
    exit ( 0 );
    return -1;
}
int history_func ( const char * args[] )
{
    FILE * hist = fopen ( hist_path, "r" );
    if ( hist != NULL )
    {
        char line [ MAXCMDLEN ];
        int line_count = 0;
        while ( fgets ( line, sizeof line, hist ) != NULL )
        {
            fprintf ( stdout, " %d  %s", ++line_count, line );
        }
        fclose ( hist );
    }
    return 0;
}

int contains_equal( char ** words )
{ // returns 1 if first word has an equal sign. return 0 otherwise
    char * str = words[0];
    int ii;
    for (ii =0; ii< strlen(str);ii++)
    {
	if ( str[ii] == '=' )
	    return 1;
    }
    return 0;
}

int split_str ( char * string, char ** to, int * redirections )
{
    char quote = '\0';
    char currword[MAXCMDLEN];
    int position = 0;
    int wordlen = 0, wordcount = 0;
    for ( position = 0; string[position] != '\0' ; position ++ )
    {
        switch ( string[position] )
        {
        case ' ':
        case '\t':
            if ( wordlen > 0 )
            {
                currword[wordlen] = '\0';
                to[wordcount++] = strdup ( currword );
                wordlen = 0;
            }
            break;
        case '|':
        case '>':
        case '<':
            if ( wordlen > 0 )
            {
                currword[wordlen] = '\0';
                to[wordcount++] = strdup ( currword );
                wordlen = 0;
            }
            currword[0] = string[position];
            currword[1] = '\0';
            to[wordcount] = strdup ( currword );
            *redirections++ = wordcount++;
            //if previous word is also a pipe, return an error
            if ( ( wordcount > 1 && ( redirections[-1] == redirections[-2] + 1 ) )
                    || wordcount == 1 )
                return -1;
            break;
	/*case '\\':
		if (string[position+1] =='\0')
		    return -1;
		currword[wordlen++] = string[++position];
		break;
	*/
        case '"':
        case '\'':
            quote = string[position];
            while ( string[position + 1] != quote
                    && string[position + 1] != '\\'
                    && string[position + 1 ] != '\0' )
                currword[wordlen++] = string[++position];

            if ( string[position + 1 ] == '\\' )
            {
                currword[wordlen++] = string[position + 2];
                position += 2;
            }
            if ( string[position + 1 ] == '\0' )
            {   //quotation marks mismatch
                return -1;
            }
            position++;
	    // if currword starts with '$', replace it with the variable's value
	    if ( currword[0] == '$' && quote == '"')
	    {
		currword[wordlen] = '\0';
	        char * envvar = getenv(currword+1);
		if ( strcmp ( currword+1, "?" ) == 0) // last return code is fixed here 
		{
		    envvar = (char * )malloc(sizeof(char)*10);
		    snprintf( envvar, 2, "%d", last_code);
		    envvar[strlen(envvar)] = '\0';
		}

		//replace $* with envvar.
		int ii;
		currword[wordlen] = '\0';
		if (envvar == NULL )
		    currword[0]='\0';
		else
		{
		    for ( ii=0; ii< strlen(envvar); ii++)
			currword[ii] = envvar[ii];
		    currword[ii]='\0';
		}   
		wordlen = strlen(currword);
	    }
            break;
	case '$':
	{
	    //get string until space, quote or pipe
	    char tmp[MAXCMDLEN];
	    int startpos = position;
	    position++;
	    while ( string[position] != '\0'
		 && string[position] != '>'
		 && string[position] != '<'
		 && string[position] != '|'
		 && string[position] != '"'
		 && string[position] != '\'' )
	    {
		tmp[position-startpos-1] = string[position];
		position++;
	    }
	    tmp[(position--)-startpos-1]='\0';
	    char * envvar = getenv(tmp);
	    if ( strcmp ( tmp, "?" ) == 0) 
	    {
		envvar = (char * )malloc(sizeof(char)*100);
		snprintf( envvar, 2, "%d", last_code);
		envvar[strlen(envvar)] = '\0';
	    }
	    currword[wordlen] = '\0';
	    if (envvar != NULL )
		strcat(currword, envvar);
	    wordlen = strlen(currword);
	    break;
	}
        default:
            currword[wordlen++] = string[position];
            break;
        }
    }
    currword[wordlen] = '\0';
    to[wordcount++] = strdup ( currword );
    //close both arrays with terminating signs
    to[wordcount] = NULL;
    *redirections = -1;

    // if first word contains '=', prepend "set"
    if (contains_equal( to ) )
    {
	int ii;
	for ( ii =wordcount+1; ii > 0 ;ii--)
	    to[wordcount] = to[wordcount-1];
	to[0] = "set";
	to[wordcount+1] = NULL;
    }
    return wordcount;
}

FUNCTION find_local_func ( const char *word )
{
    int ii = 0;
    for ( ii = 0; commands[ii].name != NULL; ii++ )
    {
        if ( strcmp ( commands[ii].name, word ) == 0 )
            return commands[ii].function;
    }
    return NULL;
}
FUNCTION find_env_func ( const char *word )
{
    int ii = 0;
    for ( ii = 0; env_commands[ii].name != NULL; ii++ )
    {
        if ( strcmp ( env_commands[ii].name, word ) == 0 )
            return env_commands[ii].function;
    }
    return NULL;
}

int detect_pipes ( char ** words, const int * redirections )
{
    int count = 0;
    int offset = 0;
    while ( * ( redirections + offset ) >= 0 )
    {
        if ( strcmp ( ( words[* ( redirections + offset )] ), "|" ) == 0 )
            count++;
        offset++;
    }
    return count;
}

int file_redirects ( char ** words, const int * redirections)
{
    int offset = 0;
    int changed = 0;
    while ( * ( redirections + offset ) >= 0 )
    {
        switch ( * ( words[* ( redirections + offset )] ) )
        {
            case '<':
	    {
		char *filepath = words[*redirections + 1];
		FILE *file = fopen ( filepath, "r" );
		close ( 0 );
		dup ( fileno ( file ) );
		changed++;
		//FIXME: this file pointer should be saved and closed eventually. but fuck it for now
		words[* ( redirections + offset ) ] = NULL;
	    }
                break;
            case '>':
	    {
		char *filepath = words[*redirections + 1];
		FILE *file = fopen ( filepath, "w" );
		close ( 1 );
		dup ( fileno ( file ) );
		changed++;
		words[* ( redirections + offset ) ] = NULL;
		//FIXME: this file pointer should be saved and closed eventually. but fuck it for now
	    }
                break;
            case '|':
                return changed; //we don't want any redirect past the pipe
            default:
                break;
        }
        offset++;
    }
    return changed;
}

int exec_command ( char ** words, const int *redirections )
{
    if ( strcmp ( words[0], "exit" ) == 0 ) exit ( 0 );
    FUNCTION funcptr = find_local_func ( words[0] );
    if ( detect_pipes ( words, redirections ) == 0)
    {
        int pid = fork();
        if ( pid == -1 ) {
            fprintf ( stderr, "Fork Failed\n" );
            return ( -1 );
        }
        else if ( pid == 0 )
        {
            file_redirects ( words, redirections );
            if ( funcptr != NULL )
            {
                //local command, no pipes
                int ret_code =  funcptr ( (const char ** )words + 1 );
                exit( ret_code);
            }
            return execvp ( words[0], words );
        }
        else 
        { 
            //if the command affects our current environemt, it must be executed
            funcptr = find_env_func ( words[0] );
            if ( funcptr != NULL )
                return funcptr ( (const char ** )words + 1 );

            //wait for child to end execution
            int status;
            waitpid ( pid, &status, 0 );
            return WEXITSTATUS ( status );
        }
    }
    else
    {   //pipes... they suck donkey balls
	int fd[2];
	int toppid = fork();
	if (toppid < 0 ){
            printf ( "Fork Failed\n" );
            return ( -1 );
        }
	else if ( toppid == 0)
	{
	    pipe(fd);
	    int pipe_separator =0;
	    while (strcmp(words[* (pipe_separator + redirections)], "|") != 0 )
		pipe_separator++;

	    int pid = fork();
	    if ( pid == -1 ) {
		printf ( "Fork Failed\n" );
		return ( -1 );
	    }
	    else if ( pid == 0 ) //grandchild
	    {
		close(fd[1]);
		close(0);
		dup(fd[0]);
		//prepare words and execute
		words+=* (pipe_separator + redirections)+1;
		return execvp(words[0], words);
		// setup outgoing pipes
	    }
	    else
	    { //child
		close(fd[0]);
		close(1);
		dup(fd[1]);
		//prepare words and execute
		words[* (pipe_separator + redirections)] = NULL;
		return execvp(words[0], words);
	    }
	}
	else
	{
            int status;
            waitpid ( toppid, &status, 0 );
            return WEXITSTATUS ( status );
	}
    }
}


int main ( int argc, char *argv[], char *envp[] )
{
    environment = envp;
    dir_stack = ( char ** ) malloc ( sizeof ( char * ) * 100 );
    host = strdup( getenv("USER") );
    hist_path = getenv ( "HOME" );
    strcat ( hist_path, "/.my_shell_hist" );

    while ( 1 )
    {
        prompt();
        //get command
        char buf[MAXCMDLEN];
        char *cptr = fgets ( buf, MAXCMDLEN, stdin );
        if ( cptr == NULL ) //exit via EOF
	    break;

        trim ( buf );
        //add it to history
        if ( buf[0] != '\0' && buf[1] != '\0' ) // if this is an actual command...
	{
	    FILE * history = fopen ( hist_path, "a" );
            fprintf ( history, "%s\n", buf );
	    fclose ( history );
	}

        char *words[MAXWORDS];
        int redirections[MAXWORDS];
        if ( split_str ( buf, words, redirections ) < 0 )
            fprintf ( stderr, "Invalid syntax: [%s]\n", buf );
        last_code = exec_command ( words, redirections );
    }
    return 0;
}

void prompt()
{ //code removed to please the testor script in order to avoid printing stuff to stdout/stderr
    ;//fprintf ( stderr,"[%s %s]$ ", host, curr_path );
}

char* parse_path ( char * curr, const char * path, char* to )
{
    char buffer[2 * MAXPATHLEN];
    if ( path[0] == '/' )
    { //if we have an absolute path, return it
        strcpy ( to, path );
        return to;
    }
    strcpy ( buffer, curr );
    strcat ( buffer, "/" );
    strcat ( buffer, path );
    char *dirs[MAXPATHLEN];
    char *dirp;
    int dirind = 0;
    for ( dirp = strtok ( buffer, "/" ); dirp != NULL; dirp = strtok ( NULL, "/" ) )
        dirs[dirind++] = dirp;
    dirs[dirind] = NULL;

    char *target[MAXPATHLEN];
    int targetind = 0;
    for ( dirind = 0; dirs[dirind] != NULL; dirind++ )
    {
        if ( strcmp ( dirs[dirind], "." ) == 0 )
            ; //do nothing
        else if ( strcmp ( dirs[dirind], ".." ) == 0 )
        {
            if ( targetind > 0 )
                //remove previous entry in target
                target[targetind--] = NULL;
        }
        else
            target[targetind++] = dirs[dirind];
    }
    target[targetind] = NULL;
    *to = '\0';
    for ( dirind = 0; target[dirind] != NULL; dirind++ )
    {
        strcat ( to, "/" );
        strcat ( to, target[dirind] );
    }
    return to;
}

char *trim ( char *str )
{
    char *end;
    // Trim leading space
    while ( isspace ( *str ) ) str++;
    if ( *str == 0 ) // All spaces?
        return str;
    // Trim trailing space
    end = str + strlen ( str ) - 1;
    while ( end > str && isspace ( *end ) ) end--;
    * ( end + 1 ) = 0;
    return str;
}
