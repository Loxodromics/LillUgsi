#!/usr/bin/env python3
"""
Generate a simple subsurface scattering (SSS) lookup texture.

This script creates a 2D lookup texture that pre-computes subsurface scattering
diffuse response for different light angles and surface curvatures.

The LUT uses a "wrapped lighting" approximation:
- X-axis (U): Wrapped NdotL (light angle, 0=shadowed, 1=directly lit)
- Y-axis (V): Curvature (surface bending, 0=flat, 1=highly curved)
- RGB: Diffuse response with subsurface scattering baked in

Usage:
    python generate_sss_lut.py [--size 256] [--output sss_lut.png]
"""

import numpy as np
from PIL import Image
import argparse


def generate_sss_lut(size=256, scatter_strength=0.5, falloff_power=0.3):
    """
    Generate a simple SSS lookup texture using wrapped lighting.

    Args:
        size: Texture resolution (size x size pixels)
        scatter_strength: How much light wraps into shadow (0.0-1.0)
            0.0 = no wrapping (standard Lambert)
            1.0 = maximum wrapping (full subsurface transmission)
        falloff_power: How much curvature affects the falloff curve (0.0-1.0)
            0.0 = no curvature effect
            1.0 = strong curvature effect

    Returns:
        numpy array of shape (size, size, 3) with RGB values

    How it works:

    1. Wrapped Lighting:
       Standard Lambert: diffuse = max(0, NdotL)
       Wrapped Lambert: diffuse = max(0, (NdotL - (1-wrap)) / wrap)

       The "wrap" parameter shifts the Lambert cutoff into negative angles,
       simulating light that enters the surface, scatters, and exits elsewhere.

    2. Curvature Dependency:
       On flat surfaces: minimal wrapping (some scattering)
       On curved surfaces: maximum wrapping (lots of scattering)

       This is physically motivated: on a curved edge, light can enter one side
       and exit the other after scattering through the material.

    3. Falloff Adjustment:
       A power function softens the gradient for curved surfaces,
       creating a more gradual transition from light to shadow.
    """
    lut = np.zeros((size, size, 3), dtype=np.uint8)

    for y in range(size):
        for x in range(size):
            # X-axis: wrapped NdotL (0 to 1)
            # Represents the light angle: 0 = back-facing, 1 = front-facing
            ndotl = x / (size - 1.0)

            # Y-axis: curvature (0 to 1)
            # Represents surface bending: 0 = flat, 1 = highly curved
            curvature = y / (size - 1.0)

            # Calculate wrap amount based on curvature and scatter strength
            # Flat surfaces get minimal wrapping
            # Curved surfaces get maximum wrapping
            wrap = (1.0 - scatter_strength) + curvature * scatter_strength

            # Apply wrapped lighting formula
            # This shifts the Lambert cutoff to allow light in shadow regions
            diffuse = max(0.0, (ndotl - (1.0 - wrap)) / wrap)

            # Apply power function to soften falloff on curved surfaces
            # Higher curvature = softer gradient (more subsurface scattering)
            power = 1.0 - curvature * falloff_power
            diffuse = pow(diffuse, power)

            # Convert to 8-bit RGB (grayscale for diffuse response)
            value = int(min(1.0, diffuse) * 255)
            lut[y, x] = [value, value, value]

    return lut


def visualize_lut_structure(size=256):
    """
    Generate a visualization showing what each axis represents.
    This helps understand the LUT structure.
    """
    vis = np.zeros((size * 3, size * 3, 3), dtype=np.uint8)

    # Create gradient visualizations
    for y in range(size):
        for x in range(size):
            # Top-left: NdotL gradient (X-axis)
            vis[y, x] = [int(x / (size - 1.0) * 255)] * 3

            # Top-middle: Curvature gradient (Y-axis)
            vis[y, x + size] = [int(y / (size - 1.0) * 255)] * 3

            # Top-right: Actual LUT
            # (will be filled by main LUT)

    return vis


