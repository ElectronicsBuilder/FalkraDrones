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
 * @file    main_improved.py
 * @brief   Improved FlashFS Filesystem Browser with Professional UI
 */
"""
import customtkinter as ctk
from tkinter import filedialog, Menu
import tkinter as tk
from serial_interface import SerialInterface
from parser import build_framed_command, extract_framed_responses
import time
from datetime import datetime
import struct
import os


STX = 0x7E
ETX = 0x7F
CMD_BOOTSTART               = 0x03
CMD_COMMIT                  = 0x04
CMD_EXTMEM_INIT             = 0x05
CMD_GET_VERSION             = 0x01
CMD_JUMP_TO_APP             = 0x02
CMD_RESET                   = 0x06
CMD_TRANSPORT_SWITCH        = 0x07
CMD_TRANSPORT_MODE_SWITCH   = 0x08
CMD_JUMP_TO_BOOTLOADER      = 0x09
CMD_WIFI_DISCONNECT         = 0x0A
CMD_SERIAL_DISCONNECT       = 0x0B

FS_CMD_LIST                 = 0x10
FS_CMD_READ                 = 0x11
FS_CMD_WRITE                = 0x12
FS_CMD_DELETE               = 0x13
FS_CMD_FORMAT               = 0x14
FS_CMD_SYNC_TIME            = 0x15
FS_CMD_UPLOAD_START         = 0x16
FS_CMD_UPLOAD_CHUNK         = 0x17
FS_CMD_UPLOAD_END           = 0x18
FS_CMD_EXIT                 = 0x19
FS_LOG_CMD_LIST             = 0x20
FS_LOG_CMD_READ             = 0x21
FS_LOG_CMD_DELETE           = 0x22
FS_LOG_CMD_FORMAT           = 0x23

# Transport mode enum values
APP_TRANSPORT_MODE_CONSOLE = 0
APP_TRANSPORT_MODE_COMMAND = 1
APP_TRANSPORT_MODE_DATA = 2
APP_TRANSPORT_MODE_EXTMEM = 3
APP_TRANSPORT_MODE_FILESYSTEM_COMMAND = 4
APP_TRANSPORT_MODE_FILESYSTEM_DATA = 5


def crc16(data, seed=0xFFFF):
    """CRC16 calculation - same algorithm as bootloader"""
    table = [
        0x0000, 0x1189, 0x2312, 0x329B, 0x4624, 0x57AD, 0x6536, 0x74BF,
        0x8C48, 0x9DC1, 0xAF5A, 0xBED3, 0xCA6C, 0xDBE5, 0xE97E, 0xF8F7,
        0x0919, 0x1890, 0x2A0B, 0x3B82, 0x4F3D, 0x5EB4, 0x6C2F, 0x7DA6,
        0x8551, 0x94D8, 0xA643, 0xB7CA, 0xC375, 0xD2FC, 0xE067, 0xF1EE,
        0x1232, 0x03BB, 0x3120, 0x20A9, 0x5416, 0x459F, 0x7704, 0x668D,
        0x9E7A, 0x8FF3, 0xBD68, 0xACE1, 0xD85E, 0xC9D7, 0xFB4C, 0xEAC5,
        0x1B2B, 0x0AA2, 0x3839, 0x29B0, 0x5D0F, 0x4C86, 0x7E1D, 0x6F94,
        0x9763, 0x86EA, 0xB471, 0xA5F8, 0xD147, 0xC0CE, 0xF255, 0xE3DC,
        0x2464, 0x35ED, 0x0776, 0x16FF, 0x6240, 0x73C9, 0x4152, 0x50DB,
        0xA82C, 0xB9A5, 0x8B3E, 0x9AB7, 0xEE08, 0xFF81, 0xCD1A, 0xDC93,
        0x2D7D, 0x3CF4, 0x0E6F, 0x1FE6, 0x6B59, 0x7AD0, 0x484B, 0x59C2,
        0xA135, 0xB0BC, 0x8227, 0x93AE, 0xE711, 0xF698, 0xC403, 0xD58A,
        0x3656, 0x27DF, 0x1544, 0x04CD, 0x7072, 0x61FB, 0x5360, 0x42E9,
        0xBA1E, 0xAB97, 0x990C, 0x8885, 0xFC3A, 0xEDB3, 0xDF28, 0xCEA1,
        0x3F4F, 0x2EC6, 0x1C5D, 0x0DD4, 0x796B, 0x68E2, 0x5A79, 0x4BF0,
        0xB307, 0xA28E, 0x9015, 0x819C, 0xF523, 0xE4AA, 0xD631, 0xC7B8,
        0x48C8, 0x5941, 0x6BDA, 0x7A53, 0x0EEC, 0x1F65, 0x2DFE, 0x3C77,
        0xC480, 0xD509, 0xE792, 0xF61B, 0x82A4, 0x932D, 0xA1B6, 0xB03F,
        0x41D1, 0x5058, 0x62C3, 0x734A, 0x07F5, 0x167C, 0x24E7, 0x356E,
        0xCD99, 0xDC10, 0xEE8B, 0xFF02, 0x8BBD, 0x9A34, 0xA8AF, 0xB926,
        0x5AFA, 0x4B73, 0x79E8, 0x6861, 0x1CDE, 0x0D57, 0x3FCC, 0x2E45,
        0xD6B2, 0xC73B, 0xF5A0, 0xE429, 0x9096, 0x811F, 0xB384, 0xA20D,
        0x53E3, 0x426A, 0x70F1, 0x6178, 0x15C7, 0x044E, 0x36D5, 0x275C,
        0xDFAB, 0xCE22, 0xFCB9, 0xED30, 0x998F, 0x8806, 0xBA9D, 0xAB14,
        0x6CAC, 0x7D25, 0x4FBE, 0x5E37, 0x2A88, 0x3B01, 0x099A, 0x1813,
        0xE0E4, 0xF16D, 0xC3F6, 0xD27F, 0xA6C0, 0xB749, 0x85D2, 0x945B,
        0x65B5, 0x743C, 0x46A7, 0x572E, 0x2391, 0x3218, 0x0083, 0x110A,
        0xE9FD, 0xF874, 0xCAEF, 0xDB66, 0xAFD9, 0xBE50, 0x8CCB, 0x9D42,
        0x7E9E, 0x6F17, 0x5D8C, 0x4C05, 0x38BA, 0x2933, 0x1BA8, 0x0A21,
        0xF2D6, 0xE35F, 0xD1C4, 0xC04D, 0xB4F2, 0xA57B, 0x97E0, 0x8669,
        0x7787, 0x660E, 0x5495, 0x451C, 0x31A3, 0x202A, 0x12B1, 0x0338,
        0xFBCF, 0xEA46, 0xD8DD, 0xC954, 0xBDEB, 0xAC62, 0x9EF9, 0x8F70
    ]

    crc = seed
    for byte in data:
        tbl_idx = ((crc >> 8) ^ byte) & 0xFF
        crc = ((crc << 8) ^ table[tbl_idx]) & 0xFFFF
    return crc


class FlashFSBrowser(ctk.CTk):
    def __init__(self):
        super().__init__()

        # Window setup
        self.protocol("WM_DELETE_WINDOW", self.on_closing)
        self.title("FlashFS Filesystem Browser")
        self.geometry("1100x750")
        self.resizable(True, True)

        # State variables
        self.serial = None
        self.selected_file = None
        self.is_log_file = False
        self.debug_logging = False
        self.connected = False
        self.last_list_time = 0
        self.command_cooldown = 0.3
        self.progress_line_count = 0
        self.file_data = []  # Store file list data

        # Configure grid weight for resizing
        self.grid_rowconfigure(2, weight=1)
        self.grid_columnconfigure(0, weight=1)

        # Create UI components
        self.create_connection_panel()
        self.create_toolbar()
        self.create_main_content()
        self.create_status_bar()

        # Initialize
        self.update_ui_state()

    def create_connection_panel(self):
        """Create professional connection panel at top"""
        conn_frame = ctk.CTkFrame(self, height=80)
        conn_frame.grid(row=0, column=0, sticky="ew", padx=10, pady=(10, 0))
        conn_frame.grid_propagate(False)

        # Title
        title_label = ctk.CTkLabel(conn_frame, text="Connection Settings",
                                   font=ctk.CTkFont(size=14, weight="bold"))
        title_label.grid(row=0, column=0, columnspan=6, sticky="w", padx=10, pady=(8, 5))

        # Transport type
        ctk.CTkLabel(conn_frame, text="Transport:").grid(row=1, column=0, padx=(10, 5), pady=5, sticky="e")
        self.transport_var = ctk.StringVar(value='Serial')
        self.transport_menu = ctk.CTkSegmentedButton(conn_frame, variable=self.transport_var,
                                                     values=['Serial', 'Wi-Fi'],
                                                     command=self.on_transport_change,
                                                     width=180)
        self.transport_menu.grid(row=1, column=1, padx=5, pady=5, sticky="w")

        # Serial port
        from serial.tools import list_ports
        ports = [p.device for p in list_ports.comports()]

        ctk.CTkLabel(conn_frame, text="Port:").grid(row=1, column=2, padx=(15, 5), pady=5, sticky="e")
        self.port_var = ctk.StringVar(value=ports[0] if ports else 'COM14')
        self.port_menu = ctk.CTkOptionMenu(conn_frame, variable=self.port_var, values=ports, width=100)
        self.port_menu.grid(row=1, column=3, padx=5, pady=5, sticky="w")

        # Baud rate
        ctk.CTkLabel(conn_frame, text="Baud:").grid(row=1, column=4, padx=(15, 5), pady=5, sticky="e")
        self.baud_var = ctk.StringVar(value="3000000")
        self.baud_menu = ctk.CTkOptionMenu(conn_frame, variable=self.baud_var,
                                          values=["115200", "230400", "460800", "921600", "3000000"],
                                          width=110)
        self.baud_menu.grid(row=1, column=5, padx=5, pady=5, sticky="w")

        # WiFi fields (hidden by default)
        self.wifi_ip_label = ctk.CTkLabel(conn_frame, text="IP Address:")
        self.wifi_port_label = ctk.CTkLabel(conn_frame, text="Port:")
        self.wifi_ip_var = ctk.StringVar(value='192.168.1.250')
        self.wifi_port_var = ctk.StringVar(value='8080')
        self.wifi_ip_entry = ctk.CTkEntry(conn_frame, textvariable=self.wifi_ip_var, width=130)
        self.wifi_port_entry = ctk.CTkEntry(conn_frame, textvariable=self.wifi_port_var, width=80)

        # Connect button and status indicator
        self.connect_btn = ctk.CTkButton(conn_frame, text="Connect", width=100,
                                        command=self.toggle_connection)
        self.connect_btn.grid(row=1, column=6, padx=(20, 5), pady=5)

        self.status_indicator = ctk.CTkLabel(conn_frame, text="●", font=ctk.CTkFont(size=20),
                                            text_color="gray")
        self.status_indicator.grid(row=1, column=7, padx=(0, 10), pady=5)

    def create_toolbar(self):
        """Create toolbar with action buttons"""
        toolbar_frame = ctk.CTkFrame(self, height=50)
        toolbar_frame.grid(row=1, column=0, sticky="ew", padx=10, pady=5)
        toolbar_frame.grid_propagate(False)

        # Buttons in toolbar
        self.refresh_btn = ctk.CTkButton(toolbar_frame, text="🔄 Refresh", width=90,
                                        command=self.send_list_command)
        self.refresh_btn.pack(side="left", padx=5, pady=8)

        self.upload_btn = ctk.CTkButton(toolbar_frame, text="⬆ Upload", width=90,
                                       command=self.upload_file)
        self.upload_btn.pack(side="left", padx=5, pady=8)

        self.download_btn = ctk.CTkButton(toolbar_frame, text="⬇ Download", width=100,
                                         command=self.download_file)
        self.download_btn.pack(side="left", padx=5, pady=8)

        self.view_btn = ctk.CTkButton(toolbar_frame, text="👁 View", width=90,
                                     command=self.view_file)
        self.view_btn.pack(side="left", padx=5, pady=8)

        self.delete_btn = ctk.CTkButton(toolbar_frame, text="🗑 Delete", width=90,
                                       command=self.delete_file, fg_color="#8B0000")
        self.delete_btn.pack(side="left", padx=5, pady=8)

        # Separator
        sep = ctk.CTkLabel(toolbar_frame, text="|", text_color="gray")
        sep.pack(side="left", padx=10)

        self.sync_time_btn = ctk.CTkButton(toolbar_frame, text="🕐 Sync Time", width=100,
                                          command=self.sync_controller_time)
        self.sync_time_btn.pack(side="left", padx=5, pady=8)

        self.format_btn = ctk.CTkButton(toolbar_frame, text="⚠ Format", width=90,
                                       command=self.format_filesystem, fg_color="#8B4513")
        self.format_btn.pack(side="left", padx=5, pady=8)

        # Right side - debug toggle
        self.debug_btn = ctk.CTkButton(toolbar_frame, text="Debug: OFF", width=100,
                                      command=self.toggle_debug_logging)
        self.debug_btn.pack(side="right", padx=5, pady=8)

    def create_main_content(self):
        """Create main content area with file list and log"""
        content_frame = ctk.CTkFrame(self)
        content_frame.grid(row=2, column=0, sticky="nsew", padx=10, pady=5)

        # Configure content frame grid
        content_frame.grid_rowconfigure(0, weight=1)
        content_frame.grid_columnconfigure(0, weight=2)  # File list gets 2/3
        content_frame.grid_columnconfigure(1, weight=1)  # Log gets 1/3

        # Left panel - File list
        file_panel = ctk.CTkFrame(content_frame)
        file_panel.grid(row=0, column=0, sticky="nsew", padx=(5, 2.5), pady=5)
        file_panel.grid_rowconfigure(1, weight=1)
        file_panel.grid_columnconfigure(0, weight=1)

        # File list header
        file_header = ctk.CTkFrame(file_panel, height=35)
        file_header.grid(row=0, column=0, sticky="ew", padx=5, pady=(5, 0))
        file_header.grid_propagate(False)

        ctk.CTkLabel(file_header, text="Filesystem Contents",
                    font=ctk.CTkFont(size=13, weight="bold")).pack(side="left", padx=10, pady=5)

        self.file_count_label = ctk.CTkLabel(file_header, text="0 files",
                                             text_color="gray")
        self.file_count_label.pack(side="right", padx=10, pady=5)

        # File list scrollable frame
        self.file_list_frame = ctk.CTkScrollableFrame(file_panel)
        self.file_list_frame.grid(row=1, column=0, sticky="nsew", padx=5, pady=5)

        # Right panel - Log output
        log_panel = ctk.CTkFrame(content_frame)
        log_panel.grid(row=0, column=1, sticky="nsew", padx=(2.5, 5), pady=5)
        log_panel.grid_rowconfigure(1, weight=1)
        log_panel.grid_columnconfigure(0, weight=1)

        # Log header
        log_header = ctk.CTkFrame(log_panel, height=35)
        log_header.grid(row=0, column=0, sticky="ew", padx=5, pady=(5, 0))
        log_header.grid_propagate(False)

        ctk.CTkLabel(log_header, text="Activity Log",
                    font=ctk.CTkFont(size=13, weight="bold")).pack(side="left", padx=10, pady=5)

        clear_log_btn = ctk.CTkButton(log_header, text="Clear", width=60,
                                      command=self.clear_log)
        clear_log_btn.pack(side="right", padx=5, pady=5)

        # Log textbox
        self.output_box = ctk.CTkTextbox(log_panel, font=ctk.CTkFont(family="Consolas", size=10))
        self.output_box.grid(row=1, column=0, sticky="nsew", padx=5, pady=5)

    def create_status_bar(self):
        """Create status bar at bottom"""
        status_frame = ctk.CTkFrame(self, height=30)
        status_frame.grid(row=3, column=0, sticky="ew", padx=10, pady=(0, 10))
        status_frame.grid_propagate(False)

        self.status_label = ctk.CTkLabel(status_frame, text="Ready", anchor="w")
        self.status_label.pack(side="left", padx=10, pady=5)

        self.file_info_label = ctk.CTkLabel(status_frame, text="No file selected",
                                           text_color="gray", anchor="e")
        self.file_info_label.pack(side="right", padx=10, pady=5)

    def update_ui_state(self):
        """Update UI state based on connection status"""
        state = "normal" if self.connected else "disabled"

        self.refresh_btn.configure(state=state)
        self.upload_btn.configure(state=state)
        self.sync_time_btn.configure(state=state)
        self.format_btn.configure(state=state)

        # File-specific buttons
        file_state = "normal" if (self.connected and self.selected_file) else "disabled"
        self.download_btn.configure(state=file_state)
        self.delete_btn.configure(state=file_state)
        self.view_btn.configure(state=file_state)

        # Update status indicator
        if self.connected:
            self.status_indicator.configure(text_color="green")
            self.status_label.configure(text=f"Connected - {self.transport_var.get()} Mode")
        else:
            self.status_indicator.configure(text_color="gray")
            self.status_label.configure(text="Disconnected")

    # === Helper Methods ===

    def log(self, msg):
        """Add message to log"""
        timestamp = datetime.now().strftime("%H:%M:%S")
        self.output_box.insert("end", f"[{timestamp}] {msg}\n")
        self.output_box.see("end")

    def clear_log(self):
        """Clear the log"""
        self.output_box.delete("1.0", "end")
        self.log("Log cleared")

    def is_wifi_transport(self):
        """Return True when Wi-Fi/TCP transport is active"""
        return self.transport_var.get() == 'Wi-Fi'

    def get_transport_timeouts(self, wifi_first=2.5, wifi_sub=1.0, uart_first=0.3, uart_sub=0.2):
        """Helper to derive transport-aware timeout pairs"""
        is_wifi = self.is_wifi_transport()
        if is_wifi:
            return is_wifi, wifi_first, wifi_sub
        return is_wifi, uart_first, uart_sub

    def create_progress_bar(self, percent, width=20):
        """Create a visual progress bar string"""
        filled = int((percent / 100) * width)
        bar = "█" * filled + "░" * (width - filled)
        return f"[{bar}]"

    def update_progress_bar(self, percent, bytes_sent, file_size, filename):
        """Update progress bar in place"""
        progress_bar = self.create_progress_bar(percent)
        progress_text = f"[UPLOAD] {progress_bar} {percent}% ({bytes_sent}/{file_size} bytes) - {filename}"

        # Remove the last progress line and add new one
        if self.progress_line_count > 0:
            # Get current content
            content = self.output_box.get("1.0", "end-1c")
            lines = content.split('\n')
            # Remove last line and rejoin
            if lines:
                lines = lines[:-1]
                new_content = '\n'.join(lines)
                self.output_box.delete("1.0", "end")
                self.output_box.insert("1.0", new_content + '\n')

        self.log(progress_text)
        self.progress_line_count = 1

    def toggle_debug_logging(self):
        """Toggle debug logging on/off"""
        self.debug_logging = not self.debug_logging
        self.debug_btn.configure(text=f"Debug: {'ON' if self.debug_logging else 'OFF'}")
        self.log(f"Debug logging {'enabled' if self.debug_logging else 'disabled'}")

    # === Connection Methods ===

    def on_transport_change(self, value):
        """Handle transport mode change"""
        if value == 'Wi-Fi':
            # Hide Serial fields
            self.port_menu.grid_remove()
            self.baud_menu.grid_remove()
            # Show WiFi fields
            self.wifi_ip_label.grid(row=1, column=2, padx=(15, 5), pady=5, sticky="e")
            self.wifi_ip_entry.grid(row=1, column=3, padx=5, pady=5, sticky="w")
            self.wifi_port_label.grid(row=1, column=4, padx=(15, 5), pady=5, sticky="e")
            self.wifi_port_entry.grid(row=1, column=5, padx=5, pady=5, sticky="w")
        else:
            # Hide WiFi fields
            self.wifi_ip_label.grid_remove()
            self.wifi_ip_entry.grid_remove()
            self.wifi_port_label.grid_remove()
            self.wifi_port_entry.grid_remove()
            # Show Serial fields
            self.port_menu.grid()
            self.baud_menu.grid()

    def toggle_connection(self):
        """Toggle between connect and disconnect"""
        if self.connected:
            self.disconnect_serial()
        else:
            self.connect_serial()

    def connect_serial(self):
        """Connect to device"""
        transport = self.transport_var.get()
        try:
            if transport == 'Serial':
                port = self.port_var.get()
                baud = int(self.baud_var.get())
                self.serial = SerialInterface(port, baud)
                self.log(f"Connected to {port} at {baud} baud")
            else:
                from tcp_interface import TCPInterface
                host = self.wifi_ip_var.get()
                port = int(self.wifi_port_var.get())
                self.serial = TCPInterface(host, port)
                self.log(f"Connected to {host}:{port} via TCP")

            # Switch to filesystem mode
            if transport == 'Wi-Fi':
                mode_switch_packet = build_framed_command(CMD_TRANSPORT_MODE_SWITCH,
                                                         bytes([APP_TRANSPORT_MODE_FILESYSTEM_COMMAND]))
                self.serial.send_bytes(mode_switch_packet)
                self.log("Sent transport mode switch to FILESYSTEM_COMMAND")
                time.sleep(0.2)
            else:
                self.serial.send_raw_command("jumpToFilesystem\n")
                time.sleep(0.1)

            # Clear buffer
            try:
                self.serial.ser.read(1024)
            except Exception:
                pass

            # Sync time
            time.sleep(0.1)
            self.sync_controller_time()

            # Update state
            self.connected = True
            self.connect_btn.configure(text="Disconnect", fg_color="#8B0000")
            self.transport_menu.configure(state="disabled")
            self.port_menu.configure(state="disabled")
            self.baud_menu.configure(state="disabled")

            self.update_ui_state()

            # Auto-refresh file list
            time.sleep(0.2)
            self.send_list_command()

        except Exception as e:
            self.log(f"[ERROR] Failed to connect: {e}")

    def disconnect_serial(self):
        """Disconnect from device"""
        try:
            if self.serial and hasattr(self.serial, 'ser'):
                self.log("Exiting filesystem mode...")
                packet = build_framed_command(FS_CMD_EXIT)
                try:
                    self.serial.send_bytes(packet)
                except Exception:
                    pass

                time.sleep(0.3)

                # Flush buffers
                try:
                    if hasattr(self.serial, 'ser') and self.serial.ser and self.serial.ser.is_open:
                        for _ in range(3):
                            time.sleep(0.1)
                            in_waiting = self.serial.ser.in_waiting
                            if in_waiting > 0:
                                self.serial.ser.read(in_waiting)
                except Exception:
                    pass

                try:
                    self.serial.close()
                except Exception:
                    pass
                self.log("Disconnected from serial port")

            # Update state
            self.connected = False
            self.serial = None
            self.selected_file = None

            self.connect_btn.configure(text="Connect", fg_color=["#3B8ED0", "#1F6AA5"])
            self.transport_menu.configure(state="normal")
            self.port_menu.configure(state="normal")
            self.baud_menu.configure(state="normal")

            # Clear file list
            for widget in self.file_list_frame.winfo_children():
                widget.destroy()
            self.file_data = []
            self.file_count_label.configure(text="0 files")

            self.update_ui_state()

        except Exception as e:
            self.log(f"[ERROR] Failed to disconnect: {e}")

    # === Filesystem Methods ===

    def send_list_command(self):
        """List filesystem contents"""
        if not self.serial:
            return

        try:
            self.refresh_btn.configure(state="disabled")
            self.update()

            # Rate limiting
            now = time.time()
            time_since_last = now - self.last_list_time
            if time_since_last < self.command_cooldown:
                time.sleep(self.command_cooldown - time_since_last)

            # Flush stale data
            time.sleep(0.05)
            stale_data = self.serial.ser.read(8192)
            if stale_data and self.debug_logging:
                self.log(f"[DEBUG] Flushed {len(stale_data)} stale bytes")

            self.log("Listing filesystem contents...")
            packet = build_framed_command(FS_CMD_LIST)
            self.serial.send_bytes(packet)
            self.last_list_time = time.time()

            # Progressive reading
            raw_response = b''
            read_attempts = 0
            max_attempts = 100

            is_wifi, first_data_timeout, subsequent_timeout = self.get_transport_timeouts(
                wifi_first=3.5, wifi_sub=1.5, uart_first=0.3, uart_sub=0.2
            )

            last_data_time = time.time()
            first_byte_received = False

            while read_attempts < max_attempts:
                new_data = self.serial.ser.read(4096)
                if new_data:
                    if not first_byte_received:
                        first_byte_received = True
                    raw_response += new_data
                    last_data_time = time.time()

                timeout = subsequent_timeout if first_byte_received else first_data_timeout
                elapsed = time.time() - last_data_time
                if elapsed > timeout:
                    break

                read_attempts += 1
                time.sleep(0.2)

            lines = extract_framed_responses(raw_response)
            self.log(f"Received {len(lines)} entries")
            self.display_files(lines)

        finally:
            self.refresh_btn.configure(state="normal" if self.connected else "disabled")

    def display_files(self, lines):
        """Display files in list"""
        # Clear existing
        for widget in self.file_list_frame.winfo_children():
            widget.destroy()

        self.file_data = []
        file_count = 0

        for line in lines:
            if line.startswith("[FILE]"):
                # Parse file info
                parts = line[7:].split(" (")
                name = parts[0].strip()
                size_info = parts[1].rstrip(")") if len(parts) > 1 else "unknown size"

                # Determine file type and icon
                if name.endswith(".log"):
                    icon = "📄"
                    file_type = "LOG"
                    color = "#90EE90"  # Light green
                elif name.endswith(".txt"):
                    icon = "📝"
                    file_type = "TXT"
                    color = "#87CEEB"  # Sky blue
                elif name.endswith(".wav"):
                    icon = "🔊"
                    file_type = "WAV"
                    color = "#DDA0DD"  # Plum
                elif name.endswith((".dat", ".bin")):
                    icon = "🔢"
                    file_type = "BIN"
                    color = "#FFA07A"  # Light salmon
                else:
                    icon = "📋"
                    file_type = "FILE"
                    color = "#D3D3D3"  # Light gray

                # Store file data
                self.file_data.append({"name": name, "size": size_info, "type": file_type})
                file_count += 1

                # Create file entry button
                file_frame = ctk.CTkFrame(self.file_list_frame, fg_color="transparent")
                file_frame.pack(fill="x", padx=5, pady=2)

                btn = ctk.CTkButton(file_frame, text=f"{icon} {name}",
                                   anchor="w", width=300,
                                   command=lambda n=name: self.select_file(n))
                btn.pack(side="left", fill="x", expand=True)

                size_label = ctk.CTkLabel(file_frame, text=size_info,
                                         text_color="gray", width=100)
                size_label.pack(side="right", padx=5)

            elif line.startswith("[DIR]"):
                name = line[6:].strip()
                label = ctk.CTkLabel(self.file_list_frame, text=f"📁 {name}",
                                    anchor="w", font=ctk.CTkFont(weight="bold"))
                label.pack(fill="x", padx=5, pady=2)

        self.file_count_label.configure(text=f"{file_count} file{'s' if file_count != 1 else ''}")

    def select_file(self, name):
        """Select a file"""
        self.selected_file = name
        self.is_log_file = name.endswith(".log")

        # Determine file type
        if self.is_log_file:
            file_type = "LOG FILE"
        elif name.endswith(".txt"):
            file_type = "TEXT FILE"
        elif name.endswith((".dat", ".bin")):
            file_type = "BINARY FILE"
        elif name.endswith(".wav"):
            file_type = "AUDIO FILE"
        else:
            file_type = "DATA FILE"

        self.file_info_label.configure(text=f"{file_type}: {name}")
        self.log(f"Selected {file_type.lower()}: {name}")
        self.update_ui_state()

    def upload_file(self):
        """Upload file to filesystem"""
        if not self.serial:
            self.log("[ERROR] Not connected to controller")
            return

        # Open file dialog to select binary file
        file_path = filedialog.askopenfilename(
            title="Select Binary File to Upload",
            filetypes=[
                ("Audio files", "*.wav"),
                ("Binary files", "*.bin"),
                ("Data files", "*.dat"),
                ("Text files", "*.txt"),
                ("All files", "*.*")
            ]
        )

        if not file_path:
            return

        filename = os.path.basename(file_path)

        try:
            actual_file_size = os.path.getsize(file_path)

            with open(file_path, 'rb') as f:
                file_data = f.read()

            file_size = len(file_data)
            self.log(f"[DEBUG] File on disk: {actual_file_size} bytes")
            self.log(f"[DEBUG] Data read: {file_size} bytes")
            self.log(f"[INFO] Uploading {filename} ({file_size} bytes)")

            # Start chunked upload
            self.upload_binary_file(filename, file_data)

        except Exception as e:
            self.log(f"[ERROR] Failed to read file: {e}")

    def upload_binary_file(self, filename, file_data):
        """Upload binary file using chunked protocol"""
        try:
            # Disable upload button during upload
            self.upload_btn.configure(state="disabled")
            self.progress_line_count = 0  # Reset progress tracking

            file_size = len(file_data)
            chunk_size = 1024  # Data chunk size (will add 2 CRC bytes = 1026 total)

            # Step 1: Send upload start command
            start_data = filename.encode() + b'\x00'  # Null-terminated filename
            start_data += file_size.to_bytes(4, 'little')  # File size (little-endian)

            self.log(f"[UPLOAD] Starting upload: {filename} ({file_size} bytes)")

            packet = build_framed_command(FS_CMD_UPLOAD_START, start_data)
            self.serial.send_bytes(packet)

            # Wait and retry reading response (longer for slow Wi‑Fi/NCP)
            time.sleep(0.2)  # Initial wait time
            # Read response with multiple attempts and longer timeout
            responses = []
            # Increase attempts and use a larger read buffer to tolerate delays
            for attempt in range(25):  # Try up to ~5 seconds (25 * 0.2s)
                raw = self.serial.ser.read(1024)  # Larger read buffer
                if raw:
                    new_responses = extract_framed_responses(raw)
                    responses.extend(new_responses)
                    if any("Upload started" in r for r in new_responses):
                        break
                # Pause between attempts to allow controller processing and flash IO
                time.sleep(0.2)
            # Debug: Show all responses received
            self.log(f"[DEBUG] Received {len(responses)} responses:")
            for i, resp in enumerate(responses):
                self.log(f"[DEBUG]   {i+1}: {resp}")

            # Check for success response
            upload_started = any("Upload started" in r for r in responses)
            if not upload_started:
                self.log("[ERROR] Upload start failed - no success response")
                # Check for error responses
                error_responses = [r for r in responses if "ERROR" in r or "Invalid" in r or "Failed" in r]
                if error_responses:
                    for err in error_responses:
                        self.log(f"[CONTROLLER ERROR] {err}")
                return False

            self.log("[SUCCESS] Upload start confirmed - controller in data mode")

            # Step 2: Send file data as raw chunks (no framing, controller is in data mode)
            bytes_sent = 0
            chunk_num = 0

            while bytes_sent < file_size:
                chunk_end = min(bytes_sent + chunk_size, file_size)
                chunk_data = file_data[bytes_sent:chunk_end]

                # Calculate CRC16 for chunk data
                crc_value = crc16(chunk_data)
                crc_bytes = struct.pack('>H', crc_value)  # Big-endian 16-bit CRC
                chunk_with_crc = chunk_data + crc_bytes

                # Show detailed chunk information only when debug logging enabled
                if self.debug_logging:
                    self.log(f"[CHUNK {chunk_num:2d}] Sending: {len(chunk_data)} data + 2 CRC = {len(chunk_with_crc)} bytes total")
                    self.log(f"[CHUNK {chunk_num:2d}] Data range: bytes {bytes_sent}-{chunk_end-1}, CRC: 0x{crc_value:04X}")
                    self.log(f"[CHUNK {chunk_num:2d}] First 16 bytes: {chunk_data[:16].hex()}")
                    self.log(f"[CHUNK {chunk_num:2d}] CRC bytes: {crc_bytes.hex()}")

                # Send chunk and wait for ACK
                self.serial.ser.write(chunk_with_crc)
                self.serial.ser.flush()

                # Wait for chunk ACK with optimized timing
                raw_response = b''
                # Reduced initial wait since the root cause is fixed
                time.sleep(0.1)  # Reduced initial wait for flash write operation

                for read_attempt in range(6):  # Fewer attempts needed
                    new_data = self.serial.ser.read(512)
                    if new_data:
                        raw_response += new_data
                        # Check if we have a complete ACK frame (9 bytes: STX + LEN + CODE + OFFSET + CRC + ETX)
                        if len(raw_response) >= 9:
                            # Look for ACK frame pattern anywhere in response
                            for i in range(len(raw_response) - 8):
                                if (raw_response[i] == 0x7E and
                                    raw_response[i+1] == 0x06 and  # LEN = 6
                                    raw_response[i+8] == 0x7F):     # ETX
                                    break  # Found complete ACK frame
                            else:
                                continue  # No complete ACK found yet
                            break  # Found complete ACK, exit read loop

                    time.sleep(0.05)  # Reduced wait between read attempts

                # Debug: Show raw response only when debug logging enabled
                if self.debug_logging:
                    if raw_response:
                        self.log(f"[CHUNK {chunk_num:2d}] Raw response ({len(raw_response)} bytes): {raw_response.hex()[:100]}{'...' if len(raw_response) > 50 else ''}")
                    else:
                        self.log(f"[CHUNK {chunk_num:2d}]  No response data received")

                # Parse ACK response - find ACK frame anywhere in the response
                ack_received = False
                if raw_response:
                    # Look for ACK frame pattern: 7E 06 00 XX XX XX XX XX 7F (9 bytes total)
                    for i in range(len(raw_response) - 8):
                        if (raw_response[i] == 0x7E and
                            raw_response[i+1] == 0x06 and  # LEN = 6 (1 code + 4 offset + 1 crc)
                            raw_response[i+8] == 0x7F):     # ETX at position i+8

                            try:
                                ack_code = raw_response[i+2]  # 0=OK, 1=ERR, 2=INVALID, 3=DONE
                                offset = struct.unpack('<I', raw_response[i+3:i+7])[0]  # Little-endian to match controller

                                if ack_code == 0:  # APP_REPLY_CHUNK_OK
                                    ack_received = True
                                    if self.debug_logging:
                                        self.log(f"[CHUNK {chunk_num:2d}]  ACK received: OK, offset={offset}")
                                elif ack_code == 1:  # APP_REPLY_CHUNK_ERR
                                    self.log(f"[CHUNK {chunk_num:2d}]  ACK received: CRC ERROR - controller rejected chunk")
                                    return False
                                elif ack_code == 3:  # APP_REPLY_CHUNK_DONE
                                    ack_received = True
                                    if self.debug_logging:
                                        self.log(f"[CHUNK {chunk_num:2d}]  ACK received: DONE, offset={offset}")
                                else:
                                    if self.debug_logging:
                                        self.log(f"[CHUNK {chunk_num:2d}]  ACK received: unexpected code {ack_code}, offset={offset}")
                                break  # Found valid ACK, stop searching
                            except Exception as e:
                                self.log(f"[ERROR] Failed to parse ACK for chunk {chunk_num}: {e}")

                if not ack_received:
                    if self.debug_logging:
                        self.log(f"[CHUNK {chunk_num:2d}]  No valid ACK received - continuing anyway")

                bytes_sent = chunk_end
                chunk_num += 1

                # Show progress with visual bar (update every chunk)
                percent = (bytes_sent * 100) // file_size
                self.update_progress_bar(percent, bytes_sent, file_size, filename)

                # Process UI events to keep app responsive
                self.update()  # Process pending UI events

                # Add minimal inter-chunk delay
                if bytes_sent < file_size:  # Don't delay after last chunk
                    time.sleep(0.02)  # Further reduced delay for better performance

            # Step 3: Wait for controller to switch back to command mode, then send upload end
            self.log("[UPLOAD] All data sent, waiting for controller to switch to command mode...")
            time.sleep(0.5)  # Restore wait time for mode switch - this is critical

            packet = build_framed_command(FS_CMD_UPLOAD_END, b'')
            self.serial.send_bytes(packet)
            time.sleep(0.3)  # Restore wait for end response - file table serialization takes time

            # Check final response with multiple retries
            responses = []
            raw_all = b''
            for attempt in range(5):  # Try multiple times to get the response
                raw = self.serial.ser.read(1024)  # Larger buffer
                if raw:
                    raw_all += raw
                    new_responses = extract_framed_responses(raw)
                    responses.extend(new_responses)
                    if any("Upload complete" in r for r in new_responses):
                        break
                time.sleep(0.2)  # Wait between attempts

            # Debug: Show all responses received
            self.log(f"[DEBUG] Received {len(responses)} end responses")
            success = False
            if len(responses) == 0 and raw_all:
                # No parsed textual responses, show raw hex to help debugging
                hex_preview = raw_all.hex()[:200]
                self.log(f"[DEBUG] Raw end-response bytes: {hex_preview}{'...' if len(raw_all.hex())>200 else ''}")
            for resp in responses:
                self.log(f"[CONTROLLER] {resp}")
                if "Upload complete" in resp:
                    success = True

            if success:
                self.log(f"[SUCCESS] Upload completed: {filename}")
                # Refresh file list to show new file
                self.send_list_command()
            else:
                self.log(f"[ERROR] Upload may have failed")

        except Exception as e:
            self.log(f"[ERROR] Upload failed: {e}")
        finally:
            # Re-enable upload button
            self.upload_btn.configure(state="normal" if self.connected else "disabled")

    def download_file(self):
        """Download file from filesystem"""
        if not self.serial or not self.selected_file:
            return

        # Ask user where to save the file
        save_path = filedialog.asksaveasfilename(
            title="Save File As",
            initialfile=self.selected_file,
            defaultextension=".*",
            filetypes=[("All files", "*.*")]
        )

        if not save_path:
            return

        self.log(f"[INFO] Downloading {self.selected_file}")
        packet = build_framed_command(FS_CMD_READ, self.selected_file.encode())
        self.serial.send_bytes(packet)

        # Progressive reading with adaptive timeouts for WiFi vs UART
        raw = b''
        read_attempts = 0
        max_attempts = 60  # Increased for WiFi tolerance

        # Transport-specific timeouts
        is_wifi, first_data_timeout, subsequent_timeout = self.get_transport_timeouts(
            wifi_first=3.0, wifi_sub=1.2, uart_first=0.3, uart_sub=0.3
        )

        last_data_time = time.time()
        first_byte_received = False

        if self.debug_logging:
            self.log(f"[DEBUG] Transport: {'WiFi' if is_wifi else 'UART'}, first_timeout={first_data_timeout}s, subsequent_timeout={subsequent_timeout}s")

        while read_attempts < max_attempts:
            new_data = self.serial.ser.read(8192)
            if new_data:
                if not first_byte_received:
                    first_byte_received = True
                    elapsed = time.time() - last_data_time
                    if self.debug_logging:
                        self.log(f"[DEBUG] First data after {elapsed:.3f}s")
                raw += new_data
                last_data_time = time.time()

            # Use different timeout based on reception state
            timeout = subsequent_timeout if first_byte_received else first_data_timeout
            elapsed = time.time() - last_data_time
            if elapsed > timeout:
                if self.debug_logging:
                    self.log(f"[DEBUG] Timeout after {elapsed:.3f}s ({'subsequent' if first_byte_received else 'initial'})")
                break

            read_attempts += 1
            time.sleep(0.1)

        lines = extract_framed_responses(raw)

        try:
            with open(save_path, "w") as f:
                for line in lines:
                    f.write(line + "\n")
            self.log(f"[SUCCESS] Saved to: {save_path} ({len(lines)} lines)")
        except Exception as e:
            self.log(f"[ERROR] Failed to save file: {e}")

    def view_file(self):
        """View file contents"""
        if not self.serial or not self.selected_file:
            return

        self.log(f"[INFO] Viewing: {self.selected_file}")
        packet = build_framed_command(FS_CMD_READ, self.selected_file.encode())
        self.serial.send_bytes(packet)

        # Progressive reading with optimized timing
        self.log("[INFO] Reading file data...")
        raw_data = b''
        read_attempts = 0
        max_attempts = 60  # Reduced to 30 seconds total (500ms each)
        last_data_time = time.time()

        while read_attempts < max_attempts:
            # Read available data
            new_data = self.serial.ser.read(8192)  # Larger chunks for efficiency
            if new_data:
                raw_data += new_data
                last_data_time = time.time()
                if self.debug_logging:
                    self.log(f"[DEBUG] Read {len(new_data)} bytes, total: {len(raw_data)} bytes")

            # Reduced timeout - stop if no data for 2 seconds
            read_timeout = 0.1
            if time.time() - last_data_time > read_timeout:
                self.log(f"[INFO] No data received for {read_timeout} seconds, processing {len(raw_data)} bytes...")
                break

            read_attempts += 1
            time.sleep(0.3)  # Reduced wait time between reads

            # Process UI events to keep app responsive
            self.update()

        self.log(f"[INFO] Total data received: {len(raw_data)} bytes in {read_attempts} attempts")
        lines = extract_framed_responses(raw_data)

        self.log(f"[VIEW] --- {self.selected_file} --- ({len(lines)} lines)")
        for line in lines[:10]:  # Show first 10 lines in log
            self.log("  " + line)
        if len(lines) > 10:
            self.log(f"  ... and {len(lines) - 10} more lines")

        popup = ctk.CTkToplevel(self)
        popup.title(f"View: {self.selected_file} ({len(lines)} lines)")
        popup.geometry("800x500")
        text_area = ctk.CTkTextbox(popup)
        text_area.pack(expand=True, fill="both", padx=10, pady=10)
        for line in lines:
            text_area.insert("end", line + "\n")
        text_area.configure(state="disabled")

    def delete_file(self):
        """Delete file"""
        if not self.serial or not self.selected_file:
            return

        # Confirmation dialog
        import tkinter.messagebox as msgbox
        if not msgbox.askyesno("Confirm Delete",
                               f"Are you sure you want to delete '{self.selected_file}'?\n\nThis action cannot be undone!"):
            return

        self.log(f"[INFO] Deleting {self.selected_file}")
        packet = build_framed_command(FS_CMD_DELETE, self.selected_file.encode())
        self.serial.send_bytes(packet)

        # Progressive reading with adaptive timeouts for WiFi vs UART
        raw = b''
        read_attempts = 0
        max_attempts = 60  # Increased for WiFi tolerance

        # Transport-specific timeouts
        is_wifi, first_data_timeout, subsequent_timeout = self.get_transport_timeouts(
            wifi_first=2.5, wifi_sub=0.8, uart_first=0.3, uart_sub=0.3
        )

        last_data_time = time.time()
        first_byte_received = False

        if self.debug_logging:
            self.log(f"[DEBUG] Transport: {'WiFi' if is_wifi else 'UART'}, first_timeout={first_data_timeout}s, subsequent_timeout={subsequent_timeout}s")

        while read_attempts < max_attempts:
            new_data = self.serial.ser.read(512)
            if new_data:
                if not first_byte_received:
                    first_byte_received = True
                    elapsed = time.time() - last_data_time
                    if self.debug_logging:
                        self.log(f"[DEBUG] First data after {elapsed:.3f}s")
                raw += new_data
                last_data_time = time.time()
                # Check for completion indicator
                if b'deleted' in raw.lower() or b'ERROR' in raw:
                    break

            # Use different timeout based on reception state
            timeout = subsequent_timeout if first_byte_received else first_data_timeout
            elapsed = time.time() - last_data_time
            if elapsed > timeout:
                if self.debug_logging:
                    self.log(f"[DEBUG] Timeout after {elapsed:.3f}s ({'subsequent' if first_byte_received else 'initial'})")
                break

            read_attempts += 1
            time.sleep(0.05)

        lines = extract_framed_responses(raw)

        for line in lines:
            self.log(f"[DELETE] {line}")

        # Clear selected file
        self.selected_file = None
        self.update_ui_state()

        # Refresh file list
        time.sleep(0.1)  # Small delay before refresh
        self.send_list_command()

    def format_filesystem(self):
        """Format filesystem"""
        if not self.serial:
            return

        # Confirmation dialog
        import tkinter.messagebox as msgbox
        if not msgbox.askyesno("Confirm Format",
                               "Are you sure you want to format the entire filesystem?\n\nThis will DELETE ALL FILES permanently!"):
            return

        self.log("[INFO] Formatting filesystem...")
        packet = build_framed_command(FS_CMD_FORMAT)
        self.serial.send_bytes(packet)

        # Progressive reading with adaptive timeouts for WiFi vs UART
        raw = b''
        read_attempts = 0
        max_attempts = 60  # Increased for WiFi tolerance

        # Transport-specific timeouts
        is_wifi, first_data_timeout, subsequent_timeout = self.get_transport_timeouts(
            wifi_first=4.0, wifi_sub=1.5, uart_first=0.5, uart_sub=0.3
        )

        last_data_time = time.time()
        first_byte_received = False

        if self.debug_logging:
            self.log(f"[DEBUG] Transport: {'WiFi' if is_wifi else 'UART'}, first_timeout={first_data_timeout}s, subsequent_timeout={subsequent_timeout}s")

        while read_attempts < max_attempts:
            new_data = self.serial.ser.read(1024)
            if new_data:
                if not first_byte_received:
                    first_byte_received = True
                    elapsed = time.time() - last_data_time
                    if self.debug_logging:
                        self.log(f"[DEBUG] First data after {elapsed:.3f}s")
                raw += new_data
                last_data_time = time.time()
                # Check for completion indicator
                if b'formatted' in raw.lower() or b'complete' in raw.lower():
                    time.sleep(0.1)  # Small delay to catch any trailing responses
                    break

            # Use different timeout based on reception state
            timeout = subsequent_timeout if first_byte_received else first_data_timeout
            elapsed = time.time() - last_data_time
            if elapsed > timeout:
                if self.debug_logging:
                    self.log(f"[DEBUG] Timeout after {elapsed:.3f}s ({'subsequent' if first_byte_received else 'initial'})")
                break

            read_attempts += 1
            time.sleep(0.1)

        lines = extract_framed_responses(raw)

        for line in lines:
            self.log(f"[FORMAT] {line}")

        # Refresh file list
        time.sleep(0.2)  # Delay before refresh to ensure format is complete
        self.send_list_command()

    def sync_controller_time(self):
        """Synchronize controller RTC with PC time"""
        if not self.serial:
            return

        try:
            pc_time = datetime.now()
            time_str = pc_time.strftime("%Y-%m-%dT%H:%M:%S")

            self.log(f"Syncing controller time to: {time_str}")

            packet = build_framed_command(FS_CMD_SYNC_TIME, time_str.encode())
            self.serial.send_bytes(packet)

            # Read response
            raw_response = b''
            is_wifi, first_timeout, sub_timeout = self.get_transport_timeouts(
                wifi_first=2.5, wifi_sub=0.8, uart_first=0.3, uart_sub=0.2
            )

            last_data_time = time.time()
            first_byte_received = False
            read_attempts = 0

            while read_attempts < 60:
                new_data = self.serial.ser.read(512)
                if new_data:
                    if not first_byte_received:
                        first_byte_received = True
                    raw_response += new_data
                    last_data_time = time.time()
                    if b'TIME UPDATED' in raw_response or b'ERROR' in raw_response:
                        break

                timeout = sub_timeout if first_byte_received else first_timeout
                if time.time() - last_data_time > timeout:
                    break

                read_attempts += 1
                time.sleep(0.05)

            lines = extract_framed_responses(raw_response)
            for line in lines:
                if line.startswith("TIME UPDATED"):
                    self.log(f"✓ {line}")
                else:
                    self.log(line)

        except Exception as e:
            self.log(f"Time sync failed: {e}")

    def on_closing(self):
        """Handle application closing"""
        if self.connected:
            self.disconnect_serial()
        self.destroy()


if __name__ == "__main__":
    ctk.set_appearance_mode("dark")
    ctk.set_default_color_theme("blue")
    app = FlashFSBrowser()
    app.mainloop()