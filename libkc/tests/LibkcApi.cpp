#include <dlfcn.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

#include <string>
#include <stdexcept>

#include "LibkcApi.h"

namespace {
    template <typename T>
    T load_symbol(void *handle, const char *name) {
        dlerror();
        void *symbol = dlsym(handle, name);
        const char *error = dlerror();
        if (error != nullptr) {
            throw std::runtime_error(std::string("dlsym failed for ") + name + ": " + error);
        }
        return reinterpret_cast<T>(symbol);
    }
}

LibkcApi load_api() {
    char exe_path[PATH_MAX];
    ssize_t size = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (size < 0) {
        throw std::runtime_error("readlink(/proc/self/exe) failed");
    }
    exe_path[size] = '\0';

    std::string so_path(exe_path);
    size_t slash = so_path.find_last_of('/');
    if (slash == std::string::npos) {
        throw std::runtime_error("failed to locate test executable directory");
    }
    so_path = so_path.substr(0, slash + 1) + "libkc.so";

    void *handle = dlopen(so_path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (handle == nullptr) {
        throw std::runtime_error(std::string("dlopen failed: ") + dlerror());
    }

    LibkcApi api{};
    api.handle = handle;
    api.memcpy_ptr = load_symbol<memcpy_fn>(handle, "memcpy");
    api.memmove_ptr = load_symbol<memmove_fn>(handle, "memmove");
    api.memset_ptr = load_symbol<memset_fn>(handle, "memset");
    api.memcmp_ptr = load_symbol<memcmp_fn>(handle, "memcmp");
    api.strlen_ptr = load_symbol<strlen_fn>(handle, "strlen");
    api.strspn_ptr = load_symbol<strspn_fn>(handle, "strspn");
    api.strchr_ptr = load_symbol<strchr_fn>(handle, "strchr");
    api.strnchr_ptr = load_symbol<strnchr_fn>(handle, "strnchr");
    api.strcmp_ptr = load_symbol<strcmp_fn>(handle, "strcmp");
    api.strncmp_ptr = load_symbol<strncmp_fn>(handle, "strncmp");
    api.strcpy_ptr = load_symbol<strcpy_fn>(handle, "strcpy");
    api.strncpy_ptr = load_symbol<strncpy_fn>(handle, "strncpy");
    api.snprintf_s_ptr = load_symbol<snprintf_s_fn>(handle, "snprintf_s");
    return api;
}

void unload_api(LibkcApi &api) {
    if (api.handle != nullptr) {
        dlclose(api.handle);
        api.handle = nullptr;
    }
}
