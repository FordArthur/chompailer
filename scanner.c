#include "scanner.h"

#define mktok(_type, _line, _index, _length, _token) ((Token) {.type = _type, .line = _line, .index = _index, .length = _length, .token = _token})

static unsigned long _INDEX = 1;
static unsigned long _LINE = 1;

static char** _LINES;

void print_token(Token* tok) {
  printf("\t|\n    %lu\t| %s (%d)\n\t| ^\n\t  %lu - %lu\n\n", tok->line, tok->token, tok->type, tok->index, tok->length);
}

// Size must not take into account null delimiter
static inline char* reserve_token(const unsigned long size, const char* stream) {
  char* tok = Malloc((size + 1)*sizeof(char));
  strncpy(tok, stream, size);
  tok[size] = '\0';
  return tok;
}

static inline char escape_char(char c) {
  switch (c) {
    case 'n': return '\n';
    case 'b': return '\b';
    case 'f': return '\f';
    case 'r': return '\r';
    case 'v': return '\v';
    case '0': return '\0';
    case 't': return '\t';
    default: return c;
  }
}

static inline bool isoper(char c) {
  unsigned long i = 0;
  const char s[] = "ºª!·$%&/=?¿^+*<>,.:-_|@#~½¬•";
  #pragma unroll 4
  for (; s[i] && s[i] != c; i++);
  return s[i];
}

static inline bool isiden(char c) {
  return isalnum(c) || c == '_' || c == '\'';
}

static inline char consume(char** stream_ptr) {
  if (!**stream_ptr)
    return '\0';

  char ch = *((*stream_ptr)++);
  if (ch == '\n') {
    push(_LINES, *stream_ptr);
    _LINE++;
    _INDEX = 1;
  } else {
    _INDEX++;
  }
  return ch;
}

Tokens scanner(char *stream) {
  bool is_correct_stream = true;
  _LINES = new_vector_with_capacity(*_LINES, 32);
  push(_LINES, stream);
  Token* token_stream = new_vector_with_capacity(*token_stream, 128);
  Error* error_buffer = new_vector_with_capacity(*error_buffer, 16);
  // This is fine, what we want to store in this vector is pointers, not structures
  Token** infixes = new_vector_with_capacity(*infixes, 8); // NOLINT(bugprone-sizeof-expression)

  for (char curchar; (curchar = consume(&stream));) {
    switch (curchar) {
      case ' ':
      case ',':
      case '\n':
      case '\t':
        continue;
      case '(':
        push(
          token_stream, 
          mktok(OPEN_PAREN, _LINE, _INDEX, 1, NULL)
        )
        break;
      case ')':
        push(
          token_stream, 
          mktok(CLOSE_PAREN, _LINE, _INDEX, 1, NULL)
        );
        break;
      case '[':
        push(
          token_stream, 
          mktok(OPEN_BRACKET, _LINE, _INDEX, 1, NULL)
        );
        break;
      case ']':
        push(
          token_stream, 
          mktok(CLOSE_BRACKET, _LINE, _INDEX, 1, NULL)
        );
        break;
      case '{':
        push(
          token_stream, 
          mktok(OPEN_CURLY, _LINE, _INDEX, 1, NULL)
        );
        break;
      case '}':
        push(
          token_stream, 
          mktok(CLOSE_CURLY, _LINE, _INDEX, 1, NULL)
        );
        break;
      case ';':
        push(
          token_stream, 
          mktok(SEMI_COLON, _LINE, _INDEX, 1, NULL)
        );
        break;
      case '/':
        if (stream[0] == '/') {
          char* start_comment = stream - 1;
          unsigned long index = _INDEX - 1;
          unsigned long size;

          while (*stream && *stream != '\n')
            consume(&stream);

          start_comment = reserve_token(size = stream - start_comment, start_comment);

          push(
            token_stream,
            mktok(COMMENT, _LINE, index, size, start_comment)
          );

        } else if (stream[0] == '*') {
          char* start_comment = stream - 1;
          unsigned long index = _INDEX - 1;
          unsigned long size;

          while (*stream && stream[1] != '*' && stream[2] != '/')
            consume(&stream);

          stream += 3;
          _INDEX += 3;
          start_comment = reserve_token(size = stream - start_comment, start_comment);
          consume(&stream);

          push(
            token_stream,
            mktok(COMMENT, _LINE, index, size, start_comment)
          );
        } else 
          goto scan_symbol;
        break;
      case '\'':
        if (stream[1] == '\'' || stream[0] == '\\' && stream[2] == '\'') {
          char offs, character = (offs = stream[1] == '\'')? stream[0] : escape_char(stream[1]);

          push(
            token_stream,
            mktok(CHARACTER, _LINE, _INDEX, 1, reserve_token(1, &character))
          );

          stream += (!offs) + 2;
          _INDEX += (!offs) + 2;
        } else {
          is_correct_stream = false;
          push(
            error_buffer,
            mkerr(SCANNER, _LINE, _INDEX - 1, "Unmatched single qoute (must delimit a character or an escaped character)")
          );
        }
        break;
      case '"': {
        char* start_string = new_vector_with_capacity(*start_string, 32);
        push(start_string, '\"');
        unsigned long index = _INDEX - 1;

        while (*stream && *stream != '"') {
          if (*stream == '\\') {
            stream++;
            push(start_string, escape_char(*stream));
          } else {
            push(start_string, *stream);
          }
          consume(&stream);
        }
        if (!*stream) {
          is_correct_stream = false;
          push(
            error_buffer,
            mkerr(SCANNER, _LINE, index, "Unmatched string quotation")
          );
          break;
        }

        push(start_string, '\"');

        push(
          token_stream, 
          mktok(STRING, _LINE, index, _INDEX - index - 1, start_string)
        );
        consume(&stream);
        break;
      }
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9': {
        char* start_number = stream - 1;
        unsigned long index = _INDEX - 1;
        unsigned long size;
        TokenType numtype = NATURAL;

      scan_num:
        while (isdigit(*stream))
          consume(&stream);

        if (*stream == '.' && numtype == NATURAL) {
          consume(&stream);
          numtype = REAL;
          goto scan_num;
        }

        start_number = reserve_token(size = stream - start_number, start_number);

        push(
          token_stream, 
          mktok(numtype, _LINE, index, size, start_number)
        );

        break;
      }
      default: 
      scan_symbol: {
        char* start_token = stream - 1;
        unsigned long index = _INDEX - 1;
        unsigned long size;

        bool is_alnum = isiden(curchar);
        bool (*symbol_scanning)(char) = is_alnum? isiden : isoper;

        while (*stream && (*symbol_scanning)(*stream)) {
          consume(&stream);
        }

        start_token = reserve_token(size = stream - start_token, start_token);

        push(
          token_stream, 
          mktok(isupper(*start_token)? TYPE_K : is_alnum? IDENTIFIER : OPERATOR, _LINE, index, size, start_token)
        //      ^-------------------------------------------------------------- type should be stored somewhere so it
        //                                                                      can be added to the infixes if appropiate
        );

        break;
      }
    }
  }

  push(
    token_stream,
    mktok(_EOF, _LINE, _INDEX, 1, NULL);
  );
  push(_LINES, stream);
  return (Tokens) {
    .is_correct_stream = is_correct_stream,
    .lines = _LINES,
    .scanned.token_stream = token_stream,
    .scanned.infixes = NULL,
    .error_buf = error_buffer
  };
}

