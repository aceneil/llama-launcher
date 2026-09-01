# 🦙 Llama Launcher

📖 **Language / 语言**: [English](../README.md)| 简体中文

**Llama Launcher** 是一款轻量、直观的 GUI/TUI 管理工具，旨在简化通过 [`llama.cpp`](https://github.com/ggerganov/llama.cpp) 运行本地大语言模型（LLM）的操作流程。

无需记忆和手动输入繁琐的 `llama-server` 命令行参数，Llama Launcher 提供了一个轻松便捷的界面，让你只需一次点击，即可选择模型、配置硬件与运行时参数、保存自定义预设并调度服务器实例。

---

## ✨ 核心特性

* **🎛️ 可视化命令生成器**：无需手动输入原始命令行参数，即可轻松配置上下文大小 (`-c`)、GPU 加速层数 (`-ngl`)、线程数 (`-t`)、主机/端口绑定以及采样参数等关键设置。
* **📂 模型与预设管理**：
  * 自动检测并扫描本地 GGUF 模型文件。
  * 为不同模型保存并加载自定义配置（例如为 7B 和 70B 模型分别设置独立的运行参数）。
* **⚡ 进程生命周期控制**：
  * 一键启动、暂停和停止本地 `llama-server` 实例。
  * 实时日志流查看器（支持 stdout/stderr 监控）。
* **🔌 OpenAI API 兼容**：启动后提供标准的 OpenAI 兼容 API 端点（如 `http://localhost:8080/v1`），可直接无缝连接 Open WebUI、TypingMind 等前端工具或本地插件。

---

## 🚀 快速开始

### 环境准备

* **llama.cpp**：确保本地已编译或下载 `llama-server`（或 `server` 可执行文件）。
* **模型文件**：从 Hugging Face 或其他来源下载 GGUF 格式的模型文件，并存放在本地目录中。

### 安装步骤

1. **克隆项目仓库**：
   ```bash
   git clone [https://github.com/aceneil/llama-launcher.git](https://github.com/aceneil/llama-launcher.git)
   cd llama-launcher
   ```

2. **安装依赖并运行**：

   ```bash
   # 如果使用 Python 环境：
   pip install -r requirements.txt
   python main.py
   
   # 如果使用 Node.js / Web 技术栈：
   npm install
   npm run dev
   ```

## ⚙️ 使用流程

1. **配置路径**：设置指向 `llama-server` 可执行文件的路径，并指定 GGUF 模型所在的文件夹。
2. **选择模型**：从自动扫描出的模型列表中选择需要加载的模型。
3. **调整参数**：
   - **上下文长度 (`-c`)**：根据可用的 RAM/VRAM 内存大小进行调整。
   - **GPU 卸载层数 (`-ngl`)**：如果使用 CUDA/Metal/ROCm 加速，尽量调高卸载层数以提升推理速度。
   - **线程数 (`-t`)**：根据 CPU 核心情况设置最佳线程数。
4. **启动服务**：点击 **Start Server**。实时监控启动日志，直至服务器状态切换为 `Ready`。
5. **连接客户端**：打开你偏好的 LLM 客户端或 WebUI，将 API 基础路径指向 `http://127.0.0.1:8080`。

## 🛠️ 配置与预设

系统支持动态管理或通过本地配置文件（如 `config.json` / `config.toml`）管理参数。你可以针对不同的应用场景保存独立的预设配置：

- **代码编写预设**：高上下文窗口 (`-c 16384`)，低随机性温度 (`0.2`)。
- **创意写作预设**：中等上下文窗口 (`-c 4096`)，较高随机性温度 (`0.7`)。

## ₿ 赞助与支持

如果项目对您有帮助请打赏我,作者目前只有5060ti-16g未能完整测试app可靠性,如给您带来不便请见谅。

<div align="center">
  <img src="../assets/wechat-reward.png" width="255" alt="微信打赏码">
  <p>微信打赏码</p>
</div>

---

## 📄 开源协议

本项目基于 [MIT 许可证](https://www.google.com/search?q=LICENSE) 开源。