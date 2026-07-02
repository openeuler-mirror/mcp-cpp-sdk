#!/usr/bin/env python3
"""
批量替换transport模块中的std::cout为TRANSPORT_LOG_*宏
"""

import re
import os

def replace_cout_in_file(file_path):
    """替换文件中的std::cout输出为TRANSPORT_LOG_*宏"""
    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # 替换std::cout << "..." << std::endl; 为 TRANSPORT_LOG_INFO("...");
    pattern1 = r'std::cout\s*<<\s*"([^"]*)"\s*<<\s*std::endl;'
    replacement1 = r'TRANSPORT_LOG_INFO("\1");'
    content = re.sub(pattern1, replacement1, content)
    
    # 替换std::cout << "..." << std::endl; 为 TRANSPORT_LOG_INFO("...");
    pattern2 = r'std::cout\s*<<\s*"([^"]*)"\s*<<\s*std::endl;'
    replacement2 = r'TRANSPORT_LOG_INFO("\1");'
    content = re.sub(pattern2, replacement2, content)
    
    # 替换std::cout << "..." << std::endl; 为 TRANSPORT_LOG_INFO("...");
    pattern3 = r'std::cout\s*<<\s*"([^"]*)"\s*<<\s*std::endl;'
    replacement3 = r'TRANSPORT_LOG_INFO("\1");'
    content = re.sub(pattern3, replacement3, content)
    
    # 替换std::cout << "..." << std::endl; 为 TRANSPORT_LOG_INFO("...");
    pattern4 = r'std::cout\s*<<\s*"([^"]*)"\s*<<\s*std::endl;'
    replacement4 = r'TRANSPORT_LOG_INFO("\1");'
    content = re.sub(pattern4, replacement4, content)
    
    # 替换std::cout << "..." << std::endl; 为 TRANSPORT_LOG_INFO("...");
    pattern5 = r'std::cout\s*<<\s*"([^"]*)"\s*<<\s*std::endl;'
    replacement5 = r'TRANSPORT_LOG_INFO("\1");'
    content = re.sub(pattern5, replacement5, content)
    
    # 替换std::cout << "..." << std::endl; 为 TRANSPORT_LOG_INFO("...");
    pattern6 = r'std::cout\s*<<\s*"([^"]*)"\s*<<\s*std::endl;'
    replacement6 = r'TRANSPORT_LOG_INFO("\1");'
    content = re.sub(pattern6, replacement6, content)
    
    # 替换std::cout << "..." << std::endl; 为 TRANSPORT_LOG_INFO("...");
    pattern7 = r'std::cout\s*<<\s*"([^"]*)"\s*<<\s*std::endl;'
    replacement7 = r'TRANSPORT_LOG_INFO("\1");'
    content = re.sub(pattern7, replacement7, content)
    
    # 替换std::cout << "..." << std::endl; 为 TRANSPORT_LOG_INFO("...");
    pattern8 = r'std::cout\s*<<\s*"([^"]*)"\s*<<\s*std::endl;'
    replacement8 = r'TRANSPORT_LOG_INFO("\1");'
    content = re.sub(pattern8, replacement8, content)
    
    # 替换std::cout << "..." << std::endl; 为 TRANSPORT_LOG_INFO("...");
    pattern9 = r'std::cout\s*<<\s*"([^"]*)"\s*<<\s*std::endl;'
    replacement9 = r'TRANSPORT_LOG_INFO("\1");'
    content = re.sub(pattern9, replacement9, content)
    
    # 替换std::cout << "..." << std::endl; 为 TRANSPORT_LOG_INFO("...");
    pattern10 = r'std::cout\s*<<\s*"([^"]*)"\s*<<\s*std::endl;'
    replacement10 = r'TRANSPORT_LOG_INFO("\1");'
    content = re.sub(pattern10, replacement10, content)
    
    with open(file_path, 'w', encoding='utf-8') as f:
        f.write(content)
    
    print(f"已处理文件: {file_path}")

def main():
    files_to_process = [
        'sse_http_example.cpp',
        'sse_https_example.cpp', 
        'ipc_example.cpp'
    ]
    
    for filename in files_to_process:
        if os.path.exists(filename):
            replace_cout_in_file(filename)
        else:
            print(f"文件不存在: {filename}")

if __name__ == "__main__":
    main()
