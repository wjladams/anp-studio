#include "cppanp/network.hpp"

#include <gtest/gtest.h>

using cppanp::AnpNetwork;

TEST(SubnetTest, AltNamesComeFromSubnetworks) {
  AnpNetwork net(false);
  net.add_cluster("Controls");
  net.add_node("Controls", "Benefit");
  net.add_node("Controls", "Cost");

  AnpNetwork& b = net.subnet("Benefit");
  b.add_cluster("Alternatives");
  b.set_alternatives_cluster("Alternatives");
  b.add_node("Alternatives", "Alt1");
  b.add_node("Alternatives", "Alt2");

  AnpNetwork& c = net.subnet("Cost");
  c.add_cluster("Alternatives");
  c.set_alternatives_cluster("Alternatives");
  c.add_node("Alternatives", "Alt1");
  c.add_node("Alternatives", "Alt2");
  c.add_node("Alternatives", "Alt3");

  const auto alts = net.alt_names();
  ASSERT_EQ(alts.size(), 3u);
  EXPECT_EQ(alts[0], "Alt1");
  EXPECT_EQ(alts[1], "Alt2");
  EXPECT_EQ(alts[2], "Alt3");
}

TEST(SubnetTest, WeightedSynthesisAndInvert) {
  // Parent: two control nodes with equal global weight via identity-like
  // structure is hard; instead build a tiny network where Benefit and Cost
  // are the only nodes and form a 2-cycle with equal weights, OR simpler:
  // use global_priority by constructing priorities through a hierarchy.
  //
  // Simpler approach for synthesis unit: call priority_map on parent after
  // giving each control a subnet with known alt scores, and force equal
  // parent weights by making both controls alternatives of a Goal with 1:1.
  AnpNetwork net;
  net.add_cluster("Controls");
  net.add_node("Controls", "Benefit");
  net.add_node("Controls", "Cost");
  // Goal in a Criteria cluster pointing to Benefit and Cost equally.
  net.add_cluster("Criteria");
  net.add_node("Criteria", "Goal");
  net.node_connect("Goal", "Benefit");
  net.node_connect("Goal", "Cost");
  // Leave comparison at identity defaults: both connected, matrix is I 2x2
  // until we set comparisons — with only connections and diagonal 1s and
  // zeros off-diagonal, Harker will fill. Set equal preference explicitly:
  net.set_node_comparison("Goal", "Benefit", "Cost", 1.0);

  AnpNetwork& b = net.subnet("Benefit");
  b.add_cluster("Alternatives");
  b.set_alternatives_cluster("Alternatives");
  b.add_node("Alternatives", "Alt1");
  b.add_node("Alternatives", "Alt2");
  b.add_cluster("Crit");
  b.add_node("Crit", "BGoal");
  b.node_connect("BGoal", "Alt1");
  b.node_connect("BGoal", "Alt2");
  b.set_node_comparison("BGoal", "Alt1", "Alt2", 3.0);  // 0.75, 0.25

  AnpNetwork& c = net.subnet("Cost");
  c.add_cluster("Alternatives");
  c.set_alternatives_cluster("Alternatives");
  c.add_node("Alternatives", "Alt1");
  c.add_node("Alternatives", "Alt2");
  c.add_cluster("Crit");
  c.add_node("Crit", "CGoal");
  c.node_connect("CGoal", "Alt1");
  c.node_connect("CGoal", "Alt2");
  c.set_node_comparison("CGoal", "Alt1", "Alt2", 1.0 / 3.0);  // 0.25, 0.75

  // Without invert: equal parent weights -> average of (0.75,0.25) and (0.25,0.75)
  auto scores = net.priority_map();
  EXPECT_NEAR(scores.at("Alt1"), 0.5, 1e-5);
  EXPECT_NEAR(scores.at("Alt2"), 0.5, 1e-5);

  // Invert Cost subnet: scores become (0.75, 0.25), average with Benefit stays
  // (0.75, 0.25).
  net.node("Cost").set_invert(true);
  scores = net.priority_map();
  EXPECT_NEAR(scores.at("Alt1"), 0.75, 1e-5);
  EXPECT_NEAR(scores.at("Alt2"), 0.25, 1e-5);
}

TEST(SubnetTest, NestedSubnetworkAltNames) {
  AnpNetwork net(false);
  net.add_cluster("Top");
  net.add_node("Top", "Parent");

  AnpNetwork& mid = net.subnet("Parent");
  mid.add_cluster("Mid");
  mid.add_node("Mid", "Child");

  AnpNetwork& leaf = mid.subnet("Child");
  leaf.add_cluster("Alternatives");
  leaf.set_alternatives_cluster("Alternatives");
  leaf.add_node("Alternatives", "DeepAlt");

  const auto alts = net.alt_names();
  ASSERT_EQ(alts.size(), 1u);
  EXPECT_EQ(alts[0], "DeepAlt");
}
