#include "cppanp/synthesis.hpp"

#include <cmath>
#include <gtest/gtest.h>

#include <map>
#include <string>
#include <vector>

#include "cppanp/network.hpp"

using cppanp::AnpNetwork;
using cppanp::SynthesisKind;
using cppanp::SynthesisOptions;
using cppanp::eval_expression;
using cppanp::synthesize;
using cppanp::synthesize_additive;
using cppanp::synthesize_custom;
using cppanp::synthesize_multiplicative;

TEST(SynthesisTest, EvalExpressionBasics) {
  const std::map<std::string, double> vars{{"Benefits", 0.6}, {"Costs", 0.3}};
  EXPECT_NEAR(eval_expression("Benefits / Costs", vars), 2.0, 1e-12);
  EXPECT_NEAR(eval_expression("Benefits + Costs", vars), 0.9, 1e-12);
  EXPECT_NEAR(eval_expression("(Benefits - Costs) * 2", vars), 0.6, 1e-12);
}

TEST(SynthesisTest, AdditiveMatchesWeightedAverage) {
  std::map<std::string, double> weights{{"Benefit", 0.5}, {"Cost", 0.5}};
  std::map<std::string, std::map<std::string, double>> scores{
      {"Benefit", {{"A", 0.75}, {"B", 0.25}}},
      {"Cost", {{"A", 0.25}, {"B", 0.75}}},
  };
  const auto out = synthesize_additive(weights, scores);
  EXPECT_NEAR(out.at("A"), 0.5, 1e-12);
  EXPECT_NEAR(out.at("B"), 0.5, 1e-12);
}

TEST(SynthesisTest, MultiplicativeProduct) {
  std::map<std::string, double> weights{{"Benefit", 0.5}, {"Cost", 0.5}};
  std::map<std::string, std::map<std::string, double>> scores{
      {"Benefit", {{"A", 0.8}, {"B", 0.2}}},
      {"Cost", {{"A", 0.5}, {"B", 0.5}}},
  };
  const auto out = synthesize_multiplicative(weights, scores);
  // Equal weights -> sqrt(0.8*0.5)=sqrt(0.4), sqrt(0.2*0.5)=sqrt(0.1), normalize
  const double a = std::sqrt(0.4);
  const double b = std::sqrt(0.1);
  EXPECT_NEAR(out.at("A"), a / (a + b), 1e-9);
  EXPECT_NEAR(out.at("B"), b / (a + b), 1e-9);
}

TEST(SynthesisTest, CustomExpressionPerAlt) {
  std::map<std::string, std::map<std::string, double>> scores{
      {"Benefits", {{"A", 0.6}, {"B", 0.4}}},
      {"Costs", {{"A", 0.2}, {"B", 0.8}}},
  };
  const std::vector<std::string> alts{"A", "B"};
  const auto out = synthesize_custom("Benefits / Costs", scores, alts);
  // A: 0.6/0.2=3, B: 0.4/0.8=0.5 -> normalize 3/(3.5), 0.5/3.5
  EXPECT_NEAR(out.at("A"), 3.0 / 3.5, 1e-9);
  EXPECT_NEAR(out.at("B"), 0.5 / 3.5, 1e-9);
}

namespace {

AnpNetwork make_two_subnet_parent() {
  AnpNetwork net;
  net.add_cluster("Controls");
  net.add_node("Controls", "Benefit");
  net.add_node("Controls", "Cost");
  net.add_cluster("Criteria");
  net.add_node("Criteria", "Goal");
  net.node_connect("Goal", "Benefit");
  net.node_connect("Goal", "Cost");
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
  b.set_node_comparison("BGoal", "Alt1", "Alt2", 3.0);

  AnpNetwork& c = net.subnet("Cost");
  c.add_cluster("Alternatives");
  c.set_alternatives_cluster("Alternatives");
  c.add_node("Alternatives", "Alt1");
  c.add_node("Alternatives", "Alt2");
  c.add_cluster("Crit");
  c.add_node("Crit", "CGoal");
  c.node_connect("CGoal", "Alt1");
  c.node_connect("CGoal", "Alt2");
  c.set_node_comparison("CGoal", "Alt1", "Alt2", 1.0 / 3.0);
  return net;
}

}  // namespace

TEST(SynthesisTest, NetworkAdditiveDefault) {
  AnpNetwork net = make_two_subnet_parent();
  auto scores = net.priority_map();
  EXPECT_NEAR(scores.at("Alt1"), 0.5, 1e-5);
  EXPECT_NEAR(scores.at("Alt2"), 0.5, 1e-5);
}

TEST(SynthesisTest, NetworkCustomFormula) {
  AnpNetwork net = make_two_subnet_parent();
  net.node("Cost").set_invert(true);
  SynthesisOptions opt;
  opt.kind = SynthesisKind::Custom;
  opt.custom_expr = "Benefit / Cost";
  net.set_synthesis_options(opt);
  // After invert, Cost scores become (0.75, 0.25); Benefit (0.75, 0.25).
  // Ratio 1 for both -> equal after normalize.
  auto scores = net.priority_map();
  EXPECT_NEAR(scores.at("Alt1"), 0.5, 1e-4);
  EXPECT_NEAR(scores.at("Alt2"), 0.5, 1e-4);
}

TEST(SynthesisTest, DispatchHelper) {
  std::map<std::string, double> weights{{"X", 1.0}};
  std::map<std::string, std::map<std::string, double>> scores{
      {"X", {{"A", 1.0}}},
  };
  SynthesisOptions opt;
  opt.kind = SynthesisKind::Additive;
  auto out = synthesize(opt, weights, scores, {"A"});
  EXPECT_NEAR(out.at("A"), 1.0, 1e-12);
}
