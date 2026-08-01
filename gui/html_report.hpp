/**
 * @file html_report.hpp
 * @brief Shared Qt-safe HTML helpers for QTextBrowser report panes.
 *
 * Styles stay within Qt's rich-text CSS subset (no Bootstrap / modern layout).
 * Colors align with gui/resources/app.qss.
 */

#pragma once

#include <QString>
#include <string>
#include <vector>

#include "anpcpp/matrix.hpp"

namespace HtmlReport {

/** Escape text for HTML body content. */
[[nodiscard]] QString escape(const QString& s);

/** Format a number for table cells. */
[[nodiscard]] QString formatNumber(double v, int decimals = 4);

/**
 * @brief Wrap @p body in a full HTML document with shared report CSS.
 */
[[nodiscard]] QString wrapDocument(const QString& body);

/** Error paragraph (red accent). */
[[nodiscard]] QString errorParagraph(const QString& message);

/**
 * @brief Render a labeled matrix as a striped HTML table.
 * @param rowLabels Row header labels (size should match @p m.rows()).
 * @param colLabels Column header labels (size should match @p m.cols()).
 */
[[nodiscard]] QString matrixTable(const anpcpp::Matrix& m,
                                  const std::vector<std::string>& rowLabels,
                                  const std::vector<std::string>& colLabels,
                                  int decimals = 4);

/**
 * @brief Render a labeled vector as a two-column striped table.
 */
[[nodiscard]] QString vectorTable(const anpcpp::Vector& v,
                                  const std::vector<std::string>& labels,
                                  const QString& labelHeader = QStringLiteral(
                                      "Node"),
                                  const QString& valueHeader = QStringLiteral(
                                      "Value"),
                                  int decimals = 4);

/**
 * @brief Ordered name list with optional heading.
 */
[[nodiscard]] QString nameList(const std::vector<std::string>& names,
                               const QString& heading = QString());

/**
 * @brief Open a report table (callers append rows, then @ref tableEnd).
 *
 * Used for one-off tables (e.g. subnet synthesis) that share the same look.
 */
[[nodiscard]] QString tableBegin();
[[nodiscard]] QString tableEnd();
[[nodiscard]] QString headerCell(const QString& text, bool empty = false);
[[nodiscard]] QString rowBegin(std::size_t rowIndex);
[[nodiscard]] QString rowEnd();
[[nodiscard]] QString stubCell(const QString& text, std::size_t rowIndex);
[[nodiscard]] QString dataCell(const QString& text, std::size_t rowIndex);
[[nodiscard]] QString dataCell(double value,
                               std::size_t rowIndex,
                               int decimals = 4);

}  // namespace HtmlReport
