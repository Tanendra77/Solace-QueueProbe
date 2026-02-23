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
[![Solace C SDK](https://img.shields.io/badge/Solace%20C%20SDK-7.33.1.1-green)](https://solace.com/downloads/)
[![License](https://img.shields.io/badge/License-Apache%202.0-yellow.svg)](LICENSE)
[![TLS](https://img.shields.io/badge/TLS-OpenSSL%203.x-red)](https://www.openssl.org/)

👋 If you find this utility useful, please give it a ⭐ above! Thanks! 🙏

A lightweight Solace PubSub+ message consumer written in C. Supports **queue consuming**, **queue browsing**, and **topic subscribing** — all with colour-coded output, JSON pretty-printing, SSL/TLS, and SOCKS5/HTTP-Connect proxy support.

Pre-compiled Windows 64-bit binary included — no SDK install needed.

Built with inspiration from the formatting logic found in [solace-pretty-dump](https://github.com/SolaceLabs/solace-pretty-dump).

---

## Table of Contents

- [Table of Contents](#table-of-contents)
- [Features](#features)
  - [Messaging Modes](#messaging-modes)
  - [Connectivity](#connectivity)
  - [Output \& Logging](#output--logging)
  - [Configuration](#configuration)
- [Quick Start (Windows)](#quick-start-windows)
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
  - [Linux](#linux)
  - [Windows (MinGW/MSYS2 UCRT64)](#windows-mingwmsys2-ucrt64)
- [How It Works](#how-it-works)
- [Files](#files)
- [Dependencies](#dependencies)
- [License](#license)

---

## Features

### Messaging Modes
- **Queue Consumer** — binds to a durable queue and consumes with explicit client ACK; messages are removed after delivery
- **Browse Mode** (`--browse`) — inspect queue contents non-destructively; messages remain on the queue
- **Topic Subscriber** (`--topic`) — session-level direct topic subscriptions; flag is repeatable for multiple topics

### Connectivity
- **SSL/TLS** (`tcps://`) — full TLS transport via OpenSSL 3.x; DLLs bundled, no separate install needed
- **SOCKS5 / HTTP-Connect proxy** (`--proxy`) — tunnel the Solace connection through a proxy
- **Certificate validation** (`--certdir`) — point at a trust-store directory for production cert checking; `--no-verify` to skip for dev/self-signed certs

### Output & Logging
- **ANSI colour-coded log levels** (INFO / WARN / ERROR / DEBUG) with millisecond-precision timestamps
- **JSON auto-detection** — payloads that are valid JSON are automatically pretty-printed with syntax highlighting
- **Clean log file** — ANSI colour codes are stripped automatically; log file is always plain text even when colour is on in the terminal
- **Rich message metadata** — AppMsgId, ReplyTo, CorrelationId, SenderId, DeliveryMode, ClassOfService, Priority, TTL, DMQEligible logged per message
- **TTY auto-detection** — colours are automatically disabled when output is piped or redirected

### Configuration
- **Config file** (`queueprobe.conf`) — store all settings in a `key = value` file; auto-loaded from the exe directory with no extra flags
- **Flexible config loading** — pass `--config <path>` for an explicit path, or use a positional `.conf` argument (`QueueProbe.exe myenv.conf`)
- **CLI always wins** — any command-line flag overrides the config file value
- **Inline comments** — config values support `# comment` suffix (e.g. `logfile = out.log  # daily log`)

---

## Quick Start (Windows)

All required DLLs are bundled in the `windows/` folder. No installation needed.

```cmd
cd windows
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

> **TLS note:** For `tcps://` connections, `libcrypto-3.dll` and `libssl-3.dll` must be present in the `windows/` folder alongside the exe. They are already included in this repo.

---

## Modes

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

**Auto-load** — place a file named `queueprobe.conf` in the same directory as the exe. It is loaded automatically with no extra flags needed:

```cmd
cd windows
QueueProbe.exe          ← reads queueprobe.conf automatically
```

**Explicit path:**
```cmd
QueueProbe.exe --config C:\configs\prod.conf
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
logfile  = trade.log
```

For a fully commented template see [`queueprobe.conf.example`](queueprobe.conf.example) in this repo.

> **Security:** `queueprobe.conf` is listed in `.gitignore` — it will never be accidentally committed since it typically contains credentials.

---

## TLS / SSL

For `tcps://` connections, the Solace SDK loads OpenSSL at runtime from the exe's directory.

**With certificate validation (recommended for production):**
```cmd
QueueProbe.exe q.test --host tcps://broker:55443 --vpn vpn1 --username user --password pass --certdir C:\certs\ca
```

**Without certificate validation (useful for self-signed certs / dev):**
```cmd
QueueProbe.exe q.test --host tcps://broker:55443 --vpn vpn1 --username user --password pass --no-verify
```

> By default, SSL certificate validation is **disabled**. Use `--certdir` to enable it for production.

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

### Prerequisites

Download the [Solace C API](https://solace.com/downloads/) for your platform and extract it.

### Linux

```bash
export SOLACE_HOME=/path/to/solclient-7.33.1.1
export LD_LIBRARY_PATH=$SOLACE_HOME/lib:$LD_LIBRARY_PATH

# Production binary
gcc src/solace_queue_consumer_proxy.c \
  -I$SOLACE_HOME/include \
  -L$SOLACE_HOME/lib \
  -lsolclient -o solace_consumer

# Debug binary
gcc src/solace_consumer_debug.c \
  -I$SOLACE_HOME/include \
  -L$SOLACE_HOME/lib \
  -lsolclient -o solace_consumer_debug
```

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

powershell:
```bash
gcc src/solace_consumer_debug.c -I solclient-7.33.1.1-win/include -L solclient-7.33.1.1-win/lib/Win64 -DSOLCLIENT_CONST_PROPERTIES -lsolclient -lws2_32 -static-libgcc -o windows/QueueProbe.exe
```

> **`-DSOLCLIENT_CONST_PROPERTIES`** is required — without it the SDK typedef resolves to `char**` instead of `const char**`, causing compile errors on property arrays.
>
> **`-static-libgcc`** embeds the GCC runtime so the exe only needs `libsolclient.dll` plus the Windows UCRT (built into Windows 10+).

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
│   ├── solace_queue_consumer_proxy.c   # Production consumer (minimal output)
│   └── solace_consumer_debug.c         # Debug consumer (colour logs + JSON pretty-print)
├── windows/
│   ├── QueueProbe.exe                  # Pre-compiled Windows 64-bit binary
│   ├── libsolclient.dll                # Solace client library
│   ├── libsolclient_d.dll              # Solace client library (debug build)
│   ├── libcrypto-3.dll                 # OpenSSL — required for tcps://
│   └── libssl-3.dll                    # OpenSSL — required for tcps://
├── queueprobe.conf.example     # Config file template
├── CHANGELOG.md
├── LICENSE
└── README.md
```

The debug binary adds:
- Millisecond-precision timestamped logging
- ANSI colour-coded log levels
- JSON payload auto-detection and syntax-highlighted pretty-printing
- Full session/flow event diagnostics
- Log file output (`--logfile`)

---

## Dependencies

| Library | Version | Purpose |
|---------|---------|---------|
| libsolclient | 7.33.1.1 | Solace PubSub+ messaging |
| OpenSSL | 3.x | TLS transport (required for `tcps://`) |
| Windows UCRT | Windows 10+ built-in | C runtime |

---

## License

Copyright 2025 Tanendra77 (github.com/Tanendra77)

Licensed under the [Apache License, Version 2.0](LICENSE).