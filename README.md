# ft_traceroute

### About

`traceroute` is a common CLI tool which is included many distros. 
This tool provides the route of packet to network host.

The goal of this project is recode some original `traceroute` features 
and explore how does it work. This project is a part of `42` course, check 
the original [task](https://cdn.intra.42.fr/pdf/pdf/13249/en.subject.pdf).

### Prepare environment
With a very high probability you can use this repo on any linux-like distro,
but it was tested only in the environment described below. 

```bash
docker build -t remote -f Dockerfile.remote .
docker run -d --cap-add sys_ptrace -p127.0.0.1:2222:22 --name remote_env remote
ssh-keygen -f "$HOME/.ssh/known_hosts" -R "[localhost]:2222"
```

### Build

```bash
make
```

### Usage

```bash
./ft_traceroute google.com
```


### Available features

Check more:
```bash
./ft_traceroute --help
```

### Technical details

#### Tracerouting with ICMP (--icmp, required root or additional capability `setcap(8)`)

To complete the task I had to use raw socket, send some 
[ICMP messages](send_icmp_msg_v4.c) with increasing TTL and handle 
the "response".
This goal was achieved on the previous project, and I just needed to reuse  
the code. I would like do go deeper, because the original `traceroute` works 
with no root privileges.


#### Tracerouting with UDP (no root)
Calling `traceroute google.com` with no options generates some UDP messages
addressed to a particular port (!) which supposed to be unused. Error `ICMP_UNREACH_PORT` signalizes that the message reached destination. 

This part of socket usage seems to be covered badly, so some lines of code 
will be commented.

```c
// Create a UDP socket
sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
// Response messages will be not UDP but ICMP so we need some configuration
setsock = setsockopt(sock, SOL_IP, IP_RECVERR, (int[1]){ 1 }, sizeof(int));

// Check intermediate stages in send_udp_trcrt_msg_v4.c
...
// Actually sending out datagram
sendto(sock, "abc", 3, 0, dest_ip, sizeof dest_ip);

// Handling "response".
while (true) {
    // This call will immediately return -1 if no errors in queue
    ret = recvmsg(sock, &msg, MSG_ERRQUEUE)
    if (ret < 0) continue; // continue waiting
    
    // Handling error with some strange staff
    // provided by socket library. Check
    // process_trace.c trcrt_handle_udp() function
    // for more steps.
}
```

### Links

* https://habr.com/ru/post/281272/
* https://habr.com/ru/post/129627/