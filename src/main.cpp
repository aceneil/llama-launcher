// =====================================================================
//  llama-launcher — llama.cpp 配置式启动器 (Win32, 单文件) v0.2
//  交叉编译: x86_64-w64-mingw32-g++ main.cpp -o llama-launcher.exe
//             -mwindows -municode -static -O2
// =====================================================================
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <cstdio>

#pragma comment(lib, "comctl32.lib")

// ---------------- 控件 ID ----------------
enum { IDC_COMBO_MODEL=101, IDC_EDIT_IP, IDC_EDIT_PORT,
       IDC_COMBO_BACKEND, IDC_COMBO_CTX, IDC_COMBO_THINK,
       IDC_CHK_FA, IDC_CHK_KV, IDC_CHK_AUTOBROWSER,
       IDC_CHK_PRELOAD,
       IDC_EDIT_TEMP, IDC_EDIT_MAXTOK, IDC_BTN_START, IDC_BTN_STOP,
       IDC_EDIT_LOG, IDC_STATIC_STATUS, IDC_BTN_REDETECT };
#define IDI_ICON1 101
#define WM_TRAYICON (WM_APP + 1)
#define ID_TRAY_SHOW 201
#define ID_TRAY_EXIT 202

// ---------------- 全局 ----------------
static NOTIFYICONDATAW g_nid = {};
static HWND g_hwnd, g_hComboModel, g_hEditIP, g_hEditPort, g_hComboBackend,
            g_hComboCtx, g_hComboThink, g_hChkFA, g_hChkKV, g_hChkAutoBrowser,
            g_hChkPreload,
            g_hEditTemp, g_hEditMaxTok, g_hBtnStart, g_hBtnStop, g_hBtnRedetect, g_hLog, g_hStatus, g_hStatus2;
static std::wstring g_webUrl = L"http://localhost:8080";
static PROCESS_INFORMATION g_pi = {};
static HANDLE g_hReadPipe = INVALID_HANDLE_VALUE;
static std::wstring g_exeDir;
static std::vector<std::pair<std::wstring,std::wstring>> g_models;  // (显示名, 模型ID)
static HBRUSH g_hLogBrush = nullptr;

static std::wstring ws(const std::string& s) {
    std::wstring r; r.reserve(s.size());
    for (char c : s) r += (wchar_t)(unsigned char)c;
    return r;
}
static std::wstring trim(const std::wstring& s) {
    size_t a = s.find_first_not_of(L" \t\r\n"), b = s.find_last_not_of(L" \t\r\n");
    return (a == std::wstring::npos) ? L"" : s.substr(a, b - a + 1);
}
static void logLine(const std::wstring& line) {
    int len = GetWindowTextLengthW(g_hLog);
    SendMessageW(g_hLog, EM_SETSEL, len, len);
    SendMessageW(g_hLog, EM_REPLACESEL, 0, (LPARAM)line.c_str());
    SendMessageW(g_hLog, EM_REPLACESEL, 0, (LPARAM)L"\r\n");
    SendMessageW(g_hLog, EM_SCROLLCARET, 0, 0);
}

