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
  command_t* container;
  command_t script;
};

/* Function declarations. */
int getGrammar(int c);

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
  curr_stream->script = NULL;
  
  int curr = 0;
  int line = 0;
  
  char prevChar = '\0';
  char prevToken = '\0';

  bool pipeline = false;

  /* Read stream into array. */
  for(curr = get_next_byte(get_next_byte_argument); curr != EOF; )
  {
    char* arrayOfTokens;

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
      else error(1, 0, "%u: Syntax Error - Not a valid comment.", line);
    }

    /* Start storing if there is something to store from stream. */
    if(curr != EOF)
    {
      arrayOfTokens = checked_malloc((sizeof(char) * 2));
     
      if(getGrammar(curr) == TOKEN)
      {
        arrayOfTokens[0] = curr;
        
        /* Checks for syntax errors. */
        switch(curr) {
        case ';':
          break;
        case '|':
          break;
        case '&':
          break;
        case '(':
          break;
        case ')':
          break;
        case '<':
          break;
        case '>':
          break;
        }

        prevChar = curr;
        prevToken = prevChar;
        curr = get_next_byte(get_next_byte_argument);

        /* Checks && and || syntax. */
        switch(prevChar) {
        case '|':
          if(curr != '|')
            //error(1, 0, "%u: Syntax Error: Missing |", line);
            pipeline = true;
          break;
        case '&':
          if(curr != '&')
          {
            free(arrayOfTokens);
            error(1, 0, "%u: Syntax Error: Missing &", line);
          }
          break;
        }  
      }  
      else if(getGrammar(curr) == OTHER)
      {
        free(arrayOfTokens);
        error(1, 0, "%u: Syntax Error- Not WORD OR TOKEN OR SPACE", line);
      }
      else
        /* Add WORD to array of tokens. */
        while(getGrammar(curr) == WORD)
        {
          /* Check if there is enough space and reallocate if needed. */
          if(((strlen(arrayOfTokens) * sizeof(char) + 1) + (sizeof(char) + 1) < sizeof(arrayOfTokens))
          {
            arrayOfTokens = checked_grow_alloc(arrayOfTokens, sizeof(char))
          }
          
          strncat(arrayOfTokens, curr);
          strncat(arrayOfTokens, '\0');
        }
    }
  }
  return 0;
}

command_t
read_command_stream (command_stream_t s)
{
  /* FIXME: Replace this with your implementation too.  */
  //error (1, 0, "command reading not yet implemented");
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
