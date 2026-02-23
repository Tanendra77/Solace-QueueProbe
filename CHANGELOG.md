# Changelog

All notable changes to **QueueProbe** will be documented in this file.

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).
This project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [v1.0.1] — 2026-02-23

### Added

#### Config File Support
- **Auto-load** — if `queueprobe.conf` exists in the exe directory it is loaded automatically with no flags needed
- **`--config <path>`** flag — explicit config file path
- **Positional `.conf` argument** — `QueueProbe.exe myenv.conf` is treated as a config file path
- Three-tier priority: hardcoded defaults → config file → CLI args (CLI always wins)
- `key = value` format; full-line `#` / `;` comments and inline `[whitespace]#` comments supported
- `queueprobe.conf.example` template added to the repo
- `queueprobe.conf` and `*.conf` added to `.gitignore` to prevent credential leaks

#### Output & Logging
- **`--no-color` flag** — disable all ANSI colour output
- **TTY auto-detection** — colours are automatically disabled when stdout is not a terminal (piped / redirected)
- **Clean log file** — ANSI escape sequences are stripped before writing to the log file; log is always plain text
- **Payload in log** — message payload (JSON or binary) is now written to the log file, not only to the terminal
- **Message separators in log** — `+----- Message #N -----+` header and closing line written to log for easy parsing
- **Extended message metadata** — the following fields are now logged per message: `AppMsgId`, `ReplyTo`, `CorrelationId`, `SenderId`, `ClassOfService`, `Priority`, `TTL`, `DMQEligible`

---

## [v1.0.0] — 2025-02-22

Initial public release of **QueueProbe** — a lightweight Solace PubSub+ message consumer written in C.

### Added

#### Core Features
- **Queue consumer mode** (default): binds to a durable queue and consumes messages with explicit client ACK (`SOLCLIENT_FLOW_PROP_ACKMODE_CLIENT`)
- **Browse mode** (`--browse`): reads queue messages non-destructively — no ACK, messages stay on the queue
- **Topic subscriber mode** (`--topic`): session-level direct topic subscriptions; `--topic` flag is repeatable for multiple subscriptions

#### Connectivity
- **SSL/TLS support** (`tcps://`): full TLS transport using OpenSSL 3.x loaded at runtime; `libcrypto-3.dll` and `libssl-3.dll` bundled in `windows/`
- **Certificate validation** (`--certdir`): optional trust-store directory for production certificate verification
- **`--no-verify`** flag: skip SSL cert validation for self-signed certs / dev environments
- **SOCKS5 proxy** (`--proxy socks5://...`): tunnels the Solace connection through a SOCKS5 proxy using the Solace SDK `host%proxy` notation
- **HTTP-Connect proxy** (`--proxy httpc://...`): HTTP CONNECT tunnel support

#### Output & Logging
- ANSI colour-coded log levels (INFO, WARN, ERROR, DEBUG) with millisecond-precision timestamps
- **JSON auto-detection**: message payloads that are valid JSON are automatically pretty-printed with syntax highlighting
- Simultaneous output to stdout and a rotating log file (`--logfile`, default: `solace_debug.log`)
- Full session and flow event diagnostics with response codes
- Custom Solace SDK log callback at NOTICE level for SDK-internal events
- **ASCII art banner** ("QueueProbe" in bold green, "by Tanendra77@Github" in orange) on startup
- UTF-8 console output on Windows (`SetConsoleOutputCP(CP_UTF8)`) so em-dashes and other Unicode render correctly
- ANSI virtual terminal processing enabled on Windows 10+ (`ENABLE_VIRTUAL_TERMINAL_PROCESSING`)

#### CLI Options
| Option | Description |
|--------|-------------|
| `<queue_name>` | Durable queue to consume/browse |
| `--host <url>` | Broker URL (`tcp://` or `tcps://`), default `tcp://localhost:55555` |
| `--vpn <name>` | Message VPN name |
| `--username` / `--password` | Client credentials |
| `--proxy <url>` | SOCKS5 or HTTP-Connect proxy URL |
| `--browse` | Browse mode (no ACK) |
| `--topic <topic>` | Topic subscription (repeatable) |
| `--certdir <path>` | Trust-store directory for TLS cert validation |
| `--no-verify` | Disable TLS certificate verification |
| `--logfile <path>` | Log file path |

#### Packaging
- Pre-compiled **Windows 64-bit binary** (`QueueProbe.exe`) built with MSYS2 UCRT64 GCC — no SDK install required
- `libsolclient.dll` (Solace C API v7.33.1.1) bundled
- `libcrypto-3.dll` and `libssl-3.dll` (OpenSSL 3.x) bundled for `tcps://` connections
- `-static-libgcc` used — no MinGW runtime DLL dependency; runs on Windows 10+ out of the box

### Fixed
- Session rx callback always set (not only in topic mode) — resolves `Null rx callback pointer in solClient_session_create` crash when connecting to queues/browse flows

---

[v1.0.1]: https://github.com/Tanendra77/Solace-QueueProbe/releases/tag/v1.0.1
[v1.0.0]: https://github.com/Tanendra77/Solace-QueueProbe/releases/tag/v1.0.0
