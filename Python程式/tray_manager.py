import math

import pystray
from PIL import Image, ImageDraw


def _create_image():
    size = 64
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    cx = cy = size // 2
    r = size // 2 - 3
    draw.ellipse([3, 3, size - 3, size - 3], outline="#00FF41", width=3)
    draw.ellipse([7, 7, size - 7, size - 7], fill="#111111")
    for angle in range(0, 360, 30):
        rad = math.radians(angle - 90)
        x1 = cx + int((r - 5) * math.cos(rad))
        y1 = cy + int((r - 5) * math.sin(rad))
        x2 = cx + int((r - 10) * math.cos(rad))
        y2 = cy + int((r - 10) * math.sin(rad))
        draw.line([x1, y1, x2, y2], fill="#00FF41", width=2)
    draw.line([cx, cy, cx, cy - 16], fill="#00FF41", width=3)
    draw.line([cx, cy, cx + 10, cy], fill="#00FF41", width=2)
    draw.ellipse([cx - 3, cy - 3, cx + 3, cy + 3], fill="#00FF41")
    return img


class TrayManager:
    def __init__(self, on_settings=None, on_exit=None):
        self.on_settings = on_settings
        self.on_exit = on_exit
        self._icon = None

    def start(self):
        img = _create_image()
        menu = pystray.Menu(
            pystray.MenuItem("設定", self._on_settings_clicked),
            pystray.Menu.SEPARATOR,
            pystray.MenuItem("離開", self._on_exit_clicked)
        )
        self._icon = pystray.Icon("ai_clock", img, "AI時鐘", menu)
        self._icon.run_detached()

    def stop(self):
        if self._icon:
            self._icon.stop()
            self._icon = None

    def _on_settings_clicked(self, *args):
        if self.on_settings:
            self.on_settings()

    def _on_exit_clicked(self, *args):
        if self.on_exit:
            self.on_exit()
