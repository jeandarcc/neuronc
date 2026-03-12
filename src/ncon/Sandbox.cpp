#include "neuronc/ncon/Sandbox.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <system_error>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <accctrl.h>
#include <aclapi.h>
#include <sddl.h>
#include <windows.h>
#endif

namespace neuron::ncon {

namespace {

std::filesystem::path defaultCacheRoot() {
#ifdef _WIN32
  const char *localAppData = std::getenv("LOCALAPPDATA");
  if (localAppData != nullptr && *localAppData != '\0') {
    return std::filesystem::path(localAppData) / "NeuronPP" / "ncon" / "work";
  }
#else
  const char *xdgCache = std::getenv("XDG_CACHE_HOME");
  if (xdgCache != nullptr && *xdgCache != '\0') {
    return std::filesystem::path(xdgCache) / "neuronpp" / "ncon" / "work";
  }
  const char *home = std::getenv("HOME");
  if (home != nullptr && *home != '\0') {
    return std::filesystem::path(home) / ".cache" / "neuronpp" / "ncon" / "work";
  }
#endif
  return std::filesystem::temp_directory_path() / "neuronpp-ncon-work";
}

std::filesystem::path defaultNativeCacheRoot() {
#ifdef _WIN32
  const char *localAppData = std::getenv("LOCALAPPDATA");
  if (localAppData != nullptr && *localAppData != '\0') {
    return std::filesystem::path(localAppData) / "NeuronPP" / "ncon" / "native";
  }
#else
  const char *xdgCache = std::getenv("XDG_CACHE_HOME");
  if (xdgCache != nullptr && *xdgCache != '\0') {
    return std::filesystem::path(xdgCache) / "neuronpp" / "ncon" / "native";
  }
  const char *home = std::getenv("HOME");
  if (home != nullptr && *home != '\0') {
    return std::filesystem::path(home) / ".cache" / "neuronpp" / "ncon" / "native";
  }
#endif
  return std::filesystem::temp_directory_path() / "neuronpp-ncon-native";
}

std::string trimCopy(std::string text) {
  auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
  text.erase(text.begin(), std::find_if(text.begin(), text.end(), notSpace));
  text.erase(std::find_if(text.rbegin(), text.rend(), notSpace).base(),
             text.end());
  return text;
}

std::string normalizeSlashes(std::string text) {
  std::replace(text.begin(), text.end(), '\\', '/');
  return text;
}

bool normalizeSandboxPath(const std::string &rawPath, std::string *outMount,
                          std::string *outRelative, std::string *outError) {
  if (outMount == nullptr || outRelative == nullptr) {
    if (outError != nullptr) {
      *outError = "internal error: null sandbox path output";
    }
    return false;
  }

  std::string path = trimCopy(normalizeSlashes(rawPath));
  const std::size_t colonPos = path.find(":/");
  if (colonPos == std::string::npos) {
    if (outError != nullptr) {
      *outError = "sandbox paths must use app:/, res:/, or work:/ prefixes";
    }
    return false;
  }

  *outMount = path.substr(0, colonPos);
  std::transform(outMount->begin(), outMount->end(), outMount->begin(),
                 [](unsigned char ch) {
                   return static_cast<char>(std::tolower(ch));
                 });
  *outRelative = path.substr(colonPos + 2);

  std::string sanitized;
  std::size_t cursor = 0;
  while (cursor < outRelative->size()) {
    while (cursor < outRelative->size() && (*outRelative)[cursor] == '/') {
      ++cursor;
    }
    if (cursor >= outRelative->size()) {
      break;
    }
    const std::size_t nextSlash = outRelative->find('/', cursor);
    const std::string segment =
        outRelative->substr(cursor, nextSlash == std::string::npos
                                        ? std::string::npos
                                        : nextSlash - cursor);
    if (segment.empty() || segment == "." || segment == ".." ||
        segment.find(':') != std::string::npos) {
      if (outError != nullptr) {
        *outError = "sandbox path escapes its mount root: " + rawPath;
      }
      return false;
    }
    if (!sanitized.empty()) {
      sanitized.push_back('/');
    }
    sanitized += segment;
    if (nextSlash == std::string::npos) {
      break;
    }
    cursor = nextSlash + 1;
  }

  *outRelative = sanitized;
  return true;
}

std::string normalizePermissionPrefix(const std::string &rawValue,
                                      std::string *outError) {
  std::string mount;
  std::string relative;
  if (!normalizeSandboxPath(rawValue, &mount, &relative, outError)) {
    return std::string();
  }
  if (relative.empty()) {
    return mount + ":/";
  }
  return mount + ":/" + relative;
}

bool permissionMatches(const std::string &allowEntry,
                       const std::string &logicalPath) {
  if (allowEntry == logicalPath) {
    return true;
  }
  if (allowEntry.size() > logicalPath.size()) {
    return false;
  }
  if (logicalPath.compare(0, allowEntry.size(), allowEntry) != 0) {
    return false;
  }
  if (!allowEntry.empty() && allowEntry.back() == '/') {
    return true;
  }
  return logicalPath.size() == allowEntry.size() ||
         logicalPath[allowEntry.size()] == '/';
}

bool isAllowedByList(const std::vector<std::string> &allowList,
                     const std::string &logicalPath) {
  for (const auto &entry : allowList) {
    if (permissionMatches(entry, logicalPath)) {
      return true;
    }
  }
  return false;
}

#ifdef _WIN32

class WinHandle {
public:
  WinHandle() = default;
  explicit WinHandle(HANDLE handle) : m_handle(handle) {}
  WinHandle(const WinHandle &) = delete;
  WinHandle &operator=(const WinHandle &) = delete;

