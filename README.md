# myOS

My try to create kind of OS for RISC-V board VisionFive 2 on JH7110 chip.


Build on board under Linux.
Copy to /boot/.
Run from U-Boot.

## Supported boards
- VisionFive2
- Banana Pi BPI-F3
- QEMU

## Building:
meson setup build -Dboard=bpi-f3
ninja -C build

## Run
