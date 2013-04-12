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

/* Enumeration for token types before being converted to commands. */
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
};

/* Defined 'struct TOKEN'. */
typedef struct TOKEN
{
  enum token_type type;
  char *token_name;
} *token_t;

/* Defined 'struct command_stream'. */
struct command_stream
{
  void *stream;
  int (*byte_function) (void*);
  int has_char_waiting;
  token_t ctoken;
};

token_t get_token(command_stream_t s);
command_t parse(token_t *toBeParsed, int len);

command_stream_t
make_command_stream (int (*get_next_byte) (void *),
		     void *get_next_byte_argument)
{
  /* FIXME: Replace this with your implementation.  You may need to
     add auxiliary functions and otherwise modify the source code.
     You can also use external functions defined in the GNU C Library.  */
  //error (1, 0, "command reading not yet implemented");

  /* Initialize stream. */
  command_stream_t curr_stream = checked_malloc(sizeof(struct command_stream));
  curr_stream->byte_function = get_next_byte;
  curr_stream->stream = get_next_byte_argument;
  curr_stream->has_char_waiting = -1;
  curr_stream->ctoken = NULL;
 
  return curr_stream;
}

command_t
read_command_stream (command_stream_t s)
{
  /* FIXME: Replace this with your implementation too.  */

  token_t *tokArr = checked_malloc(sizeof(struct TOKEN)); 
  token_t prev = NULL;
  size_t size = 0;
  while (true) {
    token_t curr = get_token(s);

    if(curr->type == NEWLINE && prev && prev->type == WORD)
      curr->type = SEMICOLON;
    else if (curr->type == NEWLINE)
      continue;
    if (curr->type == END_FILE && prev && prev->type == SEMICOLON) {
      tokArr = checked_realloc(tokArr, (size - 1) * sizeof(struct TOKEN));
      break;
    }
    else if (curr->type == END_FILE)
      break;

    // put at end of array and resize if it's full
    tokArr = checked_realloc(tokArr, (size+1) * sizeof(struct TOKEN));
    size++;
    tokArr[size - 1] = curr;

    prev = curr;
  }
  
  if (size == 0) return NULL;

  return parse(tokArr, size);
}


/*************************************************
 *************************************************
 ******* Start of TOKENIZER Implementation *******
 *************************************************
 *************************************************/
 
 /* Function declaration */
token_t get_next_token(command_stream_t s);

/* Checks if there is a byte taken from a previous call and uses it before moving on. */
int get(command_stream_t s)
{
  if(s->has_char_waiting == -1)
  {
    return s->byte_function(s->stream);
  }
  else
  {
    int temp = s->has_char_waiting;
    s->has_char_waiting = -1;
    return temp;
  }
}

/* Checks if there is a byte and puts it back into the stream due to overlooking. */
int put(command_stream_t s, int c)
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

/* Gets current token. */
token_t get_token(command_stream_t s) 
{
  if (!s->ctoken) 
  {
    return get_next_token(s);
  }
  else 
  {
    token_t ret = s->ctoken;
    s->ctoken = NULL;
    return ret;
  }
}

/* Puts token into command stream. */
int put_token(command_stream_t s, token_t t) 
{
  if (s->ctoken) 
  {
    return 1;
  }
  else
  {
    s->ctoken = t;
    return 0;
  }
}

/* Allocate memory for token. */
token_t create_token(void)
{
  token_t new_token = checked_malloc(sizeof(struct TOKEN));
  new_token->type = 0;
  new_token->token_name = NULL;
  return new_token;
}

