/**
 * @file cluster_layout.hpp
 * @brief Topology-aware layout of cluster windows (DAG ranks or circle).
 */

#pragma once

#include <QHash>
#include <QPointF>
#include <QSizeF>
#include <QString>

namespace anpcpp {
class AnpNetwork;
}

namespace cluster_layout {

/** @brief Default gap between cluster AABBs (scene units). */
inline constexpr qreal kGap = 48.0;
/** @brief Extra horizontal space between DAG ranks. */
inline constexpr qreal kRankGap = 100.0;
/** @brief Origin for the top-left of the layout bounding area. */
inline constexpr qreal kOriginX = 40.0;
inline constexpr qreal kOriginY = 40.0;

/**
 * @brief Computes scene top-left positions for every cluster in @p net.
 *
 * Builds a directed cluster meta-graph inferred from node connections
 * (edge A→B when any node in A links into B; self-loops ignored). Acyclic
 * graphs get a left-to-right ranked layout with same-rank ties stacked
 * vertically. Graphs with a cycle get a circular arrangement sized so
 * AABBs do not overlap.
 *
 * @param net Current view network.
 * @param sizes Optional explicit sizes keyed by cluster name. Missing entries
 *        are estimated from node counts using ClusterItem geometry constants.
 * @return Map of cluster name → scene top-left. Empty if @p net has no clusters.
 */
[[nodiscard]] QHash<QString, QPointF> organize(
    const anpcpp::AnpNetwork& net,
    const QHash<QString, QSizeF>& sizes = {});

/**
 * @brief Estimates cluster window size from node count (matches ClusterItem).
 */
[[nodiscard]] QSizeF estimateSize(int nodeCount);

}  // namespace cluster_layout
