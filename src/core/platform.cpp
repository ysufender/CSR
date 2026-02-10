#include "platform.hpp"

dlID_t DLLoad(std::string_view path)
{
#ifdef CSR_WIN
    return LoadLibrary(path.data());
#elif defined(CSR_UNIX) || defined(CSR_APPLE)
    return dlopen(path.data(), RTLD_NOW);
#endif
}

bool DLUnload(dlID_t dlID)
{
#ifdef CSR_WIN
    return FreeLibrary(dlID);
#elif defined(CSR_UNIX) || defined(CSR_APPLE)
    return dlclose(dlID);
#endif
}

std::filesystem::path GetExePath()
{
#ifdef CSR_WIN
    wchar_t path[MAX_PATH];
    GetModuleFileNameW( NULL, path, MAX_PATH );
#elif defined(CSR_UNIX)
    char path[PATH_MAX];
    ssize_t count = readlink( "/proc/self/exe", path, PATH_MAX );
    if( count < 0 || count >= PATH_MAX )
        return {};
    path[count] = '\0';
#elif defined(CSR_APPLE)
    char path[PATH_MAX];
    uint32_t bufsize = PATH_MAX;
    if (!_NSGetExecutablePath(path, &bufsize))
        return std::filesystem::path{path}.parent_path() / ""; // to finish the folder path with (back)slash
    return {};  // some error
#endif

    return std::filesystem::path { path };
}
