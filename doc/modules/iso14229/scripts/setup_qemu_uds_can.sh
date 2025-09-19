#!/usr/bin/env bash
# scripts/setup_qemu_uds_can.sh
#
# Optional helper script to set up a virtual CAN (vcan) interface
# for manual UDS/ISO-TP testing under QEMU.
#
# NOT required for unit tests or CI.

set -e

echo "[+] Setting up virtual CAN interface for QEMU testing"

# Kernel modules
sudo modprobe can
sudo modprobe vcan

# Create vcan0 if it doesn't exist
if ! ip link show vcan0 &>/dev/null; then
	echo "[+] Creating vcan0"
	sudo ip link add dev vcan0 type vcan
fi

sudo ip link set up vcan0

echo "[+] vcan0 is up"
echo
echo "This script is OPTIONAL."
echo "It is only needed for manual UDS testing under QEMU."