  WinHandle(WinHandle &&other) noexcept : m_handle(other.release()) {}

  WinHandle &operator=(WinHandle &&other) noexcept {
    if (this != &other) {
      reset(other.release());
    }
    return *this;
  }

  ~WinHandle() { reset(); }

  HANDLE get() const { return m_handle; }

  HANDLE release() {
    HANDLE released = m_handle;
    m_handle = nullptr;
    return released;
  }

  void reset(HANDLE handle = nullptr) {
    if (m_handle != nullptr && m_handle != INVALID_HANDLE_VALUE) {
      CloseHandle(m_handle);
    }
    m_handle = handle;
  }

  bool valid() const {
    return m_handle != nullptr && m_handle != INVALID_HANDLE_VALUE;
  }

private:
  HANDLE m_handle = nullptr;
};

std::wstring utf8ToWide(const std::string &text) {
  if (text.empty()) {
    return std::wstring();
  }

  const int required =
      MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
  if (required <= 0) {
    return std::wstring(text.begin(), text.end());
  }

  std::wstring converted(static_cast<std::size_t>(required - 1), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, converted.data(), required);
  return converted;
}

std::string win32ErrorMessage(DWORD errorCode) {
  LPSTR buffer = nullptr;
  const DWORD size = FormatMessageA(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
          FORMAT_MESSAGE_IGNORE_INSERTS,
      nullptr, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      reinterpret_cast<LPSTR>(&buffer), 0, nullptr);
  if (size == 0 || buffer == nullptr) {
    return "Win32 error " + std::to_string(errorCode);
  }

  std::string message(buffer, buffer + size);
  LocalFree(buffer);
  message = trimCopy(message);
  if (message.empty()) {
    return "Win32 error " + std::to_string(errorCode);
  }
  return message;
}

bool setLowIntegrityLabel(const std::filesystem::path &path,
                          std::string *outError) {
  if (path.empty()) {
    if (outError != nullptr) {
      *outError = "cannot apply low integrity label to an empty path";
    }
    return false;
  }

  PSECURITY_DESCRIPTOR securityDescriptor = nullptr;
  if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
          L"S:(ML;OICI;NW;;;LW)", SDDL_REVISION_1, &securityDescriptor,
          nullptr)) {
    if (outError != nullptr) {
      *outError =
          "failed to build low integrity descriptor: " +
          win32ErrorMessage(GetLastError());
    }
    return false;
  }

