import CoreGraphics
import Foundation

let options = CGWindowListOption.optionOnScreenOnly.union(.excludeDesktopElements)
let raw = CGWindowListCopyWindowInfo(options, kCGNullWindowID)! as NSArray
var matches = 0
for case let window as NSDictionary in raw {
    let owner = window.object(forKey: kCGWindowOwnerName) as? String ?? ""
    let title = window.object(forKey: kCGWindowName) as? String ?? ""
    let layer = window.object(forKey: kCGWindowLayer) as? Int ?? -1
    let id = window.object(forKey: kCGWindowNumber) as? Int ?? 0
    if owner == "AeroSim" && layer == 0 && title.contains("AeroSim") {
        matches += 1
        print("\(id)|\(owner)|\(title)|\(layer)")
    }
}
if matches != 1 {
    fputs("expected exactly one AeroSim layer-0 window, found \(matches)\n", stderr)
    exit(2)
}
