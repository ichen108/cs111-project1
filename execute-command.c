// UCLA CS 111 Lab 1 command execution

#include "command.h"
#include "command-internals.h"

#include <error.h>

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdio.h>
#include <fcntl.h>
#include "alloc.h"
#include "string.h"

typedef struct dep_node* dep_node_t;
typedef struct dep_graph *dep_graph_t;

struct dep_node
{
  command_t c;
  dep_node_t* before;
  size_t bSize;
  size_t bMem;

  dep_node_t* after;
  size_t aSize;
  size_t aMem;

  char** in;
  size_t inSize;
  size_t inMem;
  char** out;
  size_t outSize;
  size_t outMem;
  char** args;
  size_t argSize;
  size_t argMem;
};

int
command_status (command_t c)
{
  return c->status;
}

/* Function Declaration. */
int execute(command_t c);
int memNeedGrow(void* ptr, size_t* len, size_t obSize, size_t mem);
void execute_t(command_stream_t s);

void initNode (dep_node_t n, command_t c)
{
  n->c = c;
  n->bMem = sizeof(dep_node_t);
  n->before = checked_malloc(n->bMem);
  n->bSize = 0;
  n->aMem = sizeof(dep_node_t);
  n->after = checked_malloc(n->aMem);
  n->aSize = 0;
  n->in = checked_malloc(sizeof (char *)*512);
  n->out = checked_malloc(sizeof (char*)*512);
  n->args = checked_malloc(sizeof (char*)*512);
}

void destroy_node (dep_node_t *n)
{
	if (*n == NULL)
	  return;	
	free((*n)->args);
	free((*n)->out);
	free((*n)->in);
	free((*n)->before);
	free((*n)->after);
	free(*n);
	*n = NULL;
}

struct dep_graph 
{
	// Points to executable nodes
	dep_node_t* exec;
	size_t execSize;
	size_t execMem;
	
	// Points to dependency nodes
	dep_node_t* dep;
	size_t depSize;
	size_t depMem;
};

// Adds a node to dependency graph
int addToDep(dep_graph_t d, dep_node_t n, int choose)
{
	switch (choose)
	{
	  case 0: // Executable node
		if (d->execMem < (sizeof (dep_node_t) * (d->execSize + 1)))
		  d->exec = checked_grow_alloc (d->exec, &(d->execMem));
		d->exec[d->execSize] = n;
		d->execSize++;
		break;
	
	  case 1: // Dependency node
		if (d->depMem < (sizeof (dep_node_t) * (d->depSize + 1)))
		  d->dep = checked_grow_alloc (d->dep, &(d->depMem));
		d->dep[d->depSize] = n;
		d->depSize++;
		break;
	  default:
		return -1;
	}
	
	return 0;
}

int remove_node (dep_graph_t d, size_t epos)
{
	if (epos >= d->execSize)
	  return -1;
	free (d->exec[epos]);
	size_t it;
	for (it = epos; it < (d->execSize - 1); it++)
	{
	  d->exec[it] = d->exec[it + 1];
	}	
	d->execSize--;
	return 0;
}

int eToD (dep_graph_t d, size_t pos)
{
	if (pos >= d->execSize)
	  return -1;
	
	int err = addToDep(d, d->exec[pos], 1);
	d->depSize++;
	
	size_t it;
	for (it = pos; it < (d->execSize - 1); it++)
	{
	  d->exec[it] = d->exec[it + 1];
	}
	d->depSize--;
	return 0;
}

int dToE (dep_graph_t d, size_t pos)
{
	if (pos >= d->depSize)
	   return -1;
	
	int err = addToDep(d, d->dep[pos], 0);
	d->execSize++;
	
	size_t it;
	for (it = pos; it < (d->depSize - 1); it++)
	{
	  d->dep[it] = d->dep[it + 1];
	}
	d->depSize--;
	return 0;
}


void initGraph (dep_graph_t d)
{
	d->exec = checked_malloc (sizeof (dep_node_t));
	d->dep = checked_malloc (sizeof (dep_node_t));
	d->execSize = 0;
	d->execMem = sizeof (dep_node_t);
	d->depSize = 0;
	d->depMem = sizeof (dep_node_t);
}

void destroyGraph (dep_graph_t* d)
{
	size_t i;
	for (i = 0; i < (*d)->execSize; i++)
	{
		destroy_node( &( (*d)->exec[i] ) );
	}
	
	for (i = 0; i < (*d)->depSize; i++)
	{
		destroy_node( &( (*d)->dep[i] ) );
	}
	
	free ((*d)->exec);
	free ((*d)->dep);
	
	free (*d);
	*d = NULL;
}

