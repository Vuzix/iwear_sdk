# iwear_sdk

VuzixAPI.h:
This is the shared API definition between the host USB interface and the
device interface running on the internal processors of the iWear devices.

This file should remain system agnostic and ensure that byte packing is
enabled otherwise disconnects will happen between the hardware messaging
and the host decoding.

This file should not assume any floating point capabilities as some of the
iWear devices run simple microcontrollers that will not be able to process
floating point math.


VuzixUSBProtocal.cpp/h:
This is the reference interface which shows the processes by which 
communications with the device can be established.
