# 快捷控制台

快捷控制台是一个 C++20、原生 Win32、Windows x64 命令面板。程序默认以普通权限启动，通过 ConPTY 分别托管长期存在的 Windows PowerShell 和 WSL Bash 会话，并把本地 JSON 中的命令显示为可编辑按钮。

## 当前功能

- 原生 Win32 无框窗口、八方向鼠标缩放、Unicode、Per-Monitor V2 DPI 和按需管理员提权。
- 紧凑浅色卡片式界面：无系统顶边的自绘标题栏、彩色状态点、标签页、圆角命令按钮和虚线“添加按钮”卡片。
- 宽屏按钮区最多使用四列布局；按钮区、终端区和输入区由两条分隔线调整高度，按钮区至少保留一行，终端至少保留四行。
- PowerShell 与 WSL 使用两个独立的长期 ConPTY 会话，切换时保留目录、进程、屏幕和命令历史。
- 暗色自绘终端支持 VT 光标、颜色、滚动区域、备用屏幕、应用光标模式、括号粘贴和常见键盘驱动 TUI。
- 点击终端后可直接输入；拖选后可用 `Ctrl+C`、`Ctrl+Shift+C` 或右键复制，`Ctrl+Shift+V` 粘贴；无选区时 `Ctrl+C` 仍发送中断。底部输入框支持多行编辑，`Enter` 换行，`Ctrl+Enter` 整体执行，`Ctrl+Up/Ctrl+Down` 浏览当前终端历史。
- 托管命令使用 UTF-8 Base64 包装和完成标记，支持 busy 状态、确认执行和退出码显示。
- 两套终端分别自动恢复；其中一套不可用不会阻塞另一套。
- WSL 后台启动遇到 Windows 提升要求时只记录状态，不在应用启动时打断用户；用户点击 WSL 后才沿用一次性 UAC 重开并保持 WSL 目标。Linux 命令出现 `permission denied` 时通过同一会话的 `sudo` 重试，密码可直接在终端输入。
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
build\Release\快捷控制台.exe
```

运行时配置位于 `%LOCALAPPDATA%\快捷控制台\config.json`。首次启动会创建目录并写入三个默认标签页；若新版配置尚不存在而 EXE 旁有旧 `config.json`，程序会自动迁移旧配置。默认标签仅在首次创建时写入，后续删除不会在重启时恢复。配置同时记录窗口尺寸及按钮、终端、输入三个区域的分隔位置。

## 配置

当前配置格式为 `version: 3`，核心结构如下：

```json
{
  "version": 3,
  "ui": {
    "active_terminal": "powershell"
  },
  "tabs": [
    {
      "id": "common",
      "name": "常用",
      "buttons": [
        {
          "id": "wsl-status",
          "name": "WSL 状态",
          "command": "wsl.exe --status",
          "terminal": "powershell",
          "confirm": false,
          "enabled": true
        }
      ]
    }
  ]
}
```

`terminal` 可为 `powershell` 或 `wsl`。旧版 v1/v2 按钮缺少该字段时仍按 PowerShell 执行，不根据命令文本自动改写。程序启动不会自动执行配置中的普通按钮命令；保存时先写临时文件，再替换目标文件。

默认标签页包含常用、OpenClaw 和系统分类，业务命令仅作为示例配置，不会写死到执行器中。

## 源码结构

| 模块 | 责任 |
| --- | --- |
| `MainWindow` | 主窗口、双分隔布局、多行输入、DPI、后台消息和生命周期 |
| `ButtonPanel` / `TabBar` | 圆角按钮卡片、四列布局、标签页绘制和交互 |
| `TerminalSession` | 通用 ConPTY 进程启动、读写线程、队列、Resize 和句柄释放 |
| `TerminalModel` / `TerminalView` | VT 屏幕、回滚、TUI 键盘编码和双缓冲自绘 |
| `TerminalParser` | 保留 VT 序列的 UTF-8 流式解码 |
| `CommandExecutor` | PowerShell/Bash 命令包装、完成标记、busy 和退出码 |
| `ConfigManager` | 用户 AppData 配置、UTF-8 JSON、旧路径迁移、UI 状态和原子保存 |
| `CommandDialog` | 命令名称、内容、目标终端和确认选项编辑 |

UI 线程不直接读取 ConPTY 管道。终端后台线程通过 `PostMessageW` 把输出和退出事件交给主窗口，窗口关闭时先停止会话，再释放线程和句柄。

## 已验证项目

- CMake Release x64 编译与链接通过。
- VT 屏幕、UTF-8 分片、备用屏幕、宽字符和键盘编码有自动测试。
- 真实 PowerShell/WSL/TUI 与多 DPI 交互仍需按项目验收清单人工确认。

## 当前边界

当前终端面向常见键盘驱动的 VT/xterm TUI；不实现终端鼠标协议、超链接、图片或 Sixel。WSL 页固定连接系统默认发行版并启动 Bash 登录 Shell，不提供多发行版或自定义 shell 管理。
