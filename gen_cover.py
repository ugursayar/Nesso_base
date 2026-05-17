from PIL import Image, ImageDraw, ImageFont

W, H = 420, 300

# Base layer
img = Image.new("RGB", (W, H), (13, 17, 23))
d = ImageDraw.Draw(img)

# Top accent bar
d.rectangle([0, 0, W, 5], fill=(37, 99, 235))

# ── Fonts ──
def try_font(path, size):
    try:
        return ImageFont.truetype(path, size)
    except Exception:
        return ImageFont.load_default()

font_big   = try_font("C:/Windows/Fonts/arialbd.ttf", 38)
font_med   = try_font("C:/Windows/Fonts/arialbd.ttf", 16)
font_small = try_font("C:/Windows/Fonts/arial.ttf",   13)
font_tag   = try_font("C:/Windows/Fonts/arialbd.ttf", 12)
font_tiny  = try_font("C:/Windows/Fonts/arial.ttf",   11)

# ── Title ──
d.text((20, 18),  "Nesso", font=font_big, fill=(241, 245, 249))
d.text((20, 58),  "base",  font=font_big, fill=(37, 99, 235))
d.text((20, 104), "Arduino Nesso N1  ·  ESP32-C6", font=font_small, fill=(100, 116, 139))
d.text((20, 121), "v1.0.0", font=font_small, fill=(71, 85, 105))

# ── Device illustration ──
cx, cy = 340, 105
bw, bh = 58, 92
# Body
d.rounded_rectangle(
    [cx - bw//2, cy - bh//2, cx + bw//2, cy + bh//2],
    radius=9, fill=(30, 41, 59), outline=(37, 99, 235), width=2)
# Screen bezel
d.rounded_rectangle(
    [cx - bw//2 + 5, cy - bh//2 + 7, cx + bw//2 - 5, cy + 14],
    radius=3, fill=(13, 17, 23), outline=(59, 130, 246), width=1)
# Screen content lines
for i in range(3):
    lx = cx - 15
    ly = cy - bh//2 + 16 + i * 9
    shade = (59, 130, 246) if i == 0 else (30, 58, 138)
    d.line([lx, ly, lx + 30, ly], fill=shade, width=2)
# Two buttons
d.ellipse([cx - 20, cy + 22, cx - 9, cy + 33], fill=(51, 65, 85))
d.ellipse([cx + 9,  cy + 22, cx + 20, cy + 33], fill=(51, 65, 85))
# USB-C port
d.rounded_rectangle([cx - 8, cy + bh//2 - 8, cx + 8, cy + bh//2 - 2],
                    radius=2, fill=(51, 65, 85))

# ── Divider ──
d.line([20, 148, W - 20, 148], fill=(30, 41, 59), width=1)

# ── Feature tags ──
# (label, bg_rgb, border_rgb, text_rgb)
tags = [
    ("WiFi",      (14, 71, 120),  (14, 165, 233), (224, 242, 254)),
    ("BLE",       (76, 29, 149),  (139, 92, 246), (237, 233, 254)),
    ("LoRa",      (6, 78, 59),    (16, 185, 129), (209, 250, 229)),
    ("IR",        (120, 53, 15),  (245, 158, 11), (254, 243, 199)),
    ("RF433",     (127, 29, 29),  (239, 68, 68),  (254, 226, 226)),
    ("RFID",      (49, 46, 129),  (99, 102, 241), (224, 231, 255)),
]
misc = [
    ("Web FM",    (30, 41, 59),   (71, 85, 105),  (148, 163, 184)),
    ("Serial CLI",(30, 41, 59),   (71, 85, 105),  (148, 163, 184)),
    ("UDP Robot", (30, 41, 59),   (71, 85, 105),  (148, 163, 184)),
]

TAG_H = 22
PAD   = 9

def draw_tags(row, y):
    x = 20
    for label, bg, border, fg in row:
        tw = int(d.textlength(label, font=font_tag))
        bw2 = tw + PAD * 2
        d.rounded_rectangle([x, y, x + bw2, y + TAG_H],
                            radius=4, fill=bg, outline=border, width=1)
        d.text((x + PAD, y + (TAG_H - 12) // 2), label, font=font_tag, fill=fg)
        x += bw2 + 7
    return y + TAG_H + 7

y = 158
y = draw_tags(tags, y)
draw_tags(misc, y)

# ── Bottom bar ──
d.rectangle([0, H - 34, W, H], fill=(22, 27, 34))
d.line([0, H - 34, W, H - 34], fill=(37, 99, 235), width=1)
d.text((20, H - 23), "github.com/ugursayar/Nesso_base", font=font_tiny, fill=(71, 85, 105))
label_r = "Arduino · M5Stack"
rw = int(d.textlength(label_r, font=font_tiny))
d.text((W - rw - 16, H - 23), label_r, font=font_tiny, fill=(37, 99, 235))

out = "d:/Arduino/Nesso_base/cover.png"
img.save(out, "PNG")
print(f"Saved {out}  ({W}x{H}px)")
