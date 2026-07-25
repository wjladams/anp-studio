#pragma once

#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "cppanp/limit_matrix.hpp"
#include "cppanp/matrix.hpp"
#include "cppanp/pairwise.hpp"
#include "cppanp/synthesis.hpp"

namespace cppanp {

class AnpNetwork;
class AnpCluster;

class AnpNode {
public:
  AnpNode(AnpNetwork* network, AnpCluster* cluster, std::string name);

  [[nodiscard]] const std::string& name() const noexcept { return name_; }
  [[nodiscard]] const std::string& description() const noexcept {
    return description_;
  }
  void set_description(std::string description) {
    description_ = std::move(description);
  }

  [[nodiscard]] AnpCluster* cluster() const noexcept { return cluster_; }
  [[nodiscard]] AnpNetwork* network() const noexcept { return network_; }

  [[nodiscard]] bool invert() const noexcept { return invert_; }
  void set_invert(bool value) { invert_ = value; }

  void connect_to(AnpNode* dest);
  void disconnect_from(AnpNode* dest);
  [[nodiscard]] bool is_connected_to(const AnpNode* dest) const;
  [[nodiscard]] bool is_connected_to_cluster(const std::string& cluster_name) const;

  [[nodiscard]] PairwiseJudgments* node_pairwise(const std::string& dest_cluster);
  [[nodiscard]] const PairwiseJudgments* node_pairwise(
      const std::string& dest_cluster) const;

  // Column of the unscaled supermatrix for this node (indexed by global node order).
  [[nodiscard]] Vector unscaled_column() const;

  [[nodiscard]] AnpNetwork* subnetwork() const noexcept {
    return subnetwork_.get();
  }
  [[nodiscard]] bool has_subnetwork() const noexcept {
    return subnetwork_ != nullptr;
  }
  AnpNetwork& ensure_subnetwork();

private:
  friend class AnpNetwork;
  friend class AnpCluster;

  AnpNetwork* network_ = nullptr;
  AnpCluster* cluster_ = nullptr;
  std::string name_;
  std::string description_;
  bool invert_ = false;
  // Destination cluster name -> pairwise over connected nodes in that cluster.
  std::map<std::string, PairwiseJudgments> node_prioritizers_;
  std::unique_ptr<AnpNetwork> subnetwork_;
};

class AnpCluster {
public:
  AnpCluster(AnpNetwork* network, std::string name);

  [[nodiscard]] const std::string& name() const noexcept { return name_; }
  [[nodiscard]] const std::string& description() const noexcept {
    return description_;
  }
  void set_description(std::string description) {
    description_ = std::move(description);
  }

  [[nodiscard]] AnpNetwork* network() const noexcept { return network_; }
  [[nodiscard]] std::size_t nnodes() const noexcept { return nodes_.size(); }

  AnpNode& add_node(const std::string& name);
  [[nodiscard]] AnpNode* find_node(const std::string& name);
  [[nodiscard]] const AnpNode* find_node(const std::string& name) const;
  [[nodiscard]] AnpNode& node(const std::string& name);
  [[nodiscard]] const AnpNode& node(const std::string& name) const;

  [[nodiscard]] std::vector<std::string> node_names() const;
  [[nodiscard]] std::vector<AnpNode*> nodes();
  [[nodiscard]] std::vector<const AnpNode*> nodes() const;

  void cluster_connect(AnpCluster* dest);
  [[nodiscard]] PairwiseJudgments& cluster_pairwise() { return prioritizer_; }
  [[nodiscard]] const PairwiseJudgments& cluster_pairwise() const {
    return prioritizer_;
  }

private:
  friend class AnpNetwork;
  friend class AnpNode;

  AnpNetwork* network_ = nullptr;
  std::string name_;
  std::string description_;
  std::vector<std::unique_ptr<AnpNode>> nodes_;
  PairwiseJudgments prioritizer_;
};

class AnpNetwork {
public:
  static constexpr const char* kDefaultAlternativesCluster = "Alternatives";

  explicit AnpNetwork(bool create_alts_cluster = true);

  AnpNetwork(const AnpNetwork&) = delete;
  AnpNetwork& operator=(const AnpNetwork&) = delete;
  AnpNetwork(AnpNetwork&&) noexcept = default;
  AnpNetwork& operator=(AnpNetwork&&) noexcept = default;

