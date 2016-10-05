#include "DistributedSTOServer.hh"

int main(int argc, char **argv) {
  DistributedSTOServer server(9090);
  server.serve();
  return 0;
}

