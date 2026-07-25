// Tree134 -- a simple AHP tree hierarchy.
//
//   Goal:        "Best Car"                 (1 node)
//   Criteria:    Cost, Quality, Style        (3 nodes, compared wrt the goal)
//   Alternatives Civic, Accord, CR-V         (evaluated under EACH criterion)
//
// The goal points at the three criteria; each criterion points at the shared
// alternatives cluster. That makes the scaled supermatrix a strict hierarchy
// (nilpotent), so cppanp resolves the limit matrix with the hierarchy formula.

#include <iostream>

#include "anp_print.hpp"
#include "cppanp/network.hpp"

using namespace cppanp;
using namespace cppanp::examples;

int main() {
  print_header("Tree134: goal / 3 criteria / 3 alternatives hierarchy");

  AnpNetwork net;  // creates the default "Alternatives" cluster
  net.add_cluster("Goal");
  net.add_cluster("Criteria");

  net.add_node("Goal", "Best Car");
  for (const char* c : {"Cost", "Quality", "Style"}) net.add_node("Criteria", c);
  for (const char* a : {"Civic", "Accord", "CR-V"})
    net.add_node("Alternatives", a);

  // Goal -> criteria, each criterion -> alternatives.
  for (const char* c : {"Cost", "Quality", "Style"})
    net.node_connect("Best Car", c);
  for (const char* c : {"Cost", "Quality", "Style"})
    for (const char* a : {"Civic", "Accord", "CR-V"}) net.node_connect(c, a);

  // Criteria importance wrt the goal (Cost > Quality > Style).
  net.set_node_comparison("Best Car", "Cost", "Quality", 2.0);
  net.set_node_comparison("Best Car", "Cost", "Style", 4.0);
  net.set_node_comparison("Best Car", "Quality", "Style", 2.0);

  // Alternatives wrt Cost (cheaper is better: Civic > Accord > CR-V).
  net.set_node_comparison("Cost", "Civic", "Accord", 2.0);
  net.set_node_comparison("Cost", "Civic", "CR-V", 4.0);
  net.set_node_comparison("Cost", "Accord", "CR-V", 2.0);

  // Alternatives wrt Quality (Accord > CR-V > Civic).
  net.set_node_comparison("Quality", "Accord", "Civic", 3.0);
  net.set_node_comparison("Quality", "Accord", "CR-V", 2.0);
  net.set_node_comparison("Quality", "CR-V", "Civic", 2.0);

  // Alternatives wrt Style (CR-V > Civic > Accord).
  net.set_node_comparison("Style", "CR-V", "Civic", 2.0);
  net.set_node_comparison("Style", "CR-V", "Accord", 3.0);
  net.set_node_comparison("Style", "Civic", "Accord", 2.0);

  print_network_inputs(net);
  print_network_results(net);
  return 0;
}
