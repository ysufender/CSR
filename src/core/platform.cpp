#include "platform.hpp"

dlID_t DLLoad(std::string_view path)
{
#if defined(_WIN32) || defined(__CYGWIN__)
    return LoadLibrary(path.data());
#elif defined(unix) || defined(__unix) || defined(__unix__) || defined(__APPLE__) || defined(__MACH__)
    return dlopen(path.data(), RTLD_NOW);
#endif
}

bool DLUnload(dlID_t dlID)
{
#if defined(_WIN32) || defined(__CYGWIN__)
    return FreeLibrary(path.data());
#elif defined(unix) || defined(__unix) || defined(__unix__) || defined(__APPLE__) || defined(__MACH__)
    return dlclose(dlID);
#endif
}

std::filesystem::path GetExePath()
{
#if defined(_WIN32) || defined(__CYGWIN__)
    wchar_t path[MAX_PATH];
    GetModuleFileNameW( NULL, path, MAX_PATH );
#elif defined(unix) || defined(__unix) || defined(__unix__)
    // Linux specific
    char path[PATH_MAX];
    ssize_t count = readlink( "/proc/self/exe", path, PATH_MAX );
    if( count < 0 || count >= PATH_MAX )
        return {};
    path[count] = '\0';
#elif defined(__APPLE__) || defined(__MACH__)
    char path[PATH_MAX];
    uint32_t bufsize = PATH_MAX;
    if (!_NSGetExecutablePath(path, &bufsize))
        return std::filesystem::path{path}.parent_path() / ""; // to finish the folder path with (back)slash
    return {};  // some error
#endif

    return std::filesystem::path { path };
}
