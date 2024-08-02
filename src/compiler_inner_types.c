#include "compiler_inner_types.h"

#define RESET       "\033[0m"
#define BLACK       "\033[30m"
#define RED         "\033[31m"
#define GREEN       "\033[32m"
#define YELLOW      "\033[33m"
#define BLUE        "\033[34m"
#define MAGENTA     "\033[35m"
#define CYAN        "\033[36m"
#define WHITE       "\033[37m"
#define BOLDBLACK   "\033[1m\033[30m"
#define BOLDRED     "\033[1m\033[31m"
#define BOLDGREEN   "\033[1m\033[32m"
#define BOLDYELLOW  "\033[1m\033[33m"
#define BOLDBLUE    "\033[1m\033[34m"
#define BOLDMAGENTA "\033[1m\033[35m"
#define BOLDCYAN    "\033[1m\033[36m"
#define BOLDWHITE   "\033[1m\033[37m"

void report_error(const Error* err, char** lines) {
//  printf("\t" BOLDRED "|" RESET "\n   " YELLOW "%lu" RESET "\t" BOLDRED "|" RESET RED " %s" RESET "\n\t" BOLDRED "|" RESET YELLOW " ^ %lu" RESET "\n\n", err.line, err.err, err.index);
  printf(BOLDRED "\t|\n" RESET);
  *(lines[err->line]) = '\0';
  printf(YELLOW " %lu:%lu\t" RESET BOLDRED "|" RESET " %s\n", err->line, err->index, lines[err->line - 1]);
  printf(BOLDRED "\t|" RESET);
  for (unsigned long pad = err->index; pad > 0; pad--)
    printf(" ");
  printf(YELLOW "^ %s\n" RESET, err->err);
  printf(BOLDRED "\t|\n" RESET);
  *(lines[err->line]) = '\n';
}

