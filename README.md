# TeaFamily - 分布式进程管理系统

## 概述

TeaFamily 是一套分布式进程管理系统，包含以下组件：

| 组件 | 语言 | 角色 |
|------|------|------|
| **GreenTea** | C++ | 守护进程 - 监控并自动重启其他组件 |
| **HoneyTea** | C++ | 客户端 - 管理本地子进程（插件） |
| **LemonTea** | C++ | 服务端 - 集中管理、HTTP API |
| **OrangeTea** | Vue3 | 前端界面 - 图形化管理所有组件 |

## 架构

```
┌─────────────┐     HTTP      ┌─────────────┐     TCP      ┌─────────────┐
│  OrangeTea  │◄────────────►│  LemonTea   │◄───────────►│  HoneyTea   │
│  (Vue3 UI)  │              │  (Server)   │              │  (Client)   │
└─────────────┘              └──────┬──────┘              └──────┬──────┘
                                    │                            │
                              ┌─────┴─────┐               ┌─────┴─────┐
                              │ GreenTea  │               │ GreenTea  │
                              │ (Daemon)  │               │ (Daemon)  │
                              └───────────┘               └───────────┘
                                    │                            │
                              ┌─────┴─────┐               ┌─────┴─────┐
                              │  Plugins  │               │  Plugins  │
                              └───────────┘               └───────────┘
```

## 构建

### macOS
```bash
./scripts/macOS_build_release.sh
```

### 手动构建
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

## 配置

每个组件都有独立的 `config.json` 配置文件，详见各组件目录。

## 插件

内置插件：
- **SSH插件** - 远程终端访问
- **文件管理插件** - 远程文件浏览与管理
- **监控插件** - 系统资源监控与图表展示

详见 [插件开发文档](docs/plugin-development.md)

## OrangeTea 前端

```bash
cd OrangeTea
npm install
npm run dev      # 开发模式
npm run build    # 生产构建
```