/* Fill this token with the appropriate information. */
token_t fill_token(command_stream_t s, int c)
{
  token_t new_token = create_token();
  int lookahead;

  switch(c) {
    case ';':
      new_token->type = SEMICOLON;
      break;
    case '|':
      lookahead = get(s);
      if(lookahead == '|')
      {
        new_token->type = OR;
      }
      else
      {
        put(s, lookahead);
        new_token->type = PIPELINE;
      }
      break;
    case '&':
      lookahead = get(s);
      if(lookahead == '&')
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
      break;
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

/* Distinguish between grammar of syntax with enum. */
int getGrammar(int c)
{
  if((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || isdigit(c) || c == '!' || c == '%' || c == '+' || c == ',' || c == '-' || c == '.' || c == '/' || c == ':' || c == '@' || c == '^' || c == '_')
  {
    return 0;
  }
  else if(c == ';' || c == '|' || c == '&' || c == '(' || c == ')' || c == '<' || c == '>')
  {
    return 1;
  }
  else
  {
    error(1, 0, "%d: Syntax Error - Unsupported characters.", line);
  }
  return 1;
}

/* Removes whitespace because whitespace is not part of token definition. */
int remove_whitespace(command_stream_t s)
{
  int curr;
  int prevcurr;
  int track_newline = 0;

  while((curr = get(s)) != EOF && isspace(curr))
  {
    prevcurr = curr;
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
    if(getGrammar(prevcurr) != WORD)
    {
      do
      {
        curr = get(s);
      }
      while(curr != EOF && curr != '\n');

      line++;
      return '\n';
    }
    else
    {
      error(1, 0, "%d: Syntax Error - Not a valid comment.", line);
    }
  }

  put(s, curr);
  return track_newline;
}

/* Looks at the next section of the stream and does the appropriate actions. */
token_t get_next_token(command_stream_t s)
{
  char* word_array = '\0';
  char* temp = '\0';
  size_t size = 0;

  int curr = remove_whitespace(s);
 
  if(curr == '\n')
  {
    token_t new_token = create_token();
    new_token->type = NEWLINE;
    return new_token;
  }
  
  if(curr == EOF)
  {
    token_t new_token = create_token();
    new_token->type = END_FILE;
    return new_token;
  }

  curr = get(s);
  
  if(getGrammar(curr))
  {
    return fill_token(s, curr);
  }

  token_t new_token = create_token();
  new_token->type = WORD;

  word_array = checked_malloc(sizeof(char) * 5);
  temp = word_array;
  *(temp++) = curr;
  size++;
  
  while(1)
  {
    curr = get(s);
    
    if(curr == EOF || isspace(curr) || getGrammar(curr))
    {
      if(curr != EOF)
      {
        put(s, curr);
      }
      *temp = '\0';
      new_token->token_name = word_array;
      return new_token;
    }
    
    *(temp++) = curr;
    size++;

    if((size % 5) == 0) 
    {
      word_array = checked_realloc(word_array, size+5);
    }
  }

  return new_token;
}

/*************************************************
 *************************************************
 ******* End of TOKENIZER Implementation *********
 *************************************************
 *************************************************/

/*************************************************
 *************************************************
 ******** Start of Parser Implementation *********
 *************************************************
 *************************************************/

enum command_type tokenToCommand(token_t input)
{
   enum command_type curr;

   switch(input->type){
   case AND:
	curr = AND_COMMAND;
        break;
   case OR:
	curr = OR_COMMAND;
        break;
   case SEMICOLON:
	curr = SEQUENCE_COMMAND;
        break;
   case PIPELINE:
	curr = PIPE_COMMAND;
        break;
   default:
	curr = SIMPLE_COMMAND;
   }
   return curr;
   
} 

command_t parse(token_t *toBeParsed, int len)
{
   int i;
   int parStat = 0;

   if(len <= 0)
	error(1,0,"1: Error, empty command");

   command_t ret = checked_malloc( sizeof (struct command));
   ret->input = 0;
   ret->output = 0;
   ret->status = 0;

   for(i = 0; i < len; i++)
   {
	if(toBeParsed[i]->type == OPEN_PARENTHESES)
	   parStat++;
	if(toBeParsed[i]->type == CLOSED_PARENTHESES)
	   parStat--;

	if(parStat < 0)
	   error(1, 0, "1: Error, mismatched parentheses");

	if(parStat > 0)
	   continue;

	if(toBeParsed[i]->type == SEMICOLON)
	{
	   ret->u.command[0] = parse(toBeParsed, i);
	   ret->u.command[1] = parse(toBeParsed + i + 1, len - i - 1);
	   ret->type = tokenToCommand(toBeParsed[i]);
	   return ret;
	}
   }

   if(parStat > 0)
	 error(1, 0, "1: Error, mismatched parentheses");

   for(i = 0; i < len; i++)
   {
	if(toBeParsed[i]->type == OPEN_PARENTHESES)
	   parStat++;
	if(toBeParsed[i]->type == CLOSED_PARENTHESES)
	   parStat--;

	if(parStat > 0)
	   continue;

	if( (toBeParsed[i]->type == AND) || (toBeParsed[i]->type == OR) )
	{
	   ret->u.command[0] = parse(toBeParsed, i);
	   ret->u.command[1] = parse(toBeParsed + i + 1, len - i - 1);
	   ret->type = tokenToCommand(toBeParsed[i]);
	   return ret;	
	}
   }

   for(i = 0; i < len; i++)
   {
	if(toBeParsed[i]->type == OPEN_PARENTHESES)
	   parStat++;
	if(toBeParsed[i]->type == CLOSED_PARENTHESES)
	   parStat--;

	if(parStat > 0)
	   continue;

	if(toBeParsed[i]->type == PIPELINE)
	{
	   ret->u.command[0] = parse(toBeParsed, i);
	   ret->u.command[1] = parse(toBeParsed + i + 1, len - i - 1);
	   ret->type = tokenToCommand(toBeParsed[i]);
	   return ret;	
	}
   }

   for(i = 0; i < len; i++)
   {
	if(toBeParsed[i]->type == OPEN_PARENTHESES)
	   parStat++;
	if(toBeParsed[i]->type == CLOSED_PARENTHESES)
	   parStat--;

	if(parStat > 0)
	   continue;

	if(toBeParsed[i]->type == REDIRECT_IN && i<len && toBeParsed[i+1]->type == WORD) 
	{
	   ret->input = toBeParsed[i+1]->token_name;
	   i++;
	} else if (toBeParsed[i]->type == REDIRECT_IN && toBeParsed[i+1]->type != WORD)
	   error(1,0,"1: Error, empty redirect");

	if(toBeParsed[i]->type == REDIRECT_OUT && i<len && toBeParsed[i+1]->type == WORD) 
	{
	   ret->output = toBeParsed[i+1]->token_name;
	   i++;
	} else if (toBeParsed[i]->type == REDIRECT_OUT && toBeParsed[i+1]->type != WORD)
	   error(1,0,"1: Error, empty redirect");
   }

   if(toBeParsed[0]->type == OPEN_PARENTHESES)
   {
	for(i = len - 1; i >= 0; i--)
	{
	   if(toBeParsed[i]->type == CLOSED_PARENTHESES)
		break;
	}
	ret->type = SUBSHELL_COMMAND;
	ret->u.subshell_command = parse(toBeParsed + 1, i - 1);
   	return ret;
   }

   ret->u.word = checked_malloc(sizeof(char*)); 
   int size = 0;
   for(i = 0; i < len; i++)
   {
	if(toBeParsed[i]->type == OPEN_PARENTHESES)
	   parStat++;
	if(toBeParsed[i]->type == CLOSED_PARENTHESES)
	   parStat--;

	if(parStat > 0)
	   continue;

	if(toBeParsed[i]->type == REDIRECT_IN || toBeParsed[i]->type == REDIRECT_OUT) 
	{
	   i++;
	   continue;
	}
	
   	// put at end of array and resize if it's full
        ret->u.word = checked_realloc(ret->u.word, (size+1) * sizeof(char*));
        size++;
        ret->u.word[size - 1] = toBeParsed[i]->token_name;
   }

   ret->u.word = checked_realloc(ret->u.word, (size+1) * sizeof(char*));
   ret->type = SIMPLE_COMMAND;
   ret->u.word[size] = 0;

   if(size <= 0)
	error(1,0,"1: Error, empty command");

   return ret;
}
