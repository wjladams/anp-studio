/**
 * @file cluster_layout.hpp
 * @brief Topology-aware layout of cluster windows (DAG ranks or circle).
 */

#pragma once

#include <QHash>
#include <QPair>
#include <QPointF>
#include <QSizeF>
#include <QString>
#include <QVector>

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
 * @brief Infers directed cluster→cluster edges from node connections.
 *
 * Edge A→B exists when any node in A has a prioritizer into B (B ≠ A).
 * Self-loops are ignored. Derived from node prioritizers (not cluster_pairwise).
 *
 * @return Unique (from, to) cluster name pairs.
 */
[[nodiscard]] QVector<QPair<QString, QString>> metaEdges(
    const anpcpp::AnpNetwork& net);

/**
 * @brief Computes scene top-left positions for every cluster in @p net.
 *
 * Builds a directed cluster meta-graph via @ref metaEdges. Acyclic graphs get
 * a left-to-right ranked layout with same-rank ties stacked vertically.
 * Graphs with a cycle get a circular arrangement sized so AABBs do not overlap.
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
