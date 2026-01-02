import tkinter as tk
from tkinter import ttk, scrolledtext, messagebox
import asyncio
import threading
from bleak import BleakClient, BleakScanner

import struct
from datetime import datetime

# Standard ST P2P UUIDs
LED_WRITE_UUID   = "0000fe41-8e22-4541-9d4c-21edae82ed19"   # Write to control LED
NOTIFY_UUID      = "0000fe42-8e22-4541-9d4c-21edae82ed19"   # Notifications from device

SENSOR_CMD_PREFIX       = 0x10
SENSOR_LSM6DSO          = 0x01
SENSOR_STTSH22H         = 0x02
SENSOR_BOTH             = 0x03
SENSOR_START            = 0x01
SENSOR_STOP             = 0x00
NOTIF_SENSOR_DATA       = 0x20
NOTIF_SENSOR_STATUS     = 0x21
VERSION_REQUEST_PREFIX  = 0x30
NOTIF_VERSION_RESPONSE  = 0x30

# Sensor Command Protocol
# Format: [Device_Selection, Sensor_ID, Action]
# Device_Selection: 0x10 = Sensor commands
# Sensor_ID: 0x01 = LSM6DSO, 0x02 = STT22H, 0x03 = Both
# Action: 0x01 = Start, 0x00 = Stop

