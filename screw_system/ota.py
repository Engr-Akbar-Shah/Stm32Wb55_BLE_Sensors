import asyncio
from bleak import BleakScanner, BleakClient
from tqdm import tqdm
import struct
import os

# Only the reboot UUID (in user app)
REBOOT_CHAR_UUID = "0000fe11-8e22-4541-9d4c-21edae82ed19"

# Fixed layout
APP_START_ADDRESS = 0x08007000
FLASH_BASE = 0x08000000
SECTOR_SIZE = 4096
START_SECTOR = 7  # 0x08007000 / 4096 = 7

async def perform_ota(bin_file: str):
    if not os.path.exists(bin_file):
        print(f"ERROR: File not found: {bin_file}")
        return

    with open(bin_file, "rb") as f:
        firmware = f.read()

    file_size = len(firmware)
    if file_size == 0:
        print("ERROR: Empty firmware file")
        return

    num_sectors = (file_size + SECTOR_SIZE - 1) // SECTOR_SIZE
    print(f"Firmware: {bin_file} ({file_size} bytes)")
    print(f"Erasing {num_sectors} sectors starting from sector {START_SECTOR} (0x{APP_START_ADDRESS:08X})")

    # Step 1: Find user app and reboot
    print("\nScanning for BOLT (user app)...")
    device = await BleakScanner.find_device_by_name("BOLT", timeout=15.0)
    if not device:
        print("ERROR: BOLT not found")
        return
    print(f"Found: {device.name} ({device.address})")

    print("Sending reboot to OTA...")
    async with BleakClient(device.address) as client:
        await client.write_gatt_char(REBOOT_CHAR_UUID, struct.pack("<BBBB", 0x01, START_SECTOR, num_sectors, 0x00))
        print("Reboot command sent")

    print("Waiting 12 seconds for reboot...")
    await asyncio.sleep(12)

    # Step 2: Find OTA bootloader
    print("Scanning for BOLT (OTA mode)...")
    ota_device = None
    for _ in range(6):
        ota_device = await BleakScanner.find_device_by_name("BOLT", timeout=10.0)
        if ota_device:
            break
        print("   Not found yet... retrying")
        await asyncio.sleep(3)

    if not ota_device:
        print("ERROR: Device not in OTA mode")
        return
    print(f"OTA bootloader found: {ota_device.name} ({ota_device.address})")

    # Step 3: Connect and discover OTA characteristics
    async with BleakClient(ota_device.address) as client:
        print("Discovering services...")
        await client.get_services()  # Force discovery

        # Find Base Address and Raw Data characteristics (write/wwr properties)
        base_addr_char = None
        raw_data_char = None

        for service in client.services:
            for char in service.characteristics:
                props = char.properties
                if "write-without-response" in props or "write" in props:
                    if base_addr_char is None:
                        base_addr_char = char
                        print(f"Using Base Address char: {char.uuid}")
                    elif raw_data_char is None:
                        raw_data_char = char
                        print(f"Using Raw Data char: {char.uuid}")

        if not base_addr_char or not raw_data_char:
            print("ERROR: Could not find OTA write characteristics!")
            print("Available write chars:")
            for service in client.services:
                for char in service.characteristics:
                    if "write" in char.properties or "write-without-response" in char.properties:
                        print(f"  - {char.uuid} ({char.properties})")
            return

        # Optional larger MTU
        try:
            await client.exchange_mtu(251)
            print(f"MTU: {client.mtu_size}")
        except:
            pass

        chunk_size = client.mtu_size - 3

        # Start upload: 0x02 + start sector (uint32 little-endian)
        start_payload = bytes([0x02]) + struct.pack("<I", START_SECTOR)
        await client.write_gatt_char(base_addr_char.uuid, start_payload, response=False)
        print("Upload start sent")

        # Upload data
        print("Uploading firmware...")
        with tqdm(total=file_size, unit='B', unit_scale=True) as pbar:
            for i in range(0, file_size, chunk_size):
                chunk = firmware[i:i+chunk_size]
                await client.write_gatt_char(raw_data_char.uuid, chunk, response=False)
                pbar.update(len(chunk))
                await asyncio.sleep(0.005)

        # Finish: 0x07
        await client.write_gatt_char(base_addr_char.uuid, bytes([0x07]), response=False)
        print("\nOTA COMPLETE! Device should reboot to new firmware soon.")

if __name__ == "__main__":
    import sys
    if len(sys.argv) != 2:
        print("Usage: python ota.py <firmware.bin>")
        sys.exit(1)
    asyncio.run(perform_ota(sys.argv[1]))