import os
import sys
import threading
import traceback
import tkinter as tk

from config_manager import ConfigManager
from clock_window import ClockWindow
from settings_window import SettingsWindow
from tray_manager import TrayManager

LOG_FILE = os.path.join(os.environ.get("USERPROFILE", os.environ.get("TEMP", ".")), "ai_clock_debug.log")


def log_error(msg):
    try:
        with open(LOG_FILE, "a", encoding="utf-8") as f:
            f.write(f"{msg}\n")
    except Exception:
        pass


class App:
    def __init__(self):
        self._settings_evt = threading.Event()
        self._exit_evt = threading.Event()

        self.config = ConfigManager()
        self.root = tk.Tk()
        self.root.withdraw()

        self.clock = None
        self.settings = None
        self.tray = None

        self._init_clock()
        self._init_tray()

    def _init_clock(self):
        self.clock = ClockWindow(self.config, on_settings=self._show_settings_impl)

    def _init_tray(self):
        self.tray = TrayManager(
            on_settings=self._request_settings,
            on_exit=self._request_exit
        )
        self.tray.start()

    def _request_settings(self):
        self._settings_evt.set()

    def _request_exit(self):
        self._exit_evt.set()

    def _poll_events(self):
        if self._settings_evt.is_set():
            self._settings_evt.clear()
            self._show_settings_impl()

        if self._exit_evt.is_set():
            self._exit_evt.clear()
            self._exit_app_impl()
            return

        self.root.after(50, self._poll_events)

    def _show_settings_impl(self):
        try:
            if self.settings is not None:
                try:
                    if self.settings.window.winfo_exists():
                        self.settings.window.lift()
                        self.settings.window.focus_force()
                        return
                except tk.TclError:
                    self.settings = None

            self.settings = SettingsWindow(
                self.config,
                self.clock,
                self.root,
                on_save=self._on_settings_saved
            )
        except Exception as e:
            log_error(f"_show_settings_impl: {e}\n{traceback.format_exc()}")

    def _on_settings_saved(self):
        pass

    def _exit_app_impl(self):
        try:
            if self.clock:
                self.clock.save_position()
                self.clock.destroy()
            if self.tray:
                self.tray.stop()
        except Exception as e:
            log_error(f"exit error: {e}")
        try:
            self.root.quit()
            self.root.destroy()
        except Exception:
            pass
        sys.exit(0)

    def run(self):
        self.root.protocol("WM_DELETE_WINDOW", self._hide_to_tray)
        self.root.after(50, self._poll_events)
        self.root.mainloop()

    def _hide_to_tray(self):
        if self.clock:
            self.clock.hide()


if __name__ == "__main__":
    app = App()
    app.run()
