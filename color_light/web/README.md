# Color Light web app

A static page that controls the light over Bluetooth (Web Bluetooth). There's nothing to build or
install: it's plain HTML/JS.

| Platform | Browser |
|---|---|
| Android | Chrome |
| iPhone / iPad | [Bluefy](https://apps.apple.com/app/bluefy-web-ble-browser/id1492822055) (free). Safari and every other iOS browser lack Web Bluetooth. |
| Desktop | Chrome or Edge (Windows, macOS, ChromeOS) |
| Linux | Chrome, after enabling `chrome://flags/#enable-experimental-web-platform-features` and relaunching. Without it `navigator.bluetooth` doesn't exist. |

On Android, use Chrome's menu → *Add to Home screen* to open it like an app.

## Hosting

Web Bluetooth only works on `https://` pages (or `http://localhost`). With GitHub Pages:
*Settings → Pages → Deploy from a branch → `main` / `(root)`*, then open
`https://<user>.github.io/<repo>/color_light/web/`.

Local test on a computer with Bluetooth:

```sh
python3 -m http.server -d color_light/web 8000   # then open http://localhost:8000 in Chrome
```

## Behavior

- Connect shows the browser's device picker, filtered to names starting with `led_light` / the
  light's service. The title then shows which figure is connected (`led_light_duck` → *Duck light*).
- On connect it reads the current settings from the light.
- Modes: rainbow, fixed, pulsating, candle, fade and twinkle. In fade mode the color card has
  *First* / *Second* buttons to choose which of the two colors the swatches and picker change.
- Picking a color while in rainbow mode switches the light to fixed (the color isn't shown in rainbow).
- Sliders: brightness (always), speed and rainbow width (rainbow mode), speed (pulsating, fade
  and twinkle modes).
  Speed goes from 0.25× to 4× on a log scale. The brightness slider is quadratic so it has finer
  steps when dim. 100 % brightness is the safe maximum for the figure (see the firmware README).
- Lights with older firmware don't have these settings: the sliders and the candle, fade and
  twinkle modes are hidden and the rest works.
- Writes are sent one at a time; while dragging the color picker or a slider only the latest
  value is sent.
- The light drops connections after 2 minutes without changes (so a forgotten tab can't lock other
  phones out). The app then shows *Standby* and keeps its controls: the next change, or coming
  back to the page, reconnects and sends it. It never reconnects on its own otherwise, so a page
  left open doesn't grab the light again. If the reconnect fails it shows *Reconnect*.
- The protocol is documented in [`../README.md`](../README.md#ble-protocol).
