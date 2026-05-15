# 2.13" 410×502 AMOLED QSPI module (CO5300) — documentation & samples

**简体中文：** [`README.md`](README.md)

---

> This repository provides **sample projects** for this module, together with datasheets, specifications, and interface / bring-up documentation for selection reference and integration.

## Product overview

| Item | Description |
|:--|:--|
| Module | 2.13-inch **AMOLED** panel, **410×502** resolution |
| Interface | **QSPI** |
| Driver IC | **CO5300** |
| Spec ID | **`2.13-amoled-410x502-qspi-co5300`** is the common product designation in documentation |

---

## Repository layout

### Top-level

| Path | Contents |
|:--|:--|
| `docs/` | Datasheets, specifications, interface and initialization documentation |
| `examples/` | **Sample projects** grouped by feature |

### `examples/` layout

| Location | Description |
|:--|:--|
| `examples/` root | **LVGL8** baseline samples, or **LVGL8 / LVGL9** with **esp-lvgl-adapter** |
| `with-te/` | Display sync and tear-related samples using **TE** |
| `with-te-sw-rotate-90/` | **TE**-based flow plus **90° software rotation** |

### Sample project paths

#### Baseline and esp-lvgl-adapter

| Description | Path |
|:--|:--|
| LVGL8 baseline | `examples/esp32s3-idf5_co5300-qspi_lvgl8/` |
| esp-lvgl-adapter + LVGL8 | `examples/esp32s3-idf5_co5300-qspi_esp-lvgl-adapter_lvgl8/` |
| esp-lvgl-adapter + LVGL9 | `examples/esp32s3-idf5_co5300-qspi_esp-lvgl-adapter_lvgl9/` |

#### with-te

| Description | Path |
|:--|:--|
| esp-lvgl-adapter + LVGL8 + AMOLED with TE | `examples/with-te/esp32s3-idf5_co5300-qspi_esp-lvgl-adapter_lvgl8_amoled-with-te/` |
| esp-lvgl-adapter + LVGL9 + AMOLED with TE | `examples/with-te/esp32s3-idf5_co5300-qspi_esp-lvgl-adapter_lvgl9_amoled-with-te/` |

#### with-te-sw-rotate-90

| Description | Path |
|:--|:--|
| LVGL8 + AMOLED with TE + 90° software rotation | `examples/with-te-sw-rotate-90/esp32s3-idf5_co5300-qspi_lvgl8_amoled-with-te/` |
