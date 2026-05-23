import { ESPLoader, Transport } from "esptool-js";

import type { FirmwareRelease } from "@/entities/firmware-release";

import { FlashCancelledError } from "../model/types";

export type FlasherHandle = {
  loader: ESPLoader;
  transport: Transport;
  chip: string;
};

export type FlashProgress = {
  percent: number;
  currentFile: "bootloader" | "partitions" | "firmware";
};

const BOOTLOADER_OFFSET = 0x1000;
const PARTITIONS_OFFSET = 0x8000;
const FIRMWARE_OFFSET = 0x10000;
const FLASH_BAUD = 921600;

function isUserCancellation(err: unknown): boolean {
  if (!(err instanceof DOMException)) return false;
  return err.name === "NotFoundError" || err.name === "SecurityError";
}

export async function openFlasher(): Promise<FlasherHandle> {
  if (!("serial" in navigator)) {
    throw new Error("Web Serial is not supported in this browser.");
  }

  let port: SerialPort;
  try {
    port = await navigator.serial.requestPort();
  } catch (err) {
    if (isUserCancellation(err)) {
      throw new FlashCancelledError();
    }
    throw err;
  }

  const transport = new Transport(port, false);
  const loader = new ESPLoader({
    transport,
    baudrate: FLASH_BAUD,
  });

  const chip = await loader.main();
  return { loader, transport, chip };
}

export async function closeFlasher(handle: FlasherHandle): Promise<void> {
  try {
    await handle.transport.disconnect();
  } catch {
    // best effort: port may already be closed if the device rebooted
  }
}

async function downloadBinary(url: string): Promise<Uint8Array> {
  const res = await fetch(url);
  if (!res.ok) {
    throw new Error(`Failed to download ${url}: ${String(res.status)}`);
  }
  const buf = await res.arrayBuffer();
  return new Uint8Array(buf);
}

export async function flashRelease(
  handle: FlasherHandle,
  release: FirmwareRelease,
  onProgress: (progress: FlashProgress) => void,
): Promise<void> {
  const [bootloader, partitions, firmware] = await Promise.all([
    downloadBinary(release.assets.bootloader.downloadUrl),
    downloadBinary(release.assets.partitions.downloadUrl),
    downloadBinary(release.assets.firmware.downloadUrl),
  ]);

  const fileArray = [
    { data: bootloader, address: BOOTLOADER_OFFSET },
    { data: partitions, address: PARTITIONS_OFFSET },
    { data: firmware, address: FIRMWARE_OFFSET },
  ] as const;

  const fileLabels: FlashProgress["currentFile"][] = [
    "bootloader",
    "partitions",
    "firmware",
  ];

  const totals = fileArray.map((f) => f.data.length);
  const grandTotal = totals.reduce((a, b) => a + b, 0);
  const cumulativeBefore: number[] = [];
  let running = 0;
  for (const size of totals) {
    cumulativeBefore.push(running);
    running += size;
  }

  await handle.loader.writeFlash({
    fileArray: fileArray.map((f) => ({ data: f.data, address: f.address })),
    flashMode: "keep",
    flashFreq: "keep",
    flashSize: "keep",
    eraseAll: false,
    compress: true,
    reportProgress: (fileIndex, written, _total) => {
      const before = cumulativeBefore[fileIndex] ?? 0;
      const label = fileLabels[fileIndex] ?? "firmware";
      const percent = Math.min(
        100,
        Math.round(((before + written) / grandTotal) * 100),
      );
      onProgress({ percent, currentFile: label });
    },
  });

  try {
    await handle.loader.after("hard_reset");
  } catch {
    // hardware reset is best-effort: some boards reboot before the
    // command returns, which surfaces as a transport error
  }
}