def add_labels_to_visualization(img, lut):
    """
    Add text labels to help understand the LUT.
    Note: Requires PIL/Pillow with text rendering support.
    """
    from PIL import ImageDraw, ImageFont

    draw = ImageDraw.Draw(img)

    # Try to use a nice font, fallback to default if not available
    try:
        font = ImageFont.truetype("/System/Library/Fonts/Helvetica.ttc", 20)
    except:
        font = ImageFont.load_default()

    # Add axis labels
    height, width = lut.shape[:2]
    draw.text((width // 2, 10), "Light Angle (NdotL) →", fill=(255, 255, 255), font=font)
    draw.text((10, height // 2), "Curvature ↓", fill=(255, 255, 255), font=font)

    return img


def main():
    parser = argparse.ArgumentParser(
        description="Generate subsurface scattering lookup texture",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Generate default 256x256 LUT
  python generate_sss_lut.py

  # Generate high-resolution LUT with more scattering
  python generate_sss_lut.py --size 512 --scatter 0.7

  # Generate LUT with visualization guide
  python generate_sss_lut.py --visualize
        """
    )

    parser.add_argument(
        '--size',
        type=int,
        default=256,
        help='Texture resolution (default: 256x256)'
    )

    parser.add_argument(
        '--output',
        type=str,
        default='resources/textures/sss_lut.png',
        help='Output file path (default: resources/textures/sss_lut.png)'
    )

    parser.add_argument(
        '--scatter',
        type=float,
        default=0.5,
        help='Scatter strength 0.0-1.0 (default: 0.5)'
    )

    parser.add_argument(
        '--falloff',
        type=float,
        default=0.3,
        help='Falloff power 0.0-1.0 (default: 0.3)'
    )

    parser.add_argument(
        '--visualize',
        action='store_true',
        help='Generate visualization showing LUT structure'
    )

    args = parser.parse_args()

    # Validate parameters
    if args.size < 16 or args.size > 2048:
        print(f"Error: Size must be between 16 and 2048")
        return 1

    if args.scatter < 0.0 or args.scatter > 1.0:
        print(f"Error: Scatter strength must be between 0.0 and 1.0")
        return 1

    if args.falloff < 0.0 or args.falloff > 1.0:
        print(f"Error: Falloff power must be between 0.0 and 1.0")
        return 1

    # Generate LUT
    print(f"Generating {args.size}x{args.size} SSS LUT...")
    print(f"  Scatter strength: {args.scatter}")
    print(f"  Falloff power: {args.falloff}")

    lut = generate_sss_lut(args.size, args.scatter, args.falloff)

    # Create image
    img = Image.fromarray(lut)

    # Save
    img.save(args.output)
    print(f"✓ Saved to: {args.output}")

    # Generate visualization if requested
    if args.visualize:
        vis_path = args.output.replace('.png', '_visualization.png')

        # Create 3x side-by-side visualization
        vis_size = args.size
        vis = np.zeros((vis_size, vis_size * 3, 3), dtype=np.uint8)

        for y in range(vis_size):
            for x in range(vis_size):
                # Left: NdotL gradient (what X-axis represents)
                vis[y, x] = [int(x / (vis_size - 1.0) * 255)] * 3

                # Middle: Curvature gradient (what Y-axis represents)
                vis[y, x + vis_size] = [int(y / (vis_size - 1.0) * 255)] * 3

                # Right: Actual LUT
                vis[y, x + vis_size * 2] = lut[y, x]

        vis_img = Image.fromarray(vis)
        vis_img.save(vis_path)
        print(f"✓ Saved visualization to: {vis_path}")
        print(f"  Left: Light angle gradient (X-axis: 0=shadow, 1=light)")
        print(f"  Middle: Curvature gradient (Y-axis: 0=flat, 1=curved)")
        print(f"  Right: Final SSS LUT")

    print("\nTo use this texture in the renderer:")
    print(f"  auto sssLUT = textureManager->getOrLoadTexture(")
    print(f"      \"{args.output}\",")
    print(f"      false,  // No mipmaps needed for LUT")
    print(f"      TextureLoader::Format::RGB")
    print(f"  );")
    print(f"  snowMaterial->setSSSLUT(sssLUT);")

    return 0


if __name__ == '__main__':
    exit(main())
