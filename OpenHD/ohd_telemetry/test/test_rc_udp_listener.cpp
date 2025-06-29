#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <string>
#include <csignal>

#include "HelperSources/SocketHelper.hpp"
#include "nlohmann/json.hpp"

// Shared state between receiver callback and main thread
std::atomic<int> g_packet_count(0);
std::mutex g_latest_message_mutex;
std::string g_latest_message;
std::atomic<bool> g_terminate(false);

void signal_handler(int signal) {
    if (signal == SIGINT) {
        std::cout << "Caught SIGINT, exiting..." << std::endl;
        g_terminate = true;
    }
}

// This callback is executed by the UDPReceiver's internal thread
void on_udp_receive(const uint8_t *payload, const std::size_t payloadSize) {
    g_packet_count++;
    std::lock_guard<std::mutex> lock(g_latest_message_mutex);
    g_latest_message.assign(reinterpret_cast<const char*>(payload), payloadSize);
}

int main() {
    std::cout << "Starting RC UDP listener on 127.0.0.1:24660" << std::endl;

    // Handle Ctrl+C
    signal(SIGINT, signal_handler);

    // Create and run the UDP receiver in the background
    auto receiver = std::make_unique<SocketHelper::UDPReceiver>(
        "127.0.0.1", 24660, on_udp_receive);
    receiver->runInBackground();

    auto last_check_time = std::chrono::steady_clock::now();

    while (!g_terminate) {
        // Sleep for 100ms
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        auto current_time = std::chrono::steady_clock::now();
        auto elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - last_check_time);
        
        int packets = g_packet_count.exchange(0);
        float frequency = 0.0f;
        if (elapsed_time.count() > 0) {
            frequency = static_cast<float>(packets) / (static_cast<float>(elapsed_time.count()) / 1000.0f);
        }

        std::string message_to_print;
        {
            std::lock_guard<std::mutex> lock(g_latest_message_mutex);
            message_to_print = g_latest_message;
        }

        // Pretty print JSON if it's not empty
        std::string output;
        if (!message_to_print.empty()) {
            try {
                nlohmann::json j = nlohmann::json::parse(message_to_print);
                output = j.dump(2);
            } catch (const nlohmann::json::parse_error& e) {
                output = "Invalid JSON: " + message_to_print;
            }
        } else {
            output = "No data received yet.";
        }

        // Clear console and print
        // std::cout << "\033[2J\033[1;1H"; // This clears the console screen
        std::cout << "Listening on UDP 127.0.0.1:24660..." << std::endl;
        std::cout << "Receiving at: " << std::fixed << std::setprecision(2) << frequency << " Hz" << std::endl;
        std::cout << "Last message:" << std::endl;
        std::cout << output << std::endl;
        std::cout << "-------------------------------------" << std::endl;
        std::cout << "Press Ctrl+C to exit." << std::endl;


        last_check_time = current_time;
    }

    receiver->stopBackground();
    std::cout << "Listener stopped." << std::endl;

    return 0;
} 