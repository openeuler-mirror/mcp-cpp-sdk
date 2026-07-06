#!/usr/bin/env python3
"""
MCP SDK 代码统计脚本
统计代码总行数、提交次数和项目信息
"""

import os
import subprocess
import json
import re
from datetime import datetime
from pathlib import Path
from collections import defaultdict, Counter

class CodeStats:
    def __init__(self, project_root):
        self.project_root = Path(project_root)
        self.stats = {
            'project_info': {},
            'file_stats': {},
            'language_stats': {},
            'module_stats': {},
            'git_stats': {},
            'summary': {}
        }
        
        # 代码文件扩展名映射
        self.language_extensions = {
            '.cpp': 'C++',
            '.cc': 'C++',
            '.cxx': 'C++',
            '.c': 'C',
            '.h': 'C/C++ Header',
            '.hpp': 'C++ Header',
            '.hxx': 'C++ Header',
            '.py': 'Python',
            '.js': 'JavaScript',
            '.ts': 'TypeScript',
            '.json': 'JSON',
            '.yaml': 'YAML',
            '.yml': 'YAML',
            '.md': 'Markdown',
            '.txt': 'Text',
            '.cmake': 'CMake',
            '.sh': 'Shell',
            '.bat': 'Batch',
            '.gitignore': 'Git Ignore',
            '.gitattributes': 'Git Attributes'
        }
        
        # 排除的目录和文件
        self.exclude_dirs = {
            '.git', 'build', 'node_modules', '.vscode', 
            'a_cursor_config', '__pycache__', '.pytest_cache',
            'cmake-build-*', 'Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel'
        }
        
        self.exclude_files = {
            '.DS_Store', 'Thumbs.db', '*.o', '*.obj', '*.exe', 
            '*.dll', '*.so', '*.dylib', '*.a', '*.lib'
        }

    def run_command(self, command, cwd=None):
        """运行命令并返回结果"""
        try:
            result = subprocess.run(
                command, 
                shell=True, 
                capture_output=True, 
                text=True, 
                cwd=cwd or self.project_root
            )
            return result.stdout.strip(), result.returncode
        except Exception as e:
            print(f"命令执行失败: {command}, 错误: {e}")
            return "", 1

    def get_project_info(self):
        """获取项目基本信息"""
        print("📊 获取项目信息...")
        
        # 项目名称和版本
        cmake_file = self.project_root / "CMakeLists.txt"
        if cmake_file.exists():
            with open(cmake_file, 'r', encoding='utf-8') as f:
                content = f.read()
                name_match = re.search(r'project\((\w+)', content)
                version_match = re.search(r'VERSION\s+([\d.]+)', content)
                
                self.stats['project_info']['name'] = name_match.group(1) if name_match else "mcp_sdk"
                self.stats['project_info']['version'] = version_match.group(1) if version_match else "1.0.0"
        
        # 项目描述
        readme_file = self.project_root / "README.md"
        if readme_file.exists():
            with open(readme_file, 'r', encoding='utf-8') as f:
                content = f.read()
                self.stats['project_info']['description'] = content.split('\n')[1] if len(content.split('\n')) > 1 else "MCP SDK Project"
        
        # 项目路径
        self.stats['project_info']['path'] = str(self.project_root)
        self.stats['project_info']['analysis_time'] = datetime.now().isoformat()

    def get_git_stats(self):
        """获取Git统计信息"""
        print("📈 获取Git统计信息...")
        
        # 检查是否是Git仓库
        if not (self.project_root / '.git').exists():
            self.stats['git_stats']['is_git_repo'] = False
            return
        
        self.stats['git_stats']['is_git_repo'] = True
        
        # 提交总数
        stdout, _ = self.run_command("git rev-list --count HEAD")
        self.stats['git_stats']['total_commits'] = int(stdout) if stdout.isdigit() else 0
        
        # 分支信息
        stdout, _ = self.run_command("git branch -r")
        branches = [line.strip() for line in stdout.split('\n') if line.strip()]
        self.stats['git_stats']['branches'] = branches
        self.stats['git_stats']['branch_count'] = len(branches)
        
        # 贡献者信息
        stdout, _ = self.run_command("git shortlog -sn")
        contributors = []
        for line in stdout.split('\n'):
            if line.strip():
                parts = line.strip().split('\t')
                if len(parts) >= 2:
                    commits = int(parts[0])
                    name = parts[1]
                    contributors.append({'name': name, 'commits': commits})
        
        self.stats['git_stats']['contributors'] = contributors
        self.stats['git_stats']['contributor_count'] = len(contributors)
        
        # 最近提交信息
        stdout, _ = self.run_command("git log -1 --pretty=format:'%H|%an|%ae|%ad|%s' --date=iso")
        if stdout:
            parts = stdout.split('|')
            if len(parts) >= 5:
                self.stats['git_stats']['last_commit'] = {
                    'hash': parts[0],
                    'author': parts[1],
                    'email': parts[2],
                    'date': parts[3],
                    'message': parts[4]
                }
        
        # 文件变更统计
        stdout, _ = self.run_command("git diff --stat")
        if stdout:
            # 解析git diff --stat输出
            lines = stdout.split('\n')
            total_changes = 0
            for line in lines:
                if 'files changed' in line:
                    # 提取文件数和变更行数
                    match = re.search(r'(\d+)\s+files?\s+changed', line)
                    if match:
                        self.stats['git_stats']['files_changed'] = int(match.group(1))
                    
                    match = re.search(r'(\d+)\s+insertions?\(\+\)', line)
                    if match:
                        self.stats['git_stats']['insertions'] = int(match.group(1))
                    
                    match = re.search(r'(\d+)\s+deletions?\(-\)', line)
                    if match:
                        self.stats['git_stats']['deletions'] = int(match.group(1))

    def count_file_lines(self, file_path):
        """统计单个文件的行数"""
        try:
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                lines = f.readlines()
                
            total_lines = len(lines)
            code_lines = 0
            comment_lines = 0
            blank_lines = 0
            
            in_multiline_comment = False
            
            for line in lines:
                line = line.strip()
                
                if not line:
                    blank_lines += 1
                    continue
                
                # 处理多行注释
                if '/*' in line and '*/' in line:
                    comment_lines += 1
                    continue
                elif '/*' in line:
                    in_multiline_comment = True
                    comment_lines += 1
                    continue
                elif '*/' in line:
                    in_multiline_comment = False
                    comment_lines += 1
                    continue
                elif in_multiline_comment:
                    comment_lines += 1
                    continue
                
                # 单行注释
                if line.startswith('//') or line.startswith('#'):
                    comment_lines += 1
                else:
                    code_lines += 1
            
            return {
                'total_lines': total_lines,
                'code_lines': code_lines,
                'comment_lines': comment_lines,
                'blank_lines': blank_lines
            }
        except Exception as e:
            print(f"读取文件失败: {file_path}, 错误: {e}")
            return {
                'total_lines': 0,
                'code_lines': 0,
                'comment_lines': 0,
                'blank_lines': 0
            }

    def should_exclude_file(self, file_path):
        """检查文件是否应该被排除"""
        file_name = file_path.name
        
        # 检查排除的文件名模式
        for pattern in self.exclude_files:
            if pattern.startswith('*'):
                if file_name.endswith(pattern[1:]):
                    return True
            elif file_name == pattern:
                return True
        
        return False

    def should_exclude_dir(self, dir_path):
        """检查目录是否应该被排除"""
        dir_name = dir_path.name
        
        # 检查排除的目录
        if dir_name in self.exclude_dirs:
            return True
        
        # 检查模式匹配
        for pattern in self.exclude_dirs:
            if '*' in pattern:
                import fnmatch
                if fnmatch.fnmatch(dir_name, pattern):
                    return True
        
        return False

    def scan_files(self):
        """扫描所有文件"""
        print("🔍 扫描项目文件...")
        
        file_count = 0
        total_lines = 0
        total_code_lines = 0
        total_comment_lines = 0
        total_blank_lines = 0
        
        language_stats = defaultdict(lambda: {
            'files': 0,
            'total_lines': 0,
            'code_lines': 0,
            'comment_lines': 0,
            'blank_lines': 0
        })
        
        module_stats = defaultdict(lambda: {
            'files': 0,
            'total_lines': 0,
            'code_lines': 0,
            'comment_lines': 0,
            'blank_lines': 0,
            'languages': set()
        })
        
        for root, dirs, files in os.walk(self.project_root):
            root_path = Path(root)
            
            # 过滤排除的目录
            dirs[:] = [d for d in dirs if not self.should_exclude_dir(root_path / d)]
            
            for file in files:
                file_path = root_path / file
                
                if self.should_exclude_file(file_path):
                    continue
                
                # 获取文件扩展名
                ext = file_path.suffix.lower()
                if ext not in self.language_extensions:
                    continue
                
                # 统计文件行数
                lines_info = self.count_file_lines(file_path)
                
                # 更新统计信息
                file_count += 1
                total_lines += lines_info['total_lines']
                total_code_lines += lines_info['code_lines']
                total_comment_lines += lines_info['comment_lines']
                total_blank_lines += lines_info['blank_lines']
                
                # 按语言统计
                language = self.language_extensions[ext]
                language_stats[language]['files'] += 1
                language_stats[language]['total_lines'] += lines_info['total_lines']
                language_stats[language]['code_lines'] += lines_info['code_lines']
                language_stats[language]['comment_lines'] += lines_info['comment_lines']
                language_stats[language]['blank_lines'] += lines_info['blank_lines']
                
                # 按模块统计
                relative_path = file_path.relative_to(self.project_root)
                module = relative_path.parts[0] if len(relative_path.parts) > 1 else 'root'
                module_stats[module]['files'] += 1
                module_stats[module]['total_lines'] += lines_info['total_lines']
                module_stats[module]['code_lines'] += lines_info['code_lines']
                module_stats[module]['comment_lines'] += lines_info['comment_lines']
                module_stats[module]['blank_lines'] += lines_info['blank_lines']
                module_stats[module]['languages'].add(language)
                
                # 保存文件详细信息
                self.stats['file_stats'][str(relative_path)] = {
                    'language': language,
                    'module': module,
                    'size': file_path.stat().st_size,
                    **lines_info
                }
        
        # 转换set为list以便JSON序列化
        for module in module_stats:
            module_stats[module]['languages'] = list(module_stats[module]['languages'])
        
        self.stats['language_stats'] = dict(language_stats)
        self.stats['module_stats'] = dict(module_stats)
        
        # 更新汇总信息
        self.stats['summary'] = {
            'total_files': file_count,
            'total_lines': total_lines,
            'total_code_lines': total_code_lines,
            'total_comment_lines': total_comment_lines,
            'total_blank_lines': total_blank_lines,
            'languages_count': len(language_stats),
            'modules_count': len(module_stats)
        }

    def generate_report(self):
        """生成统计报告"""
        print("\n" + "="*60)
        print("📊 MCP SDK 代码统计报告")
        print("="*60)
        
        # 项目信息
        print(f"\n📁 项目信息:")
        print(f"  名称: {self.stats['project_info']['name']}")
        print(f"  版本: {self.stats['project_info']['version']}")
        print(f"  路径: {self.stats['project_info']['path']}")
        print(f"  分析时间: {self.stats['project_info']['analysis_time']}")
        
        # 代码统计汇总
        summary = self.stats['summary']
        print(f"\n📈 代码统计汇总:")
        print(f"  总文件数: {summary['total_files']}")
        print(f"  总行数: {summary['total_lines']:,}")
        print(f"  代码行数: {summary['total_code_lines']:,}")
        print(f"  注释行数: {summary['total_comment_lines']:,}")
        print(f"  空行数: {summary['total_blank_lines']:,}")
        print(f"  支持语言数: {summary['languages_count']}")
        print(f"  模块数: {summary['modules_count']}")
        
        # Git统计
        if self.stats['git_stats']['is_git_repo']:
            git_stats = self.stats['git_stats']
            print(f"\n🔀 Git统计:")
            print(f"  总提交数: {git_stats['total_commits']}")
            print(f"  分支数: {git_stats['branch_count']}")
            print(f"  贡献者数: {git_stats['contributor_count']}")
            
            if 'last_commit' in git_stats:
                last_commit = git_stats['last_commit']
                print(f"  最新提交: {last_commit['message'][:50]}...")
                print(f"  提交者: {last_commit['author']}")
                print(f"  提交时间: {last_commit['date']}")
        else:
            print(f"\n🔀 Git统计: 不是Git仓库")
        
        # 按语言统计
        print(f"\n💻 按语言统计:")
        for lang, stats in sorted(self.stats['language_stats'].items(), 
                                key=lambda x: x[1]['total_lines'], reverse=True):
            print(f"  {lang:15} | 文件: {stats['files']:3d} | 总行: {stats['total_lines']:6,} | 代码: {stats['code_lines']:6,}")
        
        # 按模块统计
        print(f"\n📦 按模块统计:")
        for module, stats in sorted(self.stats['module_stats'].items(), 
                                  key=lambda x: x[1]['total_lines'], reverse=True):
            languages = ', '.join(stats['languages'])
            print(f"  {module:15} | 文件: {stats['files']:3d} | 总行: {stats['total_lines']:6,} | 语言: {languages}")
        
        # 贡献者统计
        if self.stats['git_stats']['is_git_repo'] and 'contributors' in self.stats['git_stats']:
            print(f"\n👥 贡献者统计:")
            for i, contributor in enumerate(self.stats['git_stats']['contributors'][:10], 1):
                print(f"  {i:2d}. {contributor['name']:20} | 提交: {contributor['commits']:3d}")
            if len(self.stats['git_stats']['contributors']) > 10:
                print(f"  ... 还有 {len(self.stats['git_stats']['contributors']) - 10} 位贡献者")

    def save_json_report(self, output_file="code_stats.json"):
        """保存JSON格式的详细报告"""
        output_path = self.project_root / output_file
        with open(output_path, 'w', encoding='utf-8') as f:
            json.dump(self.stats, f, indent=2, ensure_ascii=False)
        print(f"\n💾 详细报告已保存到: {output_path}")

    def run(self):
        """运行完整的统计分析"""
        print("🚀 开始MCP SDK代码统计分析...")
        
        self.get_project_info()
        self.get_git_stats()
        self.scan_files()
        self.generate_report()
        self.save_json_report()
        
        print(f"\n✅ 分析完成！")

def main():
    """主函数"""
    import sys
    
    # 获取项目根目录
    if len(sys.argv) > 1:
        project_root = sys.argv[1]
    else:
        project_root = os.path.dirname(os.path.abspath(__file__))
    
    # 检查项目根目录是否存在
    if not os.path.exists(project_root):
        print(f"❌ 错误: 项目目录不存在: {project_root}")
        sys.exit(1)
    
    # 运行统计分析
    stats = CodeStats(project_root)
    stats.run()

if __name__ == "__main__":
    main()
