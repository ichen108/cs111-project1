// UCLA CS 111 Lab 1 command execution

#include "command.h"
#include "command-internals.h"

#include <error.h>

/* FIXME: You may need to add #include directives, macro definitions,
   static function definitions, etc.  */

/* Added extra directives from standard library. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

int
command_status (command_t c)
{
  return c->status;
}

/* Function Declaration. */
int execute(command_t c);

void
execute_command (command_t c, bool time_travel)
{
  /* FIXME: Replace this with your implementation.  You may need to
     add auxiliary functions and otherwise modify the source code.
     You can also use external functions defined in the GNU C Library.  */
  //error (1, 0, "command execution not yet implemented");
  
  int state;  

  if(!time_travel)
  {
    state = execute(c);
    /*
    if(state < 1)
    {
      error(1, 0, "Execution failed. \n");
    }
    */
  }
  else
  {
    return;
  }
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
