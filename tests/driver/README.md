# RegAnvil RP2040 GPIO interrupt driver integration

This original C99 driver exercises RegAnvil against a documented hardware
register subset. It is a test fixture, not a complete RP2040 driver or a claim
of validation on physical hardware. All code here is original, copyright
2026 RegAnvil contributors, under this repository's MIT license. No SDK source
is included.

## Register evidence

Checked against the official [RP2040 datasheet, section 2.19.6.1](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf#page=245)
on 2026-09-10. Offsets are relative to IO_BANK0 (`0x40014000`).

| Register | Byte offset | Model |
| --- | --- | --- |
| INTR0 | `0x0f0` | RO `0x33333333`, W1C `0xcccccccc`, reset zero |
| PROC0_INTE0 | `0x100` | RW `0xffffffff`, reset zero |

[Table 286, printed pages 249-250](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf#page=250)
gives INTR0's fields; [Table 290, printed pages 253-254](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf#page=254)
gives PROC0_INTE0. Each GPIO0-7 uses four bits: level-low, level-high,
edge-low, edge-high. GPIO7's rising-edge latch is bit 31.
[Appendix A, printed page 625](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf#page=626)
defines WC as clearing on a written one, with written zero having no effect;
RegAnvil calls this W1C.

## What runs

`rp2040_irq_driver.c` uses read/write callbacks and has no RegAnvil dependency.
`test_rp2040_irq_driver.c` supplies the adapter and descriptors. Hardware updates
explicitly provide raw level and edge state between completed driver calls.

The always-active tests exercise:

- Enabling/disabling all 16 event masks at each of eight pins while preserving
  unrelated fields, including bit 31.
- Reading raw events, selecting only requested enabled edges, acknowledging
  them, and retaining disabled, unrequested, and level events.
- Repeated acknowledgements, empty requests, and a new event after an
  acknowledgement completes.
- Every per-pin edge mask, invalid pin/mask/output/callback arguments,
  callback failures at each operation stage, and caller-output preservation.
- Independent instances, reset, and correct operation after trace overflow.
- Exact trace accesses and before/after values for the principal scenario.

The driver computes the intersection of two register reads in software. The
fixture does not implement INTS/INTF coupling, processor interrupt routing,
GPIO synchronizers or pad behavior, timing, atomic register aliases, or
concurrency between those reads. It neither predicts simultaneous set/clear
priority nor counts repeated edges before acknowledgement. Enable changes
require exclusive access across the read/modify/write operation.

## Run

The root CMake project includes this executable in CTest when tests are enabled.
It can also be compiled directly without CMake or any RP2040 SDK:

```sh
mkdir -p .build/driver
clang -std=c99 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -O2 -DNDEBUG -Iinclude -Itests/driver src/mxs_reganvil.c tests/driver/rp2040_irq_driver.c tests/driver/test_rp2040_irq_driver.c -o .build/driver/rp2040_irq_test
```

Run the executable; success prints the number of checks and exits zero. Any
failed check prints its expression and source location and exits nonzero.
