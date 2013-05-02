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
typedef struct dep_graph* dep_graph_t;

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

int
command_status (command_t c)
{
  return c->status;
}

/* Function Declaration. */
int execute(command_t c);
int memNeedGrow(size_t* len, size_t obSize, size_t mem);
void execute_t(command_stream_t s);
int execute_recursive(command_t c);

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
	
	//int err = addToDep(d, d->exec[pos], 1);
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
	
	//int err = addToDep(d, d->dep[pos], 0);
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
				if (memNeedGrow(size, sizeof (char*), *mem))
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
				if (memNeedGrow(iSize, sizeof (char*), *iMem))
				   iargs = checked_grow_alloc (iargs, iMem); 
				iargs[*iSize] = c->input; 
				(*iSize)++;
			}
			break;
		case SUBSHELL_COMMAND:
			if (c->input != NULL)
			{
				if (memNeedGrow(iSize, sizeof (char*), *iMem))
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
				if (memNeedGrow(oSize, sizeof (char*), *oMem))
				   oargs = checked_grow_alloc (oargs, oMem); 
				oargs[*oSize] = c->output; 
				(*oSize)++;
			}
			break;
		case SUBSHELL_COMMAND:
			if (c->output != NULL)
			{
				if (memNeedGrow(oSize, sizeof (char*), *oMem))
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

int memNeedGrow (size_t* len, size_t obSize, size_t mem)
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
	dep_graph_t ret = NULL;
	command_t comm;
	ret = checked_malloc (sizeof (struct dep_graph));
	initGraph (ret);

	size_t argMem = (sizeof(char*));
	char** args = checked_malloc(argMem);
	char** inputs;
	char** outputs;

	size_t aSize;
	size_t IMem;
	size_t ISize;
	size_t OMem;
	size_t OSize;
	size_t aMem;
	size_t iter = 0;
	size_t position = 0;
	size_t innerpos = 0;
	while( (comm = read_command_stream(s)) )
	{
		dep_node_t n = checked_malloc (sizeof (struct dep_node));
		initNode(n, comm); 
		while(iter < ret->execSize || iter < ret->depSize)
		{	
			ISize = 0;
			IMem = sizeof(char*);
			inputs = checked_malloc(IMem);
			OMem = (sizeof(char*) * 3);
			OSize = 0;
			outputs = checked_malloc(OMem);
			aSize = 0;
			aMem = sizeof (char*);
			args = checked_malloc(aMem);
			find_args(comm, args, &aSize, &aMem);
			find_input(comm, inputs, &ISize, &IMem);
			find_output(comm, outputs, &OSize, &OMem);
			n->in = inputs;
			n->inSize = ISize;
			n->inMem = IMem;
			n->out = outputs;
			n->outSize = OSize;
			n->outMem = OMem;
			n->args = args;
			
			//if outputs is not empty and there is a node in the executable array
			//of the graph, then add a dependency to the current node and then push it to the dependancy graph
			if(outputs != NULL && iter < ret->execSize)
			{
				while(position < OSize)
				{
					while(innerpos < ret->exec[iter]->inSize)
					{
						if(strcmp(ret->exec[iter]->in[innerpos], outputs[position])==0)
						{
							if (memNeedGrow(&n->bSize, sizeof(dep_node_t) , n->bMem))
								n->before = checked_grow_alloc (n->before, &(n->bMem));
							
							n->before[n->bSize] = ret->exec[iter];
							n->bSize +=1;
						}
						innerpos+=1;
					}
					position +=1;
				}
			}
			
			//if there is a value in the output array and there is a node in the dependancy array
			//of the graph, then add a dependency to the current node and then push it to the dependancy graph, because all commands
			//are parsed sequentially, if a dependancy occurs in the dependancy graph, then it must be added to the
			//dependancy graph
			if(outputs != NULL && iter < ret->depSize)
			{
				while(outputs[position]!=NULL)
				{
					while(ret->dep[iter]->in[innerpos]!=NULL)
					{
						if(strcmp(ret->dep[iter]->in[innerpos], outputs[position])==0)
						{
							n->before[n->bSize] = ret->dep[iter];
							n->bSize +=1;
						}
						innerpos+=1;
					}
					position +=1;
				}
			}
			
			//if there is a input dependency and there is a matching value in the dependancy graph
			//push the dependancy for this node
			if(inputs != NULL && iter < ret->depSize)
			{
				while(inputs[position]!=NULL)
				{
					while(ret->dep[iter]->out[innerpos]!=NULL)
					{
						if(strcmp(ret->dep[iter]->out[innerpos], inputs[position])==0)
						{
							n->before[n->bSize] = ret->dep[iter];
							n->bSize +=1;
						}
						innerpos+=1;
					}
					position +=1;
				}
			}
			
			//same thing as above, but check the executable array for a dependancy
			if(inputs != NULL && iter < ret->execSize)
			{
				while(inputs[position]!=NULL)
				{
					while(ret->exec[iter]->out[innerpos]!=NULL)
					{
						if(strcmp(ret->exec[iter]->out[innerpos], inputs[position])==0)
						{
							n->before[n->bSize] = ret->exec[iter];
							n->bSize +=1;
						}
						innerpos+=1;
					}
					position +=1;
				}
			}
			
			//this should do the same for the arguments parsed as command line and not strict 
			//I/O redirection. This function checks the dependancy array of the graph
			if(args != NULL && iter < ret->depSize)
			{
				while(args[position]!=NULL)
				{
					while(ret->dep[iter]->out[innerpos]!=NULL)
					{
						if(strcmp(ret->dep[iter]->out[innerpos], args[position])==0)
						{
							n->before[n->bSize] = ret->dep[iter];
							n->bSize +=1;
						}
						innerpos+=1;
					}
					position +=1;
				}
			}
			
			//same as above, but it checks the executable array for the graph
			if(args != NULL && iter < ret->execSize)
			{
				while(args[position]!=NULL)
				{
					while(ret->exec[iter]->out[innerpos]!=NULL)
					{
						if(strcmp(ret->exec[iter]->out[innerpos], args[position])==0)
						{
							n->before[n->bSize] = ret->exec[iter];
							n->bSize +=1;
						}
						innerpos+=1;
					}
					position +=1;
				}
			}
			position = 0;
			innerpos = 0;
			iter +=1;
		}

		//if at any point the bef_size of the current node has been altered so it is greater than 0, add this
		//node to the dependancy array for the graph
		if(n->bSize > 0)
			addToDep(ret, n, 1); 
		else
			addToDep(ret, n, 0); //if the node has no dependancies, then push it to the executable array of the dependancy graph
		
		iter = 0;		
	}
	
	return ret;
}

