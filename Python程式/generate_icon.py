import math
from PIL import Image, ImageDraw


def create_icon():
    img = Image.new("RGBA", (64, 64), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    cx = cy = 32
    r = 29

    draw.ellipse([3, 3, 61, 61], outline="#00FF41", width=3)
    draw.ellipse([7, 7, 57, 57], fill="#111111")

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

    img.save("clock.ico", format="ICO", sizes=[(64, 64)])
    print("已產生 clock.ico")


if __name__ == "__main__":
    create_icon()
