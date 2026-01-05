#!/usr/bin/env python3
import argparse
import asyncio
import math
import os
import sys

from bleak import BleakClient, BleakScanner

FLASH_BASE_ADDR = 0x08000000
APP_BASE_ADDR_DEFAULT = 0x08007000
FLASH_PAGE_SIZE = 4096

REBOOT_CHAR_UUID = "0000fe11-8e22-4541-9d4c-21edae82ed19"
OTA_SERVICE_UUID = "0000fe20-cc7a-482a-984a-7f2ed5b3e58f"
BASE_ADDR_CHAR_UUID = "0000fe22-8e22-4541-9d4c-21edae82ed19"
REBOOT_CONFIRM_CHAR_UUID = "0000fe23-8e22-4541-9d4c-21edae82ed19"
OTA_DATA_CHAR_UUID = "0000fe24-8e22-4541-9d4c-21edae82ed19"

ACTION_START_USER_APP = 0x02
ACTION_FILE_FINISHED = 0x07

OTA_CHUNK_SIZE = 200


async def get_services_compat(client):
    """Compatible way to fetch services across Bleak versions."""
    if hasattr(client, "get_services"):
        return await client.get_services()
    return client.services


async def wait_for_device(name, timeout=30):
    print(f"[SCAN] Searching for '{name}' ({timeout}s)...")
    loop = asyncio.get_event_loop()
    deadline = loop.time() + timeout

    while True:
        devices = await BleakScanner.discover(timeout=4.0)

        for d in devices:
            dev_name = getattr(d, "name", "") or ""
            if dev_name == name:
                print(f"[SCAN] Found {d.address} ({dev_name})")
                return d

        if loop.time() > deadline:
            raise TimeoutError(f"Timeout: device '{name}' not found")

        print("[SCAN] retry...")
        await asyncio.sleep(1.0)


def compute_sector_info(app_addr, size):
    offset = app_addr - FLASH_BASE_ADDR
    first_sector = offset // FLASH_PAGE_SIZE
    num_sectors = math.ceil(size / FLASH_PAGE_SIZE)
    return first_sector, num_sectors


async def ensure_ota_mode(name, first_sector, num_sectors):
    dev = await wait_for_device(name)
    client = BleakClient(dev)

    try:
        await client.connect()
        print("[BLE] Connected")

        services = await get_services_compat(client)
        service_uuids = {s.uuid.lower() for s in services}
        char_uuids = {c.uuid.lower() for s in services for c in s.characteristics}

        if REBOOT_CHAR_UUID in char_uuids:
            # USER APP → send reboot command
            print("[MODE] User app detected. Sending reboot to OTA...")

            payload = bytes([
                0x01,                    # boot mode = OTA
                first_sector & 0xFF,     # first sector index
                num_sectors & 0xFF       # number of sectors to erase
            ])

            # IMPORTANT: this characteristic is Write WITHOUT Response
            await client.write_gatt_char(REBOOT_CHAR_UUID, payload, response=False)
            print("[BLE] Reboot command sent")

        elif OTA_SERVICE_UUID in service_uuids:
            # Already in OTA mode
            print("[MODE] Already in OTA mode")

        else:
            raise RuntimeError("No reboot characteristic / OTA service")

    finally:
        await client.disconnect()

    # give it time to reboot into BLE_Ota
    await asyncio.sleep(3.0)


async def perform_ota(name, bin_data, base_addr):
    offset = base_addr - FLASH_BASE_ADDR
    addr_bytes = offset.to_bytes(3, "big")

    dev = await wait_for_device(name)
    client = BleakClient(dev)

    reboot_evt = asyncio.Event()

    def reboot_cb(sender, data):
        print("[OTA] reboot indication:", data.hex())
        reboot_evt.set()

    try:
        await client.connect()
        print("[BLE] Connected (OTA mode)")

        services = await get_services_compat(client)
        if OTA_SERVICE_UUID not in {s.uuid.lower() for s in services}:
            raise RuntimeError("OTA service missing — not in OTA app")

        # subscribe to reboot confirm if present
        try:
            await client.start_notify(REBOOT_CONFIRM_CHAR_UUID, reboot_cb)
        except Exception:
            pass

        # start user app upload
        start_payload = bytes([ACTION_START_USER_APP]) + addr_bytes
        await client.write_gatt_char(BASE_ADDR_CHAR_UUID, start_payload, response=False)

        total = len(bin_data)
        sent = 0
        print(f"[OTA] Sending {total} bytes...")

        for i in range(0, total, OTA_CHUNK_SIZE):
            chunk = bin_data[i:i + OTA_CHUNK_SIZE]
            await client.write_gatt_char(OTA_DATA_CHAR_UUID, chunk, response=False)
            sent += len(chunk)
            sys.stdout.write(f"\r{sent}/{total}")
            sys.stdout.flush()
            await asyncio.sleep(0.001)

        print("\n[OTA] Done — finishing...")

        finish_payload = bytes([ACTION_FILE_FINISHED]) + addr_bytes
        await client.write_gatt_char(BASE_ADDR_CHAR_UUID, finish_payload, response=False)

        try:
            await asyncio.wait_for(reboot_evt.wait(), timeout=8.0)
        except asyncio.TimeoutError:
            pass

    finally:
        await client.disconnect()


async def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("binary")
    parser.add_argument("--name", default="BOLT")
    parser.add_argument("--addr", default=f"0x{APP_BASE_ADDR_DEFAULT:08X}")
    args = parser.parse_args()

    base_addr = int(args.addr, 0)

    with open(args.binary, "rb") as f:
        data = f.read()

    print(f"[INFO] Loaded {len(data)} bytes")

    first, count = compute_sector_info(base_addr, len(data))
    print(f"[INFO] Sector plan: first={first}, count={count}")

    await ensure_ota_mode(args.name, first, count)
    await perform_ota(args.name, data, base_addr)

    print("\n[DONE]")


if __name__ == "__main__":
    asyncio.run(main())
