#ifndef OPENHD_ARESTELEMETRY_H
#define OPENHD_ARESTELEMETRY_H

#include <memory>
#include <thread>
#include <utility>

#include "openhd_udp.h"
#include "openhd_action_handler.h"
#include "openhd_external_device.h"
#include "openhd_link.hpp"
#include "openhd_link_statistics.hpp"
#include "HelperSources/SocketHelper.hpp"

class ARESTelemetry {
public:
	constexpr static int ARES_UDP_RX_RADIO_PORT = 14550;
	constexpr static int ARES_UDP_TX_RADIO_PORT = 14551;
	ARESTelemetry(OHDProfile profile1, bool enableExtendedLogging = false);
	~ARESTelemetry();
  // OHDTelemetry is agnostic of the type of transmission between air and ground
  // and also agnostic weather this link exists or not (since it is already
  // using a lossy link).
  void add_udp_forwarder(std::string client_addr, int client_udp_port);
  void add_udp_listener(int udp_port);
  void set_link_handle(std::shared_ptr<OHDLink> link);
    [[nodiscard]] std::string createDebug() const;

private:
  // only either one of them both is active at a time.
  // active when air
  std::unique_ptr<SocketHelper::UDPReceiver> ares_udp_receiver;
  // active when ground
  std::unique_ptr<SocketHelper::UDPMultiForwarder> ares_udp_forwarder;
  // Main telemetry thread. Note that the endpoints also might have their own
  // Receive threads
  std::unique_ptr<std::thread> ares_loop_thread;
  bool ares_loop_thread_terminate = false;
  const bool ares_enableExtendedLogging;
  const OHDProfile ares_profile;
  std::shared_ptr<spdlog::logger> ares_console;
  std::shared_ptr<OHDLink> link_handle;
};

#endif // OPENHD_ARESTELEMETRY_H