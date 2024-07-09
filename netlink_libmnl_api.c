#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libmnl/libmnl.h>
#include <linux/rtnetlink.h>
#include <linux/if.h>

#define NLMSG_BUF_SIZE 4096

// Function to add an attribute to the netlink message
void add_attr(struct nlmsghdr *nlh, int type, const void *data, int len) {
    struct rtattr *rta = (struct rtattr *)mnl_nlmsg_get_payload_tail(nlh);
    rta->rta_type = type;
    rta->rta_len = RTA_LENGTH(len);
    memcpy(RTA_DATA(rta), data, len);
    nlh->nlmsg_len = NLMSG_ALIGN(nlh->nlmsg_len) + RTA_ALIGN(rta->rta_len);
}

int main() {
    struct mnl_socket *nl;
    struct nlmsghdr *nlh;
    struct ifinfomsg *ifm;
    char buf[NLMSG_BUF_SIZE];
    unsigned int seq, portid;

    // Create Netlink socket
    nl = mnl_socket_open(NETLINK_ROUTE);
    if (nl == NULL) {
        perror("mnl_socket_open");
        return -1;
    }

    // Bind the socket
    if (mnl_socket_bind(nl, 0, MNL_SOCKET_AUTOPID) < 0) {
        perror("mnl_socket_bind");
        mnl_socket_close(nl);
        return -1;
    }

    // Get port ID
    portid = mnl_socket_get_portid(nl);

    // Initialize Netlink message
    nlh = mnl_nlmsg_put_header(buf);
    nlh->nlmsg_type = RTM_NEWLINK;
    nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_EXCL;
    nlh->nlmsg_seq = seq = time(NULL);

    // Initialize ifinfomsg
    ifm = mnl_nlmsg_put_extra_header(nlh, sizeof(struct ifinfomsg));
    ifm->ifi_family = AF_UNSPEC;
    ifm->ifi_type = ARPHRD_ETHER;
    ifm->ifi_index = 0;
    ifm->ifi_flags = IFF_UP;
    ifm->ifi_change = 0xFFFFFFFF;

    // Add attributes
    const char *ifname = "dummy0";
    add_attr(nlh, IFLA_IFNAME, ifname, strlen(ifname) + 1);

    struct rtattr *linkinfo = mnl_nlmsg_put_extra_header(nlh, RTA_LENGTH(0));
    linkinfo->rta_type = IFLA_LINKINFO;
    linkinfo->rta_len = RTA_LENGTH(0);
    mnl_nlmsg_add_extra_tail(nlh, RTA_LENGTH(0));

    struct rtattr *info_data = mnl_nlmsg_put_extra_header(linkinfo, RTA_LENGTH(strlen("dummy") + 1));
    info_data->rta_type = IFLA_INFO_KIND;
    info_data->rta_len = RTA_LENGTH(strlen("dummy") + 1);
    strcpy(mnl_nlmsg_get_payload_tail(info_data), "dummy");
    linkinfo->rta_len = RTA_ALIGN(linkinfo->rta_len) + RTA_ALIGN(info_data->rta_len);
    nlh->nlmsg_len = NLMSG_ALIGN(nlh->nlmsg_len) + RTA_ALIGN(info_data->rta_len);

    // Send Netlink message
    if (mnl_socket_sendto(nl, nlh, nlh->nlmsg_len) < 0) {
        perror("mnl_socket_sendto");
        mnl_socket_close(nl);
        return -1;
    }

    printf("Dummy interface 'dummy0' created successfully.\n");

    // Clean up
    mnl_socket_close(nl);
    return 0;
}
