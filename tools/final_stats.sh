#!/bin/bash

# MCP SDK 最终代码统计脚本
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

echo -e "${BLUE}🚀 开始MCP SDK代码统计分析...${NC}"

# 项目根目录
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
echo "项目根目录: $PROJECT_ROOT"

# 1. 项目基本信息
echo -e "${YELLOW}📊 获取项目信息...${NC}"
echo ""
echo "================================================"
echo "项目基本信息"
echo "================================================"

# 项目名称和版本
if [ -f "CMakeLists.txt" ]; then
    PROJECT_NAME=$(grep 'project(' CMakeLists.txt | head -1 | sed 's/project(//' | cut -d' ' -f1)
    PROJECT_VERSION=$(grep 'VERSION' CMakeLists.txt | head -1 | sed 's/.*VERSION[[:space:]]*//' | cut -d' ' -f1)
    echo "项目名称: ${PROJECT_NAME:-mcp_sdk}"
    echo "项目版本: ${PROJECT_VERSION:-1.0.0}"
fi

# 项目描述
if [ -f "README.md" ]; then
    DESCRIPTION=$(sed -n '2p' README.md)
    echo "项目描述: $DESCRIPTION"
fi

echo "项目路径: $PROJECT_ROOT"
echo "分析时间: $(date '+%Y-%m-%d %H:%M:%S')"

# 2. Git统计信息
echo ""
echo "================================================"
echo "Git统计信息"
echo "================================================"

if [ -d ".git" ]; then
    # 总提交数
    TOTAL_COMMITS=$(git rev-list --count HEAD 2>/dev/null || echo "0")
    echo "总提交数: $TOTAL_COMMITS"
    
    # 分支信息
    BRANCH_COUNT=$(git branch -r 2>/dev/null | wc -l)
    echo "分支数: $BRANCH_COUNT"
    
    # 贡献者信息
    CONTRIBUTOR_COUNT=$(git shortlog -sn 2>/dev/null | wc -l)
    echo "贡献者数: $CONTRIBUTOR_COUNT"
    
    # 最近提交信息
    if [ "$TOTAL_COMMITS" -gt 0 ]; then
        LAST_COMMIT_AUTHOR=$(git log -1 --pretty=format:'%an' 2>/dev/null)
        LAST_COMMIT_DATE=$(git log -1 --pretty=format:'%ad' --date=iso 2>/dev/null)
        LAST_COMMIT_MSG=$(git log -1 --pretty=format:'%s' 2>/dev/null)
        
        echo "最新提交: ${LAST_COMMIT_MSG:0:50}..."
        echo "提交者: $LAST_COMMIT_AUTHOR"
        echo "提交时间: $LAST_COMMIT_DATE"
    fi
else
    echo "Git状态: 不是Git仓库"
fi

# 3. 代码文件统计
echo ""
echo "================================================"
echo "代码文件统计"
echo "================================================"

# 初始化计数器
TOTAL_FILES=0
TOTAL_LINES=0

echo "正在扫描文件..."

# 创建临时文件存储统计结果
TEMP_FILE=$(mktemp)
TEMP_LANG_FILE=$(mktemp)
TEMP_MODULE_FILE=$(mktemp)

# 扫描文件并统计
find "$PROJECT_ROOT" -type f \( -name "*.cpp" -o -name "*.cc" -o -name "*.cxx" -o -name "*.c" -o -name "*.h" -o -name "*.hpp" -o -name "*.hxx" -o -name "*.py" -o -name "*.js" -o -name "*.ts" -o -name "*.json" -o -name "*.yaml" -o -name "*.yml" -o -name "*.md" -o -name "*.txt" -o -name "*.cmake" -o -name "*.sh" -o -name "*.bat" \) | while read -r file; do
    # 跳过排除的目录
    if [[ "$file" == *"/.git/"* ]] || [[ "$file" == *"/build/"* ]] || [[ "$file" == *"/node_modules/"* ]] || [[ "$file" == *"/.vscode/"* ]] || [[ "$file" == *"/a_cursor_config/"* ]]; then
        continue
    fi
    
    # 获取文件扩展名
    ext="${file##*.}"
    ext=".$ext"
    
    # 统计行数
    file_lines=$(wc -l < "$file" 2>/dev/null || echo "0")
    
    # 按语言分类
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
    
    # 按模块分类
    relative_path="${file#$PROJECT_ROOT/}"
    module=$(echo "$relative_path" | cut -d'/' -f1)
    if [ -z "$module" ] || [ "$module" = "$relative_path" ]; then
        module="root"
    fi
    
    # 写入临时文件
    echo "$lang|$file_lines" >> "$TEMP_LANG_FILE"
    echo "$module|$file_lines" >> "$TEMP_MODULE_FILE"
    echo "$file_lines" >> "$TEMP_FILE"
done

# 计算总文件数和总行数
TOTAL_FILES=$(wc -l < "$TEMP_FILE" 2>/dev/null || echo "0")
TOTAL_LINES=$(awk '{sum += $1} END {print sum}' "$TEMP_FILE" 2>/dev/null || echo "0")

# 输出汇总统计
echo "总文件数: $TOTAL_FILES"
echo "总行数: $(printf "%'d" $TOTAL_LINES)"

# 按语言统计
echo ""
echo "按语言统计:"
if [ -f "$TEMP_LANG_FILE" ] && [ -s "$TEMP_LANG_FILE" ]; then
    awk -F'|' '
    {
        lang_count[$1]++
        lang_lines[$1] += $2
    }
    END {
        for (lang in lang_count) {
            printf "%-15s | 文件: %3d | 总行: %6d\n", lang, lang_count[lang], lang_lines[lang]
        }
    }' "$TEMP_LANG_FILE" | sort -k4 -nr
fi

# 按模块统计
echo ""
echo "按模块统计:"
if [ -f "$TEMP_MODULE_FILE" ] && [ -s "$TEMP_MODULE_FILE" ]; then
    awk -F'|' '
    {
        module_count[$1]++
        module_lines[$1] += $2
    }
    END {
        for (module in module_count) {
            printf "%-15s | 文件: %3d | 总行: %6d\n", module, module_count[module], module_lines[module]
        }
    }' "$TEMP_MODULE_FILE" | sort -k4 -nr
fi

# 4. 贡献者统计
if [ -d ".git" ] && [ "$TOTAL_COMMITS" -gt 0 ]; then
    echo ""
    echo "================================================"
    echo "贡献者统计"
    echo "================================================"
    git shortlog -sn 2>/dev/null | head -10 | while read -r commits author; do
        printf "%-20s | 提交: %3d\n" "$author" "$commits"
    done
fi

# 清理临时文件
rm -f "$TEMP_FILE" "$TEMP_LANG_FILE" "$TEMP_MODULE_FILE"

# 5. 显示结果
echo ""
echo -e "${GREEN}✅ 分析完成！${NC}"

# 显示汇总信息
echo -e "\n${PURPLE}📈 汇总信息:${NC}"
echo -e "${BLUE}总文件数:${NC} $TOTAL_FILES"
echo -e "${BLUE}总行数:${NC} $(printf "%'d" $TOTAL_LINES)"

if [ -d ".git" ]; then
    echo -e "${BLUE}总提交数:${NC} $TOTAL_COMMITS"
    echo -e "${BLUE}贡献者数:${NC} $CONTRIBUTOR_COUNT"
fi

echo -e "\n${GREEN}🎉 代码统计完成！${NC}"
