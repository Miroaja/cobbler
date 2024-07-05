#pragma once
#include "../cobbler.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <ios>
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


    usage text:

    printing a usage text goes as follows:

    usage:
      <<program name>> [options/parameters]
  value]...

    required parameters:
      <<longName>> (<<short name>>) [value] : <<description>>
      ...

    options:
      <<long name>> (<<short name>>) = <<default value>> : <<description>>
      ...

    optional parameters:
      <<long name>> (<<short name>>) = <<default value>> : <<description>>
      ...
*/

struct ArgParser {
  enum class Type : uint8_t {
    flag = 0,
    value,
  };
  inline ArgParser(int argc, const char **argv, const std::string &name)
      : _argc(argc), _argv(argv), _name(name) {
    _State s;
    s.type = Type::flag;
    s.longToken = "--help";
    s.shortToken = {};
    s.defaultValue = "";
    s.description = "print this text";
    s.filler = [this]() {
      _usage();
      exit(EXIT_SUCCESS);
    };
    _parserState.push_back(s);
  }

  inline void operator()() {
    for (auto &currentState : _parserState) {
      switch (currentState.type) {
      case Type::flag: {
        const char **end = _argv + _argc;
        int fCount = 0;
        const char *prev = *_argv;
        std::for_each(_argv + 1, end,
                      [&prev, this, currentState, &fCount](const char *v) {
                        bool isInValueField = std::any_of(
                            _parserState.begin(), _parserState.end(),
                            [prev](const _State &t) {
                              return t.type == Type::value &&
                                     (t.longToken == prev ||
                                      (t.shortToken
                                           ? t.shortToken.value() == prev
                                           : false));
                            });

                        if (v == currentState.longToken && !isInValueField) {
                          fCount++;
                        }
                        if (currentState.shortToken) {
                          if (v == currentState.shortToken && !isInValueField) {
                            fCount++;
                          }
                        }
                        prev = v;
                      });

        if (fCount > 1) {
          _error(currentState.longToken, "option present more than once");
        }
        if (std::holds_alternative<bool *>(currentState.filler)) {
          *std::get<bool *>(currentState.filler) = fCount == 1;
        } else if (fCount == 1) {
          std::get<std::function<void(void)>>(currentState.filler)();
        }

      } break;
      case Type::value: {
        const char **end = _argv + _argc;
        int fCount = 0;
        std::string value = "";

        if (*(_argv + _argc - 1) == currentState.longToken) {
          _error(currentState.longToken,
                 "parameter present without without value");
        } else if (currentState.shortToken) {
          if (*(_argv + _argc - 1) == currentState.shortToken.value()) {
            _error(currentState.shortToken.value(),
                   "parameter present without without value");
          }
        }

        for (const char **i = _argv; i != end - 1; i++) {
          if (*i == currentState.longToken) {
            if (fCount++ > 0) {
              _error(currentState.longToken, "option present more than once");
            }
            value = *(i + 1);
          }
          if (currentState.shortToken) {
            if (*i == currentState.shortToken.value()) {
              if (fCount++ > 0) {
                _error(currentState.shortToken.value(),
                       "option present more than once");
              }
              value = *(i + 1);
            }
          }
        }
        if (fCount > 0) {
          if (std::holds_alternative<std::string *>(currentState.filler)) {
            *std::get<std::string *>(currentState.filler) = value;
          } else {
            std::get<std::function<void(const std::string &)>>(
                currentState.filler)(value);
          }
        } else {
          if (!currentState.isOptional) {
            _error(currentState.longToken,
                   "required parameter was not present in args");
          } else {
            if (std::holds_alternative<std::string *>(currentState.filler)) {
              *std::get<std::string *>(currentState.filler) =
                  currentState.defaultValue;
            } else {
              std::get<std::function<void(const std::string &)>>(
                  currentState.filler)(currentState.defaultValue);
            }
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
                         std::optional<std::string> shortname = {},
                         std::optional<std::string> description = {}) {
    if (longName == "--help") {
      COBBLER_ERROR("The flag \"--help\" is reserved.");
    }

    _State s;
    s.type = Type::flag;
    s.longToken = "--" + longName;
    if (shortname) {
      shortname = "-" + shortname.value();
    }
    s.shortToken = shortname;
    s.defaultValue = "";
    s.description = description;
    s.filler = value;
    s.isOptional = true;
    _parserState.push_back(s);
    return *this;
  }

  inline ArgParser &flag(std::function<void(void)> filler,
                         const std::string &longName,
                         std::optional<std::string> shortname = {},
                         std::optional<std::string> description = {}) {
    if (longName == "--help") {
      COBBLER_ERROR("The flag \"--help\" is reserved.");
    }

    _State s;
    s.type = Type::flag;
    s.longToken = "--" + longName;
    if (shortname) {
      shortname = "-" + shortname.value();
    }
    s.shortToken = shortname;
    s.defaultValue = "";
    s.description = description;
    s.filler = filler;
    s.isOptional = true;
    _parserState.push_back(s);
    return *this;
  }

  inline ArgParser &value(std::string *value, const std::string &longName,
                          std::optional<std::string> shortname = {},
                          std::optional<std::string> description = {}) {
    if (longName == "--help") {
      COBBLER_ERROR("The flag \"--help\" is reserved.");
    }

    _State s;
    s.type = Type::value;
    s.longToken = "--" + longName;
    if (shortname) {
      shortname = "-" + shortname.value();
    }
    s.shortToken = shortname;
    s.description = description;
    s.defaultValue = "";
    s.filler = value;
    s.isOptional = false;
    _parserState.push_back(s);
    return *this;
  }

  inline ArgParser &value(std::function<void(const std::string &)> filler,
                          const std::string &longName,
                          std::optional<std::string> shortname = {},
                          std::optional<std::string> description = {}) {
    if (longName == "--help") {
      COBBLER_ERROR("The flag \"--help\" is reserved.");
    }

    _State s;
    s.type = Type::value;
    s.longToken = "--" + longName;
    if (shortname) {
      shortname = "-" + shortname.value();
    }
    s.shortToken = shortname;
    s.description = description;
    s.defaultValue = "";
    s.filler = filler;
    s.isOptional = false;
    _parserState.push_back(s);
    return *this;
  }

  inline ArgParser &opt_value(std::string *value,
                              const std::string &defaultValue,
                              const std::string &longName,
                              std::optional<std::string> shortname = {},
                              std::optional<std::string> description = {}) {
    if (longName == "--help") {
      COBBLER_ERROR("The flag \"--help\" is reserved.");
    }

    _State s;
    s.type = Type::value;
    s.longToken = "--" + longName;
    if (shortname) {
      shortname = "-" + shortname.value();
    }
    s.shortToken = shortname;
    s.description = description;
    s.defaultValue = defaultValue;
    s.filler = value;
    s.isOptional = true;
    _parserState.push_back(s);
    return *this;
  }

  inline ArgParser &opt_value(std::function<void(const std::string &)> filler,
                              const std::string &defaultValue,
                              const std::string &longName,
                              std::optional<std::string> shortname = {},
                              std::optional<std::string> description = {}) {
    if (longName == "--help") {
      COBBLER_ERROR("The flag \"--help\" is reserved.");
    }

    _State s;
    s.type = Type::value;
    s.longToken = "--" + longName;
    if (shortname) {
      shortname = "-" + shortname.value();
    }
    s.shortToken = shortname;
    s.description = description;
    s.defaultValue = defaultValue;
    s.filler = filler;
    s.isOptional = true;
    _parserState.push_back(s);
    return *this;
  }

private:
  void _error(const std::string &token, const std::string &reason) {
    COBBLER_ERROR("Incorrect argument \"%s\" : %s", token.c_str(),
                  reason.c_str());
    exit(EXIT_FAILURE);
  }

  void _usage() {
    bool containsFlags = std::any_of(
        _parserState.begin(), _parserState.end(), [](const _State &s) {
          return s.type == Type::flag && s.longToken != "--help";
        });
    bool containsValues =
        std::any_of(_parserState.begin(), _parserState.end(),
                    [](const _State &s) { return s.type == Type::value; });

    std::cout << "usage:\n  " << _name;
    switch ((containsFlags ? 0b01 : 0b00) | (containsValues ? 0b10 : 0b00)) {
    case 0b01: {
      std::cout << " [options]\n\n";
    } break;
    case 0b10: {
      std::cout << " [parameters]\n\n";
    } break;
    case 0b11: {
      std::cout << " [options/parameters]\n\n";
    } break;
    default: {
      exit(EXIT_SUCCESS);
    } break;
    }

    if (std::any_of(_parserState.begin(), _parserState.end(),
                    [](const _State &s) {
                      return s.type == Type::value && !s.isOptional;
                    })) {
      std::cout << "required parameters:\n";
      for (const _State &s : _parserState) {
        if (s.type == Type::value && !s.isOptional) {
          std::cout << "  " << s.longToken;
          if (s.shortToken) {
            std::cout << " (" << s.shortToken.value() << ")";
          }
          std::cout << " [value]";
          if (s.description) {
            std::cout << " : " << s.description.value();
          }
          std::cout << "\n";
        }
      }
    }

    if (containsFlags) {
      std::cout << "\noptions:\n";
      for (const _State &s : _parserState) {
        if (s.type == Type::flag) {
          std::cout << "  " << s.longToken;
          if (s.shortToken) {
            std::cout << " (" << s.shortToken.value() << ")";
          }
          if (s.description) {
            std::cout << " : " << s.description.value();
          }
          std::cout << "\n";
        }
      }
    }
    if (std::any_of(_parserState.begin(), _parserState.end(),
                    [](const _State &s) {
                      return s.type == Type::value && s.isOptional;
                    })) {
      std::cout << "\noptional parameters:\n";
      for (const _State &s : _parserState) {
        if (s.type == Type::value && !s.isOptional) {
          std::cout << "  " << s.longToken;
          if (s.shortToken) {
            std::cout << " (" << s.shortToken.value() << ")";
          }
          std::cout << " = " << s.defaultValue;
          if (s.description) {
            std::cout << " : " << s.description.value();
          }
          std::cout << "\n";
        }
      }
    }
    std::cout.flush();
    exit(EXIT_SUCCESS);
  }

  struct _State {
    ArgParser::Type type;

    bool isOptional;
    std::string defaultValue;

    std::string longToken;
    std::optional<std::string> shortToken;
    std::optional<std::string> description;
    std::variant<bool *, std::string *,
                 std::function<void(const std::string &)>,
                 std::function<void(void)>>
        filler;
  };

  std::vector<_State> _parserState;
  int _argc;
  const char **_argv;
  std::string _name;
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
                          std::filesystem::path target, const char **argv,
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
  // Evil const dropping cast
  execvp(target.c_str(), (char *const *)argv);

  auto errorval = errno;
  COBBLER_ERROR("Excec encountered an error: %s", strerror(errorval));
  exit(EXIT_FAILURE);
}

} // namespace util
} // namespace cbl
