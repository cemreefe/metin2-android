#!/usr/bin/env python3
"""Build the touch HUD's textures into <data>/mobile/ (uncompressed 32-bit TGA).

The button frame is the minimap's steel ring from the game's own minimap.dds: its clean
lower-left quadrant is mirrored into a full, symmetric ring and filled with the dark stone of
the stock slot background, so the HUD reuses the client's art instead of drawing its own.
Needs Pillow (with DDS support) and a staged client data directory.
"""
import math, os, sys
from PIL import Image, ImageChops, ImageDraw, ImageFilter

SIZE = 128
SS = 4  # supersampling for procedural shapes

def ring(data):
	src = Image.open(os.path.join(data, "ymir work/ui/minimap.dds")).convert("RGBA").crop((0, 0, 136, 137))
	cx, cy = 68, 68
	quad = src.crop((0, cy, cx, cy + cx))
	full = Image.new("RGBA", (2 * cx, 2 * cx))
	full.paste(quad, (0, cx))
	full.paste(quad.transpose(Image.FLIP_LEFT_RIGHT), (cx, cx))
	full.paste(quad.transpose(Image.FLIP_TOP_BOTTOM), (0, 0))
	full.paste(quad.transpose(Image.ROTATE_180), (cx, 0))
	return full.resize((SIZE, SIZE), Image.LANCZOS)

def disc_mask(size, frac, blur = 0.0):
	big = Image.new("L", (size * SS, size * SS), 0)
	r = size * SS * frac / 2.0
	c = size * SS / 2.0
	ImageDraw.Draw(big).ellipse((c - r, c - r, c + r, c + r), fill = 255)
	m = big.resize((size, size), Image.LANCZOS)
	return m.filter(ImageFilter.GaussianBlur(blur)) if blur else m

def radial(size, f):
	m = Image.new("L", (size, size))
	c = (size - 1) / 2.0
	m.putdata([int(255 * max(0.0, min(1.0, f(math.hypot(x - c, y - c) / c))))
			for y in range(size) for x in range(size)])
	return m

def stone(data):
	pub = Image.open(os.path.join(data, "ymir work/ui/public.dds")).convert("RGBA")
	tile = pub.crop((2, 350, 30, 378)).resize((SIZE // 2, SIZE // 2), Image.BICUBIC)
	out = Image.new("RGBA", (SIZE, SIZE))
	for y in (0, SIZE // 2):
		for x in (0, SIZE // 2):
			out.paste(tile, (x, y))
	# inner shadow: darker towards the rim, lit slightly from the top
	shade = radial(SIZE, lambda d: 0.55 + 0.45 * d * d)
	black = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 255))
	out = Image.composite(black, out, shade.point(lambda v: int(v * 0.85)))
	hi = Image.new("RGBA", (SIZE, SIZE), (255, 236, 200, 255))
	top = Image.new("L", (SIZE, SIZE))
	top.putdata([int(28 * max(0.0, 1.0 - y / (SIZE * 0.55))) for y in range(SIZE) for x in range(SIZE)])
	return Image.composite(hi, out, top)

def button(data):
	body = stone(data)
	body.putalpha(ImageChops.multiply(disc_mask(SIZE, 0.86), Image.new("L", (SIZE, SIZE), 225)))
	out = Image.new("RGBA", (SIZE, SIZE))
	shadow = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
	shadow.putalpha(disc_mask(SIZE, 0.98, 3).point(lambda v: v * 2 // 5))
	out.alpha_composite(shadow)
	out.alpha_composite(body)
	out.alpha_composite(ring(data))
	return out

def cooldown():
	out = Image.new("RGBA", (SIZE, SIZE), (8, 6, 4, 0))
	out.putalpha(disc_mask(SIZE, 0.84).point(lambda v: v * 3 // 5))
	return out

def active():
	# warm light pooled inside the rim, no hard edge
	out = Image.new("RGBA", (SIZE, SIZE), (255, 196, 110, 0))
	out.putalpha(radial(SIZE, lambda d: 0.6 * max(0.0, 1.0 - abs(d - 0.74) / 0.16) ** 2))
	return out

def pill(w, h):
	big = Image.new("RGBA", (w * SS, h * SS))
	d = ImageDraw.Draw(big)
	d.rounded_rectangle((0, 0, w * SS - 1, h * SS - 1), radius = h * SS // 2, fill = (14, 10, 8, 200),
			outline = (120, 98, 70, 220), width = SS)
	return big.resize((w, h), Image.LANCZOS)

def save(img, path):
	w, h = img.size
	with open(path, "wb") as f:
		f.write(bytes([0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, w & 255, w >> 8, h & 255, h >> 8, 32, 0x28]))
		r, g, b, a = img.split()
		f.write(Image.merge("RGBA", (b, g, r, a)).tobytes())

data = sys.argv[1]
out = os.path.join(data, "mobile")
os.makedirs(out, exist_ok = True)
save(button(data), os.path.join(out, "button.tga"))
save(cooldown(), os.path.join(out, "cooldown.tga"))
save(active(), os.path.join(out, "active.tga"))
save(pill(64, 20), os.path.join(out, "label.tga"))
