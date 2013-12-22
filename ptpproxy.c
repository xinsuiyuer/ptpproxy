/**
 * @file ptpproxy.c
 * @brief Forward IEEE 1588v2 packets.
 * @author xinsuiyuer
 * @version 0.1
 * @date 2013-12-17
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>    /* Must precede if*.h */
#include <net/if.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>

#include <pthread.h>

typedef struct {
  unsigned      index;
  char          name[32];
  unsigned char mac[ETH_ALEN];
  int           sock;
} Iface;

union ethframe
{
  struct
  {
    struct ethhdr    header;
    unsigned char    data[ETH_DATA_LEN];
  } field;
  unsigned char    raw[ETH_FRAME_LEN];
};

Iface inside, outside;
union ethframe in_packet, out_packet;

int init_iface(Iface *iface, const char *name);
int send_message(const Iface *to, union ethframe *buf, int len);
int recv_message(const Iface *from, union ethframe *buf);
void forward(const Iface *from, const Iface *to, union ethframe *buf);
void* proxy_client_to_master(void *arg);
void* proxy_master_to_client(void *arg);

int main(int argc, char *argv[])
{
  (void)argc;
  (void)argv;

  pthread_t thread_id = 0;

  memset(&inside, 0, sizeof(inside));
  memset(&outside, 0, sizeof(outside));
  inside.sock  = -1;
  outside.sock = -1;

  if(argc < 3) {
    printf("usage: %s inside_iface outside_iface\n"
           "  e.g. %s br0 eth0\n"
           "inside_iface  the interface's name between router and client\n"
           "outside_iface the interface's name between router and master\n",
           argv[0], argv[0]);

    return 1;
  }

  if(0 == strcmp(argv[1], argv[2])) {
    printf("Error! inside_iface(%s) mustn't be equal to outside_iface(%s)\n",
           argv[1],
           argv[2]);

    return 1;
  }

  if(0 != init_iface(&inside, argv[1])) {
    return 1;
  }

  if(0 != init_iface(&outside, argv[2])) {
    return 1;
  }

  if(0 != pthread_create(&thread_id, NULL, proxy_client_to_master, NULL)) {
    printf("pthread_create failed\n");
  } else {
    proxy_master_to_client(NULL);
    pthread_join(thread_id, NULL);
  }

  if(-1 != inside.sock)  { close(inside.sock);  }
  if(-1 != outside.sock) { close(outside.sock); }

  return 0;
}

int init_iface(Iface *iface, const char *name)
{

  struct sockaddr_ll addr_bind;
  struct ifreq req;
  int s = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_1588));
  if(-1 == s ) {
    perror("socket");
    return -1;
  }

  memset(iface, 0, sizeof(*iface));
  memset(&addr_bind, 0, sizeof(addr_bind));
  memset(&req, 0, sizeof(req));

  iface->sock = s;
  strcpy(iface->name, name);

  printf("Initialize %s\n", name);

  iface->index = if_nametoindex(iface->name);

  strncpy(req.ifr_name, name, IFNAMSIZ);
  if(-1 == ioctl(s, SIOCGIFHWADDR, &req)) {
    perror("ioctl");
    return -1;
  }

  memcpy(iface->mac, req.ifr_hwaddr.sa_data, ETH_ALEN);

  printf("%s: index: %d, MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
         iface->name, iface->index,
         iface->mac[0],
         iface->mac[1],
         iface->mac[2],
         iface->mac[3],
         iface->mac[4],
         iface->mac[5]
         );

  /* binding to specified interface */
  addr_bind.sll_family   = PF_PACKET;
  addr_bind.sll_protocol = htons(ETH_P_1588);
  addr_bind.sll_ifindex  = iface->index;

  if(-1 == bind(s, (struct sockaddr*)&addr_bind, sizeof(addr_bind))) {
    perror("bind");
    return -1;
  }

  return 0;
}

int send_message(const Iface *to, union ethframe *buf, int len)
{

  struct sockaddr_ll addr;
  memset(&addr, 0, sizeof(addr));
  addr.sll_family  = PF_PACKET;
  addr.sll_ifindex = to->index;
  addr.sll_halen   = ETH_ALEN;
  memcpy(addr.sll_addr, buf->field.header.h_dest, ETH_ALEN);

#if CHEAT
  memcpy(buf->field.header.h_source, to->mac, ETH_ALEN);
#endif

  return sendto(to->sock, buf->raw, len, 0,
         (struct sockaddr*)&addr, sizeof(addr));
}

int recv_message(const Iface *from, union ethframe *buf) {
  return recvfrom(from->sock, buf, sizeof(union ethframe), 0, NULL, NULL);
}

void forward(const Iface *from, const Iface *to, union ethframe *buf) {

  memset(&out_packet, 0, sizeof(*buf));

  while(1) {

    int len = recv_message(from, &out_packet);
    if(-1 == len) {
      if(errno == EINTR) {
        continue;
      } else {
        perror("recv_message");
        return;
      }
    }

    if(-1 == send_message(to, &out_packet, len)) {
      if(errno == EINTR) {
        continue;
      } else {
        perror("send_message");
        return;
      }
    }

    printf("%s -> %s\n", from->name, to->name);

  }

}

void* proxy_client_to_master(void *arg) {
  (void)arg;

  forward(&inside, &outside, &out_packet);
  printf("path broken: client --> router --> master\n");
}

void* proxy_master_to_client(void *arg) {
  (void)arg;

  forward(&outside, &inside, &in_packet);
  printf("path broken: master --> router --> client\n");
}
