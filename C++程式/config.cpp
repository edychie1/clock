#include "config.h"
#include <Windows.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdlib>

static std::wstring GetExeDir() {
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(NULL, buf, MAX_PATH);
    std::wstring path(buf);
    size_t pos = path.rfind(L'\\');
    if (pos != std::wstring::npos)
        path = path.substr(0, pos + 1);
    return path;
}

static std::wstring GetConfigPath() {
    return GetExeDir() + L"config.json";
}

static std::string WstringToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), NULL, 0, NULL, NULL);
    if (len <= 0) return {};
    std::string result(len, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &result[0], len, NULL, NULL);
    return result;
}

static std::wstring Utf8ToWstring(const std::string& str) {
    if (str.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
    if (len <= 0) return {};
    std::wstring result(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &result[0], len);
    return result;
}

static std::string WstringToAnsi(const std::wstring& wstr) {
    if (wstr.empty()) return {};
    int len = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), (int)wstr.size(), NULL, 0, NULL, NULL);
    if (len <= 0) return {};
    std::string result(len, 0);
    WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), (int)wstr.size(), &result[0], len, NULL, NULL);
    return result;
}

static std::wstring GetAppDataConfigPath() {
    wchar_t buf[MAX_PATH];
    DWORD ret = GetEnvironmentVariableW(L"APPDATA", buf, MAX_PATH);
    if (ret == 0 || ret >= MAX_PATH) return L"";
    std::wstring path(buf);
    path += L"\\AIClock";
    CreateDirectoryW(path.c_str(), NULL);
    return path + L"\\config.json";
}

static std::wstring Trim(const std::wstring& s) {
    size_t start = s.find_first_not_of(L" \t\r\n");
    size_t end = s.find_last_not_of(L" \t\r\n");
    if (start == std::wstring::npos) return L"";
    return s.substr(start, end - start + 1);
}

static std::wstring Unescape(const std::wstring& s) {
    std::wstring r;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == L'\\' && i + 1 < s.size()) {
            wchar_t c = s[i + 1];
            if (c == L'"') r += L'"';
            else if (c == L'\\') r += L'\\';
            else if (c == L'n') r += L'\n';
            else if (c == L't') r += L'\t';
            else { r += L'\\'; r += c; }
            ++i;
        } else {
            r += s[i];
        }
    }
    return r;
}

static std::wstring Escape(const std::wstring& s) {
    std::wstring r;
    for (wchar_t c : s) {
        if (c == L'"') r += L"\\\"";
        else if (c == L'\\') r += L"\\\\";
        else if (c == L'\n') r += L"\\n";
        else if (c == L'\t') r += L"\\t";
        else r += c;
    }
    return r;
}

static std::wstring FindJsonStr(const std::wstring& json, const std::wstring& key) {
    std::wstring search = L"\"" + key + L"\"";
    size_t pos = json.find(search);
    if (pos == std::wstring::npos) return L"";
    pos = json.find(L':', pos + search.size());
    if (pos == std::wstring::npos) return L"";
    pos = json.find_first_not_of(L" \t\r\n", pos + 1);
    if (pos == std::wstring::npos) return L"";
    if (json[pos] == L'"') {
        ++pos;
        std::wstring val;
        for (size_t i = pos; i < json.size(); ++i) {
            if (json[i] == L'\\' && i + 1 < json.size()) {
                val += json[i];
                val += json[i + 1];
                ++i;
            } else if (json[i] == L'"') {
                break;
            } else {
                val += json[i];
            }
        }
        return Unescape(val);
    }
    size_t end = json.find_first_of(L",}\n\r", pos);
    if (end == std::wstring::npos) end = json.size();
    std::wstring val = Trim(json.substr(pos, end - pos));
    if (val == L"null") return L"";
    return val;
}

static int FindJsonInt(const std::wstring& json, const std::wstring& key, int def) {
    std::wstring val = FindJsonStr(json, key);
    if (val.empty()) return def;
    try { return std::stoi(val); } catch (...) { return def; }
}

static double FindJsonDouble(const std::wstring& json, const std::wstring& key, double def) {
    std::wstring val = FindJsonStr(json, key);
    if (val.empty()) return def;
    try { return std::stod(val); } catch (...) { return def; }
}

static bool FindJsonBool(const std::wstring& json, const std::wstring& key, bool def) {
    std::wstring val = FindJsonStr(json, key);
    if (val.empty()) return def;
    std::transform(val.begin(), val.end(), val.begin(), ::towlower);
    return val == L"true";
}

static bool ReadJsonFile(const std::wstring& path, std::wstring& outJson) {
    std::ifstream file(WstringToAnsi(path), std::ios::binary);
    if (!file.is_open()) return false;
    std::stringstream ss;
    ss << file.rdbuf();
    outJson = Utf8ToWstring(ss.str());
    return true;
}

static bool WriteJsonFile(const std::wstring& path, const std::wstring& json) {
    std::string utf8 = WstringToUtf8(json);
    std::ofstream file(WstringToAnsi(path), std::ios::binary);
    if (!file.is_open()) return false;
    file << utf8;
    return true;
}

void Config::Load() {
    std::wstring path = GetConfigPath();
    std::wstring json;

    if (ReadJsonFile(path, json)) {
        _ParseJson(json);
    } else {
        // fallback: try APPDATA (migration from old versions)
        std::wstring appdataPath = GetAppDataConfigPath();
        if (!appdataPath.empty() && ReadJsonFile(appdataPath, json)) {
            _ParseJson(json);
            // migrate to new location
            WriteJsonFile(path, json);
        }
    }

    // The registry Run key is the source of truth for auto-start; keep in sync
    auto_start = IsAutoStartEnabled();
}

