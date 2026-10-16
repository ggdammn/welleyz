"""Rebuild the Windows app/setup icon from standalone logo.svg.

Requires: python -m pip install resvg-py Pillow
"""
from io import BytesIO
from pathlib import Path

from PIL import Image
from resvg_py import svg_to_bytes


root = Path(__file__).resolve().parent.parent
source = root / "src" / "assets" / "branding" / "standalone logo.svg"
png = root / "src" / "assets" / "standalone-icon.png"
ico = root / "src" / "assets" / "standalone-icon.ico"

icon = Image.open(BytesIO(svg_to_bytes(svg_path=str(source), width=512, height=512)))
icon = icon.convert("RGBA")
icon.save(png)
icon.save(ico, format="ICO", sizes=[(size, size) for size in (16, 24, 32, 48, 64, 128, 256)])
print(f"Generated {png} and {ico}")
