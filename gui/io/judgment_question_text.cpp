/**
 * @file judgment_question_text.cpp
 * @brief Shared respondent-facing judgment question / section copy.
 */

#include "io/judgment_question_text.hpp"

QString pairwiseComparisonText(const QString& wrt, const QString& destCluster,
                               const QString& altA, const QString& altB) {
  return QStringLiteral(
             "W.r.t. %1 → %2: how much more important is %3 than %4?")
      .arg(wrt, destCluster, altA, altB);
}

QString clusterPairwiseComparisonText(const QString& cluster,
                                      const QString& altA,
                                      const QString& altB) {
  return QStringLiteral("Cluster pairwise under %1: %2 vs %3")
      .arg(cluster, altA, altB);
}

QString ratingsComparisonText(const QString& wrt, const QString& destCluster,
                              const QString& alt, const QString& valueHint) {
  if (!valueHint.isEmpty()) {
    return QStringLiteral("W.r.t. %1 → %2: rate %3 (%4)")
        .arg(wrt, destCluster, alt, valueHint);
  }
  return QStringLiteral("W.r.t. %1 → %2: rate %3").arg(wrt, destCluster, alt);
}

QString nodeSectionTitle(const QString& wrt, const QString& destCluster) {
  return QStringLiteral("Under %1 → %2").arg(wrt, destCluster);
}

QString clusterSectionTitle(const QString& cluster) {
  return QStringLiteral("Under cluster %1").arg(cluster);
}
