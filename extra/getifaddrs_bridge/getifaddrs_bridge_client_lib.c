// getifaddrs_bridge_client_lib.c  --  This file is part of tiny_computer.               
                                                                        
// Copyright (C) 2023 Caten Hu                                          
                                                                        
// Tiny Computer is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published    
// by the Free Software Foundation, either version 3 of the License,    
// or any later version.                               
                                                                         
// Tiny Computer is distributed in the hope that it will be useful,          
// but WITHOUT ANY WARRANTY; without even the implied warranty          
// of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.              
// See the GNU General Public License for more details.                 
                                                                     
// You should have received a copy of the GNU General Public License    
// along with this program.  If not, see http://www.gnu.org/licenses/.

/* this file is mainly generated by Bing */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <sys/un.h>

#define BUFSIZE 1024 // 定义缓冲区大小

// 定义一个反序列化函数，将字节数组转换为ifaddrs结构体
int TINY_deserialize_ifaddrs(char *buf, int size, struct ifaddrs **ifap) {
    int len = 0; // 记录已经读取的字节数
    struct ifaddrs *head = NULL; // 链表头指针
    struct ifaddrs *tail = NULL; // 链表尾指针
    while (len < size) {
        // 为当前接口分配内存
        struct ifaddrs *ifa = (struct ifaddrs *)malloc(sizeof(struct ifaddrs));
        if (ifa == NULL) {
            // 分配失败，释放已分配的内存
            freeifaddrs(head);
            return -1; // 返回错误
        }
        // 读取接口名称
        int namelen = strlen(buf + len) + 1; // 包括结束符
        if (len + namelen > size) break; // 缓冲区不足
        ifa->ifa_name = (char *)malloc(namelen); // 为名称分配内存
        if (ifa->ifa_name == NULL) {
            // 分配失败，释放已分配的内存
            free(ifa);
            freeifaddrs(head);
            return -1; // 返回错误
        }
        memcpy(ifa->ifa_name, buf + len, namelen); // 复制名称
        len += namelen;
        // 读取接口标志
        if (len + sizeof(unsigned int) > size) break; // 缓冲区不足
        memcpy(&ifa->ifa_flags, buf + len, sizeof(unsigned int)); // 复制标志
        len += sizeof(unsigned int);
        // 读取接口地址
        if (buf[len] != '\0') {
            // 如果有地址
            int addrlen = sizeof(struct sockaddr); // 地址结构体长度
            if (len + addrlen > size) break; // 缓冲区不足
            ifa->ifa_addr = (struct sockaddr *)malloc(addrlen); // 为地址分配内存
            if (ifa->ifa_addr == NULL) {
                // 分配失败，释放已分配的内存
                free(ifa->ifa_name);
                free(ifa);
                freeifaddrs(head);
                return -1; // 返回错误
            }
            memcpy(ifa->ifa_addr, buf + len, addrlen); // 复制地址
            len += addrlen;
        } else {
            // 如果没有地址，跳过一个空字节
            ifa->ifa_addr = NULL;
            len += 1;
        }
        // 读取接口掩码
        if (buf[len] != '\0') {
            // 如果有掩码
            int masklen = sizeof(struct sockaddr); // 掩码结构体长度
            if (len + masklen > size) break; // 缓冲区不足
            ifa->ifa_netmask = (struct sockaddr *)malloc(masklen); // 为掩码分配内存
            if (ifa->ifa_netmask == NULL) {
                // 分配失败，释放已分配的内存
                free(ifa->ifa_addr);
                free(ifa->ifa_name);
                free(ifa);
                freeifaddrs(head);
                return -1; // 返回错误
            }
            memcpy(ifa->ifa_netmask, buf + len, masklen); // 复制
            len += masklen;
        } else {
            // 如果没有掩码，跳过一个空字节
            ifa->ifa_netmask = NULL;
            len += 1;
        }
        // 读取接口广播地址或点对点地址
        if (ifa->ifa_flags & IFF_BROADCAST) {
            // 如果有广播地址
            if (buf[len] != '\0') {
                // 如果有广播地址
                int broadlen = sizeof(struct sockaddr); // 广播地址结构体长度
                if (len + broadlen > size) break; // 缓冲区不足
                ifa->ifa_broadaddr = (struct sockaddr *)malloc(broadlen); // 为广播地址分配内存
                if (ifa->ifa_broadaddr == NULL) {
                    // 分配失败，释放已分配的内存
                    free(ifa->ifa_netmask);
                    free(ifa->ifa_addr);
                    free(ifa->ifa_name);
                    free(ifa);
                    freeifaddrs(head);
                    return -1; // 返回错误
                }
                memcpy(ifa->ifa_broadaddr, buf + len, broadlen); // 复制广播地址
                len += broadlen;
            } else {
                // 如果没有广播地址，跳过一个空字节
                ifa->ifa_broadaddr = NULL;
                len += 1;
            }
        } else if (ifa->ifa_flags & IFF_POINTOPOINT) {
            // 如果有点对点地址
            if (buf[len] != '\0') {
                // 如果有点对点地址
                int dstlen = sizeof(struct sockaddr); // 点对点地址结构体长度
                if (len + dstlen > size) break; // 缓冲区不足
                ifa->ifa_dstaddr = (struct sockaddr *)malloc(dstlen); // 为点对点地址分配内存
                if (ifa->ifa_dstaddr == NULL) {
                    // 分配失败，释放已分配的内存
                    free(ifa->ifa_netmask);
                    free(ifa->ifa_addr);
                    free(ifa->ifa_name);
                    free(ifa);
                    freeifaddrs(head);
                    return -1; // 返回错误
                }
                memcpy(ifa->ifa_dstaddr, buf + len, dstlen); // 复制点对点地址
                len += dstlen;
            } else {
                // 如果没有点对点地址，跳过一个空字节
                ifa->ifa_dstaddr = NULL;
                len += 1;
            }
        } else {
            // 如果没有广播地址或点对点地址，跳过两个空字节
            ifa->ifa_broadaddr = NULL;
            ifa->ifa_dstaddr = NULL;
            len += 2;
        }
        // 读取接口数据
        if (buf[len] != '\0') {
            // 如果有数据
            // TODO: 根据不同的地址族，读取不同的数据
            // 这里暂时省略，只跳过一个空字节
            ifa->ifa_data = NULL;
            len += 1;
        } else {
            // 如果没有数据，跳过一个空字节
            ifa->ifa_data = NULL;
            len += 1;
        }
        // 将当前接口插入链表
        ifa->ifa_next = NULL;
        if (head == NULL) {
            // 如果是第一个接口，设置头指针
            head = ifa;
        } else {
            // 如果不是第一个接口，设置尾指针的下一个指针
            tail->ifa_next = ifa;
        }
        // 更新尾指针
        tail = ifa;
    }
    *ifap = head; // 返回链表头指针
    return len; // 返回读取的总字节数
}

