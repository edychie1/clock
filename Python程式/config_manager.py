import json
import os
import sys
import winreg

CONFIG_FILE = "config.json"

DEFAULTS = {
    "width": 280,
    "font_size": 40,
    "font_color": "#00FF41",
    "bg_color": "#000000",
    "opacity": 0.85,
    "timezone": "Asia/Taipei",
    "auto_start": False,
    "bg_type": "transparent",
    "bg_image": None,
    "font_weight": 700,
    "font_family": "Comic Sans MS",
    "x": None,
    "y": None
}


class ConfigManager:
    def __init__(self):
        self.config_path = self._get_config_path()
        self.data = dict(DEFAULTS)
        self.load()

    def _get_config_path(self):
        if getattr(sys, 'frozen', False):
            base = os.path.dirname(os.path.abspath(sys.argv[0]))
        else:
            base = os.path.dirname(os.path.abspath(__file__))
        return os.path.join(base, CONFIG_FILE)

    def load(self):
        try:
            with open(self.config_path, 'r', encoding='utf-8') as f:
                loaded = json.load(f)
                self.data.update(loaded)
        except (FileNotFoundError, json.JSONDecodeError):
            pass

    def save(self):
        with open(self.config_path, 'w', encoding='utf-8') as f:
            json.dump(self.data, f, indent=2, ensure_ascii=False)

    def get(self, key):
        return self.data.get(key, DEFAULTS.get(key))

    def set(self, key, value):
        self.data[key] = value

    def set_auto_start(self, enable):
        key_path = r"Software\Microsoft\Windows\CurrentVersion\Run"
        app_name = "AIClock"
        if getattr(sys, 'frozen', False):
            exe_path = sys.executable
        else:
            exe_path = os.path.abspath(sys.argv[0])

        try:
            with winreg.OpenKey(winreg.HKEY_CURRENT_USER, key_path, 0, winreg.KEY_SET_VALUE) as key:
                if enable:
                    winreg.SetValueEx(key, app_name, 0, winreg.REG_SZ, f'"{exe_path}"')
                else:
                    try:
                        winreg.DeleteValue(key, app_name)
                    except FileNotFoundError:
                        pass
        except Exception:
            pass

        self.data["auto_start"] = enable
        self.save()
