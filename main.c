#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>

#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <stdarg.h>

#include "ppp_interface.h"

extern uint8_t g_debug_level;

int m_tun_fd;
char m_tun_dev[100] = "tun0";
char m_tun_ipaddr[200] = "fe80::face/64";

int m_stty_fd;
int m_baudrate = 115200;
char m_stty_dev[100] = "tty.usbserial-FTGQYOJ2";
ppp_t *m_ppp;

int ssystem(const char *fmt, ...) __attribute__((__format__ (__printf__, 1, 2)));

int ssystem(const char *fmt, ...) {
  char cmd[128];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(cmd, sizeof(cmd), fmt, ap);
  va_end(ap);

  if (g_debug_level >= DEBUG_LEVEL_2)
    fprintf(stdout, "%s\n", cmd);
  fflush(stdout);
  return system(cmd);
}

void ifconf(char *tundev, char *ipaddr) {

#ifdef linux
  ssystem("ifconfig %s inet `hostname` up", tundev);
  ssystem("ifconfig %s add %s", tundev, ipaddr);

  /* radvd needs a link local address for routing */
#if 0
  /* fe80::1/64 is good enough */
  ssystem("ifconfig %s add fe80::1/64", tundev);
#elif 1
  /* Generate a link local address a la sixxs/aiccu */
  /* First a full parse, stripping off the prefix length */

  {
    char lladdr[40];
    char c, *ptr=(char *)ipaddr;
    uint16_t digit,ai,a[8],cc,scc,i;
    for(ai=0; ai<8; ai++) {
      a[ai]=0;
    }

    ai=0;
    cc=scc=0;
    while(c=*ptr++) {
      if(c=='/') break;
      if(c==':') {
	    if(cc)
	      scc = ai;
	    cc = 1;
	    if(++ai>7) break;
      } else {
	    cc=0;
	    digit = c-'0';
	    if (digit > 9) 
	      digit = 10 + (c & 0xdf) - 'A';
        a[ai] = (a[ai] << 4) + digit;
      }
    }
    /* Get # elided and shift what's after to the end */
    cc=8-ai;
    for(i=0;i<cc;i++) {
      if ((8-i-cc) <= scc) {
	    a[7-i] = 0;
      } else {
        a[7-i] = a[8-i-cc];
	    a[8-i-cc]=0;
      }
    }
    sprintf(lladdr,"fe80::%x:%x:%x:%x",a[1]&0xfefd,a[2],a[3],a[7]);
    ssystem("ifconfig %s add %s/64", tundev, lladdr);
  }
#endif /* link local */
#elif defined(__APPLE__)
  {
	char * itfaddr = strdup(ipaddr);
	char * prefix = index(itfaddr, '/');
	if ( prefix != NULL ) {
		*prefix = '\0';
		prefix++;
	} else {
		prefix = "64";
	}
    ssystem("ifconfig %s inet6 up", tundev );
    ssystem("ifconfig %s inet6 %s add", tundev, ipaddr);
    ssystem("sysctl -w net.inet6.ip6.forwarding=1");
    free(itfaddr);
  }
#else
  ssystem("ifconfig %s inet `hostname` %s up", tundev, ipaddr);
  ssystem("sysctl -w net.inet.ip.forwarding=1");
#endif /* !linux */

  ssystem("ifconfig %s\n", tundev);
}

void cleanup(void) {
#ifndef __APPLE__
  ssystem("ifconfig %s down", m_tun_dev);
#ifndef linux
  ssystem("sysctl -w net.ipv6.conf.all.forwarding=1");
#endif
  /* ssystem("arp -d %s", ipaddr); */
  ssystem("netstat -nr"
	  " | awk '{ if ($2 == \"%s\") print \"route delete -net \"$1; }'"
	  " | sh",
	  m_tun_dev);
#else
  {
    char *  itfaddr = strdup(m_tun_ipaddr);
    char *  prefix = index(itfaddr, '/');
    ssystem("ifconfig %s inet6 %s remove", m_tun_dev, m_tun_ipaddr);
    ssystem("ifconfig %s down", m_tun_dev);
    if ( prefix != NULL ) *prefix = '\0';
    ssystem("route delete -inet6 %s", itfaddr);
    free(itfaddr);
  }
#endif
}

