#include "scanner.h"

#define mktok(_type, _line, _index, _length, _token) ((Token) {.type = _type, .line = _line, .index = _index, .length = _length, .token = _token})

static unsigned long _INDEX = 1;
static unsigned long _LINE = 1;

static void** _LINES;

static inline unsigned long distance_between(Stream stream, void* otherstream) {
  return stream.distance_between(stream.stream, otherstream);
}
static inline void* copy_stream_offset(Stream stream, long offset) {
  return stream.copy_stream_offset(stream.stream, offset);
}
static inline void move_stream(Stream stream, long offset) {
  return stream.move_stream(&stream.stream, offset);
}
static inline char look_around(Stream stream, long offset) {
  return stream.look_around(stream.stream, offset);
}
static inline char get_char(Stream stream) {
  return stream.get_char(stream.stream);
}
static inline char consume_char(Stream stream) {
  return stream.consume_char(&stream.stream);
}

#ifdef DEBUG
#pragma GCC push_options
#pragma GCC optimize ("O0")
#endif

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

  for (; s[i] && s[i] != c; i++);
  return s[i];
}

static inline bool isiden(char c) {
  return isalnum(c) || c == '_' || c == '\'';
}

static inline char consume(Stream* stream_ptr) {
  if (!stream_ptr->get_char(stream_ptr->stream))
    return '\0';

  char ch = stream_ptr->consume_char(&stream_ptr->stream);
  if (ch == '\n') {
    push(_LINES, stream_ptr->stream);
    _LINE++;
    _INDEX = 1;
  } else {
    _INDEX++;
  }
  return ch;
}

