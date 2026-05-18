# eBanks Machine

ESP32-powered DigiPog / Pog kiosk with keypad control, NFC card support, Formbar transfers, servo-driven dispensing, deposit sensing, and a web debug / OTA page.

---

## 1. What you need

### Software

- Arduino IDE 2.x
- ESP32 board support package
- USB serial driver for your ESP32 board
- This project folder with `eBankMachine.ino` and the `.cpp/.h` files in the same sketch folder

### Arduino libraries

Install these from the Arduino Library Manager unless you already have them:

- `ArduinoJson`
- `LiquidCrystal_I2C`
- `Keypad`
- `ESP32Servo`
- `Adafruit PN532`
- `Crypto`

The ESP32 core provides these automatically:

- `WiFi`
- `WebServer`
- `ESPmDNS`
- `Update`
- `HTTPClient`
- `WiFiClientSecure`
- `Wire`

---

## 2. USB driver setup

Your computer needs the correct USB-to-serial driver before the ESP32 appears as a port.

### How to tell which driver you need

Look at the small USB chip on the ESP32 board, near the USB port.

Common chips:

| Chip on board | Driver to install |
|---|---|
| CH340 / CH341 | CH340 driver |
| CP2102 / CP210x | Silicon Labs CP210x driver | `The one I used `
| FT232 / FTDI | FTDI VCP driver |
| Native USB ESP32-S2 / S3 / C3 | Usually no extra driver needed on modern Windows/macOS/Linux |

Most cheap ESP32 DevKit boards use either **CH340** or **CP2102**.

### Windows

1. Plug in the ESP32 with a data USB cable.
2. Open **Device Manager**.
3. Check **Ports (COM & LPT)**.
4. If you see something like `USB-SERIAL CH340`, `CP210x USB to UART Bridge`, or `USB Serial Device`, the driver is working.
5. If it shows up under **Other devices** or has a warning icon, install the matching driver.
6. Unplug and replug the ESP32 after installing the driver.
7. In Arduino IDE, go to **Tools > Port** and select the new COM port.

### macOS

1. Plug in the ESP32 with a data USB cable.
2. In Arduino IDE, check **Tools > Port**.
3. Look for a port like:
   - `/dev/cu.usbserial-*`
   - `/dev/cu.SLAB_USBtoUART`
   - `/dev/cu.wchusbserial*`
4. If no port appears, install the CH340 or CP210x driver depending on the board.
5. Reboot if macOS still does not show the port.

### Linux / Chromebook Linux container

1. Plug in the ESP32.
2. Run:

```bash
ls /dev/ttyUSB* /dev/ttyACM*
```

3. Common ports are:
   - `/dev/ttyUSB0`
   - `/dev/ttyACM0`

4. If the port exists but Arduino cannot use it, add your user to the serial group:

```bash
sudo usermod -aG dialout $USER
```

5. Log out and back in.

---

## 3. USB cable warning

A lot of USB cables are charge-only. They power the ESP32 but do not carry data.

If the ESP32 lights up but no port appears:

- Try a different USB cable.
- Try a different USB port.
- Avoid USB hubs while troubleshooting.
- Use a cable that you know can transfer files, not just charge.

---

## 4. Arduino IDE board settings

In Arduino IDE:

1. Install ESP32 board support:
   - Open **Boards Manager**.
   - Search `esp32`.
   - Install **esp32 by Espressif Systems**.
2. Select a board:
   - Usually **ESP32 Dev Module** works.
3. Select the port:
   - **Tools > Port > COMx** on Windows
   - **Tools > Port > /dev/cu...** on macOS
   - **Tools > Port > /dev/ttyUSB0** or `/dev/ttyACM0` on Linux
4. Good default upload settings:
   - Upload Speed: `115200` or `921600`
   - CPU Frequency: `240MHz`
   - Flash Mode: `DIO`
   - Partition Scheme: `Default 4MB with spiffs` or normal default

If upload fails at `Connecting...`, hold the ESP32 **BOOT** button until the upload starts, then release it.

---

## 5. First upload

1. Open `eBankMachine.ino` in Arduino IDE.
2. Make sure all project files are in the same sketch folder.
3. Click **Verify**.
4. Fix any missing library errors.
5. Click **Upload**.
6. Open Serial Monitor at `115200 baud`.

After booting, the LCD should show the startup / welcome screens.

---

## 6. Finding the ESP32 on the network

