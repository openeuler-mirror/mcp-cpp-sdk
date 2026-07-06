# 客户端
./bin/mcp_filesystem_client_https_example \
  --host localhost \
  --port 8443 \
  --endpoint /mcp \
  false \
  false

# 服务器
 ./bin/mcp_filesystem_server_https_example \
  --host 0.0.0.0 \
  --port 8443 \
  --endpoint /mcp \
  --cert-file ./certs/server.crt \
  --key-file ./certs/server.key \
  --disable-session-validation