from scapy.all import ARP, Ether, sendp

def send_unicast_arp_request(target_ip, target_mac, source_ip, source_mac):
    """
    构造并发送单播 ARP 请求包
    
    :param target_ip: 目标设备的 IP 地址
    :param target_mac: 目标设备的 MAC 地址（单播发送需要指定）
    :param source_ip: 源设备的 IP 地址
    :param source_mac: 源设备的 MAC 地址
    """
    # 构造以太网头部，目标 MAC 设置为 target_mac（单播）
    ethernet = Ether(dst=target_mac, src=source_mac)

    # 构造 ARP 请求包
    arp_request = ARP(
        op=1,                 # ARP 请求 (1 表示请求, 2 表示响应)
        hwsrc=source_mac,     # 源 MAC 地址
        psrc=source_ip,       # 源 IP 地址
        hwdst=target_mac,     # 目标 MAC 地址（单播）
        pdst=target_ip        # 目标 IP 地址
    )

    # 将以太网头部和 ARP 请求包组合
    packet = ethernet / arp_request

    # 发送单播 ARP 请求包
    print(f"Sending unicast ARP request to {target_ip} ({target_mac})")
    sendp(packet, iface="YourNetworkInterfaceHere", verbose=True)

# 示例参数
if __name__ == "__main__":
    # 替换为实际值
    target_ip = "192.168.1.2"         # 目标 IP 地址
    target_mac = "00:11:22:33:44:55"  # 目标 MAC 地址
    source_ip = "192.168.1.1"         # 源 IP 地址
    source_mac = "66:77:88:99:AA:BB"  # 源 MAC 地址

    # 替换网卡名称，例如 "Ethernet" 或 "Wi-Fi"
    network_interface = "YourNetworkInterfaceHere"

    # 调用函数发送单播 ARP 请求
    send_unicast_arp_request(target_ip, target_mac, source_ip, source_mac)
