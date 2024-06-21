#include "kernel/param.h"
#include "user/user.h"

/**
 * read a line from standard input, zero out blanks, and
 * set arguments.
 *
 * @param buf the row buffer
 * @param args splitted arguments
 * @return number of characters read.
 */
int parseline(char *buf, char **args) {
  // iterator
  int it = 0;
  // start of a token
  int start = -1;
  // # tokens.
  int tokens = 0;
  char ch;

  while (read(0, &ch, 1) == 1) {
    // if not EOF
    switch (ch) {
    case '\n': {
      // end of line, parse last token
      if (start >= 0) {
        args[tokens++] = buf + start;
      }
      buf[it++] = 0;
      it--;
      goto endparse;
    }

    case ' ': {
      // end of token, should add to args.
      if (start >= 0) {
        args[tokens++] = buf + start;
      }
      buf[it++] = 0;
      start = -1;
      break;
    }

    default: {
      if (start < 0) {
        start = it;
      }
      buf[it++] = ch;
      break;
    }
    }
  }

  args[tokens++] = 0;

endparse:
  return it;
}

/*
  Write a simple version of the UNIX xargs program:
  read lines from the standard input and run a command for each line,
  supplying the line as arguments to the command.
 */
int main(int argc, char **argv) {
  /** buffer(suppose line length < 512) */
  char buf[512];
  char *args[MAXARG];
  char *exe; // executable file name
  int arg_start = 1;

  // parse executable and primal args.
  if (strcmp(argv[1], "-n") != 0) {
    exe = argv[1];
    for (int i = 2; i < argc; ++i) {
      args[i - 1] = argv[i];
      ++arg_start;
    }
  } else {
    exe = argv[3];
    for (int i = 4; i < argc; ++i) {
      args[i - 3] = argv[i];
      ++arg_start;
    }
  }
  args[0] = exe;

parse:
  int ret = parseline(buf, args + arg_start);
  if (ret == 0) {
    exit(0);
  }

  int child = fork();
  if (child == 0) {
    exec(exe, args);
    exit(0);
  } else {
    [[maybe_unused]] int tmp;
    wait(&tmp);
  }

  goto parse;

  exit(0);
}
