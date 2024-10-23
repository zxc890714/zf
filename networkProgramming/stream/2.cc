// 首先定义优化后的检查函数
inline bool isBlockUsed(const unsigned char* buffer, size_t blockSize) {
    // 快速检查关键位置
    static const size_t checkPositions[] = {
        0,              // 开始
        blockSize/4,    // 25%
        blockSize/2,    // 50%
        blockSize*3/4,  // 75%
        blockSize-1     // 结束
    };
    
    for(size_t pos : checkPositions) {
        if(buffer[pos] != 0xFF) {
            return true;
        }
    }
    return false;
}

// 主要的检查逻辑
size_t checkMTDUsage(int fd, const mtd_info_user& mtd_info) {
    size_t usedSize = 0;
    const size_t blockSize = mtd_info.erasesize;
    
    // 分配一次性buffer
    unsigned char* buffer = new unsigned char[blockSize];
    if (!buffer) {
        return 0;
    }

    // 预先计算总块数，避免重复计算
    const size_t totalBlocks = mtd_info.size / blockSize;
    
    for(size_t block = 0; block < totalBlocks; ++block) {
        const size_t offset = block * blockSize;
        
        // 坏块检查
        loff_t bad_check = offset;
        if(ioctl(fd, MEMGETBADBLOCK, &bad_check) > 0) {
            continue;
        }

        // 定位并读取
        if(lseek(fd, offset, SEEK_SET) < 0) {
            continue;
        }
        
        ssize_t readSize = read(fd, buffer, blockSize);
        if(readSize == blockSize && isBlockUsed(buffer, blockSize)) {
            usedSize += blockSize;
        }
    }

    delete[] buffer;
    return usedSize;
}

// 优化后的主函数
MTDUsage check_mtd_usage(const char* mtd_device) {
    MTDUsage usage = {0, 0, 0.0};
    
    // 打开设备
    int fd = open(mtd_device, O_RDONLY);
    if(fd < 0) {
        return usage;
    }

    // 获取MTD信息
    mtd_info_user mtd_info;
    if(ioctl(fd, MEMGETINFO, &mtd_info) < 0) {
        close(fd);
        return usage;
    }

    // 设置基本信息
    usage.total_size = mtd_info.size;
    usage.used_size = checkMTDUsage(fd, mtd_info);
    usage.usage_percent = ((float)usage.used_size / usage.total_size) * 100.0;

    close(fd);
    return usage;
}
