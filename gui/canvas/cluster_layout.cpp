/**
 * @file cluster_layout.cpp
 * @brief Topology-aware cluster window layout (DAG hierarchy or circle).
 */

#include "canvas/cluster_layout.hpp"

#include "canvas/cluster_item.hpp"

#include "anpcpp/network.hpp"

#include <QRectF>

#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <queue>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace cluster_layout {
namespace {

constexpr qreal kPi = 3.14159265358979323846;

struct MetaEdge {
  QString from;
  QString to;
  int weight = 0;
};

QSizeF sizeFor(const QString& name,
               const anpcpp::AnpNetwork& net,
               const QHash<QString, QSizeF>& sizes) {
  if (sizes.contains(name)) return sizes.value(name);
  try {
    const auto& c = net.cluster(name.toStdString());
    return estimateSize(static_cast<int>(c.nnodes()));
  } catch (...) {
    return estimateSize(0);
  }
}

std::vector<MetaEdge> buildMetaEdges(const anpcpp::AnpNetwork& net) {
  const QVector<QPair<QString, QString>> pairs = metaEdges(net);
  std::vector<MetaEdge> edges;
  edges.reserve(static_cast<size_t>(pairs.size()));
  for (const auto& pair : pairs) {
    edges.push_back({pair.first, pair.second, /*weight=*/1});
  }
  return edges;
}

bool hasDirectedCycle(const std::vector<QString>& names,
                      const std::vector<MetaEdge>& edges) {
  std::unordered_map<std::string, std::vector<std::string>> adj;
  adj.reserve(names.size());
  for (const QString& n : names) adj[n.toStdString()];
  for (const MetaEdge& e : edges) {
    adj[e.from.toStdString()].push_back(e.to.toStdString());
  }

  enum Color { White, Gray, Black };
  std::unordered_map<std::string, Color> color;
  for (const QString& n : names) color[n.toStdString()] = White;

  bool cyclic = false;
  std::function<void(const std::string&)> dfs = [&](const std::string& u) {
    color[u] = Gray;
    for (const std::string& v : adj[u]) {
      if (!color.count(v)) continue;
      if (color[v] == Gray) {
        cyclic = true;
        return;
      }
      if (color[v] == White) {
        dfs(v);
        if (cyclic) return;
      }
    }
    color[u] = Black;
  };

  for (const QString& n : names) {
    const std::string key = n.toStdString();
    if (color[key] == White) dfs(key);
    if (cyclic) break;
  }
  return cyclic;
}

std::unordered_map<std::string, int> weightedDegree(
    const std::vector<QString>& names, const std::vector<MetaEdge>& edges) {
  std::unordered_map<std::string, int> deg;
  for (const QString& n : names) deg[n.toStdString()] = 0;
  for (const MetaEdge& e : edges) {
    deg[e.from.toStdString()] += e.weight;
    deg[e.to.toStdString()] += e.weight;
  }
  return deg;
}

QHash<QString, QPointF> layoutDag(const std::vector<QString>& names,
                                  const std::vector<MetaEdge>& edges,
                                  const anpcpp::AnpNetwork& net,
                                  const QHash<QString, QSizeF>& sizes) {
  std::unordered_map<std::string, std::vector<std::string>> adj;
  std::unordered_map<std::string, int> indeg;
  for (const QString& n : names) {
    adj[n.toStdString()];
    indeg[n.toStdString()] = 0;
  }
  for (const MetaEdge& e : edges) {
    adj[e.from.toStdString()].push_back(e.to.toStdString());
    indeg[e.to.toStdString()] += 1;
  }

  // Longest-path ranks (Kahn + relax).
  std::unordered_map<std::string, int> rank;
  for (const QString& n : names) rank[n.toStdString()] = 0;

  std::queue<std::string> q;
  std::unordered_map<std::string, int> remaining = indeg;
  for (const auto& [name, d] : indeg) {
    if (d == 0) q.push(name);
  }
  while (!q.empty()) {
    const std::string u = q.front();
    q.pop();
    for (const std::string& v : adj[u]) {
      rank[v] = std::max(rank[v], rank[u] + 1);
      if (--remaining[v] == 0) q.push(v);
    }
  }

  auto deg = weightedDegree(names, edges);

  std::map<int, std::vector<QString>> bands;
  for (const QString& n : names) {
    bands[rank[n.toStdString()]].push_back(n);
  }
  for (auto& [r, band] : bands) {
    Q_UNUSED(r);
    std::sort(band.begin(), band.end(), [&](const QString& a, const QString& b) {
      const int da = deg[a.toStdString()];
      const int db = deg[b.toStdString()];
      if (da != db) return da > db;
      return a < b;
    });
  }

  // One barycenter sweep using predecessor average ranks for crossing reduction.
  for (auto& [r, band] : bands) {
    if (r == 0 || band.size() < 2) continue;
    std::unordered_map<std::string, double> bary;
    std::unordered_map<std::string, int> count;
    for (const QString& n : band) {
      bary[n.toStdString()] = 0.0;
      count[n.toStdString()] = 0;
    }
    // Index of each node in previous band.
    std::unordered_map<std::string, int> prevIndex;
    const auto prevIt = bands.find(r - 1);
    if (prevIt != bands.end()) {
      for (int i = 0; i < static_cast<int>(prevIt->second.size()); ++i) {
        prevIndex[prevIt->second[static_cast<std::size_t>(i)].toStdString()] = i;
      }
    }
    for (const MetaEdge& e : edges) {
      if (!prevIndex.count(e.from.toStdString())) continue;
      if (!bary.count(e.to.toStdString())) continue;
      bary[e.to.toStdString()] += prevIndex[e.from.toStdString()];
      count[e.to.toStdString()] += 1;
    }
    std::sort(band.begin(), band.end(), [&](const QString& a, const QString& b) {
      const double ba =
          count[a.toStdString()] > 0
              ? bary[a.toStdString()] / count[a.toStdString()]
              : static_cast<double>(band.size());
      const double bb =
          count[b.toStdString()] > 0
              ? bary[b.toStdString()] / count[b.toStdString()]
              : static_cast<double>(band.size());
      if (std::abs(ba - bb) > 1e-9) return ba < bb;
      return a < b;
    });
  }

  // Column widths and band heights.
  std::map<int, qreal> colW;
  std::map<int, qreal> bandH;
  for (const auto& [r, band] : bands) {
    qreal maxW = 0;
    qreal totalH = 0;
    for (const QString& n : band) {
      const QSizeF s = sizeFor(n, net, sizes);
      maxW = std::max(maxW, s.width());
      totalH += s.height();
    }
    if (!band.empty()) {
      totalH += kGap * static_cast<qreal>(band.size() - 1);
    }
    colW[r] = maxW;
    bandH[r] = totalH;
  }

  qreal maxBandH = 0;
  for (const auto& [r, h] : bandH) {
    Q_UNUSED(r);
    maxBandH = std::max(maxBandH, h);
  }

  QHash<QString, QPointF> out;
  qreal x = kOriginX;
  for (const auto& [r, band] : bands) {
    const qreal colHeight = bandH[r];
    qreal y = kOriginY + (maxBandH - colHeight) * 0.5;
    for (const QString& n : band) {
      const QSizeF s = sizeFor(n, net, sizes);
      // Center narrower clusters within the column.
      const qreal cx = x + (colW[r] - s.width()) * 0.5;
      out.insert(n, QPointF(cx, y));
      y += s.height() + kGap;
    }
    x += colW[r] + kRankGap;
  }
  return out;
}

bool aabbsOverlap(const QRectF& a, const QRectF& b, qreal gap) {
  return a.adjusted(-gap * 0.5, -gap * 0.5, gap * 0.5, gap * 0.5)
      .intersects(b);
}

QHash<QString, QPointF> layoutCircle(const std::vector<QString>& names,
                                     const std::vector<MetaEdge>& edges,
                                     const anpcpp::AnpNetwork& net,
                                     const QHash<QString, QSizeF>& sizes) {
  QHash<QString, QPointF> out;
  if (names.empty()) return out;

  auto deg = weightedDegree(names, edges);
  std::vector<QString> order = names;
  std::sort(order.begin(), order.end(), [&](const QString& a, const QString& b) {
    const int da = deg[a.toStdString()];
    const int db = deg[b.toStdString()];
    if (da != db) return da > db;
    return a < b;
  });

  const int n = static_cast<int>(order.size());
  if (n == 1) {
    out.insert(order[0], QPointF(kOriginX, kOriginY));
    return out;
  }

  std::vector<QSizeF> sz(static_cast<std::size_t>(n));
  qreal maxDiag = 0;
  for (int i = 0; i < n; ++i) {
    sz[static_cast<std::size_t>(i)] = sizeFor(order[static_cast<std::size_t>(i)],
                                              net, sizes);
    const QSizeF& s = sz[static_cast<std::size_t>(i)];
    maxDiag = std::max(maxDiag, std::hypot(s.width(), s.height()));
  }

  auto place = [&](qreal radius) -> QHash<QString, QPointF> {
    QHash<QString, QPointF> pos;
    // Start at top (-π/2) so the heaviest cluster sits at the top.
    for (int i = 0; i < n; ++i) {
      const qreal angle =
          -kPi / 2.0 + (2.0 * kPi * static_cast<qreal>(i)) / n;
      const QSizeF& s = sz[static_cast<std::size_t>(i)];
      const qreal cx = radius * std::cos(angle);
      const qreal cy = radius * std::sin(angle);
      pos.insert(order[static_cast<std::size_t>(i)],
                 QPointF(cx - s.width() * 0.5, cy - s.height() * 0.5));
    }
    return pos;
  };

  auto overlaps = [&](const QHash<QString, QPointF>& pos) -> bool {
    for (int i = 0; i < n; ++i) {
      const QString& a = order[static_cast<std::size_t>(i)];
      const QSizeF& sa = sz[static_cast<std::size_t>(i)];
      const QRectF ra(pos.value(a), sa);
      for (int j = i + 1; j < n; ++j) {
        const QString& b = order[static_cast<std::size_t>(j)];
        const QSizeF& sb = sz[static_cast<std::size_t>(j)];
        const QRectF rb(pos.value(b), sb);
        if (aabbsOverlap(ra, rb, kGap)) return true;
      }
    }
    return false;
  };

  // Initial radius from adjacent chord vs half-diagonals.
  qreal chord = maxDiag + kGap;
  for (int i = 0; i < n; ++i) {
    const QSizeF& a = sz[static_cast<std::size_t>(i)];
    const QSizeF& b = sz[static_cast<std::size_t>((i + 1) % n)];
    const qreal need =
        0.5 * std::hypot(a.width(), a.height()) +
        0.5 * std::hypot(b.width(), b.height()) + kGap;
    chord = std::max(chord, need);
  }
  const qreal sinHalf = std::sin(kPi / n);
  qreal radius = (sinHalf > 1e-9) ? (chord / (2.0 * sinHalf)) : chord;

  QHash<QString, QPointF> pos = place(radius);
  // Inflate until all AABB pairs clear (handles uneven heights).
  for (int iter = 0; iter < 48 && overlaps(pos); ++iter) {
    radius *= 1.12;
    pos = place(radius);
  }

  // Shift so the layout's top-left sits at the origin padding.
  qreal minX = 0;
  qreal minY = 0;
  bool first = true;
  for (auto it = pos.begin(); it != pos.end(); ++it) {
    if (first || it.value().x() < minX) minX = it.value().x();
    if (first || it.value().y() < minY) minY = it.value().y();
    first = false;
  }
  const qreal dx = kOriginX - minX;
  const qreal dy = kOriginY - minY;
  for (auto it = pos.begin(); it != pos.end(); ++it) {
    it.value() += QPointF(dx, dy);
  }
  return pos;
}

}  // namespace

