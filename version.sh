#!/bin/bash
# 版本管理脚本：按版本号查看 / 拉取 / 切换 / 发版
# 用法:
#   ./version.sh list              列出所有版本(tag)及其提交 hash
#   ./version.sh current           当前所在版本 / 分支 / 提交
#   ./version.sh get <版本号>       把该版本的代码拉到独立目录 ../Periodic-<版本号>（不影响当前工作区）
#   ./version.sh switch <版本号>    当前工作区切到该版本（只读查看用，改代码前先 ./version.sh back）
#   ./version.sh back              切回 main 分支
#   ./version.sh release <版本号>   写 VERSION、提交、打附注标签并推送到远端
set -e
cd "$(dirname "$0")"
ROOT=$(pwd)
REMOTE=origin

vlist() {
    echo "版本号        提交         日期                 说明"
    echo "--------------------------------------------------------------"
    git for-each-ref --sort=-creatordate --format='%(refname:short)|%(objectname:short)|%(creatordate:short)|%(contents:subject)' refs/tags |
    while IFS='|' read -r tag hash date subject; do
        printf "%-13s %-12s %-20s %s\n" "$tag" "$hash" "$date" "$subject"
    done
}

vcurrent() {
    local tag
    tag=$(git describe --tags --exact-match 2>/dev/null || true)
    echo "分支/状态 : $(git rev-parse --abbrev-ref HEAD)"
    echo "提交      : $(git rev-parse --short HEAD)"
    echo "版本      : ${tag:-$(cat VERSION 2>/dev/null || echo unknown)}"
    echo "VERSION   : $(cat VERSION 2>/dev/null || echo 未定义)"
    git status -sb | head -1
}

check_clean() {
    if [ -n "$(git status --porcelain)" ]; then
        echo "❌ 工作区有未提交改动，先提交或 stash 再操作"
        exit 1
    fi
}

vget() {
    local ver="$1"
    [ -z "$ver" ] && { echo "用法: ./version.sh get <版本号>"; exit 1; }
    git fetch --tags "$REMOTE"
    local tag="v$ver"
    git rev-parse "$tag" >/dev/null 2>&1 || tag="$ver"
    git rev-parse "$tag" >/dev/null 2>&1 || { echo "❌ 找不到版本 $ver"; exit 1; }
    local dir="../Periodic-${ver}"
    if [ -e "$dir" ]; then
        echo "❌ 目录已存在: $dir （先删掉或换版本号）"
        exit 1
    fi
    git worktree add "$dir" "$tag"
    echo "✅ 已拉取 $tag 到 $dir"
    echo "   提交: $(git rev-parse --short "$tag")"
}

vswitch() {
    local ver="$1"
    [ -z "$ver" ] && { echo "用法: ./version.sh switch <版本号>"; exit 1; }
    check_clean
    git fetch --tags "$REMOTE"
    local tag="v$ver"
    git rev-parse "$tag" >/dev/null 2>&1 || tag="$ver"
    git rev-parse "$tag" >/dev/null 2>&1 || { echo "❌ 找不到版本 $ver"; exit 1; }
    git checkout "$tag"
    echo "✅ 已切到 $tag（分离头指针，改代码前执行 ./version.sh back）"
}

vback() {
    git checkout main
    git pull --ff-only "$REMOTE" main
    echo "✅ 已回到 main"
}

vrelease() {
    local ver="$1"
    [ -z "$ver" ] && { echo "用法: ./version.sh release <版本号>"; exit 1; }
    if ! echo "$ver" | grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+$'; then
        echo "❌ 版本号必须是 主.次.补丁 形式，例如 1.1.0"
        exit 1
    fi
    git rev-parse "v$ver" >/dev/null 2>&1 && { echo "❌ 标签 v$ver 已存在"; exit 1; }
    printf '%s\n' "$ver" > VERSION
    git add VERSION
    git commit -m "发布 v$ver" || echo "（VERSION 无变化，跳过提交）"
    git tag -a "v$ver" -m "发布 v$ver"
    git push "$REMOTE" main
    git push "$REMOTE" "v$ver"
    echo "✅ 已发布 v$ver 并推送到远端"
}

CMD="${1:-}"
case "$CMD" in
    list)            vlist ;;
    current|status)  vcurrent ;;
    get)             vget "$2" ;;
    switch)          vswitch "$2" ;;
    back)            vback ;;
    release)         vrelease "$2" ;;
    *)
        echo "版本管理脚本"
        echo "  ./version.sh list               列出所有版本"
        echo "  ./version.sh current            当前版本/分支/提交"
        echo "  ./version.sh get <版本号>        拉取该版本到独立目录（不影响当前工作区）"
        echo "  ./version.sh switch <版本号>     当前工作区切到该版本"
        echo "  ./version.sh back               切回 main"
        echo "  ./version.sh release <版本号>    发布新版本（提交+打标签+推送）"
        ;;
esac