// 定义一个发送信号的函数，向服务器发送一个信号
int TINY_send_signal(int sockfd) {
    char sig = 'S'; // 定义信号为一个字符S
    int n = write(sockfd, &sig, 1); // 向套接字写入一个字节
    if (n < 0) {
        perror("write");
        return -1; // 返回错误
    }
    return 0; // 返回成功
}

// 定义一个接收数据的函数，从服务器接收数据并反序列化
int TINY_receive_data(int sockfd, struct ifaddrs **ifap) {
    char buf[BUFSIZE]; // 定义缓冲区
    int n = read(sockfd, buf, BUFSIZE); // 从套接字读取数据
    if (n < 0) {
        perror("read");
        return -1; // 返回错误
    }
    int len = TINY_deserialize_ifaddrs(buf, n, ifap); // 反序列化数据
    if (len < 0) {
        fprintf(stderr, "deserialize_ifaddrs failed\n");
        return -1; // 返回错误
    }
    return 0; // 返回成功
}

// 主函数
int getifaddrs(struct ifaddrs **ifap) {
    // 创建一个套接字
    int sockfd = socket(PF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        exit(1);
    }
    // 定义服务器地址结构体
    struct sockaddr_un un;
    memset(&un, 0, sizeof(un));
    un.sun_family = AF_UNIX;
    snprintf(un.sun_path, sizeof(un.sun_path), "%s", "/tmp/.getifaddrs-bridge");
    // 连接到服务器
    if (connect(sockfd, (struct sockaddr *)&un, sizeof(un)) < 0) {
        perror("connect");
        exit(1);
    }
    // 发送信号给服务器
    if (TINY_send_signal(sockfd) < 0) {
        fprintf(stderr, "send_signal failed\n");
        exit(1);
    }
    // 接收数据并反序列化
    if (TINY_receive_data(sockfd, ifap) < 0) {
        fprintf(stderr, "receive_data failed\n");
        exit(1);
    }
    // 关闭套接字
    close(sockfd);
    return 0;
}
