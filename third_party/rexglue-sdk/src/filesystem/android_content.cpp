#include <rex/platform.h>
#if REX_PLATFORM_ANDROID

#include <rex/filesystem.h>

namespace rex::filesystem {

void AndroidInitialize() {}

void AndroidShutdown() {}

bool IsAndroidContentUri(const std::string_view source) {
  return source.starts_with("content://");
}

int OpenAndroidContentFileDescriptor(const std::string_view /*uri*/, const char* /*mode*/) {
  // Game data is a real filesystem path, not a content URI.
  return -1;
}

}  // namespace rex::filesystem

#endif
