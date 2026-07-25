#pragma once

// Small formatting helpers shared by the cppanp example programs. These are
// header-only so each example stays a single translation unit.

#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "cppanp/network.hpp"

namespace cppanp::examples {

inline void print_header(const std::string& title) {
  std::cout << "\n" << std::string(72, '=') << "\n";
  std::cout << title << "\n";
  std::cout << std::string(72, '=') << "\n";
}

inline void print_section(const std::string& title) {
  std::cout << "\n--- " << title << " ---\n";
}

// Longest label width, clamped to a sensible minimum for alignment.
inline int label_width(const std::vector<std::string>& labels, int min_width = 8) {
  std::size_t w = static_cast<std::size_t>(min_width);
  for (const auto& l : labels) w = std::max(w, l.size());
  return static_cast<int>(w);
}

inline void print_vector(const std::string& label,
                         const std::vector<std::string>& names,
                         const Vector& v) {
  std::cout << label << ":\n";
  const int lw = label_width(names);
  for (std::size_t i = 0; i < v.size(); ++i) {
    const std::string& name = i < names.size() ? names[i] : std::to_string(i);
    std::cout << "  " << std::left << std::setw(lw) << name << "  "
              << std::right << std::fixed << std::setprecision(6) << v[i]
              << "\n";
  }
}

inline void print_matrix(const std::string& label,
                         const std::vector<std::string>& row_labels,
                         const std::vector<std::string>& col_labels,
                         const Matrix& m) {
  std::cout << label << ":\n";
  const int lw = label_width(row_labels);
  const int cw = 9;  // per-column width for numbers/headers

  std::cout << "  " << std::string(static_cast<std::size_t>(lw), ' ');
  for (const auto& c : col_labels) {
    std::string h = c.size() > static_cast<std::size_t>(cw - 1)
                        ? c.substr(0, static_cast<std::size_t>(cw - 1))
                        : c;
    std::cout << " " << std::right << std::setw(cw) << h;
  }
  std::cout << "\n";

  for (std::size_t i = 0; i < m.rows(); ++i) {
    const std::string& name =
        i < row_labels.size() ? row_labels[i] : std::to_string(i);
    std::cout << "  " << std::left << std::setw(lw) << name;
    for (std::size_t j = 0; j < m.cols(); ++j) {
      std::cout << " " << std::right << std::setw(cw) << std::fixed
                << std::setprecision(4) << m(i, j);
    }
    std::cout << "\n";
  }
}

inline void print_pairwise(const std::string& label,
                           const PairwiseJudgments& pw) {
  if (pw.size() < 2) {
    std::cout << label << ": (single element -> priority 1.0)\n";
    return;
  }
  const auto& names = pw.alternatives();
  print_matrix(label, names, names, pw.matrix());
  const Vector pri = pw.priorities();
  print_vector("  local priorities", names, pri);
  std::cout << "  consistency ratio: " << std::fixed << std::setprecision(4)
            << pw.consistency_ratio() << "\n";
}

// Print every pairwise input (cluster comparisons + node comparisons) that
// drives the supermatrix. Traverses in stable cluster/node order.
inline void print_network_inputs(AnpNetwork& net) {
  print_section("Cluster comparison inputs");
  bool any_cluster = false;
  for (AnpCluster* c : net.clusters()) {
    if (c->cluster_pairwise().size() >= 2) {
      any_cluster = true;
      print_pairwise("Clusters compared wrt cluster '" + c->name() + "'",
                     c->cluster_pairwise());
      std::cout << "\n";
    }
  }
  if (!any_cluster) std::cout << "(none / trivial)\n";

  print_section("Node comparison inputs");
  bool any_node = false;
  for (AnpNode* node : net.nodes()) {
    for (AnpCluster* dest : net.clusters()) {
      const PairwiseJudgments* pw = node->node_pairwise(dest->name());
      if (pw != nullptr && pw->size() >= 2) {
        any_node = true;
        print_pairwise("Nodes in '" + dest->name() +
                           "' compared wrt node '" + node->name() + "'",
                       *pw);
        std::cout << "\n";
      }
    }
  }
  if (!any_node) std::cout << "(none / trivial)\n";
}

// Print the standard chain of ANP matrices and the final alternative scores.
inline void print_network_results(AnpNetwork& net) {
  const std::vector<std::string> nodes = net.node_names();
  const std::vector<std::string> clusters = net.cluster_names();

  print_section("Unscaled supermatrix");
  print_matrix("W (unscaled)", nodes, nodes, net.unscaled_supermatrix());

  print_section("Cluster weight matrix");
  print_matrix("cluster weights", clusters, clusters,
               net.cluster_weight_matrix());

  print_section("Scaled (weighted) supermatrix");
  print_matrix("W (scaled, column-stochastic)", nodes, nodes,
               net.scaled_supermatrix());

  print_section("Limit matrix");
  print_matrix("W^inf", nodes, nodes, net.limit_matrix());

  print_section("Global priorities (all nodes)");
  print_vector("global priority", nodes, net.global_priority());

  print_section("Alternative scores");
  print_vector("alternatives", net.alt_names(), net.priority());
}

}  // namespace cppanp::examples
