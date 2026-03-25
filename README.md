# UECLI - Unreal Engine Command Line Interface

> **MCP 已死，CLI 当立。**
>
> MCP（Model Context Protocol）方案依赖中间代理层，链路长、调试难、协议脆弱。UECLI 反其道而行 —— 一个 TCP Server 嵌入 Editor，任何能发 JSON 的工具直连引擎，零中间层、零依赖、毫秒级响应。AI Agent、脚本、CI 管线，谁都能用。

## What is UECLI

UECLI 是一个 **独立的 UE5 Editor 插件**，通过内置 TCP Server（默认端口 `31111`）暴露 **90+ 条命令**，覆盖材质、编辑器、资产、项目、TextureGraph 五大领域。

- **零依赖**：不依赖 Python / Node / MCP SDK，任何语言 `socket.connect()` 即用
- **嵌入式**：Server 跑在 Editor 进程内，命令在 GameThread 执行，完整访问引擎 API
- **AI-Native**：JSON-in / JSON-out 协议天然适配 LLM Function Calling
- **异步支持**：长耗时操作通过 `async_execute` + `get_task_result` 轮询，不阻塞

## Command Coverage

| Module | Commands | Capabilities |
|--------|----------|-------------|
| **Editor** | 23 | Actor CRUD, viewport control, level management, PIE, transform, property reflection |
| **Material** | 43 | Material graph creation/editing, node connection, parameter setting, compile, material functions |
| **Asset** | 11 | List, find, rename, duplicate, import, export assets |
| **Project** | 9 | Input mapping, project settings, engine configuration |
| **TextureGraph** | 10 | Create/edit TextureGraph, add/connect nodes, set properties, export, batch patch |

## Quick Start

### 1. Install

Copy `UECLI/` into your project's `Plugins/` directory, then rebuild.

### 2. Verify

Launch the Editor, then ping the server:

```powershell
# PowerShell
powershell -ExecutionPolicy Bypass -File Plugins/UECLI/Scripts/Send-UECLI.ps1 ping
```

```bash
# Or with any TCP client
echo '{"command":"ping","params":{}}' | nc 127.0.0.1 31111
```

### 3. Explore

```powershell
# List all available commands
powershell -ExecutionPolicy Bypass -File Plugins/UECLI/Scripts/Send-UECLI.ps1 list_tools

# Create a material
powershell -ExecutionPolicy Bypass -File Plugins/UECLI/Scripts/Send-UECLI.ps1 create_material '{"name":"M_Test"}'

# Spawn an actor
powershell -ExecutionPolicy Bypass -File Plugins/UECLI/Scripts/Send-UECLI.ps1 spawn_actor '{"class":"StaticMeshActor","name":"MyActor"}'
```

## TCP Protocol

Connect to `127.0.0.1:31111` (override with `-uecliport=XXXXX`).

```jsonc
// Request
{"command": "command_name", "params": {"key": "value"}}

// Success
{"status": "success", "data": {...}}

// Error
{"status": "error", "error": "description"}
```

### Async Commands

```jsonc
// Submit
{"command": "async_execute", "params": {"command": "heavy_operation", "params": {...}}}
// → {"status": "success", "data": {"task_id": "xxx"}}

// Poll
{"command": "get_task_result", "params": {"task_id": "xxx"}}
```

## Why Not MCP?

| | MCP | UECLI |
|---|---|---|
| Architecture | App ↔ MCP Server ↔ Proxy ↔ UE | App ↔ UE (direct TCP) |
| Dependencies | Python/Node runtime, MCP SDK | None |
| Latency | ~100ms+ (IPC + protocol overhead) | <10ms (localhost TCP) |
| Debugging | Multi-process, hard to trace | Single process, `UE_LOG` |
| Stability | Protocol version mismatch, process crash | In-process, lifecycle tied to Editor |
| Integration | Only MCP-compatible clients | Any language, any tool |

## Scripts

| Script | Purpose |
|--------|---------|
| `Build-UECLI.ps1` | Build the plugin via UBT |
| `Send-UECLI.ps1` | Send a single command to the server |
| `Test-UECLI.ps1` | Run full automation test suite |
| `Smoke-UECLI.ps1` | Quick smoke test |

## Source Structure

```
Source/UECLI/
├── Public/
│   ├── UECLIModule.h                     # Module entry point
│   ├── UECLICommandlet.h                 # -run=UECLI commandlet
│   ├── ToolRegistry/
│   │   ├── UECLIToolRegistry.h           # Command registry singleton
│   │   └── UECLIToolSchema.h             # Schema & parameter definitions
│   ├── Server/
│   │   ├── UECLIServer.h                 # TCP server (UEditorSubsystem)
│   │   └── UECLIServerRunnable.h         # Listener thread
│   └── Commands/
│       ├── UECLICommonUtils.h            # JSON utils, serialization, reflection
│       ├── UECLIEditorCommands.h         # Editor commands
│       ├── UECLIMaterialCommands.h       # Material commands
│       ├── UECLIAssetCommands.h          # Asset commands
│       ├── UECLIProjectCommands.h        # Project commands
│       └── UECLITextureGraphCommands.h   # TextureGraph commands
└── Private/ (mirrors Public)
```

## Requirements

- Unreal Engine 5.6+
- Windows / Mac / Linux (Editor only)

## License

MIT
