#ifndef COBBLER_H
#define COBBLER_H

#include <atomic>
#include <cassert>
#include <concepts>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <future>
#include <spawn.h>
#include <sstream>
#include <string.h>
#include <string>
#include <sys/file.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

// TODO: define global os-switch semantics

#define COBBLER_PUSH_INDENT()                                                  \
  { cbl::indentLevel++; }
#define COBBLER_POP_INDENT()                                                   \
  { cbl::indentLevel--; }

#if !defined(WIN32)
#define COBBLER_LOG(msg, ...)                                                  \
  {                                                                            \
    std::scoped_lock printLock(cbl::backend::procMux);                         \
    for (int i = 0; i < cbl::indentLevel; i++) {                               \
      printf("==");                                                            \
    }                                                                          \
    if (cbl::indentLevel > 0) {                                                \
      printf(" ");                                                             \
    }                                                                          \
    printf("\033[0;34m[INFO] ====> " msg "\033[0m\n", ##__VA_ARGS__);          \
  }
#define COBBLER_WARN(msg, ...)                                                 \
  {                                                                            \
    std::scoped_lock printLock(cbl::backend::procMux);                         \
    for (int i = 0; i < cbl::indentLevel; i++) {                               \
      printf("==");                                                            \
    }                                                                          \
    if (cbl::indentLevel > 0) {                                                \
      printf(" ");                                                             \
    }                                                                          \
    printf("\033[0;33m[WARNING] => " msg "\033[0m\n", ##__VA_ARGS__);          \
  }
#define COBBLER_ERROR(msg, ...)                                                \
  {                                                                            \
    std::scoped_lock printLock(cbl::backend::procMux);                         \
    for (int i = 0; i < cbl::indentLevel; i++) {                               \
      fprintf(stderr, "==");                                                   \
    }                                                                          \
    if (cbl::indentLevel > 0) {                                                \
      fprintf(stderr, " ");                                                    \
    }                                                                          \
    fprintf(stderr, "\033[0;31m[ERROR] ===> " msg "\033[0m\n", ##__VA_ARGS__); \
  }
#else
// TODO: use typical os-switch
#endif

// VERSION : 0.0.5;
namespace cbl {
inline std::atomic_int indentLevel = 0;
namespace backend {
#ifndef COBBLER_NO_DEFAULT_BACKEND
class ProcMux {
public:
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
// TODO: implement
#elif __ANDROID__
// TODO: investigate if any major differences to __unix__, if not merge
#elif __unix__
  using native_handle_type = int;

  inline ProcMux() noexcept {
    char a[] = "/tmp/COBBLER_XXXXXX_LOCK";
    _handle = mkstemp(a);
  }
  inline ProcMux(const ProcMux &) = delete;
  ~ProcMux() noexcept { close(_handle); }

  inline void lock() { flock(_handle, LOCK_EX); }
  inline void unlock() { flock(_handle, LOCK_UN); }
  inline bool try_lock() { return flock(_handle, LOCK_EX) == 0; }
#else
#error "Unknown target system, cannot build default backend"
#endif
  inline native_handle_type native_handle() { return _handle; }

private:
  native_handle_type _handle;
};
inline ProcMux procMux;

template <std::convertible_to<std::string>... S>
inline std::string concatenateVariadic(S const &...strings) {
  std::stringstream stream;
  using List = int[];
  (void)List{0, ((void)(stream << strings << " "), 0)...};
  const auto &s = stream.str();
  return s.substr(0, s.length() - 1);
}
template <std::convertible_to<std::string>... S>
inline std::vector<std::string> splatVariadicToArgVector(S const &...strings) {
  using List = std::vector<std::string>;
  List l{std::string(strings)...};
  return l;
}
template <std::convertible_to<std::string> S>
inline std::vector<const char *> toLocalArglist(const std::vector<S> &cmd) {
  std::vector<const char *> result = {};
  for (int i = 0; i < cmd.size(); i++) {
    result.push_back(cmd[i].c_str());
  }
  result.push_back(nullptr);
  return result;
}

#ifdef __unix__
// TODO: Find way to run programs without having to supply a fully qualified
// name each time
inline std::tuple<int, pid_t> spawnOnUnix(const std::vector<std::string> &cmd) {
  pid_t pid;
  auto args = toLocalArglist(cmd);
  int status;

  return {posix_spawn(&pid, cmd.front().c_str(), nullptr, nullptr,
                      const_cast<char *const *>(args.data()), environ),
          pid};
}

inline int forkAndRun(const std::vector<std::string> &cmd) {
  pid_t cPid = fork();
  if (cPid < 0) { /* ERROR */
    COBBLER_ERROR("Could not create child!");
    exit(EXIT_FAILURE);
  } else if (cPid == 0) { /* CHILD */
    std::filesystem::path p = cmd.front();
    auto args = toLocalArglist(cmd);

    execvpe(p.string().c_str(), const_cast<char *const *>(args.data()),
            environ);
    auto errorval = errno;
    COBBLER_ERROR("Excec encountered an error: %s", strerror(errorval));
    exit(EXIT_FAILURE);
  }
  return cPid;
}
#endif // __unix__

inline void call(const std::vector<std::string> &cmd) {
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
// TODO: implement
#elif __ANDROID__
// TODO: investigate if any major differences to __unix__, if not merge
#elif __unix__
#if 0
  auto [status, pid] = spawnOnUnix(cmd);
  if (status == 0) {
    do {
      if (waitpid(pid, &status, 0) != -1) {
      } else {
        COBBLER_ERROR("Encountered error while waiting for process: %s",
                      cmd.front().c_str());
        exit(EXIT_FAILURE);
      }
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));
  } else {
    COBBLER_ERROR("Failed to start process: %s because %s", cmd.front().c_str(),
                  strerror(status));
  }
#endif

  pid_t cPid = forkAndRun(cmd);
  int status;
  waitpid(cPid, &status, 0);
  if (!(WIFEXITED(status) && WEXITSTATUS(status) == 0)) {
    auto errorval = errno;
    COBBLER_ERROR("Command %s encountered an error", cmd.front().c_str());
  }
}

#else
#error "Unknown target system, cannot build default backend"
#endif

  inline std::future<void> callAsync(const std::vector<std::string> &cmd) {
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
// TODO: implement
#elif __ANDROID__
// TODO: investigate if any major differences to __unix__, if not merge
#elif __unix__
  pid_t cPid = forkAndRun(cmd);
  /* PARENT */
  std::promise<void> wait_promise;
  std::future<void> wait_future = wait_promise.get_future();
  std::thread t(
      [cPid, cmd](std::promise<void> wp) {
        int status;
        waitpid(cPid, &status, 0);
        if (!(WIFEXITED(status) && WEXITSTATUS(status) == 0)) {
          auto errorval = errno;
          COBBLER_ERROR("Command %s encountered an error", cmd.front().c_str());
        }
        wp.set_value();
      },
      std::move(wait_promise));
  t.detach();
  return wait_future;
#else
#error "Unknown target system, cannot build default backend"
#endif
  }
#endif
} // namespace backend
} // namespace cbl

namespace cbl {
enum class io : uint8_t { sync = 0, async };
struct Cobbler {
  inline void operator()() {
    std::vector<std::future<void>> unfinished = {};
    for (_Command &c : _commands) {
      COBBLER_LOG("Executing %s command: %s",
                  c.calltype == io::async ? "asynchronous" : "synchronous",
                  c.call.front().c_str());
      if (c.calltype == io::sync) {
        backend::call(c.call);
      } else {
        unfinished.push_back(backend::callAsync(c.call));
      }
    }
    for (auto &f : unfinished) {
      f.wait();
    }
    COBBLER_LOG("Waiting for all commands to finish");
  }

  inline void clear() { _commands.clear(); }

  template <io TYPE = io::sync, typename... S>
  inline Cobbler &cmd(S const &...command) {
    _commands.push_back(
        {.calltype = TYPE,
         .call = backend::splatVariadicToArgVector(command...)});
    return (*this);
  }

  template <io TYPE = io::sync>
  inline Cobbler &cmd(const std::vector<std::string> &command) {
    _commands.push_back({.calltype = TYPE, .call = command});
    return (*this);
  }

private:
  struct _Command {
    io calltype;
    std::vector<std::string> call;
  };

  std::vector<_Command> _commands;
  std::atomic_int _asyncCounter;
  std::atomic_int _completedAsyncs;
};
} // namespace cbl
#endif // !COBBLER_H