  PACL sacl = nullptr;
  BOOL saclPresent = FALSE;
  BOOL saclDefaulted = FALSE;
  const BOOL gotSacl = GetSecurityDescriptorSacl(
      securityDescriptor, &saclPresent, &sacl, &saclDefaulted);
  if (!gotSacl || !saclPresent || sacl == nullptr) {
    if (outError != nullptr) {
      *outError = "failed to extract low integrity SACL";
    }
    LocalFree(securityDescriptor);
    return false;
  }

  std::wstring nativePath = path.wstring();
  const DWORD status = SetNamedSecurityInfoW(
      nativePath.data(), SE_FILE_OBJECT, LABEL_SECURITY_INFORMATION, nullptr,
      nullptr, nullptr, sacl);
  LocalFree(securityDescriptor);
  if (status != ERROR_SUCCESS) {
    if (outError != nullptr) {
      *outError = "failed to label sandbox path '" + path.string() +
                  "' as low integrity: " + win32ErrorMessage(status);
    }
    return false;
  }
  return true;
}

bool applyLowIntegrityToken(HANDLE token, std::string *outError) {
  SID_IDENTIFIER_AUTHORITY mandatoryLabelAuthority =
      SECURITY_MANDATORY_LABEL_AUTHORITY;
  PSID lowIntegritySid = nullptr;
  if (!AllocateAndInitializeSid(&mandatoryLabelAuthority, 1,
                                SECURITY_MANDATORY_LOW_RID, 0, 0, 0, 0, 0, 0,
                                0, &lowIntegritySid)) {
    if (outError != nullptr) {
      *outError = "failed to allocate low integrity SID: " +
                  win32ErrorMessage(GetLastError());
    }
    return false;
  }

  const DWORD labelSize =
      static_cast<DWORD>(sizeof(TOKEN_MANDATORY_LABEL) +
                         GetLengthSid(lowIntegritySid));
  std::vector<unsigned char> buffer(labelSize, 0);
  auto *label =
      reinterpret_cast<TOKEN_MANDATORY_LABEL *>(buffer.data());
  label->Label.Attributes = SE_GROUP_INTEGRITY;
  label->Label.Sid = lowIntegritySid;

  const BOOL ok = SetTokenInformation(token, TokenIntegrityLevel, label,
                                      labelSize);
  FreeSid(lowIntegritySid);
  if (!ok) {
    if (outError != nullptr) {
      *outError = "failed to lower sandbox token integrity: " +
                  win32ErrorMessage(GetLastError());
    }
    return false;
  }
  return true;
}

bool createRestrictedPrimaryToken(WinHandle *outToken,
                                  std::string *outError) {
  if (outToken == nullptr) {
    if (outError != nullptr) {
      *outError = "internal error: null restricted token output";
    }
    return false;
  }

  HANDLE rawProcessToken = nullptr;
  if (!OpenProcessToken(GetCurrentProcess(),
                        TOKEN_DUPLICATE | TOKEN_ASSIGN_PRIMARY | TOKEN_QUERY |
                            TOKEN_ADJUST_DEFAULT | TOKEN_ADJUST_SESSIONID,
                        &rawProcessToken)) {
    if (outError != nullptr) {
      *outError = "failed to open process token: " +
                  win32ErrorMessage(GetLastError());
    }
    return false;
  }
  WinHandle processToken(rawProcessToken);

  BYTE adminSidBuffer[SECURITY_MAX_SID_SIZE];
  DWORD adminSidSize = sizeof(adminSidBuffer);
  BYTE powerUsersSidBuffer[SECURITY_MAX_SID_SIZE];
  DWORD powerUsersSidSize = sizeof(powerUsersSidBuffer);
  std::vector<SID_AND_ATTRIBUTES> denyOnlySids;

  if (CreateWellKnownSid(WinBuiltinAdministratorsSid, nullptr, adminSidBuffer,
                         &adminSidSize)) {
    SID_AND_ATTRIBUTES sid{};
    sid.Sid = adminSidBuffer;
    denyOnlySids.push_back(sid);
  }
  if (CreateWellKnownSid(WinBuiltinPowerUsersSid, nullptr, powerUsersSidBuffer,
                         &powerUsersSidSize)) {
    SID_AND_ATTRIBUTES sid{};
    sid.Sid = powerUsersSidBuffer;
    denyOnlySids.push_back(sid);
  }

  HANDLE rawRestrictedToken = nullptr;
  if (!CreateRestrictedToken(processToken.get(), DISABLE_MAX_PRIVILEGE,
                             static_cast<DWORD>(denyOnlySids.size()),
                             denyOnlySids.empty() ? nullptr
                                                  : denyOnlySids.data(),
                             0, nullptr, 0, nullptr, &rawRestrictedToken)) {
    if (outError != nullptr) {
      *outError = "failed to create restricted token: " +
                  win32ErrorMessage(GetLastError());
    }
    return false;
  }

  outToken->reset(rawRestrictedToken);
  return applyLowIntegrityToken(outToken->get(), outError);
}

