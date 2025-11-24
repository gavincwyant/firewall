#!/bin/bash

#exit if command fails
set -e

QUEUE_NUM=0
RULE="INPUT 1 -j NFQUEUE --queue-num $QUEUE_NUM"

#RULE="INPUT -p tcp ! --dport 22 -j NFQUEUE --queue-num $QUEUE_NUM" #all tcp traffic EXCEPT port 22 (ssh because we need it lol)
#RULE2="INPUT -p udp -j NFQUEUE --queue-num $QUEUE_NUM" #all udp traffic

cleanup() {
    echo "Deleting NFQUEUE rules"
    sudo iptables -D $RULE 2>/dev/null || true #we don't want output from this, send to /dev/null
    #sudo iptables -D $RULE2 2>/dev/null || true
}

trap cleanup EXIT #run cleanup on exit!

echo "Inserting NFQUEUE rules"
if ! sudo iptables -C $RULE 2>/dev/null; then #only insert if not already present!
    sudo iptables -I $RULE
    #sudo iptables -I $RULE2
fi

echo "| Building Firewall |"
#-v for verbose
sudo ./fire_wall

echo "_ Tearing down Firewall _"
