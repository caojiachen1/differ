# Differ - 相似图片查找器 (C++/Qt)

一个基于 Qt6 + C++ 的桌面应用，提供：
- 文件夹递归索引图片（PNG/JPG/JPEG/BMP/GIF/WEBP/TIFF）
- 计算 64-bit pHash 感知哈希并持久化到 SQLite
- 生成缩略图缓存（AppData/Local/…/Differ/thumbs）
- 以图片为查询，按汉明距离召回相似图片，支持 TopK 与阈值
- 图形界面：左侧索引控制、中央缩略图网格、右侧预览与元信息、工具栏与状态条

> 这是一个高性能、可扩展的基础实现。后续可接入 GPU/FAISS/CLIP 等进行更高维语义检索。

## 构建（Windows, CMake + Qt6）

前置：
- 安装 CMake 3.21+
- 安装 Qt 6.2+（含 Qt6::Widgets/Sql/Concurrent 组件）
- 安装 Visual Studio 2022 C++ 构建工具（或 MSVC 编译工具链）

示例（x64，本机已配置好 Qt6 环境变量或使用 Qt 的“开发者命令提示”）：

快速方式（推荐，脚本会自动选择合适的生成器；支持 MSVC 或 MinGW）：

```cmd
cd d:\Codebase\differ
scripts\build.bat
scripts\run.bat
```

手工方式（可选，按你的 Qt 安装路径调整 CMAKE_PREFIX_PATH）：

```cmd
cd d:\Codebase\differ
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:\Qt\6.6.3\msvc2019_64"
cmake --build build --target differ --config Release
build\bin\Release\differ.exe
```

说明：
- 根目录下的 `build.bat` 会根据 `CMAKE_PREFIX_PATH` 判断你使用的是 MSVC 还是 MinGW 套件：
	- 若路径包含 `mingw`，将使用 "MinGW Makefiles" 生成器，并尝试自动加入 Qt Tools MinGW 的 `bin` 到 PATH。
	- 否则默认使用 "Visual Studio 17 2022" 生成器（MSVC）。
- 你可以在环境变量设置 `CMAKE_PREFIX_PATH`，或直接编辑 `build.bat` 顶部默认路径：
	- MSVC 示例：`C:\Qt\6.6.3\msvc2019_64`
	- MinGW 示例：`D:\Qt\6.9.3\mingw_64`
	- 注意：Qt 套件与编译器需匹配（MSVC 套件配 VS 生成器；MinGW 套件配 MinGW 生成器）。

## 在 VS Code 中使用（避免 Ninja 冲突）

本仓库内置了 `CMakePresets.json`，推荐使用 VS Code 的 CMake Tools 选择预设来配置：

1. 打开命令面板，运行 “CMake: Select Configure Preset”，选择 `mingw-release`（或 `mingw-debug`）
2. 运行 “CMake: Configure” 完成生成
3. 运行 “CMake: Build” 或直接使用状态栏按钮

说明：
- 预设会将构建目录固定为 `build-mingw` 或 `build-mingw-debug`，避免与 Ninja/VS 生成器冲突
- 预设已内置 `CMAKE_PREFIX_PATH` 与 `Qt6_DIR`（当前默认 `D:/Qt/6.9.3/mingw_64`），如你的 Qt 路径不同请编辑 `CMakePresets.json`
- 若之前在 `build/` 下使用过 Ninja 导致冲突，请删除 `build/CMakeCache.txt` 和 `build/CMakeFiles/`，或改用预设指定的构建目录

## 使用
1. 启动程序后，在左侧“索引”中选择图片目录并点击“开始索引”
2. 索引完成后，中央展示缩略图。选择一张图，点击右侧“以所选/图片为查询，查找相似”，或使用工具栏“打开查询图片”
3. 通过工具栏调整 TopK 和最大汉明距离

数据位置：
- 数据库：`%LOCALAPPDATA%/Local/<Organization>/Differ/index.db`（由 `QStandardPaths::AppDataLocation` 决定）
- 缩略图：同目录 `thumbs/`

## 架构概览
- `ImageIndexer`：后台扫描与特征计算（pHash、缩略图）并写入 `SqliteStore`
- `SqliteStore`：QtSql SQLite 封装，表结构：`images(id, path, mtime, size, phash)`
- `ThumbnailModel`：从 DB 读取记录，提供缩略图网格数据模型；支持相似检索与结果展示
- `ImageHash`：实现 64-bit pHash（DCT 8x8）与汉明距离
- `MainWindow`：界面、交互与设置持久化

## 后续可选增强
- 双阶段检索：先 pHash 粗筛 + 语义向量（CLIP/ONNXRuntime/GPU）细排
- 向量库：FAISS/HNSW（百万级数据量）
- 文件系统监控：增量更新索引与删除同步
- 更复杂的缓存策略和异步缩略图加载

## 许可
本示例以学习和演示为目的，按你的项目需要调整与扩展。
