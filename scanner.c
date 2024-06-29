#include "scanner.h"

#include <stdio.h> // Debugging

#define mktok(_type, _line, _index, _length, _token) ((Token) {.type = _type, .line = _line, .index = _index, .length = _length, .token = _token})
#define mkerr(_type, _line, _index, _err) ((Error) {.type = _type, .line = _line, .index = _index, .err = _err}) 

static unsigned long _INDEX = 1;
static unsigned long _LINE = 1;

static InfixIndicator* _INFIXES;
static char** _LINES;

static bool _IS_CORRECT_STREAM = true;

void print_token(Token tok) {
  printf("\t|\n    %lu\t| %s (%d)\n\t| ^\n\t  %lu - %lu\n\n", tok.line, tok.token, tok.type, tok.index, tok.length);
}

// Size must not take into account null delimiter
static inline char* reserve_token(const unsigned long size, const char* stream) {
  char* tok = Malloc((size + 1)*sizeof(char));
  strncpy(tok, stream, size);
  tok[size] = '\0';
  return tok;
}

static inline bool in_string(char c, const char* s) {
  unsigned long i = 0;
  for (; s[i] && s[i] != c; i++);
  return s[i];
}

static inline char consume(char** stream_ptr) {
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
  _LINES = new_vector_with_capacity(*_LINES, 128);
  Token* token_stream = new_vector_with_capacity(*token_stream, 32);
  Error* error_stream = new_vector_with_capacity(*error_stream, 16);
  _INFIXES = new_vector_with_capacity(*_INFIXES, 8);
  

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
        }
        break;
      case '\'':
        if (stream[1] == '\'' || stream[0] == '\\' && stream[2] == '\'') {
          push(
            token_stream,
            mktok(CHARACTER, _LINE, _INDEX, 1, stream)
          );
          char offs;
          stream[offs = 1 + (stream[0] == '\\')] = '\0';
          stream += offs + 1;
          _INDEX += offs + 1;
        } else {
          _IS_CORRECT_STREAM = false;
          push(
            error_stream,
            mkerr(SCANNER, _LINE, _INDEX - 1, "Unmatched single qoute (must delimit a character or an escaped character)")
          );
        }
        break;
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

        if (*stream == '.') {
          if (numtype == NATURAL) {
            consume(&stream);
            numtype = REAL;
            goto scan_num;
          }
          _IS_CORRECT_STREAM = false;
          push(
            error_stream, 
            mkerr(SCANNER, _LINE, index, "Too many dots on a number")
          );
          consume(&stream);
          break;
        }

        if (*stream && !isspace(*stream)) {
          _IS_CORRECT_STREAM = false;
          push(
            error_stream, 
            mkerr(SCANNER, _LINE, index, "Must separate number with space")
          );
          break;
        }

        start_number = reserve_token(size = stream - start_number, start_number);

        push(
          token_stream, 
          mktok(numtype, _LINE, index, size, start_number)
        );

        break;
      }
      default: {
        char* start_token = stream - 1;
        unsigned long index = _INDEX - 1;
        unsigned long size;

        bool is_alnum = false;
        bool is_oper = false;

        while (*stream && !isspace(*stream)) {
          curchar = consume(&stream);
          is_alnum |= isalnum(curchar);
          is_oper |= in_string(curchar, "ºª!·$%&/=?¿^+*<>,.:-_|@#~½¬•");
        }

        if (is_alnum && is_oper) {
          _IS_CORRECT_STREAM = false;
          push(
            error_stream, 
            mkerr(SCANNER, _LINE, index, "Symbol must not use alphanumerics and operator characters at the same time")
          );
          break;
        }

        start_token = reserve_token(size = stream - start_token, start_token);

        push(
          token_stream, 
          mktok(is_alnum? IDENTIFIER : OPERATOR, _LINE, index, size, start_token)
        );

        if (*stream) {
          *stream = '\0';
          consume(&stream);
        }
        break;
      }
    }
  }

  if (_IS_CORRECT_STREAM)
    return (Tokens) {
      .is_correct_stream = true,
      .scanned.token_stream = token_stream,
      .scanned.infixes = NULL,
      .scanned.lines = NULL,
    };
  else 
    return (Tokens) {
      .is_correct_stream = false,
      .errors = error_stream
    };
}

int main(int argc, char* argv[]) {
  if (argc != 2)
    return 1;
  Tokens stream = scanner(argv[1]);

  printf("\n\n");
  if (stream.is_correct_stream) {
    for_each(i, stream.scanned.token_stream)
      print_token(stream.scanned.token_stream[i]);
  } else {
    for_each(i, stream.errors)
      report_error(stream.errors[i]);
  }
  return 0;
}