bool configureSandboxJob(HANDLE job, bool processSpawnAllowed,
                         std::string *outError) {
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
  limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
  if (!processSpawnAllowed) {
    limits.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_ACTIVE_PROCESS;
    limits.BasicLimitInformation.ActiveProcessLimit = 1;
  }

  if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits,
                               sizeof(limits))) {
    if (outError != nullptr) {
      *outError = "failed to configure sandbox job object: " +
                  win32ErrorMessage(GetLastError());
    }
    return false;
  }

  return true;
}

std::wstring quoteCommandLineArgument(const std::wstring &argument) {
  if (argument.empty()) {
    return L"\"\"";
  }

  if (argument.find_first_of(L" \t\n\v\"") == std::wstring::npos) {
    return argument;
  }

  std::wstring quoted = L"\"";
  std::size_t backslashCount = 0;
  for (wchar_t ch : argument) {
    if (ch == L'\\') {
      ++backslashCount;
      continue;
    }
    if (ch == L'"') {
      quoted.append(backslashCount * 2 + 1, L'\\');
      quoted.push_back(ch);
      backslashCount = 0;
      continue;
    }
    quoted.append(backslashCount, L'\\');
    backslashCount = 0;
    quoted.push_back(ch);
  }
  quoted.append(backslashCount * 2, L'\\');
  quoted.push_back(L'"');
  return quoted;
}

std::wstring buildCommandLine(const std::filesystem::path &runnerPath,
                              const std::vector<std::string> &arguments) {
  std::wstring commandLine =
      quoteCommandLineArgument(runnerPath.wstring());
  for (const auto &argument : arguments) {
    commandLine.push_back(L' ');
    commandLine += quoteCommandLineArgument(utf8ToWide(argument));
  }
  return commandLine;
}

std::string lowercaseCopy(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char ch) {
                   return static_cast<char>(std::tolower(ch));
                 });
  return value;
}

std::vector<std::string>
buildSandboxHelperArguments(const std::filesystem::path &runnerPath,
                            const std::filesystem::path &stagedContainerPath) {
  std::vector<std::string> arguments;
  const std::string stem = lowercaseCopy(runnerPath.stem().string());
  if (stem == "neuron") {
    arguments.push_back("ncon");
  }
  arguments.push_back("__sandbox_run");
  arguments.push_back(stagedContainerPath.string());
  return arguments;
}

#endif

} // namespace

