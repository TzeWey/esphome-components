# mel_ac

Mitsubishi Electric Air-Conditioner ESPHome Component

## Features

- Communicates to the unit directly via the `CN105` connector.
- Fast response
- Syncs changes made from other sources such as the IR remote.

## Requirements

- ESPHome supported hardware
- ESPHome 2024.3.1 or newer
  - Verified with ESPHome 2024.3.1, but may work with earlier versions that
    supports [External Components](https://esphome.io/components/external_components.html)

## Supported Hardware

This component should work with ESP8266 or ESP32 platforms.
It has been tested with the following:

- Seeed Studio XIAO ESP32C3 (board:seeed_xiao_esp32c3)

## Supported Mitsubishi Models

This component was created with reference to the awesome
[HeatPump](https://github.com/SwiCago/HeatPump) library but only implements the
necessary commands that are accessible by the
[ESPHome Climate Component](https://esphome.io/components/climate/index.html).
It should work with other reported models that has the `CN105` connector.

Functionality of this component has been verified on the following units:

- MSXY-FN10VE (baud_rate: 2400)

## Usage

### 1. Connect to the Air Conditioning Unit

There are many resources available on the internet that covers finding and
connecting to the CN105 on the unit. One such reference would be
[2017-10-24-CN105_Connector](https://casualhacker.net/post/2017-10-24-CN105_Connector).

Next, wire up the module to the connector as detailed in the [SwiCago/HeatPump
README](https://github.com/SwiCago/HeatPump/blob/master/README.md#demo-circuit).

NOTE: The `RX/TX` voltage levels of the unit is 5V (at least with the MSXY-FN10VE).
My setup uses a voltage level translator as there doesn't seem to be any
documentation that indicates the ESP32C3's IO is 5V tolerant.

### 2. Add this repository as an External Component in ESPHome 2024.3.1 or newer

This repository implements the component as an ESPHome's
[External Component](https://esphome.io/components/external_components.html)

Add this repository to your ESPHome config:

```yaml
external_components:
  - source: github://TzeWey/esphome-components
    components: [mel_ac]
```

### 3. Configure the Component

Configure the `UART` that is connected to the CN105 connector with the following
options:

| Option    | Supported Values |
| --------- | ---------------- |
| baud_rate | 2400, 4800, 9600 |
| parity    | EVEN             |

Example Configuration:

```yaml
uart:
  id: CN105
  tx_pin: 9
  rx_pin: 10
  baud_rate: 2400
  parity: EVEN
```

> If communication to the unit fails, try changing to another supported baud rate.

Configure the `mel_ac` component to reference the `UART` component created above
by its `id`.

Example Configuration:

```yaml
climate:
  - platform: mel_ac
    name: "TEST AC"
    uart_id: CN105
```

# Component Configuration

Below is an example configuration that was used during development that enables
over-the-air updates. You'll need to create a `secrets.yaml` file inside of your
`esphome` directory with entries for the various items prefixed with `!secret`.

```yaml
substitutions:
  name: test
  friendly_name: TEST

esphome:
  name: ${name}
  friendly_name: ${friendly_name}
  name_add_mac_suffix: false
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
  # NOTE: disable serial port logging if your hardware only has 1 UART port (e.g. ESP8266)
  # baud_rate: 0

# Enable Home Assistant API
api:

# Allow Over-The-Air updates
ota:

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
    components: [mel_ac]

uart:
  id: CN105
  tx_pin: 9
  rx_pin: 10
  baud_rate: 2400
  parity: EVEN

climate:
  - platform: mel_ac
    name: "AC"
    uart_id: CN105
    # startup_delay: 10s # delay startup to debug component init from logs
    supported_modes:
      - "COOL"
      - "DRY"
    supported_fan_modes:
      - "QUIET"
      - "FOCUS"
    supported_swing_modes:
      - "VERTICAL"
```

### Climate Modes

Additional climate modes may be available for your model of Air Conditioner.
This can be easily enabled or hidden depending on your usage by configuring the
`suppored_*_modes` options.

For example, if your model has a heater function, you may want to add "HEAT" as
one of the `supported_modes`:

```yaml
climate:
. . .
    supported_modes:
      - "COOL"
      - "HEAT"
```

### Component Options

| Options               | Description                                  | Default      |
| --------------------- | -------------------------------------------- | ------------ |
| max_refresh_rate      | Status refresh interval in milliseconds      | 1s           |
| startup_delay         | Delay the component start-up in milliseconds | 0s           |
| supported_modes       | List of supported Climate Modes              |              |
| supported_fan_modes   | List of supported Climate Fan Modes          |              |
| supported_swing_modes | List of supported Climate Swing Modes        | ["VERTICAL"] |

| supported_modes | Internal POWER | Internal MODE | Description      |
| --------------- | -------------- | ------------- | ---------------- |
| "OFF"           | "OFF"          | ---           | Always available |
| "AUTO"          | "ON"           | "AUTO"        | Always available |
| "HEAT_COOL"     | "ON"           | "AUTO"        |                  |
| "COOL"          | "ON"           | "COOL"        |                  |
| "HEAT"          | "ON"           | "HEAT"        |                  |
| "DRY"           | "ON"           | "DRY"         |                  |
| "FAN_ONLY"      | "ON"           | "FAN"         |                  |

| supported_fan_modes | Internal FAN | Comments         |
| ------------------- | ------------ | ---------------- |
| "AUTO"              | "AUTO"       | Always available |
| "LOW"               | "1"          | Always available |
| "MEDIUM"            | "2"          | Always available |
| "HIGH"              | "3"          | Always available |
| "FOCUS"             | "4"          |                  |
| "QUIET"             | "QUIET"      |                  |

| supported_swing_modes | Internal VANE_VERT | Internal VANE_HORZ | Comments         |
| --------------------- | ------------------ | ------------------ | ---------------- |
| "OFF"                 | "AUTO"             | "AUTO"             | Always available |
| "BOTH"                | "SWING"            | "SWING"            |                  |
| "VERTICAL"            | "SWING"            | "AUTO"             |                  |
| "HORIZONTAL"          | "AUTO"             | "SWING"            |                  |

> Other vane modes are not configurable through HomeAssistant but may be configured from the IR remote.

### Other Options

- _id_ (_Optional_): used to identify multiple instances (e.g. "ac_room1")
- _name_ (_Required_): The name of the climate component (e.g. "Room 1 Air Conditioner")
- _visual_ (_Optional_): The core `Climate` component has several _visual_
  options that can be set. \
  See the [Climate Component](https://esphome.io/components/climate/index.html)
  documentation for details.

# References

- [ESPHome](https://github.com/esphome/esphome)
- [esphome-mitsubishiheatpump](https://github.com/geoffdavis/esphome-mitsubishiheatpump)
- [MEL-AC lib for Mongoose OS](https://github.com/mongoose-os-libs/mel-ac)
- [HeatPump](https://github.com/SwiCago/HeatPump)
