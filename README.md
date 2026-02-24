<pre align="center">
  ██████╗ ██╗   ██╗███████╗██╗   ██╗███████╗██████╗ ██████╗  ██████╗ ██████╗ ███████╗
 ██╔═══██╗██║   ██║██╔════╝██║   ██║██╔════╝██╔══██╗██╔══██╗██╔═══██╗██╔══██╗██╔════╝
 ██║   ██║██║   ██║█████╗  ██║   ██║█████╗  ██████╔╝██████╔╝██║   ██║██████╔╝█████╗
 ██║▄▄ ██║██║   ██║██╔══╝  ██║   ██║██╔══╝  ██╔═══╝ ██╔══██╗██║   ██║██╔══██╗██╔══╝
 ╚██████╔╝╚██████╔╝███████╗╚██████╔╝███████╗██║     ██║  ██║╚██████╔╝██████╔╝███████╗
  ╚══▀▀═╝  ╚═════╝ ╚══════╝ ╚═════╝ ╚══════╝╚═╝     ╚═╝  ╚═╝ ╚═════╝ ╚═════╝ ╚══════╝
</pre>

[![Version](https://img.shields.io/badge/version-v1.0.1-brightgreen)](CHANGELOG.md)
[![Language](https://img.shields.io/badge/Language-C-blue)](https://en.wikipedia.org/wiki/C_(programming_language))
[![Platform](https://img.shields.io/badge/Platform-Windows%2064--bit-0078D4?logo=windows)](https://www.microsoft.com/windows)
[![Platform](https://img.shields.io/badge/Platform-Linux%2064--bit-FCC624?logo=linux&logoColor=black)](LINUX_BUILD.md)
[![Solace C SDK](https://img.shields.io/badge/Solace%20C%20SDK-7.33.1.1-green)](https://solace.com/downloads/)
[![License](https://img.shields.io/badge/License-Apache%202.0-yellow.svg)](LICENSE)
[![TLS](https://img.shields.io/badge/TLS-OpenSSL%203.x-red)](https://www.openssl.org/)

👋 If you find this utility useful, please give it a ⭐ above! Thanks! 🙏

A lightweight Solace PubSub+ message consumer written in C. Supports **queue consuming**, **queue browsing**, and **topic subscribing** — all with colour-coded output, JSON pretty-printing, SSL/TLS, and SOCKS5/HTTP-Connect proxy support.

Pre-compiled binaries for **Windows** and **Linux** (64-bit) included — no SDK install needed.

Built with inspiration from the formatting logic found in [solace-pretty-dump](https://github.com/SolaceLabs/solace-pretty-dump).

---

> **[📦 Plug & Play — Download pre-compiled binary](#quick-start)**  &nbsp;|&nbsp;  **[🔧 Build from Source](#building-from-source)**

---

## Table of Contents

- [Table of Contents](#table-of-contents)
- [Quick Start](#quick-start)
  - [Windows](#quick-start-windows)
  - [Linux](#quick-start-linux)
- [Features](#features)
  - [Messaging Modes](#messaging-modes)
  - [Connectivity](#connectivity)
  - [Output \& Logging](#output--logging)
  - [Configuration](#configuration)
- [Modes](#modes)
  - [Queue Consumer *(default)*](#queue-consumer-default)
  - [Browse Mode (`--browse`)](#browse-mode---browse)
  - [Topic Subscriber (`--topic`)](#topic-subscriber---topic)
- [CLI Options](#cli-options)
- [Config File](#config-file)
- [TLS / SSL](#tls--ssl)
- [Proxy Support](#proxy-support)
- [Building from Source](#building-from-source)
  - [Prerequisites](#prerequisites)
  - [Windows (MinGW/MSYS2 UCRT64)](#windows-mingwmsys2-ucrt64)
  - [Linux](#linux-1)
- [How It Works](#how-it-works)
- [Files](#files)
- [Dependencies](#dependencies)
- [License](#license)

---

## Quick Start

Download the latest release for your platform from the **[Releases](../../releases/latest)** page — all required libraries are bundled, no SDK install needed.

### Quick Start (Windows)

Extract `QueueProbe-vX.Y.Z-windows-x64.zip` and run from the extracted folder:

```cmd
QueueProbe.exe <queue_name> --host tcp://<broker>:<port> --vpn <vpn> --username <user> --password <pass>
```

**Example — queue consumer (plain TCP):**
```cmd
QueueProbe.exe q.test --host tcp://broker.example.com:55555 --vpn my_vpn --username user --password pass
```

**Example — queue consumer (TLS):**
```cmd
QueueProbe.exe q.test --host tcps://mr-connection-xxx.messaging.solace.cloud:55443 --vpn my_vpn --username solace-cloud-client --password mypassword
```

**Example — via SOCKS5 proxy:**
```cmd
QueueProbe.exe q.test --host tcp://broker.example.com:55555 --vpn my_vpn --username user --password pass --proxy "socks5://proxy.example.com:1080"
```

> **TLS note:** `libcrypto-3.dll` and `libssl-3.dll` are bundled — no separate OpenSSL install needed.

---

### Quick Start (Linux)

Extract `QueueProbe-vX.Y.Z-linux-x64.tar.gz` and run from the extracted folder:

```bash
tar -xzf QueueProbe-vX.Y.Z-linux-x64.tar.gz
cd QueueProbe-vX.Y.Z-linux-x64
chmod +x QueueProbe
./QueueProbe <queue_name> --host tcp://<broker>:<port> --vpn <vpn> --username <user> --password <pass>
```

**Example — queue consumer (plain TCP):**
```bash
./QueueProbe q.test --host tcp://broker.example.com:55555 --vpn my_vpn --username user --password pass
```

**Example — queue consumer (TLS):**
```bash
./QueueProbe q.test --host tcps://mr-connection-xxx.messaging.solace.cloud:55443 --vpn my_vpn --username solace-cloud-client --password mypassword
```

**Example — via SOCKS5 proxy:**
```bash
./QueueProbe q.test --host tcp://broker.example.com:55555 --vpn my_vpn --username user --password pass --proxy "socks5://proxy.example.com:1080"
```

> **`libsolclient.so` is bundled** in the tar.gz. The binary uses `rpath=$ORIGIN` so it loads the library from its own directory automatically — no `LD_LIBRARY_PATH` export or root access needed.
>
> OpenSSL is **not** bundled — it comes pre-installed on any modern Linux distro (`libssl`, `libcrypto`).

---

## Features

### Messaging Modes
- **Queue Consumer** — binds to a durable queue and consumes with explicit client ACK; messages are removed after delivery
- **Browse Mode** (`--browse`) — inspect queue contents non-destructively; messages remain on the queue
- **Topic Subscriber** (`--topic`) — session-level direct topic subscriptions; flag is repeatable for multiple topics

### Connectivity
- **SSL/TLS** (`tcps://`) — full TLS transport via OpenSSL; bundled on Windows, system-provided on Linux
- **SOCKS5 / HTTP-Connect proxy** (`--proxy`) — tunnel the Solace connection through a proxy
- **Certificate validation** (`--certdir`) — point at a trust-store directory for production cert checking; `--no-verify` to skip for dev/self-signed certs

### Output & Logging
- **ANSI colour-coded log levels** (INFO / WARN / ERROR / DEBUG) with millisecond-precision timestamps
- **JSON auto-detection** — payloads that are valid JSON are automatically pretty-printed with syntax highlighting
- **Clean log file** — ANSI colour codes are stripped automatically; log file is always plain text even when colour is on in the terminal
- **Rich message metadata** — AppMsgId, ReplyTo, CorrelationId, SenderId, DeliveryMode, ClassOfService, Priority, TTL, DMQEligible logged per message
- **TTY auto-detection** — colours are automatically disabled when output is piped or redirected

### Configuration
- **Config file** (`queueprobe.conf`) — store all settings in a `key = value` file; auto-loaded from the binary's directory with no extra flags
- **Flexible config loading** — pass `--config <path>` for an explicit path, or use a positional `.conf` argument
- **CLI always wins** — any command-line flag overrides the config file value
- **Inline comments** — config values support `# comment` suffix (e.g. `logfile = out.log  # daily log`)

---

## Modes

> On Linux replace `QueueProbe.exe` with `./QueueProbe` in all examples below.

### Queue Consumer *(default)*

Binds to a durable queue and consumes messages. Each message is explicitly ACKed so it is removed from the queue after delivery.

```cmd
QueueProbe.exe <queue_name> [options]
```

### Browse Mode (`--browse`)

Reads messages off the queue **without consuming them** — messages remain on the queue. Useful for inspecting queue contents non-destructively.

```cmd
QueueProbe.exe <queue_name> --browse [options]
```

### Topic Subscriber (`--topic`)

Subscribes to one or more direct topics at the session level. Use `--topic` multiple times for multiple subscriptions.

```cmd
QueueProbe.exe --topic "market/data/>" --topic "orders/new" [options]
```

---

## CLI Options

| Option | Default | Description |
|--------|---------|-------------|
| `<queue_name>` | *(required for queue/browse)* | Name of the durable queue |
| `--host <url>` | `tcp://localhost:55555` | Broker URL (`tcp://` or `tcps://`) |
| `--vpn <name>` | `default` | Message VPN name |
| `--username <user>` | `default` | Client username |
| `--password <pass>` | *(empty)* | Client password |
| `--proxy <url>` | *(none)* | Proxy URL |
| `--browse` | *(off)* | Enable browse mode (no ACK) |
| `--topic <topic>` | *(none)* | Topic subscription (repeatable) |
| `--certdir <path>` | *(none)* | Trust-store directory (enables cert validation) |
| `--no-verify` | *(off)* | Disable SSL certificate validation |
| `--logfile <file>` | `solace_debug.log` | Log output file path |
| `--config <file>` | *(none)* | Load settings from a config file |
| `--no-color` | *(off)* | Disable ANSI colour output |

---

## Config File

For repeated use, store all settings in a plain-text `key = value` file instead of typing flags every time.

**Auto-load** — place a file named `queueprobe.conf` in the same directory as the binary. It is loaded automatically with no extra flags needed.

**Explicit path:**
```bash
# Windows
QueueProbe.exe --config C:\configs\prod.conf

# Linux
./QueueProbe --config /etc/queueprobe/prod.conf
```

**CLI args always win** — any flag passed on the command line overrides the config file.

**Example `queueprobe.conf`:**
```ini
# QueueProbe config
host     = tcps://mr-connection-xxx.messaging.solace.cloud:55443
vpn      = my_vpn
username = solace-cloud-client
password = mypassword
queue    = q.test
logfile  = test.log
```

For a fully commented template see [`queueprobe.conf.example`](windows/queueprobe.conf.example) in this repo.

> **Security:** `queueprobe.conf` is listed in `.gitignore` — it will never be accidentally committed since it typically contains credentials.

---

## TLS / SSL

**With certificate validation (recommended for production):**
```bash
# Windows
QueueProbe.exe q.test --host tcps://broker:55443 --vpn vpn1 --username user --password pass --certdir C:\certs\ca

# Linux
./QueueProbe q.test --host tcps://broker:55443 --vpn vpn1 --username user --password pass --certdir /etc/ssl/certs
```

**Without certificate validation (useful for self-signed certs / dev):**
```bash
QueueProbe.exe q.test --host tcps://broker:55443 --vpn vpn1 --username user --password pass --no-verify
```

> By default, SSL certificate validation is **disabled**. Use `--certdir` to enable it for production.
>
> On Windows, `libcrypto-3.dll` and `libssl-3.dll` are bundled alongside the binary. On Linux, the Solace SDK uses the system OpenSSL — no extra files needed.

---

## Proxy Support

Proxy is specified using `--proxy` with one of the following URL formats:

```
socks5://192.168.1.1:1080
httpc://proxy.company.com:3128
socks5://user:password@proxy.company.com:13128
```

Internally the SDK's `host%proxy` notation is used — the proxy URL is appended to the broker host with a `%` separator, which is the Solace SDK convention for proxy-aware connections.

---

## Building from Source

> See [LINUX_BUILD.md](LINUX_BUILD.md) for a detailed step-by-step Linux build guide.

### Prerequisites

Download the [Solace C API](https://solace.com/downloads/) for your platform and extract it alongside the repo root.

### Windows (MinGW/MSYS2 UCRT64)

```bash
gcc src/solace_consumer_debug.c \
  -I solclient-7.33.1.1-win/include \
  -L solclient-7.33.1.1-win/lib/Win64 \
  -DSOLCLIENT_CONST_PROPERTIES \
  -lsolclient -lws2_32 \
  -static-libgcc \
  -o windows/QueueProbe.exe
```

> **`-DSOLCLIENT_CONST_PROPERTIES`** is required — without it the SDK typedef resolves to `char**` instead of `const char**`, causing compile errors on property arrays.
>
> **`-static-libgcc`** embeds the GCC runtime so the exe only needs `libsolclient.dll` plus the Windows UCRT (built into Windows 10+).

### Linux

```bash
export SOLACE_HOME=/path/to/solclient-7.33.1.1

gcc src/solace_consumer_debug.c \
  -I$SOLACE_HOME/include \
  -L$SOLACE_HOME/lib \
  -DSOLCLIENT_CONST_PROPERTIES \
  -Wl,-rpath,'$ORIGIN' \
  -lsolclient \
  -o linux/QueueProbe

# Bundle the Solace library for a self-contained folder
cp $SOLACE_HOME/lib/libsolclient.so* linux/
```

> **`-Wl,-rpath,'$ORIGIN'`** makes the binary look for `.so` files in its own directory at runtime — same behaviour as Windows DLLs placed next to an `.exe`. No `LD_LIBRARY_PATH` needed by end users.

Or use the Makefile (from the `src/` directory):

```bash
SOLACE_HOME=/path/to/solclient-7.33.1.1 make linux
```

---

## How It Works

The consumer follows the standard Solace C API lifecycle:

1. **Initialize** — `solClient_initialize()` sets up the library
2. **Context** — `solClient_context_create()` starts the event dispatch thread
3. **Session** — `solClient_session_create()` + `solClient_session_connect()` connects to the broker
4. **Flow** *(queue/browse mode)* — `solClient_session_createFlow()` binds to the queue
5. **Subscription** *(topic mode)* — `solClient_session_topicSubscribeExt()` adds topic subscriptions
6. **Receive** — message callback fires per message; in queue mode each message is explicitly acknowledged with `solClient_flow_sendAck(flow, msgId)`; browse mode receives without ACK
7. **Shutdown** — `SIGINT`/`SIGTERM` triggers ordered cleanup: flow → session → context

**Proxy mechanism:** when `--proxy` is passed, the broker and proxy URLs are combined into a single `SOLCLIENT_SESSION_PROP_HOST` value using `%` as separator: `tcp://broker:55555%socks5://proxy:1080`.

---

## Files

```
├── src/
│   └── solace_consumer_debug.c     # Single source file (Windows + Linux)
├── windows/
│   ├── QueueProbe.exe              # Pre-compiled Windows 64-bit binary
│   ├── libsolclient.dll            # Solace client library
│   ├── libcrypto-3.dll             # OpenSSL — required for tcps://
│   ├── libssl-3.dll                # OpenSSL — required for tcps://
│   └── queueprobe.conf.example     # Config file template
├── linux/
│   ├── QueueProbe                  # Pre-compiled Linux 64-bit binary
│   ├── libsolclient.so             # Solace client library (symlink)
│   └── libsolclient.so.1.7.33.1.1 # Solace client library (versioned)
├── .github/workflows/release.yml  # Automated release workflow
├── LINUX_BUILD.md                  # Linux build guide
├── CHANGELOG.md
├── LICENSE
└── README.md
```

---

## Dependencies

| Library | Version | Windows | Linux |
|---------|---------|---------|-------|
| libsolclient | 7.33.1.1 | bundled (`libsolclient.dll`) | bundled (`libsolclient.so`) |
| OpenSSL | 3.x | bundled (`libcrypto-3.dll`, `libssl-3.dll`) | system-provided |
| C runtime | — | Windows UCRT (built into Windows 10+) | glibc (built into any modern distro) |

---

## License

Copyright 2025 Tanendra77 (github.com/Tanendra77)

Licensed under the [Apache License, Version 2.0](LICENSE).
