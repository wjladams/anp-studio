// Network23 -- a fully connected ANP network with feedback.
//
//   Criteria:     Price, Quality           (2 nodes)
//   Alternatives: A, B, C                   (3 nodes)
//
// Every node depends on every other node (criteria on alternatives, alternatives
// on criteria, plus inner dependence). The scaled supermatrix therefore has
// feedback, so cppanp resolves the limit matrix with the iterative "calculus"
// method rather than the hierarchy short-circuit.

#include <iostream>

#include "anp_print.hpp"
#include "cppanp/network.hpp"

using namespace cppanp;
using namespace cppanp::examples;

int main() {
  print_header("Network23: fully connected 2-cluster ANP network");

  AnpNetwork net;  // default "Alternatives" cluster
  net.add_cluster("Criteria");

  for (const char* c : {"Price", "Quality"}) net.add_node("Criteria", c);
  for (const char* a : {"A", "B", "C"}) net.add_node("Alternatives", a);

  // Inner dependence among the two criteria (single-element -> weight 1).
  net.node_connect("Price", "Quality");
  net.node_connect("Quality", "Price");

  // Cluster weights (feedback between the clusters, both directions).
  net.set_cluster_comparison("Criteria", "Alternatives", "Criteria", 2.0);
  net.set_cluster_comparison("Alternatives", "Criteria", "Alternatives", 2.0);

  // Alternatives wrt Price (A cheapest > B > C).
  net.set_node_comparison("Price", "A", "B", 2.0);
  net.set_node_comparison("Price", "A", "C", 4.0);
  net.set_node_comparison("Price", "B", "C", 2.0);

  // Alternatives wrt Quality (C best > B > A).
  net.set_node_comparison("Quality", "C", "A", 4.0);
  net.set_node_comparison("Quality", "C", "B", 2.0);
  net.set_node_comparison("Quality", "B", "A", 2.0);

  // Criteria importance wrt each alternative (feedback into the criteria).
  net.set_node_comparison("A", "Price", "Quality", 3.0);
  net.set_node_comparison("B", "Price", "Quality", 1.0);
  net.set_node_comparison("C", "Quality", "Price", 3.0);

  // Inner dependence among alternatives (wrt each alternative, rank the others).
  net.set_node_comparison("A", "B", "C", 2.0);
  net.set_node_comparison("B", "A", "C", 2.0);
  net.set_node_comparison("C", "A", "B", 2.0);

  print_network_inputs(net);
  print_network_results(net);
  return 0;
}
