Drone Communication System
This project implements a drone communication system where drones and a central server exchange control commands, telemetry data, and files. The system includes three primary modes of operation: receiving control commands via UDP, sending telemetry data via TCP, and transferring files via TCP.

Features
Control Commands (UDP): Drones receive encrypted control commands (x, y coordinates and speed) from the server.
Telemetry Data (TCP): Drones send encrypted telemetry data (x, y coordinates and speed) to the server at regular intervals.
File Transfer (TCP): Drones can transfer files (e.g., images, videos) to the server.
Requirements
C++17
Sockets (POSIX)
Multithreading (std::thread)
Atomic (std::atomic)

Libraries:
<iostream>
<fstream>
<thread>
<atomic>
<cstring>
<sys/socket.h>
<netinet/in.h>
<arpa/inet.h>
<unistd.h>

How It Works
1. Client (Drone)
The drone has three main functionalities:

Mode 1: Control Command (UDP)
Drones receive control commands from the server to update their x, y coordinates and speed. This communication happens over UDP, and the messages are XOR-encrypted.

Mode 2: Telemetry Data (TCP)
Drones periodically send telemetry data to the server. This data includes the drone's control port, x and y positions, and speed, encrypted using XOR encryption.

Mode 3: File Transfer (TCP)
Drones can transfer a file (e.g., logs, images) to the server using TCP.

2. Server
The server manages multiple drones, receives their telemetry data, sends control commands, and accepts file transfers.

Mode 1: List Connected Drones
The server displays a list of all connected drones and their details.

Mode 2: Receive Telemetry (TCP)
The server listens for telemetry data from multiple drones on separate threads, decrypts the data, and updates the list of connected drones.

Mode 3: Send Control Commands (UDP)
The server sends encrypted control commands to specific drones, updating their x, y positions and speed.

Mode 4: Receive File Transfer (TCP)
The server accepts files transferred from drones and saves them locally.

How to run the project:
Run the following command in the terminal containing the code and the makefile
```
make clean
make
```

To start the server & client (run in separate terminals)
```
./server
./client
```
