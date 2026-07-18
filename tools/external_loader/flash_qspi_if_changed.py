"""
/**
 * MIT License
 * 
 * Copyright (c) 2025 ElectronicsBuilder
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * 
 * @file    flash_qspi_if_changed.py
 * @brief   Flash QSPI if changed
 */
"""
import os
import sys
import hashlib
import subprocess
from pathlib import Path

# === CONFIGURATION ===
PROJECT_ROOT = Path(__file__).resolve().parents[2]
BIN_PATH = PROJECT_ROOT / "build" / "Debug" / "FalkraDronesExternalFlash.bin"
STAMP_PATH = Path(__file__).parent / ".last_qspi_hash.txt"
LOADER_PATH = Path(__file__).parent / "W25Q128J_FALKRADRONES.stldr"

CUBEPROG_CLI = "STM32_Programmer_CLI.exe" if os.name == "nt" else "STM32_Programmer_CLI"
CUBEPROG_PATH = Path("C:/Program Files/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin") / CUBEPROG_CLI \
    if os.name == "nt" else Path("/opt/stm32cubeprogrammer/bin") / CUBEPROG_CLI

# === FUNCTION TO COMPUTE FILE HASH ===
def compute_sha256(path):
    sha256 = hashlib.sha256()
    with open(path, 'rb') as f:
        for block in iter(lambda: f.read(4096), b""):
            sha256.update(block)
    return sha256.hexdigest()

# === CHECK FILES EXIST ===
if not BIN_PATH.exists():
    print(f"ERROR: Bin file not found: {BIN_PATH}")
    sys.exit(1)

if not LOADER_PATH.exists():
    print(f"ERROR: Loader file not found: {LOADER_PATH}")
    sys.exit(1)

# === CHECK HASH ===
current_hash = compute_sha256(BIN_PATH)
prev_hash = STAMP_PATH.read_text().strip() if STAMP_PATH.exists() else ""

if current_hash == prev_hash:
    print("QSPI asset is unchanged — skipping flash.")
    sys.exit(0)

# === RUN FLASH COMMAND ===
print("QSPI bin changed — flashing...")

command = [
    str(CUBEPROG_PATH),
    "-c", "port=SWD",
    "-el", str(LOADER_PATH),
    "-d", str(BIN_PATH), "0x90000000"
]

result = subprocess.run(command)

if result.returncode == 0:
    STAMP_PATH.write_text(current_hash)
    print("QSPI flash successful. Hash updated.")
else:
    print("QSPI flash failed.")
    sys.exit(result.returncode)
