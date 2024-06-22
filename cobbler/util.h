#pragma once
#include "../cobbler.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <tuple>
#include <uchar.h>
#include <unistd.h>
#include <variant>
#include <vector>

namespace cbl {
namespace util {

/* TODO: Parsing commandline args

  Command line args are either:
  A flag
  or
  A value

  Flags are defined by their format:
    flags always start with a "-"

    flags are can be in long form, e.g.
    "--flag-that-exists", and also (not separately)
     a short form e.g. "-f"

    flags always take a boolean reference to fill

  Anything that is not a flag is a value:
    values are always strings

    values may be optional, but must always
    follow a flag or exist at the extremes of the args


    if a value is attached to a flag, that flag no longer
    provides a boolean value to fill. Instead the value
    associated with that argument is either filled into
    a string reference, or is passed to a functor
    void(const std::string&) to be parsed and assigned further

    if a value is declared as required and is associated
    with a flag, that flag becomes a required feature as well
    e.g. the value "mode" attached to flag "-m" is required,
    making the flag and the value required in the format:
    "-m mode"


    usage:

     TODO: write this!!!
*/

struct ArgParser {
  enum class Type : uint8_t {
    flag = 0,
    value,
  };
  inline void operator()() {
    for (auto &t : _parserState) {
      switch (t.type) {
      case Type::flag: {
        const char **end = _argv + _argc;
        int fCount = 0;
        const char *prev = *_argv;
        std::for_each(_argv + 1, end, [&prev, this, t, &fCount](const char *v) {
          bool isInValueField = std::any_of(
              _parserState.begin(), _parserState.end(),
              [prev](const _State &t) {
                return t.type == Type::value &&
                       (t.longToken == prev ||
                        (t.shortToken ? t.shortToken.value() == prev : false));
              });

          if (v == t.longToken && !isInValueField) {
            fCount++;
          }
          if (t.shortToken) {
            if (v == t.shortToken && !isInValueField) {
              fCount++;
            }
          }
          prev = v;
        });

        if (fCount > 1) {
          _error(t.longToken, "flag present more than once");
        }
        *std::get<bool *>(t.filler) = fCount == 1;
      } break;
      case Type::value: {
        const char **end = _argv + _argc;
        int fCount = 0;
        std::string value = "";

        if (*(_argv + _argc - 1) == t.longToken) {
          _error(t.longToken, "value flag was at the end of command");
        } else if (t.shortToken) {
          if (*(_argv + _argc - 1) == t.shortToken.value()) {
            _error(t.shortToken.value(),
                   "value flag was at the end of command");
          }
        }

        for (const char **i = _argv; i != end - 1; i++) {
          if (*i == t.longToken) {
            if (fCount++ > 0) {
              _error(t.longToken, "flag present more than once");
            }
            value = *(i + 1);
          }
          if (t.shortToken) {
            if (*i == t.shortToken.value()) {
              if (fCount++ > 0) {
                _error(t.shortToken.value(), "flag present more than once");
              }
              value = *(i + 1);
            }
          }
        }
        if (fCount > 0) {
          if (std::holds_alternative<std::string *>(t.filler)) {
            *std::get<std::string *>(t.filler) = value;
          } else {
            std::get<std::function<void(const std::string &)>>(t.filler)(value);
          }
        } else {
          if (!t.isOptional) {
            _error(t.longToken, "required value was not present in args");
          }
        }
      } break;
      default: {
        // Is not realistically reachable
      }
      }
    }
  }

  inline ArgParser &flag(bool *value, const std::string &longName,
                         std::optional<std::string> shortname = {}) {
    // TODO: implement pushing flags
    return *this;
  }

  inline ArgParser &value(std::string *value, const std::string &longName,
                          std::optional<std::string> shortname = {}) {
    // TODO: see above
    return *this;
  }

  inline ArgParser &value(std::function<void(const std::string &)>,
                          const std::string &longName,
                          std::optional<std::string> shortname = {}) {
    // TODO: see above
    return *this;
  }

private:
  void _error(const std::string &token, const std::string &reason) {
    // TODO: Print usage!
    exit(EXIT_FAILURE);
  }

  struct _State {
    ArgParser::Type type;

    bool isOptional;

    std::string longToken;
    std::optional<std::string> shortToken;

    std::variant<bool *, std::string *,
                 std::function<void(const std::string &)>>
        filler;
  };

  std::vector<_State> _parserState;
  int _argc;
  const char **_argv;
};

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
