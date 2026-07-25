#include "cppanp/json_io.hpp"

#include <fstream>
#include <sstream>
#include <utility>

#include <nlohmann/json.hpp>

namespace cppanp {
namespace {

using json = nlohmann::json;

json pairwise_to_json(const PairwiseJudgments& pw) {
  json j;
  j["alternatives"] = pw.alternatives();
  json matrix = json::array();
  for (std::size_t i = 0; i < pw.size(); ++i) {
    json row = json::array();
    for (std::size_t j = 0; j < pw.size(); ++j) {
      row.push_back(pw.comparison(i, j));
    }
    matrix.push_back(row);
  }
  j["matrix"] = matrix;
  return j;
}

void pairwise_from_json(PairwiseJudgments& pw, const json& j) {
  const auto alts = j.at("alternatives").get<std::vector<std::string>>();
  for (const auto& a : alts) {
    pw.add_alternative(a, /*ignore_existing=*/true);
  }
  if (!j.contains("matrix")) return;
  const auto& matrix = j.at("matrix");
  for (std::size_t i = 0; i < alts.size(); ++i) {
    for (std::size_t j = i + 1; j < alts.size(); ++j) {
      const double v = matrix.at(i).at(j).get<double>();
      pw.set_comparison(i, j, v);
    }
  }
}

json network_to_json_obj(const AnpNetwork& net) {
  json j;
  j["clusters"] = json::array();
  for (const AnpCluster* c : net.clusters()) {
    json cj;
    cj["name"] = c->name();
    cj["description"] = c->description();
    double x = 0, y = 0;
    if (net.cluster_position(c->name(), x, y)) {
      cj["x"] = x;
      cj["y"] = y;
    }
    cj["cluster_pairwise"] = pairwise_to_json(c->cluster_pairwise());
    cj["nodes"] = json::array();
    for (const AnpNode* n : c->nodes()) {
      json nj;
      nj["name"] = n->name();
      nj["description"] = n->description();
      nj["invert"] = n->invert();
      double nx = 0, ny = 0;
      if (net.node_position(n->name(), nx, ny)) {
        nj["x"] = nx;
        nj["y"] = ny;
      }
      json pw_map = json::object();
      for (const AnpCluster* dest : net.clusters()) {
        const PairwiseJudgments* pw = n->node_pairwise(dest->name());
        if (pw != nullptr && !pw->empty()) {
          pw_map[dest->name()] = pairwise_to_json(*pw);
        }
      }
      nj["node_pairwise"] = pw_map;
      if (n->has_subnetwork()) {
        nj["subnetwork"] = network_to_json_obj(*n->subnetwork());
      }
      cj["nodes"].push_back(nj);
    }
    j["clusters"].push_back(cj);
  }
  if (net.alternatives_cluster() != nullptr) {
    j["alternatives_cluster"] = net.alternatives_cluster()->name();
  }
  const SynthesisOptions& syn = net.synthesis_options();
  j["synthesis"] = {
      {"kind", syn.kind == SynthesisKind::Additive         ? "additive"
               : syn.kind == SynthesisKind::Multiplicative ? "multiplicative"
                                                           : "custom"},
      {"custom_expr", syn.custom_expr},
  };
  return j;
}

void populate_network(AnpNetwork& net, const json& j) {
  for (const auto& cj : j.at("clusters")) {
    const std::string cname = cj.at("name").get<std::string>();
    AnpCluster* existing = net.find_cluster(cname);
    AnpCluster& cluster =
        existing != nullptr ? *existing : net.add_cluster(cname);
    if (cj.contains("description")) {
      cluster.set_description(cj.at("description").get<std::string>());
    }
    if (cj.contains("x") && cj.contains("y")) {
      net.set_cluster_position(cname, cj.at("x").get<double>(),
                               cj.at("y").get<double>());
    }
    for (const auto& nj : cj.at("nodes")) {
      const std::string nname = nj.at("name").get<std::string>();
      if (net.find_node(nname) == nullptr) {
        net.add_node(cname, nname);
      }
      AnpNode& node = net.node(nname);
      if (nj.contains("description")) {
        node.set_description(nj.at("description").get<std::string>());
      }
      if (nj.contains("invert")) {
        node.set_invert(nj.at("invert").get<bool>());
      }
      if (nj.contains("x") && nj.contains("y")) {
        net.set_node_position(nname, nj.at("x").get<double>(),
                              nj.at("y").get<double>());
      }
    }
  }

  if (j.contains("alternatives_cluster")) {
    const std::string ac = j.at("alternatives_cluster").get<std::string>();
    if (net.find_cluster(ac) != nullptr) {
      net.set_alternatives_cluster(ac);
    }
  }

  for (const auto& cj : j.at("clusters")) {
    AnpCluster& cluster = net.cluster(cj.at("name").get<std::string>());
    if (cj.contains("cluster_pairwise")) {
      pairwise_from_json(cluster.cluster_pairwise(), cj.at("cluster_pairwise"));
    }
    for (const auto& nj : cj.at("nodes")) {
      AnpNode& node = net.node(nj.at("name").get<std::string>());
      if (nj.contains("node_pairwise")) {
        for (auto it = nj.at("node_pairwise").begin();
             it != nj.at("node_pairwise").end(); ++it) {
          const std::string dest_cluster = it.key();
          const auto alts =
              it.value().at("alternatives").get<std::vector<std::string>>();
          for (const std::string& dest_name : alts) {
            if (net.find_node(dest_name) != nullptr) {
              net.node_connect(node.name(), dest_name);
            }
          }
          PairwiseJudgments* pw = node.node_pairwise(dest_cluster);
          if (pw != nullptr) {
            pairwise_from_json(*pw, it.value());
          }
        }
      }
      if (nj.contains("subnetwork")) {
        AnpNetwork& sub = node.ensure_subnetwork();
        populate_network(sub, nj.at("subnetwork"));
      }
    }
  }

  if (j.contains("synthesis")) {
    SynthesisOptions syn;
    const std::string kind = j.at("synthesis").value("kind", "additive");
    if (kind == "multiplicative") {
      syn.kind = SynthesisKind::Multiplicative;
    } else if (kind == "custom") {
      syn.kind = SynthesisKind::Custom;
    } else {
      syn.kind = SynthesisKind::Additive;
    }
    syn.custom_expr = j.at("synthesis").value("custom_expr", "");
    net.set_synthesis_options(std::move(syn));
  }
}

}  // namespace

std::string network_to_json(const AnpNetwork& network) {
  json doc;
  doc["format"] = "cppanp";
  doc["version"] = 1;
  doc["network"] = network_to_json_obj(network);
  return doc.dump(2);
}

std::unique_ptr<AnpNetwork> network_from_json(const std::string& json_text) {
  json doc;
  try {
    doc = json::parse(json_text);
  } catch (const json::exception& e) {
    throw JsonIoError(std::string("JSON parse error: ") + e.what());
  }
  if (!doc.contains("network")) {
    throw JsonIoError("missing 'network' object");
  }
  if (doc.value("format", "") != "cppanp") {
    throw JsonIoError("unsupported format (expected cppanp)");
  }
  if (doc.value("version", 1) != 1) {
    throw JsonIoError("unsupported cppanp JSON version");
  }

  bool has_default_alts = false;
  for (const auto& cj : doc.at("network").at("clusters")) {
    if (cj.at("name").get<std::string>() ==
        AnpNetwork::kDefaultAlternativesCluster) {
      has_default_alts = true;
      break;
    }
  }
  auto net = std::make_unique<AnpNetwork>(has_default_alts);
  try {
    populate_network(*net, doc.at("network"));
  } catch (const json::exception& e) {
    throw JsonIoError(std::string("invalid network JSON: ") + e.what());
  } catch (const std::exception& e) {
    throw JsonIoError(e.what());
  }
  return net;
}

void save_network_file(const AnpNetwork& network, const std::string& path) {
  std::ofstream out(path);
  if (!out) {
    throw JsonIoError("cannot open file for write: " + path);
  }
  out << network_to_json(network);
}

std::unique_ptr<AnpNetwork> load_network_file(const std::string& path) {
  std::ifstream in(path);
  if (!in) {
    throw JsonIoError("cannot open file for read: " + path);
  }
  std::ostringstream ss;
  ss << in.rdbuf();
  return network_from_json(ss.str());
}

}  // namespace cppanp
