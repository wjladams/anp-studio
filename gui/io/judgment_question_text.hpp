/**
 * @file judgment_question_text.hpp
 * @brief Shared respondent-facing judgment question / section copy.
 *
 * Used by Excel templates and Google Forms so wording stays in sync.
 */

#pragma once

#include <QString>

/** @brief Plain-English pairwise comparison (node prioritizer). */
[[nodiscard]] QString pairwiseComparisonText(const QString& wrt,
                                             const QString& destCluster,
                                             const QString& altA,
                                             const QString& altB);

/** @brief Plain-English cluster pairwise comparison. */
[[nodiscard]] QString clusterPairwiseComparisonText(const QString& cluster,
                                                    const QString& altA,
                                                    const QString& altB);

/**
 * @brief Plain-English ratings prompt.
 * @param valueHint Optional parenthetical (e.g. category ids); may be empty.
 */
[[nodiscard]] QString ratingsComparisonText(const QString& wrt,
                                            const QString& destCluster,
                                            const QString& alt,
                                            const QString& valueHint = {});

/** @brief Section header for node prioritizer blocks. */
[[nodiscard]] QString nodeSectionTitle(const QString& wrt,
                                       const QString& destCluster);

/** @brief Section header for cluster pairwise blocks. */
[[nodiscard]] QString clusterSectionTitle(const QString& cluster);