// ---------------- 硬件检测 ----------------
static std::wstring runCmd(const std::wstring& cmd) {
    SECURITY_ATTRIBUTES sa = {sizeof(sa), nullptr, TRUE};
    HANDLE r, w; CreatePipe(&r, &w, &sa, 0); SetHandleInformation(w, HANDLE_FLAG_INHERIT, 1);
    STARTUPINFOW si = {sizeof(si)}; si.dwFlags = STARTF_USESTDHANDLES; si.hStdOutput = si.hStdError = w;
    PROCESS_INFORMATION pi = {};
    std::wstring full = L"cmd.exe /c " + cmd;
    std::wstring result;
    if (CreateProcessW(nullptr, &full[0], nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(w);
        char buf[4096]; DWORD n;
        while (ReadFile(r, buf, sizeof(buf)-1, &n, nullptr) && n) { buf[n]=0; result += ws(buf); }
        WaitForSingleObject(pi.hProcess, 5000);
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    } else CloseHandle(w);
    CloseHandle(r);
    return result;
}

struct HwInfo { std::wstring gpu; double vramGB=0; std::wstring vendor=L"cpu";
                double ramGB=0; int cores=0; };

static HwInfo detectHardware() {
    HwInfo h;
    std::wstring out = runCmd(L"nvidia-smi --query-gpu=name,memory.total --format=csv,noheader,nounits");
    if (!out.empty()) {
        size_t p = out.find(L",");
        if (p != std::wstring::npos) {
            h.gpu = trim(out.substr(0, p));
            h.vramGB = _wtof(trim(out.substr(p+1)).c_str()) / 1024.0;
            h.vendor = L"cuda";
        }
    }
    if (h.vendor == L"cpu") {
        std::wstring cards = runCmd(L"wmic path win32_VideoController get name");
        std::wistringstream iss(cards); std::wstring line;
        while (std::getline(iss, line)) {
            std::wstring n = trim(line);
            if (n.find(L"Radeon") != std::wstring::npos || n.find(L"AMD") != std::wstring::npos ||
                n.find(L"Arc") != std::wstring::npos) { h.gpu = n; h.vendor = L"vulkan"; break; }
        }
    }
    MEMORYSTATUSEX ms = {sizeof(ms)}; GlobalMemoryStatusEx(&ms);
    h.ramGB = (double)ms.ullTotalPhys / 1024.0 / 1024.0 / 1024.0;
    SYSTEM_INFO si; GetSystemInfo(&si); h.cores = si.dwNumberOfProcessors;
    return h;
}

static void computeBestConfig(const HwInfo& h, std::wstring& ctx, std::wstring& tier) {
    if (h.vramGB >= 32)      { ctx = L"32768"; tier = L"32G+ 档"; }
    else if (h.vramGB >= 24) { ctx = L"32768"; tier = L"24G 档"; }
    else if (h.vramGB >= 16) { ctx = L"32768"; tier = L"16G 档"; }
    else if (h.vramGB >= 11) { ctx = L"16384"; tier = L"12G 档"; }
    else if (h.vramGB >= 7)  { ctx = L"8192";  tier = L"8G 档"; }
    else if (h.vramGB >= 4)  { ctx = L"4096";  tier = L"6G 及以下"; }
    else                     { ctx = L"4096";  tier = L"纯 CPU 模式"; }
    if (h.ramGB < 16 && _wtoi(ctx.c_str()) > 8192) ctx = L"8192";
}

// ---------------- 模型扫描(递归) ----------------
// 记录 (显示名, 模型ID)。llama.cpp router 的 --models-dir 命名规则:
//   子目录模型 → 子目录名;根目录散文件 → 文件名去掉 .gguf
static void scanDirRecursive(const std::wstring& dir, const std::wstring& relDir, std::vector<std::pair<std::wstring,std::wstring>>& out) {
    std::wstring pattern = dir + L"\\*";
    WIN32_FIND_DATAW fd;
    HANDLE hf = FindFirstFileW(pattern.c_str(), &fd);
    if (hf == INVALID_HANDLE_VALUE) return;
    do {
        std::wstring name = fd.cFileName;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (name == L"." || name == L"..") continue;
            scanDirRecursive(dir + L"\\" + name, relDir.empty() ? name : relDir + L"\\" + name, out);
        } else {
            std::wstring low = name;
            for (auto& c : low) c = towlower(c);
            // 跳过 mmproj 多模态投影文件(不能单独当主模型)
            if (low.size() > 5 && low.substr(low.size()-5) == L".gguf" &&
                low.find(L"mmproj") == std::wstring::npos) {
                // 模型 ID:子目录模型取第一级子目录名,根目录散文件取文件名去 .gguf
                std::wstring id;
                size_t slash = relDir.find(L'\\');
                if (!relDir.empty()) id = (slash == std::wstring::npos) ? relDir : relDir.substr(0, slash);
                else id = name.substr(0, name.size()-5);
                out.emplace_back(name, id);
            }
        }
    } while (FindNextFileW(hf, &fd));
    FindClose(hf);
}

static void scanModels() {
    g_models.clear();
    scanDirRecursive(g_exeDir + L"\\models", L"", g_models);
    SendMessageW(g_hComboModel, CB_RESETCONTENT, 0, 0);
    if (g_models.empty())
        SendMessageW(g_hComboModel, CB_ADDSTRING, 0, (LPARAM)L"(models 目录无 GGUF)");
    for (auto& m : g_models) SendMessageW(g_hComboModel, CB_ADDSTRING, 0, (LPARAM)m.first.c_str());
    SendMessageW(g_hComboModel, CB_SETCURSEL, 0, 0);
}

