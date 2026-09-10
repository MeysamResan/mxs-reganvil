# Announcement draft for manual sharing

## RegAnvil 0.1.0: test embedded C register behavior without a board

I released RegAnvil, a small C99 library for native driver tests. It models 32-bit registers with RW, RO, write-one-to-clear, reserved-bit, and read-clear behavior, plus an optional access trace.

A typical bug it catches: a driver reads two pending interrupt flags and writes that value back while trying to acknowledge only one. On a W1C register, both flags disappear. RegAnvil lets a test check that the unhandled flag remains pending.

The core is one header and one C file, with caller-owned storage and no heap allocation or runtime dependencies. It includes CMake integration, a working demonstration, and an RP2040 register-subset fixture.

Drivers need replaceable register accessors. The library models register rules; CPU emulation and physical-hardware validation are outside its scope.

- [Source and documentation](https://github.com/MeysamResan/mxs-reganvil)
- [Release 0.1.0](https://github.com/MeysamResan/mxs-reganvil/releases/tag/v0.1.0)
- License: MIT

Feedback from people testing embedded C drivers would help identify useful integration cases.

---

Before sharing, follow the community's posting rules and adapt the text to its audience. Add the documentation-site URL only after it is deployed. Do not describe Context7 availability until indexing is confirmed, or claim that the overlay is accepted into vcpkg's curated registry.
