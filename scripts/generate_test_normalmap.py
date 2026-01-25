#!/usr/bin/env python3
"""
Generate a test normal map with known patterns for debugging normal mapping.

Normal map color encoding:
- RGB (128, 128, 255) = Tangent space (0, 0, 1) = Flat surface
- Red channel: X direction in tangent space (128 = neutral, 255 = right, 0 = left)
- Green channel: Y direction in tangent space (128 = neutral, 255 = up, 0 = down)
- Blue channel: Z direction (always pointing out, typically 200-255)
"""

from PIL import Image, ImageDraw, ImageFont
import numpy as np

# Create 512x512 image
width, height = 512, 512
img = np.zeros((height, width, 3), dtype=np.uint8)

# Helper function to set normal (expects values in -1 to 1 range)
def encode_normal(nx, ny, nz):
	"""Convert normal vector to RGB color (tangent space -> texture space)"""
	# Normalize
	length = np.sqrt(nx*nx + ny*ny + nz*nz)
	if length > 0:
		nx, ny, nz = nx/length, ny/length, nz/length

	# Map from [-1, 1] to [0, 255]
	r = int((nx * 0.5 + 0.5) * 255)
	g = int((ny * 0.5 + 0.5) * 255)
	b = int((nz * 0.5 + 0.5) * 255)
	return (r, g, b)

# Region 1: Top-left - FLAT SURFACE (reference)
# Should appear as base purple/blue
flat_normal = encode_normal(0, 0, 1)
img[0:170, 0:170] = flat_normal

# Region 2: Top-center - BUMP UP (positive Y in tangent space)
# Should light differently when light moves vertically
bump_up = encode_normal(0, 0.3, 1)
img[0:170, 171:341] = bump_up

# Region 3: Top-right - BUMP DOWN (negative Y in tangent space)
bump_down = encode_normal(0, -0.3, 1)
img[0:170, 342:512] = bump_down

# Region 4: Middle-left - BUMP RIGHT (positive X in tangent space)
# Should light differently when light moves horizontally
bump_right = encode_normal(0.3, 0, 1)
img[171:341, 0:170] = bump_right

# Region 5: Middle-center - GRADIENT TEST
# Creates a smooth bump in the center
for y in range(171, 341):
	for x in range(171, 341):
		# Create radial bump
		cx, cy = 256, 256
		dx = (x - cx) / 85.0  # Normalized distance
		dy = (y - cy) / 85.0
		dist = np.sqrt(dx*dx + dy*dy)

		if dist < 1.0:
			# Smooth bump with falloff
			height = (1.0 - dist) * 0.5
			nx = -dx * height * 2
			ny = -dy * height * 2
			nz = 1.0
			img[y, x] = encode_normal(nx, ny, nz)
		else:
			img[y, x] = flat_normal

# Region 6: Middle-right - BUMP LEFT (negative X in tangent space)
bump_left = encode_normal(-0.3, 0, 1)
img[171:341, 342:512] = bump_left

# Region 7: Bottom-left - DIAGONAL BUMP (northeast)
bump_ne = encode_normal(0.3, 0.3, 1)
img[342:512, 0:170] = bump_ne

# Region 8: Bottom-center - DIAGONAL BUMP (northwest)
bump_nw = encode_normal(-0.3, 0.3, 1)
img[342:512, 171:341] = bump_nw

# Region 9: Bottom-right - STRONG BUMP (tests normalization)
# Higher slope to verify proper normalization
bump_strong = encode_normal(0.5, 0.5, 1)
img[342:512, 342:512] = bump_strong

# Save the image
pil_img = Image.fromarray(img, 'RGB')

# Add labels (optional, just for reference when viewing the texture file)
draw = ImageDraw.Draw(pil_img)
# Try to use a basic font
try:
	font = ImageFont.truetype("/System/Library/Fonts/Helvetica.ttc", 16)
except:
	font = ImageFont.load_default()

labels = [
	(20, 80, "FLAT\n(0,0,1)"),
	(190, 80, "UP\n(0,+Y,1)"),
	(360, 80, "DOWN\n(0,-Y,1)"),
	(20, 250, "RIGHT\n(+X,0,1)"),
	(190, 250, "RADIAL\nBUMP"),
	(360, 250, "LEFT\n(-X,0,1)"),
	(20, 420, "NE\n(+X,+Y,1)"),
	(190, 420, "NW\n(-X,+Y,1)"),
	(360, 420, "STRONG\n(0.5,0.5,1)"),
]

for x, y, text in labels:
	# Draw text with outline for visibility
	draw.text((x, y), text, fill=(255, 255, 255), font=font, stroke_width=1, stroke_fill=(0, 0, 0))

# Save
output_path = "resources/textures/test_normalmap.png"
pil_img.save(output_path)
print(f"✓ Generated test normal map: {output_path}")
print(f"  Size: {width}x{height}")
print(f"  9 distinct test regions with known normal directions")
print(f"\nRegion guide:")
print(f"  Top row:    Flat | Up bump | Down bump")
print(f"  Middle row: Right | Radial | Left")
print(f"  Bottom row: NE | NW | Strong")
print(f"\nUsage: Apply this texture to your cube and use debug modes F1-F10 to verify")