bool initializeSandbox(const std::string &containerHash,
                       const neuron::NconPermissionConfig &permissions,
                       SandboxContext *outContext, std::string *outError) {
  if (outContext == nullptr) {
    if (outError != nullptr) {
      *outError = "internal error: null sandbox output";
    }
    return false;
  }

  *outContext = SandboxContext{};
  outContext->networkAllowed = permissions.network == NconNetworkPolicy::Allow;
  outContext->processSpawnAllowed = permissions.processSpawnAllowed;

  for (const auto &entry : permissions.fsRead) {
    const std::string normalized = normalizePermissionPrefix(entry, outError);
    if (normalized.empty()) {
      return false;
    }
    outContext->fsReadAllowList.push_back(normalized);
  }
  for (const auto &entry : permissions.fsWrite) {
    const std::string normalized = normalizePermissionPrefix(entry, outError);
    if (normalized.empty()) {
      return false;
    }
    outContext->fsWriteAllowList.push_back(normalized);
  }

  std::error_code ec;
  outContext->workDirectory = defaultCacheRoot() / containerHash;
  outContext->resourceDirectory = outContext->workDirectory / "res";
  outContext->nativeCacheDirectory = defaultNativeCacheRoot();
  std::filesystem::create_directories(outContext->workDirectory, ec);
  if (ec) {
    if (outError != nullptr) {
      *outError = "failed to create ncon work directory: " + ec.message();
    }
    return false;
  }
  std::filesystem::create_directories(outContext->resourceDirectory, ec);
  if (ec) {
    if (outError != nullptr) {
      *outError = "failed to create ncon sandbox directories: " + ec.message();
    }
    return false;
  }
  std::filesystem::create_directories(outContext->nativeCacheDirectory, ec);
  if (ec) {
    if (outError != nullptr) {
      *outError = "failed to create ncon native cache directory: " + ec.message();
    }
    return false;
  }
  return true;
}

bool isLogicalPathAllowed(const SandboxContext &context,
                          const std::string &logicalPath,
                          SandboxAccessMode mode, std::string *outError) {
  std::string mount;
  std::string relative;
  if (!normalizeSandboxPath(logicalPath, &mount, &relative, outError)) {
    return false;
  }

  const std::string normalized =
      relative.empty() ? (mount + ":/") : (mount + ":/" + relative);
  const bool allowed = mode == SandboxAccessMode::Read
                           ? isAllowedByList(context.fsReadAllowList, normalized)
                           : isAllowedByList(context.fsWriteAllowList,
                                             normalized);
  if (!allowed) {
    if (outError != nullptr) {
      *outError = "sandbox permission denied: " + normalized;
    }
    return false;
  }
  if (mode == SandboxAccessMode::Write && mount != "work") {
    if (outError != nullptr) {
      *outError = "sandbox write access is only supported for work:/ paths";
    }
    return false;
  }
  return true;
}

bool resolveLogicalPath(const SandboxContext &context,
                        const std::string &logicalPath,
                        SandboxAccessMode mode,
                        std::filesystem::path *outPath,
                        std::string *outError) {
  if (outPath == nullptr) {
    if (outError != nullptr) {
      *outError = "internal error: null logical path output";
    }
    return false;
  }
  if (!isLogicalPathAllowed(context, logicalPath, mode, outError)) {
    return false;
  }

  std::string mount;
  std::string relative;
  if (!normalizeSandboxPath(logicalPath, &mount, &relative, outError)) {
    return false;
  }

  if (mount == "work") {
    *outPath = relative.empty() ? context.workDirectory
                                : (context.workDirectory /
                                   std::filesystem::path(relative));
  } else if (mount == "res") {
    *outPath = relative.empty() ? context.resourceDirectory
                                : (context.resourceDirectory /
                                   std::filesystem::path(relative));
  } else {
    if (outError != nullptr) {
      *outError = "app:/ paths are metadata-only and not file-backed";
    }
    return false;
  }

  if (mode == SandboxAccessMode::Write) {
    std::error_code ec;
    std::filesystem::create_directories(outPath->parent_path(), ec);
    if (ec) {
      if (outError != nullptr) {
        *outError = "failed to create sandbox directories: " + ec.message();
      }
      return false;
    }
  }

  return true;
}

bool resolveResourcePath(const SandboxContext &context,
                         const std::string &resourceId,
                         std::filesystem::path *outPath,
                         std::string *outError) {
  if (outPath == nullptr) {
    if (outError != nullptr) {
      *outError = "internal error: null resource path output";
    }
    return false;
  }

  std::string logicalPath = resourceId;
  if (logicalPath.rfind("res:/", 0) != 0) {
    logicalPath = "res:/" + logicalPath;
  }
  if (!resolveLogicalPath(context, logicalPath, SandboxAccessMode::Read, outPath,
                          outError)) {
    return false;
  }

  std::string mount;
  std::string relative;
  if (!normalizeSandboxPath(logicalPath, &mount, &relative, outError)) {
    return false;
  }

  const auto mounted = context.mountedResources.find(relative);
  if (mounted != context.mountedResources.end()) {
    *outPath = mounted->second;
    return true;
  }

  if (std::filesystem::exists(*outPath)) {
    return true;
  }

  if (outError != nullptr) {
    *outError = "resource not found: " + relative;
  }
  return false;
}

