#pragma once

#include <string>
#include "stats_common.h"

// serves over unix socket
class stats_server {
public:
  stats_server(const std::string &sockfile);
  void serve_forever(); // blocks current thread
private:
  bool handle_cmd_get_counter_value(const std::string &name, packet &pkt);
  void serve_client(int fd);
  std::string sockfile_;
  bool handle_persist_latency(packet &pkt);
  bool handle_throughput(packet &pkt);
  bool handle_ext4_delayed_allocation_blocks(packet &pkt, const std::string &devname);
};

