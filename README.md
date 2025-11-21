# Userspace Firewall written in C

## Project Context
This project implements a userspace firewall using Linux Netfilter’s NFQUEUE interface to mitigate SYN flood and distributed SYN flood attacks. It demonstrates packet‑level processing, rate‑limiting algorithms, and resilient system design under adversarial traffic.

### Project Architecture

End system running firewall and a server on port 8080. Our iptable rules tell netfilter to route all incoming packets to a queue and then to userspace where 
our firewall will make a decision of whether to accept or drop each packet. This decision is made based on multiple considerations. Our firewall uses token buckets to make decisions. 
There is a global bucket, for the purpose of putting a cap on the total traffic that can enter the system. There is a hashmap that maps seen IPs to a per-IP bucket for the purpose of 
limiting the traffic of any one IP. Finally there is another hashmap that aims to keep track of legitimate users of the system. These users will be unimpeded by the rate limiting of 
our other buckets during an attack. 

#### ASCII Diagram
```text
                  Incoming Traffic
                         |
                    [ iptables ]
                         |
                 -j NFQUEUE (queue 0)
                         |
                    Userspace Firewall
         +--------------------------------------+
         | Global Token Bucket (system cap)     |
         | Per-IP Token Buckets (hashtable)     |
         | Trusted Users Table   (IP -> Bucket) |
         +--------------------------------------+
                   |                |
                 ACCEPT            DROP
                   |                |
               Server (8080)        X

```

### Attack Mitigation

The attack that I am aiming to mitigate with this firewall is a DDos SYN flood attack. I am simulating this attack using hping3 on a Kali Linux virtual machine. While the attack is ongoing, I simulate 
legitimate users using aws ec2 instances that send requests to the server via curl. The success of my firewall is measured by how much (or hopefully how little) interruption these legitimate users
incur.


## Design Decisions

### Why NFQUEUE?

I chose NFQUEUE because I wanted a way to intercept and *act* on packets before they were processed by the OS. This project began on my native OS (MacOS) using /dev/bpf0 (Berkeley Packet Filters). This is where I wrote the code for processing the packet headers. Once I got to the point of the project where I could begin acting on packets, I realized that BPFs would not allow me to do this because I was getting the packets **after** the OS had processed it. My research led me to iptables and NFQUEUE as a solution to this problem and I found it to be the best fit for what I wanted to implement. The result of this decision was that I had to 
change to system OS to Linux. I did this originally with an AWS EC2 instance. This worked initially, however, I quickly realized that if I was going to be performing SYN attacks, they needed to take place over my own LAN. I then chose to change the system architecture one last time to two laptops connected by an ethernet cable with statically assigned IP addresses (hping3 spoofs IP addresses during the attack). This decision gave me the most control over my project with the least amount of variables.

### Why token buckets?

My goal with this project was to mitigate a Distributed Denial of Service attack. I landed on a token bucket mechanism because it felt very intuitive for this problem.

### How are “legitimate users” detected under spoofed traffic?

There are two IP tables managed in this firewall. One that maintains the most recent IP addresses that the system encounters and another that only gets entries of IP addresses that are confirmed to be legitimate users. We confirm legitimate users through metrics such as repeated traffic, packets per second, and protocol completeness (are the SYN-ACKs being ACKed?)

## Results

### Current Status

The firewall is mitigating DoS attacks from hping3 while supporting all legitimate traffic. 

### Planned Work

Handle DDoS attacks (spoofed ips by hping3) while supporting all legitimate traffic. I am also working on the physical architecture of the project. My next attempt is going to be with 2 laptops connected with a single ethernet cable. If that goes well, I may try to make the laptops go through a small travel router. That way I can scale the legitimate users very easily!

### Learning Outcomes

This project strengthened my understanding of Linux kernel packet processing, adversarial network conditions, and scalable mitigation techniques. It reinforced my interest in network security, distributed systems, and defensive tooling—areas I hope to pursue in the MSIS program at CMU INI.
