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

- Connect shows the browser's device picker, filtered to `led_light` / the light's service.
- On connect it reads the current mode and color from the light.
- Picking a color while in rainbow mode switches the light to fixed (the color isn't shown in rainbow).
- Writes are sent one at a time; while dragging the color picker only the latest color is sent.
- If the connection drops, it retries 3 times, then shows *Reconnect*.
- The protocol is documented in [`../README.md`](../README.md#ble-protocol).