void execute_dep_graph(dep_graph_t d)
{
	size_t iter = 0;
	size_t innerIter = 0;
	int status;
	int addstat;
	int dpos;
	if (d->execSize>0)
	{
		int pid;
		pid = fork();
		if (pid == 0)
			execute_recursive(d->exec[0]->c);
		else if (pid < 0)
			error(1, 0, "Child process failed to fork");
		else if (pid > 0)
		{	
			while(iter < d->depSize)
			{
				for(innerIter = 0; innerIter < d->dep[iter]->bSize; innerIter++)
				{
					if(d->dep[iter]->before[innerIter] == d->exec[0])
					{
						for(dpos = innerIter; d->dep[dpos+1] != NULL; dpos++)
							d->dep[dpos] = d->dep[dpos+1];
						d->dep[iter]->bSize -= 1;
					}
				}
				if (d->dep[iter]->bSize == 0)
					addstat = dToE(d, iter);
				iter++;
			}
			remove_node(d, 0);
			if (d->execSize == 0)
			{
				while (waitpid(-1, &status, 0) > 0)
					continue;
				exit(0);
			}
			execute_dep_graph(d);
		}
	}
	
	//TODO:
	// while d has independent nodes
		// remove a node n from d
		// Fork
		
		// If child:
		// execute n
		//for each node m dependent on n
		//	pop n from m's dependency array
		//	if m's dependency array is empty
		//		insert m into d.n_exec;
	
		// If parent:
		//	recurse
}

