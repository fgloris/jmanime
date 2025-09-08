# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目架构

这是一个基于 C++23 的微服务视频观看平台，采用 gRPC 进行服务间通信。

### 主要组件
- **backend/user_service**: 用户服务，处理认证、用户管理
- **backend/video_service**: 视频服务，处理视频相关功能  
- **backend/common**: 共享组件(配置、连接池、线程池)
- **backend/proto**: gRPC 协议定义文件
- **frontend**: 前端目录(当前为空)

### 架构模式
每个服务采用分层架构:
- **application**: 应用服务层
- **domain**: 领域层 
- **infrastructure**: 基础设施层(数据库访问)
- **interface**: 接口层(gRPC 服务实现)

### 技术栈
- C++23 with CMake
- gRPC & Protocol Buffers
- MySQL (through mysqlclient)
- Boost 1.89
- JWT authentication (jwt-cpp)
- Connection pooling for database

## 构建命令

### 初始构建
```bash
cd backend
# 清理并重新构建(推荐)
rm -rf build/*
cd build
cmake ..
make -j$(nproc)
```

### 增量构建
```bash
cd backend/build
make -j$(nproc)
```

### 运行服务
```bash
# 用户服务
./backend/build/user_service/user_service

# 用户服务客户端测试
./backend/build/user_service/user_service_client

# 邮件测试工具
./backend/build/user_service/testmail

# 视频服务
./backend/build/video_service/video_service

# 视频转码测试
./backend/build/video_service/testtranscoder
```

## 开发工作流

1. **修改 Proto 文件**: 修改 `backend/proto/*.proto` 后需要重新构建以生成新的 C++ 代码
2. **数据库设置**: 运行 `backend/mysql_setup/` 中的脚本初始化数据库
3. **添加依赖**: 第三方库通过 git submodules 管理，位于 `backend/third_parties/`

## 重要配置

- **CMake版本**: 3.22+
- **C++标准**: C++23
- **Boost路径**: /usr/local (自编译版本 1.89)
- **编译选项**: 开启调试模式，导出编译命令用于 IDE
- **生成文件**: Protocol buffer 生成文件位于 `backend/build/generated/`

## 测试

目前测试文件位于各服务的 `tests/` 目录下:
- `backend/user_service/tests/`: 用户服务测试
- 各服务也有独立的测试工具(如 testmail.cpp, testtranscoder.cpp)