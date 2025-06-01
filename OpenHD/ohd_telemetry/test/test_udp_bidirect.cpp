#include "openhd_udp.h"
#include "HelperSources/SocketHelper.hpp"

constexpr static int ARES_UDP_RX_RADIO_PORT = 24550;
constexpr static int ARES_UDP_TX_RADIO_PORT = 24551;

int main() {
    auto ares_udp_forwarder = std::make_unique<SocketHelper::UDPMultiForwarder>();
    auto ares_udp_receiver = std::make_unique<SocketHelper::UDPReceiver>(
        SocketHelper::ADDRESS_ANY, 
 		ARES_UDP_TX_RADIO_PORT,
      [](const uint8_t *payload, const std::size_t payloadSize) {
		if(payloadSize > 0) {
            std::cout << "Received " << payloadSize << " bytes" << std::endl;
			std::cout << "Payload: " << std::string(payload, payload + payloadSize) << std::endl;
		}
      });

	ares_udp_forwarder->addForwarder("127.0.0.1", ARES_UDP_RX_RADIO_PORT);
    ares_udp_receiver->runInBackground();

	int i = 0;
	while(true) {
		std::this_thread::sleep_for(std::chrono::seconds(1));
		i++;
		std::string payload = "Hello, world!" + std::to_string(i);
		ares_udp_forwarder->forwardPacketViaUDP((uint8_t*)payload.c_str(), payload.size());		
	}
}