#include "cobbler/util.h"
#include <filesystem>
#include <vector>

const std::string buildPath = "bin/";
const std::string exeName = "out";

int main(void) {
  cbl::Cobbler c;
  // std::string files = "";
  //  c.cmd({}, {}, "mkdir", "-p", buildPath);
  //  for (std::filesystem::directory_entry e :
  //       std::filesystem::directory_iterator(std::filesystem::current_path()))
  //       {
  //    if (e.path().has_extension() && e.path().extension() == ".cpp") {
  //      COBBLER_LOG("Adding %s to compilation...",
  //                  e.path().filename().string().c_str());
  //      files += e.path().string();
  //    }
  //  }
  //  c.cmd<cbl::io::async>({}, {}, "clang++", "-std=c++20", files, "-o",
  //                        buildPath + exeName);
  //  COBBLER_LOG("I'm stuff");
  //  COBBLER_ERROR("I'm stuff but angry");
  std::vector<std::filesystem::path> objs;
  objs.push_back(cbl::util::compile(
      c, "test.cpp", std::filesystem::current_path() / "bin" / "int",
      "-std=c++20"));
  c();
  c.clear();
  cbl::util::link(c, objs, std::filesystem::current_path() / "bin", "test.elf");
  c();
}
