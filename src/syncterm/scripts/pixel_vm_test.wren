// Pixel API availability checks shared by the menu and picker VM harnesses.

import "syncterm" for PixelBlit, PixelBuffer, PixelColor, PixelMask, Screen

class PixelVmTest {
  static run() {
    var pass = 0
    var fail = 0
    var check = Fn.new {|condition|
      if (condition) {
        pass = pass + 1
      } else {
        fail = fail + 1
      }
    }

    var colour = PixelColor.rgb(0x123456)
    var pixels = PixelBuffer.new(2, 1, colour)
    var mask = PixelMask.new(2, 1, true)
    var blit = PixelBlit.new()
    check.call(pixels.count == 2 && pixels[0] == colour)
    check.call(mask.count == 2 && mask[0] && mask[1])
    check.call(blit.scaleX == 1 && blit.scaleY == 1)
    check.call(Screen.pixelSize == null || Screen.pixelSize.count == 2)
    return [pass, fail]
  }
}

class MenuTest {
  static run() { PixelVmTest.run() }
}
