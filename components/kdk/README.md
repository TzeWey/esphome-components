# kdk

KDK Fan ESPHome Component

## Overview

- Replaces the DNSK-P11 entirely
- Fast response
- Syncs changes made from the IR remote

## Requirements

- ESPHome supported hardware
- ESPHome 2024.3.1 or newer
  - Verified with ESPHome 2024.6.4, but may work with earlier versions that
    supports [External Components](https://esphome.io/components/external_components.html)

## Supported Hardware

This component should work with ESP8266 or ESP32 platforms.
It has been tested with the following:

- Seeed Studio XIAO ESP32C3 (board:seeed_xiao_esp32c3)

## Supported KDK Models

[ESPHome Light Component](https://esphome.io/components/light).
[ESPHome Fan Component](https://esphome.io/components/climate).

It should work with other models that has the `DNSK-P11` module.

Functionality of this component has been verified on the following units:

- K12YC (baud_rate: 9600)
- K12UC (baud_rate: 9600)

## Usage

### 1. Replace the DNSK-P11 module

TODO: Update documentation

**WARNING**: Turn OFF mains power before opening the fan.

NOTE: The `RX/TX` voltage levels of the unit is 5V.
A voltage level translator is recommended if the MCU used is not 5V tolerant.

### 2. Add this repository as an External Component in ESPHome 2024.3.1 or newer

This repository implements the component as an ESPHome's
[External Component](https://esphome.io/components/external_components.html)

Add this repository to your ESPHome config:

```yaml
external_components:
  - source: github://TzeWey/esphome-components
    components: [kdk]
```

### 3. Configure the Component

Communication with the fan is over UART with a baud rate of 9600, 8-bits,
1 stop bit, even parity.

Configure the `uart` component as follows:

```yaml
uart:
  id: KDK_CN5
  tx_pin: 20
  rx_pin: 21
  baud_rate: 9600
  parity: EVEN
```

Configure the `kdk` component to reference the `uart` component created above
by its `id`.

Example Configuration:

```yaml
kdk:
  uart_id: KDK_CN5

light:
  - platform: kdk
    name: "Light"

fan:
  - platform: kdk
    name: "Fan"
```

> Remove 'light' if the fan model does not come with LED lights (e.g. K12YC)

## TODO

- Expose Night Light functionality

# Component Configuration

Below is an example configuration that was used during development that enables
over-the-air updates. You'll need to create a `secrets.yaml` file inside of your
`esphome` directory with entries for the various items prefixed with `!secret`.

[Safe Mode](https://esphome.io/components/safe_mode) must be disabled as it will
prevent the fan from booting properly. The fan will cycle the module's power
when communication fails for 10s. This is not enough time to recover via OTA.

```yaml
substitutions:
  name: esphome-web-xxxxxx
  friendly_name: Living Room KDK

esphome:
  name: ${name}
  friendly_name: ${friendly_name}
  name_add_mac_suffix: false
  platformio_options:
    board_build.flash_mode: dio
  project:
    name: esphome.web
    version: "1.0"

# Update to match your hardware
esp32:
  board: seeed_xiao_esp32c3
  framework:
    type: esp-idf

# Enable logging
logger:
  level: DEBUG
  # NOTE: disable serial port logging if your hardware only has 1 UART port (e.g. ESP8266)
  # baud_rate: 0

# Disable Safe Mode as OTA is not possible since fan will repeatedly cycle the
# module's power when communication fails for 10s.
safe_mode:
  disabled: true

# Enable Home Assistant API
api:

# Allow Over-The-Air updates
ota:
  - platform: esphome

# Allow provisioning Wi-Fi via serial
improv_serial:

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

dashboard_import:
  package_import_url: github://esphome/firmware/esphome-web/esp32c3.yaml@v2
  import_full_config: true

# To have a "next url" for improv serial
web_server:

external_components:
  - source:
      type: git
      url: https://github.com/TzeWey/esphome-components
    components: [kdk]
    refresh: 0s

uart:
  id: KDK_CN5
  tx_pin: 20
  rx_pin: 21
  baud_rate: 9600
  parity: EVEN

kdk:
  uart_id: KDK_CN5

fan:
  - platform: kdk
    name: "Fan"

light:
  - platform: kdk
    name: "Light"
    type: MAIN_LIGHT
  - platform: kdk
    name: "Night Light"
    type: NIGHT_LIGHT
```

# References

- [ESPHome](https://github.com/esphome/esphome)
- [esphome-panasonic-ac](https://github.com/DomiStyle/esphome-panasonic-ac)
