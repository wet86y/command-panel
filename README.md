# 快捷控制台

快捷控制台（CommandPanel）是一个 C++20、原生 Win32、Windows x64 命令面板。程序启动时请求管理员权限，通过 ConPTY 托管一个长期存在的 Windows PowerShell 会话，并把本地 JSON 中的命令显示为可编辑按钮。

## 当前功能

- 原生 Win32 无框窗口、八方向鼠标缩放、Unicode、Per-Monitor V2 DPI 和管理员模式启动。
- 紧凑浅色卡片式界面：无系统顶边的自绘标题栏、彩色状态点、标签页、圆角命令按钮和虚线“添加按钮”卡片。
- 宽屏按钮区最多使用四列布局；按钮区、终端区和输入区由两条分隔线调整高度，按钮区至少保留一行，终端至少保留四行。
- 暗色 RichEdit 终端输出区，支持清空、中止和重置终端。
- PowerShell 输出按到达顺序追加，从上到下阅读；基础 ANSI/VT 序列、UTF-8、回车换行和退格会经过解析和规范化。
- 底部输入框始终支持多行粘贴和滚动；Enter 整体执行，Shift+Enter 插入换行，Ctrl+Up/Ctrl+Down 浏览历史。
- 托管命令使用 UTF-8 Base64 包装和完成标记，支持 busy 状态、确认执行和退出码显示。
- PowerShell 异常退出后自动尝试恢复；ConPTY、PowerShell 或 WSL 不可用时在终端区域显示诊断信息。
- 标签页支持单击切换、原位双击改名和右键删除；右侧 `+` 会立即创建并选中唯一默认名称。按钮右键菜单仅保留编辑和删除，并与编辑、确认弹窗使用统一的自绘浅色风格。

## 构建

要求：Visual Studio 2026 Community / MSVC 14.51、CMake 3.24 或更高版本、Windows x64。项目使用静态 CRT `/MT`，不依赖 Qt、.NET、Node、联网服务或构建时下载。

在 Visual Studio Developer PowerShell 中执行：

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release
```

最终产物位于：

```text
build\Release\CommandPanel.exe
build\Release\config.json
```

`config.json` 是运行时用户配置，不应提交到 Git；首次运行时可以从 `config.example.json` 复制并按需修改。配置只从 EXE 所在目录读取。

## 配置

当前配置格式为 `version: 2`，核心结构如下：

```json
{
  "version": 2,
  "tabs": [
    {
      "id": "common",
      "name": "常用",
      "buttons": [
        {
          "id": "wsl-status",
          "name": "WSL 状态",
          "command": "wsl.exe --status",
          "confirm": false,
          "enabled": true
        }
      ]
    }
  ]
}
```

程序启动不会自动执行配置中的命令；命令只有在用户点击按钮或手工输入后才会执行。旧版 `version: 1` 配置会自动按默认分类迁移。保存时先写临时文件，再替换目标文件，避免直接覆盖有效配置。

默认标签页包含常用、OpenClaw 和系统分类，业务命令仅作为示例配置，不会写死到执行器中。

## 源码结构

| 模块 | 责任 |
| --- | --- |
| `MainWindow` | 主窗口、双分隔布局、多行输入、DPI、后台消息和生命周期 |
| `ButtonPanel` / `TabBar` | 圆角按钮卡片、四列布局、标签页绘制和交互 |
| `TerminalSession` | ConPTY、PowerShell、读写线程、队列、Resize 和句柄释放 |
| `TerminalParser` | UTF-8 流解码、基础 ANSI/VT 过滤、回车换行和退格处理 |
| `CommandExecutor` | Base64 命令包装、执行 ID、完成标记、busy 和退出码 |
| `ConfigManager` | UTF-8 JSON、UTF-16 UI 转换、迁移和原子保存 |
| `CommandDialog` | 命令名称、命令内容和确认选项编辑 |

UI 线程不直接读取 ConPTY 管道。终端后台线程通过 `PostMessageW` 把输出和退出事件交给主窗口，窗口关闭时先停止会话，再释放线程和句柄。

## 已验证项目

- CMake Release x64 编译与链接通过。
- 最终 EXE 已启动检查，管理员模式、PowerShell 会话和 WSL 诊断路径可进入。
- 概念图对应的浅色圆角界面、紧凑按钮区和更高终端区已完成视觉回归。
- 输出追加顺序、清空、中止、重置按钮和窗口关闭生命周期已完成代码级回归。

## 第一版边界

第一版有意不实现完整 VT100 光标定位、alternate screen、TUI、多终端、远程控制、服务、联网、自动更新、托盘、暗色模式、拖拽排序和开机自启。
