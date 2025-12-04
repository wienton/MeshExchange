
![BackgroundRepository](images/anime.jpg)

# MeshExchange

A lightweight, secure file exchange service written in pure C, designed for reliability and minimal dependencies. Includes a custom build system powered by the `.wien` domain-specific language for expressive and maintainable compilation workflows.

## Features

- **Secure file transfers** with optional end-to-end encryption  
- **Minimalist C implementation** — no external runtime dependencies  
- **Cross-platform compatibility** (Linux, macOS, BSD)  
- **Integrated build system** using the `.wien` configuration language  
- **Modular architecture** — daemon, client, and server components built independently  
- **Support for cleanup, variables, conditionals, and compilation blocks** in `.wien` scripts  

## Components

- **`daemon`** — background service for managing file exchange sessions  
- **`client`** — command-line interface for uploading and downloading files  
- **`server`** — handles incoming connections and file routing  

## Build System: `.wien`

MeshExchange uses a custom, human-readable build definition language (`.wien`) that supports:
- Variable substitution  
- Conditional compilation (`if`/`endif`)  
- Target-specific compilation blocks (`.comp`)  
- Text output and clean rules  
- Dependency-aware builds without Makefiles  

## Quick Start

1. Clone the repository  
2. Run the build system:  
   ```bash
   ./wien

   ```
Select a target (`daemon`, `client`, `server`, or `all`) from the interactive menu.  
Executables are placed in the project root.

## Documentation & Resources

- **Source code**: [GitHub Repository](https://github.com/wienton/meshexchange)  
- **`.wien` language guide**: [docs/wien-syntax.md](docs/wien-syntax.md)  
- **License**: MeshExchange LICENSE 2025
- **Report issues**: [GitHub Issues](https://github.com/wienton/meshexchange/issues)  

> Note: This project is built with clarity, performance, and auditability in mind — no frameworks, no bloat.

## Contact for me

- **Telegram:** [Telegram](https://t.me/wienton)
- **Web:** [WebSite](http://wienton.ru/)