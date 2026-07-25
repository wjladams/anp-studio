#include "cppanp/json_io.hpp"

#include <gtest/gtest.h>

#include "cppanp/network.hpp"

using cppanp::AnpNetwork;
using cppanp::network_from_json;
using cppanp::network_to_json;

TEST(JsonIoTest, RoundTripFlatNetwork) {
  AnpNetwork net;
  net.add_cluster("Criteria");
  net.add_node("Criteria", "Price");
  net.add_node("Criteria", "Quality");
  net.add_node("Alternatives", "A");
  net.add_node("Alternatives", "B");
  net.node_connect("Price", "A");
  net.node_connect("Price", "B");
  net.set_node_comparison("Price", "A", "B", 2.0);
  net.set_cluster_position("Criteria", 10, 20);
  net.set_node_position("Price", 1, 2);

  const std::string json = network_to_json(net);
  auto loaded = network_from_json(json);
  ASSERT_NE(loaded, nullptr);
  EXPECT_EQ(loaded->nclusters(), net.nclusters());
  EXPECT_EQ(loaded->nnodes(), net.nnodes());
  EXPECT_TRUE(loaded->node("Price").is_connected_to(&loaded->node("A")));
  EXPECT_NEAR(loaded->node("Price")
                  .node_pairwise("Alternatives")
                  ->comparison("A", "B"),
              2.0, 1e-12);
  double x = 0, y = 0;
  EXPECT_TRUE(loaded->cluster_position("Criteria", x, y));
  EXPECT_NEAR(x, 10, 1e-12);
  EXPECT_NEAR(y, 20, 1e-12);
  EXPECT_TRUE(loaded->unscaled_supermatrix().is_near(net.unscaled_supermatrix(),
                                                     1e-9, 1e-9));
}

TEST(JsonIoTest, RoundTripWithSubnet) {
  AnpNetwork net(false);
  net.add_cluster("Controls");
  net.add_node("Controls", "Benefits");
  AnpNetwork& sub = net.subnet("Benefits");
  sub.add_cluster("Alternatives");
  sub.set_alternatives_cluster("Alternatives");
  sub.add_node("Alternatives", "Plan1");
  sub.add_node("Alternatives", "Plan2");

  const std::string json = network_to_json(net);
  auto loaded = network_from_json(json);
  ASSERT_TRUE(loaded->node("Benefits").has_subnetwork());
  EXPECT_EQ(loaded->node("Benefits").subnetwork()->alt_names().size(), 2u);
  EXPECT_EQ(loaded->alt_names().size(), 2u);
}

TEST(JsonIoTest, RemoveNodeAndDisconnect) {
  AnpNetwork net;
  net.add_cluster("C");
  net.add_node("C", "X");
  net.add_node("C", "Y");
  net.node_connect("X", "Y");
  EXPECT_TRUE(net.node("X").is_connected_to(&net.node("Y")));
  net.node_disconnect("X", "Y");
  EXPECT_FALSE(net.node("X").is_connected_to(&net.node("Y")));
  net.node_connect("X", "Y");
  net.remove_node("Y");
  EXPECT_EQ(net.nnodes(), 1u);
  EXPECT_EQ(net.find_node("Y"), nullptr);
}
