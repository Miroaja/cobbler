#pragma once
#include "../cobbler.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <unistd.h>

namespace cbl {
namespace util {

/* TODO: Parsing commandline args
//
//  Command line args are either:
//  A flag
//  or
//  A value
//
//  Flags are defined by their format:
//    flags always start with a "-"
//
//    flags are can be in long form, e.g.
//    "--flag-that-exists", and also (not separately)
//     a short form e.g. "-f"
//
//    flags always take a boolean reference to fill
//
//  Anything that is not a flag is a value:
//    values are always strings
//
//    values may be optional, but must always
//    follow a flag or exist at the extremes of the args
//
//
//    if a value is attached to a flag, that flag no longer
//    provides a boolean value to fill. Instead the value
//    associated with that argument is either filled into
//    a string reference, or is passed to a functor
//    void(const std::string&) to be parsed and assigned further
//
//    if a value is declared as required and is associated
//    with a flag, that flag becomes a required feature as well
//    e.g. the value "mode" attached to flag "-m" is required,
//    making the flag and the value required in the format:
//    "-m mode"
//
//
//    usage:
//
//     TODO: write this!!!
*/

template <io TYPE = io::async, typename... S>
inline std::filesystem::path
compile(Cobbler &c, const std::filesystem::path &unit,
        const std::filesystem::path &targetPath, const S &...extraFlags) {
  c.cmd<TYPE>({}, {}, "c++", "-c", unit.string(), "-o",
              (targetPath / (unit.stem().string() + ".o")).string(),
              extraFlags...);

  return (targetPath / unit.stem()).string() + ".o";
}

template <io TYPE = io::async, typename... S>
inline void link(Cobbler &c, const std::vector<std::filesystem::path> &objects,
                 const std::filesystem::path &target, const S &...extraFlags) {
  std::vector<std::string> command = {};
  command.push_back("ld");
#if !defined(WIN32)
  // turns out there is no good way to do this simply, best that can be done is
  // to run c++ -v on a file and flatten the args for each system (currently
  // this works pretty much perfectly for clang on linux)
  command.push_back("-pie");
  command.push_back("--hash-style=gnu");
  command.push_back("--build-id");
  command.push_back("--eh-frame-hdr");
  command.push_back("-m");
  command.push_back("elf_x86_64");
  command.push_back("-dynamic-linker");
  command.push_back("/lib64/ld-linux-x86-64.so.2");
  command.push_back("/usr/lib64/Scrt1.o");
  command.push_back("/usr/lib64/crti.o");
  command.push_back("/usr/lib64/gcc/x86_64-pc-linux-gnu/14.1.1/crtbeginS.o");
  command.push_back("-L/usr/lib64/gcc/x86_64-pc-linux-gnu/14.1.1");
  command.push_back("-L/usr/lib64");
  command.push_back("-L/lib64 -L/usr/lib64");
  command.push_back("-L/lib");
  command.push_back("-L/usr/lib");
  command.push_back("-lstdc++");
  command.push_back("-lm");
  command.push_back("-lgcc_s");
  command.push_back("-lgcc");
  command.push_back("-lc");
  command.push_back("-lgcc_s");
  command.push_back("-lgcc");
  command.push_back("/usr/lib64/gcc/x86_64-pc-linux-gnu/14.1.1/crtendS.o");
  command.push_back("/usr/lib64/crtn.o");
#else // I have no idea how windows does this shit
  // TODO: Implement!
#endif
  command.push_back("-lc");
  for (const auto &c : objects) {
    command.push_back(c.string());
  }

  command.push_back("-o");
  command.push_back(target.string());
  auto rrg = __internal::splatVariadicToArgVector(extraFlags...);
  command.insert(command.end(), rrg.begin(), rrg.end());
  c.cmd<TYPE>({}, {}, command);
}

inline bool isNewerThan(const std::filesystem::path &a,
                        const std::filesystem::path &b) {
  return std::filesystem::last_write_time(a) >
         std::filesystem::last_write_time(b);
}

template <typename... S>
inline void rebuildAndRun(Cobbler &c, std::vector<std::filesystem::path> units,
                          std::filesystem::path target, char *const *argv,
                          const S &...extraFlags) {

  for (auto &unit : units) {
    if (!unit.is_absolute()) {
      unit = (std::filesystem::current_path() / unit);
    }
  }
  if (!target.is_absolute()) {
    target = (std::filesystem::current_path() / target);
  }

  COBBLER_LOG("Rebuilding self...");
  c.clear();

  for (const auto &unit : units) {
    util::compile(c, unit, target.parent_path(), extraFlags...);
  }
  c();
  c.clear();
  for (auto &unit : units) {
    unit = target.parent_path() / (unit.stem().string() + ".o");
  }
  util::link<io::sync>(c, units, target);
  for (const auto &unit : units) {
    c.cmd({}, {}, "rm", (unit.stem().string() + ".o"));
  }
  c();
  COBBLER_LOG("Restarting program %s", target.c_str());
  execvp(target.c_str(), argv);
  auto errorval = errno;
  COBBLER_ERROR("Excec encountered an error: %s", strerror(errorval));
  exit(EXIT_FAILURE);
}

} // namespace util
} // namespace cbl