int stty_telos(int fd, int baudrate) {
  struct termios tty;
  speed_t speed;
  int i;

  switch(baudrate) {
  case -2:
    break;			/* Use default. */
  case 9600:
    speed = B9600;
    break;
  case 19200:
    speed = B19200;
    break;
  case 38400:
    speed = B38400;
    break;
  case 57600:
    speed = B57600;
    break;
  case 115200:
    speed = B115200;
    break;
  case 230400:
    speed = B230400;
    break;
  default:
    fprintf(stderr, "unknown baudrate %d", baudrate);
    return -1;
  }

  if(tcflush(fd, TCIOFLUSH) == -1)
    fprintf(stderr, "Error: tcflush()\n");

  if(tcgetattr(fd, &tty) == -1)
    fprintf(stderr, "Error: tcgetattr()\n");

  cfmakeraw(&tty);

  /* Nonblocking read. */
  tty.c_cc[VTIME] = 0;
  tty.c_cc[VMIN] = 0;

  tty.c_cflag &= ~CRTSCTS;
  tty.c_cflag &= ~HUPCL;
  tty.c_cflag &= ~CLOCAL;

  cfsetispeed(&tty, speed);
  cfsetospeed(&tty, speed);

  if(tcsetattr(fd, TCSAFLUSH, &tty) == -1)
    fprintf(stderr, "Error: tcsetattr()\n");

#if 1
  /* Nonblocking read and write. */
  /* if(fcntl(fd, F_SETFL, O_NONBLOCK) == -1) err(1, "fcntl"); */

  tty.c_cflag |= CLOCAL;
  if(tcsetattr(fd, TCSAFLUSH, &tty) == -1)
    fprintf(stderr, "Error: tcsetattr()\n");

  i = TIOCM_DTR;
  if(ioctl(fd, TIOCMBIS, &i) == -1)
    fprintf(stderr, "Error: ioctl()\n");
#endif

  usleep(10*1000);		/* Wait for hardware 10ms. */

  /* Flush input and output buffers. */
  if(tcflush(fd, TCIOFLUSH) == -1)
    fprintf(stderr, "Error: tcflush()\n");

  return 0;
}

void ppp_ip6_recv(ppp_t *ppp, uint8_t *buf, uint16_t len) {
  write(m_tun_fd, buf, len);
}

int raw_send(int fd, uint8_t *buf, uint16_t len) {
  return write(fd, buf, len);
}

void ppp_up(ppp_t *ppp) {
  if (g_debug_level >= DEBUG_LEVEL_2)
    printf("ppp up\n");
}

void ppp_down(ppp_t *ppp) {
  if (g_debug_level >= DEBUG_LEVEL_2)
    printf("ppp down\n");

  exit(0);
}

int serial_to_hdlc(int fd, ppp_t *ppp) {
  uint8_t buf[1500];
  int size;

  size = read(fd, buf, sizeof(buf));
  if (size > 0)
    ppp_interface_raw_recv(ppp, buf, size);

  return 0;
}

int tun_to_ppp(int fd, ppp_t *ppp) {
  uint8_t buf[1500];
  int size;

  size = read(fd, buf, sizeof(buf));
  if (size > 0)
    ppp_interface_send(ppp, buf, size);

  return 0;
}

void read_stdin(ppp_t *ppp) {
  char buf[500];
  int size;

  size = read(STDIN_FILENO, buf, sizeof(buf));
  if (size < 0)
    return;
#if 0
  if (memcmp(buf, "start", 5) == 0) {
    ppp_interface_enable(ppp);
  } else if (memcmp(buf, "stop", 4) == 0) {
    ppp_interface_disable(ppp);
  }
#endif
  if (strncmp(buf, "show", 5) == 0) {
    printf("m_tun_dev:%s\n", m_tun_dev);
    printf("m_tun_ipaddr:%s\n", m_tun_ipaddr);
    printf("m_stty_dev:%s\n", m_stty_dev);
    printf("baudrate:%d\n", m_baudrate);
    printf("debug_level:%d\n", g_debug_level);
  }
}

int dev_open(char *name, int flag) {
  char tun_name[100] = "/dev/";
  strcat(tun_name, name);
  return open(tun_name, flag);
}

