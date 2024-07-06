#include "chompailer.h"
#include "parser.h"
#include "vec.h"

int main(int argc, char *argv[]) {
  if (argc != 3) {
    printf("The Glorious Chompiler, Beta\n\tUsage: [function] [argument]\n\n\t(For now) the only functions available are:\n\t\ts\tuse scanner on argument\n\t\tp\tuse parser on argument\n");
    return 1;
  }
  switch (argv[1][0]) {
    case 's': {
      Tokens stream = scanner(argv[2]);

      if (stream.is_correct_stream) {
        for_each(i, stream.scanned.token_stream)
        print_token(stream.scanned.token_stream[i]);
      } else {
        for_each(i, stream.error_buf)
        report_error(stream.error_buf[i], stream.lines);
      }
      break;
    }
    case 'p': {
      Tokens stream = scanner(argv[2]);
      if (!stream.is_correct_stream) {
        for_each(i, stream.error_buf)
          report_error(stream.error_buf[i], stream.lines);
        return 1;
      }
      AST ast = parser(stream.scanned.token_stream, stream.scanned.infixes, stream.error_buf);
      if (!ast.is_correct_ast) {
        for_each(i, stream.error_buf)
          report_error(stream.error_buf[i], stream.lines);
        return 1;
      }
      for_each(i, ast.ast)
        print_AST(ast.ast + i);

      break;
    }

  }
  return 0;
}