// ---------------- llama-server 参数探测 ----------------
static std::wstring g_help;
static bool hasFlag(const wchar_t* f) {
    std::wstring h = g_help;
    std::wstring::size_type p = 0;
    while ((p = h.find(f, p)) != std::wstring::npos) {
        if (p == 0 || h[p-1] == L' ' || h[p-1] == L'\n' || h[p-1] == L'\r' || h[p-1] == L'\t') {
            wchar_t after = (p + wcslen(f) < h.size()) ? h[p + wcslen(f)] : 0;
            if (!after || after == L' ' || after == L'\n' || after == L'\r' || after == L'\t' || after == L'[' || after == L'=') return true;
        }
        p += wcslen(f);
    }
    return false;
}

// ---------------- 启动 / 停止 ----------------
static std::wstring findServer() {
    std::vector<std::wstring> found;
    std::wstring pattern = g_exeDir + L"\\*";
    WIN32_FIND_DATAW fd; HANDLE hf = FindFirstFileW(pattern.c_str(), &fd);
    if (hf != INVALID_HANDLE_VALUE) {
        do {
            std::wstring name = fd.cFileName;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                if (name == L"." || name == L"..") continue;
                std::wstring sub = g_exeDir + L"\\" + name + L"\\llama-server.exe";
                if (GetFileAttributesW(sub.c_str()) != INVALID_FILE_ATTRIBUTES) found.push_back(sub);
            } else if (name == L"llama-server.exe") found.push_back(g_exeDir + L"\\llama-server.exe");
        } while (FindNextFileW(hf, &fd));
        FindClose(hf);
    }
    if (found.empty()) return L"";
    for (auto& f : found) {
        std::wstring low = f;
        for (auto& c : low) c = towlower(c);
        if (low.find(L"cuda") != std::wstring::npos || low.find(L"vulkan") != std::wstring::npos) return f;
    }
    return found[0];
}

static void readPipeThread(LPVOID) {
    char buf[4096]; DWORD n;
    std::string acc;
    while (ReadFile(g_hReadPipe, buf, sizeof(buf)-1, &n, nullptr) && n) {
        buf[n] = 0; acc += buf;
        size_t pos;
        while ((pos = acc.find('\n')) != std::string::npos) {
            std::string line = acc.substr(0, pos); acc.erase(0, pos+1);
            while (!line.empty() && (line.back()=='\r'||line.back()=='\n')) line.pop_back();
            PostMessageW(g_hwnd, WM_USER+1, 0, (LPARAM)_wcsdup(ws(line).c_str()));
        }
    }
    PostMessageW(g_hwnd, WM_USER+2, 0, 0);
    return;
}

static void stopServer() {
    if (g_pi.hProcess) {
        TerminateProcess(g_pi.hProcess, 0);
        CloseHandle(g_pi.hProcess); CloseHandle(g_pi.hThread);
        g_pi = {};
    }
    if (g_hReadPipe != INVALID_HANDLE_VALUE) { CloseHandle(g_hReadPipe); g_hReadPipe = INVALID_HANDLE_VALUE; }
    EnableWindow(g_hBtnStart, TRUE); EnableWindow(g_hBtnStop, FALSE);
    logLine(L"[启动器] 服务已停止");
}

