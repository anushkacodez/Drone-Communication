#include <iostream>
#include <fstream>
#include <thread>
#include <unordered_map>
#include <atomic>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "encryption.h"
#include <ctime>
#include <sstream>

#define PORT_CONTROL 8080
#define PORT_TELEMETRY 8081
#define PORT_FILE_TRANSFER 8082  // Port for file transfers using TCP
#define BUFFER_SIZE 1024
#define XOR_KEY "mysecretkey"

// Data structure to store information about each drone
struct Drone {
    int control_port;  // Drone's control port for receiving commands
    std::string ip;    // Drone's IP address
    int x;             // x position
    int y;             // y position
    int speed;         // speed
};

// Global map to store connected drones (key: telemetry port, value: Drone)
std::unordered_map<int, Drone> connected_drones;

// Global flags to control telemetry and control command threads
std::atomic<bool> receiving_telemetry(false);
std::atomic<bool> sending_control_commands(false);

// Mode 1: List connected drones
void list_connected_drones() {
    if (connected_drones.empty()) {
        std::cout << "No drones connected.\n";
        return;
    }
    std::cout << "Connected drones: \n";
    for (const auto& [telemetry_port, drone] : connected_drones) {
        std::cout << "Drone with telemetry port " << telemetry_port << " (Control port: " << drone.control_port << ", IP: " << drone.ip 
                  << ") - Position: (" << drone.x << ", " << drone.y << "), Speed: " << drone.speed << "\n";
    }
}

// Mode 2: Handling telemetry data from a single drone (using TCP)
void handle_single_drone_telemetry(int client_sock, std::string client_ip, int telemetry_port) {
    char buffer[BUFFER_SIZE];
    
    std::cout << "Handling telemetry for drone from IP: " << client_ip << ", Telemetry port: " << telemetry_port << "\n";

    while (receiving_telemetry) {
        int len = recv(client_sock, buffer, sizeof(buffer), 0);
        if (len > 0) {
            std::string telemetry = xor_encrypt_decrypt(std::string(buffer, len), XOR_KEY);
            std::cout<<"Telemetry jo v "<<telemetry<<'\n';
            // Extract the control port from the telemetry data
            int control_port;
            sscanf(telemetry.c_str(), "control_port=%d", &control_port);

            // If the drone is new, add it to the list of connected drones
            if (connected_drones.find(telemetry_port) == connected_drones.end()) {
                connected_drones[telemetry_port] = {control_port, client_ip, 0, 0, 0};
                std::cout << "New drone connected from IP: " << client_ip << ", Telemetry port: " << telemetry_port << ", Control port: " << control_port << "\n";
            }

            std::cout << "Received telemetry from drone on telemetry port " << telemetry_port << ": " << telemetry << "\n";
        } else {
            std::cerr << "Error receiving telemetry data or connection closed by client.\n";
            break;
        }
    }

    std::cout << "Stopped receiving telemetry data from drone on telemetry port " << telemetry_port << ".\n";
    close(client_sock);  // Close the client socket when done
}

// Mode 2: Accept connections from multiple drones and handle each in a separate thread
void accept_telemetry_connections(int telemetry_sock) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    while (receiving_telemetry) {
        // Accept incoming telemetry connections from drones
        int client_sock = accept(telemetry_sock, (struct sockaddr*)&client_addr, &addr_len);
        if (client_sock < 0) {
            std::cerr << "Error accepting telemetry connection.\n";
            continue;
        }

        // Get the client's IP address and telemetry port
        std::string client_ip = inet_ntoa(client_addr.sin_addr);
        int telemetry_port = ntohs(client_addr.sin_port);

        // Handle the telemetry data from this drone in a separate thread
        std::cout<<"client socket "<<client_sock<<" client_ip "<<client_ip<<" telemetry port "<<telemetry_port<<std::endl;
        std::thread telemetry_thread(handle_single_drone_telemetry, client_sock, client_ip, telemetry_port);
        telemetry_thread.detach();  // Detach the thread to allow multiple drones to connect
    }
}

// Mode 3: Sending control commands to drones (Update x, y, and speed using UDP)
void send_control_command(int sock) {
    if (connected_drones.empty()) {
        std::cout << "No drones are currently connected.\n";
        return;
    }

    int target_telemetry_port;
    std::cout << "Enter the telemetry port of the drone to send command: ";
    std::cin >> target_telemetry_port;

    // Validate port
    if (connected_drones.find(target_telemetry_port) == connected_drones.end()) {
        std::cout << "No drone connected on that telemetry port.\n";
        return;
    }

    // Get control port from connected_drones
    int control_port = connected_drones[target_telemetry_port].control_port;
    std::string drone_ip = connected_drones[target_telemetry_port].ip;

    std::cout << "Sending command to drone at IP: " << drone_ip << " on control port: " << control_port << "\n";

    // std::cout<<"Control port is = "<<control_port<<'\n';
    int x, y, speed;
    std::cout << "Enter new x position: ";
    std::cin >> x;
    std::cout << "Enter new y position: ";
    std::cin >> y;
    std::cout << "Enter new speed: ";
    std::cin >> speed;

    // Prepare the command to send
    std::string command = "x=" + std::to_string(x) + " y=" + std::to_string(y) + " speed=" + std::to_string(speed);
    std::string encrypted_command = xor_encrypt_decrypt(command, XOR_KEY);

    // Send the command to the drone's control port (UDP)
    struct sockaddr_in drone_addr;
    drone_addr.sin_family = AF_INET;
    drone_addr.sin_port = htons(control_port);  // Use the control port
    drone_addr.sin_addr.s_addr = inet_addr(connected_drones[target_telemetry_port].ip.c_str());

    int bytes_sent = sendto(sock, encrypted_command.c_str(), encrypted_command.size(), 0, (struct sockaddr*)&drone_addr, sizeof(drone_addr));

    if (bytes_sent < 0) {
        std::cerr << "Failed to send control command. Error: " << strerror(errno) << "\n";
    } else {
        std::cout << "Sent command to drone with telemetry port " << target_telemetry_port << " on control port " << control_port << ": " << command << "\n";
    }
    
    std::cout << "Sent command to drone with telemetry port " << target_telemetry_port << " on control port " << control_port << ": " << command << "\n";

    // Update drone's position and speed in the server's records
    connected_drones[target_telemetry_port].x = x;
    connected_drones[target_telemetry_port].y = y;
    connected_drones[target_telemetry_port].speed = speed;
}

