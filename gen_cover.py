# Generate the M5Burner cover card (assets/cover.png, 420x300).
# Rendered at 2x and downscaled for crisp text; the photo panel uses a real
# device shot from assets/. Bump VERSION (and TAGLINE if features change),
# re-run, and re-upload the cover in M5Burner alongside the release.
import os
from PIL import Image, ImageDraw, ImageFont

VERSION = "1.2.0"
TAGLINE = "up to 4-input robot TX"
PHOTO   = "assets/20260722_140133.jpg"
PHOTO_CROP = (40, 380, 700, 1080)   # N1 + stick + antenna region of that shot

ROOT = os.path.dirname(os.path.abspath(__file__))
W, H = 840, 600                      # 2x canvas -> saved at 420x300

img = Image.new("RGB", (W, H), (13, 17, 23))
d = ImageDraw.Draw(img)

# Top accent bar
d.rectangle([0, 0, W, 10], fill=(37, 99, 235))

# ── Fonts (Segoe with Arial fallback) ──
def try_font(names, size):
    for n in names:
        try:
            return ImageFont.truetype(f"C:/Windows/Fonts/{n}", size)
        except Exception:
            pass
    return ImageFont.load_default()

font_big  = try_font(["segoeuib.ttf", "arialbd.ttf"], 84)
font_meta = try_font(["segoeui.ttf",  "arial.ttf"],   27)
font_ver  = try_font(["seguisb.ttf",  "arialbd.ttf"], 27)
font_tag  = try_font(["seguisb.ttf",  "arialbd.ttf"], 24)
font_tiny = try_font(["segoeui.ttf",  "arial.ttf"],   23)

# ── Photo panel (right) ──
photo = Image.open(os.path.join(ROOT, PHOTO)).crop(PHOTO_CROP)
PX, PY, PW, PH = 462, 40, 344, 366
photo = photo.resize((PW, PH), Image.LANCZOS)
mask = Image.new("L", (PW, PH), 0)
ImageDraw.Draw(mask).rounded_rectangle([0, 0, PW - 1, PH - 1], radius=26, fill=255)
img.paste(photo, (PX, PY), mask)
d.rounded_rectangle([PX, PY, PX + PW - 1, PY + PH - 1], radius=26,
                    outline=(48, 54, 61), width=3)

# ── Title block (left) ──
d.text((34, 44),  "Nesso", font=font_big, fill=(241, 245, 249))
d.text((34, 132), "base",  font=font_big, fill=(68, 147, 248))
d.text((34, 252), "Arduino Nesso N1  ·  ESP32-C6", font=font_meta, fill=(139, 148, 158))
d.text((34, 294), f"v{VERSION}", font=font_ver, fill=(68, 147, 248))
vw = int(d.textlength(f"v{VERSION}", font=font_ver))
d.text((34 + vw + 14, 294), f"·  {TAGLINE}", font=font_meta, fill=(139, 148, 158))

# ── Divider ──
d.line([34, 416, W - 34, 416], fill=(30, 41, 59), width=2)

# ── Feature tags: (label, bg, border, text) ──
tags = [
    ("WiFi",  (14, 71, 120),  (14, 165, 233), (224, 242, 254)),
    ("BLE",   (76, 29, 149),  (139, 92, 246), (237, 233, 254)),
    ("LoRa",  (6, 78, 59),    (16, 185, 129), (209, 250, 229)),
    ("IR",    (120, 53, 15),  (245, 158, 11), (254, 243, 199)),
    ("RF433", (127, 29, 29),  (239, 68, 68),  (254, 226, 226)),
    ("RFID",  (49, 46, 129),  (99, 102, 241), (224, 231, 255)),
    ("IMU",   (63, 26, 71),   (192, 84, 205),  (250, 226, 255)),
]
misc = [
    ("4 Inputs",   (30, 41, 59), (71, 85, 105), (148, 163, 184)),
    ("Speaker",    (30, 41, 59), (71, 85, 105), (148, 163, 184)),
    ("Web FM",     (30, 41, 59), (71, 85, 105), (148, 163, 184)),
    ("Serial CLI", (30, 41, 59), (71, 85, 105), (148, 163, 184)),
]

TAG_H, PAD = 44, 18

def draw_tags(row, y):
    x = 34
    for label, bg, border, fg in row:
        tw = int(d.textlength(label, font=font_tag))
        d.rounded_rectangle([x, y, x + tw + PAD * 2, y + TAG_H],
                            radius=9, fill=bg, outline=border, width=2)
        d.text((x + PAD, y + 8), label, font=font_tag, fill=fg)
        x += tw + PAD * 2 + 14
    return y + TAG_H + 14

def row_width(row):
    return sum(int(d.textlength(l, font=font_tag)) + PAD * 2 + 14 for l, *_ in row) - 14

for name, row in (("tags", tags), ("misc", misc)):
    w = row_width(row)
    print(f"  {name} row: {w}px of {W - 68}px available" + ("  *** OVERFLOW ***" if w > W - 68 else ""))

y = draw_tags(tags, 424)
draw_tags(misc, y)

# ── Bottom bar ──
d.rectangle([0, H - 60, W, H], fill=(22, 27, 34))
d.line([0, H - 60, W, H - 60], fill=(37, 99, 235), width=2)
d.text((34, H - 44), "github.com/ugursayar/Nesso_base", font=font_tiny, fill=(110, 118, 129))
label_r = "Arduino · M5Stack"
rw = int(d.textlength(label_r, font=font_tiny))
d.text((W - rw - 34, H - 44), label_r, font=font_tiny, fill=(68, 147, 248))

out = os.path.join(ROOT, "assets", "cover.png")
img.resize((420, 300), Image.LANCZOS).save(out, "PNG")
print(f"Saved {out} (420x300)")
