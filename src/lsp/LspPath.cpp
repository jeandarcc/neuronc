#include "LspPath.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>

namespace neuron::lsp::detail {

namespace {

std::string percentDecode(std::string_view text) {
  std::string decoded;
  decoded.reserve(text.size());
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (text[i] == '%' && i + 2 < text.size()) {
      const auto hexValue = [](char ch) -> int {
        if (ch >= '0' && ch <= '9') {
          return ch - '0';
        }
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        if (ch >= 'a' && ch <= 'f') {
          return 10 + (ch - 'a');
        }
        return -1;
      };
      const int hi = hexValue(text[i + 1]);
      const int lo = hexValue(text[i + 2]);
      if (hi >= 0 && lo >= 0) {
        decoded.push_back(static_cast<char>((hi << 4) | lo));
        i += 2;
        continue;
      }
    }
    decoded.push_back(text[i] == '+' ? ' ' : text[i]);
  }
  return decoded;
}

std::string percentEncodePath(std::string_view text) {
  std::string encoded;
  encoded.reserve(text.size());
  constexpr char hex[] = "0123456789ABCDEF";
  for (unsigned char ch : text) {
    const bool isSafe =
        std::isalnum(ch) || ch == '/' || ch == '-' || ch == '_' || ch == '.' ||
        ch == '~' || ch == ':';
    if (isSafe) {
      encoded.push_back(static_cast<char>(ch));
      continue;
    }
    encoded.push_back('%');
    encoded.push_back(hex[(ch >> 4) & 0x0F]);
    encoded.push_back(hex[ch & 0x0F]);
  }
  return encoded;
}

} // namespace

std::string trimCopy(std::string text) {
  const auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
  text.erase(text.begin(), std::find_if(text.begin(), text.end(), notSpace));
  text.erase(std::find_if(text.rbegin(), text.rend(), notSpace).base(),
             text.end());
  return text;
}

std::string toLowerCopy(std::string_view text) {
  std::string lowered(text);
  std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                 [](unsigned char ch) {
                   return static_cast<char>(std::tolower(ch));
                 });
  return lowered;
}

bool startsWith(std::string_view value, std::string_view prefix) {
  return prefix.empty() ||
         (value.size() >= prefix.size() &&
          value.substr(0, prefix.size()) == prefix);
}

bool containsCaseInsensitive(std::string_view text, std::string_view needle) {
  if (needle.empty()) {
    return true;
  }
  return toLowerCopy(text).find(toLowerCopy(needle)) != std::string::npos;
}

std::optional<std::string> readFileText(const fs::path &path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    return std::nullopt;
  }
  return std::string(std::istreambuf_iterator<char>(input),
                     std::istreambuf_iterator<char>());
}

std::string normalizeModuleName(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return value;
}

fs::path normalizePath(const fs::path &path) {
  fs::path normalized = path;
  std::error_code ec;
  if (!normalized.is_absolute()) {
    normalized = fs::absolute(normalized, ec);
  }
  return normalized.lexically_normal();
}

std::string pathKey(const fs::path &path) {
  std::string key = normalizePath(path).generic_string();
#if defined(_WIN32)
  std::transform(key.begin(), key.end(), key.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
#endif
  return key;
}

bool samePath(const fs::path &lhs, const fs::path &rhs) {
  if (lhs.empty() || rhs.empty()) {
    return lhs == rhs;
  }
  return pathKey(lhs) == pathKey(rhs);
}

bool sameLocation(const SourceLocation &lhs, const SourceLocation &rhs) {
  return lhs.line == rhs.line && lhs.column == rhs.column &&
         samePath(lhs.file, rhs.file);
}

bool sameSymbolLocation(const SymbolLocation &lhs, const SymbolLocation &rhs) {
  return sameLocation(lhs.location, rhs.location);
}

bool isValidIdentifier(std::string_view name) {
  if (name.empty()) {
    return false;
  }

  const unsigned char first = static_cast<unsigned char>(name.front());
  if (!(std::isalpha(first) || name.front() == '_')) {
    return false;
  }

  for (char ch : name) {
    const unsigned char value = static_cast<unsigned char>(ch);
    if (std::isalnum(value) || ch == '_') {
      continue;
    }
    return false;
  }
  return true;
}

fs::path uriToPath(const std::string &uri) {
  constexpr std::string_view prefix = "file://";
  if (uri.rfind(prefix, 0) != 0) {
    return fs::path(uri);
  }

  std::string decoded =
      percentDecode(std::string_view(uri).substr(prefix.size()));
  if (!decoded.empty() && decoded.front() == '/' && decoded.size() > 2 &&
      std::isalpha(static_cast<unsigned char>(decoded[1])) &&
      decoded[2] == ':') {
    decoded.erase(decoded.begin());
  }
#if defined(_WIN32)
  std::replace(decoded.begin(), decoded.end(), '/', '\\');
#else
  std::replace(decoded.begin(), decoded.end(), '\\', fs::path::preferred_separator);
#endif
  return fs::path(decoded).lexically_normal();
}

std::string pathToUri(const fs::path &path) {
  fs::path absolute = path;
  std::error_code ec;
  if (!absolute.is_absolute()) {
    absolute = fs::absolute(absolute, ec);
  }
  std::string generic = absolute.generic_string();
#if defined(_WIN32)
  if (generic.size() > 1 && generic[1] == ':') {
    generic.insert(generic.begin(), '/');
  }
#endif
  return "file://" + percentEncodePath(generic);
}

std::string fileStem(const fs::path &path) { return path.stem().string(); }

} // namespace neuron::lsp::detail
