#!/bin/bash
set -e
MODE="$1"
cd /home/ink/src/Periodic

if [ "$MODE" = "full" ]; then
    echo "===== Periodic 全量编译 ====="
    rm -rf build bin
    mkdir -p build bin
    cd build
    cmake ..
    make -j8
    echo "✅ 全量编译完成，程序在 ./bin/Periodic"
elif [ "$MODE" = "fast" ]; then
    echo "===== Periodic 增量编译 ====="
    mkdir -p build bin
    cd build
    cmake ..
    make -j8
    echo "✅ 增量编译完成，程序在 ./bin/Periodic"
elif [ "$MODE" = "clean" ] || [ "$MODE" = "myclean" ]; then
    echo "===== Periodic 清理 ====="
    rm -rf build bin
    echo "✅ 已清理所有编译产物"
else
    echo "===== Periodic 默认编译 ====="
    mkdir -p build bin
    cd build
    cmake ..
    make -j8
    echo "✅ 编译完成，程序在 ./bin/Periodic"
    echo "用法: ./rebuild.sh [full|fast|clean]"
fi