static void startServer() {
    if (g_pi.hProcess) { logLine(L"[启动器] 已在运行"); return; }
    std::wstring server = findServer();
    if (server.empty()) { logLine(L"[启动器] 未找到 llama-server.exe(请放在本程序同目录或子目录)"); return; }
    if (g_models.empty()) { logLine(L"[启动器] models 目录没有模型"); return; }

    wchar_t ip[64], port[16], temp[16], mtok[16];
    GetWindowTextW(g_hEditIP, ip, 64); GetWindowTextW(g_hEditPort, port, 16);
    GetWindowTextW(g_hEditTemp, temp, 16); GetWindowTextW(g_hEditMaxTok, mtok, 16);
    int ctxIdx = SendMessageW(g_hComboCtx, CB_GETCURSEL, 0, 0);
    wchar_t ctxBuf[16]; SendMessageW(g_hComboCtx, CB_GETLBTEXT, ctxIdx, (LPARAM)ctxBuf);
    std::wstring ctxStr = ctxBuf;
    bool fa = SendMessageW(g_hChkFA, BM_GETCHECK, 0, 0) == BST_CHECKED;
    bool kv = SendMessageW(g_hChkKV, BM_GETCHECK, 0, 0) == BST_CHECKED;

    g_help = runCmd(L"\"" + server + L"\" --help");

    std::wstring args = L"\"" + server + L"\"";
    args += L" --models-dir \"" + g_exeDir + L"\\models\"";
    args += L" --host " + std::wstring(trim(ip).empty()?L"127.0.0.1":trim(ip));
    args += L" --port " + std::wstring(port);
    args += L" -c " + ctxStr;

    // ★ 启动时自动加载所选模型(router 模式的 load-on-startup)
    //    写入 exe 同目录 presets.ini 的 [模型ID] 段,并传 --models-preset;
    //    只更新本模型段,不破坏文件里其他已有配置。
    if (SendMessageW(g_hChkPreload, BM_GETCHECK, 0, 0) == BST_CHECKED &&
        hasFlag(L"--models-preset")) {
        int sel = SendMessageW(g_hComboModel, CB_GETCURSEL, 0, 0);
        if (sel >= 0 && sel < (int)g_models.size()) {
            std::wstring presetPath = g_exeDir + L"\\presets.ini";
            WritePrivateProfileStringW(g_models[sel].second.c_str(), L"load-on-startup", L"true", presetPath.c_str());
            args += L" --models-preset \"" + presetPath + L"\"";
            logLine(L"[启动器] 已设 " + g_models[sel].second + L" 启动自动加载 (presets.ini)");
        } else {
            logLine(L"[启动器] 未选模型,跳过自动加载设置");
        }
    }
    if (hasFlag(L"--jinja")) args += L" --jinja";
    if (fa && hasFlag(L"--flash-attn")) args += L" -fa on";
    if (kv && hasFlag(L"--cache-type-k")) args += L" --cache-type-k q8_0 --cache-type-v q8_0";
    int thinkIdx = SendMessageW(g_hComboThink, CB_GETCURSEL, 0, 0);
    if (thinkIdx == 0) {
        if (hasFlag(L"--reasoning-budget")) args += L" --reasoning-budget 0";
        else if (hasFlag(L"--chat-template-kwargs")) args += L" --chat-template-kwargs {\"enable_thinking\":false}";
    } else if (thinkIdx == 1) {
        if (hasFlag(L"--reasoning-budget")) args += L" --reasoning-budget -1";
    }
    if (hasFlag(L"--temp")) args += L" --temp " + std::wstring(temp);
    if (hasFlag(L"--top-p")) args += L" --top-p 0.8";
    if (hasFlag(L"--min-p")) args += L" --min-p 0";
    if (hasFlag(L"--presence-penalty")) args += L" --presence-penalty 1.0";
    if (hasFlag(L"--n-predict")) args += L" -n " + std::wstring(mtok);
    else if (hasFlag(L"--predict")) args += L" --predict " + std::wstring(mtok);
    if (hasFlag(L"--fit-target")) args += L" --fit-target 1024";
    else if (g_help.find(L"cuda") != std::wstring::npos) args += L" -ngl 99";

    SECURITY_ATTRIBUTES sa = {sizeof(sa), nullptr, TRUE};
    HANDLE r, w; if (!CreatePipe(&r, &w, &sa, 0)) return;
    SetHandleInformation(r, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW si = {sizeof(si)};
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = si.hStdError = w; si.wShowWindow = SW_HIDE;
    std::vector<wchar_t> cmdline(args.begin(), args.end()); cmdline.push_back(0);
    BOOL ok = CreateProcessW(nullptr, cmdline.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, g_exeDir.c_str(), &si, &g_pi);
    CloseHandle(w);
    if (!ok) {
        logLine(L"[启动器] 启动失败: " + std::to_wstring(GetLastError()));
        CloseHandle(r); return;
    }
    g_hReadPipe = r;
    EnableWindow(g_hBtnStart, FALSE); EnableWindow(g_hBtnStop, TRUE);
    logLine(L"[启动器] 启动命令:");
    logLine(args);
    CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)readPipeThread, nullptr, 0, nullptr);
    // 更新 WebUI 地址(0.0.0.0 → localhost),启动日志末尾会以链接形式输出
    {
        std::wstring ipv = trim(ip).empty() ? L"localhost" : trim(ip);
        if (ipv == L"0.0.0.0") ipv = L"localhost";
        g_webUrl = L"http://" + ipv + L":" + std::wstring(port);
    }
    // 启动日志刷完(8 秒)后在末尾追加提示
    SetTimer(g_hwnd, 1, 8000, nullptr);
    if (SendMessageW(g_hChkAutoBrowser, BM_GETCHECK, 0, 0) == BST_CHECKED) {
        std::wstring url = L"http://" + std::wstring(trim(ip).empty()?L"127.0.0.1":trim(ip)) + L":" + port;
        ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
}

