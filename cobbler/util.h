#pragma once
#include "../cobbler.h"
#include <cstdio>
#include <filesystem>

namespace cbl {
namespace util {
template <io TYPE = io::async, typename... S>
inline std::filesystem::path
compile(Cobbler &c, const std::filesystem::path &unit,
        const std::filesystem::path &target, const S &...extraFlags) {
  c.cmd<TYPE>({}, {}, "c++", "-c", unit.string(), "-o",
              (target / (unit.stem().string() + ".o")).string(), extraFlags...);

  return (target / unit.stem()).string() + ".o";
}
template <io TYPE = io::async, typename... S>
inline void link(Cobbler &c, const std::vector<std::filesystem::path> &objects,
                 const std::filesystem::path &target, const std::string &name,
                 const S &...extraFlags) {
  std::vector<std::string> command = {};
  command.push_back("ld");
#if !defined(WIN32)
  // FIX: These need to be accessed in a more standard way
  // Current commands are taken from ""clang++ -v"" of the test file. Most of it
  // is thus pretty standard, but especially the search paths could be
  // troublesome.
  command.push_back("-pie");
  command.push_back("--hash-style=gnu");
  command.push_back("--build-id");
  command.push_back("--eh-frame-hdr");
  command.push_back("-m");
  command.push_back("elf_x86_64");
  command.push_back("-dynamic-linker");
  command.push_back("/lib64/ld-linux-x86-64.so.2");
  command.push_back("-o");
  command.push_back("a.out");
  command.push_back("/usr/bin/../lib64/gcc/x86_64-pc-linux-gnu/14.1.1/../../../"
                    "../lib64/Scrt1.o");
  command.push_back("/usr/bin/../lib64/gcc/x86_64-pc-linux-gnu/14.1.1/../../../"
                    "../lib64/crti.o");
  command.push_back(
      "/usr/bin/../lib64/gcc/x86_64-pc-linux-gnu/14.1.1/crtbeginS.o");
  command.push_back("-L/usr/bin/../lib64/gcc/x86_64-pc-linux-gnu/14.1.1");
  command.push_back(
      "-L/usr/bin/../lib64/gcc/x86_64-pc-linux-gnu/14.1.1/../../../../lib64");
  command.push_back("-L/lib/../lib64 -L/usr/lib/../lib64");
  command.push_back("-L/lib");
  command.push_back("-L/usr/lib");
  command.push_back("-lstdc++");
  command.push_back("-lm");
  command.push_back("-lgcc_s");
  command.push_back("-lgcc");
  command.push_back("-lc");
  command.push_back("-lgcc_s");
  command.push_back("-lgcc");
  command.push_back(
      "/usr/bin/../lib64/gcc/x86_64-pc-linux-gnu/14.1.1/crtendS.o");
  command.push_back("/usr/bin/../lib64/gcc/x86_64-pc-linux-gnu/14.1.1/../../../"
                    "../lib64/crtn.o");
#else // I have no idea how windows does this shit
  // TODO: Implement!
#endif
  command.push_back("-lc");
  for (const auto &c : objects) {
    command.push_back(c.string());
  }

  command.push_back("-o");
  command.push_back((target / name).string());
  auto rrg = __internal::splatVariadicToArgVector(extraFlags...);
  command.insert(command.end(), rrg.begin(), rrg.end());
  c.cmd<TYPE>({}, {}, command);
}

inline void selfRebuild() {
  // TODO: Implement
}

} // namespace util
} // namespace cbl
