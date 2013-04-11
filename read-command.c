// UCLA CS 111 Lab 1 command reading

#include "command.h"
#include "command-internals.h"

#include <error.h>

/* FIXME: You may need to add #include directives, macro definitions,
   static function definitions, etc.  */

/* Added alloc.h file for memory allocation. */
#include "alloc.h"

/* Added extra directives from standard library. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* FIXME: Define the type 'struct command_stream' here.  This should
   complete the incomplete type declaration in command.h.  */

/* Global Variables. */
int line = 0;

/* Defined 'struct command_stream'. */
typedef struct command_stream
{
  void *stream;
  int (*get_next_byte) (void*);
  int has_char_waiting;
  char ungotten_char;
};

enum token_type 
{
  WORD,			// 'a-z', 'A-Z', '0-9', '!', '%', '+', ',', '-', '.', '/', ':', '@', '^', '_' 
  NEWLINE,		// '\n'
  SEMICOLON,		// ';'
  PIPELINE,		// '|'
  OR,			// '||'
  AND,			// '&&'
  OPEN_PARENTHESES,	// '('
  CLOSED_PARENTHESES,	// ')'
  REDIRECT_IN,		// '<'
  REDIRECT_OUT,		// '>'
  END_FILE,		// EOF

  SPACE,		// space
  SPECIAL_TOKEN,	// ';', '|', '&&', '||', '(', ')', '<', '>', ' '\n' '
  UNSUPPORTED,		// unsupported token
};

typedef struct TOKEN
{
  enum token_type type;
  char *token_name;
} *token_t;


command_stream_t
make_command_stream (int (*get_next_byte) (void *),
		     void *get_next_byte_argument)
{
  /* FIXME: Replace this with your implementation.  You may need to
     add auxiliary functions and otherwise modify the source code.
     You can also use external functions defined in the GNU C Library.  */
  //error (1, 0, "command reading not yet implemented");
  command_stream_t curr_stream = checked_malloc(sizeof(struct command_stream));
  curr_stream->stream = get_next_byte_argument;
  curr_stream->has_char_waiting = -1;
  curr_stream->ungotten_char = '\0';
 
  return curr_stream;
}

command_t
read_command_stream (command_stream_t s)
{
  /* FIXME: Replace this with your implementation too.  */
  error (1, 0, "command reading not yet implemented");
  return 0;
}

/* Start implementation of PARSER */

command_t
parse_command_stream(token_stream_t s)
{

   //implementation goes here
   /* 
      Takes in a stream of token objects which contain token type and token name information
      ***Need to convert the tokens to commands***
      Attempt to build tree recursively and return the root node of the tree (a command object)
      Tree is rebuilt through DFS

      PSEUDOCODE:

	command_t curr;
	for(int i = 0; i < tokenstream.size; i++)	// iterate along COMPLETE commands
	{
		for (int j = 0; j < tokenstream[i].size; j++) // iterate within complete commands
		{
			
   */

   
}



/* Start implementation of TOKENIZER */

/* Checks if there is a byte taken from get_next_byte() and uses it if available. */
int get_check(command_stream_t s)
{
  if(s->has_char_waiting == -1)
  {
    return s->get_next_byte(s->stream);
  }
  else
  {
    int temp = s->has_char_waiting;
    s->has_char_waiting = -1;
    return temp;
  }
}

/* Checks if there is a byte and returns it back into the stream due to overlooking. */
int unget_check(command_stream_t s, int c)
{
  if(s->has_char_waiting != -1)
  {
    return 1;
  }
  else
  {
    s->has_char_waiting = c;
    return 0;
  }
}

/* Allocate memory for token. */
token_t create_token(void)
{
  token_t new_token;
  new_token = checked_malloc(sizeof(struct TOKEN));
  new_token->type = 0;
  new_token->token_name = NULL;
  return new_token;
}

/* Fill this token with the appropriate information. */
token_t fill_token(command_stream_t s, int c)
{
  token_t new_token = create_token();

  switch(c) {
    case ';':
      new_token->type = SEMICOLON;
      break;
    case '|':
      if(get_check(s) == '|')
      {
        new_token->type = OR;
      }
      else
      {
        unget_check(s, get_check(s));
        new_token->type = PIPELINE;
      }
      break;
    case '&':
      if(get_check(s) == '&')
      {
        new_token->type = AND;
      }
      else
      {
        error(1, 0, "%d: Syntax Error - Misusage of &.\n", line);
      }
      break;
    case '(':
      new_token->type = OPEN_PARENTHESES;
    case ')':
      new_token->type = CLOSED_PARENTHESES;
      break;
    case '<':
      new_token->type = REDIRECT_IN;
      break;
    case '>':
      new_token->type = REDIRECT_OUT;
      break;
  }
  return new_token;
}

/* Distinguish between grammar of syntax with enum. Return 0 for WORD and 1 for SPECIAL TOKEN. */
int getGrammar(int c)
{
  if((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || isdigit(c) || c == '!' || c == '%' || c == '+' || c == ',' || c == '-' || c == '.' || c == '/' || c == ':' || c == '@' || c == '^' || c == '_')
    return 0;
  else if(c == ';' || c == '|' || c == '&' || c == '(' || c == ')' || c == '<' || c == '>' || c == '\n')
    return 1;
  else
    error(1, 0, "%d: Syntax Error - Unsupported characters.", line);
}

int consume_whitespace(command_stream_t s)
{
  int curr;
  int track_newline = 0;

  while((curr = get_check(s) != EOF) && (isspace(curr)))
  {
    if(curr == '\n')
    {
      line++;
      track_newline = '\n';
    }
  }

  if(curr == EOF)
  {
    return EOF;
  }

  if(curr == '#')
  {
    while(((curr = get_check(s)) != '\n') && (curr != EOF))
    {
      line++;
      return '\n';
    }
  }
  //else
  //{
  //    error(1, 0, "%d: Syntax Error - Not a valid comment.", line);
  //}

  unget_check(s, curr);
  return track_newline;
}

token_t get_next_token(command_stream_t s)
{
  char* word_array;
  char* temp;
  size_t size = 0;

  int curr = consume_whitespace(s);
 
  if(curr == '\n')
  {
    token_t new_token = create_token();
    new_token->type = NEWLINE;
  }
  
  if(curr == EOF)
  {
    token_t new_token = create_token();
    new_token->type = END_FILE;
  }

  curr = get_check(s);
  if(getGrammar(curr))
  {
    return fill_token(s, curr);
  }

  token_t new_token = create_token();
  new_token->type = WORD;

  word_array = checked_malloc(sizeof(char) * 5);
  *(temp++) = curr;
  size++;
  
  for(;;)
  {
    curr = get_check(s);
    
    if(curr == EOF || isspace(curr) || getGrammar(curr))
    {
      if(curr != EOF)
      {
        unget_check(s, curr);
      }
      *temp = '\0';
      new_token->token_name = word_array;
      return new_token;
    }
    
    *(temp++) == curr;
    size++;

    if((size % 5) == 0) 
    {
      word_array = checked_realloc(word_array, size+5);
    }
  }
  
  return new_token;
}
