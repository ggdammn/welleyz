#include <windows.h>
#include <winhttp.h>
#include <shellapi.h>
#include <algorithm>
#include <string>
#include <vector>

#ifndef WELLEYZ_VERSION
#define WELLEYZ_VERSION "0.0.0"
#endif
#ifndef WELLEYZ_UPDATE_URL
#define WELLEYZ_UPDATE_URL ""
#endif

namespace {
constexpr int checkId = 101, installId = 102, urlId = 103;
HWND statusLabel = nullptr, installButton = nullptr, urlEdit = nullptr;
std::wstring installerUrl;
std::wstring latestVersion;

std::wstring wide(const std::string& text) {
    if (text.empty())
        return {};
    const auto length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.size(), nullptr, 0);
    std::wstring result((size_t)length, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.size(), &result[0], length);
    return result;
}

std::wstring moduleFolder() {
    wchar_t name[MAX_PATH]{};
    GetModuleFileNameW(nullptr, name, MAX_PATH);
    std::wstring folder(name);
    folder.resize(folder.find_last_of(L"\\/") + 1);
    return folder;
}

std::string jsonString(const std::string& source, const std::string& key, size_t begin = 0) {
    const auto pos = source.find("\"" + key + "\"", begin);
    if (pos == std::string::npos)
        return {};
    auto colon = source.find(':', pos + key.size() + 2);
    if (colon == std::string::npos)
        return {};
    const auto quote = source.find('"', colon + 1);
    if (quote == std::string::npos)
        return {};
    std::string value;
    for (auto i = quote + 1; i < source.size(); ++i) {
        if (source[i] == '"')
            break;
        if (source[i] == '\\' && i + 1 < source.size()) {
            ++i;
            if (source[i] == 'n') {
                value += '\n';
                continue;
            }
        }
        value += source[i];
    }
    return value;
}

std::wstring initialEndpoint() {
    wchar_t saved[2048]{};
    DWORD length = sizeof(saved);
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\shadowave\\welleyz", L"UpdateEndpoint",
                     RRF_RT_REG_SZ, nullptr, saved, &length) == ERROR_SUCCESS)
        return saved;
    const std::string compiled = WELLEYZ_UPDATE_URL;
    if (!compiled.empty())
        return wide(compiled);
    const auto path = moduleFolder() + L"release.json";
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                              0, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return {};
    std::vector<char> data(8192);
    DWORD read = 0;
    const bool ok = ReadFile(file, data.data(), (DWORD)data.size(), &read, nullptr) != 0;
    CloseHandle(file);
    return ok ? wide(jsonString(std::string(data.data(), read), "updateEndpoint")) : L"";
}

bool newer(const std::wstring& candidate, const std::wstring& current) {
    auto numbers = [](std::wstring version) {
        if (!version.empty() && (version[0] == L'v' || version[0] == L'V'))
            version.erase(0, 1);
        std::vector<int> result;
        size_t start = 0;
        for (int i = 0; i < 3; ++i) {
            result.push_back(_wtoi(version.c_str() + start));
            const auto dot = version.find(L'.', start);
            if (dot == std::wstring::npos)
                break;
            start = dot + 1;
        }
        result.resize(3, 0);
        return result;
    };
    return numbers(candidate) > numbers(current);
}

