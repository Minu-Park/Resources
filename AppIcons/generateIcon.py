import os
import sys
import shutil
from PIL import Image

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 generateIcon.py <IMAGEPATH>")
        sys.exit(1)
        
    image_path = sys.argv[1]
    if not os.path.exists(image_path):
        print(f"Error: Source image not found at '{image_path}'")
        sys.exit(1)
        
    script_dir = os.path.dirname(os.path.abspath(__file__))
    png_path = os.path.join(script_dir, "AppIcon.png")
    ico_path = os.path.join(script_dir, "AppIcon.ico")
    icns_path = os.path.join(script_dir, "AppIcon.icns")
    
    print(f"Opening source image: {image_path}")
    img = Image.open(image_path)
    
    # Save the original image as AppIcon.png
    print(f"Saving source image as {png_path}...")
    img.save(png_path, format="PNG")
    
    # Generate ICO (Windows: AppIcon.png is full-bleed, perfect for ICO)
    print("Generating AppIcon.ico...")
    ico_sizes = [(16, 16), (32, 32), (48, 48), (256, 256)]
    img.save(ico_path, format="ICO", sizes=ico_sizes)
    print(f"ICO saved to {ico_path}")
    
    # Generate ICNS (macOS: Dynamic padding to create conforming squircle margins from full-bleed PNG)
    print("Generating AppIcon.icns...")
    # Create macOS-conforming 1024x1024 canvas with 10% transparent margins (824x824 active area)
    mac_canvas = Image.new("RGBA", (1024, 1024), (0, 0, 0, 0))
    resized_mac = img.resize((824, 824), Image.Resampling.LANCZOS)
    mac_canvas.paste(resized_mac, (100, 100))
    
    use_iconutil = False
    if sys.platform == "darwin" and shutil.which("iconutil") is not None:
        use_iconutil = True
        
    if use_iconutil:
        print("Using macOS native iconutil to generate conforming ICNS...")
        iconset_dir = os.path.join(script_dir, "AppIcon.iconset")
        os.makedirs(iconset_dir, exist_ok=True)
        
        sizes_dict = {
            "icon_16x16.png": (16, 16),
            "icon_16x16@2x.png": (32, 32),
            "icon_32x32.png": (32, 32),
            "icon_32x32@2x.png": (64, 64),
            "icon_128x128.png": (128, 128),
            "icon_128x128@2x.png": (256, 256),
            "icon_256x256.png": (256, 256),
            "icon_256x256@2x.png": (512, 512),
            "icon_512x512.png": (512, 512),
            "icon_512x512@2x.png": (1024, 1024)
        }
        
        for name, sz in sizes_dict.items():
            resized = mac_canvas.resize(sz, Image.Resampling.LANCZOS)
            resized.save(os.path.join(iconset_dir, name))
            
        # Run iconutil
        ret = os.system(f"iconutil -c icns {iconset_dir}")
        if ret == 0:
            print(f"ICNS successfully generated via iconutil and saved to {icns_path}")
        else:
            print("Warning: iconutil failed, falling back to Pillow direct save.")
            use_iconutil = False
            
        # Clean up iconset
        shutil.rmtree(iconset_dir)

    if not use_iconutil:
        # Fallback to Pillow direct save for non-macOS platforms
        print("Saving ICNS directly using Pillow...")
        icns_sizes = [(16, 16), (32, 32), (64, 64), (128, 128), (256, 256), (512, 512), (1024, 1024)]
        try:
            mac_canvas.save(icns_path, format="ICNS", sizes=icns_sizes)
            print(f"ICNS saved directly to {icns_path}")
        except Exception as e:
            print("Failed to save ICNS directly:", e)
        
    print("Icon generation completed successfully!")

if __name__ == "__main__":
    main()