The machine connects to Wi-Fi using the values in `config.cpp`.

Before uploading, set:

```cpp
const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";
```


### Option A: use the keypad

Press **B three times** on the keypad. The LCD should show the ESP32 IP address if Wi-Fi is connected.

Then open:

```text
http://ESP_IP_ADDRESS/
http://ESP_IP_ADDRESS/debug
```

Example:

```text
http://192.168.1.50/debug
```

### Option B: use Serial Monitor

Open Serial Monitor at `115200 baud` and reset the ESP32.

Look for lines like:

```text
HTTP: http://192.168.x.x/
Debug: http://192.168.x.x/debug
```

### Option C: try mDNS

The project sets an OTA host name. Try:

```text
http://digipog-kiosk.local/
http://digipog-kiosk.local/debug
```

This may not work on every school network or Windows setup. The raw IP address is more reliable.

---

## 7. Web pages

### Main OTA page

```text
http://ESP_IP_ADDRESS/
```

Default login page:

```text
username: admin
password: admin
```

The OTA upload form then asks for the OTA password from `config.cpp`.

### Debug page

```text
http://ESP_IP_ADDRESS/debug
```

The debug page shows:

- current mode
- motion state
- Wi-Fi status
- inventory count
- limit switch status
- live debug log
- testing controls
- servo test buttons
- IR scan tools
- NFC scan tools
- reboot button

Warning: in the current code, `/debug` is not protected by a login. Anyone on the same network who knows the IP can open it.

---

## 8. OTA update flow

After the first USB upload, you can upload newer firmware through the browser.

1. Build/export the compiled binary from Arduino IDE.
2. Open:

```text
http://ESP_IP_ADDRESS/
```

3. Log in.
4. Choose the `.bin` file.
5. Enter the OTA password.
6. Upload.
7. The ESP32 reboots after a successful update.

If OTA fails, go back to USB upload.

---

## 9. Troubleshooting: computer cannot find ESP32

### No port appears at all

Most likely causes:

- charge-only USB cable
- missing CH340 / CP210x driver
- bad USB port
- bad board

Fix:

1. Try another USB cable.
2. Try another USB port.
3. Install the correct USB serial driver.
4. Replug the board.
5. Check Device Manager / Arduino Port menu again.

### Port appears, but upload fails

Try:

- Hold **BOOT** while uploading.
- Press **EN / RESET** once when Arduino says `Connecting...`.
- Lower upload speed to `115200`.
- Close Serial Monitor before uploading.
- Make sure no other program is using the COM port.

### Upload works, but Wi-Fi page does not load

Check:

- Wi-Fi SSID and password in `config.cpp`.
- ESP32 and computer are on the same network.
- School networks may block device-to-device traffic.
- Use the IP from the LCD or Serial Monitor instead of `.local`.

### `.local` address does not work

Use the IP address instead:

```text
http://ESP_IP_ADDRESS/debug
```

mDNS is convenient, but it is not guaranteed on every network.

### ESP32 keeps rebooting

Check:

- power supply is stable
- servo is not pulling too much current from the ESP32
- wiring shorts
- Serial Monitor crash messages
- correct board selected in Arduino IDE

---

## 10. Important security notes

This project contains Wi-Fi credentials, API keys, kiosk account info, and OTA credentials in code.

Before sharing publicly:

- remove real Wi-Fi passwords
- rotate exposed API keys
- remove real kiosk PINs
- move secrets into a separate ignored config file
- protect `/debug`
- change the default web login

Recommended repo setup:

```text
config.example.cpp   safe placeholder config
config.cpp           real private config, ignored by git
.gitignore           includes config.cpp
```

Example `.gitignore` entry:

```gitignore
config.cpp
*.bin
*.elf
*.map
```

---

## 11. Basic mode controls

From the main menu:

| Key | Mode |
|---|---|
| A | DigiPogs to Pogs / withdraw |
| B | Pogs to DigiPogs / deposit |
| C | Student-to-student transfer |
| D | NFC card write |
| B x3 | Show IP address |

Inside most flows:

| Key | Action |
|---|---|
| `#` | confirm / next |
| `*` | cancel / clear / back |

---

## 12. Current known notes

- `/debug` is powerful and currently not protected.
- First upload must be done over USB.
- OTA only works after Wi-Fi connects.
- If the ESP32 cannot be found, fix USB drivers/cable before debugging the code.
- If the network blocks local devices, the web UI may not load even though Wi-Fi connects.