bool httpsGet(const std::wstring& url, std::vector<char>& result,
              HANDLE file = INVALID_HANDLE_VALUE) {
    URL_COMPONENTS parts{sizeof(parts)};
    wchar_t host[256]{}, path[4096]{};
    parts.lpszHostName = host;
    parts.dwHostNameLength = 256;
    parts.lpszUrlPath = path;
    parts.dwUrlPathLength = 4096;
    if (url.rfind(L"https://", 0) != 0 || !WinHttpCrackUrl(url.c_str(), 0, 0, &parts) ||
        parts.nScheme != INTERNET_SCHEME_HTTPS)
        return false;
    const auto session = WinHttpOpen(L"welleyz updater/" WELLEYZ_VERSION,
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0);
    if (!session)
        return false;
    WinHttpSetTimeouts(session, 5000, 5000, 10000, 30000);
    const auto connection = WinHttpConnect(session, host, parts.nPort, 0);
    const auto request = connection
                             ? WinHttpOpenRequest(connection, L"GET", path, nullptr, nullptr,
                                                  WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)
                             : nullptr;
    bool ok = false;
    if (request &&
        WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0,
                           0) &&
        WinHttpReceiveResponse(request, nullptr)) {
        DWORD status = 0, length = sizeof(status);
        if (WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                nullptr, &status, &length, nullptr) &&
            status == 200) {
            ok = true;
            char buffer[32768];
            DWORD count = 0, written = 0;
            for (;;) {
                if (!WinHttpReadData(request, buffer, sizeof(buffer), &count)) {
                    ok = false;
                    break;
                }
                if (count == 0)
                    break;
                if (file == INVALID_HANDLE_VALUE) {
                    if (result.size() + count > 2 * 1024 * 1024) {
                        ok = false;
                        break;
                    }
                    result.insert(result.end(), buffer, buffer + count);
                } else if (!WriteFile(file, buffer, count, &written, nullptr) || written != count) {
                    ok = false;
                    break;
                }
            }
        }
    }
    if (request)
        WinHttpCloseHandle(request);
    if (connection)
        WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    return ok;
}

void status(const std::wstring& message) {
    SetWindowTextW(statusLabel, message.c_str());
    UpdateWindow(statusLabel);
}

void check(HWND window) {
    wchar_t text[2048]{};
    GetWindowTextW(urlEdit, text, 2048);
    const std::wstring endpoint(text);
    if (endpoint.rfind(L"https://api.github.com/repos/", 0) != 0 ||
        endpoint.find(L"/releases/latest") == std::wstring::npos) {
        status(L"Enter a GitHub Releases API URL to check for updates.\r\nExample: "
               L"https://api.github.com/repos/OWNER/REPO/releases/latest");
        return;
    }
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\shadowave\\welleyz", 0, nullptr, 0,
                        KEY_SET_VALUE, nullptr, &key, nullptr) == ERROR_SUCCESS) {
        RegSetValueExW(key, L"UpdateEndpoint", 0, REG_SZ, (const BYTE*)text,
                       (DWORD)((endpoint.size() + 1) * sizeof(wchar_t)));
        RegCloseKey(key);
    }
    EnableWindow(GetDlgItem(window, checkId), FALSE);
    EnableWindow(installButton, FALSE);
    status(L"Checking GitHub Releases...");
    std::vector<char> body;
    if (!httpsGet(endpoint, body))
        status(L"Could not reach GitHub Releases. Check the URL and network connection.");
    else {
        const std::string release(body.begin(), body.end());
        latestVersion = wide(jsonString(release, "tag_name"));
        installerUrl.clear();
        size_t position = 0;
        while ((position = release.find("\"browser_download_url\"", position)) !=
               std::string::npos) {
            const auto asset = wide(jsonString(release, "browser_download_url", position));
            if (asset.rfind(L"https://github.com/", 0) == 0 &&
                asset.find(L"welleyz-") != std::wstring::npos &&
                asset.find(L"-setup.exe") != std::wstring::npos) {
                installerUrl = asset;
                break;
            }
            position += 22;
        }
        if (latestVersion.empty())
            status(L"GitHub returned no release version.");
        else if (!newer(latestVersion, wide(WELLEYZ_VERSION)))
            status(L"welleyz is up to date (" + wide(WELLEYZ_VERSION) + L").");
        else if (installerUrl.empty())
            status(L"Update " + latestVersion + L" found, but it has no welleyz setup.exe asset.");
        else {
            status(L"Update available: " + latestVersion +
                   L"\r\nClick Download and install to run the official setup.");
            EnableWindow(installButton, TRUE);
        }
    }
    EnableWindow(GetDlgItem(window, checkId), TRUE);
}

