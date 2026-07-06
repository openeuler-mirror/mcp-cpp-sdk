#!/bin/bash

# MCP SDK 代码统计脚本 (Shell版本)
# 统计代码总行数、提交次数和项目信息

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# 项目根目录
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUTPUT_FILE="code_stats.txt"

echo -e "${BLUE}🚀 开始MCP SDK代码统计分析...${NC}"
echo "项目根目录: $PROJECT_ROOT"

# 创建输出文件
> "$OUTPUT_FILE"

# 函数：添加标题到输出文件
add_header() {
    echo "" >> "$OUTPUT_FILE"
    echo "================================================" >> "$OUTPUT_FILE"
    echo "$1" >> "$OUTPUT_FILE"
    echo "================================================" >> "$OUTPUT_FILE"
}

# 函数：添加统计信息到输出文件
add_stats() {
    echo "$1" >> "$OUTPUT_FILE"
}

# 1. 项目基本信息
echo -e "${YELLOW}📊 获取项目信息...${NC}"
add_header "项目基本信息"

# 项目名称和版本
if [ -f "CMakeLists.txt" ]; then
    PROJECT_NAME=$(grep -o 'project([^)]*' CMakeLists.txt | sed 's/project(//' | cut -d' ' -f1 | head -1)
    PROJECT_VERSION=$(grep 'VERSION' CMakeLists.txt | sed 's/.*VERSION[[:space:]]*//' | cut -d' ' -f1 | head -1)
    add_stats "项目名称: ${PROJECT_NAME:-mcp_sdk}"
    add_stats "项目版本: ${PROJECT_VERSION:-1.0.0}"
fi

# 项目描述
if [ -f "README.md" ]; then
    DESCRIPTION=$(sed -n '2p' README.md)
    add_stats "项目描述: $DESCRIPTION"
fi

add_stats "项目路径: $PROJECT_ROOT"
add_stats "分析时间: $(date '+%Y-%m-%d %H:%M:%S')"

# 2. Git统计信息
echo -e "${YELLOW}📈 获取Git统计信息...${NC}"
add_header "Git统计信息"

if [ -d ".git" ]; then
    # 总提交数
    TOTAL_COMMITS=$(git rev-list --count HEAD 2>/dev/null || echo "0")
    add_stats "总提交数: $TOTAL_COMMITS"
    
    # 分支信息
    BRANCH_COUNT=$(git branch -r 2>/dev/null | wc -l)
    add_stats "分支数: $BRANCH_COUNT"
    
    # 贡献者信息
    CONTRIBUTOR_COUNT=$(git shortlog -sn 2>/dev/null | wc -l)
    add_stats "贡献者数: $CONTRIBUTOR_COUNT"
    
    # 最近提交信息
    if [ "$TOTAL_COMMITS" -gt 0 ]; then
        LAST_COMMIT_HASH=$(git log -1 --pretty=format:'%H' 2>/dev/null)
        LAST_COMMIT_AUTHOR=$(git log -1 --pretty=format:'%an' 2>/dev/null)
        LAST_COMMIT_DATE=$(git log -1 --pretty=format:'%ad' --date=iso 2>/dev/null)
        LAST_COMMIT_MSG=$(git log -1 --pretty=format:'%s' 2>/dev/null)
        
        add_stats "最新提交: ${LAST_COMMIT_MSG:0:50}..."
        add_stats "提交者: $LAST_COMMIT_AUTHOR"
        add_stats "提交时间: $LAST_COMMIT_DATE"
    fi
    
    # 文件变更统计
    if [ "$TOTAL_COMMITS" -gt 0 ]; then
        DIFF_STAT=$(git diff --stat 2>/dev/null | tail -1)
        FILES_CHANGED=$(echo "$DIFF_STAT" | grep -o '[0-9]\+ files\? changed' | grep -o '[0-9]\+')
        INSERTIONS=$(echo "$DIFF_STAT" | grep -o '[0-9]\+ insertions\?([+]' | grep -o '[0-9]\+')
        DELETIONS=$(echo "$DIFF_STAT" | grep -o '[0-9]\+ deletions\?(-' | grep -o '[0-9]\+')
        
        if [ -n "$FILES_CHANGED" ]; then
            add_stats "变更文件数: $FILES_CHANGED"
        fi
        if [ -n "$INSERTIONS" ]; then
            add_stats "新增行数: $INSERTIONS"
        fi
        if [ -n "$DELETIONS" ]; then
            add_stats "删除行数: $DELETIONS"
        fi
    fi
else
    add_stats "Git状态: 不是Git仓库"
