# Llama Launcher

本地 llama.cpp 配置式启动器(Win32 单文件 GUI,无依赖)。

启动后**不直接运行**,先自动识别硬件并填入最佳配置,确认后再点「启动」——适合不想记一堆命令行参数的用户。

## 功能

- 🖥️ **硬件自动识别**:启动时检测 GPU(NVIDIA/AMD/Intel)、显存、内存、CPU 核心数,自动匹配最佳档位(32G+/24G/16G/12G/8G/6G/CPU),两行显示(CPU+内存 / 显卡)
- 🎛️ **配置可选**:模型(递归扫描 `models` 目录,下拉展开显示全名,自动过滤 mmproj)、监听 IP、端口、后端(CUDA/Vulkan/CPU)、上下文、思考模式、Flash Attention、KV 量化、温度、生成上限
- 📡 **局域网访问**:IP 默认 `0.0.0.0`,局域网内设备可直接访问
- 📜 **cmd 风格日志**:右侧黑底实时输出 llama-server 日志
- 💾 **配置记忆**:上次选择自动保存(`launcher.ini`)
- 🔄 **router 模式**:多模型热切换(基于 llama.cpp `--models-dir`)
- 🎨 **自定义图标**:内置 8-bit 像素风图标(奔跑的马)

## 使用

1. 下载 Release 里的 `Llama Launcher.exe`
2. 放到 **llama.cpp 解压目录**(与 `models` 文件夹同级,或任何含 `llama-server.exe` 的子目录)
3. 双击运行 → 自动检测硬件并填好配置 → 点「启动」
4. 浏览器(或局域网设备)访问 `http://<IP>:<端口>` 使用 WebUI

## 目录结构

```
llama-launcher/
├── src/main.cpp      # 全部源码(单文件)
├── build.sh          # Linux 交叉编译脚本
├── README.md
└── LICENSE           # MIT
```

## 编译

```bash
# Linux 交叉编译 → Windows exe
sudo apt install g++-mingw-w64-x86-64
./build.sh            # 产出 dist/Llama Launcher.exe

# Windows 本地(需 MinGW 或 VS)
x86_64-w64-mingw32-g++ src/main.cpp -o "Llama Launcher.exe" -mwindows -municode -static -O2
```

## 依赖

- llama.cpp(带 `llama-server.exe` 和 `--models-dir` router 模式支持)
- 模型:GGUF 格式,放入 `models` 目录(可带子目录;`mmproj-*.gguf` 多模态投影文件自动过滤,由主模型自动配对)

## 许可证

[MIT](LICENSE) © 2026 aceneil