void install(HWND window) {
    if (installerUrl.empty())
        return;
    wchar_t folder[MAX_PATH]{}, temp[MAX_PATH]{};
    if (!GetTempPathW(MAX_PATH, folder) || !GetTempFileNameW(folder, L"wel", 0, temp)) {
        status(L"Could not create a temporary download file.");
        return;
    }
    const std::wstring filename = std::wstring(temp) + L".exe";
    DeleteFileW(temp);
    const auto file =
        CreateFileW(filename.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, 0, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        status(L"Could not write the update file.");
        return;
    }
    EnableWindow(installButton, FALSE);
    status(L"Downloading welleyz " + latestVersion + L" setup...");
    std::vector<char> unused;
    const bool downloaded = httpsGet(installerUrl, unused, file);
    CloseHandle(file);
    if (!downloaded) {
        DeleteFileW(filename.c_str());
        status(L"Download failed. Check the network connection and try again.");
        EnableWindow(installButton, TRUE);
        return;
    }
    HANDLE verify = CreateFileW(filename.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                OPEN_EXISTING, 0, nullptr);
    char signature[2]{};
    DWORD read = 0;
    const bool executable = verify != INVALID_HANDLE_VALUE &&
                            ReadFile(verify, signature, sizeof(signature), &read, nullptr) &&
                            read == 2 && signature[0] == 'M' && signature[1] == 'Z';
    if (verify != INVALID_HANDLE_VALUE)
        CloseHandle(verify);
    if (!executable) {
        DeleteFileW(filename.c_str());
        status(L"The download is not a Windows installer.");
        EnableWindow(installButton, TRUE);
        return;
    }
    const auto launched =
        ShellExecuteW(window, L"runas", filename.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if ((INT_PTR)launched <= 32) {
        status(L"Setup could not start. Try again after closing FL Studio.");
        EnableWindow(installButton, TRUE);
    } else {
        status(L"Setup started. Close FL Studio before replacing the VST3.\r\nThe installer will "
               L"complete the update.");
    }
}

LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_COMMAND:
        if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == checkId)
            check(window);
        if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == installId)
            install(window);
        if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDCANCEL)
            DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(window, message, wParam, lParam);
    }
}
} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    WNDCLASSW cls{};
    cls.lpfnWndProc = windowProc;
    cls.hInstance = instance;
    cls.lpszClassName = L"WelleyzUpdater";
    cls.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(101));
    cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    cls.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassW(&cls);
    const auto window = CreateWindowExW(
        0, cls.lpszClassName, L"welleyz updater", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 510, 255, nullptr, nullptr, instance, nullptr);
    const auto font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    auto control = [&](const wchar_t* type, const wchar_t* label, DWORD style, int x, int y,
                       int width, int height, int id) {
        const auto child = CreateWindowExW(type == std::wstring(L"EDIT") ? WS_EX_CLIENTEDGE : 0,
                                           type, label, WS_CHILD | WS_VISIBLE | style, x, y, width,
                                           height, window, (HMENU)(INT_PTR)id, instance, nullptr);
        SendMessageW(child, WM_SETFONT, (WPARAM)font, TRUE);
        return child;
    };
    control(L"STATIC",
            L"welleyz updater  |  installed: "
            L"" WELLEYZ_VERSION,
            0, 20, 16, 465, 22, 0);
    control(L"STATIC", L"GitHub Releases API URL", 0, 20, 44, 465, 19, 0);
    urlEdit = control(L"EDIT", initialEndpoint().c_str(), ES_AUTOHSCROLL, 20, 65, 455, 25, urlId);
    statusLabel = control(L"STATIC", L"Enter your repository URL and check for updates.", 0, 20,
                          102, 455, 46, 0);
    control(L"BUTTON", L"Check for updates", BS_PUSHBUTTON, 20, 161, 147, 32, checkId);
    installButton =
        control(L"BUTTON", L"Download and install", BS_PUSHBUTTON, 177, 161, 165, 32, installId);
    EnableWindow(installButton, FALSE);
    control(L"BUTTON", L"Close", BS_PUSHBUTTON, 352, 161, 123, 32, IDCANCEL);
    ShowWindow(window, show);
    UpdateWindow(window);
    MSG message;
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return 0;
}
