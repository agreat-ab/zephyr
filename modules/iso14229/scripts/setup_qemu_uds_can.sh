#!/bin/bash
# setup_qemu_uds_can.sh
# This script sets up a QEMU Raspberry Pi environment for UDS over CAN testing with vcan

set -e

# --- Step 1: Install Required Packages ---
echo "[+] Installing required packages..."
sudo apt update
sudo apt install -y qemu qemu-system-arm can-utils python3-pip unzip
pip3 install python-can isotp uds

# --- Step 2: Setup vcan interface ---
echo "[+] Setting up vcan interface..."
sudo modprobe can
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan || true
sudo ip link set up vcan0

# --- Step 3: Run UDS Client Test ---
echo "[+] Creating uds_client.py..."
cat << 'EOF' > uds_client.py
EOF

# --- Step 4: Create ECU Responder Script ---
echo "[+] Creating uds_responder.py..."
cat << 'EOF' > uds_responder.py
EOF

# --- Final Instructions ---
echo "[+] Setup complete!"
echo "To run your simulation:"
echo "1. In terminal 1: python3 uds_responder.py"
echo "2. In terminal 2 (after 1 second): python3 uds_client.py"