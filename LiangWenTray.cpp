// LiangWenTray.cpp
// 梁文峰/梁文谷 托盘提醒程序（C++/Win32，兼容 MinGW 和 MSVC）

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <string>
#include <ctime>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "kernel32.lib")

// 常量
#define WM_TRAYICON   (WM_APP + 1)
#define ID_EXIT       1001
#define TIMER_CHECK   1
#define CHECK_INTERVAL_MS  1000

// 全局变量
HINSTANCE g_hInst = nullptr;
HWND g_hWnd = nullptr;
NOTIFYICONDATAW g_nid = {};
HICON g_iconFeng = nullptr;   // 梁文峰图标
HICON g_iconGu = nullptr;     // 梁文谷图标
int g_currentState = -1;      // 0=梁文峰, 1=梁文谷
bool g_trayAdded = false;
bool g_weekendActive = false; // 周末标记（用于在周末开始时弹出提示）

// 函数声明
enum class Period { Feng = 0, Gu = 1 };
Period GetCurrentPeriod();
void GetBeijingDateTime(SYSTEMTIME &st);
void GetBeijingTime(int &hour, int &minute);
bool IsWeekend();
const wchar_t* WeekdayName(int weekday);
void GetBalloonTexts(Period period, wchar_t* info, wchar_t* title);
std::wstring GetExeDir();
bool LoadIcons();
void AddTrayIcon();
void UpdateTrayIcon(Period period);
void ShowBalloon(Period period);
void RemoveTrayIcon();
void ShowContextMenu(HWND hwnd);
void InstallStartup();
void UninstallStartup();
void CheckStartupStatus();

// 窗口过程
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_TIMER:
        if (wParam == TIMER_CHECK) {
            Period p = GetCurrentPeriod();
            int newState = static_cast<int>(p);
            bool weekend = IsWeekend();
            // 时期切换，或刚刚进入周末时，触发气泡提醒
            if (newState != g_currentState || (weekend && !g_weekendActive)) {
                g_currentState = newState;
                g_weekendActive = weekend;
                UpdateTrayIcon(p);
                ShowBalloon(p);
            }
        }
        return 0;

    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP) {
            ShowContextMenu(hwnd);
        }
        return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) == ID_EXIT) {
            DestroyWindow(hwnd);
        }
        return 0;

    case WM_DESTROY:
        RemoveTrayIcon();
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

// 获取当前北京时间（UTC+8，含星期）
void GetBeijingDateTime(SYSTEMTIME &st) {
    SYSTEMTIME utc;
    GetSystemTime(&utc);   // UTC 时间

    // 转为 FILETIME 并加上 8 小时
    FILETIME ft;
    SystemTimeToFileTime(&utc, &ft);
    ULARGE_INTEGER ul;
    ul.LowPart = ft.dwLowDateTime;
    ul.HighPart = ft.dwHighDateTime;
    ul.QuadPart += 8ULL * 3600ULL * 10000000ULL;   // +8 小时（100ns 单位）
    ft.dwLowDateTime = ul.LowPart;
    ft.dwHighDateTime = ul.HighPart;
    FileTimeToSystemTime(&ft, &st);
}

// 获取当前北京时间的时分
void GetBeijingTime(int &hour, int &minute) {
    SYSTEMTIME st;
    GetBeijingDateTime(st);
    hour = st.wHour;
    minute = st.wMinute;
}

// 星期名称
const wchar_t* WeekdayName(int weekday) {
    static const wchar_t* names[] = {
        L"星期日", L"星期一", L"星期二", L"星期三",
        L"星期四", L"星期五", L"星期六"
    };
    return names[weekday];
}

// 是否为周末（按北京时间判断：周六 / 周日）
bool IsWeekend() {
    SYSTEMTIME st;
    GetBeijingDateTime(st);
    return (st.wDayOfWeek == 6 || st.wDayOfWeek == 0);
}

