// BenefitsCosts -- a standard Benefits/Costs control model with subnetworks.
//
// This mirrors the SuperDecisions / ANP "BOCR"-style pattern: a top-level
// control network ranks the control criteria (here Benefits and Costs), and
// each control node owns its OWN subnetwork that evaluates the same
// alternatives. The alternative scores from the subnetworks are synthesized up
// to the top level, weighted by the control-criteria priorities. The Costs
// subnetwork is marked "inverted" so that a more-costly plan contributes less.
//
//   Top:      Choose (goal) -> { Benefits, Costs }
//   Benefits subnet: { Performance, Convenience } -> { Plan1, Plan2, Plan3 }
//   Costs    subnet: { Money, Risk }              -> { Plan1, Plan2, Plan3 }

#include <iostream>

#include "anp_print.hpp"
#include "cppanp/network.hpp"

using namespace cppanp;
using namespace cppanp::examples;

namespace {

// Build a simple "factors -> alternatives" hierarchy inside a subnetwork.
void build_subnet(AnpNetwork& sub,
                  const std::string& f1,
                  const std::string& f2) {
  sub.add_cluster("Factors");
  sub.add_cluster("Alternatives");
  sub.set_alternatives_cluster("Alternatives");

  sub.add_node("Factors", f1);
  sub.add_node("Factors", f2);
  for (const char* a : {"Plan1", "Plan2", "Plan3"})
    sub.add_node("Alternatives", a);

  for (const std::string& f : {f1, f2})
    for (const char* a : {"Plan1", "Plan2", "Plan3"}) sub.node_connect(f, a);
}

}  // namespace

int main() {
  print_header("BenefitsCosts: control network with subnetworks");

  AnpNetwork net(/*create_alts_cluster=*/false);
  net.add_cluster("Goal");
  net.add_cluster("Control");
  net.add_node("Goal", "Choose");
  net.add_node("Control", "Benefits");
  net.add_node("Control", "Costs");

  net.node_connect("Choose", "Benefits");
  net.node_connect("Choose", "Costs");
  // Benefits count for twice as much as Costs in this decision.
  net.set_node_comparison("Choose", "Benefits", "Costs", 2.0);

  // --- Benefits subnetwork -------------------------------------------------
  AnpNetwork& ben = net.subnet("Benefits");
  build_subnet(ben, "Performance", "Convenience");
  ben.set_node_comparison("Performance", "Plan1", "Plan2", 2.0);
  ben.set_node_comparison("Performance", "Plan1", "Plan3", 4.0);
  ben.set_node_comparison("Performance", "Plan2", "Plan3", 2.0);
  ben.set_node_comparison("Convenience", "Plan3", "Plan1", 3.0);
  ben.set_node_comparison("Convenience", "Plan3", "Plan2", 2.0);
  ben.set_node_comparison("Convenience", "Plan2", "Plan1", 2.0);

  // --- Costs subnetwork (inverted) ----------------------------------------
  AnpNetwork& cost = net.subnet("Costs");
  build_subnet(cost, "Money", "Risk");
  // wrt Money: which plan costs MORE (Plan3 priciest > Plan2 > Plan1).
  cost.set_node_comparison("Money", "Plan3", "Plan2", 2.0);
  cost.set_node_comparison("Money", "Plan3", "Plan1", 4.0);
  cost.set_node_comparison("Money", "Plan2", "Plan1", 2.0);
  // wrt Risk: which plan is riskier (Plan1 riskiest > Plan2 > Plan3).
  cost.set_node_comparison("Risk", "Plan1", "Plan2", 2.0);
  cost.set_node_comparison("Risk", "Plan1", "Plan3", 3.0);
  cost.set_node_comparison("Risk", "Plan2", "Plan3", 2.0);
  // High cost should hurt, not help.
  net.node("Costs").set_invert(true);

  // --- Top-level control network ------------------------------------------
  print_section("Top-level control network inputs");
  print_network_inputs(net);
  print_vector("Control-criteria weights (global priority)", net.node_names(),
               net.global_priority());

  // --- Each subnetwork's own calculation ----------------------------------
  print_header("Benefits subnetwork");
  print_network_inputs(ben);
  print_network_results(ben);

  print_header("Costs subnetwork (scores are inverted when synthesized)");
  print_network_inputs(cost);
  print_network_results(cost);

  // --- Final synthesis -----------------------------------------------------
  print_header("Synthesized alternative scores");
  std::cout << "Benefits weight * benefit-score + Costs weight * (1 - cost-score)"
               ", renormalized:\n";
  print_vector("final scores", net.alt_names(), net.priority());
  return 0;
}
