# Differ - Similar Image Finder (C++/Qt)

> **Note:** This repository is deprecated. The new version has been migrated to [https://github.com/caojiachen1/differ.NET](https://github.com/caojiachen1/differ.NET), using Avalonia + .NET framework, with improved UI aesthetics and integrated more accurate DINOv3 algorithm.

A desktop application based on Qt6 + C++ for finding similar images.

## Build (Windows, CMake + Qt6)

Prerequisites:
- CMake 3.21+
- Qt 6.2+ (including Widgets/Sql/Concurrent)
- Visual Studio 2022 or MinGW

Run:
```cmd
build.bat
run.bat
```

## Usage
1. Select image directory, click "Start Indexing"
2. Select image, find similar images
3. Adjust TopK and Hamming distance

Data locations:
- Database: %LOCALAPPDATA%/Differ/index.db
- Thumbnails: same directory thumbs/

## License
This example is for learning and demonstration purposes. Adjust and extend as needed for your project.