void Config::_ParseJson(const std::wstring& json) {
    width = FindJsonInt(json, L"width", width);
    font_size = FindJsonInt(json, L"font_size", font_size);
    font_color = FindJsonStr(json, L"font_color");
    if (font_color.empty()) font_color = L"#00FF41";
    bg_color = FindJsonStr(json, L"bg_color");
    if (bg_color.empty()) bg_color = L"#000000";
    opacity = FindJsonDouble(json, L"opacity", opacity);
    timezone = FindJsonStr(json, L"timezone");
    if (timezone.empty()) timezone = L"Asia/Taipei";
    auto_start = FindJsonBool(json, L"auto_start", auto_start);
    bg_type = FindJsonStr(json, L"bg_type");
    if (bg_type.empty()) bg_type = L"transparent";
    bg_image = FindJsonStr(json, L"bg_image");
    font_weight = FindJsonInt(json, L"font_weight", font_weight);
    font_family = FindJsonStr(json, L"font_family");
    if (font_family.empty()) font_family = L"Comic Sans MS";
    seconds_size = FindJsonInt(json, L"seconds_size", seconds_size);
    seconds_gap = FindJsonInt(json, L"seconds_gap", seconds_gap);
    time_line_gap = FindJsonInt(json, L"time_line_gap", time_line_gap);
    line_date_gap = FindJsonInt(json, L"line_date_gap", line_date_gap);
    line_width = FindJsonInt(json, L"line_width", line_width);
    line_color = FindJsonStr(json, L"line_color");
    if (line_color.empty()) line_color = L"#FFFFFF";
    hour_size = FindJsonInt(json, L"hour_size", 100);
    minute_size = FindJsonInt(json, L"minute_size", 100);
    hour_minute_gap = FindJsonInt(json, L"hour_minute_gap", 0);
    x = FindJsonInt(json, L"x", -1);
    y = FindJsonInt(json, L"y", -1);
}

void Config::Save() {
    std::wstring json;
    json += L"{\n";
    auto add = [&](const std::wstring& k, const std::wstring& v) {
        json += L"  \"" + k + L"\": \"" + Escape(v) + L"\",\n";
    };
    auto add_int = [&](const std::wstring& k, int v) {
        json += L"  \"" + k + L"\": " + std::to_wstring(v) + L",\n";
    };
    auto add_double = [&](const std::wstring& k, double v) {
        json += L"  \"" + k + L"\": " + std::to_wstring(v) + L",\n";
    };
    auto add_bool = [&](const std::wstring& k, bool v) {
        json += L"  \"" + k + L"\": " + (v ? L"true" : L"false") + L",\n";
    };
    auto add_nullable = [&](const std::wstring& k, const std::wstring& v) {
        if (v.empty())
            json += L"  \"" + k + L"\": null,\n";
        else
            json += L"  \"" + k + L"\": \"" + Escape(v) + L"\",\n";
    };

    add_int(L"width", width);
    add_int(L"font_size", font_size);
    add(L"font_color", font_color);
    add(L"bg_color", bg_color);
    add_double(L"opacity", opacity);
    add(L"timezone", timezone);
    add_bool(L"auto_start", auto_start);
    add(L"bg_type", bg_type);
    add_nullable(L"bg_image", bg_image);
    add_int(L"font_weight", font_weight);
    add(L"font_family", font_family);
    add_int(L"seconds_size", seconds_size);
    add_int(L"seconds_gap", seconds_gap);
    add_int(L"time_line_gap", time_line_gap);
    add_int(L"line_date_gap", line_date_gap);
    add_int(L"line_width", line_width);
    add(L"line_color", line_color);
    add_int(L"hour_size", hour_size);
    add_int(L"minute_size", minute_size);
    add_int(L"hour_minute_gap", hour_minute_gap);
    add_int(L"x", x);
    json += L"  \"y\": " + std::to_wstring(y) + L"\n";
    json += L"}\n";

    WriteJsonFile(GetConfigPath(), json);
}

static bool ReadAutoStartEntry(std::wstring& outValue) {
    HKEY hKey;
    LONG ret = RegOpenKeyExW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0, KEY_QUERY_VALUE, &hKey);
    if (ret != ERROR_SUCCESS) return false;
    wchar_t buf[1024] = { 0 };
    DWORD size = sizeof(buf);
    LONG qr = RegQueryValueExW(hKey, L"AIClock", NULL, NULL, (BYTE*)buf, &size);
    RegCloseKey(hKey);
    if (qr != ERROR_SUCCESS) return false;
    outValue = buf;
    return true;
}

bool Config::IsAutoStartEnabled() {
    std::wstring v;
    return ReadAutoStartEntry(v);
}

bool Config::SetAutoStart(bool enable) {
    HKEY hKey;
    LONG ret = RegOpenKeyExW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0, KEY_SET_VALUE, &hKey);
    if (ret != ERROR_SUCCESS) return false;

    LONG result = ERROR_SUCCESS;
    if (enable) {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(NULL, exePath, MAX_PATH);
        std::wstring quoted = L"\"" + std::wstring(exePath) + L"\"";
        result = RegSetValueExW(hKey, L"AIClock", 0, REG_SZ,
            (BYTE*)quoted.c_str(), (DWORD)((quoted.size() + 1) * sizeof(wchar_t)));
    } else {
        result = RegDeleteValueW(hKey, L"AIClock");
    }
    RegCloseKey(hKey);
    if (result != ERROR_SUCCESS) return false;

    auto_start = enable;
    Save();
    return true;
}
