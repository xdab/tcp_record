# AGENTS.md

tcp_record is a POSIX C program that connects to a TCP server streaming raw audio samples,
applies a squelch gate (noise-activated noise suppression), and auto-records transmissions to timestamped WAV files.
It supports multiple sample formats (s8, u8, s16, u16, f32) with configurable endianness
and includes a calibration mode for tuning squelch thresholds. Built with `gcc` and linked only against `libm`.

Read `README.md` for usage, CLI options, and examples.

## Project Structure

```text
include/    - Header files for each module (types, options, network, format, squelch, wav, calibration)
src/        - C source files (main + one per module)
Makefile    - Build system (gcc, -O2, -Wall -Wextra, -lm)
```

## Build & Verify

```sh
make clean && make       # build
./bin/tcp_record -h      # verify help output works
```

## Code Conventions

- Guard all headers with `#ifndef` / `#define` / `#endif`.
- Prefer early returns (guard clauses) over deep nesting. Flatten `if` chains by returning or continuing early.
- Maximum indentation depth: **4 levels**. If you find yourself going deeper, that is a signal to extract a function or restructure the control flow.

## Keeping Docs Up to Date

AGENTS.md and README.md are living documents.
If conventions change, new modules are added, build steps evolve, or usage shifts — update them to stay accurate.
README.md is the human entrypoint; AGENTS.md is for the AI agents.
