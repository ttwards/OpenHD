#include "ARESTelemetry.h"

#include "AirTelemetry.h"
#include "GroundTelemetry.h"

ARESTelemetry::ARESTelemetry(OHDProfile profile1, bool enableExtendedLogging)
    : ares_profile(profile1), ares_enableExtendedLogging(enableExtendedLogging) {
    ares_console = openhd::log::create_or_get("ares_tele");
	assert(ares_console);
	ares_console->info("ARESTelemetry initialized");

    ares_udp_forwarder = std::make_unique<SocketHelper::UDPMultiForwarder>();
    ares_udp_receiver = std::make_unique<SocketHelper::UDPReceiver>(
        SocketHelper::ADDRESS_ANY, 
 		ARES_UDP_RX_RADIO_PORT,
      [this](const uint8_t *payload, const std::size_t payloadSize) {
		if(link_handle && payloadSize > 0) {
            auto data_vec = std::make_shared<std::vector<uint8_t>>(payload, payload + payloadSize);
			link_handle->transmit_ares_data({data_vec}); // n_injections defaults to 1
		}
      });
	add_udp_forwarder("127.0.0.1", ARES_UDP_TX_RADIO_PORT);
    ares_udp_receiver->runInBackground();
	ares_loop_thread = std::make_unique<std::thread>([this]() {
    while (!ares_loop_thread_terminate) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  });
}

ARESTelemetry::~ARESTelemetry() {
    ares_loop_thread_terminate = true;
    ares_loop_thread->join();
}

void ARESTelemetry::add_udp_forwarder(std::string client_addr, int client_udp_port) {
  ares_udp_forwarder->addForwarder(client_addr, client_udp_port);
}

void ARESTelemetry::set_link_handle(std::shared_ptr<OHDLink> link) {
  link_handle = std::move(link);
  link_handle->register_on_receive_ares_data_cb([this](std::shared_ptr<std::vector<uint8_t>> data) {
		if(ares_udp_forwarder && data->size() > 0) {
			ares_udp_forwarder->forwardPacketViaUDP(data->data(), data->size());
		}
	});
}