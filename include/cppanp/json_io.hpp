#pragma once

#include <memory>
#include <stdexcept>
#include <string>

#include "cppanp/network.hpp"

namespace cppanp {

class JsonIoError : public std::runtime_error {
public:
  explicit JsonIoError(const std::string& message)
      : std::runtime_error(message) {}
};

// Serialize a network (including nested subnetworks and layout hints) to JSON.
[[nodiscard]] std::string network_to_json(const AnpNetwork& network);

// Parse JSON produced by network_to_json (or compatible version-1 documents).
[[nodiscard]] std::unique_ptr<AnpNetwork> network_from_json(
    const std::string& json_text);

void save_network_file(const AnpNetwork& network, const std::string& path);
[[nodiscard]] std::unique_ptr<AnpNetwork> load_network_file(
    const std::string& path);

}  // namespace cppanp
