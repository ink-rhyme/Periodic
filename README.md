# Periodic · 元素周期表

C++17 / OpenGL 3.3 / GLFW / FreeType 的桌面元素周期表。
元素数据全部来自 `assets/elements_cn.csv` 与 `assets/elements_en.csv`，代码里只写死界面样式与布局常量。

## 编译

```bash
./rebuild.sh fast     # 增量
./rebuild.sh full     # 全量
./rebuild.sh clean    # 清理
```

产物 `bin/Periodic`，assets 由 CMake 的 POST_BUILD 自动拷到 `bin/assets`。

## 运行参数

| 参数 | 作用 |
| --- | --- |
| `--version` / `-v` | 打印版本号与提交哈希后退出 |
| `--select <原子序数>` | 启动时选中指定元素 |
| `--shot <路径.bmp>` | 渲染一帧并截图后退出 |
| `--tooltip` | 带上原子核悬浮窗截图 |
| `--icon <路径.bmp>` | 导出图标（只画深空底+轨道+核子） |
| `--bench` | 200 帧性能测试 |
| `--debug` | 画角标诊断 |

## 版本管理

版本号记录在 `VERSION` 文件，配合 git tag（`v1.0.0` 形式）使用；程序启动会打印 `Periodic v<版本> (<提交>)`。

```bash
./version.sh list                # 列出所有版本及其提交 hash
./version.sh current             # 当前版本 / 分支 / 提交
./version.sh get 1.0.0           # 把 v1.0.0 的代码拉到独立目录 ../Periodic-1.0.0（不动当前工作区）
./version.sh switch 1.0.0        # 当前工作区切到 v1.0.0（分离头指针，只读查看）
./version.sh back                # 切回 main
./version.sh release 1.1.0       # 写 VERSION、提交、打附注标签并推送
```

`get` 用的是 `git worktree`，所以可以同时把多个版本摊在不同目录里对照，互不干扰。
