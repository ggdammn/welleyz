#include <windows.h>
#include <shellapi.h>
#include <initializer_list>
#include <string>

namespace {
bool exists(const std::wstring& path) {
    const auto attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

std::wstring installedUninstaller() {
    // When launched from the installed folder, use the Inno Setup uninstaller
    // beside this executable. The registry handles custom installation paths
    // when this launcher is started from the release ZIP instead.
    wchar_t module[MAX_PATH]{};
    if (GetModuleFileNameW(nullptr, module, MAX_PATH) > 0) {
        std::wstring path(module);
        const auto slash = path.find_last_of(L"\\/");
        if (slash != std::wstring::npos) {
            path.resize(slash + 1);
            path += L"unins000.exe";
            if (exists(path))
                return path;
        }
    }

    constexpr const wchar_t* key = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\"
                                   L"{E89622F2-012D-4A76-86C9-19976496BE80}_is1";
    for (const auto hive : {HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER}) {
        wchar_t location[2048]{};
        DWORD size = sizeof(location);
        if (RegGetValueW(hive, key, L"InstallLocation", RRF_RT_REG_SZ, nullptr, location, &size) ==
            ERROR_SUCCESS) {
            std::wstring path(location);
            if (!path.empty() && path.back() != L'\\')
                path += L'\\';
            path += L"unins000.exe";
            if (exists(path))
                return path;
        }
        wchar_t command[2048]{};
        size = sizeof(command);
        if (RegGetValueW(hive, key, L"UninstallString", RRF_RT_REG_SZ, nullptr, command, &size) ==
            ERROR_SUCCESS) {
            const std::wstring text(command);
            const auto start = text.find(L'"');
            const auto end =
                start == std::wstring::npos ? std::wstring::npos : text.find(L'"', start + 1);
            const auto exe = start != std::wstring::npos && end != std::wstring::npos
                                 ? text.substr(start + 1, end - start - 1)
                                 : text.substr(0, text.find(L".exe") + 4);
            if (exists(exe))
                return exe;
        }
    }
    return {};
}
} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    const auto uninstaller = installedUninstaller();
    if (uninstaller.empty()) {
        MessageBoxW(nullptr, L"welleyz is not installed on this computer.", L"welleyz",
                    MB_OK | MB_ICONINFORMATION);
        return 1;
    }

    const auto launched =
        ShellExecuteW(nullptr, L"runas", uninstaller.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if ((INT_PTR)launched <= 32) {
        MessageBoxW(nullptr, L"Could not start the welleyz uninstaller.", L"welleyz",
                    MB_OK | MB_ICONERROR);
        return 1;
    }
    return 0;
}
