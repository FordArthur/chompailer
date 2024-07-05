#include "chompailer.h"

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
    case 'p':
      printf("Unimplemented, sorry!\n");
      break;
  }
}
