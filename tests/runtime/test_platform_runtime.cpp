#include "neuron_platform.h"
#include "neuron_runtime.h"

#include <cstdio>
#include <cstring>
#include <string>
#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

namespace {

class ScopedStdinRedirect {
public:
  ~ScopedStdinRedirect() {
    restore();
    if (temp_ != nullptr) {
      std::fclose(temp_);
    }
  }

  bool begin(const char *text, size_t len) {
    if (text == nullptr) {
      return false;
    }

    temp_ = std::tmpfile();
    if (temp_ == nullptr) {
      return false;
    }
    if (std::fwrite(text, 1, len, temp_) != len) {
      return false;
    }
    if (std::fputc('\n', temp_) == EOF) {
      return false;
    }
    std::fflush(temp_);
    std::rewind(temp_);

#if defined(_WIN32)
    const int stdin_fd = _fileno(stdin);
    const int temp_fd = _fileno(temp_);
    saved_fd_ = _dup(stdin_fd);
    if (saved_fd_ < 0) {
      return false;
    }
    if (_dup2(temp_fd, stdin_fd) < 0) {
      _close(saved_fd_);
      saved_fd_ = -1;
      return false;
    }
#else
    const int stdin_fd = fileno(stdin);
    const int temp_fd = fileno(temp_);
    saved_fd_ = dup(stdin_fd);
    if (saved_fd_ < 0) {
      return false;
    }
    if (dup2(temp_fd, stdin_fd) < 0) {
      close(saved_fd_);
      saved_fd_ = -1;
      return false;
    }
#endif

    active_ = true;
    return true;
  }

private:
  void restore() {
    if (!active_) {
      return;
    }

#if defined(_WIN32)
    const int stdin_fd = _fileno(stdin);
    (void)_dup2(saved_fd_, stdin_fd);
    _close(saved_fd_);
#else
    const int stdin_fd = fileno(stdin);
    (void)dup2(saved_fd_, stdin_fd);
    close(saved_fd_);
#endif

    saved_fd_ = -1;
    active_ = false;
  }

  FILE *temp_ = nullptr;
  int saved_fd_ = -1;
  bool active_ = false;
};

class ScopedStdoutCapture {
public:
  ~ScopedStdoutCapture() {
    restore();
    if (temp_ != nullptr) {
      std::fclose(temp_);
    }
  }

  bool begin() {
    temp_ = std::tmpfile();
    if (temp_ == nullptr) {
      return false;
    }

#if defined(_WIN32)
    const int stdout_fd = _fileno(stdout);
    const int temp_fd = _fileno(temp_);
    saved_fd_ = _dup(stdout_fd);
    if (saved_fd_ < 0) {
      return false;
    }
    if (_dup2(temp_fd, stdout_fd) < 0) {
      _close(saved_fd_);
      saved_fd_ = -1;
      return false;
    }
#else
    const int stdout_fd = fileno(stdout);
    const int temp_fd = fileno(temp_);
    saved_fd_ = dup(stdout_fd);
    if (saved_fd_ < 0) {
      return false;
    }
    if (dup2(temp_fd, stdout_fd) < 0) {
      close(saved_fd_);
      saved_fd_ = -1;
      return false;
    }
#endif

    active_ = true;
    return true;
  }

  std::string readAll() {
    if (!active_ || temp_ == nullptr) {
      return {};
    }

    std::fflush(stdout);
    if (std::fseek(temp_, 0, SEEK_SET) != 0) {
      return {};
    }

    std::string text;
    char buffer[256] = {0};
    while (std::fgets(buffer, sizeof(buffer), temp_) != nullptr) {
      text += buffer;
    }
    return text;
  }

private:
  void restore() {
    if (!active_) {
      return;
    }

    std::fflush(stdout);
#if defined(_WIN32)
    const int stdout_fd = _fileno(stdout);
    (void)_dup2(saved_fd_, stdout_fd);
    _close(saved_fd_);
#else
    const int stdout_fd = fileno(stdout);
    (void)dup2(saved_fd_, stdout_fd);
    close(saved_fd_);
#endif

    saved_fd_ = -1;
    active_ = false;
  }

  FILE *temp_ = nullptr;
  int saved_fd_ = -1;
  bool active_ = false;
};

} // namespace

