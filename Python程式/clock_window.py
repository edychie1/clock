import ctypes
import os
import tkinter as tk
from datetime import datetime
from zoneinfo import ZoneInfo

from PIL import Image, ImageTk


def _font_family(config):
    return config.get("font_family") or "Comic Sans MS"


def _weight_str(w):
    return "bold" if isinstance(w, int) and w >= 600 else "normal"


class ClockWindow:
    def __init__(self, config, on_settings=None):
        self.config = config
        self.on_settings = on_settings
        self._drag_data = {"x": 0, "y": 0}
        self.bg_image = None
        self.bg_photo = None

        self.window = tk.Toplevel()
        self.window.title("AI時鐘")
        self.window.overrideredirect(True)
        self.window.attributes("-topmost", True)
        self.window.attributes("-alpha", config.get("opacity"))

        bg = config.get("bg_color")
        self.window.configure(bg=bg)

        self.canvas = tk.Canvas(self.window, bg=bg, highlightthickness=0)
        self.canvas.pack(fill=tk.BOTH, expand=True)

        self._update_transparency()

        size = config.get("font_size")
        date_size = max(12, int(size * 0.45))
        fg = config.get("font_color")
        w = config.get("width")
        h = self._calc_height(size)

        time_y = int(size * 0.8) + 8
        line_y = time_y + int(size * 0.5) + 10
        date_y = line_y + int(date_size * 0.5) + 10
        cx = w // 2

        family = _font_family(config)
        self.time_text_id = self.canvas.create_text(
            cx, time_y, text="",
font=(family, size, _weight_str(config.get("font_weight"))),
            fill=fg, anchor=tk.CENTER, tags="time"
        )
        self.line_id = self.canvas.create_line(
            0, line_y, w, line_y,
            fill=fg, width=1, tags="separator"
        )
        self.date_text_id = self.canvas.create_text(
            cx, date_y, text="",
            font=(family, date_size),
            fill=fg, anchor=tk.CENTER, tags="date"
        )

        self.window.geometry(f"{w}x{h}")

        self.canvas.bind("<Button-1>", self._start_drag)
        self.canvas.bind("<B1-Motion>", self._do_drag)
        self.canvas.bind("<ButtonRelease-1>", self._stop_drag)
        self.canvas.bind("<Double-Button-1>", self._open_settings)

        x, y = config.get("x"), config.get("y")
        if x is not None and y is not None:
            self.window.geometry(f"+{x}+{y}")
        else:
            self._place_default()

        self.window.protocol("WM_DELETE_WINDOW", self.hide)
        self.window.bind("<Alt-F4>", lambda e: self.hide())

        self._force_topmost()
        self.window.after(2000, self._force_topmost)
        self.window.after(5000, self._periodic_topmost)

        self._load_bg_image()
        self._update_display()

    def _calc_height(self, size):
        date_size = max(12, int(size * 0.45))
        return int(size * 1.6) + int(date_size * 1.6) + 22

    def _update_transparency(self):
        bg_type = self.config.get("bg_type")
        bg = self.config.get("bg_color")
        if bg_type == "transparent":
            self.window.attributes("-transparentcolor", bg)
        else:
            self.window.attributes("-transparentcolor", "")

    def _force_topmost(self):
        try:
            hwnd = int(str(self.window.winfo_id()), 0)
            ctypes.windll.user32.SetWindowPos(
                ctypes.wintypes.HWND(hwnd), -1,
                0, 0, 0, 0, 0x0002 | 0x0001)
        except Exception:
            pass
        self.window.attributes("-topmost", True)

    def _periodic_topmost(self):
        self._force_topmost()
        self.window.after(5000, self._periodic_topmost)

    def _load_bg_image(self):
        self.canvas.delete("bg_image")
        self.bg_photo = None
        self.bg_image = None

        if self.config.get("bg_type") != "image":
            return

        path = self.config.get("bg_image")
        if not path or not os.path.isfile(path):
            return

        try:
            img = Image.open(path)
            w = self.config.get("width")
            h = self._calc_height(self.config.get("font_size"))
            img = img.resize((w, h), Image.LANCZOS)
            self.bg_photo = ImageTk.PhotoImage(img)
            self.canvas.create_image(0, 0, image=self.bg_photo, anchor=tk.NW, tags="bg_image")
            self.canvas.tag_lower("bg_image")
        except Exception:
            pass

    def _place_default(self):
        self.window.update_idletasks()
        sw = self.window.winfo_screenwidth()
        sh = self.window.winfo_screenheight()
        w = self.config.get("width")
        x = sw - w - 60
        y = sh - 150
        self.window.geometry(f"+{x}+{y}")

    def _start_drag(self, event):
        self._drag_data["x"] = event.x
        self._drag_data["y"] = event.y

    def _do_drag(self, event):
        dx = event.x - self._drag_data["x"]
        dy = event.y - self._drag_data["y"]
        x = self.window.winfo_x() + dx
        y = self.window.winfo_y() + dy
        self.window.geometry(f"+{x}+{y}")

    def _stop_drag(self, event):
        self.save_position()

    def _update_display(self):
        try:
            tz = ZoneInfo(self.config.get("timezone"))
            now = datetime.now(tz)
        except (KeyError, TypeError):
            now = datetime.now()
        weekday = now.strftime("%A")
        self.canvas.itemconfig(self.time_text_id, text=now.strftime("%H:%M:%S"))
        self.canvas.itemconfig(self.date_text_id, text=f"{now.year}-{now.month}-{now.day} {weekday}")
        self.window.after(1000, self._update_display)

    def _open_settings(self, event=None):
        if self.on_settings:
            self.on_settings()

    def set_font_size(self, size, weight=None):
        date_size = max(12, int(size * 0.45))
        if weight is None:
            weight = _weight_str(self.config.get("font_weight"))
        else:
            weight = _weight_str(weight)
        family = _font_family(self.config)
        self.canvas.itemconfig(self.time_text_id, font=(family, size, weight))
        self.canvas.itemconfig(self.date_text_id, font=(family, date_size))

    def set_font_family(self, family, size=None, weight=None):
        if size is None:
            size = self.config.get("font_size")
        if weight is None:
            weight = self.config.get("font_weight")
        date_size = max(12, int(size * 0.45))
        wgt = _weight_str(weight)
        self.canvas.itemconfig(self.time_text_id, font=(family, size, wgt))
        self.canvas.itemconfig(self.date_text_id, font=(family, date_size))

    def set_font_color(self, color):
        self.canvas.itemconfig(self.time_text_id, fill=color)
        self.canvas.itemconfig(self.date_text_id, fill=color)
        self.canvas.itemconfig(self.line_id, fill=color)

    def set_background_image(self, path):
        self.config.set("bg_image", path)
        self.config.set("bg_type", "image")
        self._update_transparency()
        self._load_bg_image()

    def show(self):
        self.window.deiconify()
        self.window.lift()

    def hide(self):
        self.window.withdraw()

    def refresh(self):
        size = self.config.get("font_size")
        date_size = max(12, int(size * 0.45))
        w = self.config.get("width")
        h = self._calc_height(size)
        bg = self.config.get("bg_color")
        fg = self.config.get("font_color")

        self.window.configure(bg=bg)
        self.canvas.configure(bg=bg)
        self.window.attributes("-alpha", self.config.get("opacity"))
        self._update_transparency()

        family = _font_family(self.config)
        self.canvas.itemconfig(self.time_text_id, font=(family, size, _weight_str(self.config.get("font_weight"))), fill=fg)
        self.canvas.itemconfig(self.date_text_id, font=(family, date_size), fill=fg)
        self.canvas.itemconfig(self.line_id, fill=fg)

        cx = w // 2
        time_y = int(size * 0.8) + 8
        line_y = time_y + int(size * 0.5) + 10
        date_y = line_y + int(date_size * 0.5) + 10
        self.canvas.coords(self.time_text_id, cx, time_y)
        self.canvas.coords(self.line_id, 0, line_y, w, line_y)
        self.canvas.coords(self.date_text_id, cx, date_y)

        self.window.geometry(f"{w}x{h}")

        self._load_bg_image()

    def save_position(self):
        self.config.set("x", self.window.winfo_x())
        self.config.set("y", self.window.winfo_y())
        self.config.save()

    def destroy(self):
        self.save_position()
        self.window.destroy()
