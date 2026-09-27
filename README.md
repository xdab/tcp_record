# tcp_record

A lightweight TCP audio stream recorder with squelch gating. Connects to a TCP server serving raw audio samples, applies a noise-activated squelch gate, and automatically records transmissions to WAV files.

## Building

```sh
make
```

The binary is placed in `bin/tcp_record`.

## Usage

```sh
tcp_record -a <addr> -p <port> -f <format> [options]
```

### Options

| Flag                  | Description                                                                                                         |
| --------------------- | ------------------------------------------------------------------------------------------------------------------- |
| `-a, --addr`          | Server address (IP or hostname)                                                                                     |
| `-p, --port`          | Server port                                                                                                         |
| `-I, --file-input`    | Read raw samples from `<file>` instead of TCP (mutually exclusive with `-a`/`-p`)                                   |
| `-f, --format`        | Sample format: `s8`, `u8`, `s16`, `u16`, `f32` (append `le`/`be` for endianness, e.g. `s16be`; default: big-endian) |
| `-r, --rate`          | Sample rate (default: 48000)                                                                                        |
| `-s, --sql`           | Squelch open threshold (0 = off)                                                                                    |
| `-S, --sql-close`     | Squelch close threshold (default: same as `-s`)                                                                     |
| `-b, --bandwidth`     | Signal bandwidth in Hz for envelope normalization                                                                   |
| `-P, --record`        | Auto-record: write WAV files on squelch open                                                                        |
| `-L, --label`         | Filename label (default: `REC`)                                                                                     |
| `--min-duration`      | Discard recordings shorter than `<s>` seconds (default: 0.25)                                                       |
| `--tsql <hz>`         | CTCSS tone squelch: gate on a tone at `<hz>` (e.g. `127.3`)                                                         |
| `--tsql-level <dbfs>` | Tone gate threshold in dBFS (default: -33)                                                                          |
| `--tsql-delay <ms>`   | Ms below threshold before the tone gate closes (default: 100)                                                       |
| `-D, --recdir`        | Directory for recorded WAVs (default: cwd)                                                                          |
| `-o, --stdout`        | Output s16le samples to stdout                                                                                      |
| `-c, --cal`           | Calibrate: print envelope histogram to help tune thresholds                                                         |
| `-d`                  | Enable debug output                                                                                                 |
| `-h, --help`          | Display help                                                                                                        |

### Examples

Record from a TCP audio server with auto-recording:

```sh
tcp_record -a 192.168.1.100 -p 7475 -f s16be -s 0.02 -P -D ./recordings
```

Record only while a 127.3 Hz CTCSS tone is present (threshold varies per tx; run with `-c --tsql` to see the actual levels):

```sh
tcp_record -a 192.168.1.100 -p 7475 -f s16le -r 16000 --tsql 127.3 \
    --tsql-level -34 -P -D ./recordings
```

Pipe audio to stdout:

```sh
tcp_record -a 192.168.1.100 -p 7475 -f s16be -s 0.02 -o | aplay -r 48000 -f S16_LE -c 1
```

Calibrate squelch thresholds:

```sh
tcp_record -a 192.168.1.100 -p 7475 -f s16be -s 0.02 -b 6000 -c
```

Replay a raw capture file through the same pipeline (useful for testing
thresholds without a live server):

```sh
tcp_record -I capture.raw -f s16le -r 16000 -s 0.33 -S 0.67 -b 12500 \
    --tsql 127.3 -P -D ./recordings
```

## How It Works

1. Connects to the TCP server and receives raw audio bytes
2. Converts samples to floating point (supports s8, u8, s16, u16, f32 with byte-swap handling)
3. Applies a squelch gate: high-pass filters the signal, computes an envelope, and opens/closes based on thresholds with hysteresis
4. Optionally gates on a CTCSS tone level (`--tsql`): opens when the tone level crosses `--tsql-level`, closes after `--tsql-delay` ms below it; ANDed with the noise squelch when both are enabled
5. When the combined gate opens, starts writing a timestamped WAV file
6. When the combined gate closes, stops recording
7. Gracefully exits and patches WAV headers on SIGINT/SIGTERM

## WAV Files

Recordings are written to a temporary file (`TEMP_<hex>.wav`) and renamed to the final name only when the recording is long enough. Short recordings (below `--min-duration`) are deleted without ever appearing under the final name.

Final filenames follow the pattern `<label>_<port>_<YYYYMMDD_HHMMSS>.wav`, e.g. `REC_7475_20260822_221728.wav`. If a recording would overwrite an existing file (e.g. two recordings started within the same second), a `_N` suffix is appended.
