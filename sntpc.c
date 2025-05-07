#include <arpa/inet.h>
#include <err.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define SECONDS_1900_1970 (25567 * 86400U)

#pragma pack(1)
struct ntp_packet_t {
  uint32_t flags;
  uint32_t root_delay;
  uint32_t root_dispersion;
  char reference_identifier[4];
  uint64_t reference_timestamp;
  uint32_t originate_timestamp_hi;
  uint32_t originate_timestamp_lo;
  uint64_t receive_timestamp;
  uint32_t transmit_timestamp_hi;
  uint32_t transmit_timestamp_lo;
  uint32_t key_identifier;
  unsigned char message_digest[16];
};
#pragma pack()

int backwards = 0;
int settime = 1;
int port = 123;
const char *server = "0.0.0.0";
int threshold = 1;
int verbose = 0;

volatile sig_atomic_t stop = 0;

void handle_sigint(int sig) {
  stop = 1;
}

void usage() {
  printf("Usage: sntpc [-bhnv] [-p port] [-s server] [-t threshold]\n");
  printf("        -b  Allow time shift backwards (default forward only)\n");
  printf("        -h  Show this help message\n");
  printf("        -n  No set time (dry run)\n");
  printf("        -p  Set server port number (default 123)\n");
  printf("        -s  Set server name or IPv4 address (default pool.ntp.org)\n");
  printf("        -t  Set maximum time offset threshold (default 300 seconds)\n");
  printf("        -v  Verbose (default silent)\n");
  exit(0);
}

int main(int argc, char *argv[]) {
  if (sizeof(struct ntp_packet_t) != 68) {
    errx(1, "Structure size mismatch (got %lu, expected 68)", sizeof(struct ntp_packet_t));
  }

  int ch;
  while ((ch = getopt(argc, argv, "bhnp:s:t:v")) != -1) {
    switch (ch) {
    case 'b': backwards = 1; break;
    case 'h': usage(); break;
    case 'n': settime = 0; break;
    case 'p': port = atoi(optarg); break;
    case 's': server = optarg; break;
    case 't': threshold = atoi(optarg); break;
    case 'v': verbose = 1; break;
    }
  }

  signal(SIGINT, handle_sigint);
  signal(SIGTERM, handle_sigint);

  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) err(1, "socket");

  in_addr_t addr = inet_addr(server);
  if (addr == INADDR_NONE) {
    struct hostent *he = gethostbyname(server);
    if (he == NULL) errx(1, "gethostbyname: %s", hstrerror(h_errno));
    int n = 0;
    while (he->h_addr_list[n] != NULL) n++;
    addr = *(in_addr_t *)he->h_addr_list[arc4random() % n];
  }

  struct sockaddr_in sin;
  sin.sin_family = AF_INET;
  sin.sin_port = htons(port);
  sin.sin_addr.s_addr = addr;

  if (connect(sock, (struct sockaddr *)&sin, sizeof(sin)) < 0) err(1, "connect");

  while (!stop) {
    struct ntp_packet_t request;
    bzero(&request, sizeof(request));
    request.flags = htonl((4 << 27) | (3 << 24));
    request.transmit_timestamp_hi = htonl(time(NULL) + SECONDS_1900_1970);
    request.transmit_timestamp_lo = arc4random();

    if (send(sock, &request, sizeof(request), 0) < 0) {
      perror("send");
      break;
    }

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(sock, &fds);
    struct timeval tv = {2, 0};

    if (select(sock + 1, &fds, NULL, NULL, &tv) > 0) {
      struct ntp_packet_t reply;
      ssize_t n = recv(sock, &reply, sizeof(reply), 0);
      if (n >= (ssize_t)sizeof(reply) - 20) {
        uint32_t seconds_since_1900 = ntohl(reply.transmit_timestamp_hi);
        uint32_t seconds_since_1970 = seconds_since_1900 - SECONDS_1900_1970;
        time_t server_time = seconds_since_1970;
        time_t local_time = time(NULL);
        int offset = (int)server_time - (int)local_time;

        printf("server time: %s", ctime(&server_time));
        printf("local  time: %s", ctime(&local_time));
        printf("offset: %d seconds\n\n", offset);
      }
    } else {
      fprintf(stderr, "timeout or error\n");
    }

    sleep(1);
  }

  printf("sntpc: exiting on signal\n");
  close(sock);
  return 0;
}
