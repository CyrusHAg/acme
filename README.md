# ACME — Embedded build targets

Generated Make targets for Arduino, ESP-IDF, and CH32 projects in reproducible
Nix environments with pinned host tools.

Each `project.yaml` selects a platform. ACME generates the corresponding
`build-<platform>-<name>`, `flash-<platform>-<name>`, and
`monitor-<platform>-<name>` targets.

---

## Requirements

- [Nix](https://nixos.org/download) — provides the host tools and isolated shells

Arduino cores, ESP-IDF and its target toolchains, the CH32 RISC-V compiler, and
ch32fun are installed under `bin/` automatically on first use. ESP-IDF and
CH32 use versioned directories; Arduino CLI manages a shared core/library
installation under `bin/`.

## Host environment and dependency versions

All four shells use the same Nixpkgs commit and content hash from
`nix/nixpkgs.json`, through `nix/pkgs.nix`. They do not use the machine's Nix
channel or its Nixpkgs overlays/configuration. The current pin is from the
26.05 stable line and selects Python 3.11 and Arduino CLI 1.4.1. It supports
Linux, Apple Silicon macOS, and Intel macOS; each machine gets native host
tools from the same package definitions. No flakes setup is required.

Check actual shell creation and required Python imports on each computer:

```sh
bash tools/check-shells.sh
```

This checks the host environments; representative firmware builds are still
needed when adopting new host or firmware dependencies.

The pin covers host tools and their Python dependencies, not everything
downloaded by the firmware tools:

- Arduino CLI continues to install cores and libraries. Cores and libraries
  without a version select the latest available release on installation, so
  a fresh clone can differ from an existing installation. Libraries can be
  pinned with `Name@version` in `project.yaml`.
- ESP-IDF defaults to `v6.0.3`. Use a release tag or commit in `version` rather
  than a moving branch when stable firmware dependencies matter. Its own
  installer manages its Python environment and target tools.
- CH32 defaults to a fixed ch32fun commit and a compiler release verified by
  SHA-256.

The host Python environments contain only PyYAML and, where needed, pyserial.
YAML command-line operations use `yq-go`; network discovery uses the official
Arduino `mdns-discovery` 1.1.0 executable, downloaded on first scan into `bin/`
with a fixed SHA-256 for Linux x86-64/ARM64 and macOS Intel/Apple Silicon.
Discovery does not require the Arduino CLI, a Python virtualenv, or a system
Avahi service. The check script runs shells sequentially. Nix and firmware
compilation use their default parallelism. For Make, `NIX_SHELL_FLAGS` can
pass optional settings to Nix.

Keep the Nix pin under version control with ACME. Updating the machine's
channel will not update ACME. To update ACME's host tools, change both `rev`
and `sha256` in `nix/nixpkgs.json` deliberately, then open all four shells and
build representative Arduino, IDF, and CH32 projects on Linux and the Macs
before adopting the update. A successful Nix evaluation alone does not prove
that packages build or that firmware compiles. Intel macOS support ends with
the 26.05 Nixpkgs line, so future upgrades must take that into account if an
Intel Mac is still in use.

ESP-IDF Python environments already created under `bin/esp-idf-tools/` for
a different Python interpreter may need to be recreated. Its backend's
`clean-bin CONFIRM=yes` target removes the selected IDF checkout and tools;
the next build installs them again.

---

## Getting started

```sh
# Generate project targets
make

# Build an example
make build-arduino-blink
```

On first `flash` or `ota`, ACME scans for available devices and asks you to pick
one. The choice is saved — next time it just works.

---

## Project structure

```
your-project/
  examples/
    arduino/
      blink/
        blink.ino
        project.yaml
  wifi.yaml          # optional, git-ignored WiFi credentials
  acme.mk            # copied from ACME repo
  makefile           # one-liner that includes ACME
```

**`makefile`** (drop-in, you can add custom targets):
```makefile
-include acme.mk
```

**`acme.mk`** (copied from ACME repo, finds ACME automatically):
```makefile
# looks for acme in ./acme or ../acme, or set ACME_DIR manually
```

---

## Platforms

### Arduino

```yaml
platform: arduino
board: esp32:esp32:m5stack_cardputer
baudrate: 115200

dependencies:
  - FastLED
  - WebSockets@2.3.5
  - ArduinoJson@7.1.0

# local libs with library.properties
lib_dirs:
  - ../lib/my-arduino-lib

# local code without library.properties — injected into sketch dir
inject:
  - ../lib/my-raw-lib

# compiler defines
defines:
  - MY_FEATURE=1
```

**`wifi.yaml`** (optional, add to `.gitignore`):
```yaml
ssid: MyNetwork
psk: mypassword
```

When present, `STASSID` and `STAPSK` are automatically added as compiler defines.
`acme.mk` prefers an application-local `wifi.yaml`; the ACME root file is the
fallback. Defining `STASSID` or `STAPSK` in `project.yaml` emits a warning; use
`wifi.yaml` to avoid duplicate definitions. Changing, removing, or recreating
that file invalidates the Arduino and ESP-IDF build caches. Invalid YAML or a
missing `yq` causes an explicit error when the file is present.

### ESP-IDF

For ordinary applications, only `project.yaml` and one or more `.c`/`.cpp`
files are required. ACME generates the root and component `CMakeLists.txt`,
`sdkconfig.defaults`, and partition configuration under `.cache/`. An existing
full ESP-IDF project with its own root `CMakeLists.txt` is still accepted.

The default ESP-IDF release is `v6.0.3`; `version` also accepts a tag, branch,
or commit hash. The selected release and its official tools are installed in
versioned directories under `bin/`.

```yaml
platform: idf
target: esp32c3
version: v6.0.3
baudrate: 115200
flash_size: 4MB       # 4MB, 8MB, or 16MB
filesystem: spiffs
components:             # direct dependencies enable a minimal build
  - esp_driver_gpio
defines:
  - MY_FEATURE=1
```

The same optional root `wifi.yaml` used by Arduino supplies `STASSID` and
`STAPSK` defines to ESP-IDF builds.

The standard partition tables include NVS, OTA metadata, two equal OTA app
slots, and a `storage` SPIFFS partition. A `data/` directory enables `fs`,
`flash-fs`, `serve`, and `ota-fs`; firmware OTA uses HTTP `POST /update` and
filesystem OTA uses `POST /update-fs`. See `examples/esp-idf/ota` for the
matching device-side server and mDNS advertisement.

### CH32

CH32 projects use [ch32fun](https://github.com/cnlohr/ch32fun). The source file
must have the same name as its directory, for example `blink/blink.c`. ch32fun
is pinned by hash and cloned into a versioned directory under `bin/`. ACME also
downloads the matching xPack RISC-V compiler for the host and verifies its
SHA-256 instead of building a cross compiler locally.

```yaml
platform: ch32
mcu: CH32V003
toolchain: 15.2.0-1
version: 6670407ae29d06fb6155ca1e0f7a5058918d05d8
programmer: esp32     # default; minichlink is also supported
defines:
  - FUNCONF_USE_DEBUGPRINTF=1
```

The default `esp32` programmer is `examples/arduino/ch32-programmer`. Build and
flash that once with Arduino, connect its SWIO pin to the CH32, then the CH32
`flash` and `monitor` targets use `tools/ch32_flash.py` automatically.

---

## Targets

| Target | Description |
|---|---|
| `make` | Generate project targets (default) |
| `make build-<platform>-<name>` | Compile a project |
| `make flash-<platform>-<name>` | Build and flash a project |
| `make monitor-<platform>-<name>` | Open its monitor |
| `make list-usb-<platform>-<name>` | List serial devices without platform-specific tooling |
| `make forget-usb-<platform>-<name>` | Forget the saved serial-device choice |

Projects with a `data/` directory additionally receive `serve`, `fs`,
`flash-fs`, and `ota-fs` targets. Network-capable projects provide firmware
`ota`. Every project provides the shared USB discovery targets.

---

## Examples

Projects are grouped by platform under `examples/`:

```sh
make build-arduino-blink
make build-arduino-blink-c3-zero
make build-arduino-ota32
make build-arduino-ota8266
make build-arduino-websockets
make build-arduino-ch32-programmer
make build-ch32-blink
make build-ch32-uart
make flash-ch32-blink
make build-idf-blink
make build-idf-hello
make fs-idf-filesystem
make ota-idf-ota
```

If the example has a `data/` directory, ACME also generates filesystem targets:

```sh
make serve-arduino-websockets
make fs-arduino-websockets
make ota-fs-arduino-websockets
make flash-fs-arduino-websockets
```

---

## Variables

Override any of these on the command line:

```sh
make build-arduino-blink
make flash-arduino-blink PORT=/dev/cu.usbmodem1234
make ota-arduino-websockets OTAIP=192.168.1.42 OTAPORT=80
make monitor-arduino-blink BAUD=9600
```

HTTP OTA uses port 80 by default; set `OTAPORT` only when the device serves
the ACME HTTP protocol on a different port.

---

## Device selection

On first `flash`, `monitor`, or `ota`, ACME scans for available devices and
shows an interactive list:

```
  1) /dev/cu.usbmodem1123401   serial   ESP32 Family Device
  2) /dev/cu.usbmodem1123402   serial   ESP32 Family Device
Select device [1]:
```

The selection is saved in `.cache/devices/usb/<sketch>` or
`.cache/devices/ota/<sketch>`. If the device is no longer available on the next
USB operation, ACME asks whether to clear the saved config and rescan. OTA
stores the device ID, hostname, address, and port in JSON, verifies `/info`
before uploading, and rediscovers that same ID when its endpoint changes.
It never silently replaces a saved device with another one. Use
`make forget-ota-<platform>-<name>` to select a different device.

## Shared HTTP OTA

Arduino ESP32/ESP8266 and ESP-IDF examples implement the same `http-v1`
contract:

| Request | Behavior |
|---|---|
| `GET /info` | JSON containing `acme: 1`, `ota_protocol: "http-v1"`, stable MAC-based `id`, hostname, platform, and firmware version |
| `POST /update` | Stream a firmware binary to the OTA partition; reboot only after successful completion |
| `POST /update-fs` | Stream a filesystem image; reboot after successful completion |

Uploads use raw `application/octet-stream` bodies with `Content-Length`, not
multipart forms. The host sends them with `curl`; the firmware processes
chunks of at most 1 KiB. Applications must provide an OTA-capable partition
layout. Filesystem uploads write the filesystem image rather than individual
files; mounted filesystems must be unmounted before updating them.

Devices advertise `_arduino._tcp.local.` with TXT fields `acme=1`,
`ota_protocol=http-v1`, and `id=<device-id>`. The Arduino service name allows
reuse of the official discovery tool; the TXT fields distinguish ACME HTTP
devices from legacy ArduinoOTA devices. Discovery has a configurable deadline
(`OTA_DISCOVERY_TIMEOUT=5` by default), deduplicates identities, and handles
service additions/removals. It requires multicast reachability on the local
network. Rediscovery of a saved ID stops as soon as that device is found.
The uploader uses the IP resolved by mDNS, so automatic discovery does not
depend on the OS resolving `.local` names. An explicit `OTAIP` bypasses
discovery but still verifies `/info`.

```sh
make scan-arduino-websockets
make ota-arduino-websockets
make ota-fs-arduino-websockets
make ota-idf-ota OTAIP=192.168.1.42
python3 tools/scan.py --timeout 5 --json
```

The bundled `AcmeOTA` Arduino library is available automatically during ACME
builds. The `ota32` and `ota8266` examples show its minimal setup. The
`websockets` example shares port 80 between HTTP OTA and its web page, keeps
WebSockets on port 81, and unmounts SPIFFS during filesystem updates. The IDF
example uses its native HTTP server and OTA APIs.

**Migration:** firmware using the old `ArduinoOTA`/`espota.py` protocol must
first receive the updated example over USB. The new host uploader does not
speak the old protocol. Legacy `IP:port` cache entries are rediscovered and
replaced with identity-based JSON records. The example endpoints retain the
previous examples' unauthenticated local-network behavior.

Application projects can set `ota_port`, `ota_path`, and `ota_fs_path` in
`project.yaml`; those endpoints must match the firmware. Set `OTA_TIMEOUT`
to change the upload deadline (120 seconds by default). Run host protocol
tests with `python3 -m unittest discover -s tests -p test_ota.py`; native
streaming tests use `test_firmware_ota.py` and require `g++`. These tests do not
replace compilation with the target toolchains or OTA testing on real boards.

---

## How ACME is included

ACME can be used in three ways:

- **Embedded** — copy the `acme/` folder inside your project
- **Sibling** — keep `acme/` next to your project folder
- **Custom path** — set `ACME_DIR=/path/to/acme` before running make

`acme.mk` handles discovery automatically.
