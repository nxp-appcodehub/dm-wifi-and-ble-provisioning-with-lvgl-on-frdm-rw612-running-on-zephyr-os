# NXP Application Code Hub

[<img src="https://mcuxpresso.nxp.com/static/icon/nxp-logo-color.svg" width="100"/>](https://www.nxp.com)

## Wi-Fi and BLE Provisioning with LVGL UI on FRDM-RW612 running on Zephyr

This demo enables the FRDM-RW612 to provision Wi-Fi credentials over Bluetooth LE. The board acts as a BLE GATT peripheral advertising a custom Wi-Fi provisioning service. A mobile app connects over BLE, writes the target Wi-Fi SSID and password, and the device connects to the Wi-Fi access point using WPA2-PSK. The full provisioning journey is visualized on an LCD-PAR-S035 display through an LVGL v9 user interface.

#### Boards: FRDM-RW612
#### Categories: Wireless Connectivity, Graphics
#### Peripherals: Wi-Fi, BLE, Display
#### Toolchains: Zephyr SDK, GCC

## Table of Contents

1. [Software](#step1)
2. [Hardware](#step2)
3. [Setup](#step3)
4. [Results](#step4)
5. [FAQs](#step5)
6. [Support](#step6)
7. [Release Notes](#step7)

## 1. Software<a name="step1"></a>
- Zephyr 4.4.0
- Zephyr SDK version 1.0.1

## 2. Hardware<a name="step2"></a>
- [FRDM-RW612](https://www.nxp.com/part/FRDM-RW612)
- [LCD-PAR-S035](https://www.nxp.com/part/LCD-PAR-S035)

## 3. Setup<a name="step3"></a>

Before getting started, make sure you have a proper Zephyr development environment. Follow the official [Zephyr Getting Started Guide](https://docs.zephyrproject.org/latest/getting_started/index.html).

> [!NOTE]
> This demo has been tested on Zephyr SDK version 1.0.1. When installing the SDK it is recommended to specify the version using `west sdk install --version 1.0.1`.

- Cloning the source code and setting up the workspace:

    ```bash
    west init my-workspace -m https://github.com/nxp-appcodehub/dm-wifi-ble-provisioning-lvgl-rw612 --mr main
    cd my-workspace
    west update
    # Set up environment variables:
    #   Ubuntu Linux: source zephyr/zephyr-env.sh
    #   Windows:      zephyr\zephyr-env.cmd
    west blobs fetch    # fetch nxp-hal binary blobs (Wi-Fi / BLE firmware)
    ```

### Building the Zephyr application

    ```bash
    west build -b frdm_rw612 app -d build_wifi_ble_provisioning --pristine --shield lcd_par_s035_spi
    ```

### Flash the Zephyr application to the board

    ```bash
    set PATH=%PATH%;<Linkserver path>
    west flash --runner linkserver -d build_wifi_ble_provisioning
    ```

## 4. Results<a name="step4"></a>

### 4.1 FRDM-RW612 Console output

After flashing, open a serial terminal (115200 8N1) on the board's debug UART. The boot log shows the provisioning subsystems coming up and the device advertising over BLE:

    ```bash
    *** Booting Zephyr OS build nxp-v4.1.0-48061-g8557b5995e65 ***
    [00:05:01.148,322] <inf> main: === Wi-Fi + BLE Provisioning FRDM-RW612 ===
    [00:05:01.148,417] <inf> main: Zephyr 4.4.0 | LVGL v9.5
    [00:05:01.234,512] <inf> ui: UI initialised - display 480x320
    [00:05:03.234,626] <inf> wifi_mgr: Wi-Fi manager initialised
    [00:05:03.283,540] <inf> bt_hci_core: HCI transport: BT NXP
    [00:05:03.283,665] <inf> bt_hci_core: Identity: 00:50:43:00:00:66 (public)
    [00:05:03.283,682] <inf> bt_hci_core: HCI: version 6.1 (0x0f) revision 0x8300, manufacturer 0x0025
    [00:05:03.283,691] <inf> bt_hci_core: LMP: version 6.1 (0x0f) subver 0x141b
    [00:05:03.283,701] <inf> ble_prov: Bluetooth enabled
    [00:05:03.286,520] <inf> ble_prov: BLE advertising started - device name: NXP-WiFi-Prov
    [00:05:03.286,570] <inf> main: Provisioning ready - advertising as "NXP-WiFi-Prov"
    ```

### 4.2 Provisioning from the mobile app

1. In nRF Connect, scan and connect to **NXP-WiFi-Prov**.
2. Discover services. The custom provisioning service uses the base UUID `8b84xxxx-f6fa-4e33-9a0c-e14d9f35f201`:

    | Characteristic | UUID (`xxxx`) | Properties  | Description                     |
    |----------------|---------------|-------------|---------------------------------|
    | SSID           | `0002`        | Write       | Wi-Fi network name (<=32 bytes) |
    | PSK            | `0003`        | Write       | Wi-Fi password (<=64 bytes)     |
    | CONTROL        | `0004`        | Write       | `0x01` = trigger connect        |
    | STATUS         | `0005`        | Read/Notify | Provisioning status byte        |

3. Write the **SSID** (UTF-8) to the SSID characteristic.
4. Write the **password** (UTF-8) to the PSK characteristic.
5. Write **`01`** (hex byte) to the CONTROL characteristic to start the connection.

On success the console shows the full provisioning sequence:

    ```bash
    [00:05:08.762,916] <inf> ble_prov: BLE central connected
    [00:05:08.774,809] <inf> main: [APP] BLE connected! Waiting for credentials
    [00:05:25.672,617] <inf> ble_prov: SSID received: "Asus_5G-TWT"
    [00:05:38.452,580] <inf> ble_prov: PSK received (10 bytes)
    [00:05:45.517,507] <inf> ble_prov: CTRL byte: 0x01
    [00:05:45.525,626] <inf> main: [APP] Credentials received - SSID: "Asus_5G-TWT"
    [00:05:45.525,792] <inf> wifi_mgr: Connecting to SSID: "Asus_5G-TWT" (security: WPA2-PSK)
    [00:05:53.267,141] <inf> wifi_mgr: DHCP bound - Wi-Fi fully connected
    [00:05:53.267,599] <inf> wifi_mgr: Wi-Fi connected - waiting for DHCP
    Connected
    [00:05:53.280,588] <inf> main: [APP] Wi-Fi connected! IP: 192.168.1.136
    ```

### 4.3 On-screen (LVGL) flow

The LCD walks through the provisioning journey, reflecting the application state machine:

`Boot` -> `Advertising` -> `BLE Connected` -> `Wi-Fi Connecting` -> `Wi-Fi Connected` (or `Failed`)

## 5. FAQs<a name="step5"></a>

*Include FAQs here if appropriate. If there are none, then remove this section.*
 
## 6. Support<a name="step6"></a>

*Provide URLs for help here.*

#### Project Metadata

<!----- Boards ----->
[![Board badge](https://img.shields.io/badge/Board-FRDM–RW612-blue)]()

<!----- Categories ----->
[![Category badge](https://img.shields.io/badge/Category-WIRELESS%20CONNECTIVITY-yellowgreen)]()
[![Category badge](https://img.shields.io/badge/Category-GRAPHICS-yellowgreen)]()

<!----- Peripherals ----->
[![Peripheral badge](https://img.shields.io/badge/Peripheral-WIFI-green)]()
[![Peripheral badge](https://img.shields.io/badge/Peripheral-BLE-green)]()
[![Peripheral badge](https://img.shields.io/badge/Peripheral-DISPLAY-green)]()

<!----- Toolchains ----->
[![Toolchain badge](https://img.shields.io/badge/Toolchain-ZEPHYR%20SDK-orange)]()

Questions regarding the content/correctness of this example can be entered as Issues within this GitHub repository.

>**Warning**: For more general technical questions regarding NXP Microcontrollers and the difference in expected functionality, create a support ticket through the [NXP Community](https://community.nxp.com/).

## 7. Release Notes<a name="step7"></a>

| Version | Description / Update                    | Date        |
|:-------:|-----------------------------------------|:-----------:|
| 1.0     | Initial release on Application Code Hub | October 16<sup>th</sup> 2026 |
