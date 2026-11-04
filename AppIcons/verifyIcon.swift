import AppKit

func fail(_ message: String) -> Never {
    FileHandle.standardError.write(Data((message + "\n").utf8))
    exit(1)
}

func render(_ image: NSImage, size: Int, scale: Int) -> NSBitmapImageRep {
    let bitmap = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: size * scale,
        pixelsHigh: size * scale, bitsPerSample: 8, samplesPerPixel: 4, hasAlpha: true,
        isPlanar: false, colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0)!
    bitmap.size = NSSize(width: size, height: size)
    NSGraphicsContext.saveGraphicsState()
    NSGraphicsContext.current = NSGraphicsContext(bitmapImageRep: bitmap)
    image.draw(in: NSRect(x: 0, y: 0, width: size, height: size),
        from: .zero, operation: .sourceOver, fraction: 1)
    NSGraphicsContext.restoreGraphicsState()
    return bitmap
}

func bounds(_ bitmap: NSBitmapImageRep, alpha: Int) -> [Int] {
    var left = bitmap.pixelsWide, top = bitmap.pixelsHigh, right = 0, bottom = 0
    for y in 0..<bitmap.pixelsHigh {
        for x in 0..<bitmap.pixelsWide {
            if Int((bitmap.colorAt(x: x, y: y)!.alphaComponent * 255).rounded()) >= alpha {
                left = min(left, x); top = min(top, y)
                right = max(right, x + 1); bottom = max(bottom, y + 1)
            }
        }
    }
    return [left, top, right, bottom]
}

let arguments = CommandLine.arguments
if arguments.count < 2 || arguments.count > 3 {
    fail("Usage: xcrun swift verifyIcon.swift <Consumer.app> [capture-directory]")
}
let app = URL(fileURLWithPath: arguments[1]).standardizedFileURL
let resources = app.appendingPathComponent("Contents/Resources")
guard let bundle = Bundle(url: app),
      bundle.object(forInfoDictionaryKey: "CFBundleIconName") as? String == "AppIcon",
      FileManager.default.fileExists(atPath: resources.appendingPathComponent("AppIcon.icns").path),
      FileManager.default.fileExists(atPath: resources.appendingPathComponent("Assets.car").path) else {
    fail("Consumer must select AppIcon and package its ICNS/catalog pair")
}
let capture = arguments.count == 3 ? URL(fileURLWithPath: arguments[2]) : nil
if let capture = capture {
    try FileManager.default.createDirectory(at: capture, withIntermediateDirectories: true)
}
let candidate = NSWorkspace.shared.icon(forFile: app.path)
let reference = NSWorkspace.shared.icon(forFile: "/System/Applications/System Settings.app")
for size in [16, 32, 64, 128] {
    for scale in [1, 2] {
        let actual = render(candidate, size: size, scale: scale)
        let expected = render(reference, size: size, scale: scale)
        let label = "\(size)pt @\(scale)x"
        for threshold in [1, 32, 128, 230] {
            let actualBounds = bounds(actual, alpha: threshold)
            let expectedBounds = bounds(expected, alpha: threshold)
            guard actualBounds == expectedBounds else {
                fail("\(label) alpha \(threshold): \(actualBounds) differs from system \(expectedBounds)")
            }
        }
        // Check the whole opaque contour, not just three points on a dark plate.
        for y in 1..<(actual.pixelsHigh - 1) {
            for x in 1..<(actual.pixelsWide - 1) {
                let color = actual.colorAt(x: x, y: y)!.usingColorSpace(.deviceRGB)!
                let neighbors = [(x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)]
                let isContour = color.alphaComponent >= 230.0 / 255.0 && neighbors.contains {
                    actual.colorAt(x: $0.0, y: $0.1)!.alphaComponent < 230.0 / 255.0
                }
                if isContour && max(color.redComponent, color.greenComponent, color.blueComponent) < 0.7 {
                    fail("\(label): dark discontinuity in white tile contour at (\(x), \(y))")
                }
            }
        }
        if let capture = capture {
            for (name, bitmap) in [("consumer", actual), ("system-settings", expected)] {
                try bitmap.representation(using: .png, properties: [:])!.write(
                    to: capture.appendingPathComponent("\(name)-\(size)-\(scale)x.png"))
            }
        }
        print("\(label): system body/shadow bounds match; white contour has no dark discontinuities")
    }
}