  AnpCluster& add_cluster(const std::string& name);
  [[nodiscard]] AnpCluster* find_cluster(const std::string& name);
  [[nodiscard]] const AnpCluster* find_cluster(const std::string& name) const;
  [[nodiscard]] AnpCluster& cluster(const std::string& name);
  [[nodiscard]] const AnpCluster& cluster(const std::string& name) const;

  [[nodiscard]] AnpCluster* alternatives_cluster() const noexcept {
    return alts_cluster_;
  }
  void set_alternatives_cluster(const std::string& name);

  AnpNode& add_node(const std::string& cluster_name, const std::string& node_name);
  [[nodiscard]] AnpNode* find_node(const std::string& name);
  [[nodiscard]] const AnpNode* find_node(const std::string& name) const;
  [[nodiscard]] AnpNode& node(const std::string& name);
  [[nodiscard]] const AnpNode& node(const std::string& name) const;

  void node_connect(const std::string& src, const std::string& dest);
  void node_disconnect(const std::string& src, const std::string& dest);
  void set_node_comparison(const std::string& wrt_node,
                           const std::string& a,
                           const std::string& b,
                           double value);
  void set_cluster_comparison(const std::string& wrt_cluster,
                              const std::string& a,
                              const std::string& b,
                              double value);

  void remove_node(const std::string& name);
  void remove_cluster(const std::string& name);
  void clear_subnetwork(const std::string& node_name);

  [[nodiscard]] const SynthesisOptions& synthesis_options() const noexcept {
    return synthesis_;
  }
  void set_synthesis_options(SynthesisOptions options) {
    synthesis_ = std::move(options);
  }

  // Optional canvas layout hints (persisted in JSON; ignored by calculations).
  void set_cluster_position(const std::string& name, double x, double y);
  void set_node_position(const std::string& name, double x, double y);
  [[nodiscard]] bool cluster_position(const std::string& name,
                                      double& x,
                                      double& y) const;
  [[nodiscard]] bool node_position(const std::string& name,
                                   double& x,
                                   double& y) const;

  [[nodiscard]] std::size_t nnodes() const;
  [[nodiscard]] std::size_t nclusters() const noexcept {
    return clusters_.size();
  }
  [[nodiscard]] std::vector<std::string> cluster_names() const;
  [[nodiscard]] std::vector<std::string> node_names() const;
  [[nodiscard]] std::vector<AnpCluster*> clusters();
  [[nodiscard]] std::vector<const AnpCluster*> clusters() const;
  [[nodiscard]] std::vector<AnpNode*> nodes();
  [[nodiscard]] std::vector<const AnpNode*> nodes() const;

  [[nodiscard]] Matrix unscaled_supermatrix() const;
  [[nodiscard]] Matrix cluster_weight_matrix() const;
  [[nodiscard]] Matrix scaled_supermatrix() const;
  [[nodiscard]] Matrix limit_matrix(
      const LimitMatrixOptions& options = {}) const;
  [[nodiscard]] Vector global_priority(
      const LimitMatrixOptions& options = {}) const;

  [[nodiscard]] bool has_subnet() const;
  AnpNetwork& subnet(const std::string& node_name);
  [[nodiscard]] std::vector<std::string> alt_names() const;
  // Alternative scores (flat or synthesized from subnetworks).
  [[nodiscard]] Vector priority(const LimitMatrixOptions& options = {}) const;
  // Alternative scores keyed in the same order as alt_names().
  [[nodiscard]] std::map<std::string, double> priority_map(
      const LimitMatrixOptions& options = {}) const;

private:
  friend class AnpNode;
  friend class AnpCluster;

  void register_node(AnpNode* node);
  void unregister_node(const std::string& name);
  [[nodiscard]] std::size_t node_index(const std::string& name) const;

  [[nodiscard]] Vector subnet_synthesize(
      const LimitMatrixOptions& options) const;

  std::vector<std::unique_ptr<AnpCluster>> clusters_;
  AnpCluster* alts_cluster_ = nullptr;
  std::unordered_map<std::string, AnpNode*> node_index_;
  SynthesisOptions synthesis_;
  std::map<std::string, std::pair<double, double>> cluster_positions_;
  std::map<std::string, std::pair<double, double>> node_positions_;
};

}  // namespace cppanp