// 判断当前时期（周末全天为梁文谷时期）
Period GetCurrentPeriod() {
    if (IsWeekend()) {
        return Period::Gu;
    }

    int hour, minute;
    GetBeijingTime(hour, minute);

    bool morning = (hour >= 9 && hour < 12);
    bool afternoon = (hour >= 14 && hour < 18);
    if (morning || afternoon) {
        return Period::Feng;
    }
    return Period::Gu;
}

// 生成气泡通知文本（周末显示“今天是周X，全天为梁文谷时期”）
void GetBalloonTexts(Period period, wchar_t* info, wchar_t* title) {
    if (IsWeekend()) {
        SYSTEMTIME st;
        GetBeijingDateTime(st);
        wsprintfW(info, L"今天是%s，全天为梁文谷时期", WeekdayName(st.wDayOfWeek));
        wcscpy(title, L"梁文谷");
        return;
    }

    int hour, minute;
    GetBeijingTime(hour, minute);
    wchar_t timeStr[16];
    wsprintfW(timeStr, L"%02d:%02d", hour, minute);
    if (period == Period::Feng) {
        wsprintfW(info, L"现在是%s，梁文峰时期", timeStr);
        wcscpy(title, L"梁文峰");
    } else {
        wsprintfW(info, L"现在是%s，梁文谷时期", timeStr);
        wcscpy(title, L"梁文谷");
    }
}

// 获取 exe 所在目录（包括末尾反斜杠）
std::wstring GetExeDir() {
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring full(path);
    size_t pos = full.find_last_of(L"\\/");
    if (pos != std::wstring::npos) {
        return full.substr(0, pos + 1);
    }
    return L"";
}

