#pragma once
#include <string>
#include <vector>

struct Config {
    int width = 200;
    int font_size = 40;
    std::wstring font_color = L"#00FF41";
    std::wstring bg_color = L"#000000";
    double opacity = 0.85;
    std::wstring timezone = L"Asia/Taipei";
    bool auto_start = false;
    std::wstring bg_type = L"transparent";
    std::wstring bg_image;
    int font_weight = 700;
    std::wstring font_family = L"Comic Sans MS";
    int seconds_size = 60;  // seconds font size as % of main font size
    int seconds_gap = -4;   // gap between minutes and seconds (pixels, negative = overlap)
    int time_line_gap = 20;  // pixels from time text bottom to separator line
    int line_date_gap = 0;  // pixels from separator line to date text top
    int line_width = 200;    // separator line width in pixels
    std::wstring line_color = L"#FFFFFF"; // separator line color
    int hour_size = 100;     // hour font size as % of main font size (100 = same)
    int minute_size = 100;   // minute font size as % of main font size (100 = same)
    int hour_minute_gap = 0; // gap between hour and minute text (pixels)
    int x = -1;
    int y = -1;

    void Load();
    void Save();
    bool SetAutoStart(bool enable);
    bool IsAutoStartEnabled();
    void _ParseJson(const std::wstring& json);
};
