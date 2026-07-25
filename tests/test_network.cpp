#include "cppanp/network.hpp"

#include <gtest/gtest.h>

using cppanp::AnpNetwork;
using cppanp::Matrix;
using cppanp::Vector;

TEST(NetworkTest, CreatesAlternativesClusterByDefault) {
  AnpNetwork net;
  ASSERT_NE(net.alternatives_cluster(), nullptr);
  EXPECT_EQ(net.alternatives_cluster()->name(), "Alternatives");
  EXPECT_EQ(net.nclusters(), 1u);
}

TEST(NetworkTest, EmptyWithoutAlternativesCluster) {
  AnpNetwork net(false);
  EXPECT_EQ(net.alternatives_cluster(), nullptr);
  EXPECT_EQ(net.nclusters(), 0u);
}

TEST(NetworkTest, AddClusterAndNode) {
  AnpNetwork net;
  net.add_cluster("Criteria");
  net.add_node("Criteria", "Cost");
  net.add_node("Alternatives", "Alt1");
  EXPECT_EQ(net.nnodes(), 2u);
  EXPECT_EQ(net.node("Cost").cluster()->name(), "Criteria");
}

TEST(NetworkTest, DuplicateNodeThrows) {
  AnpNetwork net;
  net.add_cluster("Criteria");
  net.add_node("Criteria", "Cost");
  EXPECT_THROW(net.add_node("Alternatives", "Cost"), std::invalid_argument);
}

TEST(NetworkTest, NodeConnectCreatesClusterLink) {
  AnpNetwork net;
  net.add_cluster("Criteria");
  net.add_node("Criteria", "Cost");
  net.add_node("Alternatives", "Alt1");
  net.node_connect("Cost", "Alt1");

  EXPECT_TRUE(net.node("Cost").is_connected_to(&net.node("Alt1")));
  EXPECT_TRUE(
      net.cluster("Criteria").cluster_pairwise().has_alternative("Alternatives"));
}

TEST(NetworkTest, Descriptions) {
  AnpNetwork net;
  net.add_cluster("Criteria");
  net.cluster("Criteria").set_description("top criteria");
  net.add_node("Criteria", "Cost");
  net.node("Cost").set_description("price");
  EXPECT_EQ(net.cluster("Criteria").description(), "top criteria");
  EXPECT_EQ(net.node("Cost").description(), "price");
}

// Classic AHP: one Criteria node comparing two alts with 2:1 preference.
TEST(NetworkTest, FlatAhpUnscaledAndPriority) {
  AnpNetwork net;
  net.add_cluster("Criteria");
  net.add_node("Criteria", "Goal");
  net.add_node("Alternatives", "A");
  net.add_node("Alternatives", "B");

  net.node_connect("Goal", "A");
  net.node_connect("Goal", "B");
  net.set_node_comparison("Goal", "A", "B", 2.0);
  // Only destination cluster is Alternatives; give it weight 1.
  // Cluster pairwise for Criteria has only Alternatives -> priority 1 automatically.

  const Matrix U = net.unscaled_supermatrix();
  // Node order: Goal, A, B (Alternatives created first, then Criteria added...)
  // Wait: default creates Alternatives first, then we add Criteria.
  // Order: Alternatives nodes first (A, B), then Criteria (Goal)?
  // clusters_: [Alternatives, Criteria]
  // node order: A, B, Goal
  const std::vector<std::string> names = net.node_names();
  ASSERT_EQ(names.size(), 3u);
  EXPECT_EQ(names[0], "A");
  EXPECT_EQ(names[1], "B");
  EXPECT_EQ(names[2], "Goal");

  // Goal column (col 2) should be [2/3, 1/3, 0]
  EXPECT_NEAR(U(0, 2), 2.0 / 3.0, 1e-9);
  EXPECT_NEAR(U(1, 2), 1.0 / 3.0, 1e-9);
  EXPECT_NEAR(U(2, 2), 0.0, 1e-12);

  const Matrix S = net.scaled_supermatrix();
  // Cluster weight Alternatives wrt Criteria is 1, so Goal column unchanged
  // before normalize; A,B columns are zeros -> remain zero.
  EXPECT_NEAR(S(0, 2), 2.0 / 3.0, 1e-9);
  EXPECT_NEAR(S(1, 2), 1.0 / 3.0, 1e-9);

  const auto scores = net.priority_map();
  EXPECT_NEAR(scores.at("A"), 2.0 / 3.0, 1e-6);
  EXPECT_NEAR(scores.at("B"), 1.0 / 3.0, 1e-6);
}

TEST(NetworkTest, ClusterWeightsScaleSupermatrixBlocks) {
  AnpNetwork net;
  net.add_cluster("C1");
  net.add_cluster("C2");
  net.add_node("C1", "N1");
  net.add_node("C2", "N2");
  net.add_node("Alternatives", "A");

  net.node_connect("N1", "A");
  net.node_connect("N2", "A");
  // Cluster comparisons from Alternatives? Not needed.
  // From C1: compare destination clusters — only Alternatives connected.
  // Give C1 a second connection so we can set weights: connect C1->C2 as well
  // via a dummy? Simpler: connect N1 also to N2 so C1 connects to C2 and Alts.
  net.node_connect("N1", "N2");
  net.set_cluster_comparison("C1", "Alternatives", "C2", 3.0);

  const Matrix U = net.unscaled_supermatrix();
  const Matrix S = net.scaled_supermatrix();

  // Find indices
  const auto names = net.node_names();
  std::size_t iA = 0, iN1 = 0, iN2 = 0;
  for (std::size_t i = 0; i < names.size(); ++i) {
    if (names[i] == "A") iA = i;
    if (names[i] == "N1") iN1 = i;
    if (names[i] == "N2") iN2 = i;
  }

  // Cluster priorities wrt C1 for Alts:C2 = 3:1 -> 0.75, 0.25
  EXPECT_NEAR(U(iA, iN1), 1.0, 1e-9);  // only alt in that pairwise
  // After scaling, A entry in N1 column multiplied by 0.75, N2 by 0.25, then col norm
  const double scaled_A = U(iA, iN1) * 0.75;
  const double scaled_N2 = U(iN2, iN1) * 0.25;
  const double col_sum = scaled_A + scaled_N2;  // Goal self 0
  EXPECT_NEAR(S(iA, iN1), scaled_A / col_sum, 1e-9);
  EXPECT_NEAR(S(iN2, iN1), scaled_N2 / col_sum, 1e-9);
  EXPECT_NEAR(S.col_sums()[iN1], 1.0, 1e-9);
}
