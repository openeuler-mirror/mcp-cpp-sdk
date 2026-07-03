

# 1.首先先编译

mkdir build 
cd build
cmake ..
make -j 4

# 2. 运行远程https

./bin/agent_https_demo

# 3. 运行ipc
nvm use 22.19

./bin/agnet_ipc_demo

