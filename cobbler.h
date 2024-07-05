#pragma once
#include <atomic>
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <optional>
#include <sstream>
#include <string.h>
#include <string>
#include <sys/wait.h>
#include <thread>
#include <uchar.h>
#include <unistd.h>
#include <vector>

// VERSION : 0.0.4;
namespace cbl {
namespace __internal {
inline std::mutex printMux;
inline std::atomic_int indentLevel;
} // namespace __internal
} // namespace cbl

#define COBBLER_PUSH_INDENT()                                                  \
  {                                                                            \
    std::scoped_lock printLock(cbl::__internal::printMux);                     \
    cbl::__internal::indentLevel++;                                            \
  }
#define COBBLER_POP_INDENT()                                                   \
  {                                                                            \
    std::scoped_lock printLock(cbl::__internal::printMux);                     \
    cbl::__internal::indentLevel--;                                            \
  }

#if !defined(WIN32)
#define COBBLER_LOG(msg, ...)                                                  \
  {                                                                            \
    std::scoped_lock printLock(cbl::__internal::printMux);                     \
    for (int i = 0; i < cbl::__internal::indentLevel; i++) {                   \
      printf("  ");                                                            \
    }                                                                          \
    printf("\033[0;34m[INFO] " msg "\033[0m\n", ##__VA_ARGS__);                \
  }
#define COBBLER_WARN(msg, ...)                                                 \
  {                                                                            \
    std::scoped_lock printLock(cbl::__internal::printMux);                     \
    for (int i = 0; i < cbl::__internal::indentLevel; i++) {                   \
      printf("  ");                                                            \
    }                                                                          \
    printf("\033[0;33m[WARNING] " msg "\033[0m\n", ##__VA_ARGS__);             \
  }
#define COBBLER_ERROR(msg, ...)                                                \
  {                                                                            \
    std::scoped_lock printLock(cbl::__internal::printMux);                     \
    for (int i = 0; i < cbl::__internal::indentLevel; i++) {                   \
      fprintf(stderr, "  ");                                                   \
    }                                                                          \
    fprintf(stderr, "\033[0;31m[ERROR] " msg "\033[0m\n", ##__VA_ARGS__);      \
  }
#else
/// TODO: DO WIN32 HERE!!!
#endif

namespace cbl {
namespace __internal {
template <class... S>
inline std::string concatenateVariadic(S const &...strings) {
  std::stringstream stream;
  using List = int[];
  (void)List{0, ((void)(stream << strings << " "), 0)...};
  const auto &s = stream.str();
  return s.substr(0, s.length() - 1);
}
template <class... S>
inline std::vector<std::string> splatVariadicToArgVector(S const &...strings) {
  using List = std::vector<std::string>;
  List l{std::string(strings)...};
  return l;
}
inline std::vector<const char *>
toLocalArglist(const std::vector<std::string> &args) {
  std::vector<const char *> result;
  for (auto &s : args) {
    result.push_back(s.c_str());
  }
  result.push_back(nullptr);
  return result;
}
} // namespace __internal

template <typename T> class expected {
  // TODO: implement
};

enum class io : uint8_t { sync = 0, async };
struct Pipe {
  inline Pipe() { pipe(_fds); }

  inline std::string get() {
    char buf;
    std::string ret = "";
    while (read(_fds[1], &buf, 1) > 0) {
      ret.push_back(buf);
    }
    return ret;
  }

private:
  friend class Cobbler;
  int _fds[2] = {};
  void _close() {
    close(_fds[0]);
    close(_fds[1]);
  }
};

// Commands are pushed into a "Cobbler"
// Commands can be pushed in any of the following type:
//
// S/ynchronous - these commands are executed in order,
// their output is stored in a pipe if such a pipe is declared at the appension
// point, which can be accessed by any command in synchronous or asynchronous
// order.
//
// A/synchronous - these commands are forked into a separate thread when their
// appension point is reached.
struct Cobbler {
  inline void operator()() {
    _completedAsyncs = 0;
    _asyncCounter = 0;
    for (_Command &c : _commands) {
      COBBLER_LOG("Executing %s command: %s",
                  c.calltype == io::async ? "asynchronous" : "synchronous",
                  c.call.front().c_str());
      _executeCommand(c);
    }
    COBBLER_LOG("Waiting for all commands to finish");
    while (_completedAsyncs != _asyncCounter) { /***/
    }
  }

  inline void clear() { _commands.clear(); }

  template <io TYPE = io::sync, typename... S>
  inline Cobbler &cmd(std::optional<Pipe> in, std::optional<Pipe> out,
                      S const &...command) {
    _commands.push_back(
        {.calltype = TYPE,
         .call = __internal::splatVariadicToArgVector(command...),
         .outPipe = out,
         .inPipe = in});
    return (*this);
  }

  template <io TYPE = io::sync>
  inline Cobbler &cmd(std::optional<Pipe> in, std::optional<Pipe> out,
                      const std::vector<std::string> &command) {
    _commands.push_back(
        {.calltype = TYPE, .call = command, .outPipe = out, .inPipe = in});
    return (*this);
  }

private:
  struct _Command {
    io calltype;
    std::vector<std::string> call;
    std::optional<Pipe> outPipe;
    std::optional<Pipe> inPipe;
  };

  inline void _executeCommand(_Command &c) {
    pid_t cPid = fork();
    if (cPid < 0) { /* ERROR */
      assert("Could not create child!");
    } else if (cPid == 0) { /* CHILD */
      if (c.inPipe) {
        if (dup2(c.inPipe.value()._fds[0], STDIN_FILENO) == -1) {
          auto errorval = errno;
          COBBLER_ERROR("Could not link inPipe to stdin: %s",
                        strerror(errorval));
          abort();
        }
        c.inPipe.value()._close();
      }
      if (c.outPipe) {
        if (dup2(c.outPipe.value()._fds[1], STDOUT_FILENO) == -1) {
          auto errorval = errno;
          COBBLER_ERROR("Could not link outPipe to stdout: %s",
                        strerror(errorval));
          abort();
        }

        c.outPipe.value()._close();
      }

      auto v = __internal::toLocalArglist(c.call);
      std::filesystem::path p = c.call[0];
      execvp(p.filename().c_str(), const_cast<char *const *>(v.data()));
      auto errorval = errno;
      COBBLER_ERROR("Excec encountered an error: %s", strerror(errorval));
      exit(EXIT_FAILURE);
    } else { /* PARENT */

      auto handleError = [cPid]() {
        std::string option = "";
        COBBLER_ERROR("Child exited abnormally! [C]ontinue or [e]xit: ");
      invalidChoice:
        std::getline(std::cin, option);
        if (option.empty()) {
          return;
        }
        char8_t choise = std::tolower(option[0]);
        switch (choise) {
        case 'e': {
          COBBLER_ERROR("Child process %i %s", cPid,
                        "exited abnormally, continuation cancelled!\n")
          abort();
        }
        case 'c': {
          return;
        }
        default: {
          COBBLER_ERROR("\n[C]ontinue or [e]xit: ");
          option = "";
          goto invalidChoice;
        }
        }
      };
      if (c.calltype == io::sync) {
        int status;
        waitpid(cPid, &status, 0);
        if (!WIFEXITED(status)) {
          handleError();
        }
        if (c.inPipe) {
          c.inPipe.value()._close();
        }
        if (c.outPipe) {
          if (close(c.outPipe.value()._fds[1]) == -1) {
            auto errorval = errno;
            COBBLER_ERROR("Could not close write of outPipe: %s",
                          strerror(errorval));
            abort();
          }
        }
      } else {
        _asyncCounter++;
        std::thread t([cPid, handleError, &c, this]() {
          int status;
          waitpid(cPid, &status, 0);
          if (!WIFEXITED(status)) {
            handleError();
          }
          if (c.inPipe) {
            c.inPipe.value()._close();
          }
          if (c.outPipe) {
            if (close(c.outPipe.value()._fds[1]) == -1) {
              auto errorval = errno;
              COBBLER_ERROR("Could not close write of outPipe: %s",
                            strerror(errorval));
              abort();
            }
          }
          _completedAsyncs++;
        });
        t.detach();
      }
    }
  }

  std::vector<_Command> _commands;
  std::atomic_int _asyncCounter;
  std::atomic_int _completedAsyncs;
};
} // namespace cbl
