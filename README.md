## This is a firewall written entirely in C that utilizes iptable rules and the Linux netfilter framework.

### Project Architecture

End system running firewall and a server on port 8080. Our iptable rules tell netfilter to route all incoming packets to a queue and then to userspace where 
our firewall will make a decision of whether to accept or drop each packet. This decision is made based on multiple considerations. Our firewall uses token buckets to make decisions. 
There is a global bucket, for the purpose of putting a cap on the total traffic that can enter the system. There is a hashmap that maps seen IPs to a per-IP bucket for the purpose of 
limiting the traffic of any one IP. Finally there is another hashmap that aims to keep track of legitimate users of the system. These users will be unimpeded by the rate limiting of 
our other buckets during an attack. 

### Attack Mitigation

The attack that I am aiming to mitigate with this firewall is a DDos SYN flood attack. I am simulating this attack using hping3 on a Kali Linux virtual machine. While the attack is ongoing, I simulate 
legitimate users using aws ec2 instances that send requests to the server via curl. The success of my firewall is measured by how much (or hopefully how little) interruption these legitimate users
incur.

### Results

TODO: have not concluded the project :) But at this moment in time, my firewall mitigates a Dos attack beautifully (because that's quite easy) and I am currently implementing a solution to handle
DDos attacks (spoofed ips by hping3). I am also working on the physical architecture of the project. My next attempt is going to be with 2 laptops connected with a single ethernet cable. If 
that goes well, I may try to make the laptops go through a small travel router. That way I can scale the legitimate users very easily!