fi

# 3. 代码文件统计
echo -e "${YELLOW}🔍 扫描项目文件...${NC}"
add_header "代码文件统计"

# 定义要统计的文件类型
CODE_EXTENSIONS="cpp cc cxx c h hpp hxx py js ts json yaml yml md txt cmake sh bat"

# 初始化计数器
TOTAL_FILES=0
TOTAL_LINES=0
TOTAL_CODE_LINES=0
TOTAL_COMMENT_LINES=0
TOTAL_BLANK_LINES=0

# 按语言统计
declare -A LANG_FILES
declare -A LANG_LINES
declare -A LANG_CODE_LINES
declare -A LANG_COMMENT_LINES
declare -A LANG_BLANK_LINES

# 按模块统计
declare -A MODULE_FILES
declare -A MODULE_LINES
declare -A MODULE_CODE_LINES
declare -A MODULE_COMMENT_LINES
declare -A MODULE_BLANK_LINES

# 排除的目录
EXCLUDE_DIRS=".git build node_modules .vscode a_cursor_config __pycache__ .pytest_cache cmake-build-* Debug Release RelWithDebInfo MinSizeRel"

# 排除的文件
EXCLUDE_FILES=".DS_Store Thumbs.db *.o *.obj *.exe *.dll *.so *.dylib *.a *.lib"

# 函数：检查是否应该排除目录
should_exclude_dir() {
    local dir="$1"
    for exclude in $EXCLUDE_DIRS; do
        if [[ "$dir" == *"$exclude"* ]]; then
            return 0
        fi
    done
    return 1
}

# 函数：检查是否应该排除文件
should_exclude_file() {
    local file="$1"
    for exclude in $EXCLUDE_FILES; do
        if [[ "$file" == *"$exclude"* ]]; then
            return 0
        fi
    done
    return 1
}

# 函数：统计文件行数
count_file_lines() {
    local file="$1"
    local ext="$2"
    
    if ! [ -r "$file" ]; then
        echo "0 0 0 0"
        return
    fi
    
    # 使用awk统计行数
    awk '
    BEGIN { total=0; code=0; comment=0; blank=0; in_comment=0 }
    {
        total++
        line = $0
        gsub(/^[ \t]+|[ \t]+$/, "", line)  # 去除首尾空白
        
        if (line == "") {
            blank++
        } else if (in_comment) {
            comment++
            if (line ~ /\*\//) {
                in_comment = 0
            }
        } else if (line ~ /\/\*.*\*\//) {
            comment++
        } else if (line ~ /\/\*/) {
            comment++
            in_comment = 1
        } else if (line ~ /\/\// || line ~ /^#/) {
            comment++
        } else {
            code++
        }
    }
    END { print total, code, comment, blank }
    ' "$file"
}

# 扫描文件
echo "正在扫描文件..."

while IFS= read -r -d '' file; do
    # 获取文件扩展名
    ext="${file##*.}"
    ext=".$ext"
    
    # 检查文件类型
    if [[ " $CODE_EXTENSIONS " =~ " ${ext#.} " ]]; then
        # 检查是否应该排除
        if should_exclude_file "$(basename "$file")"; then
            continue
        fi
        
        # 检查是否在排除的目录中
        dir_path="$(dirname "$file")"
        if should_exclude_dir "$dir_path"; then
            continue
        fi
        
        # 统计行数
        read -r file_total file_code file_comment file_blank <<< "$(count_file_lines "$file" "$ext")"
        
        # 更新总计数
        TOTAL_FILES=$((TOTAL_FILES + 1))
        TOTAL_LINES=$((TOTAL_LINES + file_total))
        TOTAL_CODE_LINES=$((TOTAL_CODE_LINES + file_code))
        TOTAL_COMMENT_LINES=$((TOTAL_COMMENT_LINES + file_comment))
        TOTAL_BLANK_LINES=$((TOTAL_BLANK_LINES + file_blank))
        
        # 按语言统计
        case "$ext" in
            .cpp|.cc|.cxx) lang="C++" ;;
            .c) lang="C" ;;
            .h|.hpp|.hxx) lang="C/C++ Header" ;;
            .py) lang="Python" ;;
            .js) lang="JavaScript" ;;
            .ts) lang="TypeScript" ;;
            .json) lang="JSON" ;;
            .yaml|.yml) lang="YAML" ;;
            .md) lang="Markdown" ;;
            .txt) lang="Text" ;;
            .cmake) lang="CMake" ;;
            .sh) lang="Shell" ;;
            .bat) lang="Batch" ;;
            *) lang="Other" ;;
        esac
        
        LANG_FILES["$lang"]=$((${LANG_FILES["$lang"]:-0} + 1))
        LANG_LINES["$lang"]=$((${LANG_LINES["$lang"]:-0} + file_total))
        LANG_CODE_LINES["$lang"]=$((${LANG_CODE_LINES["$lang"]:-0} + file_code))
        LANG_COMMENT_LINES["$lang"]=$((${LANG_COMMENT_LINES["$lang"]:-0} + file_comment))
        LANG_BLANK_LINES["$lang"]=$((${LANG_BLANK_LINES["$lang"]:-0} + file_blank))
        
        # 按模块统计
        relative_path="${file#$PROJECT_ROOT/}"
        module=$(echo "$relative_path" | cut -d'/' -f1)
        if [ -z "$module" ] || [ "$module" = "$relative_path" ]; then
            module="root"
        fi
        
        MODULE_FILES["$module"]=$((${MODULE_FILES["$module"]:-0} + 1))
        MODULE_LINES["$module"]=$((${MODULE_LINES["$module"]:-0} + file_total))
        MODULE_CODE_LINES["$module"]=$((${MODULE_CODE_LINES["$module"]:-0} + file_code))
        MODULE_COMMENT_LINES["$module"]=$((${MODULE_COMMENT_LINES["$module"]:-0} + file_comment))
        MODULE_BLANK_LINES["$module"]=$((${MODULE_BLANK_LINES["$module"]:-0} + file_blank))
    fi
