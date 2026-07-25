#include "cppanp/network.hpp"

#include "cppanp/synthesis.hpp"

#include <stdexcept>
#include <utility>

namespace cppanp {

// ---------------------------------------------------------------------------
// AnpNode
// ---------------------------------------------------------------------------

AnpNode::AnpNode(AnpNetwork* network, AnpCluster* cluster, std::string name)
    : network_(network), cluster_(cluster), name_(std::move(name)) {}

void AnpNode::connect_to(AnpNode* dest) {
  if (dest == nullptr) {
    throw std::invalid_argument("cannot connect to a null node");
  }
  if (dest->network_ != network_) {
    throw std::invalid_argument("cannot connect nodes from different networks");
  }
  PairwiseJudgments& pw = node_prioritizers_[dest->cluster_->name()];
  pw.add_alternative(dest->name_, /*ignore_existing=*/true);
  cluster_->cluster_connect(dest->cluster_);
}

void AnpNode::disconnect_from(AnpNode* dest) {
  if (dest == nullptr) {
    return;
  }
  const auto it = node_prioritizers_.find(dest->cluster_->name());
  if (it == node_prioritizers_.end()) {
    return;
  }
  it->second.remove_alternative(dest->name_, /*ignore_missing=*/true);
  if (it->second.empty()) {
    node_prioritizers_.erase(it);
  }
}

bool AnpNode::is_connected_to(const AnpNode* dest) const {
  if (dest == nullptr) {
    return false;
  }
  const auto it = node_prioritizers_.find(dest->cluster_->name());
  if (it == node_prioritizers_.end()) {
    return false;
  }
  return it->second.has_alternative(dest->name_);
}

bool AnpNode::is_connected_to_cluster(const std::string& cluster_name) const {
  return node_prioritizers_.find(cluster_name) != node_prioritizers_.end();
}

PairwiseJudgments* AnpNode::node_pairwise(const std::string& dest_cluster) {
  const auto it = node_prioritizers_.find(dest_cluster);
  return it == node_prioritizers_.end() ? nullptr : &it->second;
}

const PairwiseJudgments* AnpNode::node_pairwise(
    const std::string& dest_cluster) const {
  const auto it = node_prioritizers_.find(dest_cluster);
  return it == node_prioritizers_.end() ? nullptr : &it->second;
}

Vector AnpNode::unscaled_column() const {
  Vector rval(network_->nnodes(), 0.0);
  const std::vector<std::string> names = network_->node_names();
  for (const auto& [cluster_name, pw] : node_prioritizers_) {
    (void)cluster_name;
    if (pw.empty()) {
      continue;
    }
    const Vector local = pw.priorities();
    for (std::size_t i = 0; i < pw.size(); ++i) {
      const std::size_t gi = network_->node_index(pw.alternatives()[i]);
      rval[gi] = local[i];
    }
  }
  return rval;
}

AnpNetwork& AnpNode::ensure_subnetwork() {
  if (!subnetwork_) {
    subnetwork_ = std::make_unique<AnpNetwork>(/*create_alts_cluster=*/false);
  }
  return *subnetwork_;
}

// ---------------------------------------------------------------------------
// AnpCluster
// ---------------------------------------------------------------------------

AnpCluster::AnpCluster(AnpNetwork* network, std::string name)
    : network_(network), name_(std::move(name)) {}

AnpNode& AnpCluster::add_node(const std::string& name) {
  if (network_->find_node(name) != nullptr) {
    throw std::invalid_argument("node already exists in network: " + name);
  }
  nodes_.push_back(std::make_unique<AnpNode>(network_, this, name));
  AnpNode* node = nodes_.back().get();
  network_->register_node(node);
  return *node;
}

AnpNode* AnpCluster::find_node(const std::string& name) {
  for (auto& node : nodes_) {
    if (node->name() == name) {
      return node.get();
    }
  }
  return nullptr;
}

const AnpNode* AnpCluster::find_node(const std::string& name) const {
  for (const auto& node : nodes_) {
    if (node->name() == name) {
      return node.get();
    }
  }
  return nullptr;
}

AnpNode& AnpCluster::node(const std::string& name) {
  AnpNode* n = find_node(name);
  if (n == nullptr) {
    throw std::invalid_argument("unknown node in cluster: " + name);
  }
  return *n;
}

const AnpNode& AnpCluster::node(const std::string& name) const {
  const AnpNode* n = find_node(name);
  if (n == nullptr) {
    throw std::invalid_argument("unknown node in cluster: " + name);
  }
  return *n;
}

std::vector<std::string> AnpCluster::node_names() const {
  std::vector<std::string> names;
  names.reserve(nodes_.size());
  for (const auto& node : nodes_) {
    names.push_back(node->name());
  }
  return names;
}

std::vector<AnpNode*> AnpCluster::nodes() {
  std::vector<AnpNode*> out;
  out.reserve(nodes_.size());
  for (auto& node : nodes_) {
    out.push_back(node.get());
  }
  return out;
}

std::vector<const AnpNode*> AnpCluster::nodes() const {
  std::vector<const AnpNode*> out;
  out.reserve(nodes_.size());
  for (const auto& node : nodes_) {
    out.push_back(node.get());
  }
  return out;
}

void AnpCluster::cluster_connect(AnpCluster* dest) {
  if (dest == nullptr) {
    throw std::invalid_argument("cannot connect to a null cluster");
  }
  prioritizer_.add_alternative(dest->name(), /*ignore_existing=*/true);
}

// ---------------------------------------------------------------------------
// AnpNetwork
// ---------------------------------------------------------------------------

AnpNetwork::AnpNetwork(bool create_alts_cluster) {
  if (create_alts_cluster) {
    AnpCluster& alts = add_cluster(kDefaultAlternativesCluster);
    alts_cluster_ = &alts;
  }
}

AnpCluster& AnpNetwork::add_cluster(const std::string& name) {
  if (find_cluster(name) != nullptr) {
    throw std::invalid_argument("cluster already exists: " + name);
  }
  clusters_.push_back(std::make_unique<AnpCluster>(this, name));
  return *clusters_.back();
}

AnpCluster* AnpNetwork::find_cluster(const std::string& name) {
  for (auto& c : clusters_) {
    if (c->name() == name) {
      return c.get();
    }
  }
  return nullptr;
}

const AnpCluster* AnpNetwork::find_cluster(const std::string& name) const {
  for (const auto& c : clusters_) {
    if (c->name() == name) {
      return c.get();
    }
  }
  return nullptr;
}

AnpCluster& AnpNetwork::cluster(const std::string& name) {
  AnpCluster* c = find_cluster(name);
  if (c == nullptr) {
    throw std::invalid_argument("unknown cluster: " + name);
  }
  return *c;
}

const AnpCluster& AnpNetwork::cluster(const std::string& name) const {
  const AnpCluster* c = find_cluster(name);
  if (c == nullptr) {
    throw std::invalid_argument("unknown cluster: " + name);
  }
  return *c;
}

void AnpNetwork::set_alternatives_cluster(const std::string& name) {
  alts_cluster_ = &cluster(name);
}

AnpNode& AnpNetwork::add_node(const std::string& cluster_name,
                              const std::string& node_name) {
  return cluster(cluster_name).add_node(node_name);
}

AnpNode* AnpNetwork::find_node(const std::string& name) {
  const auto it = node_index_.find(name);
  return it == node_index_.end() ? nullptr : it->second;
}

const AnpNode* AnpNetwork::find_node(const std::string& name) const {
  const auto it = node_index_.find(name);
  return it == node_index_.end() ? nullptr : it->second;
}

AnpNode& AnpNetwork::node(const std::string& name) {
  AnpNode* n = find_node(name);
  if (n == nullptr) {
    throw std::invalid_argument("unknown node: " + name);
  }
  return *n;
}

const AnpNode& AnpNetwork::node(const std::string& name) const {
  const AnpNode* n = find_node(name);
  if (n == nullptr) {
    throw std::invalid_argument("unknown node: " + name);
  }
  return *n;
}

void AnpNetwork::node_connect(const std::string& src, const std::string& dest) {
  node(src).connect_to(&node(dest));
}

void AnpNetwork::node_disconnect(const std::string& src,
                                 const std::string& dest) {
  node(src).disconnect_from(&node(dest));
}

void AnpNetwork::set_node_comparison(const std::string& wrt_node,
                                     const std::string& a,
                                     const std::string& b,
                                     double value) {
  AnpNode& wrt = node(wrt_node);
  const AnpNode& na = node(a);
  const AnpNode& nb = node(b);
  if (na.cluster()->name() != nb.cluster()->name()) {
    throw std::invalid_argument(
        "node comparisons must be within the same destination cluster");
  }
  // Ensure connections exist so the pairwise table includes both alternatives.
  wrt.connect_to(const_cast<AnpNode*>(&na));
  wrt.connect_to(const_cast<AnpNode*>(&nb));
  PairwiseJudgments* pw = wrt.node_pairwise(na.cluster()->name());
  if (pw == nullptr) {
    throw std::logic_error("missing node pairwise after connect");
  }
  pw->set_comparison(a, b, value);
}

void AnpNetwork::set_cluster_comparison(const std::string& wrt_cluster,
                                        const std::string& a,
                                        const std::string& b,
                                        double value) {
  AnpCluster& wrt = cluster(wrt_cluster);
  AnpCluster& ca = cluster(a);
  AnpCluster& cb = cluster(b);
  wrt.cluster_connect(&ca);
  wrt.cluster_connect(&cb);
  wrt.cluster_pairwise().set_comparison(a, b, value);
}

void AnpNetwork::remove_node(const std::string& name) {
  AnpNode* n = find_node(name);
  if (n == nullptr) {
    throw std::invalid_argument("unknown node: " + name);
  }
  // Drop connections that mention this node.
  for (AnpNode* other : nodes()) {
    if (other == n) continue;
    other->disconnect_from(n);
    n->disconnect_from(other);
  }
  AnpCluster* c = n->cluster();
  for (auto it = c->nodes_.begin(); it != c->nodes_.end(); ++it) {
    if (it->get() == n) {
      unregister_node(name);
      node_positions_.erase(name);
      c->nodes_.erase(it);
      return;
    }
  }
}

void AnpNetwork::remove_cluster(const std::string& name) {
  AnpCluster* c = find_cluster(name);
  if (c == nullptr) {
    throw std::invalid_argument("unknown cluster: " + name);
  }
  // Remove nodes first (copies names because remove_node mutates).
  const std::vector<std::string> names = c->node_names();
  for (const std::string& n : names) {
    remove_node(n);
  }
  // Drop this cluster from other clusters' pairwise tables.
  for (auto& other : clusters_) {
    if (other.get() == c) continue;
    other->prioritizer_.remove_alternative(name, /*ignore_missing=*/true);
  }
  if (alts_cluster_ == c) {
    alts_cluster_ = nullptr;
  }
  cluster_positions_.erase(name);
  for (auto it = clusters_.begin(); it != clusters_.end(); ++it) {
    if (it->get() == c) {
      clusters_.erase(it);
      return;
    }
  }
}

void AnpNetwork::clear_subnetwork(const std::string& node_name) {
  node(node_name).subnetwork_.reset();
}

void AnpNetwork::set_cluster_position(const std::string& name,
                                      double x,
                                      double y) {
  (void)cluster(name);  // validate
  cluster_positions_[name] = {x, y};
}

void AnpNetwork::set_node_position(const std::string& name,
                                   double x,
                                   double y) {
  (void)node(name);
  node_positions_[name] = {x, y};
}

bool AnpNetwork::cluster_position(const std::string& name,
                                  double& x,
                                  double& y) const {
  const auto it = cluster_positions_.find(name);
  if (it == cluster_positions_.end()) return false;
  x = it->second.first;
  y = it->second.second;
  return true;
}

bool AnpNetwork::node_position(const std::string& name,
                               double& x,
                               double& y) const {
  const auto it = node_positions_.find(name);
  if (it == node_positions_.end()) return false;
  x = it->second.first;
  y = it->second.second;
  return true;
}

std::size_t AnpNetwork::nnodes() const {
  return node_index_.size();
}

std::vector<std::string> AnpNetwork::cluster_names() const {
  std::vector<std::string> names;
  names.reserve(clusters_.size());
  for (const auto& c : clusters_) {
    names.push_back(c->name());
  }
  return names;
}

std::vector<std::string> AnpNetwork::node_names() const {
  std::vector<std::string> names;
  names.reserve(nnodes());
  for (const auto& c : clusters_) {
    for (const auto& n : c->nodes_) {
      names.push_back(n->name());
    }
  }
  return names;
}

std::vector<AnpCluster*> AnpNetwork::clusters() {
  std::vector<AnpCluster*> out;
  out.reserve(clusters_.size());
  for (auto& c : clusters_) {
    out.push_back(c.get());
  }
  return out;
}

std::vector<const AnpCluster*> AnpNetwork::clusters() const {
  std::vector<const AnpCluster*> out;
  out.reserve(clusters_.size());
  for (const auto& c : clusters_) {
    out.push_back(c.get());
  }
  return out;
}

std::vector<AnpNode*> AnpNetwork::nodes() {
  std::vector<AnpNode*> out;
  out.reserve(nnodes());
  for (auto& c : clusters_) {
    for (auto& n : c->nodes_) {
      out.push_back(n.get());
    }
  }
  return out;
}

std::vector<const AnpNode*> AnpNetwork::nodes() const {
  std::vector<const AnpNode*> out;
  out.reserve(nnodes());
  for (const auto& c : clusters_) {
    for (const auto& n : c->nodes_) {
      out.push_back(n.get());
    }
  }
  return out;
}

void AnpNetwork::register_node(AnpNode* node) {
  node_index_[node->name()] = node;
}

void AnpNetwork::unregister_node(const std::string& name) {
  node_index_.erase(name);
}

std::size_t AnpNetwork::node_index(const std::string& name) const {
  const std::vector<std::string> names = node_names();
  for (std::size_t i = 0; i < names.size(); ++i) {
    if (names[i] == name) {
      return i;
    }
  }
  throw std::invalid_argument("unknown node: " + name);
}

Matrix AnpNetwork::unscaled_supermatrix() const {
  const std::size_t n = nnodes();
  Matrix rval(n, n, 0.0);
  const auto node_list = nodes();
  for (std::size_t col = 0; col < node_list.size(); ++col) {
    const Vector column = node_list[col]->unscaled_column();
    for (std::size_t row = 0; row < n; ++row) {
      rval(row, col) = column[row];
    }
  }
  return rval;
}

Matrix AnpNetwork::cluster_weight_matrix() const {
  const std::size_t nc = nclusters();
  Matrix W(nc, nc, 0.0);
  for (std::size_t col = 0; col < nc; ++col) {
    const AnpCluster* src = clusters_[col].get();
    const PairwiseJudgments& pw = src->cluster_pairwise();
    if (pw.empty()) {
      continue;
    }
    const Vector local = pw.priorities();
    for (std::size_t i = 0; i < pw.size(); ++i) {
      const AnpCluster* dest = find_cluster(pw.alternatives()[i]);
      if (dest == nullptr) {
        continue;
      }
      // Find row index of dest cluster.
      for (std::size_t row = 0; row < nc; ++row) {
        if (clusters_[row].get() == dest) {
          W(row, col) = local[i];
          break;
        }
      }
    }
  }
  return W;
}

Matrix AnpNetwork::scaled_supermatrix() const {
  Matrix rval = unscaled_supermatrix();
  const auto cluster_list = clusters();
  const std::size_t nclusters = cluster_list.size();

  std::size_t col = 0;
  for (std::size_t col_cp = 0; col_cp < nclusters; ++col_cp) {
    const AnpCluster* col_cluster = cluster_list[col_cp];
    const PairwiseJudgments& pw = col_cluster->cluster_pairwise();
    std::map<std::string, double> cluster_pris;
    if (!pw.empty()) {
      const Vector local = pw.priorities();
      for (std::size_t i = 0; i < pw.size(); ++i) {
        cluster_pris[pw.alternatives()[i]] = local[i];
      }
    }

    for (std::size_t col_node = 0; col_node < col_cluster->nnodes();
         ++col_node) {
      (void)col_node;
      std::size_t row = 0;
      for (std::size_t row_cp = 0; row_cp < nclusters; ++row_cp) {
        const AnpCluster* row_cluster = cluster_list[row_cp];
        double priority = 0.0;
        const auto it = cluster_pris.find(row_cluster->name());
        if (it != cluster_pris.end()) {
          priority = it->second;
        }
        for (std::size_t row_node = 0; row_node < row_cluster->nnodes();
             ++row_node) {
          (void)row_node;
          rval(row, col) *= priority;
          ++row;
        }
      }
      ++col;
    }
  }

  column_normalize_inplace(rval);
  return rval;
}

Matrix AnpNetwork::limit_matrix(const LimitMatrixOptions& options) const {
  return calculus_limit(scaled_supermatrix(), options);
}

Vector AnpNetwork::global_priority(const LimitMatrixOptions& options) const {
  return priority_from_limit(limit_matrix(options));
}

bool AnpNetwork::has_subnet() const {
  for (const AnpNode* n : nodes()) {
    if (n->has_subnetwork()) {
      return true;
    }
  }
  return false;
}

AnpNetwork& AnpNetwork::subnet(const std::string& node_name) {
  return node(node_name).ensure_subnetwork();
}

std::vector<std::string> AnpNetwork::alt_names() const {
  if (has_subnet()) {
    std::vector<std::string> rval;
    for (const AnpNode* n : nodes()) {
      if (!n->has_subnetwork()) {
        continue;
      }
      for (const std::string& alt : n->subnetwork()->alt_names()) {
        bool found = false;
        for (const std::string& existing : rval) {
          if (existing == alt) {
            found = true;
            break;
          }
        }
        if (!found) {
          rval.push_back(alt);
        }
      }
    }
    return rval;
  }
  if (alts_cluster_ == nullptr) {
    return {};
  }
  return alts_cluster_->node_names();
}

Vector AnpNetwork::priority(const LimitMatrixOptions& options) const {
  const std::map<std::string, double> scores = priority_map(options);
  const std::vector<std::string> alts = alt_names();
  Vector rval(alts.size(), 0.0);
  for (std::size_t i = 0; i < alts.size(); ++i) {
    const auto it = scores.find(alts[i]);
    rval[i] = it == scores.end() ? 0.0 : it->second;
  }
  return rval;
}

std::map<std::string, double> AnpNetwork::priority_map(
    const LimitMatrixOptions& options) const {
  if (has_subnet()) {
    const Vector synth = subnet_synthesize(options);
    const std::vector<std::string> alts = alt_names();
    std::map<std::string, double> out;
    for (std::size_t i = 0; i < alts.size(); ++i) {
      out[alts[i]] = synth[i];
    }
    return out;
  }

  const Vector gp = global_priority(options);
  const std::vector<std::string> names = node_names();
  const std::vector<std::string> alts = alt_names();
  std::map<std::string, double> raw;
  double total = 0.0;
  for (const std::string& alt : alts) {
    for (std::size_t i = 0; i < names.size(); ++i) {
      if (names[i] == alt) {
        raw[alt] = gp[i];
        total += gp[i];
        break;
      }
    }
  }
  if (total != 0.0) {
    for (auto& [name, value] : raw) {
      (void)name;
      value /= total;
    }
  }
  return raw;
}

Vector AnpNetwork::subnet_synthesize(const LimitMatrixOptions& options) const {
  const Vector gp = global_priority(options);
  const std::vector<std::string> names = node_names();

  std::map<std::string, double> subnet_weights;
  std::map<std::string, std::map<std::string, double>> alt_scores;

  for (const AnpNode* n : nodes()) {
    if (!n->has_subnetwork()) {
      continue;
    }
    double weight = 0.0;
    for (std::size_t i = 0; i < names.size(); ++i) {
      if (names[i] == n->name()) {
        weight = gp[i];
        break;
      }
    }
    subnet_weights[n->name()] = weight;

    std::map<std::string, double> scores = n->subnetwork()->priority_map(options);
    if (n->invert()) {
      for (auto& [alt, val] : scores) {
        (void)alt;
        val = 1.0 - val;
      }
    }
    alt_scores[n->name()] = std::move(scores);
  }

  const std::map<std::string, double> combined =
      synthesize(synthesis_, subnet_weights, alt_scores, alt_names());
  const std::vector<std::string> alts = alt_names();
  Vector rval(alts.size(), 0.0);
  for (std::size_t i = 0; i < alts.size(); ++i) {
    const auto it = combined.find(alts[i]);
    rval[i] = it == combined.end() ? 0.0 : it->second;
  }
  const double total = rval.sum();
  if (total != 0.0) {
    rval.normalize();
  }
  return rval;
}

}  // namespace cppanp
