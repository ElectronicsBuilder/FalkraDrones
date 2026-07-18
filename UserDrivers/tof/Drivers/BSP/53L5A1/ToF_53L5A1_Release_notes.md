---
pagetitle: Release Notes for 53L5A1 BSP 
lang: en
header-includes: <link rel="icon" type="image/x-icon" href="_htmresc/favicon.png" />
---

::: {.row}
::: {.col-sm-12 .col-lg-4}

<center>
# Release Notes for <mark>53L5A1</mark> BSP
Copyright &copy; 2022 STMicroelectronics\
    
[![ST logo](_htmresc/st_logo_2020.png)](https://www.st.com){.logo}
</center>

# Purpose

The **53L5A1 BSP** is a software component intenteded to be used within the **STM32Cube** ecosystem. This software implements the BSP v2.0 specifications for the X-NUCLEO-53L5A1 Expansion Board on STM32. It is built on top of STM32Cube software technology that ease portability across different STM32 micro-controllers. 

Here is the list of references: 

- [X-NUCLEO-53L5A1: 8x8 multizone with wide field of view ranging sensor expansion board based on VL53L5CX for STM32 Nucleo](https://www.st.com/content/st_com/en/products/evaluation-tools/product-evaluation-tools/imaging-evaluation-boards/x-nucleo-53l5a1.html)
- [VL53L5CX: Time-of-Flight 8x8 multizone ranging sensor with wide field of view](https://www.st.com/content/st_com/en/products/imaging-and-photonics-solutions/time-of-flight-sensors/vl53l5cx.html)
- [VL53L5CX-SATEL: Breakout board for VL53L5CX](https://www.st.com/content/st_com/en/products/evaluation-tools/product-evaluation-tools/imaging-evaluation-boards/vl53l5cx-satel.html)

:::

::: {.col-sm-12 .col-lg-8}
# Update History
::: {.collapse}
<input type="checkbox" id="collapse-section6" checked aria-hidden="true">
<label for="collapse-section6" aria-hidden="true">__v1.0.3 / May 11th 2022__</label>
<div>			

## Main Changes

### Maintenance release and product update

- Removed useless vl53l5cx_i2c_recover() function as 53L5A1 shield has no IO expander

## Contents
<small>The components flagged by "[]{.icon-st-update}" have changed since the
previous release. "[]{.icon-st-add}" are new.</small>

Components

  Name                                                        Version                                           Release note
  ----------------------------------------------------------- ------------------------------------------------- ------------------------------------------------------------------------------------------------------------------------------------------------
  **X-NUCLEO-53L5A1 BSP Driver**                                                     V1.0.3[]{.icon-st-update}  [release note](.\Release_Notes.html)

Note: in the table above, components **highlighted** have changed since previous release.

## Known Limitations


  Headline
  ----------------------------------------------------------
  No known limitations

## Development Toolchains and Compilers

- IAR System Workbench V9.20.1
- ARM Keil V5.32
- STM32CubeIDE v1.9.0

## Supported Devices and Boards

- X-NUCLEO-53L5A1
- VL53L5CX-SATEL

## Backward Compatibility

N/A

## Dependencies

This software release is compatible with:

- VL53L5CX Component Driver v1.0.2
- STM32CubeHAL F4 V1.7.12
- STM32CubeHAL L4 V1.13.0

</div>
:::

::: {.collapse}
<input type="checkbox" id="collapse-section6" checked aria-hidden="true">
<label for="collapse-section6" aria-hidden="true">__v1.0.2 / January 12th 2022__</label>
<div>			

## Main Changes

### Maintenance release and product update

- Added missing LICENSE files

## Contents
<small>The components flagged by "[]{.icon-st-update}" have changed since the
previous release. "[]{.icon-st-add}" are new.</small>

Components

  Name                                                        Version                                           Release note
  ----------------------------------------------------------- ------------------------------------------------- ------------------------------------------------------------------------------------------------------------------------------------------------
  **X-NUCLEO-53L5A1 BSP Driver**                                                     V1.0.2[]{.icon-st-update}  [release note](.\Release_Notes.html)

Note: in the table above, components **highlighted** have changed since previous release.

## Known Limitations


  Headline
  ----------------------------------------------------------
  No known limitations

## Development Toolchains and Compilers

- IAR System Workbench V9.20.1
- ARM Keil V5.32
- STM32CubeIDE v1.8.0

## Supported Devices and Boards

- X-NUCLEO-53L5A1
- VL53L5CX-SATEL

## Backward Compatibility

N/A

## Dependencies

This software release is compatible with:

- VL53L5CX Component Driver v1.0.1
- STM32CubeHAL F4 V1.7.12
- STM32CubeHAL L4 V1.13.0

</div>
:::

::: {.collapse}
<input type="checkbox" id="collapse-section6" checked aria-hidden="true">
<label for="collapse-section6" aria-hidden="true">__v1.0.1 / January 12th 2022__</label>
<div>			

## Main Changes

### Update with ULD L5CX driver 1.2.0

The component bare driver has been updated with ULD 1.2.0.

## Contents
<small>The components flagged by "[]{.icon-st-update}" have changed since the
previous release. "[]{.icon-st-add}" are new.</small>

Components

  Name                                                        Version                                           Release note
  ----------------------------------------------------------- ------------------------------------------------- ------------------------------------------------------------------------------------------------------------------------------------------------
  **X-NUCLEO-53L5A1 BSP Driver**                                                     V1.0.1[]{.icon-st-update}  [release note](.\Release_Notes.html)

Note: in the table above, components **highlighted** have changed since previous release.

## Known Limitations


  Headline
  ----------------------------------------------------------
  No known limitations

## Development Toolchains and Compilers

- IAR System Workbench V8.50.9
- ARM Keil V5.32
- STM32CubeIDE v1.8.0

## Supported Devices and Boards

- X-NUCLEO-53L5A1
- VL53L5CX-SATEL

## Backward Compatibility

N/A

## Dependencies

This software release is compatible with:

- VL53L5CX Component Driver v1.0.1
- STM32CubeHAL F4 V1.7.12
- STM32CubeHAL L4 V1.13.0

</div>
:::

::: {.collapse}
<input type="checkbox" id="collapse-section6" checked aria-hidden="true">
<label for="collapse-section6" aria-hidden="true">__v1.0.0 / March 15th 2021__</label>
<div>			

## Main Changes

### First release

This is the first release of the **53L5A1 BSP** Driver.

## Contents
<small>The components flagged by "[]{.icon-st-update}" have changed since the
previous release. "[]{.icon-st-add}" are new.</small>

Components

  Name                                                        Version                                           License                                                                                                       Release note
  ----------------------------------------------------------- ------------------------------------------------- ------------------------------------------------------------------------------------------------------------- ------------------------------------------------------------------------------------------------------------------------------------------------
  **X-NUCLEO-53L5A1 BSP Driver**                                                     V1.0.0[]{.icon-st-add}                                            [BSD 3-Clause](https://opensource.org/licenses/BSD-3-Clause)                                                [release note](.\Release_Notes.html)

Note: in the table above, components **highlighted** have changed since previous release.

## Known Limitations


  Headline
  ----------------------------------------------------------
  No known limitations

## Development Toolchains and Compilers

- IAR System Workbench V8.50.9
- ARM Keil V5.32
- STM32CubeIDE v1.6.1

## Supported Devices and Boards

- X-NUCLEO-53L5A1
- VL53L5CX-SATEL

## Backward Compatibility

N/A

## Dependencies

This software release is compatible with:

- VL53L5CX Component Driver v1.0.0
- STM32CubeHAL F4 V1.7.12
- STM32CubeHAL L4 V1.13.0

</div>
:::

:::
:::

<footer class="sticky">
::: {.columns}
::: {.column width="95%"}
For complete documentation on **STM32Cube Expansion Packages** ,
visit: [STM32Cube Expansion Packages](https://www.st.com/en/embedded-software/stm32cube-expansion-packages.html)
:::
::: {.column width="5%"}
<abbr title="Based on template cx566953 version 2.0">Info</abbr>
:::
:::
</footer>
