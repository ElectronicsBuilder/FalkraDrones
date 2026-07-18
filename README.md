# FalkraDrones Firmware

Firmware for the **Falkra Controller**, an STM32F767-based embedded system using modular C++ drivers and FreeRTOS.

---

## Target MCU

- STM32F767ZGT6 (ARM Cortex-M7)
- Peripherals used: UART, QSPI, I2C, SPI...
- RTOS: CMSIS-RTOS v2 (FreeRTOS backend)


---

## Build Instructions (Linux & Windows)

See [GIT_CHEATSHEET.md](GIT_CHEATSHEET.md) for full setup, build, and GitHub CI steps across Windows & Linux.

This project uses CMake + Ninja and the **Arm GNU Toolchain**.

### Clone the Repo

```bash
git clone https://github.com/ElectronicsBuilder/FalkraDrones.git
cd FalkraDrones
```

### Linux Setup

```bash
# Install build tools and ARM toolchain
sudo apt update
sudo apt install cmake ninja-build gcc-arm-none-eabi gdb-arm-none-eabi

# Configure & Build
cmake -B build -G Ninja
cmake --build build
```






### Windows Setup (STM32CubeCLT)

1. Install STM32CubeCLT
2. Add CMake + Ninja to PATH from STM32CubeCLT
3. Open CMD or PowerShell

```cmd
cd path\to\FalkraDrones

# Configure & Build
cmake -B build -G Ninja
cmake --build build
```


**Output ELF:** build/FalkraDrones.elf


** tasks.json "example" **  <- Load QSPI Assets if changed 
{
  "version": "2.0.0",
  "tasks": [
    {
      "label": "build",
      "type": "shell",
      "command": "cmake --build ${workspaceFolder}/build/Debug",
      "group": {
        "kind": "build",
        "isDefault": true
      },
      "problemMatcher": []
    },
    {
      "label": "flash-qspi-assets",
      "type": "shell",
      "command": "python",
      "args": [
        "${workspaceFolder}/tools/external_loader/flash_qspi_if_changed.py"
      ],
      "problemMatcher": []
    }

  ]
}



## GitHub Actions

This project builds automatically using GitHub Actions CI on every push to `main`

---

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

### Copyright Notice

Copyright (c) 2025 ElectronicsBuilder

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

---

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## Support

For issues and questions, please open an issue on GitHub: https://github.com/ElectronicsBuilder/FalkraDrones/issues
