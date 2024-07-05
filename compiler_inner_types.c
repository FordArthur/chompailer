#include "compiler_inner_types.h"
#include <stdio.h>

#define RESET   "\033[0m"
#define BLACK   "\033[30m"      /* Black */
#define RED     "\033[31m"      /* Red */
#define GREEN   "\033[32m"      /* Green */
#define YELLOW  "\033[33m"      /* Yellow */
#define BLUE    "\033[34m"      /* Blue */
#define MAGENTA "\033[35m"      /* Magenta */
#define CYAN    "\033[36m"      /* Cyan */
#define WHITE   "\033[37m"      /* White */
#define BOLDBLACK   "\033[1m\033[30m"      /* Bold Black */
#define BOLDRED     "\033[1m\033[31m"      /* Bold Red */
#define BOLDGREEN   "\033[1m\033[32m"      /* Bold Green */
#define BOLDYELLOW  "\033[1m\033[33m"      /* Bold Yellow */
#define BOLDBLUE    "\033[1m\033[34m"      /* Bold Blue */
#define BOLDMAGENTA "\033[1m\033[35m"      /* Bold Magenta */
#define BOLDCYAN    "\033[1m\033[36m"      /* Bold Cyan */
#define BOLDWHITE   "\033[1m\033[37m"      /* Bold White */

void report_error(const Error err, char** lines) {
//  printf("\t" BOLDRED "|" RESET "\n   " YELLOW "%lu" RESET "\t" BOLDRED "|" RESET RED " %s" RESET "\n\t" BOLDRED "|" RESET YELLOW " ^ %lu" RESET "\n\n", err.line, err.err, err.index);
  printf(BOLDRED "\t|\n" RESET);
  *(lines[err.line]) = '\0';
  printf(YELLOW "    %lu\t" RESET BOLDRED "|" RESET " %s\n", err.line, lines[err.line - 1]);
  *(lines[err.line]) = '\n';
  printf(BOLDRED "\t|" RESET);
  for (unsigned long pad = err.index; pad > 0; pad--)
    printf(" ");
  printf(YELLOW "^ %s\n" RESET, err.err);
  printf(BOLDRED "\t|\n" RESET);
}
