// UCLA CS 111 Lab 1 command reading

#include "command.h"
#include "command-internals.h"

#include <error.h>

/* FIXME: You may need to add #include directives, macro definitions,
   static function definitions, etc.  */

/* Added  alloc.h file for memory allocation. */
#include "alloc.h"

/* Added extra directives from standard library. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Added extra definitions.
 * Enumeration of Grammar Syntax (Types of Grammar). 
 * NOTE: BY DEFAULT, THE FIRST TYPE IS SET TO 0 AND INCREMENTS TILL END. */
enum grammar
{
  WORD,		// 'a-z', 'A-Z', '0-9', '!', '%', '+', ',', '-', '.', '/', ':', '@', '^', '_'  
  TOKEN,	// ';', '|', '&&', '||', '(', ')', '<', '>', ' '\n' '
  SPACE,	// space or tab (new line is considered as ';')
  OTHER,	// None of the above.
};

/* FIXME: Define the type 'struct command_stream' here.  This should
   complete the incomplete type declaration in command.h.  */

/* Defined 'struct command_stream'. */ 
struct command_stream
{
  int position;
  char** str_token;
  size_t num_token;
  command_t* container;
  command_t script;
};

/* Function declarations. */
int getGrammar(int c);
void free_memory(char* array, command_stream_t s);
command_t parse(command_stream_t s, int start, int end, int *line, int *ferror);

command_stream_t
make_command_stream (int (*get_next_byte) (void *),
		     void *get_next_byte_argument)
{
  /* FIXME: Replace this with your implementation.  You may need to
     add auxiliary functions and otherwise modify the source code.
     You can also use external functions defined in the GNU C Library.  */
  //error (1, 0, "command reading not yet implemented");

  /* Initialize curr_stream which is of type command_stream_t for the current stream processed. */
  command_stream_t curr_stream = checked_malloc(sizeof(struct command_stream));
  curr_stream->position = 0;
  curr_stream->str_token = checked_malloc(sizeof(char*));
  curr_stream->num_token = 0;
  curr_stream->script = NULL;
  
  int curr = 0;
  int line = 0;
  int ferror = 0;
  int parentheses = 0;
  
  char prevChar = '\0';
  char prevprevChar = '\0';

  bool stop = false;
  bool pipeline = false;

  /* For loop reads stream and checks for syntax error. */
  for(curr = get_next_byte(get_next_byte_argument); curr != EOF; )
  {
    char* arrayOfTokens;
    size_t char_size = sizeof(char);

    /* Checks for leading spaces and removes them from stream. */
    while(getGrammar(curr) == SPACE)
    {
      prevChar = curr;
      curr = get_next_byte(get_next_byte_argument);
    }

   /* Checks for valid comments and removes them from stream. */ 
    if(curr == '#')
    {
      if(getGrammar(prevChar) == TOKEN || getGrammar(prevChar) == SPACE)
      {
        while(curr != '\n' && curr != EOF)
        {
          prevChar = curr;
          curr = get_next_byte(get_next_byte_argument);
        }
      }
      else
      {
         free_memory(arrayOfTokens, curr_stream);
         error(1, 0, "%i: Syntax Error - Not a valid comment.", line);
      }
    }

    /* Start storing if there is something to store from stream. */
    if(curr != EOF)
    {
      arrayOfTokens = checked_malloc(char_size * 2);
     
      if(getGrammar(curr) == TOKEN)
      {
        arrayOfTokens[0] = curr;
        
        /* Checks for syntax errors and sets precedent conditions. 
         * See spec for details and conventions. */
        switch(curr) {
        case ';':
        case '|':
        case '&':
          if(prevprevChar == ';' || prevprevChar == '\n' || prevprevChar == '\0')
          {
            free_memory(arrayOfTokens, curr_stream);
            error(1, 0, "%i: Syntax Error - Misusage of ; or | or || or &&.", line);
          }
          break;
        case '(':
          parentheses++;
          break;
        case ')':
          parentheses--;
          break;
        case '<':
        case '>':
          if(prevChar == '<' || prevChar == '>' || prevChar == '\0')
          {  
            free_memory(arrayOfTokens, curr_stream);
            error(1, 0, "%i: Syntax Error - Misusage of > or <.", line);
          }
          break;
        case '\n':
          if(prevChar == '<' || prevChar == '>')
          {
            free_memory(arrayOfTokens, curr_stream);
            error(1, 0, "%i: Syntax Error - Misusage of > or <.", line);
          }
          if(getGrammar(prevChar) == WORD || prevChar == ')')
          {
            arrayOfTokens[0] = ';';
          }
          else 
          {
            stop = true;
          }
          break;
        }

        /* Moves in placement in stream after TOKEN. */
        prevChar = curr;
        prevprevChar = prevChar;
        curr = get_next_byte(get_next_byte_argument);

        /* Checks && and || syntax. */
        switch(prevChar) {
        case '|':
          if(curr != '|')
            //error(1, 0, "%i: Syntax Error: Missing |", line);
            pipeline = true;
          break;
        case '&':
          if(curr != '&')
          {
            free_memory(arrayOfTokens, curr_stream);
            error(1, 0, "%i: Syntax Error: Missing &", line);
          }
          break;
        }  
      }  
      else if(getGrammar(curr) == OTHER)
      {
        /* Syntax not used in this shell. */
        free_memory(arrayOfTokens, curr_stream);
        error(1, 0, "%i: Syntax Error- Not WORD OR TOKEN OR SPACE", line);
      }
      else
      {
        /* Add WORD to array of tokens. */
        while(getGrammar(curr) == WORD)
        {
          /* Check if there is enough space and reallocate if needed. */
          if(((strlen(arrayOfTokens) * char_size + 1) + (char_size + 1)) < sizeof(arrayOfTokens))
          {
            arrayOfTokens = checked_grow_alloc(arrayOfTokens, &char_size);
          }
          
          /* Concatenate current char and end of C-String to end of array.
           * NOTE: strcat doesn't allow concatenating nulls. */   
          size_t end = strlen(arrayOfTokens);
          arrayOfTokens[end] = curr + '\0';

          /* Move placement in stream to collect WORD. */
          prevChar = curr;
          prevprevChar = prevChar;
          curr = get_next_byte(get_next_byte_argument);   
        }
      }
      
      /* Check if there is enough space and reallocate if needed. */
      size_t char_ptr_size = sizeof(char*);
      curr_stream->str_token = checked_grow_alloc(curr_stream->str_token, &char_ptr_size);
      
      /* Put gathered tokens into command_stream_t array of C-Strings. */
      if(stop == false)
      {
        curr_stream->str_token[curr_stream->num_token] = arrayOfTokens;
        curr_stream->num_token++;
      }
      else if(arrayOfTokens != NULL)
      {
        free(arrayOfTokens);
      }
    }
  } 

  /* Reports parentheses matching error. */
  if(parentheses != 0)
  {
    error(1, 0, "0: Syntax Error - Unmatched parentheses.");
  }

  line++;

  curr_stream->script = parse(curr_stream, 0, curr_stream->num_token - 1, &line, &ferror);
  curr_stream->container = checked_malloc(sizeof(command_t*));
  return curr_stream;
}