done < <(find "$PROJECT_ROOT" -type f -print0)

# 输出汇总统计
add_stats "总文件数: $TOTAL_FILES"
add_stats "总行数: $(printf "%'d" $TOTAL_LINES)"
add_stats "代码行数: $(printf "%'d" $TOTAL_CODE_LINES)"
add_stats "注释行数: $(printf "%'d" $TOTAL_COMMENT_LINES)"
add_stats "空行数: $(printf "%'d" $TOTAL_BLANK_LINES)"

# 按语言统计
add_header "按语言统计"
for lang in "${!LANG_FILES[@]}"; do
    files=${LANG_FILES["$lang"]}
    lines=${LANG_LINES["$lang"]}
    code_lines=${LANG_CODE_LINES["$lang"]}
    printf "%-15s | 文件: %3d | 总行: %6s | 代码: %6s\n" \
        "$lang" "$files" "$(printf "%'d" $lines)" "$(printf "%'d" $code_lines)" >> "$OUTPUT_FILE"
done | sort -k4 -nr

# 按模块统计
add_header "按模块统计"
for module in "${!MODULE_FILES[@]}"; do
    files=${MODULE_FILES["$module"]}
    lines=${MODULE_LINES["$module"]}
    code_lines=${MODULE_CODE_LINES["$module"]}
    printf "%-15s | 文件: %3d | 总行: %6s | 代码: %6s\n" \
        "$module" "$files" "$(printf "%'d" $lines)" "$(printf "%'d" $code_lines)" >> "$OUTPUT_FILE"
done | sort -k4 -nr

# 4. 贡献者统计
if [ -d ".git" ] && [ "$TOTAL_COMMITS" -gt 0 ]; then
    add_header "贡献者统计"
    git shortlog -sn 2>/dev/null | head -10 | while read -r commits author; do
        printf "%-20s | 提交: %3d\n" "$author" "$commits" >> "$OUTPUT_FILE"
    done
fi

# 5. 显示结果
echo -e "${GREEN}✅ 分析完成！${NC}"
echo -e "${CYAN}📊 统计结果已保存到: $OUTPUT_FILE${NC}"

# 显示汇总信息
echo -e "\n${PURPLE}📈 汇总信息:${NC}"
echo -e "${BLUE}总文件数:${NC} $TOTAL_FILES"
echo -e "${BLUE}总行数:${NC} $(printf "%'d" $TOTAL_LINES)"
echo -e "${BLUE}代码行数:${NC} $(printf "%'d" $TOTAL_CODE_LINES)"
echo -e "${BLUE}注释行数:${NC} $(printf "%'d" $TOTAL_COMMENT_LINES)"
echo -e "${BLUE}空行数:${NC} $(printf "%'d" $TOTAL_BLANK_LINES)"

if [ -d ".git" ]; then
    echo -e "${BLUE}总提交数:${NC} $TOTAL_COMMITS"
    echo -e "${BLUE}贡献者数:${NC} $CONTRIBUTOR_COUNT"
fi

echo -e "\n${GREEN}🎉 代码统计完成！${NC}"
