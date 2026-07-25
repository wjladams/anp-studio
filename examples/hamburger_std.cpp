// HamburgerStd -- SuperDecisions sample hamburger market-share ANP network.
//
// Source: Creative Decisions Foundation / SuperDecisions tutorial
//   "Tutorial on Complex Decision Models (ANP)" (v28_man04.pdf), Tables 1–5.
// The published unweighted (unscaled) supermatrix and cluster-weight matrix are
// reconstituted here by turning each local priority block back into pairwise
// ratios a_ij = p_i / p_j. Target synthesize result (normalized by cluster):
//   McDonald's 0.5549, Burger King 0.2801, Wendy's 0.1650
// (actual 1994 Market Share Reporter: 0.5823 / 0.2857 / 0.1320).

#include <iomanip>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "anp_print.hpp"
#include "cppanp/network.hpp"

using namespace cppanp;
using namespace cppanp::examples;

namespace {

using Pri = std::pair<std::string, double>;

// Connect dest nodes to `wrt` and fill pairwise ratios from local priorities.
void set_node_pris(AnpNetwork& net,
                   const std::string& wrt,
                   const std::vector<Pri>& pris) {
  if (pris.empty()) return;
  for (const auto& [name, _] : pris) {
    (void)_;
    net.node_connect(wrt, name);
  }
  if (pris.size() == 1) return;
  for (std::size_t i = 0; i < pris.size(); ++i) {
    for (std::size_t j = i + 1; j < pris.size(); ++j) {
      net.set_node_comparison(wrt, pris[i].first, pris[j].first,
                              pris[i].second / pris[j].second);
    }
  }
}

void set_cluster_pris(AnpNetwork& net,
                      const std::string& wrt,
                      const std::vector<Pri>& pris) {
  if (pris.size() < 2) return;
  for (std::size_t i = 0; i < pris.size(); ++i) {
    for (std::size_t j = i + 1; j < pris.size(); ++j) {
      net.set_cluster_comparison(wrt, pris[i].first, pris[j].first,
                                 pris[i].second / pris[j].second);
    }
  }
}

AnpNetwork build_hamburger() {
  AnpNetwork net;  // creates "Alternatives"
  net.add_cluster("Advertising");
  net.add_cluster("Quality of Food");
  net.add_cluster("Other");

  for (const char* a : {"McDonalds", "BurgerKing", "Wendys"})
    net.add_node("Alternatives", a);
  for (const char* a : {"Creativity", "Promotion", "Frequency"})
    net.add_node("Advertising", a);
  for (const char* a : {"Nutrition", "Taste", "Portion"})
    net.add_node("Quality of Food", a);
  for (const char* a :
       {"Price", "Location", "Service", "Speed", "Cleanliness", "MenuItem",
        "Takeout", "Reputation"})
    net.add_node("Other", a);

  // ---- Unweighted-supermatrix columns (Table 2), by parent node ------------
  // Alternatives (inner dependence + links into every other cluster)
  set_node_pris(net, "McDonalds",
                {{"BurgerKing", 0.8000}, {"Wendys", 0.2000}});
  set_node_pris(net, "McDonalds",
                {{"Creativity", 0.2074},
                 {"Promotion", 0.1298},
                 {"Frequency", 0.6628}});
  set_node_pris(net, "McDonalds",
                {{"Nutrition", 0.3319}, {"Taste", 0.1388}, {"Portion", 0.5293}});
  set_node_pris(net, "McDonalds",
                {{"Price", 0.0329},
                 {"Location", 0.1063},
                 {"Service", 0.0237},
                 {"Speed", 0.0483},
                 {"Cleanliness", 0.3328},
                 {"MenuItem", 0.1593},
                 {"Takeout", 0.0736},
                 {"Reputation", 0.2232}});

  set_node_pris(net, "BurgerKing",
                {{"McDonalds", 0.8333}, {"Wendys", 0.1667}});
  set_node_pris(net, "BurgerKing",
                {{"Creativity", 0.1783},
                 {"Promotion", 0.1120},
                 {"Frequency", 0.7096}});
  set_node_pris(net, "BurgerKing",
                {{"Nutrition", 0.2810}, {"Taste", 0.0720}, {"Portion", 0.6470}});
  set_node_pris(net, "BurgerKing",
                {{"Price", 0.2408},
                 {"Location", 0.2231},
                 {"Service", 0.1418},
                 {"Speed", 0.1407},
                 {"Cleanliness", 0.1096},
                 {"MenuItem", 0.0512},
                 {"Takeout", 0.0506},
                 {"Reputation", 0.0422}});

  set_node_pris(net, "Wendys", {{"McDonalds", 0.7500}, {"BurgerKing", 0.2500}});
  set_node_pris(net, "Wendys",
                {{"Creativity", 0.2810},
                 {"Promotion", 0.0720},
                 {"Frequency", 0.6470}});
  set_node_pris(net, "Wendys",
                {{"Nutrition", 0.6241}, {"Taste", 0.2823}, {"Portion", 0.0936}});
  set_node_pris(net, "Wendys",
                {{"Price", 0.0300},
                 {"Location", 0.1417},
                 {"Service", 0.0648},
                 {"Speed", 0.0641},
                 {"Cleanliness", 0.2756},
                 {"MenuItem", 0.1571},
                 {"Takeout", 0.0589},
                 {"Reputation", 0.2078}});

  // Advertising
  set_node_pris(net, "Creativity",
                {{"McDonalds", 0.6141},
                 {"BurgerKing", 0.2685},
                 {"Wendys", 0.1174}});
  set_node_pris(net, "Creativity",
                {{"Promotion", 0.1250}, {"Frequency", 0.8750}});
  set_node_pris(net, "Creativity",
                {{"Location", 0.7095},
                 {"MenuItem", 0.1377},
                 {"Reputation", 0.1528}});

  set_node_pris(net, "Promotion",
                {{"McDonalds", 0.7174},
                 {"BurgerKing", 0.1942},
                 {"Wendys", 0.0884}});
  set_node_pris(net, "Promotion",
                {{"Creativity", 0.3333}, {"Frequency", 0.6667}});
  set_node_pris(net, "Promotion", {{"Price", 0.8333}, {"MenuItem", 0.1667}});

  set_node_pris(net, "Frequency",
                {{"McDonalds", 0.7174},
                 {"BurgerKing", 0.1942},
                 {"Wendys", 0.0884}});
  set_node_pris(net, "Frequency",
                {{"Creativity", 0.5000}, {"Promotion", 0.5000}});
  set_node_pris(net, "Frequency",
                {{"Location", 0.1958},
                 {"MenuItem", 0.3108},
                 {"Reputation", 0.4934}});

  // Quality of Food (Nutrition/Taste -> Alternatives only; Portion also Other)
  set_node_pris(net, "Nutrition",
                {{"McDonalds", 0.2488},
                 {"BurgerKing", 0.1561},
                 {"Wendys", 0.5951}});
  set_node_pris(net, "Taste",
                {{"McDonalds", 0.2899},
                 {"BurgerKing", 0.1040},
                 {"Wendys", 0.6061}});
  set_node_pris(net, "Portion",
                {{"McDonalds", 0.5989},
                 {"BurgerKing", 0.1262},
                 {"Wendys", 0.2749}});
  set_node_pris(net, "Portion", {{"Price", 0.8571}, {"Takeout", 0.1429}});

  // Other
  set_node_pris(net, "Price",
                {{"McDonalds", 0.6531},
                 {"BurgerKing", 0.2507},
                 {"Wendys", 0.0962}});
  set_node_pris(net, "Price", {{"Promotion", 0.8333}, {"Frequency", 0.1667}});
  set_node_pris(net, "Price", {{"Nutrition", 0.1667}, {"Portion", 0.8333}});
  set_node_pris(net, "Price", {{"Location", 0.5000}, {"Takeout", 0.5000}});

  set_node_pris(net, "Location",
                {{"McDonalds", 0.6531},
                 {"BurgerKing", 0.2507},
                 {"Wendys", 0.0962}});

  set_node_pris(net, "Service",
                {{"McDonalds", 0.3319},
                 {"BurgerKing", 0.1388},
                 {"Wendys", 0.5293}});
  set_node_pris(net, "Service",
                {{"Location", 0.0981},
                 {"Speed", 0.2857},
                 {"Cleanliness", 0.5181},
                 {"Reputation", 0.0981}});

  set_node_pris(net, "Speed",
                {{"McDonalds", 0.5387},
                 {"BurgerKing", 0.3624},
                 {"Wendys", 0.0989}});
  set_node_pris(net, "Speed",
                {{"Service", 0.1873}, {"Takeout", 0.7313}, {"Reputation", 0.0814}});

  set_node_pris(net, "Cleanliness",
                {{"McDonalds", 0.2500},
                 {"BurgerKing", 0.2500},
                 {"Wendys", 0.5000}});
  set_node_pris(net, "Cleanliness",
                {{"Location", 0.1711}, {"Service", 0.0780}, {"Speed", 0.7509}});

  set_node_pris(net, "MenuItem",
                {{"McDonalds", 0.4934},
                 {"BurgerKing", 0.1958},
                 {"Wendys", 0.3108}});
  set_node_pris(net, "MenuItem",
                {{"Creativity", 0.0780},
                 {"Promotion", 0.1711},
                 {"Frequency", 0.7509}});
  set_node_pris(net, "MenuItem",
                {{"Nutrition", 0.0756}, {"Taste", 0.6952}, {"Portion", 0.2292}});
  set_node_pris(net, "MenuItem",
                {{"Price", 0.1153},
                 {"Location", 0.0526},
                 {"Speed", 0.1946},
                 {"Cleanliness", 0.6375}});

  set_node_pris(net, "Takeout",
                {{"McDonalds", 0.4837},
                 {"BurgerKing", 0.3133},
                 {"Wendys", 0.2029}});
  set_node_pris(net, "Takeout",
                {{"Location", 0.6572}, {"Service", 0.0548}, {"Speed", 0.2880}});

  set_node_pris(net, "Reputation",
                {{"McDonalds", 0.6749},
                 {"BurgerKing", 0.2238},
                 {"Wendys", 0.1012}});
  set_node_pris(net, "Reputation",
                {{"Creativity", 0.0819},
                 {"Promotion", 0.3678},
                 {"Frequency", 0.5503}});
  set_node_pris(net, "Reputation",
                {{"Nutrition", 0.0936}, {"Taste", 0.6241}, {"Portion", 0.2823}});
  set_node_pris(net, "Reputation",
                {{"Price", 0.0627},
                 {"Location", 0.2653},
                 {"Service", 0.0444},
                 {"Speed", 0.0835},
                 {"Cleanliness", 0.2378},
                 {"MenuItem", 0.1929},
                 {"Takeout", 0.0567},
                 {"Reputation", 0.0567}});

  // ---- Cluster weights matrix (Table 1) ------------------------------------
  set_cluster_pris(net, "Alternatives",
                   {{"Alternatives", 0.2128},
                    {"Advertising", 0.5319},
                    {"Quality of Food", 0.0659},
                    {"Other", 0.1893}});
  set_cluster_pris(net, "Advertising",
                   {{"Alternatives", 0.2956},
                    {"Advertising", 0.2571},
                    {"Other", 0.4473}});
  set_cluster_pris(net, "Quality of Food",
                   {{"Alternatives", 0.5000}, {"Other", 0.5000}});
  set_cluster_pris(net, "Other",
                   {{"Alternatives", 0.1304},
                    {"Advertising", 0.6079},
                    {"Quality of Food", 0.0655},
                    {"Other", 0.1969}});

  return net;
}

}  // namespace

int main() {
  print_header(
      "HamburgerStd: SuperDecisions hamburger market-share ANP network");

  AnpNetwork net = build_hamburger();

  print_section("Structure");
  std::cout << "Clusters: ";
  for (const auto& c : net.cluster_names()) std::cout << c << "  ";
  std::cout << "\nnodes=" << net.nnodes() << "\n";

  print_network_inputs(net);
  print_network_results(net);

  const Vector alt = net.priority();
  const auto names = net.alt_names();
  std::cout << "\nSuperDecisions published synthesize (normalized by cluster):\n"
               "  McDonald's 0.5549   Burger King 0.2801   Wendy's 0.1650\n"
               "Actual 1994 market share: 0.5823 / 0.2857 / 0.1320\n";
  std::cout << "cppanp result:\n";
  for (std::size_t i = 0; i < alt.size(); ++i) {
    std::cout << "  " << names[i] << "  " << std::fixed << std::setprecision(4)
              << alt[i] << "\n";
  }
  return 0;
}
