# Adds a `save_screenshot "<path.png>"` monitor command: saves what the
# current machine's LTDC is showing. For human review only -- tests read
# the wind_ui log line, never the pixels.
import System


def mc_save_screenshot(path):
    png = self.Machine["sysbus.ltdc"].TakeScreenshot().ToPng()
    if hasattr(png, "Save"):
        png.Save(path)
    else:
        System.IO.File.WriteAllBytes(path, png.ToArray())
