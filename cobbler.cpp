#include <cobbler.h>
#include <cobbler/util.h>
#include <cstdlib>
#include <filesystem>
#include <unistd.h>

int main(int argc, const char **argv) {
  cbl::Cobbler c;

  if (cbl::util::isNewerThan("cobbler.cpp", "install")) {
    cbl::util::rebuildAndRun(c, {"cobbler.cpp"}, "install", argv);
  }

  if (geteuid() != 0) {
    COBBLER_ERROR("Program needs to be ran as root!");
    exit(EXIT_FAILURE);
  }

  COBBLER_LOG("Copying headers!");
  c.cmd({}, {}, "cp", "-rf",
        (std::filesystem::current_path() / "cobbler.h").string(),
        "/usr/local/include/");
  c.cmd({}, {}, "cp", "-rf",
        (std::filesystem::current_path() / "cobbler").string(),
        "/usr/local/include/");
  c();
  COBBLER_LOG("Done!");
}
