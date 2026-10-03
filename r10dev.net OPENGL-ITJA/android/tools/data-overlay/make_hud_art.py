#!/usr/bin/env python3
"""Write the touch HUD's procedural textures (uncompressed 32-bit TGA) into <out>/mobile/."""
import math, os, struct, sys

def tga(path, size, shade):
	px = bytearray()
	c = (size - 1) / 2.0
	for y in range(size):
		for x in range(size):
			d = math.hypot(x - c, y - c) / c
			px += bytes(shade(d))
	with open(path, "wb") as f:
		f.write(struct.pack("<BBBHHBHHHHBB", 0, 0, 2, 0, 0, 0, 0, 0, size, size, 32, 0x28))
		f.write(px)

def aa(d, r):
	return max(0.0, min(1.0, (r - d) * 40.0))

def button(d):
	body = aa(d, 0.92)
	rim = aa(d, 1.0) - aa(d, 0.86)
	a = max(body * 0.55, rim * 0.95)
	if rim > body * 0.55:
		return (60, 180, 230, int(255 * a))  # BGRA gold rim
	return (40, 30, 25, int(255 * a))

def disk(d):
	return (0, 0, 0, int(255 * 0.6 * aa(d, 0.92)))

def glow(d):
	return (90, 220, 255, int(255 * 0.9 * (aa(d, 1.0) - aa(d, 0.84))))

out = os.path.join(sys.argv[1], "mobile")
os.makedirs(out, exist_ok=True)
tga(os.path.join(out, "button.tga"), 128, button)
tga(os.path.join(out, "cooldown.tga"), 128, disk)
tga(os.path.join(out, "active.tga"), 128, glow)
