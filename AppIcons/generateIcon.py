"""Generate consumer app icons; macOS assets use Apple's native icon compiler."""

import argparse
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", nargs="?", type=Path)
    parser.add_argument("--macos-only", action="store_true",
                        help="Rebuild native macOS assets from the existing AppIcon.png")
    args = parser.parse_args()
    directory = Path(__file__).resolve().parent
    source = directory / "AppIcon.png"
    if args.macos_only:
        if args.image is not None or sys.platform != "darwin":
            parser.error("--macos-only requires macOS and no image argument")
    else:
        if args.image is None:
            parser.error("an image argument or --macos-only is required")
        from PIL import Image
        with Image.open(args.image) as original:
            image = original.copy()
        image.save(source, format="PNG")
        image.save(directory / "AppIcon.ico", format="ICO",
                   sizes=[(16, 16), (32, 32), (48, 48), (256, 256)])

    if sys.platform != "darwin":
        print("PNG and ICO generated. Rebuild macOS assets on macOS before publication.")
        return

    # Keep the supplied artwork; Apple owns the native tile, size and shadow.
    document = directory / "AppIcon.icon"
    shutil.copyfile(source, document / "Assets" / "Artwork.png")
    with tempfile.TemporaryDirectory(prefix="resources-app-icon-") as temporary:
        output = Path(temporary)
        subprocess.run([
            "xcrun", "actool", str(document), "--compile", str(output),
            "--platform", "macosx", "--minimum-deployment-target", "11.0",
            "--app-icon", "AppIcon", "--target-device", "mac",
            "--output-partial-info-plist", str(output / "Info.plist"),
            "--output-format", "human-readable-text",
        ], check=True)
        for name in ("AppIcon.icns", "Assets.car"):
            shutil.copyfile(output / name, directory / name)
    print("Generated AppIcon.icns and Assets.car. Package both with CFBundleIconName=AppIcon.")


if __name__ == "__main__":
    main()
