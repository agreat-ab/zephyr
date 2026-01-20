import isotp
import can
import time
from uds import UdsServer, services

bus = can.interface.Bus(channel='vcan0', bustype='socketcan')
tp_addr = isotp.Address(isotp.AddressingMode.Normal_11bits, txid=0x7E8, rxid=0x7E0)
stack = isotp.CanStack(bus=bus, address=tp_addr)

class MyECU:
    def __init__(self):
        self.server = UdsServer(stack)
        self.server.add_service(services.DiagnosticSessionControl())

    def run(self):
        print("ECU simulation running...")
        while True:
            self.server.step()
            time.sleep(0.01)

if __name__ == '__main__':
    ecu = MyECU()
    ecu.run()