// ---------------- 配置保存 ----------------
static std::wstring iniPath() { return g_exeDir + L"\\launcher.ini"; }
static void saveConfig() {
    wchar_t ip[64], port[16], temp[16], mtok[16];
    GetWindowTextW(g_hEditIP, ip, 64); GetWindowTextW(g_hEditPort, port, 16);
    GetWindowTextW(g_hEditTemp, temp, 16); GetWindowTextW(g_hEditMaxTok, mtok, 16);
    int m = SendMessageW(g_hComboModel, CB_GETCURSEL, 0, 0);
    int b = SendMessageW(g_hComboBackend, CB_GETCURSEL, 0, 0);
    int c = SendMessageW(g_hComboCtx, CB_GETCURSEL, 0, 0);
    int t = SendMessageW(g_hComboThink, CB_GETCURSEL, 0, 0);
    WritePrivateProfileStringW(L"launcher", L"model", std::to_wstring(m).c_str(), iniPath().c_str());
    WritePrivateProfileStringW(L"launcher", L"ip", ip, iniPath().c_str());
    WritePrivateProfileStringW(L"launcher", L"port", port, iniPath().c_str());
    WritePrivateProfileStringW(L"launcher", L"backend", std::to_wstring(b).c_str(), iniPath().c_str());
    WritePrivateProfileStringW(L"launcher", L"ctx", std::to_wstring(c).c_str(), iniPath().c_str());
    WritePrivateProfileStringW(L"launcher", L"think", std::to_wstring(t).c_str(), iniPath().c_str());
    WritePrivateProfileStringW(L"launcher", L"fa", SendMessageW(g_hChkFA,BM_GETCHECK,0,0)==BST_CHECKED?L"1":L"0", iniPath().c_str());
    WritePrivateProfileStringW(L"launcher", L"kv", SendMessageW(g_hChkKV,BM_GETCHECK,0,0)==BST_CHECKED?L"1":L"0", iniPath().c_str());
    WritePrivateProfileStringW(L"launcher", L"autobrowser", SendMessageW(g_hChkAutoBrowser,BM_GETCHECK,0,0)==BST_CHECKED?L"1":L"0", iniPath().c_str());
    WritePrivateProfileStringW(L"launcher", L"preload", SendMessageW(g_hChkPreload,BM_GETCHECK,0,0)==BST_CHECKED?L"1":L"0", iniPath().c_str());
    WritePrivateProfileStringW(L"launcher", L"temp", temp, iniPath().c_str());
    WritePrivateProfileStringW(L"launcher", L"maxtok", mtok, iniPath().c_str());
}
static void loadConfig() {
    wchar_t buf[128];
    GetPrivateProfileStringW(L"launcher", L"ip", L"0.0.0.0", buf, 128, iniPath().c_str()); SetWindowTextW(g_hEditIP, buf);
    GetPrivateProfileStringW(L"launcher", L"port", L"8080", buf, 128, iniPath().c_str()); SetWindowTextW(g_hEditPort, buf);
    GetPrivateProfileStringW(L"launcher", L"temp", L"0.7", buf, 128, iniPath().c_str()); SetWindowTextW(g_hEditTemp, buf);
    GetPrivateProfileStringW(L"launcher", L"maxtok", L"8192", buf, 128, iniPath().c_str()); SetWindowTextW(g_hEditMaxTok, buf);
    int m = GetPrivateProfileIntW(L"launcher", L"model", 0, iniPath().c_str());
    int b = GetPrivateProfileIntW(L"launcher", L"backend", 0, iniPath().c_str());
    int c = GetPrivateProfileIntW(L"launcher", L"ctx", 0, iniPath().c_str());
    int t = GetPrivateProfileIntW(L"launcher", L"think", 0, iniPath().c_str());
    int cnt = SendMessageW(g_hComboModel, CB_GETCOUNT, 0, 0);
    if (m >= 0 && m < cnt) SendMessageW(g_hComboModel, CB_SETCURSEL, m, 0);
    SendMessageW(g_hComboBackend, CB_SETCURSEL, b, 0);
    SendMessageW(g_hComboCtx, CB_SETCURSEL, c, 0);
    SendMessageW(g_hComboThink, CB_SETCURSEL, t, 0);
    SendMessageW(g_hChkFA, BM_SETCHECK, GetPrivateProfileIntW(L"launcher", L"fa", 1, iniPath().c_str()) ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(g_hChkKV, BM_SETCHECK, GetPrivateProfileIntW(L"launcher", L"kv", 1, iniPath().c_str()) ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(g_hChkAutoBrowser, BM_SETCHECK, GetPrivateProfileIntW(L"launcher", L"autobrowser", 0, iniPath().c_str()) ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(g_hChkPreload, BM_SETCHECK, GetPrivateProfileIntW(L"launcher", L"preload", 1, iniPath().c_str()) ? BST_CHECKED : BST_UNCHECKED, 0);
}

// ---------------- 窗口 ----------------
static void addLabel(HWND parent, const wchar_t* text, int x, int y, int w, int h) {
    CreateWindowW(L"STATIC", text, WS_CHILD|WS_VISIBLE, x, y, w, h, parent, nullptr, nullptr, nullptr);
}

static void applyDetected() {
    HwInfo h = detectHardware();
    std::wstring ctx, tier;
    computeBestConfig(h, ctx, tier);
    int backendSel = 0;
    if (h.vendor == L"cuda") backendSel = 2; else if (h.vendor == L"vulkan") backendSel = 3;
    SendMessageW(g_hComboBackend, CB_SETCURSEL, backendSel, 0);
    const wchar_t* ctxItems[] = {L"4096", L"8192", L"16384", L"32768"};
    int ctxSel = 0;
    for (int i = 0; i < 4; i++) if (ctx == ctxItems[i]) { ctxSel = i; break; }
    SendMessageW(g_hComboCtx, CB_SETCURSEL, ctxSel, 0);
    std::wstring cpuLine = L"CPU " + std::to_wstring(h.cores) + L" 核 · 内存 " + std::to_wstring((int)h.ramGB) + L"G";
    std::wstring gpuLine = (h.vendor == L"cpu")
        ? (tier)
        : (h.gpu + L" · " + tier);
    SetWindowTextW(g_hStatus, cpuLine.c_str());
    SetWindowTextW(g_hStatus2, gpuLine.c_str());
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        g_hwnd = hwnd;
        g_exeDir.resize(MAX_PATH);
        DWORD n = GetModuleFileNameW(nullptr, &g_exeDir[0], MAX_PATH);
        g_exeDir.resize(n);
        size_t pos = g_exeDir.find_last_of(L"\\/");
        if (pos != std::wstring::npos) g_exeDir = g_exeDir.substr(0, pos);

        // 布局:左侧表单,右侧日志(cmd 风格)
        const int LX = 16, PW = 371, RX = 395, RW = 400, LH = 20;
        const int LW0 = 56, IX = LX + LW0 + 8;   // 标签宽 + 间距
        int y = 8;
        // 状态两行:CPU+内存 / 显卡
        g_hStatus = CreateWindowW(L"STATIC", L"", WS_CHILD|WS_VISIBLE, LX, y, PW, LH, hwnd, (HMENU)IDC_STATIC_STATUS, nullptr, nullptr);
        g_hStatus2 = CreateWindowW(L"STATIC", L"", WS_CHILD|WS_VISIBLE, LX, y+20, PW, LH, hwnd, (HMENU)IDC_STATIC_STATUS, nullptr, nullptr);
        y += 44;
        // 模型(下拉展开宽度 620,显示全名)
        addLabel(hwnd, L"模型:", LX, y+2, LW0, LH);
        g_hComboModel = CreateWindowW(L"COMBOBOX", L"", WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST|WS_VSCROLL, IX, y, PW-LW0-8, 200, hwnd, (HMENU)IDC_COMBO_MODEL, nullptr, nullptr);
        SendMessageW(g_hComboModel, CB_SETDROPPEDWIDTH, 620, 0);
        y += 30;
        // IP + 端口
        addLabel(hwnd, L"IP:", LX, y+2, LW0, LH);
        g_hEditIP = CreateWindowW(L"EDIT", L"0.0.0.0", WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL, IX, y, 110, LH+4, hwnd, (HMENU)IDC_EDIT_IP, nullptr, nullptr);
        addLabel(hwnd, L"端口:", IX+120, y+2, 44, LH);
        g_hEditPort = CreateWindowW(L"EDIT", L"8080", WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL, IX+164, y, 60, LH+4, hwnd, (HMENU)IDC_EDIT_PORT, nullptr, nullptr);
        y += 30;
        // 后端 / 上下文 / 思考
        addLabel(hwnd, L"后端:", LX, y+2, LW0, LH);
        g_hComboBackend = CreateWindowW(L"COMBOBOX", L"", WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST|WS_VSCROLL, IX, y, 70, 200, hwnd, (HMENU)IDC_COMBO_BACKEND, nullptr, nullptr);
        for (auto* s : {L"自动检测", L"强制 CPU", L"CUDA", L"Vulkan"}) SendMessageW(g_hComboBackend, CB_ADDSTRING, 0, (LPARAM)s);
        addLabel(hwnd, L"上下文:", IX+84, y+2, 52, LH);
        g_hComboCtx = CreateWindowW(L"COMBOBOX", L"", WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST|WS_VSCROLL, IX+136, y, 56, 200, hwnd, (HMENU)IDC_COMBO_CTX, nullptr, nullptr);
        for (auto* s : {L"4K", L"8K", L"16K", L"32K"}) SendMessageW(g_hComboCtx, CB_ADDSTRING, 0, (LPARAM)s);
        addLabel(hwnd, L"思考:", IX+204, y+2, 36, LH);
        g_hComboThink = CreateWindowW(L"COMBOBOX", L"", WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST|WS_VSCROLL, IX+240, y, 46, 200, hwnd, (HMENU)IDC_COMBO_THINK, nullptr, nullptr);
        for (auto* s : {L"off", L"on", L"auto"}) SendMessageW(g_hComboThink, CB_ADDSTRING, 0, (LPARAM)s);
        y += 30;
        // 开关
        g_hChkFA = CreateWindowW(L"BUTTON", L"Flash Attention", WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX, LX, y, 118, LH, hwnd, (HMENU)IDC_CHK_FA, nullptr, nullptr);
        g_hChkKV = CreateWindowW(L"BUTTON", L"KV 量化", WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX, LX+126, y, 76, LH, hwnd, (HMENU)IDC_CHK_KV, nullptr, nullptr);
        g_hChkAutoBrowser = CreateWindowW(L"BUTTON", L"自动开浏览器", WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX, LX+210, y, 100, LH, hwnd, (HMENU)IDC_CHK_AUTOBROWSER, nullptr, nullptr);
        y += 28;
        // 温度 / 生成上限(同行)
        addLabel(hwnd, L"温度:", LX, y+2, LW0, LH);
        g_hEditTemp = CreateWindowW(L"EDIT", L"0.7", WS_CHILD|WS_VISIBLE|WS_BORDER, IX, y, 60, LH+4, hwnd, (HMENU)IDC_EDIT_TEMP, nullptr, nullptr);
        addLabel(hwnd, L"生成上限:", IX+70, y+2, 64, LH);
        g_hEditMaxTok = CreateWindowW(L"EDIT", L"8192", WS_CHILD|WS_VISIBLE|WS_BORDER, IX+134, y, 60, LH+4, hwnd, (HMENU)IDC_EDIT_MAXTOK, nullptr, nullptr);
        y += 34;
        // 启动时自动加载所选模型(router 模式 load-on-startup)
        g_hChkPreload = CreateWindowW(L"BUTTON", L"启动自动加载所选模型(写 presets.ini)", WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX, LX, y, 280, LH, hwnd, (HMENU)IDC_CHK_PRELOAD, nullptr, nullptr);
        y += 28;
        // 按钮(相对左侧表单居中,与上行保持间距)
        y += 42;
        const int BTN_W = 90, BTN_GAP = 10;
        int btnX = LX + (PW - (BTN_W * 3 + BTN_GAP * 2)) / 2;
        g_hBtnStart = CreateWindowW(L"BUTTON", L"启 动", WS_CHILD|WS_VISIBLE, btnX, y, BTN_W, 30, hwnd, (HMENU)IDC_BTN_START, nullptr, nullptr);
        g_hBtnStop = CreateWindowW(L"BUTTON", L"停 止", WS_CHILD|WS_VISIBLE, btnX+BTN_W+BTN_GAP, y, BTN_W, 30, hwnd, (HMENU)IDC_BTN_STOP, nullptr, nullptr);
        g_hBtnRedetect = CreateWindowW(L"BUTTON", L"重新检测", WS_CHILD|WS_VISIBLE, btnX+(BTN_W+BTN_GAP)*2, y, BTN_W, 30, hwnd, (HMENU)IDC_BTN_REDETECT, nullptr, nullptr);
        // 右侧日志(cmd 风格)
        g_hLog = CreateWindowW(L"EDIT", L"", WS_CHILD|WS_VISIBLE|WS_BORDER|ES_MULTILINE|ES_AUTOVSCROLL|ES_READONLY|WS_VSCROLL,
            RX, 12, RW, 250, hwnd, (HMENU)IDC_EDIT_LOG, nullptr, nullptr);
        HFONT hfLog = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, FIXED_PITCH, 0, L"Consolas");
        SendMessageW(g_hLog, WM_SETFONT, (WPARAM)hfLog, TRUE);
        g_hLogBrush = CreateSolidBrush(RGB(12, 12, 12));

        EnableWindow(g_hBtnStop, FALSE);
        scanModels();
        loadConfig();
        applyDetected();
        // 创建系统托盘图标
        g_nid.cbSize = sizeof(g_nid);
        g_nid.hWnd = hwnd;
        g_nid.uID = 1;
        g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        g_nid.uCallbackMessage = WM_TRAYICON;
        g_nid.hIcon = LoadIconW(GetModuleHandleW(nullptr), (LPCWSTR)(ULONG_PTR)IDI_ICON1);
        wcscpy(g_nid.szTip, L"Llama Launcher");
        Shell_NotifyIconW(NIM_ADD, &g_nid);
        return 0;
    }
    case WM_TRAYICON: {
        if (lp == WM_LBUTTONUP) {          // 单击:恢复窗口
            ShowWindow(hwnd, SW_SHOW);
            SetForegroundWindow(hwnd);
        } else if (lp == WM_RBUTTONUP) {   // 右键:弹出菜单
            HMENU menu = CreatePopupMenu();
            AppendMenuW(menu, MF_STRING, ID_TRAY_SHOW, L"显示主窗口");
            AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(menu, MF_STRING, ID_TRAY_EXIT, L"退出");
            POINT pt; GetCursorPos(&pt);
            SetForegroundWindow(hwnd);
            TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, nullptr);
            DestroyMenu(menu);
        }
        return 0;
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wp;
        HWND h = (HWND)lp;
        if (h == g_hLog) {
            SetTextColor(hdc, RGB(0xCC, 0xCC, 0xCC));
            SetBkColor(hdc, RGB(12, 12, 12));
            return (LRESULT)g_hLogBrush;
        }
        break;
    }

    case WM_COMMAND: {
        int id = LOWORD(wp);
        if (id == IDC_BTN_START) { saveConfig(); startServer(); }
        else if (id == IDC_BTN_STOP) stopServer();
        else if (id == IDC_BTN_REDETECT) applyDetected();
        else if (id == ID_TRAY_SHOW) {
            ShowWindow(hwnd, SW_SHOW);
            SetForegroundWindow(hwnd);
        } else if (id == ID_TRAY_EXIT) {
            saveConfig();
            stopServer();
            Shell_NotifyIconW(NIM_DELETE, &g_nid);
            DestroyWindow(hwnd);
        }
        break;
    }
    case WM_TIMER:
        if (wp == 1) {          // 启动日志刷完后的 WebUI 提示
            KillTimer(hwnd, 1);
            logLine(L"[启动器] WebUI: " + g_webUrl);
        }
        return 0;
    case WM_USER+1: {
        wchar_t* line = (wchar_t*)lp;
        if (line) { logLine(line); free(line); }
        return 0;
    }
    case WM_USER+2: {
        if (g_pi.hProcess) { WaitForSingleObject(g_pi.hProcess, 0); CloseHandle(g_pi.hProcess); CloseHandle(g_pi.hThread); g_pi = {}; }
        if (g_hReadPipe != INVALID_HANDLE_VALUE) { CloseHandle(g_hReadPipe); g_hReadPipe = INVALID_HANDLE_VALUE; }
        EnableWindow(g_hBtnStart, TRUE); EnableWindow(g_hBtnStop, FALSE);
            logLine(L"[启动器] 进程已退出");
        return 0;
    }
    case WM_CLOSE:
        // 点 X:隐藏到系统托盘(服务继续运行),不退出
        ShowWindow(hwnd, SW_HIDE);
        return 0;
    case WM_DESTROY:
        if (g_hLogBrush) DeleteObject(g_hLogBrush);
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int nCmdShow) {
    const wchar_t CLASS[] = L"LlamaLauncherMain";
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(nullptr, (LPCWSTR)(ULONG_PTR)IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = CLASS;
    wc.hIcon = LoadIconW(hInst, (LPCWSTR)(ULONG_PTR)IDI_ICON1);
    wc.hIconSm = LoadIconW(hInst, (LPCWSTR)(ULONG_PTR)IDI_ICON1);
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowW(CLASS, L"Llama Launcher       ·by aceneil", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 840, 372, nullptr, nullptr, hInst, nullptr);
    ShowWindow(hwnd, nCmdShow);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