class SimpleBOLTController:
    def clear_logs(self):
        self.notify_text.config(state="normal")
        self.notify_text.delete("1.0", "end")
        self.notify_text.config(state="disabled")

    def __init__(self, root):
        self.root = root
        self.root.title("BOLT LED & Sensor Controller")
        self.root.geometry("750x850")
        self.root.resizable(True, True)

        self.client = None
        self.is_connected = False
        self.disconnect_in_progress = False

        self.lsm6dso_active = False
        self.sttsh22h_active = False
        self.both_sensors_active = False
        self.lsm6dso_count = 0
        self.sttsh22h_count = 0
        
        # Sensor states
        self.lsm6dso_active = False
        self.sttsh22h_active = False
        self.both_sensors_active = False

        # === UI Elements ===
        # Status label
        self.status_label = ttk.Label(
            root, 
            text="Status: Disconnected", 
            font=("Arial", 12, "bold"),
            foreground="red"
        )
        self.status_label.pack(pady=10)

        self.version_label = ttk.Label(
            root,
            text="Firmware Version: Unknown",
            font=("Arial", 10),
            foreground="gray"
        )
        self.version_label.pack(pady=5)

        # Connection button
        self.connect_button = ttk.Button(
            root, 
            text="Connect to BOLT", 
            command=self.toggle_connection,
            width=25
        )
        self.connect_button.pack(pady=5)
        self.fetch_version_button = ttk.Button(
            root,
            text="Fetch Version",
            command=self.fetch_version,
            width=25,
            state="disabled"
        )
        self.fetch_version_button.pack(pady=5)

        # Create tabbed interface
        self.notebook = ttk.Notebook(root)
        self.notebook.pack(padx=20, pady=10, fill="both", expand=True)

        # === LED Control Tab ===
        led_tab = ttk.Frame(self.notebook)
        self.notebook.add(led_tab, text="LED Control")

        led_frame = ttk.LabelFrame(led_tab, text="LED Control", padding=15)
        led_frame.pack(padx=10, pady=10, fill="both", expand=True)

        self.led_on_button = ttk.Button(
            led_frame, text="LED ON", command=lambda: self.send_led_command(0x01),
            width=20, state="disabled"
        )
        self.led_on_button.pack(pady=5)

        self.led_off_button = ttk.Button(
            led_frame, text="LED OFF", command=lambda: self.send_led_command(0x00),
            width=20, state="disabled"
        )
        self.led_off_button.pack(pady=5)

        # === Sensor Data Tab ===
        sensor_tab = ttk.Frame(self.notebook)
        self.notebook.add(sensor_tab, text="Sensor Data")

        sensor_frame = ttk.LabelFrame(sensor_tab, text="Sensor Control", padding=15)
        sensor_frame.pack(padx=10, pady=10, fill="both", expand=True)

        # LSM6DSO Button
        lsm_frame = ttk.Frame(sensor_frame)
        lsm_frame.pack(pady=8, fill="x")
        
        ttk.Label(lsm_frame, text="LSM6DSO (Accel/Gyro):", width=25, anchor="w").pack(side="left", padx=5)
        self.lsm6dso_button = ttk.Button(
            lsm_frame,
            text="START",
            command=self.toggle_lsm6dso,
            width=15,
            state="disabled"
        )
        self.lsm6dso_button.pack(side="left", padx=5)
        self.lsm6dso_status = ttk.Label(lsm_frame, text="●", foreground="red", font=("Arial", 16))
        self.lsm6dso_status.pack(side="left", padx=5)

        # STT22H Button
        temp_frame = ttk.Frame(sensor_frame)
        temp_frame.pack(pady=8, fill="x")
        
        ttk.Label(temp_frame, text="STT22H (Temperature):", width=25, anchor="w").pack(side="left", padx=5)
        self.sttsh22h_button = ttk.Button(
            temp_frame,
            text="START",
            command=self.toggle_sttsh22h,
            width=15,
            state="disabled"
        )
        self.sttsh22h_button.pack(side="left", padx=5)
        self.sttsh22h_status = ttk.Label(temp_frame, text="●", foreground="red", font=("Arial", 16))
        self.sttsh22h_status.pack(side="left", padx=5)

        # Both Sensors Button
        both_frame = ttk.Frame(sensor_frame)
        both_frame.pack(pady=8, fill="x")
        
        ttk.Label(both_frame, text="Both Sensors:", width=25, anchor="w").pack(side="left", padx=5)
        self.both_sensors_button = ttk.Button(
            both_frame,
            text="START BOTH",
            command=self.toggle_both_sensors,
            width=15,
            state="disabled"
        )
        self.both_sensors_button.pack(side="left", padx=5)
        self.both_sensors_status = ttk.Label(both_frame, text="●", foreground="red", font=("Arial", 16))
        self.both_sensors_status.pack(side="left", padx=5)

        # Separator
        ttk.Separator(sensor_frame, orient="horizontal").pack(fill="x", pady=15)

        # Sensor Info
        info_text = (
            "Command Protocol:\n"
            "• LSM6DSO:  [0x10, 0x01, 0x01/0x00] - Start/Stop\n"
            "• STT22H:  [0x10, 0x02, 0x01/0x00] - Start/Stop\n"
            "• Both:    [0x10, 0x03, 0x01/0x00] - Start/Stop"
        )
        info_label = ttk.Label(sensor_frame, text=info_text, font=("Consolas", 9), 
                               justify="left", foreground="gray")
        info_label.pack(pady=5)

        # === Notification / Serial-like viewer (shared) ===
        notify_frame = ttk.LabelFrame(root, text="Device Notifications / Status", padding=10)
        notify_frame.pack(padx=20, pady=10, fill="both", expand=True)

        btn_frame = ttk.Frame(notify_frame)
        btn_frame.pack(fill="x", pady=5)

        clear_btn = ttk.Button(
            btn_frame,
            text="Clear Logs",
            command=self.clear_logs,
            width=15
        )
        clear_btn.pack(side="right")

        self.notify_text = scrolledtext.ScrolledText(
            notify_frame, height=10, state="disabled", font=("Consolas", 9)
        )
        self.notify_text.pack(fill="both", expand=True)

        # Start asyncio loop
        self.loop = asyncio.new_event_loop()
        self.async_thread = threading.Thread(target=self._run_async_loop, daemon=True)
        self.async_thread.start()

    def fetch_version(self):
        """Send a version request command to the device"""
        async def _fetch():
            if not self.client or not self.client.is_connected:
                self.log_device("✗ Cannot fetch version: Not connected")
                return

            try:
                payload = bytes([VERSION_REQUEST_PREFIX])  # Simple 1-byte request: 0x30
                await self.client.write_gatt_char(LED_WRITE_UUID, payload, response=False)
                self.log_device("→ Version request sent (0x30)")
            except Exception as e:
                self.log_device(f"✗ Version request failed: {e}")

        asyncio.run_coroutine_threadsafe(_fetch(), self.loop)

    def _run_async_loop(self):
        asyncio.set_event_loop(self.loop)
        self.loop.run_forever()

    def toggle_connection(self):
        if self.is_connected:
            self.connect_button.config(state="disabled", text="Disconnecting...")
            asyncio.run_coroutine_threadsafe(self._disconnect(), self.loop)
        else:
            asyncio.run_coroutine_threadsafe(self._connect(), self.loop)

    async def _connect(self):
        try:
            self.root.after(0, lambda: self.status_label.config(text="Scanning...", foreground="orange"))
            self.root.after(0, lambda: self.connect_button.config(state="disabled", text="Scanning..."))

            devices = await BleakScanner.discover(timeout=8.0)
            target = next((d for d in devices if d.name == "BOLT"), None)

            if not target:
                self._update_ui_disconnected("BOLT not found")
                self.root.after(0, lambda: messagebox.showerror("Error", "BOLT device not found"))
                return

            self.root.after(0, lambda: self.status_label.config(
                text=f"Connecting to {target.address[-8:]}...", foreground="blue"))

            self.client = BleakClient(target.address, disconnected_callback=self._on_disconnect)
            await self.client.connect()

            # Enable notifications
            await self.client.start_notify(NOTIFY_UUID, self._notification_handler)
                
            self.root.after(500, self.fetch_version)
            self._update_ui_connected()
            self.log_device("✓ Connected successfully")
            self.log_device(f"✓ Notifications enabled on UUID: ...{NOTIFY_UUID[-12:]}")

        except Exception as e:
            self._update_ui_disconnected(f"Connection failed")
            self.root.after(0, lambda: messagebox.showerror("Connection Error", str(e)))
            self.log_device(f"✗ Connection error: {e}")

    async def _disconnect(self):
        """Properly disconnect from device"""
        if self.disconnect_in_progress:
            return
    
        self.disconnect_in_progress = True
    
        try:
            # Stop all sensors first
            if self.lsm6dso_active or self.sttsh22h_active or self.both_sensors_active:
                self.log_device("Stopping sensors...")
                if self.both_sensors_active:
                    await self._send_sensor_command(SENSOR_BOTH, SENSOR_STOP)
                else:
                    if self.lsm6dso_active:
                        await self._send_sensor_command(SENSOR_LSM6DSO, SENSOR_STOP)
                    if self.sttsh22h_active:
                        await self._send_sensor_command(SENSOR_STTSH22H, SENSOR_STOP)
                await asyncio.sleep(0.2)
        
            # Disconnect
            if self.client and self.client.is_connected:
                await self.client.stop_notify(NOTIFY_UUID)
                await self.client.disconnect()
                self.log_device("✓ Disconnected")
        
            self._update_ui_disconnected("Disconnected")
        
        except Exception as e:
            self.log_device(f"✗ Disconnect error: {e}")
            self._update_ui_disconnected("Disconnect error")
        finally:
            self.disconnect_in_progress = False
            self.client = None

    def _on_disconnect(self, client):
        """Callback when device disconnects unexpectedly"""
        if not self.disconnect_in_progress:
            self.log_device("⚠ Device disconnected unexpectedly")
            self._update_ui_disconnected("Connection lost")

    def _update_ui_connected(self):
        def _update():
            self.is_connected = True
            self.connect_button.config(text="Disconnect", state="normal")
            self.led_on_button.config(state="normal")
            self.led_off_button.config(state="normal")
            self.lsm6dso_button.config(state="normal")
            self.sttsh22h_button.config(state="normal")
            self.both_sensors_button.config(state="normal")
            self.fetch_version_button.config(state="normal")
            self.status_label.config(text="Connected to BOLT", foreground="green")
        self.root.after(0, _update)

    def _update_ui_disconnected(self, msg="Disconnected"):
        def _update():
            self.is_connected = False
            self.connect_button.config(text="Connect to BOLT", state="normal")
            self.led_on_button.config(state="disabled")
            self.led_off_button.config(state="disabled")
            self.lsm6dso_button.config(state="disabled")
            self.sttsh22h_button.config(state="disabled")
            self.both_sensors_button.config(state="disabled")
            self.fetch_version_button.config(state="disabled")
            self.status_label.config(text=f"Status: {msg}", foreground="red")
            
            # Reset sensor states
            self.lsm6dso_active = False
            self.sttsh22h_active = False
            self.both_sensors_active = False
            self._update_sensor_button_states()
            self.version_label.config(text="Firmware Version: Unknown", foreground="gray")
            
        self.root.after(0, _update)

    # === LED Control ===
    async def _send_led_command(self, value: int):
        if not self.client or not self.client.is_connected:
            return
        try:
            payload = bytes([0x00, value])  # 0x00 = all devices, then ON/OFF
            await self.client.write_gatt_char(LED_WRITE_UUID, payload, response=False)
            self.log_device(f"→ LED {'ON' if value else 'OFF'} | Payload: {' '.join(f'{b:02X}' for b in payload)}")
        except Exception as e:
            self.log_device(f"✗ LED write failed: {e}")

    def send_led_command(self, value: int):
        asyncio.run_coroutine_threadsafe(self._send_led_command(value), self.loop)

    # === Sensor Control ===
    async def _send_sensor_command(self, sensor_id: int, action: int):
        """
        Send sensor command to device
        sensor_id: 0x01=LSM6DSO, 0x02=STT22H, 0x03=Both
        action: 0x01=Start, 0x00=Stop
        """
        if not self.client or not self.client.is_connected:
            return False
        
        try:
            payload = bytes([0x10, sensor_id, action])  # 0x10 = sensor command prefix
            await self.client.write_gatt_char(LED_WRITE_UUID, payload, response=False)
            
            sensor_names = {0x01: "LSM6DSO", 0x02: "STT22H", 0x03: "BOTH"}
            action_str = "START" if action else "STOP"
            self.log_device(
                f"→ {sensor_names.get(sensor_id, 'UNKNOWN')} {action_str} | "
                f"Payload: {' '.join(f'{b:02X}' for b in payload)}"
            )
            return True
        except Exception as e:
            self.log_device(f"✗ Sensor command failed: {e}")
            return False

    def toggle_lsm6dso(self):
        async def _toggle():
            if self.both_sensors_active:
                self.log_device("⚠ Stop 'Both Sensors' first")
                return
            
            new_state = not self.lsm6dso_active
            action = 0x01 if new_state else 0x00
            
            if await self._send_sensor_command(0x01, action):
                self.lsm6dso_active = new_state
                self.root.after(0, self._update_sensor_button_states)
        
        asyncio.run_coroutine_threadsafe(_toggle(), self.loop)

    def toggle_sttsh22h(self):
        async def _toggle():
            if self.both_sensors_active:
                self.log_device("⚠ Stop 'Both Sensors' first")
                return
            
            new_state = not self.sttsh22h_active
            action = 0x01 if new_state else 0x00
            
            if await self._send_sensor_command(0x02, action):
                self.sttsh22h_active = new_state
                self.root.after(0, self._update_sensor_button_states)
        
        asyncio.run_coroutine_threadsafe(_toggle(), self.loop)

    def toggle_both_sensors(self):
        async def _toggle():
            new_state = not self.both_sensors_active
            action = 0x01 if new_state else 0x00
            
            if await self._send_sensor_command(0x03, action):
                self.both_sensors_active = new_state
                if new_state:
                    # When starting both, disable individual buttons
                    self.lsm6dso_active = False
                    self.sttsh22h_active = False
                self.root.after(0, self._update_sensor_button_states)
        
        asyncio.run_coroutine_threadsafe(_toggle(), self.loop)

    def _update_sensor_button_states(self):
        """Update button text and status indicators"""
        # LSM6DSO
        if self.lsm6dso_active:
            self.lsm6dso_button.config(text="STOP")
            self.lsm6dso_status.config(foreground="green")
        else:
            self.lsm6dso_button.config(text="START")
            self.lsm6dso_status.config(foreground="red")
        
        # STT22H
        if self.sttsh22h_active:
            self.sttsh22h_button.config(text="STOP")
            self.sttsh22h_status.config(foreground="green")
        else:
            self.sttsh22h_button.config(text="START")
            self.sttsh22h_status.config(foreground="red")
        
        # Both
        if self.both_sensors_active:
            self.both_sensors_button.config(text="STOP BOTH")
            self.both_sensors_status.config(foreground="green")
            # Disable individual sensor buttons when both is active
            self.lsm6dso_button.config(state="disabled")
            self.sttsh22h_button.config(state="disabled")
        else:
            self.both_sensors_button.config(text="START BOTH")
            self.both_sensors_status.config(foreground="red")
            # Re-enable individual buttons
            if self.is_connected:
                self.lsm6dso_button.config(state="normal")
                self.sttsh22h_button.config(state="normal")

    # === Notification Handler ===
    def _notification_handler(self, sender, data: bytes):
        """Called when device sends notification"""
        try:
            hex_data = ' '.join(f'{b:02X}' for b in data)
            text = f"← Received ({len(data)} bytes): {hex_data}"

            # Parse notifications based on your protocol
            if len(data) >= 2:
                if data[0] == 0xAA:  # LED status
                    if data[1] == 0x01:
                        text += " → LED ON ✓"
                    elif data[1] == 0x00:
                        text += " → LED OFF ✓"
                
                elif data[0] == NOTIF_SENSOR_STATUS:  # 0x21 - Status confirmation
                    sensor_id = data[1] if len(data) > 1 else 0
                    status = data[2] if len(data) > 2 else 0
                    sensor_names = {SENSOR_LSM6DSO: "LSM6DSO", SENSOR_STTSH22H: "STT22H", SENSOR_BOTH: "BOTH"}
                    sensor_name = sensor_names.get(sensor_id, f"Sensor {sensor_id}")
                    status_str = "STARTED" if status == SENSOR_START else "STOPPED"
                    text = f"← {sensor_name} {status_str} ✓"

                elif data[0] == NOTIF_SENSOR_DATA:  # 0x20 - Actual sensor data
                    sensor_id = data[1] if len(data) > 1 else 0
    
                    # Parse LSM6DSO data
                    if sensor_id == SENSOR_LSM6DSO and len(data) >= 14:
                        accel_x = struct.unpack('>h', data[2:4])[0]
                        accel_y = struct.unpack('>h', data[4:6])[0]
                        accel_z = struct.unpack('>h', data[6:8])[0]
                        gyro_x = struct.unpack('>h', data[8:10])[0]
                        gyro_y = struct.unpack('>h', data[10:12])[0]
                        gyro_z = struct.unpack('>h', data[12:14])[0]
        
                        self.lsm6dso_count += 1
        
                        # Only log every 10th sample to reduce spam
                        if self.lsm6dso_count % 10 == 0:
                            text = f"← LSM6DSO #{self.lsm6dso_count}: Accel X={accel_x:5d} Y={accel_y:5d} Z={accel_z:5d} | Gyro X={gyro_x:5d} Y={gyro_y:5d} Z={gyro_z:5d}"
                        else:
                            return  # Skip logging for other samples
    
                    # Parse Temperature data
                    elif sensor_id == SENSOR_STTSH22H and len(data) >= 4:
                        temp_raw = struct.unpack('>h', data[2:4])[0]
                        temp_celsius = temp_raw / 100.0
        
                        self.sttsh22h_count += 1
        
                        # Only log every 10th sample to reduce spam
                        if self.sttsh22h_count % 10 == 0:
                            text = f"← STT22H #{self.sttsh22h_count}: Temperature: {temp_celsius:.2f} °C"
                        else:
                            return  # Skip logging for other samples
                elif data[0] == NOTIF_VERSION_RESPONSE:
                    if len(data) >= 4:
                        major = data[1]
                        minor = data[2]
                        patch = data[3]
                        version_str = f"{major}.{minor}.{patch}"
                        # Log and update UI immediately
                        self.log_device(f"← Firmware Version: {version_str}")
                        self.root.after(0, lambda v=version_str: self.version_label.config(
                            text=f"Firmware Version: {v}",
                            foreground="black"
                        ))
                        # DO NOT continue to "unknown format" – we found what we wanted
                        return  # <-- Critical: stop processing this notification for logging
                    else:
                        self.log_device(f"← Version response too short: {' '.join(f'{b:02X}' for b in data)}")
                        return

                else:
                    text = f"← Received sensor data (unknown format): {hex_data}"
            
            self.log_device(text)

        except Exception as e:
            self.log_device(f"✗ Notification parse error: {e}")

    def log_device(self, message: str):
        """Thread-safe log to text area"""
        timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        def _update():
            self.notify_text.config(state="normal")
            self.notify_text.insert("end", f"[{timestamp}] {message}\n")
            self.notify_text.see("end")
            self.notify_text.config(state="disabled")
        self.root.after(0, _update)


def main():
    root = tk.Tk()
    app = SimpleBOLTController(root)

    def on_closing():
        """Cleanup on window close"""
        if app.client and app.client.is_connected:
            # Schedule disconnect
            future = asyncio.run_coroutine_threadsafe(app._disconnect(), app.loop)
            try:
                future.result(timeout=2.0)
            except:
                pass
        
        # Stop event loop
        app.loop.call_soon_threadsafe(app.loop.stop)
        root.destroy()

    root.protocol("WM_DELETE_WINDOW", on_closing)
    root.mainloop()


if __name__ == "__main__":
    main()