bool stageContainerForSandbox(const std::filesystem::path &sourceContainerPath,
                              const SandboxContext &context,
                              std::filesystem::path *outStagedContainerPath,
                              std::string *outError) {
  if (outStagedContainerPath == nullptr) {
    if (outError != nullptr) {
      *outError = "internal error: null staged container output";
    }
    return false;
  }
  if (sourceContainerPath.empty() ||
      !std::filesystem::exists(sourceContainerPath)) {
    if (outError != nullptr) {
      *outError = "container not found for sandbox staging: " +
                  sourceContainerPath.string();
    }
    return false;
  }

  std::error_code ec;
  std::filesystem::create_directories(context.workDirectory, ec);
  if (ec) {
    if (outError != nullptr) {
      *outError = "failed to create sandbox work directory: " + ec.message();
    }
    return false;
  }
  std::filesystem::create_directories(context.resourceDirectory, ec);
  if (ec) {
    if (outError != nullptr) {
      *outError = "failed to create sandbox resource directory: " +
                  ec.message();
    }
    return false;
  }

#ifdef _WIN32
  if (!setLowIntegrityLabel(context.workDirectory, outError) ||
      !setLowIntegrityLabel(context.resourceDirectory, outError) ||
      !setLowIntegrityLabel(context.nativeCacheDirectory, outError)) {
    return false;
  }
#endif

  const std::filesystem::path stagedContainerPath =
      context.workDirectory / "container.ncon";
  std::filesystem::copy_file(sourceContainerPath, stagedContainerPath,
                             std::filesystem::copy_options::overwrite_existing,
                             ec);
  if (ec) {
    if (outError != nullptr) {
      *outError = "failed to stage container into sandbox: " + ec.message();
    }
    return false;
  }

#ifdef _WIN32
  if (!setLowIntegrityLabel(stagedContainerPath, outError)) {
    return false;
  }
#endif

  *outStagedContainerPath = stagedContainerPath;
  return true;
}

