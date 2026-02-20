#include "test-pch.h"

#include <dlfcn.h>
#include <unistd.h>

#include "LibkcTest.h"

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

LibkcApi LibkcTest::kc;

void LibkcApi::load_api() {
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

    handle = dlopen(so_path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (handle == nullptr) {
        throw std::runtime_error(std::string("dlopen failed: ") + dlerror());
    }

    memcpy = load_symbol<decltype(memcpy)>(handle, "memcpy");
    memmove = load_symbol<decltype(memmove)>(handle, "memmove");
    memset = load_symbol<decltype(memset)>(handle, "memset");
    memcmp = load_symbol<decltype(memcmp)>(handle, "memcmp");
    strlen = load_symbol<decltype(strlen)>(handle, "strlen");
    strspn = load_symbol<decltype(strspn)>(handle, "strspn");
    strchr = load_symbol<decltype(strchr)>(handle, "strchr");
    strnchr = load_symbol<decltype(strnchr)>(handle, "strnchr");
    strcmp = load_symbol<decltype(strcmp)>(handle, "strcmp");
    strncmp = load_symbol<decltype(strncmp)>(handle, "strncmp");
    strcpy = load_symbol<decltype(strcpy)>(handle, "strcpy");
    strncpy = load_symbol<decltype(strncpy)>(handle, "strncpy");
    snprintf_s = load_symbol<decltype(snprintf_s)>(handle, "snprintf_s");
}

void LibkcApi::unload_api() {
    if (handle != nullptr) {
        dlclose(handle);
        handle = nullptr;
    }
}
