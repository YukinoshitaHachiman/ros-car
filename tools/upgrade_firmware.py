#!/usr/bin/env python3
"""
ROSbot IAP Firmware Upgrade Tool

Reads firmware .bin from tools/bin/ and sends it to STM32 via serial port.

Usage:
    python3 upgrade_firmware.py <firmware.bin> [--port /dev/ttyUSB0] [--baud 115200]
    python3 upgrade_firmware.py --list     # list available .bin files
"""

import argparse
import os
import sys
import time

try:
    import serial
except ImportError:
    print("[ERR] pyserial not installed. Run: pip install pyserial")
    sys.exit(1)

# ─── Protocol constants ───────────────────────────────────────────────
FRAME_HEAD1 = 0xAA
FRAME_HEAD2 = 0x55

CMD_ERASE  = 0x45  # 'E' erase storage area
CMD_DATA   = 0x44  # 'D' write one page
CMD_FINISH = 0x46  # 'F' finish and set ready flag
CMD_INFO   = 0x49  # 'I' query partition info

FLASH_PAGE_SIZE = 1024
MAX_RETRIES     = 3
RESP_TIMEOUT    = 5.0   # seconds: erase timeout (may take a while)
PAGE_TIMEOUT    = 2.0   # seconds: per-page timeout

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
BIN_DIR    = os.path.join(SCRIPT_DIR, "bin")


# ─── CRC16 (XMODEM, poly=0x1021, init=0) ─────────────────────────────
def crc16_xmodem(data: bytes) -> int:
    crc = 0
    for byte in data:
        crc ^= (byte << 8)
        for _ in range(8):
            if crc & 0x8000:
                crc = (crc << 1) ^ 0x1021
            else:
                crc = (crc << 1)
    return crc & 0xFFFF


# ─── Frame builders ───────────────────────────────────────────────────
def build_frame(cmd: int, payload: bytes = b"") -> bytes:
    """Build a protocol frame: AA 55 <cmd> <len_l> <len_h> [payload] <crc_l> <crc_h>"""
    length = len(payload)
    head = bytes([FRAME_HEAD1, FRAME_HEAD2, cmd,
                  length & 0xFF, (length >> 8) & 0xFF])
    crc_data = bytes([cmd, length & 0xFF, (length >> 8) & 0xFF]) + payload
    crc = crc16_xmodem(crc_data)
    tail = bytes([crc & 0xFF, (crc >> 8) & 0xFF])
    return head + payload + tail


# ─── Response reader ──────────────────────────────────────────────────
def read_response(ser: serial.Serial, timeout: float) -> tuple:
    """Read and validate a response frame. Returns (cmd, status) or (None, None)."""
    deadline = time.time() + timeout
    buf = bytearray()

    while time.time() < deadline:
        b = ser.read(1)
        if not b:
            continue
        buf.append(b[0])

        # look for AA 55 header in buffer
        while len(buf) >= 4:
            if buf[0] == FRAME_HEAD1 and buf[1] == FRAME_HEAD2:
                return buf[2], buf[3]  # (cmd, status)
            buf.pop(0)

    return None, None


# ─── Command helpers ──────────────────────────────────────────────────
def send_cmd(ser: serial.Serial, cmd: int, payload: bytes = b"",
             timeout: float = RESP_TIMEOUT, retries: int = MAX_RETRIES) -> bool:
    """Send a command, wait for response, return True on success (status==0)."""
    frame = build_frame(cmd, payload)
    for attempt in range(1, retries + 1):
        ser.reset_input_buffer()
        ser.write(frame)
        ser.flush()
        resp_cmd, status = read_response(ser, timeout)
        if resp_cmd == cmd and status == 0x00:
            return True
        if resp_cmd is not None:
            print(f"  [WARN] cmd=0x{cmd:02X} attempt {attempt}: "
                  f"resp_cmd=0x{resp_cmd:02X} status=0x{status:02X}")
        else:
            print(f"  [WARN] cmd=0x{cmd:02X} attempt {attempt}: no response (timeout)")
    return False


