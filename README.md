# pan_tilt

Firmware for a pan/tilt turret on an **STM32F411CEU6**. Two servos aim the
head, a laser marks the target, an HC-SR04 measures range, and a Raspberry Pi
drives it all over one UART link.

Work in progress.

## Pins

| Pin | Function |
|---|---|
| PA6 | TIM3_CH1 PWM — pan servo |
| PA7 | TIM3_CH2 PWM — tilt servo |
| PA9 / PA10 | USART1 TX / RX — Pi link |
| PB0 | Laser, via an NPN low-side switch |
| PB6 | TIM4_CH1 input capture — HC-SR04 ECHO |
| PB7 | GPIO output — HC-SR04 TRIG |

PB0 drives a transistor base through 1 kΩ, not the laser directly. The HC-SR04
is a 5 V part — check PB6 is 5 V tolerant or fit a divider on ECHO.

## Serial

115200 8N1, lines ending in `\n` or `\r`.

```
LASER:ON        ->  OK
LASER:OFF       ->  OK
<anything else> ->  ERR:BADCMD
```

The board sends `BOOT:pan_tilt` at startup and `RANGE:47` (centimetres) about
ten times a second.

## Build

Needs [Arm GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads),
GNU Make, and a SEGGER J-Link. Toolchain paths are at the top of `GNUmakefile`
and in `.vscode/settings.json` — edit them for your machine.

```bash
make            # build
make flash      # build and program over SWD
make range      # live HC-SR04 readings
make help       # all targets
```

`make gdbserver` and `make debug` for GDB; F5 in VS Code with Cortex-Debug.

## CubeMX

`Makefile` is generated from `pan_tilt.ioc` and edits to it are lost. Custom
targets live in `GNUmakefile`, which Make prefers and CubeMX never touches.
It also re-scans `Core/Src`, so new files build with no config change.

## Safety

This is an eye hazard on a head that moves under software control. Point it at
a wall, not a window.

## License

MIT — see [LICENSE](LICENSE).
