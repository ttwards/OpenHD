
#include "openhd_udp.h"
#include "HelperSources/SocketHelper.hpp"
#include <atomic>
#include <thread>
#include <chrono>
#include <iostream>
#include <string>
#include <iomanip>

constexpr static int ARES_UDP_RX_RADIO_PORT = 24550;
constexpr static int ARES_UDP_TX_RADIO_PORT = 24551;

std::atomic<long long> packets_sent_count{0};
std::atomic<long long> packets_successfully_parsed_count{0};
std::atomic<long long> packets_lost_in_gaps{0};
std::atomic<long long> highest_sequence_received{-1};
std::atomic<long long> parsing_error_count{0};
std::atomic<bool> first_packet_processed_flag{false};

int main() {
    auto ares_udp_forwarder = std::make_unique<SocketHelper::UDPMultiForwarder>();
    long long last_received_sequence_number_for_callback = -1;

    auto ares_udp_receiver = std::make_unique<SocketHelper::UDPReceiver>(
        SocketHelper::ADDRESS_ANY, 
 		ARES_UDP_TX_RADIO_PORT,
      [&last_received_sequence_number_for_callback](const uint8_t *payload, const std::size_t payloadSize) {
		if(payloadSize > 0) {
			std::string received_str(reinterpret_cast<const char*>(payload), payloadSize);
			try {
				long long current_sequence_number = std::stoll(received_str);
				packets_successfully_parsed_count++;

				long long current_max_rx = highest_sequence_received.load(std::memory_order_relaxed);
				while(current_sequence_number > current_max_rx) {
					if(highest_sequence_received.compare_exchange_weak(current_max_rx, current_sequence_number, std::memory_order_release, std::memory_order_relaxed)) {
						break;
					}
				}

				if (!first_packet_processed_flag.load(std::memory_order_acquire)) {
					if (current_sequence_number > 0) {
						packets_lost_in_gaps += current_sequence_number;
					}
					first_packet_processed_flag.store(true, std::memory_order_release);
				} else {
					if (last_received_sequence_number_for_callback != -1 && current_sequence_number > last_received_sequence_number_for_callback + 1) {
						long long missed_count = current_sequence_number - (last_received_sequence_number_for_callback + 1);
						packets_lost_in_gaps += missed_count;
					}
				}
				last_received_sequence_number_for_callback = current_sequence_number;

			} catch (const std::invalid_argument& ia) {
				parsing_error_count++;
			} catch (const std::out_of_range& oor) {
				parsing_error_count++;
			}
		}
      });

	ares_udp_forwarder->addForwarder("127.0.0.1", ARES_UDP_TX_RADIO_PORT);
    ares_udp_receiver->runInBackground();

    std::thread stats_thread([]() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));

            if (!first_packet_processed_flag.load(std::memory_order_relaxed)) {
                std::cout << "[STATS @ 200ms] Waiting for the first packet..." << std::endl;
                continue;
            }

            long long current_total_sent_pkts = packets_sent_count.load(std::memory_order_relaxed);
            long long current_successfully_parsed_pkts = packets_successfully_parsed_count.load(std::memory_order_relaxed);
            long long current_parsing_errors = parsing_error_count.load(std::memory_order_relaxed);
            long long current_total_lost_in_gaps = packets_lost_in_gaps.load(std::memory_order_relaxed);
            long long current_highest_rx_seq = highest_sequence_received.load(std::memory_order_relaxed);

            std::cout << "[STATS @ 200ms] Sent: " << current_total_sent_pkts
                      << ", Parsed OK: " << current_successfully_parsed_pkts
                      << ", ParseErr: " << current_parsing_errors
                      << ", Lost(Gaps): " << current_total_lost_in_gaps;

            long long total_payloads_processed_by_lambda = current_successfully_parsed_pkts + current_parsing_errors;
            if (total_payloads_processed_by_lambda > 0) {
                double parsing_error_rate = (static_cast<double>(current_parsing_errors) / total_payloads_processed_by_lambda) * 100.0;
                std::cout << std::fixed << std::setprecision(2)
                          << ", ErrRate(Parse): " << parsing_error_rate << "%";
            } else {
                std::cout << ", ErrRate(Parse): N/A";
            }

            if (current_highest_rx_seq >= 0) {
                long long sequence_span = current_highest_rx_seq + 1;
                if (sequence_span > 0) {
                    double loss_rate_gaps = (static_cast<double>(current_total_lost_in_gaps) / sequence_span) * 100.0;
                    std::cout << ", LossRate(Gaps/SeqSpan): " << loss_rate_gaps << "%";
                }
            } else {
                std::cout << ", LossRate(Gaps/SeqSpan): N/A";
            }
            
            if (current_total_sent_pkts > 0) {
                long long apparent_lost_total = current_total_sent_pkts - current_successfully_parsed_pkts;
                if (apparent_lost_total < 0) apparent_lost_total = 0; 
                double apparent_loss_rate_total = (static_cast<double>(apparent_lost_total) / current_total_sent_pkts) * 100.0;
                std::cout << ", ApparentLoss(Overall): " << apparent_loss_rate_total << "%";
            }
            std::cout << std::endl;
        }
    });
    stats_thread.detach();

	long long current_sequence_to_send = 0;
	while(true) {
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		std::string payload_str = std::to_string(current_sequence_to_send);
		ares_udp_forwarder->forwardPacketViaUDP((uint8_t*)payload_str.c_str(), payload_str.size());
		packets_sent_count++;
		current_sequence_to_send++;
	}
}