# ─── Main upgrade flow ────────────────────────────────────────────────
def upgrade(port: str, baud: int, fw_path: str):
    # Read firmware
    fw_data = open(fw_path, "rb").read()
    fw_size = len(fw_data)
    print(f"[*] Firmware: {os.path.basename(fw_path)} ({fw_size} bytes, "
          f"{fw_size / 1024:.1f} KB)")

    # Pad to page boundary
    if fw_size % FLASH_PAGE_SIZE != 0:
        pad = FLASH_PAGE_SIZE - (fw_size % FLASH_PAGE_SIZE)
        fw_data += b"\xFF" * pad
        print(f"[*] Padded with 0xFF to {len(fw_data)} bytes "
              f"({len(fw_data) // FLASH_PAGE_SIZE} pages)")

    total_pages = len(fw_data) // FLASH_PAGE_SIZE
    print(f"[*] Total pages to send: {total_pages}")

    # Open serial
    print(f"[*] Opening {port} @ {baud} baud ...")
    ser = serial.Serial(port, baud, timeout=0.1,
                        bytesize=serial.EIGHTBITS,
                        parity=serial.PARITY_NONE,
                        stopbits=serial.STOPBITS_ONE)

    try:
        # Query info
        print("[*] Querying STM32 partition info...")
        send_cmd(ser, CMD_INFO)
        time.sleep(0.5)
        # Flush any debug output
        while ser.in_waiting:
            line = ser.readline()
            if line:
                print(f"  [STM32] {line.decode('utf-8', errors='replace').strip()}")

        # Erase
        print("[*] Erasing storage area...")
        if not send_cmd(ser, CMD_ERASE, timeout=10.0):
            print("[ERR] Erase failed. Abort.")
            return
        print("  [OK] Storage erased")

        # Send pages
        fail_count = 0
        for page_idx in range(total_pages):
            offset = page_idx * FLASH_PAGE_SIZE
            page_data = fw_data[offset:offset + FLASH_PAGE_SIZE]
            payload = page_idx.to_bytes(2, "little") + page_data

            if not send_cmd(ser, CMD_DATA, payload, timeout=PAGE_TIMEOUT):
                fail_count += 1
                if fail_count >= MAX_RETRIES:
                    print(f"[ERR] Page {page_idx}/{total_pages} failed "
                          f"{fail_count} times. Abort.")
                    return
                print(f"  [RETRY] page {page_idx}")
                page_idx -= 1  # retry same page
                continue

            fail_count = 0
            progress = (page_idx + 1) * 100 // total_pages
            bar = "#" * (progress // 5) + "-" * (20 - progress // 5)
            print(f"  [{bar}] {progress:3d}%  page {page_idx + 1}/{total_pages}", end="\r")
            sys.stdout.flush()

        print()  # newline after progress bar

        # Finish
        print("[*] Sending finish command...")
        if not send_cmd(ser, CMD_FINISH):
            print("[ERR] Finish command failed.")
            return
        print("  [OK] Upgrade complete")

        # Print STM32 debug output
        time.sleep(0.5)
        while ser.in_waiting:
            line = ser.readline()
            if line:
                print(f"  [STM32] {line.decode('utf-8', errors='replace').strip()}")

    finally:
        ser.close()

    print("[*] Done. Reset board to boot into new firmware.")


def list_bin_files():
    """List available .bin files in tools/bin/."""
    if not os.path.isdir(BIN_DIR):
        print(f"[ERR] bin directory not found: {BIN_DIR}")
        return []
    files = sorted(f for f in os.listdir(BIN_DIR) if f.endswith(".bin"))
    if not files:
        print(f"[*] No .bin files found in {BIN_DIR}")
        return []
    print(f"[*] Available firmware files in bin/:")
    for f in files:
        path = os.path.join(BIN_DIR, f)
        size = os.path.getsize(path)
        print(f"    {f:30s}  {size:>7d} bytes  ({size / 1024:.1f} KB)")
    return files


# ─── CLI ───────────────────────────────────────────────────────────────
def main():
    parser = argparse.ArgumentParser(
        description="ROSbot IAP Firmware Upgrade Tool")
    parser.add_argument("firmware", nargs="?", help="Firmware .bin filename (inside tools/bin/)")
    parser.add_argument("--port", "-p", default="/dev/ttyUSB0",
                        help="Serial port (default: /dev/ttyUSB0)")
    parser.add_argument("--baud", "-b", type=int, default=115200,
                        help="Baud rate (default: 115200)")
    parser.add_argument("--list", "-l", action="store_true",
                        help="List available .bin files and exit")
    args = parser.parse_args()

    if args.list:
        list_bin_files()
        return

    if not args.firmware:
        parser.print_help()
        print("\n[*] Use --list to see available firmware files.")
        return

    # Resolve firmware path
    fw_path = os.path.join(BIN_DIR, args.firmware)
    if not os.path.isfile(fw_path):
        print(f"[ERR] Firmware file not found: {fw_path}")
        print("[*] Use --list to see available files.")
        sys.exit(1)

    upgrade(args.port, args.baud, fw_path)


if __name__ == "__main__":
    main()