QSizeF estimateSize(int nodeCount) {
  const qreal w = ClusterItem::kWidth;
  if (nodeCount <= 0) {
    return QSizeF(w, ClusterItem::kTitleH + ClusterItem::kMinBodyH);
  }
  const qreal body =
      ClusterItem::kPad +
      nodeCount * ClusterItem::kRowH +
      (nodeCount - 1) * ClusterItem::kRowGap +
      ClusterItem::kPad;
  const qreal h =
      std::max(ClusterItem::kTitleH + ClusterItem::kMinBodyH,
               ClusterItem::kTitleH + body);
  return QSizeF(w, h);
}

QVector<QPair<QString, QString>> metaEdges(const anpcpp::AnpNetwork& net) {
  // Infer cluster→cluster links from node connections: edge A→B exists when
  // any node in A has a prioritizer into B (B ≠ A). Self-loops are ignored.
  std::set<std::pair<QString, QString>> seen;
  for (const anpcpp::AnpNode* src : net.nodes()) {
    if (src == nullptr || src->cluster() == nullptr) continue;
    const QString srcC = QString::fromStdString(src->cluster()->name());
    for (const anpcpp::AnpCluster* destC : net.clusters()) {
      if (destC == nullptr) continue;
      const QString destName = QString::fromStdString(destC->name());
      if (destName == srcC) continue;
      const anpcpp::NodePrioritizerSlot* slot =
          src->node_prioritizer(destC->name());
      if (slot == nullptr || slot->empty()) continue;
      seen.insert({srcC, destName});
    }
  }
  QVector<QPair<QString, QString>> edges;
  edges.reserve(static_cast<int>(seen.size()));
  for (const auto& pair : seen) {
    edges.push_back({pair.first, pair.second});
  }
  return edges;
}

QHash<QString, QPointF> organize(const anpcpp::AnpNetwork& net,
                                 const QHash<QString, QSizeF>& sizes) {
  std::vector<QString> names;
  names.reserve(net.nclusters());
  for (const anpcpp::AnpCluster* c : net.clusters()) {
    if (c != nullptr) names.push_back(QString::fromStdString(c->name()));
  }
  if (names.empty()) return {};

  const std::vector<MetaEdge> edges = buildMetaEdges(net);
  if (hasDirectedCycle(names, edges)) {
    return layoutCircle(names, edges, net, sizes);
  }
  return layoutDag(names, edges, net, sizes);
}

}  // namespace cluster_layout
