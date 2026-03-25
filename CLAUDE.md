# UECLI Plugin Development Guide

## Overview

UECLI 是一个完全独立的 UE Editor 插件，通过 TCP Server（端口 31111）暴露材质、编辑器、资产、项目相关命令。与 UnrealCLI 互不依赖，可同时启用。

## Project Paths

- Engine: `D:\LYH\UE\Engine` (UE 5.6.1)
- Project: `D:\LYH\UE\W0\W0.uproject`
- Plugin: `D:\LYH\UE\W0\Plugins\UECLI\`
- Source: `D:\LYH\UE\W0\Plugins\UECLI\Source\UECLI\`
- Scripts: `D:\LYH\UE\W0\Plugins\UECLI\Scripts\`

## Build & Test Commands

```powershell
# Build
powershell -ExecutionPolicy Bypass -File D:\LYH\UE\W0\Plugins\UECLI\Scripts\Build-UECLI.ps1

# Smoke test (editor must be running)
powershell -ExecutionPolicy Bypass -File D:\LYH\UE\W0\Plugins\UECLI\Scripts\Smoke-UECLI.ps1

# Ping only
powershell -ExecutionPolicy Bypass -File D:\LYH\UE\W0\Plugins\UECLI\Scripts\Test-UECLI.ps1 -PingOnly

# Full automation tests
powershell -ExecutionPolicy Bypass -File D:\LYH\UE\W0\Plugins\UECLI\Scripts\Test-UECLI.ps1

# Send command
powershell -ExecutionPolicy Bypass -File D:\LYH\UE\W0\Plugins\UECLI\Scripts\Send-UECLI.ps1 ping
powershell -ExecutionPolicy Bypass -File D:\LYH\UE\W0\Plugins\UECLI\Scripts\Send-UECLI.ps1 list_tools
powershell -ExecutionPolicy Bypass -File D:\LYH\UE\W0\Plugins\UECLI\Scripts\Send-UECLI.ps1 create_material '{"name":"M_Test"}'
```

## TCP Protocol

Server: `127.0.0.1:31111` (override: `-uecliport=XXXXX`)

```json
// Request
{"command": "command_name", "params": {}}

// Success response
{"status": "success", "data": {...}}

// Error response
{"status": "error", "error": "error message"}
```

Async commands: send `{"command":"async_execute","params":{"command":"xxx","params":{...}}}`, get `task_id`, poll with `get_task_result`.

## Source Structure

```
Source/UECLI/
├── Public/
│   ├── UECLIModule.h                    # Module entry
│   ├── UECLICommandlet.h                # -run=UECLI commandlet
│   ├── ToolRegistry/
│   │   ├── UECLIToolRegistry.h          # FUECLIToolRegistry singleton
│   │   └── UECLIToolSchema.h            # FUECLIToolSchema, FUECLIToolParam
│   ├── Server/
│   │   ├── UECLIServer.h                # UUECLIServer (UEditorSubsystem, TCP 31111)
│   │   └── UECLIServerRunnable.h        # FUECLIServerRunnable (listener thread)
│   └── Commands/
│       ├── UECLICommonUtils.h           # JSON utils, Actor serialization, property reflection
│       ├── UECLIEditorCommands.h        # 22 commands: actor, viewport, level, PIE
│       ├── UECLIMaterialCommands.h      # 42 commands: material graph, functions, compile
│       ├── UECLIAssetCommands.h         # 10 commands: list, find, rename, import
│       └── UECLIProjectCommands.h       # 8 commands: input mapping, settings
└── Private/ (mirrors Public structure)
```

## CommonUtils API

`FUECLICommonUtils` provides shared utilities. **Note: no Blueprint/K2Node utilities** (those stay in UnrealCLI).

Available:
- `CreateErrorResponse(Message)` / `CreateSuccessResponse(Data)` — standard JSON responses
- `GetVectorFromJson()`, `GetRotatorFromJson()`, `GetVector2DFromJson()` — JSON parsing
- `GetIntArrayFromJson()`, `GetFloatArrayFromJson()` — array parsing
- `ActorToJson()`, `ActorToJsonObject()` — actor serialization
- `SetObjectProperty()` — reflection-based property setter
- `SerializeObjectProperties()`, `SerializePropertyValue()`, `SerializeStructProperties()` — reflection serialization

## Naming Conventions

- API macro: `UECLI_API`
- All classes: `FUECLI*` prefix (e.g. `FUECLIEditorCommands`, `FUECLIToolRegistry`)
- UObject classes: `UUECLI*` prefix (e.g. `UUECLIServer`, `UUECLICommandlet`)
- ToolSchema types: `FUECLIToolSchema`, `FUECLIToolParam`, `FUECLICommandHandler`
- Namespace: `UECLI::Private`
- Module name: `UECLI`
- Commandlet: `-run=UECLI`

## Adding a New Command

1. Declare handler method in `Commands/UECLI*Commands.h`
2. Implement in corresponding `.cpp`
3. Register in `RegisterTools(FUECLIToolRegistry& Registry)` static method
4. Use `FUECLICommonUtils::CreateSuccessResponse()` / `CreateErrorResponse()`
5. Run `/build-uecli` to compile and test

### Handler Pattern

```cpp
// In anonymous namespace of .cpp:
TSharedRef<FUECLIEditorCommands> GetEditorCommands()
{
    static TSharedRef<FUECLIEditorCommands> Instance = MakeShared<FUECLIEditorCommands>();
    return Instance;
}

void RegisterEditorCommand(FUECLIToolRegistry& Registry, const TCHAR* PublicName, const TCHAR* InternalName = nullptr)
{
    const auto Handler = GetEditorCommands();
    const FString RoutedName = InternalName ? InternalName : PublicName;
    Registry.Register(
        FUECLIToolSchema(PublicName, TEXT("Editor"), FString()),
        [Handler, RoutedName](const TSharedPtr<FJsonObject>& Params) {
            return Handler->HandleCommand(RoutedName, Params);
        });
}
```

## Build-Fix Workflow

1. Run `Build-UECLI.ps1` — check output
2. Errors format: `file(line): error CXXXX: message` or `error LNK2019: unresolved external`
3. **Compiler error (CXXXX)**: Read the source file at error line, fix the code
4. **Linker error (LNK2019)**: Missing module dependency — add to `UECLI.Build.cs`
5. Rebuild — repeat until clean

## Dependencies (UECLI.Build.cs)

Public: Core, CoreUObject, Engine, InputCore, Json, JsonUtilities, DeveloperSettings, EngineSettings
Private: UnrealEd, EditorScriptingUtilities, EditorSubsystem, Slate, SlateCore, Projects, AssetRegistry, LevelEditor, Sockets, Networking
Editor: PropertyEditor, ToolMenus, MaterialEditor

## Known Limitations

- `delete_asset` — disabled (causes deadlock with ForceDeleteObjects)
- `save_all_assets` — disabled (triggers FlushRenderingCommands deadlock)
- No Blueprint/K2Node utilities in CommonUtils — use UnrealCLI for Blueprint operations
