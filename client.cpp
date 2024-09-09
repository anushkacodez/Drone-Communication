#include <iostream>
#include <fstream>
#include <thread>
#include <atomic>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "encryption.h"

#define SERVER_IP "127.0.0.1"  // Server is assumed to be on localhost
#define PORT_CONTROL 8080
#define PORT_TELEMETRY 8081
#define PORT_FILE_TRANSFER 8082  // Port for file transfers using TCP
#define BUFFER_SIZE 1024
#define XOR_KEY "mysecretkey"

// Drone information (x, y, speed)
int x = 0, y = 0, speed = 0;

std::atomic<bool> receiving_control_commands(false);  // Use atomic for thread-safe flag
std::atomic<bool> sending_telemetry(false);  // Control flag for telemetry

// Mode 1: Receiving control commands from the server using UDP
void receive_control_command(int sock) {
    char buffer[BUFFER_SIZE];
    struct sockaddr_in server_addr;
    socklen_t addr_len = sizeof(server_addr);

    std::cout << "Listening for control commands on the control port...\n";

    // Loop to continuously receive control commands while flag is true
    while (receiving_control_commands) {
        memset(buffer, 0, sizeof(buffer));  // Clear the buffer
        int len = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr*)&server_addr, &addr_len);

        if (len > 0) {
            std::string command = xor_encrypt_decrypt(std::string(buffer, len), XOR_KEY);
            std::cout << "Received control command: " << command << std::endl;

            // Parse the received command and update x, y, and speed
            sscanf(command.c_str(), "x=%d y=%d speed=%d", &x, &y, &speed);
            std::cout << "Updated drone position: (" << x << ", " << y << "), speed: " << speed << "\n";
        }else if (len <0) {
            std::cerr << "Error receiving control commands. Errno: " << errno << "\n";
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::cout << "Stopped receiving control commands.\n";
}

// Mode 2: Automatic telemetry sending function using TCP
void send_telemetry_data(int sock, int control_sock) {
    // std::cout<<"sock= "<<sock<<'\n';
    while (sending_telemetry) {
        // Create telemetry data
        std::string telemetry ="control_port=" + std::to_string(control_sock)+ " Location: x=" + std::to_string(x) + " y=" + std::to_string(y) + " speed=" + std::to_string(speed);
        std::string encrypted_telemetry = xor_encrypt_decrypt(telemetry, XOR_KEY);

        // Send telemetry data to the server using the same TCP connection
        send(sock, encrypted_telemetry.c_str(), encrypted_telemetry.size(), 0);
        std::cout << "Sent telemetry data: " << telemetry << std::endl;

        // Sleep for 10 seconds before sending the next telemetry
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }
    std::cout << "Telemetry stopped.\n";
}

// Mode 3: File transfer using TCP
void send_file(const std::string& file_name) {
    int sock;
    struct sockaddr_in server_addr;

    // Create TCP socket for file transfer
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        std::cerr << "Error creating file transfer socket.\n";
        return;
    }

    // Set up server address for file transfer
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT_FILE_TRANSFER);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

    // Connect to the server
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Error connecting to the server for file transfer.\n";
        close(sock);
        return;
    }

    std::ifstream file(file_name, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Unable to open file: " << file_name << std::endl;
        close(sock);
        return;
    }

    char buffer[BUFFER_SIZE];
    while (!file.eof()) {
        file.read(buffer, BUFFER_SIZE);
        std::streamsize bytes_read = file.gcount();

        if (bytes_read > 0) {
            // Send the file chunk to the server
            send(sock, buffer, bytes_read, 0);
        }
    }

    std::cout << "File transfer completed: " << file_name << std::endl;
    file.close();
    close(sock);  // Close the socket after sending the file
}

int main() {
    srand(time(0));
    int control_sock, telemetry_sock;
    struct sockaddr_in server_addr, client_addr;

    // Create UDP socket for control commands
    control_sock = socket(AF_INET, SOCK_DGRAM, 0);

    // Create TCP socket for telemetry
    telemetry_sock = socket(AF_INET, SOCK_STREAM, 0);
    std::cout<<"telemetry sock "<<telemetry_sock<<std::endl;

    // Set up client address with a unique port (assigning random unique port in range 10000-20000)
    int client_port = 10000 + rand() % 10000;  // Random unique port
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(client_port);  // Bind to a unique control port
    client_addr.sin_addr.s_addr = INADDR_ANY;

    // Bind the client to the unique port for control commands (UDP)
    if (bind(control_sock, (struct sockaddr*)&client_addr, sizeof(client_addr)) < 0) {
        std::cerr << "Error binding control socket on port " << client_port << std::endl;
        return -1;
    }

    // Set up server address for telemetry (TCP)
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    server_addr.sin_port = htons(PORT_TELEMETRY);  // Telemetry data will be sent to this port

    // Connect to the server for telemetry data (TCP)
    if (connect(telemetry_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Error connecting to server for telemetry data (TCP).\n";
        close(telemetry_sock);
        return -1;
    }

    std::cout << "Drone client started. Listening for control commands on UDP port: " << client_port << "\n";

    std::thread control_thread;  // Declare thread for receiving commands (UDP)
    std::thread telemetry_thread;  // Thread for automatic telemetry (TCP)

    // Main loop for mode selection
    while (true) {
        int mode;
        std::cout << "\nSelect Mode:\n";
        std::cout << "1. Start/Stop Receiving Control Commands (UDP)\n";
        std::cout << "2. Start/Stop Sending Telemetry Data (TCP)\n";
        std::cout << "3. Send File Transfer (TCP)\n";
        std::cout << "4. Exit\n";
        std::cout << "Enter your choice: ";
        std::cin >> mode;

        if (mode == 1) {
            if (!receiving_control_commands) {
                receiving_control_commands = true;
                control_thread = std::thread(receive_control_command, control_sock);  // Start receiving in a thread
                std::cout << "Started receiving control commands.\n";
            } else {
                receiving_control_commands = false;  // Set flag to stop the loop
                if (control_thread.joinable()) control_thread.join();  // Wait for thread to finish
                std::cout << "Stopped receiving control commands.\n";
            }
        } else if (mode == 2) {
            if (!sending_telemetry) {
                sending_telemetry = true;
                telemetry_thread = std::thread(send_telemetry_data, telemetry_sock, client_port);  // Start telemetry in a thread
                std::cout << "Started sending telemetry data.\n";
            } else {
                sending_telemetry = false;  // Set flag to stop telemetry
                if (telemetry_thread.joinable()) telemetry_thread.join();  // Wait for thread to finish
                std::cout << "Stopped sending telemetry data.\n";
            }
        } else if (mode == 3) {
            std::string file_name;
            std::cout << "Enter the file name to send: ";
            std::cin >> file_name;
            send_file(file_name);  // Send file to the server using TCP
        } else if (mode == 4) {
            receiving_control_commands = false;  // Stop receiving if active
            sending_telemetry = false;  // Stop sending telemetry if active
            if (control_thread.joinable()) control_thread.join();  // Ensure the thread finishes
            if (telemetry_thread.joinable()) telemetry_thread.join();  // Ensure telemetry thread finishes
            std::cout << "Exiting drone client.\n";
            break;  // Exit the program
        } else {
            std::cout << "Invalid option. Please select a valid mode.\n";
        }
    }

    // Close sockets before exiting
    close(control_sock);
    close(telemetry_sock);

    return 0;
}