void find_args(command_t c, char** args, size_t* size, size_t* mem)
{
	char **w;

	switch(c->type){
		case SEQUENCE_COMMAND:
			find_args(c->u.command[0], args, size, mem); 
			find_args(c->u.command[1], args, size, mem); 
			break;
		case SIMPLE_COMMAND:
			w = &(c->u.word[1]);
			while (*++w)
			{
				if (memNeedGrow(args, size, sizeof (char*), *mem))
					args = checked_grow_alloc (args, mem);
				args[*size] = *w; 
				(*size)++;
			}
			break;
		case SUBSHELL_COMMAND:
			find_args(c->u.subshell_command, args, size, mem);
		default:
			find_args(c->u.command[0], args, size, mem); 
			find_args(c->u.command[1], args, size, mem); 
			break;
	}	
	return;
}

void find_input(command_t c, char** iargs, size_t *iSize, size_t* iMem)
{
	switch (c->type){
    		case SEQUENCE_COMMAND:
			find_input(c->u.command[0], iargs, iSize, iMem);
			find_input(c->u.command[1], iargs, iSize, iMem);
			break;
		case SIMPLE_COMMAND:
			if (c->input != NULL)
			{
				if (memNeedGrow(iargs, iSize, sizeof (char*), *iMem))
				   iargs = checked_grow_alloc (iargs, iMem); 
				iargs[*iSize] = c->input; 
				(*iSize)++;
			}
			break;
		case SUBSHELL_COMMAND:
			if (c->input != NULL)
			{
				if (memNeedGrow(iargs, iSize, sizeof (char*), *iMem))
				   iargs = checked_grow_alloc (iargs, iMem); 
				iargs[*iSize] = c->input; 
				(*iSize)++;
			}
			find_input(c->u.subshell_command, iargs, iSize, iMem);
			break;
		default: 
			find_input(c->u.command[0], iargs, iSize, iMem);
			find_input(c->u.command[1], iargs, iSize, iMem);
			break;   
	}
	return;
}

void find_output(command_t c, char** oargs, size_t* oSize, size_t* oMem)
{
	switch (c->type){
    		case SEQUENCE_COMMAND:
			find_output(c->u.command[0], oargs, oSize, oMem);
			find_output(c->u.command[1], oargs, oSize, oMem);
			break;
		case SIMPLE_COMMAND:
			if (c->output != NULL)
			{
				if (memNeedGrow(oargs, oSize, sizeof (char*), *oMem))
				   oargs = checked_grow_alloc (oargs, oMem); 
				oargs[*oSize] = c->output; 
				(*oSize)++;
			}
			break;
		case SUBSHELL_COMMAND:
			if (c->output != NULL)
			{
				if (memNeedGrow(oargs, oSize, sizeof (char*), *oMem))
				   oargs = checked_grow_alloc(oargs, oMem); 
				oargs[*oSize] = c->output; 
				(*oSize)++;
			}
			find_output(c->u.subshell_command, oargs, oSize, oMem);
			break;
		default: 
			find_output(c->u.command[0], oargs, oSize, oMem);
			find_output(c->u.command[1], oargs, oSize, oMem);
			break;   
	}
	return;
}

void
execute_command (command_t c, bool time_travel)
{ 
  int state = 0;  

  if(!time_travel)
    state = execute(c);
  else
    return;
  if (state!= 0)
	error(state, 0, "Failed");
}

int memNeedGrow (void* ptr, size_t* len, size_t obSize, size_t mem)
{
  if (obSize * (*len + 1) > mem)
     return 1;
  return 0;
}

int pid_status(command_t c, pid_t p)
{
  int status;
  if(waitpid(p, &status, 0) < 0)
  {
    return 0;
  }
  else
  if(!WIFEXITED(status) || WEXITSTATUS(status) != 0)
  {
    kill(p, SIGKILL);
    error(1, 0, "Exit status error. \n");
    return 0;
  }
  else
  {
    c->status = WEXITSTATUS(status);
    return 1;
  }
}

dep_graph_t make_dep_graph(command_stream_t s)
{
	return NULL;
}

void execute_dep_graph(dep_graph_t d)
{
	return;
}

int execute_recursive(command_t c)
{
	return 0;
}