int execute_recursive(command_t c)
{
	int errorStatus;
	int childError;
	int status;
	int child1;
	int child2;
	int childstatus;
	int andStatus;
	int seq;
	int fidirect;
	int fodirect;
	int errorC = 0;
	int fd[2];

	char** argv;

	pid_t pid = fork();
	
	if(pid < 0)
	{
		error (1, 0, "Fork error");
	}
	if (pid > 0)
	{
		if(waitpid(pid, &status, 0) < 0)
		{
			return status;
			error(status, 0, "Child Process Failed");
		}
		if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
		{
			return status;
			error(status, 0, "Child Process Failed");
		}
		
		return 0;
	}
	if (pid == 0)
	{
		switch(c->type)
		{
			case SIMPLE_COMMAND: 
				argv = c->u.word;
				if (c->input!=NULL)
				{		
					fidirect = open(c->input, O_RDONLY | O_CREAT, 0666);						
					dup2(fidirect, 0);
					close(fidirect);
				}
				if (c->output!=NULL)
				{		
					fodirect = open(c->output, O_WRONLY |  O_TRUNC| O_CREAT, 0666);						
					dup2(fodirect, 1);
					close(fodirect);
				}
				errorStatus = execvp(c->u.word[0], argv);
				break;
			case SEQUENCE_COMMAND:
				execute_recursive(c->u.command[0]);
				if(c->u.command[1] != NULL)
					execute_recursive(c->u.command[1]);
				break;
			case AND_COMMAND:				
				errorC = execute_recursive(c->u.command[0]);
				if (errorC)
					exit(errorC); 
				errorC = execute_recursive(c->u.command[1]);
				if (errorC)
					exit(errorC); 
				exit(0);				
				break;
			case OR_COMMAND:				
				errorC = execute_recursive(c->u.command[0]);
				if (errorC)
				{
					errorC = execute_recursive(c->u.command[1]);
					if (errorC) 
						exit (errorC);
					else exit(0);
				}
				exit(0);				
				break;
			case PIPE_COMMAND:				
				pipe(fd);			
				child1 = fork();
				if ( child1 > 0)
				{
					child2 = fork();
					if (child2 > 0)
					{
						close(fd[0]);
						close(fd[1]);
						if (waitpid(child1, &childstatus, 0)<0)
						{
							return childstatus;
							error(childstatus, 0, "Child Process Failed");
						}
						if (!WIFEXITED(childstatus) || WEXITSTATUS(childstatus) != 0)
						{
							return childstatus;
							error(childstatus, 0, "Child Process Failedsadsadsa");
						}
						waitpid(child2, &childstatus, WNOHANG);
						exit(0);
					}
					if(child2 == 0) // Child writes
					{
						close(fd[0]);
						dup2(fd[1], 1);
						if (c->u.command[0]->input != NULL)
						{		
							fidirect = open(c->u.command[0]->input, O_WRONLY|O_CREAT|O_TRUNC, 0666);						
							dup2(fidirect, 0);
							close(fidirect);
						}
						argv = c->u.command[0]->u.word;
						childError = execvp(argv[0], argv);
					}
				}
				if (child1 == 0) // Child reads
				{
					close(fd[1]);
					dup2(fd[0], 0);
					if (c->u.command[1]->output!=NULL)
						{		
							fodirect = open(c->u.command[1]->output, O_WRONLY|O_CREAT|O_TRUNC, 0666);						
							dup2(fodirect, 1);
							close(fodirect);
						}
					argv = c->u.command[1]->u.word;
					childError = execvp(argv[0], argv);
				}				
				break;
				
				
		case SUBSHELL_COMMAND:
			if (c->input!=NULL)
			{		
				fidirect = open(c->input, O_RDONLY | O_CREAT, 0666);						
				dup2(fidirect, 0);
				close(fidirect);
			}
			if (c->output!=NULL)
			{		
				fodirect = open(c->output, O_WRONLY |  O_TRUNC| O_CREAT, 0666);						
				dup2(fodirect, 1);
				close(fodirect);
			}
			errorC = execute_recursive(c->u.subshell_command);
			if (errorC)
			    return errorC;
			else exit (0);
			break;
		default:
			error (3, 0, "No command type!");
			break;
		}	
	}
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
  
}
void execute_t(command_stream_t s)
{
   dep_graph_t d = NULL;

   d = make_dep_graph (s);
   execute_dep_graph (d);
}