TEST(PlatformEnvAndPathSmoke) {
  ASSERT_TRUE(neuron_platform_path_separator() == '/' ||
              neuron_platform_path_separator() == '\\');

  char cwd[4096] = {0};
  ASSERT_TRUE(neuron_platform_current_working_directory(cwd, sizeof(cwd)) == 1);
  ASSERT_TRUE(cwd[0] != '\0');

  char exe[4096] = {0};
  ASSERT_TRUE(neuron_platform_current_executable_path(exe, sizeof(exe)) == 1);
  ASSERT_TRUE(exe[0] != '\0');

  ASSERT_TRUE(neuron_platform_name() != nullptr);
  ASSERT_TRUE(neuron_platform_arch_name() != nullptr);
  return true;
}

TEST(PlatformTimeSmoke) {
  const int64_t before = neuron_platform_now_ms();
  neuron_platform_sleep_ms(5);
  const int64_t after = neuron_platform_now_ms();
  ASSERT_TRUE(after >= before);
  return true;
}

TEST(PlatformProcessSmoke) {
  NeuronPlatformProcessHandle process = {0};
#if defined(_WIN32)
  ASSERT_TRUE(neuron_platform_spawn_process("cmd /c exit 0", &process) == 1);
#else
  ASSERT_TRUE(neuron_platform_spawn_process("true", &process) == 1);
#endif
  int32_t exitCode = -1;
  ASSERT_TRUE(neuron_platform_wait_process(process, &exitCode) == 1);
  ASSERT_EQ(exitCode, 0);
  return true;
}

TEST(PlatformLibraryFailureReportsError) {
  NeuronPlatformLibraryHandle handle =
      neuron_platform_open_library("__definitely_missing_library__");
  ASSERT_TRUE(handle == nullptr);
  const char *error = neuron_platform_last_error();
  ASSERT_TRUE(error != nullptr);
  ASSERT_TRUE(error[0] != '\0');
  return true;
}

TEST(PlatformLibrarySmoke) {
#if defined(_WIN32)
  NeuronPlatformLibraryHandle handle =
      neuron_platform_open_library("kernel32.dll");
  ASSERT_TRUE(handle != nullptr);
  void *symbol = neuron_platform_load_symbol(handle, "GetTickCount");
  ASSERT_TRUE(symbol != nullptr);
  neuron_platform_close_library(handle);
#else
  ASSERT_TRUE(true);
#endif
  return true;
}

TEST(RuntimeInputStringPreservesUtf8Bytes) {
  const char expected[] = {
      'T', (char)0xC3, (char)0xBC, 'r', 'k', (char)0xC3, (char)0xA7, 'e', ' ',
      (char)0xC5, (char)0x9F, (char)0xC4, (char)0x9F, (char)0xC3, (char)0xBC,
      (char)0xC3, (char)0xB6, (char)0xC3, (char)0xA7, (char)0xC4, (char)0xB1,
      ' ',        (char)0xC4, (char)0xB0, (char)0xC4, (char)0x9E, (char)0xC3,
      (char)0x9C, (char)0xC3, (char)0x96, (char)0xC3, (char)0x87, '\0'};
  const size_t len = std::strlen(expected);

  ScopedStdinRedirect redirected_stdin;
  ASSERT_TRUE(redirected_stdin.begin(expected, len));

  const char *actual = neuron_io_input_string(nullptr, 0, 0, nullptr, -1);
  ASSERT_TRUE(actual != nullptr);
  ASSERT_TRUE(std::strcmp(actual, expected) == 0);
  return true;
}

TEST(RuntimeInputIntParsesBufferedDigitsWithSinglePrompt) {
  ScopedStdinRedirect redirected_stdin;
  ASSERT_TRUE(redirected_stdin.begin("41251", std::strlen("41251")));

  ScopedStdoutCapture captured_stdout;
  ASSERT_TRUE(captured_stdout.begin());

  const int64_t actual =
      neuron_io_input_int("Hello: ", 0, 0, 0, 0, 0, 0, -1);
  const std::string output = captured_stdout.readAll();

  ASSERT_EQ(actual, 41251);
  ASSERT_EQ(output, "Hello: ");
  return true;
}

TEST(RuntimeInputEnumParsesNameFromStdin) {
  const char *options = "Red\nGreen\nBlue";
  ScopedStdinRedirect redirected_stdin;
  ASSERT_TRUE(redirected_stdin.begin("Green", std::strlen("Green")));

  const int64_t actual =
      neuron_io_input_enum(nullptr, options, 3, 0, 0, -1);
  ASSERT_EQ(actual, 1);
  return true;
}