void print_help() {
  fprintf(stdout,"usage: myppp [options] ipaddress\n\n");
  fprintf(stdout,"  example: myppp -d2 -s ttyUSB0 -b 115200 -t tun0 fe80::face/64\n");
  fprintf(stdout,"  Options are:\n");
  fprintf(stdout,"    -b baudrate    9600,19200,38400,57600,115200 (default),230400\n");
  fprintf(stdout,"    -s stty_dev    Serial device\n");
  fprintf(stdout,"    -t tun         Tun interface\n");
  fprintf(stdout,"    -d[level]      Debug level\n");
  fprintf(stdout,"      -d1         No Message\n");
  fprintf(stdout,"      -d2         All operation message (Default)\n");
  fprintf(stdout,"      -d3         All PPP level event\n");
  fprintf(stdout,"      -d4         All PPP packets in hex\n");
  fprintf(stdout,"\n");
}

uint8_t m_sigint_count = 0;

void sigcleanup(int signo) {
  if (m_sigint_count == 1) {
    exit(0); // exit(0) will call cleanup()
  }

  m_sigint_count++;
  ppp_interface_disable(m_ppp);
}

int  main (int argc, char **argv) {
  int max_fd, ret, c;
  fd_set rset, wset;

  while((c = getopt(argc, argv, "b:hs:t:d:")) != -1) {
    switch(c) {
    case 'b':
      m_baudrate = atoi(optarg);
      break;

    case 's':
      if(strncmp("/dev/", optarg, 5) == 0) {
        strncpy(m_stty_dev, optarg + 5, strlen(optarg) - 5);
      } else {
        strncpy(m_stty_dev, optarg, strlen(optarg));
      }
      break;

    case 't':
      if(strncmp("/dev/", optarg, 5) == 0) {
        strncpy(m_tun_dev, optarg + 5, strlen(optarg) - 5);
      } else {
	    strncpy(m_tun_dev, optarg, strlen(optarg));
      }
      break;

    case 'd':
      g_debug_level = 2;
      if (optarg) g_debug_level = atoi(optarg);
      break;

    case 'h':
    default:
      print_help();
      return 0;
    }
  }

  argc -= (optind - 1);
  argv += (optind - 1);

  if ((argc == 2) || (argc == 3)) 
    strncpy(m_tun_ipaddr, argv[1], strlen(argv[1]));

  m_stty_fd = dev_open(m_stty_dev, O_RDWR | O_NONBLOCK);
  if (m_stty_fd < 0) {
    fprintf(stderr, "Error: open %s fail\n", m_stty_dev);
    return -1;
  }

  if (stty_telos(m_stty_fd, m_baudrate) < 0) {
    close(m_stty_fd);
    return -1;
  }

  m_ppp = ppp_interface_create(m_stty_dev, m_stty_fd,
                               ppp_ip6_recv, raw_send, ppp_up, ppp_down);
  if (m_ppp < 0) {
    fprintf(stderr, "Error: ppp_interface_create()\n");
    close(m_stty_fd);
    return -1;
  }

  m_tun_fd = dev_open(m_tun_dev, O_RDWR);
  if (m_tun_fd < 0) {
    close(m_stty_fd);
    ppp_interface_free(m_ppp);
    fprintf(stderr, "Error: open %s fail\n", m_tun_dev);
    return -1;
  }

  ifconf(m_tun_dev, m_tun_ipaddr);
  atexit(cleanup);
  signal(SIGINT, sigcleanup);
  ppp_interface_enable(m_ppp);

  while(1) {
    max_fd = 0;
    FD_ZERO(&rset);
    FD_ZERO(&wset);
  
    FD_SET(STDIN_FILENO, &rset);
    FD_SET(m_stty_fd, &rset);
    FD_SET(m_tun_fd, &rset);
    if(m_stty_fd > max_fd)
      max_fd = m_stty_fd;

    if(m_tun_fd > max_fd)
      max_fd = m_tun_fd;

    ret = select(max_fd + 1, &rset, &wset, NULL, NULL);
    if(ret == -1 && errno != EINTR) {
      fprintf(stderr, "Error:select\n");
    } else if(ret > 0) {

      if(FD_ISSET(m_tun_fd, &rset)) {
        tun_to_ppp(m_tun_fd, m_ppp);
      }

      if(FD_ISSET(m_stty_fd, &rset)) {
        serial_to_hdlc(m_stty_fd, m_ppp);
      }

      if(FD_ISSET(STDIN_FILENO, &rset)) {
        read_stdin(m_ppp); 
      }
    }
  }

  close(m_tun_fd);
  close(m_stty_fd);
  ppp_interface_free(m_ppp);
  return 0;
}














