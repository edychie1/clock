import tkinter as tk
from tkinter import ttk, colorchooser, filedialog, font


COMMON_TZ = [
    "Asia/Taipei", "Asia/Tokyo", "Asia/Shanghai", "Asia/Hong_Kong",
    "Asia/Seoul", "Asia/Singapore", "Asia/Kolkata",
    "America/New_York", "America/Chicago", "America/Denver",
    "America/Los_Angeles", "America/Sao_Paulo",
    "Europe/London", "Europe/Paris", "Europe/Berlin", "Europe/Moscow",
    "Australia/Sydney", "Australia/Perth",
    "Pacific/Auckland", "UTC"
]


class SettingsWindow:
    def __init__(self, config, clock, root, on_save=None):
        self.config = config
        self.clock = clock
        self.root = root
        self.on_save = on_save

        self.window = tk.Toplevel(root)
        self.window.title("時鐘設定")
        self.window.geometry("420x650")
        self.window.resizable(False, False)
        self.window.attributes("-topmost", True)
        self.window.focus_force()
        self.window.grab_set()

        self._build_ui()
        self._load_values()

        self.window.protocol("WM_DELETE_WINDOW", self.close)

    def _build_ui(self):
        main_frame = ttk.Frame(self.window, padding=15)
        main_frame.pack(fill=tk.BOTH, expand=True)

        row = 0

        ttk.Label(main_frame, text="視窗寬度").grid(row=row, column=0, sticky=tk.W, pady=(0, 2))
        row += 1
        self.width_var = tk.IntVar()
        scale_width = ttk.Scale(main_frame, from_=120, to=2000, variable=self.width_var,
                                command=self._preview_width)
        scale_width.grid(row=row, column=0, sticky=tk.EW, pady=(0, 10))
        ttk.Label(main_frame, textvariable=self.width_var).grid(row=row, column=1, padx=(8, 0))
        row += 1

        ttk.Label(main_frame, text="字型大小").grid(row=row, column=0, sticky=tk.W, pady=(0, 2))
        row += 1
        self.font_size_var = tk.IntVar()
        scale_font = ttk.Scale(main_frame, from_=12, to=240, variable=self.font_size_var,
                               command=self._preview_font)
        scale_font.grid(row=row, column=0, sticky=tk.EW, pady=(0, 10))
        ttk.Label(main_frame, textvariable=self.font_size_var).grid(row=row, column=1, padx=(8, 0))
        row += 1

        ttk.Label(main_frame, text="字體").grid(row=row, column=0, sticky=tk.W, pady=(0, 2))
        row += 1
        self.font_family_var = tk.StringVar()
        all_fonts = sorted(font.families())
        font_combo = ttk.Combobox(main_frame, textvariable=self.font_family_var,
                                  values=all_fonts, state="readonly", width=28)
        font_combo.grid(row=row, column=0, columnspan=2, sticky=tk.W, pady=(0, 10))
        font_combo.bind("<<ComboboxSelected>>", self._preview_font_family)
        row += 1

        ttk.Label(main_frame, text="字型顏色").grid(row=row, column=0, sticky=tk.W, pady=(0, 2))
        row += 1
        color_frame = ttk.Frame(main_frame)
        color_frame.grid(row=row, column=0, columnspan=2, sticky=tk.W, pady=(0, 10))
        self.font_color_var = tk.StringVar()
        self.color_preview = tk.Label(color_frame, text="████████",
                                      font=("Consolas", 14, "bold"), width=10)
        self.color_preview.pack(side=tk.LEFT, padx=(0, 8))
        ttk.Button(color_frame, text="選擇顏色", command=self._choose_color).pack(side=tk.LEFT)
        row += 1

        ttk.Label(main_frame, text="字體粗細").grid(row=row, column=0, sticky=tk.W, pady=(0, 2))
        row += 1
        fw_frame = ttk.Frame(main_frame)
        fw_frame.grid(row=row, column=0, columnspan=2, sticky=tk.W, pady=(0, 10))
        self.font_weight_var = tk.IntVar()
        ttk.Radiobutton(fw_frame, text="細體", variable=self.font_weight_var,
                        value=400, command=self._preview_font_weight).pack(side=tk.LEFT, padx=(0, 10))
        ttk.Radiobutton(fw_frame, text="粗體", variable=self.font_weight_var,
                        value=700, command=self._preview_font_weight).pack(side=tk.LEFT)
        row += 1

        ttk.Label(main_frame, text="透明度").grid(row=row, column=0, sticky=tk.W, pady=(0, 2))
        row += 1
        self.opacity_var = tk.DoubleVar()
        scale_op = ttk.Scale(main_frame, from_=0.1, to=1.0, variable=self.opacity_var,
                             command=self._preview_opacity)
        scale_op.grid(row=row, column=0, sticky=tk.EW, pady=(0, 10))
        self.op_label = ttk.Label(main_frame, text="")
        self.op_label.grid(row=row, column=1, padx=(8, 0))
        row += 1

        ttk.Separator(main_frame, orient=tk.HORIZONTAL).grid(
            row=row, column=0, columnspan=2, sticky=tk.EW, pady=8)
        row += 1

        ttk.Label(main_frame, text="背景設定").grid(row=row, column=0, sticky=tk.W, pady=(0, 2))
        row += 1
        bg_frame = ttk.Frame(main_frame)
        bg_frame.grid(row=row, column=0, columnspan=2, sticky=tk.W, pady=(0, 10))
        self.bg_type_var = tk.StringVar(value="transparent")
        ttk.Radiobutton(bg_frame, text="透明背景", variable=self.bg_type_var,
                        value="transparent", command=self._toggle_bg_type).pack(side=tk.LEFT, padx=(0, 10))
        ttk.Radiobutton(bg_frame, text="自訂圖片", variable=self.bg_type_var,
                        value="image", command=self._toggle_bg_type).pack(side=tk.LEFT)
        row += 1

        self.bg_image_frame = ttk.Frame(main_frame)
        self.bg_image_frame.grid(row=row, column=0, columnspan=2, sticky=tk.W, pady=(0, 10))
        self.bg_image_var = tk.StringVar()
        self.bg_image_entry = ttk.Entry(self.bg_image_frame, textvariable=self.bg_image_var,
                                        width=35, state="readonly")
        self.bg_image_entry.pack(side=tk.LEFT, padx=(0, 6))
        ttk.Button(self.bg_image_frame, text="選擇圖片", command=self._choose_bg_image).pack(side=tk.LEFT)
        row += 1

        ttk.Separator(main_frame, orient=tk.HORIZONTAL).grid(
            row=row, column=0, columnspan=2, sticky=tk.EW, pady=8)
        row += 1

        ttk.Label(main_frame, text="時區").grid(row=row, column=0, sticky=tk.W, pady=(0, 2))
        row += 1
        self.tz_var = tk.StringVar()
        tz_combo = ttk.Combobox(main_frame, textvariable=self.tz_var,
                                values=COMMON_TZ, state="readonly", width=28)
        tz_combo.grid(row=row, column=0, columnspan=2, sticky=tk.W, pady=(0, 10))
        row += 1

        self.auto_start_var = tk.BooleanVar()
        cb = ttk.Checkbutton(main_frame, text="開機自動啟動",
                             variable=self.auto_start_var)
        cb.grid(row=row, column=0, columnspan=2, sticky=tk.W, pady=(0, 15))
        row += 1

        btn_frame = ttk.Frame(main_frame)
        btn_frame.grid(row=row, column=0, columnspan=2, pady=(5, 0))
        ttk.Button(btn_frame, text="儲存", command=self.save, width=12).pack(
            side=tk.LEFT, padx=(0, 8))
        ttk.Button(btn_frame, text="取消", command=self.close, width=12).pack(
            side=tk.LEFT)

        main_frame.columnconfigure(0, weight=1)

    def _toggle_bg_type(self):
        if self.bg_type_var.get() == "image":
            self.bg_image_frame.grid()
        else:
            self.bg_image_frame.grid_remove()

    def _choose_bg_image(self):
        path = filedialog.askopenfilename(
            title="選擇背景圖片",
            filetypes=[("圖片檔案", "*.png *.jpg *.jpeg *.bmp *.gif"), ("所有檔案", "*.*")],
            parent=self.window
        )
        if path:
            self.bg_image_var.set(path)
            if self.clock:
                self.clock.set_background_image(path)

    def _load_values(self):
        self.width_var.set(self.config.get("width"))
        self.font_size_var.set(self.config.get("font_size"))
        self.font_family_var.set(self.config.get("font_family"))
        self.font_color_var.set(self.config.get("font_color"))
        self.opacity_var.set(self.config.get("opacity"))
        self._update_opacity_display()
        self.tz_var.set(self.config.get("timezone"))
        self.auto_start_var.set(self.config.get("auto_start"))
        fw = self.config.get("font_weight")
        self.font_weight_var.set(700 if (isinstance(fw, int) and fw >= 600) else 400)
        self.bg_type_var.set(self.config.get("bg_type"))
        self.bg_image_var.set(self.config.get("bg_image") or "")
        self._toggle_bg_type()
        self._update_color_preview()

    def _update_color_preview(self):
        self.color_preview.configure(fg=self.font_color_var.get())

    def _update_opacity_display(self):
        self.op_label.config(text=f"{self.opacity_var.get():.2f}")

    def _choose_color(self):
        result = colorchooser.askcolor(
            initialcolor=self.font_color_var.get(),
            title="選擇字型顏色",
            parent=self.window
        )
        if result and result[1]:
            self.font_color_var.set(result[1])
            self._update_color_preview()
            if self.clock:
                self.clock.set_font_color(result[1])

    def _calc_height(self, size):
        date_size = max(12, int(size * 0.45))
        return int(size * 1.6) + int(date_size * 1.6) + 22

    def _preview_width(self, *args):
        if self.clock:
            w = int(round(self.width_var.get()))
            self.width_var.set(w)
            size = self.font_size_var.get()
            h = self._calc_height(size)
            self.clock.window.geometry(f"{w}x{h}")
            date_size = max(12, int(size * 0.45))
            time_y = int(size * 0.8) + 8
            line_y = time_y + int(size * 0.5) + 10
            date_y = line_y + int(date_size * 0.5) + 10
            cx = w // 2
            self.clock.canvas.coords(self.clock.time_text_id, cx, time_y)
            self.clock.canvas.coords(self.clock.line_id, 0, line_y, w, line_y)
            self.clock.canvas.coords(self.clock.date_text_id, cx, date_y)

    def _preview_font(self, *args):
        if self.clock:
            size = round(self.font_size_var.get())
            self.font_size_var.set(size)
            fw = round(self.font_weight_var.get())
            self.font_weight_var.set(fw)
            family = self.font_family_var.get()
            self.clock.set_font_family(family, size=size, weight=fw)
            w = max(150, int(size * 8.0))
            self.width_var.set(w)
            h = self._calc_height(size)
            self.clock.window.geometry(f"{w}x{h}")
            date_size = max(12, int(size * 0.45))
            time_y = int(size * 0.8) + 8
            line_y = time_y + int(size * 0.5) + 10
            date_y = line_y + int(date_size * 0.5) + 10
            cx = w // 2
            self.clock.canvas.coords(self.clock.time_text_id, cx, time_y)
            self.clock.canvas.coords(self.clock.line_id, 0, line_y, w, line_y)
            self.clock.canvas.coords(self.clock.date_text_id, cx, date_y)

    def _preview_font_family(self, *args):
        if self.clock:
            family = self.font_family_var.get()
            size = round(self.font_size_var.get())
            fw = round(self.font_weight_var.get())
            self.clock.set_font_family(family, size=size, weight=fw)

    def _preview_font_weight(self, *args):
        if self.clock:
            wgt = self.font_weight_var.get()
            size = round(self.font_size_var.get())
            self.clock.set_font_size(size, weight=wgt)

    def _preview_opacity(self, *args):
        if self.clock:
            self.clock.window.attributes("-alpha", self.opacity_var.get())
        self._update_opacity_display()

    def save(self):
        self.config.set("width", self.width_var.get())
        self.config.set("font_size", self.font_size_var.get())
        self.config.set("font_family", self.font_family_var.get())
        self.config.set("font_color", self.font_color_var.get())
        self.config.set("opacity", self.opacity_var.get())
        self.config.set("timezone", self.tz_var.get())
        self.config.set("font_weight", self.font_weight_var.get())
        self.config.set("bg_type", self.bg_type_var.get())
        self.config.set("bg_image", self.bg_image_var.get() or None)

        auto_start = self.auto_start_var.get()
        self.config.set_auto_start(auto_start)

        self.config.save()

        if self.clock:
            self.clock.refresh()

        if self.on_save:
            self.on_save()

        self.close()

    def close(self):
        try:
            self.window.grab_release()
        except Exception:
            pass
        self.window.destroy()
