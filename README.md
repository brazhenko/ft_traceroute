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
```
traceroute to google.com (173.194.221.113), 30 hops max, 60 byte packets
 1  172.17.0.1 (172.17.0.1)  0.007 ms  0.006 ms  0.006 ms  
 2  192.168.20.1 (192.168.20.1)  1.475 ms  1.214 ms  1.414 ms  
 3  192.168.2.13 (192.168.2.13)  0.625 ms  0.511 ms  0.530 ms  
 4  195-133-239-81.in-addr.netone.ru (195.133.239.81)  1.735 ms  1.905 ms  1.782 ms  
 5  188-65-128-226.in-addr.netone.ru (188.65.128.226)  5.395 ms  4.745 ms  4.352 ms  
 6  188-65-128-225.in-addr.netone.ru (188.65.128.225)  1.419 ms  1.043 ms  1.014 ms  
 7  92.242.39.224 (92.242.39.224)  1.732 ms  2.718 ms  2.557 ms  
 8  10.200.16.216 (10.200.16.216)  1.814 ms  10.200.16.128 (10.200.16.128)  1.947 ms  10.200.16.216 (10.200.16.216)  1.741 ms  
 9  10.200.16.192 (10.200.16.192)  1.868 ms  1.752 ms  10.200.16.194 (10.200.16.194)  1.955 ms  
10  10.200.16.154 (10.200.16.154)  2.010 ms  10.200.16.53 (10.200.16.53)  1.881 ms  1.870 ms  
11  95.181.206.197 (95.181.206.197)  1.632 ms  1.925 ms  1.762 ms  
12  108.170.250.83 (108.170.250.83)  1.815 ms  3.397 ms  108.170.250.66 (108.170.250.66)  3.888 ms  
13  142.251.49.24 (142.251.49.24)  14.133 ms  *  *
14  ...
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
setsockopt(sock, SOL_IP, IP_RECVERR, (int[1]){ 1 }, sizeof(int));

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
    break;    
}
```

#### Reply messages
Trace-routing messages are usually answered with an ICMP message 
with a particular pair of icmp.type & icmp.code.
If you are not common with it yet, discover
[wikipedia](https://en.wikipedia.org/wiki/Internet_Control_Message_Protocol).

Original `traceroute` has a _unique_ behaviour for each situation.
I tried to reproduce some of them.

Let's imagine we have an _intermediate_ (in fact, it will be a destination)
router.

Each time it has a special configuration of firewall.
We want to find the behaviour of the original traceroute.

`NB` 33434 if the start port which traceroute uses. It is increased
for each probe. Fill free to play more with iptables configuration.

```bash
# server-side
iptables -I INPUT -p udp --dport 33434 -j REJECT --reject-with icmp-net-unreachable

# client-side
traceroute 172.17.0.3

traceroute to 172.17.0.3 (172.17.0.3), 30 hops max, 60 byte packets
 1  172.17.0.3 (172.17.0.3)  0.824 ms !N  0.710 ms  0.713 ms
```
```bash
# server-side
iptables -I INPUT -p udp --dport 33434 -j REJECT --reject-with icmp-host-unreachable

# client-side
traceroute 172.17.0.3

traceroute to 172.17.0.3 (172.17.0.3), 30 hops max, 60 byte packets
 1  172.17.0.3 (172.17.0.3)  1.784 ms !H  0.062 ms  0.045 ms
```

```bash
# server-side
iptables -I INPUT -p udp --dport 33434 -j REJECT --reject-with icmp-proto-unreachable

# client-side
traceroute 172.17.0.3

traceroute to 172.17.0.3 (172.17.0.3), 30 hops max, 60 byte packets
 1  172.17.0.3 (172.17.0.3)  0.079 ms !P  0.062 ms  0.021 ms
```

```bash
# server-side
iptables -I INPUT -p udp --dport 33434 -j REJECT --reject-with icmp-port-unreachable

# client-side
traceroute 172.17.0.3

#!!! `icmp-port-unreachable` if default answer when the port is not listened.
#!!! traceroute thinks that it is good end of tracerouting.
traceroute to 172.17.0.3 (172.17.0.3), 30 hops max, 60 byte packets
 1  172.17.0.3 (172.17.0.3)  0.704 ms  0.600 ms  0.578 ms
```

```bash
# server-side
iptables -I INPUT -p udp --dport 33434 -j REJECT --reject-with icmp-net-prohibited

# client-side
traceroute 172.17.0.3

traceroute to 172.17.0.3 (172.17.0.3), 30 hops max, 60 byte packets
 1  172.17.0.3 (172.17.0.3)  0.674 ms !X  0.339 ms  0.309 ms
```

I should notice that iptables configuration covers only a part of
ICMP-ICMP_DEST_UNREACH codes. For more exploration you need to advise another
method.

My implementation covers only this part of error codes, check `trcrt_print_result()`
in [source](process_trace.c).
