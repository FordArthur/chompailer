#include "chompailer.h"
#include "typechecker.h"

static inline char consume_charptr(void** stream_ptr) {
  return *((*(char**) stream_ptr)++);
}

static inline char get_from_charptr(void* stream_ptr) {
  return *((char*) stream_ptr);
}

static inline char look_around_charptr(void* stream, long offset) {
  return ((char*) stream)[offset];
}

static inline void move_charptr(void** stream_ptr, long offset) {
  *(char**)stream_ptr += offset;
}

static inline void* copy_charptr_offset(void* stream, long offset) {
  return ((char*) stream) + offset;
}

static inline unsigned long distance_between_charptrs(void* stream, void* other_stream) {
  return ((char*) stream) - (char*) other_stream;
}

static Stream command_line_stream = (Stream) {
  .stream = NULL,
  .consume_char = consume_charptr,
  .get_char = get_from_charptr,
  .look_around = look_around_charptr,
  .move_stream = move_charptr,
  .copy_stream_offset = copy_charptr_offset,
  .distance_between = distance_between_charptrs
};

int main(int argc, char *argv[]) {
  if (argc != 3) {
    printf("The Glorious Chompiler, Beta\n\tUsage: [function] [argument]\n\n\t(For now) the only functions available are:\n\t\ts\tuse scanner on argument\n\t\tp\tuse parser on argument\n\t\tt\tuse typechecker on argument\n");
    return 1;
  }
  command_line_stream.stream = argv[2];
  Tokens stream = scanner(command_line_stream);
  if (!stream.is_correct_stream) {
    for_each(i, stream.error_buf)
    report_error(stream.error_buf + i, (char**) stream.lines);
    return 1;
  }

  if (argv[1][0] == 's') return 0;

  AST ast = parser(stream.scanned.token_stream, stream.scanned.infixes, stream.error_buf);
  if (!ast.is_correct_ast) {
    for_each(i, stream.error_buf)
    report_error(stream.error_buf + i, (char**) stream.lines);
    return 1;
  }

  if (argv[1][0] == 't' && !checker(ast.ast, ast.error_buf)) {
    for_each(i, stream.error_buf)
    report_error(stream.error_buf + i, (char**) stream.lines);
    return 1;
  }
  for_each(i, ast.ast) {
    print_AST(ast.ast + i);
    printf("\n");
  }
  return 0;
}
