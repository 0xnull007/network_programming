#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>

#define NLMSG_BUF_SIZE 4096

void add_attr(struct nlmsghdr *nlh, int maxlen, int type, const void *data, int len) {
    int attr_len = RTA_LENGTH(len);
    struct rtattr *rta;

    if (NLMSG_ALIGN(nlh->nlmsg_len) + RTA_ALIGN(attr_len) > maxlen) {
        fprintf(stderr, "message exceeded buffer space\n");
        exit(EXIT_FAILURE);
    }

    rta = (struct rtattr *)((char *)nlh + NLMSG_ALIGN(nlh->nlmsg_len));
    rta->rta_type = type;
    rta->rta_len = attr_len;
    memcpy(RTA_DATA(rta), data, len);
    nlh->nlmsg_len = NLMSG_ALIGN(nlh->nlmsg_len) + RTA_ALIGN(attr_len);
}

int main() {
    int sock_fd;
    struct sockaddr_nl src_addr, dest_addr;
    struct nlmsghdr *nlh;
    struct ifinfomsg *ifinfo;
    struct iovec iov;
    struct msghdr msg;
    char buf[NLMSG_BUF_SIZE];
    int seq = 0;

    // Create Netlink socket
    sock_fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (sock_fd < 0) {
        perror("socket");
        return -1;
    }

    // Initialize source address
    memset(&src_addr, 0, sizeof(src_addr));
    src_addr.nl_family = AF_NETLINK;
    src_addr.nl_pid = getpid();

    // Bind the socket
    if (bind(sock_fd, (struct sockaddr *)&src_addr, sizeof(src_addr)) < 0) {
        perror("bind");
        close(sock_fd);
        return -1;
    }

    // Initialize destination address
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.nl_family = AF_NETLINK;

    // Allocate and zero out Netlink message buffer
    memset(buf, 0, NLMSG_BUF_SIZE);

    // Initialize Netlink message
    nlh = (struct nlmsghdr *)buf;
    nlh->nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
    nlh->nlmsg_type = RTM_NEWLINK;
    nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_EXCL;
    nlh->nlmsg_seq = seq++;
    nlh->nlmsg_pid = getpid();

    // Initialize ifinfomsg
    ifinfo = (struct ifinfomsg *)NLMSG_DATA(nlh);
    ifinfo->ifi_family = AF_UNSPEC;
    ifinfo->ifi_type = ARPHRD_ETHER;
    ifinfo->ifi_index = 0;
    ifinfo->ifi_flags = IFF_UP;
    ifinfo->ifi_change = 0xFFFFFFFF;

    // Add attributes
    const char *ifname = "dummy0";
    add_attr(nlh, NLMSG_BUF_SIZE, IFLA_IFNAME, ifname, strlen(ifname) + 1);

    struct rtattr *linkinfo = (struct rtattr *)((char *)nlh + NLMSG_ALIGN(nlh->nlmsg_len));
    linkinfo->rta_type = IFLA_LINKINFO;
    linkinfo->rta_len = RTA_LENGTH(0);
    nlh->nlmsg_len = NLMSG_ALIGN(nlh->nlmsg_len) + RTA_ALIGN(linkinfo->rta_len);

    struct rtattr *info_data = (struct rtattr *)((char *)linkinfo + RTA_ALIGN(linkinfo->rta_len));
    info_data->rta_type = IFLA_INFO_KIND;
    info_data->rta_len = RTA_LENGTH(strlen("dummy") + 1);
    strcpy(RTA_DATA(info_data), "dummy");

    linkinfo->rta_len = RTA_ALIGN(linkinfo->rta_len) + RTA_ALIGN(info_data->rta_len);
    nlh->nlmsg_len = NLMSG_ALIGN(nlh->nlmsg_len) + RTA_ALIGN(info_data->rta_len);

    // Set up IO vector
    iov.iov_base = (void *)nlh;
    iov.iov_len = nlh->nlmsg_len;

    // Set up message header
    memset(&msg, 0, sizeof(msg));
    msg.msg_name = (void *)&dest_addr;
    msg.msg_namelen = sizeof(dest_addr);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    // Send Netlink message
    if (sendmsg(sock_fd, &msg, 0) < 0) {
        perror("sendmsg");
        close(sock_fd);
        return -1;
    }

    printf("Dummy interface 'dummy0' created successfully.\n");

    // Clean up
    close(sock_fd);
    return 0;
}
