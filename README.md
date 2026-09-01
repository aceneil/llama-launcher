# 🦙 Llama Launcher

> 📖 **Language / 语言**: English| [简体中文](docs/README_CN.md)

**Llama Launcher** is a lightweight, intuitive GUI/TUI management tool designed to streamline running local Large Language Models (LLMs) via [`llama.cpp`](https://github.com/ggerganov/llama.cpp).

Instead of memorizing and manually typing lengthy `llama-server` command-line flags, Llama Launcher provides an effortless interface to select models, configure hardware/runtime settings, save custom presets, and orchestrate server instances with a single click.

---

## ✨ Key Features

* **🎛️ Visual Command Builder**: Easily set essential parameters such as context size (`-c`), GPU layers (`-ngl`), threads (`-t`), host/port, and sampling parameters without touching raw CLI flags.
* **📂 Model & Preset Management**: 
  * Auto-detect and scan local GGUF models.
  * Save and load customized configurations per model (e.g., dedicated settings for 7B vs. 70B models).
* **⚡ Process Lifecycle Control**:
  * One-click start, pause, and stop for local `llama-server` instances.
  * Real-time streaming log viewer (stdout/stderr monitoring).
* **🔌 Open-API Compatible**: Exposes standard OpenAI-compatible API endpoints (e.g., `http://localhost:8080/v1`) once launched, ready to connect to frontends like Open WebUI, TypingMind, or local plugins.

---

## 🚀 Quick Start

### Prerequisites

* **llama.cpp**: Ensure `llama-server` (or `server` executable) is compiled or downloaded on your machine.
* **Models**: Download GGUF format model files from Hugging Face or other sources and store them in your local directory.

### Installation

1. **Clone the repository**:
   ```bash
   git clone [https://github.com/aceneil/llama-launcher.git](https://github.com/aceneil/llama-launcher.git)
   cd llama-launcher
   ```

2. **Install dependencies & build**:

   ```bash
   # If using Python:
   pip install -r requirements.txt
   python main.py
   
   # If using Node.js / Web stack:
   npm install
   npm run dev
   ```

## ⚙️ Usage Workflow

1. **Configure Path**: Set the binary path pointing to your `llama-server` executable and specify your GGUF models folder.
2. **Select Model**: Pick the model you want to load from the auto-scanned list.
3. **Adjust Flags**:
   - **Context Length (`-c`)**: Adjust based on your available RAM/VRAM.
   - **GPU Offload (`-ngl`)**: Maximize offloaded layers to accelerate inference if using CUDA/Metal/ROCm.
   - **Threads (`-t`)**: Set optimal CPU threads for processing.
4. **Launch**: Click **Start Server**. Monitor startup logs in real-time until the server status changes to `Ready`.
5. **Connect**: Open your favorite LLM client or WebUI and point the base URL to `http://127.0.0.1:8080`.

## 🛠️ Configuration & Presets

Configurations are managed dynamically or via a local configuration file (e.g., `config.json` / `config.toml`). You can save distinct configurations for different tasks:

- **Coding Preset**: High context window (`-c 16384`), low temperature (`0.2`).
- **Creative Writing Preset**: Moderate context window (`-c 4096`), higher temperature (`0.7`).

## 🪙 Sponsorship & Support

If this project has been helpful to you, please consider donating. The author currently only has a 5060 Ti 16G and hasn't been able to fully test the app's reliability — apologies for any inconvenience.

<div align="center">
  <img src="assets/wechat-reward.png" width="255" alt="WeChat reward QR code">
  <p>WeChat reward QR code</p>
</div>

---

## 📄 License

This project is licensed under the [MIT License](https://www.google.com/search?q=LICENSE).
