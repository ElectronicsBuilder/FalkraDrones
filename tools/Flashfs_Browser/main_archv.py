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
 * @file    main.py
 * @brief   Main Application for Flashfs Browser
 */
"""
import customtkinter as ctk
from tkinter import filedialog
from serial_interface import SerialInterface
from parser import build_framed_command, extract_framed_responses
import time
from datetime import datetime
import struct


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
FS_CMD_SYNC_TIME            = 0x15  # New command for time synchronization
FS_CMD_UPLOAD_START         = 0x16  # Start binary file upload
FS_CMD_UPLOAD_CHUNK         = 0x17  # Upload file chunk
FS_CMD_UPLOAD_END           = 0x18  # End binary file upload
FS_CMD_EXIT                 = 0x19  # Exit filesystem mode
FS_LOG_CMD_LIST             = 0x20
FS_LOG_CMD_READ             = 0x21
FS_LOG_CMD_DELETE           = 0x22
FS_LOG_CMD_FORMAT           = 0x23

# Transport mode enum values (must match AppTransportMode in firmware)
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

class FlashfsFSApp(ctk.CTk):
    def __init__(self):
        super().__init__()
        
        # Handle window close event
        self.protocol("WM_DELETE_WINDOW", self.on_closing)
        self.title("Flashfs Filesystem Browser")
        self.geometry("650x700")
        self.resizable(False, False)
        self.serial = None
        self.selected_file = None
        self.is_log_file = False
        self.debug_logging = False  # Toggle for verbose logging
        self.progress_line_count = 0  # Track progress bar updates

        # Command rate limiting to prevent duplicate responses
        self.last_list_time = 0
        self.command_cooldown = 0.3  # Minimum 300ms between list commands

        # === Header ===
        header_frame = ctk.CTkFrame(self)
        header_frame.pack(fill="x", padx=10, pady=(10, 5))

        self.port_var = ctk.StringVar(value='COM14')
        self.baud_var = ctk.StringVar(value="3000000")

        from serial.tools import list_ports
        ports = [p.device for p in list_ports.comports()]

        # Transport selection (Serial or Wi-Fi)
        self.transport_var = ctk.StringVar(value='Serial')
        self.transport_menu = ctk.CTkOptionMenu(header_frame, variable=self.transport_var, values=['Serial', 'Wi-Fi'], command=self.on_transport_change)
        self.transport_menu.grid(row=0, column=0, padx=5)

        self.port_var = ctk.StringVar(value='COM14')
        self.port_menu = ctk.CTkOptionMenu(header_frame, variable=self.port_var, values=ports)
        self.port_menu.grid(row=0, column=1, padx=5)

        self.baud_menu = ctk.CTkOptionMenu(header_frame, variable=self.baud_var, values=["9600", "57600", "115200", "230400", "460800", "921600", "3000000"])
        self.baud_menu.grid(row=0, column=2, padx=5)

        # Wi-Fi fields (hidden by default)
        self.wifi_ip_var = ctk.StringVar(value='192.168.1.250')
        self.wifi_port_var = ctk.StringVar(value='8080')
        self.wifi_ip_entry = ctk.CTkEntry(header_frame, textvariable=self.wifi_ip_var)
        self.wifi_port_entry = ctk.CTkEntry(header_frame, textvariable=self.wifi_port_var)

        self.connect_btn = ctk.CTkButton(header_frame, text="Connect", command=self.toggle_connection)
        self.connect_btn.grid(row=0, column=3, padx=5)
        self.connected = False  # Track connection state
        self.default_btn_color = self.connect_btn.cget("fg_color")  # Store default color

        #self.refresh_btn = ctk.CTkButton(header_frame, text="Refresh Files", command=self.send_list_command, state="disabled")
        #self.refresh_btn.grid(row=0, column=4, padx=5)

        # === File Tree View ===
        content_frame = ctk.CTkFrame(self)
        content_frame.pack(fill="both", expand=True, padx=10, pady=(0, 5))

        self.file_frame = ctk.CTkScrollableFrame(content_frame, width=450)
        self.file_frame.pack(side="left", fill="y", padx=(0, 5), pady=5)

        action_panel = ctk.CTkFrame(content_frame, width=150)
        action_panel.pack(side="left", fill="y", padx=5, pady=5)

        self.refresh_btn = ctk.CTkButton(action_panel, text="Refresh Files", command=self.send_list_command, state="disabled")
        self.refresh_btn.pack(pady=(0, 10))

        self.sync_btn = ctk.CTkButton(action_panel, text="Sync Filesystem", command=self.sync_filesystem, state="disabled")
        self.sync_btn.pack(pady=(0, 10))

        self.upload_btn = ctk.CTkButton(action_panel, text="Upload File", command=self.upload_file, state="disabled")
        self.upload_btn.pack(pady=(0, 10))

        self.download_btn = ctk.CTkButton(action_panel, text="Download File", command=self.download_file, state="disabled")
        self.download_btn.pack(pady=(0, 10))

        self.delete_btn = ctk.CTkButton(action_panel, text="Delete File", command=self.delete_file, state="disabled")
        self.delete_btn.pack(pady=(0, 10))

        self.view_btn = ctk.CTkButton(action_panel, text="View File", command=self.view_file, state="disabled")
        self.view_btn.pack(pady=(0, 10))

        # Add file type indicator
        self.file_type_label = ctk.CTkLabel(action_panel, text="No file selected", text_color="gray")
        self.file_type_label.pack(pady=(0, 5))

        self.format_btn = ctk.CTkButton(action_panel, text="Format Filesystem", command=self.format_filesystem, state="disabled")
        self.format_btn.pack(pady=(0, 10))

        self.sync_time_btn = ctk.CTkButton(action_panel, text="Sync Time", command=self.sync_controller_time, state="disabled")
        self.sync_time_btn.pack(pady=(0, 10))
        
        # Debug logging toggle
        self.debug_btn = ctk.CTkButton(action_panel, text="Debug: OFF", command=self.toggle_debug_logging)
        self.debug_btn.pack(pady=(0, 10))

        # === Log Box ===
        self.output_box = ctk.CTkTextbox(self, height=200)
        self.output_box.pack(fill="x", padx=10, pady=(0, 10))

    def log(self, msg):
        self.output_box.insert("end", msg + "\n")
        self.output_box.see("end")
    
    def is_wifi_transport(self):
        """Return True when Wi-Fi/TCP transport is active"""
        return self.transport_var.get() == 'Wi-Fi'

    def get_transport_timeouts(self, wifi_first=2.5, wifi_sub=1.0, uart_first=0.3, uart_sub=0.2):
        """
        Helper to derive transport-aware timeout pairs.
        Wi-Fi links need longer waits because the ESP + TCP stack buffers
        filesystem replies before forwarding them back to the GUI.
        """
        is_wifi = self.is_wifi_transport()
        if is_wifi:
            return is_wifi, wifi_first, wifi_sub
        return is_wifi, uart_first, uart_sub
    
    def create_progress_bar(self, percent, width=20):
        """Create a visual progress bar string"""
        filled = int((percent / 100) * width)
        bar = "█" * filled + "░" * (width - filled)
        return f"[{bar}]"
    
    def toggle_debug_logging(self):
        """Toggle debug logging on/off"""
        self.debug_logging = not self.debug_logging
        self.debug_btn.configure(text=f"Debug: {'ON' if self.debug_logging else 'OFF'}")
        self.log(f"[INFO] Debug logging {'enabled' if self.debug_logging else 'disabled'}")
    
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

    def toggle_connection(self):
        """Toggle between connect and disconnect"""
        if self.connected:
            self.disconnect_serial()
        else:
            self.connect_serial()
    
    def on_transport_change(self, value):
        # Show/hide Wi-Fi fields depending on selection
        if value == 'Wi-Fi':
            self.port_menu.configure(state="disabled")
            self.baud_menu.configure(state="disabled")
            self.wifi_ip_entry.grid(row=0, column=1, padx=5)
            self.wifi_port_entry.grid(row=0, column=2, padx=5)
        else:
            self.port_menu.configure(state="normal")
            self.baud_menu.configure(state="normal")
            try:
                self.transport_menu.configure(state="normal")
            except Exception:
                pass
            try:
                self.wifi_ip_entry.grid_forget()
                self.wifi_port_entry.grid_forget()
            except Exception:
                pass

    def connect_serial(self):
        transport = self.transport_var.get()
        try:
            if transport == 'Serial':
                port = self.port_var.get()
                baud = int(self.baud_var.get())
                self.serial = SerialInterface(port, baud)
                self.log(f"[INFO] Connected to {port} at {baud} baud")
            else:
                # Wi-Fi transport: use TCPInterface
                from tcp_interface import TCPInterface
                host = self.wifi_ip_var.get()
                port = int(self.wifi_port_var.get())
                self.serial = TCPInterface(host, port)
                self.log(f"[INFO] Connected to {host}:{port} via TCP")

            # For WiFi: Send framed command to switch to filesystem command mode
            # For Serial: Send raw text command (legacy behavior)
            if transport == 'Wi-Fi':
                # Send CMD_TRANSPORT_MODE_SWITCH with APP_TRANSPORT_MODE_FILESYSTEM_COMMAND
                mode_switch_packet = build_framed_command(CMD_TRANSPORT_MODE_SWITCH, bytes([APP_TRANSPORT_MODE_FILESYSTEM_COMMAND]))
                self.serial.send_bytes(mode_switch_packet)
                self.log(f"[INFO] Sent transport mode switch to FILESYSTEM_COMMAND")
                time.sleep(0.2)  # Wait for mode switch to complete
            else:
                # Serial: use legacy raw text command
                self.serial.send_raw_command("jumpToFilesystem\n")
                time.sleep(0.1)  # Increased delay for mode switch

            # Clear any pending data from the receive buffer
            try:
                self.serial.ser.read(1024)
            except Exception:
                pass

            # Automatically sync time on connect (but with delay)
            time.sleep(0.1)  # Additional delay before time sync
            self.sync_controller_time()

            # Update connection state and UI
            self.connected = True
            self.connect_btn.configure(text="Disconnect", fg_color="red")
            self.port_menu.configure(state="disabled")
            self.baud_menu.configure(state="disabled")
            self.transport_menu.configure(state="disabled")

            self.refresh_btn.configure(state="normal")
            self.upload_btn.configure(state="normal")
            self.sync_btn.configure(state="normal")
            self.format_btn.configure(state="normal")
            self.sync_time_btn.configure(state="normal")
        except Exception as e:
            self.log(f"[ERROR] Failed to connect: {e}")

    def disconnect_serial(self):
        """Disconnect from serial port and update UI"""
        try:
            if self.serial and hasattr(self.serial, 'ser'):
                # Return controller to normal mode before disconnecting
                self.log("[INFO] Exiting filesystem mode...")
                packet = build_framed_command(FS_CMD_EXIT)
                try:
                    self.serial.send_bytes(packet)
                except Exception:
                    pass

                # Wait for controller to process exit command and send responses
                time.sleep(0.3)

                # Flush any pending responses multiple times to clear all buffers
                try:
                    if hasattr(self.serial, 'ser') and self.serial.ser and self.serial.ser.is_open:
                        total_flushed = 0
                        # Flush multiple times to ensure all buffered data is cleared
                        for _ in range(3):
                            time.sleep(0.1)  # Allow time for responses to arrive
                            in_waiting = self.serial.ser.in_waiting
                            if in_waiting > 0:
                                pending_data = self.serial.ser.read(in_waiting)
                                total_flushed += len(pending_data)
                            else:
                                # Try to read at least 512 bytes to catch delayed responses
                                pending_data = self.serial.ser.read(512)
                                if pending_data:
                                    total_flushed += len(pending_data)
                                else:
                                    break  # No more data

                        if total_flushed > 0 and self.debug_logging:
                            self.log(f"[DEBUG] Flushed {total_flushed} bytes on disconnect")
                except Exception as e:
                    if self.debug_logging:
                        self.log(f"[DEBUG] Buffer flush error: {e}")

                try:
                    self.serial.close()
                except Exception:
                    pass
                self.log("[INFO] Disconnected from serial port")
            
            # Update connection state and UI
            self.connected = False
            self.serial = None
            self.selected_file = None
            
            # Reset button states
            self.connect_btn.configure(text="Connect", fg_color=self.default_btn_color)
            self.port_menu.configure(state="normal")
            self.baud_menu.configure(state="normal")
            self.transport_menu.configure(state="normal")
            
            # Disable all function buttons
            self.refresh_btn.configure(state="disabled")
            self.upload_btn.configure(state="disabled")
            self.sync_btn.configure(state="disabled")
            self.format_btn.configure(state="disabled")
            self.sync_time_btn.configure(state="disabled")
            self.download_btn.configure(state="disabled")
            self.delete_btn.configure(state="disabled")
            self.view_btn.configure(state="disabled")
            
            # Reset file selection UI
            self.file_type_label.configure(text="No file selected", text_color="gray")
            
            # Clear file list
            for widget in self.file_frame.winfo_children():
                widget.destroy()
                
        except Exception as e:
            self.log(f"[ERROR] Failed to disconnect: {e}")

    def sync_filesystem(self):
        if not self.serial:
            self.log("[WARN] No serial connection for sync.")
            return

        self.log("[INFO] Sending jumpToFilesystem command...")
        self.serial.send_raw_command("jumpToFilesystem\n")
        time.sleep(0.2)  # Wait for mode switch

        # Clear any pending data from the serial buffer (includes mode switch acknowledgment)
        flushed = self.serial.ser.read(2048)  # Larger flush buffer
        if self.debug_logging and flushed:
            self.log(f"[DEBUG] Flushed {len(flushed)} bytes from buffer")

        # Sync time when re-initializing filesystem
        time.sleep(0.1)  # Additional delay before time sync
        self.sync_controller_time()

        # Small delay before listing to ensure time sync response is fully received
        time.sleep(0.1)

        self.log("[INFO] Re-initializing filesystem view...")
        self.send_list_command()

    def send_list_command(self):
        if not self.serial:
            return

        # Disable refresh button during operation to prevent rapid clicks
        try:
            self.refresh_btn.configure(state="disabled")
            self.update()  # Update UI immediately

            # Rate limiting: prevent rapid repeated commands
            now = time.time()
            time_since_last = now - self.last_list_time
            if time_since_last < self.command_cooldown:
                remaining = self.command_cooldown - time_since_last
                if self.debug_logging:
                    self.log(f"[DEBUG] Rate limit: waiting {remaining:.2f}s before sending list command")
                time.sleep(remaining)

            # CRITICAL: Flush any stale data from previous commands before sending new one
            time.sleep(0.05)  # Small delay to let any pending responses arrive
            stale_data = self.serial.ser.read(8192)  # Read and discard old data
            if stale_data and self.debug_logging:
                self.log(f"[DEBUG] Flushed {len(stale_data)} stale bytes before list command")

            self.log("[INFO] Listing all filesystem contents...")
            packet = build_framed_command(FS_CMD_LIST)
            self.serial.send_bytes(packet)
            self.last_list_time = time.time()  # Update timestamp after sending

            # Progressive reading with adaptive timeouts for WiFi vs UART
            raw_response = b''
            read_attempts = 0
            max_attempts = 100  # Increased for WiFi: 40 attempts × 50ms = 2 second total timeout

            # Transport-specific timeouts (Wi-Fi waits much longer for ESP stream)
            is_wifi, first_data_timeout, subsequent_timeout = self.get_transport_timeouts(
                wifi_first=3.5, wifi_sub=1.5, uart_first=0.3, uart_sub=0.2
            )

            last_data_time = time.time()
            first_byte_received = False

            if self.debug_logging:
                self.log(f"[DEBUG] Transport mode: {'WiFi' if is_wifi else 'UART'}, first_timeout={first_data_timeout}s, subsequent_timeout={subsequent_timeout}s")

            while read_attempts < max_attempts:
                new_data = self.serial.ser.read(4096)
                if new_data:
                    if not first_byte_received:
                        first_byte_received = True
                        elapsed = time.time() - last_data_time
                        if self.debug_logging:
                            self.log(f"[DEBUG] First data received after {elapsed:.3f}s")
                    raw_response += new_data
                    last_data_time = time.time()
                    if self.debug_logging:
                        self.log(f"[DEBUG] List: Read {len(new_data)} bytes, total: {len(raw_response)} bytes")

                # Use different timeout based on whether we've received any data yet
                timeout = subsequent_timeout if first_byte_received else first_data_timeout
                elapsed = time.time() - last_data_time
                if elapsed > timeout:
                    if self.debug_logging:
                        self.log(f"[DEBUG] Timeout after {elapsed:.3f}s ({'subsequent' if first_byte_received else 'initial'} data timeout)")
                    break

                read_attempts += 1
                time.sleep(0.2)  # Short delay between reads

            lines = extract_framed_responses(raw_response)

            self.log(f"Filesystem Contents: ({len(lines)} entries received)")
            for l in lines:
                self.log("  " + l)

            self.display_files(lines)

        finally:
            # Re-enable refresh button
            self.refresh_btn.configure(state="normal")

    def display_files(self, lines):
        for widget in self.file_frame.winfo_children():
            widget.destroy()

        for line in lines:
            if line.startswith("[FILE]"):
                # Extract filename and size from: "[FILE] filename (size bytes)"
                parts = line[7:].split(" (")
                name = parts[0].strip()
                size_info = parts[1] if len(parts) > 1 else "unknown size"
                
                # Choose icon based on file type
                if name.endswith(".log"):
                    icon = "📄"  # Log file
                    file_type = "LOG"
                elif name.endswith(".txt"):
                    icon = "📝"  # Text file
                    file_type = "TXT"
                elif name.endswith(".wav"):
                    icon = "🔊"  # Audio file
                    file_type = "WAV"
                elif name.endswith((".dat", ".bin")):
                    icon = "🔢"  # Binary file
                    file_type = "BIN"
                else:
                    icon = "📋"  # Generic file
                    file_type = "FILE"
                
                btn_text = f"{icon} {name} ({size_info}"
                btn = ctk.CTkButton(self.file_frame, text=btn_text, anchor="w",
                                    command=lambda n=name: self.select_file(n))
                btn.pack(fill="x", padx=5, pady=2)
            elif line.startswith("[DIR]"):
                name = line[6:].strip()
                label = ctk.CTkLabel(self.file_frame, text=f"📁 {name}", anchor="w")
                label.pack(fill="x", padx=5, pady=2)

    def select_file(self, name):
        self.selected_file = name
        self.is_log_file = name.endswith(".log")
        
        # Determine file type for display
        if self.is_log_file:
            file_type = "LOG FILE"
            type_color = "green"
        elif name.endswith(".txt"):
            file_type = "TEXT FILE"
            type_color = "cyan"
        elif name.endswith((".dat", ".bin")):
            file_type = "BINARY FILE"  
            type_color = "orange"
        else:
            file_type = "DATA FILE"
            type_color = "blue"

        self.download_btn.configure(state="normal")
        self.delete_btn.configure(state="normal")
        self.view_btn.configure(state="normal")
        self.file_type_label.configure(text=f"{file_type}: {name}", text_color=type_color)

        self.log(f"[INFO] Selected {file_type.lower()}: {name}")

    def upload_file(self):
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

        import os
        filename = os.path.basename(file_path)
        
        try:
            import os
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
                    # For now, continue anyway (we can add retry logic later if needed)
                    # return False
                
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
            self.upload_btn.configure(state="normal")

    def download_file(self):
        if not self.serial or not self.selected_file:
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

        local_filename = self.selected_file.replace("/", "_")  # Safe filename
        with open(local_filename, "w") as f:
            for line in lines:
                f.write(line + "\n")
        self.log(f"[INFO] Saved to local file: {local_filename} ({len(lines)} lines)")

    def delete_file(self):
        if not self.serial or not self.selected_file:
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

        # Refresh file list
        time.sleep(0.1)  # Small delay before refresh
        self.send_list_command()

    def view_file(self):
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

    def format_filesystem(self):
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
            # Get current PC time
            pc_time = datetime.now()
            time_str = pc_time.strftime("%Y-%m-%dT%H:%M:%S")
            
            self.log(f"[TIME] Syncing controller time to: {time_str}")

            # Send time sync command
            packet = build_framed_command(FS_CMD_SYNC_TIME, time_str.encode())
            self.serial.send_bytes(packet)

            # Progressive reading with adaptive timeouts for WiFi vs UART
            raw_response = b''
            read_attempts = 0
            max_attempts = 60  # Increased for WiFi tolerance

            # Transport-specific timeouts
            is_wifi, first_data_timeout, subsequent_timeout = self.get_transport_timeouts(
                wifi_first=2.5, wifi_sub=0.8, uart_first=0.3, uart_sub=0.2
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
                    raw_response += new_data
                    last_data_time = time.time()
                    # Check if we have the expected response
                    if b'TIME UPDATED' in raw_response or b'ERROR' in raw_response:
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

            lines = extract_framed_responses(raw_response)

            for line in lines:
                if line.startswith("TIME UPDATED"):
                    self.log(f"[TIME] ✓ {line}")
                else:
                    self.log(f"[TIME] {line}")
                    
        except Exception as e:
            self.log(f"[TIME]  Time sync failed: {e}")

    def on_closing(self):
        """Handle application closing - disconnect serial if connected"""
        if self.connected:
            self.disconnect_serial()
        self.destroy()

if __name__ == "__main__":
    ctk.set_appearance_mode("dark")
    ctk.set_default_color_theme("blue")
    app = FlashfsFSApp()
    app.mainloop()
