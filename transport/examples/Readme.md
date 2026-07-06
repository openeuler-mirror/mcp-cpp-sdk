
#1. https 使用的证书生成
 
 使用generate_certs.sh 生成certs的证书


#2. 启动服务器
./bin/https_server_example 0.0.0.0 8443 /mcp ./certs/server.crt ./certs/server.key "" true true


#3. 启动客户端
 
没有证书的
./bin/https_client_example localhost 8443 /mcp "" false false

有证书的
./bin/https_client_example localhost 8443 /mcp ./certs/ca.crt true true