// Mode 4: Receive file transfer from the drone using TCP
void receive_file() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    // Create TCP socket for file transfer
    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        std::cerr << "Error creating file transfer socket.\n";
        return;
    }

    // Set up server address for file transfer
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT_FILE_TRANSFER);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Bind and listen on the file transfer port
    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Error binding file transfer socket.\n";
        close(server_sock);
        return;
    }

    if (listen(server_sock, 5) < 0) {
        std::cerr << "Error listening on file transfer socket.\n";
        close(server_sock);
        return;
    }

    std::cout << "Waiting for incoming file transfer...\n";

    // Accept incoming file transfer request
    if ((client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &addr_len)) < 0) {
        std::cerr << "Error accepting file transfer connection.\n";
        close(server_sock);
        return;
    }

    std::time_t now = std::time(nullptr);
    std::stringstream ss;
    ss << "client_" << client_sock << "_" << now << ".bin";
    std::string file_name = ss.str();

    std::ofstream file(file_name, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Unable to open file for writing.\n";
        close(client_sock);
        close(server_sock);
        return;
    }

    char buffer[BUFFER_SIZE];
    while (true) {
        int bytes_received = recv(client_sock, buffer, sizeof(buffer), 0);
        if (bytes_received > 0) {
            file.write(buffer, bytes_received);
        } else {
            break;  // File transfer complete
        }
    }

    std::cout << "File transfer completed. File saved as '" << file_name << "'\n";
    file.close();
    close(client_sock);
    close(server_sock);  // Close the sockets
}

int main() {
    int control_sock, telemetry_sock;
    struct sockaddr_in server_addr;

    // Create UDP socket for control commands
    control_sock = socket(AF_INET, SOCK_DGRAM, 0);

    // Create TCP socket for telemetry data
    telemetry_sock = socket(AF_INET, SOCK_STREAM, 0);

    // Set up server addresses
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Bind for control commands (UDP)
    server_addr.sin_port = htons(PORT_CONTROL);
    bind(control_sock, (struct sockaddr*)&server_addr, sizeof(server_addr));

    // Bind for telemetry data (TCP)
    server_addr.sin_port = htons(PORT_TELEMETRY);
    bind(telemetry_sock, (struct sockaddr*)&server_addr, sizeof(server_addr));

    // Start listening for telemetry connections
    listen(telemetry_sock, 5);

    std::thread telemetry_thread;
    std::thread control_thread;
    std::thread file_transfer_thread;

    // Main loop to select modes
    while (true) {
        int mode;
        std::cout << "\nSelect Mode:\n";
        std::cout << "1. List Connected Drones\n";
        std::cout << "2. Start/Stop Receiving Telemetry Data (TCP)\n";
        std::cout << "3. Send Control Commands (UDP)\n";
        std::cout << "4. Receive File Transfer (TCP)\n";
        std::cout << "5. Exit\n";
        std::cout << "Enter your choice: ";
        std::cin >> mode;
        std::cout << '\n';

        if (mode == 1) {
            list_connected_drones();
        } else if (mode == 2) {
            if (!receiving_telemetry) {
                receiving_telemetry = true;
                telemetry_thread = std::thread(accept_telemetry_connections, telemetry_sock);
                std::cout << "Started receiving telemetry data.\n";
            } else {
                receiving_telemetry = false;
                if (telemetry_thread.joinable()) telemetry_thread.join();
            }
        } else if (mode == 3) {
            send_control_command(control_sock);  // Call directly instead of using thread
        } else if (mode == 4) {
            file_transfer_thread = std::thread(receive_file);
            if (file_transfer_thread.joinable()) file_transfer_thread.join();
        } else if (mode == 5) {
            receiving_telemetry = false;
            sending_control_commands = false;
            if (telemetry_thread.joinable()) telemetry_thread.join();
            if (file_transfer_thread.joinable()) file_transfer_thread.join();
            break;  // Exit the program
        } else {
            std::cout << "Invalid option. Please select a valid mode.\n";
        }
    }

    // Close sockets
    close(control_sock);
    close(telemetry_sock);

    return 0;
}