int execute(command_t c)
{
  switch(c->type)
  {
    /* SEQUENCE runs the first command then the second command. */
    case SEQUENCE_COMMAND:
    {
      int run = 0;

      if(!execute(c->u.command[0]))
      {
        run++;
      }
      if(!execute(c->u.command[1]))
      {
        run++;
      }
      c->status = c->u.command[1]->status;

      return !run;
    }

    /* Left writes, right reads by PIPE definition. */
    case PIPE_COMMAND:
    {
      pid_t p = fork();

      /* Child Process */
      if(p == 0)
      {
        int pipefd[2];
       
        /* Initialize the pipe file descriptors. */ 
        if(pipe(pipefd) < 0)
        {
          error(1, 0, "Pipe Failure. \n");
        }
   
        pid_t pp = fork();

        /* Check if child process. */
        if(pp == 0)
        {
          /* Close pipefd[0] and use pipefd[1] for stdout. */
          close(pipefd[0]);
          dup2(pipefd[1], 1);
          close(pipefd[1]);
  
          if(!execute(c->u.command[0]))
          {
            exit(123);
          }
          exit(1);
        }
        else
        /* It is parent process. */
        {
          /* Close pipefd[1] and use pipefd[0] for stdin. */
          close(pipefd[1]);
          dup2(pipefd[0], 0);
          close(pipefd[0]);

          if(pid_status(c, pp) == 0)
          {
            exit(123);
          }
          kill(pp, SIGKILL);
        }
        exit(1);
      }
      else
      /* It is a grandparent process. */
      {
        switch(pid_status(c, p))
        {
          case 0:
            return 0;
          case 1:
            kill(p, SIGKILL);
            return 1;
        }
      }
    }

    /* AND will run both command. */
    case AND_COMMAND:
    {
      /* If left command is correct, return right command. */
      if(execute(c->u.command[0]))
      {
        return execute(c->u.command[1]);
      }
      else
      /* Failed AND_COMMAND */
      {
        return 0;
      }
    }

    /* Check left and if it isn't valid, do right. */
    case OR_COMMAND:
    {
      /* If left command is correct, return. */
      if(execute(c->u.command[0]))
      {
        return 1;
      }
      else
      /* If left command failed, try right command. */
      {
        return(execute(c->u.command[1]));
      }
    }

    case SUBSHELL_COMMAND:
    {
      pid_t p = fork();

      /* Child process */
      if(p == 0)
      {
        /* Open input */
        if(c->input != NULL)
        {
          int fd = open(c->input, O_RDONLY);
          if(fd < 0)
          {
            error(1, 0, "Couldn't open file. \n");
          }
          dup2(fd, 0);
        }
        
        /* Open output */
        if(c->output != NULL)
        {
          int fd = open(c->output, O_WRONLY | O_CREAT | O_TRUNC, 0666);
          if(fd < 0)
          {
            error(1, 0, "Couldn't open file. \n");
          }
          dup2(fd, 1);
        }

        /* Exit child process. */
        if((execute(c->u.subshell_command)) == 0)
        {
          c->status = c->u.subshell_command->status;
          exit(123);
        }
        else
        {
          exit(1);
        }
      }
      else
      /* Parent process */
      {
        switch(pid_status(c, p))
        {
          case 0:
            return 0;
          case 1:
            kill(p, SIGKILL);
            return 1;
        }
      }
    }
    
    /* Execute simple command. */
    case SIMPLE_COMMAND:
    {
      int fdi;
      int fdo;
      
      if(*c->u.word == NULL)
      {
        return 0;
      }
      char* tok = strtok(*c->u.word, " ");
      
      int d = dup(1);

      /* Open input file */
      if(c->input != NULL)
      {
        fdi = open(c->input, O_RDONLY);
        if(fdi < 0)
        {
          close(fdi);
          error(1, 0, "Couldn't open file. \n");
        }
        dup2(fdi, 0);
      }

      /* Open output file */
      if(c->output != NULL)
      {
        fdo = open(c->output, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if(fdo < 0)
        {
          close(fdo);
          error(1, 0, "Couldn't open file. \n");
        }
        dup2(fdo, 1);
      }

      /* Handle exec special builtin utility */
      pid_t p;
      char temp[5];
      strncpy(temp, *c->u.word, 4);
      temp[4] = '\0';
      if(strcmp("exec", temp))
      {
        p = fork();
      }
      
      if(p == 0)
      {
        char* temp2[100];
        int i = 0;
       
        while(tok != NULL)
        {
          if(i == 0 && strcmp(tok, "exec"))
          {
            continue;
          }
          else
          {
            temp2[i] = tok;
            i++;
          }
          tok = strtok(NULL, " ");
        }

        temp2[i] = NULL;

        if(strncmp(temp2[0], "false", 5) == 0)
        {
          exit(123);
        }
        execvp(temp2[0], temp2);
        error(1, 0, "Doesn't exist. \n");
      }
      else
      {
        if(c->input != NULL && fdi > 0)
        {
          dup2(d, 1);
          close(fdi);
        }
        
        if(c->output != NULL && fdo > 0)
        {
          dup2(d, 1);
          close(fdo);
        }

        switch(pid_status(c, p))
        {
          case 0:
            return 0;
          case 1:
            kill(p, SIGKILL);
            return 1;
        }
      }
    }
    
    default:
      return 0;
  }

void execute_t(command_stream_t s)
{
   dep_graph_t d = NULL;

   d = make_dep_graph (s);
   execute_dep_graph (d);
}
  
}