// 加载图标（优先使用 exe 目录下的 1.ico / 2.ico，失败则使用系统默认图标）
bool LoadIcons() {
    std::wstring dir = GetExeDir();

    std::wstring fengPath = dir + L"1.ico";
    std::wstring guPath = dir + L"2.ico";

    g_iconFeng = (HICON)LoadImageW(nullptr, fengPath.c_str(), IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
    g_iconGu = (HICON)LoadImageW(nullptr, guPath.c_str(), IMAGE_ICON, 0, 0, LR_LOADFROMFILE);

    if (!g_iconFeng) {
        g_iconFeng = LoadIcon(nullptr, IDI_APPLICATION);
    }
    if (!g_iconGu) {
        g_iconGu = LoadIcon(nullptr, IDI_APPLICATION);
    }

    return true; // 至少系统默认图标能加载
}

// 添加托盘图标（首次启动时调用，并弹出初始气泡）
void AddTrayIcon() {
    Period p = GetCurrentPeriod();
    g_currentState = static_cast<int>(p);
    g_weekendActive = IsWeekend();

    // 配置 NOTIFYICONDATA
    ZeroMemory(&g_nid, sizeof(g_nid));
    g_nid.cbSize = sizeof(NOTIFYICONDATAW);
    g_nid.hWnd = g_hWnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_INFO;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = (p == Period::Feng) ? g_iconFeng : g_iconGu;
    wcscpy(g_nid.szTip, (p == Period::Feng) ? L"梁文峰" : L"梁文谷");

    // 气泡文本（周末提示全天梁文谷）
    GetBalloonTexts(p, g_nid.szInfo, g_nid.szInfoTitle);
    g_nid.dwInfoFlags = NIIF_INFO;
    g_nid.uTimeout = 10000;  // 10秒

    Shell_NotifyIconW(NIM_ADD, &g_nid);
    g_trayAdded = true;
}

// 更新托盘图标和悬停提示
void UpdateTrayIcon(Period period) {
    if (!g_trayAdded) return;

    g_nid.uFlags = NIF_ICON | NIF_TIP;
    g_nid.hIcon = (period == Period::Feng) ? g_iconFeng : g_iconGu;
    wcscpy(g_nid.szTip, (period == Period::Feng) ? L"梁文峰" : L"梁文谷");
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

// 弹出气泡通知
void ShowBalloon(Period period) {
    if (!g_trayAdded) return;

    g_nid.uFlags = NIF_INFO;
    GetBalloonTexts(period, g_nid.szInfo, g_nid.szInfoTitle);
    g_nid.dwInfoFlags = NIIF_INFO;
    g_nid.uTimeout = 10000;

    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

// 删除托盘图标
void RemoveTrayIcon() {
    if (g_trayAdded) {
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        g_trayAdded = false;
    }
}

// 显示右键菜单
void ShowContextMenu(HWND hwnd) {
    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    AppendMenuW(hMenu, MF_STRING, ID_EXIT, L"退出");

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);

    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
    DestroyMenu(hMenu);
}

// 注册开机自启（写入 HKCU Run，写后回读校验，失败给出具体错误码）
void InstallStartup() {
    HKEY hKey = nullptr;
    LONG lRet = RegCreateKeyExW(HKEY_CURRENT_USER,
                                L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                                0, nullptr, 0,
                                KEY_SET_VALUE | KEY_QUERY_VALUE,
                                nullptr, &hKey, nullptr);
    if (lRet != ERROR_SUCCESS) {
        wchar_t msg[160];
        wsprintfW(msg, L"打开注册表失败（错误码 %lu）", (unsigned long)lRet);
        MessageBoxW(nullptr, msg, L"梁文峰/梁文谷", MB_OK | MB_ICONERROR);
        return;
    }

    // 获取 exe 完整路径（动态缓冲，避免长路径被截断）
    DWORD bufSize = MAX_PATH;
    std::wstring exePath(bufSize, L'\0');
    DWORD len = 0;
    while ((len = GetModuleFileNameW(nullptr, &exePath[0], bufSize)) >= bufSize) {
        bufSize *= 2;
        exePath.resize(bufSize);
    }
    exePath.resize(len);
    std::wstring cmd = L"\"" + exePath + L"\"";

    lRet = RegSetValueExW(hKey, L"LiangWenTray", 0, REG_SZ,
                          reinterpret_cast<const BYTE*>(cmd.c_str()),
                          static_cast<DWORD>((cmd.size() + 1) * sizeof(wchar_t)));
    if (lRet != ERROR_SUCCESS) {
        wchar_t msg[160];
        wsprintfW(msg, L"写入注册表失败（错误码 %lu）", (unsigned long)lRet);
        MessageBoxW(nullptr, msg, L"梁文峰/梁文谷", MB_OK | MB_ICONERROR);
        RegCloseKey(hKey);
        return;
    }

    // 回读校验
    wchar_t readBack[1024] = {};
    DWORD cb = sizeof(readBack);
    DWORD type = 0;
    lRet = RegQueryValueExW(hKey, L"LiangWenTray", nullptr, &type,
                            reinterpret_cast<LPBYTE>(readBack), &cb);
    RegCloseKey(hKey);

    std::wstring okMsg = L"已添加开机自启：\n" + cmd;
    if (lRet == ERROR_SUCCESS && type == REG_SZ && cmd == readBack) {
        MessageBoxW(nullptr, okMsg.c_str(), L"梁文峰/梁文谷", MB_OK | MB_ICONINFORMATION);
    } else {
        wchar_t msg[512];
        wsprintfW(msg,
                  L"已写入注册表，但回读校验失败（错误码 %lu）。\n若重启后仍不自启，请检查杀毒 / 安全软件是否拦截，或在任务管理器「启动」页确认状态。",
                  (unsigned long)lRet);
        MessageBoxW(nullptr, msg, L"梁文峰/梁文谷", MB_OK | MB_ICONWARNING);
    }
}

// 取消开机自启
void UninstallStartup() {
    HKEY hKey = nullptr;
    LONG lRet = RegOpenKeyExW(HKEY_CURRENT_USER,
                              L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                              0, KEY_SET_VALUE, &hKey);
    if (lRet != ERROR_SUCCESS) {
        MessageBoxW(nullptr, L"打开注册表失败，无法取消自启", L"梁文峰/梁文谷", MB_OK | MB_ICONERROR);
        return;
    }

    lRet = RegDeleteValueW(hKey, L"LiangWenTray");
    RegCloseKey(hKey);

    if (lRet == ERROR_SUCCESS || lRet == ERROR_FILE_NOT_FOUND) {
        MessageBoxW(nullptr, L"已移除开机自启", L"梁文峰/梁文谷", MB_OK | MB_ICONINFORMATION);
    } else {
        wchar_t msg[160];
        wsprintfW(msg, L"删除注册表项失败（错误码 %lu）", (unsigned long)lRet);
        MessageBoxW(nullptr, msg, L"梁文峰/梁文谷", MB_OK | MB_ICONERROR);
    }
}

// 查看当前开机自启状态（--check）
void CheckStartupStatus() {
    HKEY hKey = nullptr;
    LONG lRet = RegOpenKeyExW(HKEY_CURRENT_USER,
                              L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                              0, KEY_QUERY_VALUE, &hKey);
    if (lRet != ERROR_SUCCESS) {
        MessageBoxW(nullptr, L"尚未注册开机自启（无法打开 Run 键）", L"梁文峰/梁文谷", MB_OK | MB_ICONINFORMATION);
        return;
    }

    wchar_t value[1024] = {};
    DWORD cb = sizeof(value);
    DWORD type = 0;
    lRet = RegQueryValueExW(hKey, L"LiangWenTray", nullptr, &type,
                            reinterpret_cast<LPBYTE>(value), &cb);
    RegCloseKey(hKey);

    if (lRet == ERROR_SUCCESS && type == REG_SZ && value[0]) {
        std::wstring msg = L"已注册开机自启，启动命令：\n" + std::wstring(value) +
            L"\n\n若重启后未自启，请检查：\n1. 任务管理器 →「启动」页是否被禁用\n" +
            L"2. 杀毒 / 安全软件是否拦截了该 exe\n3. 该路径下的 exe 是否仍然存在";
        MessageBoxW(nullptr, msg.c_str(), L"梁文峰/梁文谷", MB_OK | MB_ICONINFORMATION);
    } else {
        MessageBoxW(nullptr, L"尚未注册开机自启（注册表中没有 LiangWenTray 项）",
                    L"梁文峰/梁文谷", MB_OK | MB_ICONINFORMATION);
    }
}

// 主入口
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    g_hInst = hInstance;

    // 处理命令行参数（安装 / 卸载 / 查看自启）
    const wchar_t* cmdLine = GetCommandLineW();
    if (wcsstr(cmdLine, L"--install")) {
        InstallStartup();
        return 0;
    }
    if (wcsstr(cmdLine, L"--uninstall")) {
        UninstallStartup();
        return 0;
    }
    if (wcsstr(cmdLine, L"--check")) {
        CheckStartupStatus();
        return 0;
    }

    // 加载图标
    LoadIcons();

    // 注册窗口类
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"LiangWenTrayWindow";
    wc.hIcon = g_iconFeng;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    if (!RegisterClassW(&wc)) {
        MessageBoxW(nullptr, L"窗口类注册失败", L"错误", MB_OK | MB_ICONERROR);
        return 1;
    }

    // 创建隐藏窗口
    g_hWnd = CreateWindowExW(0, wc.lpszClassName, L"LiangWenTray",
                             0, 0, 0, 0, 0,
                             nullptr, nullptr, hInstance, nullptr);
    if (!g_hWnd) {
        MessageBoxW(nullptr, L"窗口创建失败", L"错误", MB_OK | MB_ICONERROR);
        return 1;
    }

    // 添加托盘图标（并弹出初始气泡）
    AddTrayIcon();

    // 启动定时器（每秒检查一次）
    SetTimer(g_hWnd, TIMER_CHECK, CHECK_INTERVAL_MS, nullptr);

    // 消息循环
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // 清理
    if (g_iconFeng) DestroyIcon(g_iconFeng);
    if (g_iconGu) DestroyIcon(g_iconGu);

    return 0;
}