command_t
read_command_stream (command_stream_t s)
{
  /* FIXME: Replace this with your implementation too.  */
  //error (1, 0, "command reading not yet implemented");
  
  /* Temporary addition to remove warning. */
  printf("%i", s->position);
  return 0;
}

/* Distinguish between grammar of syntax with enum.
 * Status: VERIFIED */
int getGrammar(int c)
{
  if((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || isdigit(c) || c == '!' || c == '%' || c == '+' || c == ',' || c == '-' || c == '.' || c == '/' || c == ':' || c == '@' || c == '^' || c == '_')
    return WORD;
  else if(c == ';' || c == '|' || c == '&' || c == '(' || c == ')' || c == '<' || c == '>' || c == '\n')
    return TOKEN;
  else if(isspace(c))
    return SPACE;
  else if(c == EOF)
    return -1;
  else 
    return OTHER;
}

/* Frees array containing token and command_stream. */
void free_memory(char* array, command_stream_t s)
{
  /* Free c-string. */
  free(array);
 
 /* Checks if there is something to remove in command_stream. */
  if(s == NULL)
  {
    error(1, 0, "0: Memory Error - Cannot free NULL");
  }

  /* Free each token in array. */
  size_t start = 0;
  while(start < s->num_token)
  {
    free(s->str_token[start]);
    start++;
  }

  /* Free pointer to this memory. */
  free(s->str_token);
 
  /* Checks if there is something of command_t and removes it. */
  if(s->container != NULL)
  {
    free(s->container);
  }

  /* Completely free command_stream. */
  free(s);
}

command_t parse(command_stream_t s, int start, int end, int *line, int *ferror)
{
  /* Initalization of return command. */
  command_t ret_command;
  ret_command = checked_malloc(sizeof(struct command));
  ret_command->type = -1;
  ret_command->status = -1;
  ret_command->input = 0;
  ret_command->output = 0;

  int pos = end;

  while(end > start && ((strcmp(s->str_token[end], "\n")) == 0 || strcmp(s->str_token[end], ";") == 0))
  {
      end--;
  }
}
