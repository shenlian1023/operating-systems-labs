#!/bin/bash
sudo rm -f /dev/mqueue/lab1_ipc_mq
sudo rm -f /dev/shm/lab1_ipc_shm
sudo rm -f /dev/shm/sem.lab1_sender_sem
sudo rm -f /dev/shm/sem.lab1_receiver_sem
echo "✅ IPC resources cleaned."