Tokens scanner(Stream stream) {
  bool is_correct_stream = true;
  _LINES = new_vector_with_capacity(*_LINES, 32);
  push(_LINES, stream.stream);
  Token* token_stream = new_vector_with_capacity(*token_stream, 128);
  Error* error_buffer = new_vector_with_capacity(*error_buffer, 16);
  // This is fine, what we want to store in this vector is pointers, not structures
  Token** infixes = new_vector_with_capacity(*infixes, 8); // NOLINT(bugprone-sizeof-expression)

  TrieNode* syntax_trie = create_node(0, -1);
  insert_trie("="       , EQUALS            , syntax_trie);
  insert_trie("|"       , BAR               , syntax_trie);
  insert_trie("&"       , AMPERSAND         , syntax_trie);
  insert_trie("::"      , DOUBLE_COLON      , syntax_trie);
  insert_trie("->"      , ARROW             , syntax_trie);
  insert_trie("=>"      , DOUBLE_ARROW      , syntax_trie);
  insert_trie("||"      , DOUBLE_BAR        , syntax_trie);
  insert_trie("&&"      , DOUBLE_AMPERSAND  , syntax_trie);
  insert_trie(":="      , CONSTANT_DEFINE   , syntax_trie);
  insert_trie("if"      , IF                , syntax_trie);
  insert_trie("let"     , LET               , syntax_trie);
  insert_trie("then"    , THEN              , syntax_trie);
  insert_trie("else"    , ELSE              , syntax_trie);
  insert_trie("data"    , DATA              , syntax_trie);
  insert_trie("class"   , CLASS             , syntax_trie);
  insert_trie("infixl"  , INFIXL            , syntax_trie);
  insert_trie("infixr"  , INFIXR            , syntax_trie);
  insert_trie("instance", INSTANCE          , syntax_trie);

#ifdef DEBUG
  printf("Syntax trie:\n");
  print_trie(syntax_trie);
#endif

  for (char curchar; (curchar = consume(&stream));) {
    switch (curchar) {
      case ' ':
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
      case ',':
        push(
          token_stream, 
          mktok(COMA, _LINE, _INDEX, 1, NULL)
        );
      break;
      case ';':
        push(
          token_stream, 
          mktok(SEMI_COLON, _LINE, _INDEX, 1, NULL)
        );
        break;
      case '/':
        if (get_char(stream) == '/') {
          void* start_comment = copy_stream_offset(stream, -1);
          unsigned long index = _INDEX - 1;
          unsigned long size;

          while (get_char(stream) && get_char(stream) != '\n')
            consume(&stream);

          start_comment = reserve_token(size = distance_between(stream, start_comment), start_comment);

          push(
            token_stream,
            mktok(COMMENT, _LINE, index, size, start_comment)
          );

        } else if (get_char(stream) == '*') {
          void* start_comment = copy_stream_offset(stream, -1);
          unsigned long index = _INDEX - 1;
          unsigned long size;

          while (get_char(stream) && look_around(stream, 1) != '*' && look_around(stream, 2)!= '/')
            consume(&stream);

          move_stream(stream, 3);
          _INDEX += 3;
          start_comment = reserve_token(size =distance_between(stream, start_comment), start_comment);
          consume(&stream);

          push(
            token_stream,
            mktok(COMMENT, _LINE, index, size, start_comment)
          );
        } else 
          goto scan_symbol;
        break;
      case '\'':
        if (look_around(stream, 1) == '\'' || (get_char(stream) == '\\' &&look_around(stream, 2) == '\'')) {
          char offs, character = (offs =look_around(stream, 1) == '\'') ? get_char(stream) : escape_char(look_around(stream, 1));

          push(
            token_stream,
            mktok(CHARACTER, _LINE, _INDEX, 1, reserve_token(1, &character))
          );

          move_stream(stream, (!offs) + 2);
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
        char* start_string_v = new_vector_with_capacity(*start_string_v, 32);
        push(start_string_v, '\"');
        unsigned long index = _INDEX - 1;

        while (get_char(stream) && get_char(stream) != '"') {
          if (get_char(stream) == '\\') {
            move_stream(stream, 1);
            push(start_string_v, escape_char(get_char(stream)));
          } else {
            push(start_string_v, get_char(stream));
          }
          consume(&stream);
        }
        if (!get_char(stream)) {
          is_correct_stream = false;
          push(
            error_buffer,
            mkerr(SCANNER, _LINE, index, "Unmatched string quotation")
          );
          break;
        }

        push(start_string_v, '\"');

        push(
          token_stream, 
          mktok(STRING, _LINE, index, _INDEX - index - 1, start_string_v)
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
        void* start_number = copy_stream_offset(stream, -1);
        unsigned long index = _INDEX - 1;
        unsigned long size;
        TokenType numtype = NATURAL;

      scan_num:
        while (isdigit(get_char(stream)))
          consume(&stream);

        if (get_char(stream) == '.' && numtype == NATURAL) {
          consume(&stream);
          numtype = REAL;
          goto scan_num;
        }

        start_number = reserve_token(size = distance_between(stream, start_number), start_number);

        push(
          token_stream, 
          mktok(numtype, _LINE, index, size, start_number)
        );

        break;
      }
      default: scan_symbol: {
        void* start_token = copy_stream_offset(stream, -1);
        unsigned long index = _INDEX - 1;
        unsigned long size;

        bool is_alnum = isiden(curchar);
        bool (*symbol_scanning)(char) = is_alnum? isiden : isoper;

        while (get_char(stream) && (*symbol_scanning)(get_char(stream))) {
          consume(&stream);
        }

        start_token = reserve_token(size = distance_between(stream, start_token), start_token);
        TokenType toktype = isupper(stream.get_char(start_token))? TYPE_K : follow_pattern_with_default(start_token, syntax_trie, is_alnum? IDENTIFIER : OPERATOR);

        push(
          token_stream, 
          mktok(toktype, _LINE, index, size, start_token)
        );

        if (toktype == INFIXL || toktype == INFIXR)
          push(infixes, &vector_last(token_stream));

        break;
      }
    }
  }

  push(_LINES, stream.stream);
  push(token_stream, mktok(EndOfFile, _LINE, _INDEX, 0, NULL));
  return (Tokens) {
    .is_correct_stream = is_correct_stream,
    .lines = _LINES,
    .scanned.token_stream = token_stream,
    .scanned.infixes = infixes,
    .error_buf = error_buffer
  };
}

#ifdef DEBUG
#pragma GCC pop_options
#endif

/** TODO:
 * - make it add infixes to the infixes vector
 */
