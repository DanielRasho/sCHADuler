#include <stdio.h>

#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "./deps/nob.h/nob.h"

#define BUILD_FOLDER "build/"
#define SRC_FOLDER "src/"
#define DEPS_FOLDER "deps/"

#define HELP                                                                   \
  "Usage: nob [-compiler options]\n"                                           \
  "Compiler options:\n"                                                        \
  "* -v: Compile with verbosity enabled.\n"

bool args_contains(int argc, char **argv, const char *arg, int arg_length) {
  if (argc <= 1) {
    return false;
  }

  for (int i = 0; i < argc; i++) {
    if (memcmp(argv[i], arg, arg_length) == 0) {
      return true;
    }
  }

  return false;
}

int main(int argc, char **argv) {
  NOB_GO_REBUILD_URSELF(argc, argv);

  if (args_contains(argc, argv, "-h", 2)) {
    fprintf(stderr, HELP);
    return 1;
  }

  Cmd cmd = {0};

  bool compile_with_verbosity = false;
  if (args_contains(argc, argv, "-v", 2)) {
    nob_log(NOB_WARNING, "Will compile with verbosity!");
    compile_with_verbosity = true;
  }

  if (!mkdir_if_not_exists(BUILD_FOLDER)) {
    return 1;
  }

  String_Builder sb = {0};
  sb_append_cstr(&sb, "clang ");
  if (compile_with_verbosity) {
    sb_append_cstr(&sb, "-v ");
  }

  sb_append_cstr(&sb, "-Wall ");
  sb_append_cstr(&sb, "-o " BUILD_FOLDER "main " SRC_FOLDER "main.c");

  nob_cmd_append(&cmd, "bash", "-c", sb.items);
  if (!nob_cmd_run_sync_and_reset(&cmd)) {
    return 1;
  }
  return 0;
}