bool launchSandboxedProcess(const std::filesystem::path &runnerPath,
                            const std::vector<std::string> &arguments,
                            const SandboxContext &context, int *outExitCode,
                            std::string *outError) {
#ifdef _WIN32
  if (outExitCode != nullptr) {
    *outExitCode = 1;
  }

  if (runnerPath.empty() || !std::filesystem::exists(runnerPath)) {
    if (outError != nullptr) {
      *outError = "sandbox runner executable not found: " + runnerPath.string();
    }
    return false;
  }

  if (!setLowIntegrityLabel(context.workDirectory, outError) ||
      !setLowIntegrityLabel(context.resourceDirectory, outError) ||
      !setLowIntegrityLabel(context.nativeCacheDirectory, outError)) {
    return false;
  }

  WinHandle restrictedToken;
  if (!createRestrictedPrimaryToken(&restrictedToken, outError)) {
    return false;
  }

  WinHandle job(CreateJobObjectW(nullptr, nullptr));
  if (!job.valid()) {
    if (outError != nullptr) {
      *outError = "failed to create sandbox job object: " +
                  win32ErrorMessage(GetLastError());
    }
    return false;
  }
  if (!configureSandboxJob(job.get(), context.processSpawnAllowed, outError)) {
    return false;
  }

  STARTUPINFOW startupInfo{};
  startupInfo.cb = sizeof(startupInfo);
  BOOL inheritHandles = FALSE;
  const HANDLE stdIn = GetStdHandle(STD_INPUT_HANDLE);
  const HANDLE stdOut = GetStdHandle(STD_OUTPUT_HANDLE);
  const HANDLE stdErr = GetStdHandle(STD_ERROR_HANDLE);
  if (stdIn != nullptr && stdIn != INVALID_HANDLE_VALUE && stdOut != nullptr &&
      stdOut != INVALID_HANDLE_VALUE && stdErr != nullptr &&
      stdErr != INVALID_HANDLE_VALUE) {
    startupInfo.dwFlags |= STARTF_USESTDHANDLES;
    startupInfo.hStdInput = stdIn;
    startupInfo.hStdOutput = stdOut;
    startupInfo.hStdError = stdErr;
    inheritHandles = TRUE;
  }

  std::wstring commandLine = buildCommandLine(runnerPath, arguments);
  std::vector<wchar_t> mutableCommandLine(commandLine.begin(),
                                          commandLine.end());
  mutableCommandLine.push_back(L'\0');
  std::wstring currentDirectory = context.workDirectory.wstring();

  PROCESS_INFORMATION processInfo{};
  if (!CreateProcessAsUserW(
          restrictedToken.get(), runnerPath.wstring().c_str(),
          mutableCommandLine.data(), nullptr, nullptr, inheritHandles,
          CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT, nullptr,
          currentDirectory.empty() ? nullptr : currentDirectory.c_str(),
          &startupInfo, &processInfo)) {
    if (outError != nullptr) {
      *outError = "failed to create sandboxed process: " +
                  win32ErrorMessage(GetLastError());
    }
    return false;
  }

  WinHandle process(processInfo.hProcess);
  WinHandle thread(processInfo.hThread);
  if (!AssignProcessToJobObject(job.get(), process.get())) {
    TerminateProcess(process.get(), 1);
    if (outError != nullptr) {
      *outError = "failed to assign sandboxed process to job object: " +
                  win32ErrorMessage(GetLastError());
    }
    return false;
  }

  if (ResumeThread(thread.get()) == static_cast<DWORD>(-1)) {
    TerminateProcess(process.get(), 1);
    if (outError != nullptr) {
      *outError = "failed to start sandboxed process: " +
                  win32ErrorMessage(GetLastError());
    }
    return false;
  }

  WaitForSingleObject(process.get(), INFINITE);
  DWORD exitCode = 1;
  if (!GetExitCodeProcess(process.get(), &exitCode)) {
    if (outError != nullptr) {
      *outError = "failed to query sandboxed process exit code: " +
                  win32ErrorMessage(GetLastError());
    }
    return false;
  }

  if (outExitCode != nullptr) {
    *outExitCode = static_cast<int>(exitCode);
  }
  return true;
#else
  (void)runnerPath;
  (void)arguments;
  (void)context;
  if (outError != nullptr) {
    *outError = "sandboxed child process launch is only implemented on Windows";
  }
  return false;
#endif
}

bool executeContainerInWindowsSandbox(
    const std::filesystem::path &runnerPath,
    const std::filesystem::path &sourceContainerPath,
    const std::string &containerHash,
    const neuron::NconPermissionConfig &permissions, int *outExitCode,
    std::string *outError) {
#ifdef _WIN32
  if (runnerPath.empty()) {
    if (outError != nullptr) {
      *outError = "failed to resolve current executable path for sandbox launch";
    }
    return false;
  }

  SandboxContext sandbox;
  if (!initializeSandbox(containerHash, permissions, &sandbox, outError)) {
    return false;
  }

  std::filesystem::path stagedContainerPath;
  if (!stageContainerForSandbox(sourceContainerPath, sandbox,
                                &stagedContainerPath, outError)) {
    return false;
  }

  return launchSandboxedProcess(
      runnerPath,
      buildSandboxHelperArguments(runnerPath, stagedContainerPath), sandbox,
      outExitCode, outError);
#else
  (void)runnerPath;
  (void)sourceContainerPath;
  (void)containerHash;
  (void)permissions;
  (void)outExitCode;
  if (outError != nullptr) {
    *outError = "Windows sandbox execution is only available on Windows";
  }
  return false;
#endif
}

} // namespace neuron::ncon
