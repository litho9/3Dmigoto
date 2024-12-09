#include "Globals.h"

wstring get_path(const HINSTANCE handle = nullptr) {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(handle, path, MAX_PATH);
    wcsrchr(path, L'\\')[1] = 0;
    return wstring(path);
}
wstring get_file(const HINSTANCE handle = nullptr, const wstring &name = L"") {
    return get_path(handle) + name;
}