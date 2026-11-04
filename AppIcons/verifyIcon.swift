import AppKit

func fail(_ message: String) -> Never {
    FileHandle.standardError.write(Data((message + "\n").utf8))
    exit(1)
}

func render(_ image: NSImage, size: Int) -> NSBitmapImageRep {
    let bitmap = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: size,
        pixelsHigh: size, bitsPerSample: 8, samplesPerPixel: 4, hasAlpha: true,
        isPlanar: false, colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0)!
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
    let actual = render(candidate, size: size)
    let expected = render(reference, size: size)
    for threshold in [1, 32, 128, 230] {
        let actualBounds = bounds(actual, alpha: threshold)
        let expectedBounds = bounds(expected, alpha: threshold)
        guard actualBounds == expectedBounds else {
            fail("\(size)px alpha \(threshold): \(actualBounds) differs from system \(expectedBounds)")
        }
    }
    // The supplied white tile must not acquire a second dark plate at small sizes.
    let body = bounds(actual, alpha: 230)
    let middleX = (body[0] + body[2]) / 2, middleY = (body[1] + body[3]) / 2
    for (x, y) in [(body[0] + 1, middleY), (body[2] - 2, middleY), (middleX, body[3] - 2)] {
        let color = actual.colorAt(x: x, y: y)!.usingColorSpace(.deviceRGB)!
        if max(color.redComponent, color.greenComponent, color.blueComponent) < 0.4 {
            fail("\(size)px: unexpected dark plate around the supplied white artwork")
        }
    }
    if let capture = capture {
        for (name, bitmap) in [("consumer", actual), ("system-settings", expected)] {
            try bitmap.representation(using: .png, properties: [:])!.write(
                to: capture.appendingPathComponent("\(name)-\(size).png"))
        }
    }
    print("\(size)px: system body/shadow bounds match at four alpha thresholds; no nested dark plate")
}
