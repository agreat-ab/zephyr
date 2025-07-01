import isotp
import can
from uds import UdsClient
import time

bus = can.interface.Bus(channel='vcan0', bustype='socketcan')
tp_addr = isotp.Address(isotp.AddressingMode.Normal_11bits, txid=0x7E0, rxid=0x7E8)
stack = isotp.CanStack(bus=bus, address=tp_addr)

client = UdsClient(stack)
time.sleep(1)  # give the responder time to come online
response = client.diagnostic_session_control(0x01)
print("Response:", response)