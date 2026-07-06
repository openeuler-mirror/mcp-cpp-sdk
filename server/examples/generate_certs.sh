#!/bin/bash

# 证书生成脚本
# 用于生成HTTPS客户端和服务器所需的SSL证书
#
# 使用方法:
#   ./generate_certs.sh [方案]
#
# 方案:
#   1. self-signed  - 生成自签名证书（最简单，用于测试）
#   2. ca-signed    - 生成CA证书和服务器证书（推荐用于开发/测试）
#
# 默认使用方案2 (ca-signed)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CERT_DIR="${SCRIPT_DIR}/certs"

# 创建证书目录
mkdir -p "${CERT_DIR}"

# 方案选择
MODE="${1:-ca-signed}"

echo "=========================================="
echo "🔐 SSL证书生成工具"
echo "=========================================="
echo ""

case "$MODE" in
    "self-signed"|"1")
        echo "📝 方案1: 生成自签名证书（用于测试）"
        echo ""
        
        CERT_FILE="${CERT_DIR}/server.crt"
        KEY_FILE="${CERT_DIR}/server.key"
        CA_FILE="${CERT_DIR}/ca.crt"
        
        echo "正在生成自签名证书..."
        openssl req -x509 \
            -newkey rsa:4096 \
            -keyout "${KEY_FILE}" \
            -out "${CERT_FILE}" \
            -days 365 \
            -nodes \
            -subj "/C=CN/ST=Beijing/L=Beijing/O=Test/OU=Test/CN=localhost" \
            -addext "subjectAltName=DNS:localhost,DNS:*.localhost,IP:127.0.0.1,IP:::1"
        
        # 对于自签名证书，CA文件就是证书本身
        cp "${CERT_FILE}" "${CA_FILE}"
        
        echo ""
        echo "✅ 证书生成成功！"
        echo ""
        echo "📁 生成的文件:"
        echo "   服务器证书: ${CERT_FILE}"
        echo "   服务器私钥: ${KEY_FILE}"
        echo "   CA证书(用于客户端): ${CA_FILE}"
        echo ""
        echo "💡 使用方法:"
        echo "   服务器: ./netio_https_server_demo ${CERT_FILE} ${KEY_FILE} 0.0.0.0 10443 /mcp"
        echo "   客户端: ./netio_https_client_demo localhost 10443 /mcp ${CA_FILE}"
        echo ""
        ;;
    
    "ca-signed"|"2")
        echo "📝 方案2: 生成CA证书和服务器证书（推荐）"
        echo ""
        
        # ============================================================
        # 证书文件定义
        # ============================================================
        # CA证书（根证书）:
        #   - ca.crt: CA证书，客户端用它来验证服务器证书
        #   - ca.key: CA私钥，用于签名服务器证书（保密！）
        CA_KEY="${CERT_DIR}/ca.key"
        CA_CERT="${CERT_DIR}/ca.crt"
        
        # 服务器证书:
        #   - server.crt: 服务器证书，服务器使用，由CA签名
        #   - server.key: 服务器私钥，服务器使用（保密！）
        SERVER_KEY="${CERT_DIR}/server.key"
        SERVER_CSR="${CERT_DIR}/server.csr"  # 临时文件，签名后删除
        SERVER_CERT="${CERT_DIR}/server.crt"
        
        # ============================================================
        # 步骤1: 生成CA私钥（根证书私钥）
        # ============================================================
        # 用途: CA用这个私钥来签名服务器证书
        # 使用者: CA自己（保密！）
        echo "步骤1/4: 生成CA私钥（根证书私钥）..."
        openssl genrsa -out "${CA_KEY}" 4096
        
        # ============================================================
        # 步骤2: 生成CA证书（根证书）
        # ============================================================
        # 用途: 
        #   - 客户端用它来验证服务器证书是否可信
        #   - CA用它来签名服务器证书
        # 使用者: 客户端（需要ca.crt）、CA（需要ca.key）
        # 特点: basicConstraints=CA:TRUE 标识这是证书颁发机构
        echo "步骤2/4: 生成CA证书（根证书）..."
        openssl req -new -x509 \
            -days 3650 \
            -key "${CA_KEY}" \
            -out "${CA_CERT}" \
            -subj "/C=CN/ST=Beijing/L=Beijing/O=Test CA/OU=Test/CN=Test CA" \
            -addext "basicConstraints=critical,CA:TRUE"
        
        # ============================================================
        # 步骤3: 生成服务器私钥
        # ============================================================
        # 用途: 服务器用这个私钥进行HTTPS加密通信
        # 使用者: 服务器（保密！）
        echo "步骤3/4: 生成服务器私钥..."
        openssl genrsa -out "${SERVER_KEY}" 4096
        
        # ============================================================
        # 步骤4: 生成服务器证书签名请求(CSR)
        # ============================================================
        # 用途: 向CA申请证书签名
        # 注意: CN=localhost 必须匹配服务器的主机名
        #      subjectAltName 允许的主机名/IP列表
        echo "步骤4/4: 生成并签名服务器证书..."
        openssl req -new \
            -key "${SERVER_KEY}" \
            -out "${SERVER_CSR}" \
            -subj "/C=CN/ST=Beijing/L=Beijing/O=Test Server/OU=Test/CN=localhost" \
            -addext "subjectAltName=DNS:localhost,DNS:*.localhost,IP:127.0.0.1,IP:::1"
        
        # ============================================================
        # 步骤5: 使用CA签名服务器证书
        # ============================================================
        # 用途: 生成最终的服务器证书，由CA签名
        # 使用者: 服务器（使用server.crt和server.key）
        # 验证者: 客户端（使用ca.crt验证server.crt）
        openssl x509 -req \
            -days 365 \
            -in "${SERVER_CSR}" \
            -CA "${CA_CERT}" \
            -CAkey "${CA_KEY}" \
            -CAcreateserial \
            -out "${SERVER_CERT}" \
            -extensions v3_req \
            -extfile <(cat <<EOF
[v3_req]
subjectAltName=DNS:localhost,DNS:*.localhost,IP:127.0.0.1,IP:::1
EOF
        )
        
        # 清理CSR文件
        rm -f "${SERVER_CSR}" "${CERT_DIR}/ca.srl"
        
        echo ""
        echo "✅ 证书生成成功！"
        echo ""
        echo "📁 生成的文件:"
        echo "   CA证书(用于客户端验证): ${CA_CERT}"
        echo "   CA私钥(请妥善保管): ${CA_KEY}"
        echo "   服务器证书: ${SERVER_CERT}"
        echo "   服务器私钥: ${SERVER_KEY}"
        echo ""
        echo "💡 使用方法:"
        echo "   服务器: ./netio_https_server_demo ${SERVER_CERT} ${SERVER_KEY} 0.0.0.0 10443 /mcp"
        echo "   客户端: ./netio_https_client_demo localhost 10443 /mcp ${CA_CERT}"
        echo ""
        ;;
    
    *)
        echo "❌ 错误: 未知的方案 '$MODE'"
        echo ""
        echo "用法: $0 [方案]"
        echo ""
        echo "方案:"
        echo "  1 或 self-signed  - 生成自签名证书（最简单，用于测试）"
        echo "  2 或 ca-signed    - 生成CA证书和服务器证书（推荐）"
        echo ""
        exit 1
        ;;
esac

echo "=========================================="
echo "🔒 安全提示:"
echo "=========================================="
echo "1. 这些证书仅用于开发和测试"
echo "2. 生产环境请使用由可信CA签发的证书"
echo "3. 请妥善保管私钥文件，不要泄露"
echo "4. 证书有效期: 365天（自签名）或 3650天（CA）"
echo "=========================================="

