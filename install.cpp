#include "cobbler.h"
#include "cobbler/util.h"
#include <cstdlib>
#include <filesystem>
#include <unistd.h>

int main(int argc, const char **argv) {
  cbl::Cobbler c;

  if (cbl::util::isNewerThan("install.cpp", "install")) {
    cbl::util::rebuildAndRun(c, {"install.cpp"}, "install", argv);
  }

  if (geteuid() != 0) {
    COBBLER_ERROR("Program needs to be ran as root!");
    exit(EXIT_FAILURE);
  }

  COBBLER_LOG("Copying headers!");
  COBBLER_PUSH_INDENT();
  c.cmd("c++", "-fpreprocessed", "-dD", "-E", "-w", "./cobbler.h", "-o",
        "/usr/local/include/cobbler.h");
  c.cmd("mkdir", "-p", "/usr/local/include/cobbler/");
  c.cmd("c++", "-fpreprocessed", "-dD", "-E", "-w", "./cobbler/util.h", "-o",
        "/usr/local/include/cobbler/util.h");
  c();
  COBBLER_POP_INDENT();
  COBBLER_LOG("Done!");
}
