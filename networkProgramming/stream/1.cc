#include <mtd/mtd-user.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

struct MTDUsage {
    size_t total_size;
    size_t used_size;
    float usage_percent;
};

MTDUsage check_mtd_usage(const char* mtd_device) {
    MTDUsage usage = {0, 0, 0.0};
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

    usage.total_size = mtd_info.size;
    
    // 分配一个擦除块大小的缓冲区
    unsigned char* buffer = new unsigned char[mtd_info.erasesize];
    
    // 遍历所有块
    for(size_t offset = 0; offset < mtd_info.size; offset += mtd_info.erasesize) {
        loff_t bad_check = offset;
        // 跳过坏块
        if(ioctl(fd, MEMGETBADBLOCK, &bad_check) > 0) {
            continue;
        }

        // 读取当前块
        lseek(fd, offset, SEEK_SET);
        if(read(fd, buffer, mtd_info.erasesize) > 0) {
            // 检查是否为空（未使用的块通常是全0xFF）
            bool is_used = false;
            for(size_t i = 0; i < mtd_info.erasesize; i++) {
                if(buffer[i] != 0xFF) {
                    is_used = true;
                    break;
                }
            }
            if(is_used) {
                usage.used_size += mtd_info.erasesize;
            }
        }
    }

    delete[] buffer;
    close(fd);

    // 计算使用百分比
    usage.usage_percent = ((float)usage.used_size / usage.total_size) * 100.0;
    
    return usage;
}

// 使用示例
void print_mtd_usage(const char* mtd_device) {
    MTDUsage usage = check_mtd_usage(mtd_device);
    
    printf("MTD Usage Information:\n");
    printf("Total Size: %zu bytes (%.2f MB)\n", 
           usage.total_size, 
           (float)usage.total_size / (1024*1024));
    printf("Used Size: %zu bytes (%.2f MB)\n", 
           usage.used_size, 
           (float)usage.used_size / (1024*1024));
    printf("Usage: %.2f%%\n", usage.usage_percent);
}
