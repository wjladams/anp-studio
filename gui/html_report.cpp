/**
 * @file html_report.cpp
 * @brief Qt-safe HTML report styling shared by Analysis and Researcher.
 */

#include "html_report.hpp"

namespace {

QString stripeBg(std::size_t rowIndex) {
  // Alternating rows (Qt has no :nth-child support we can rely on).
  return (rowIndex % 2 == 0) ? QStringLiteral("#ffffff")
                             : QStringLiteral("#f8f9fa");
}

QString styleBlock() {
  // Colors match gui/resources/app.qss. Stick to Qt rich-text CSS properties:
  // color, background-color, font-*, padding, margin, border, text-align,
  // white-space. Avoid flex/grid/:hover/:nth-child.
  return QStringLiteral(
      "<style type='text/css'>"
      "body {"
      "  font-family: 'Ubuntu', 'Cantarell', 'Noto Sans', sans-serif;"
      "  font-size: 13px;"
      "  color: #202124;"
      "  background-color: #ffffff;"
      "  margin: 12px;"
      "}"
      "h1 {"
      "  font-size: 18px;"
      "  font-weight: 600;"
      "  color: #202124;"
      "  margin-top: 18px;"
      "  margin-bottom: 8px;"
      "  padding-bottom: 4px;"
      "  border-bottom: 1px solid #dadce0;"
      "}"
      "h2 {"
      "  font-size: 15px;"
      "  font-weight: 600;"
      "  color: #202124;"
      "  margin-top: 14px;"
      "  margin-bottom: 6px;"
      "}"
      "h3 {"
      "  font-size: 14px;"
      "  font-weight: 600;"
      "  color: #3c4043;"
      "  margin-top: 12px;"
      "  margin-bottom: 6px;"
      "}"
      "p, li {"
      "  color: #3c4043;"
      "  line-height: 1.45;"
      "}"
      "a {"
      "  color: #1a73e8;"
      "  text-decoration: none;"
      "}"
      "code {"
      "  font-family: 'Ubuntu Mono', 'Consolas', 'Courier New', monospace;"
      "  font-size: 12px;"
      "  color: #202124;"
      "  background-color: #f1f3f4;"
      "  padding: 1px 5px;"
      "}"
      "ul, ol {"
      "  margin-top: 4px;"
      "  margin-bottom: 8px;"
      "}"
      "table.report {"
      "  border-collapse: collapse;"
      "  border: 1px solid #dadce0;"
      "  margin: 6px 0 14px 0;"
      "  background-color: #ffffff;"
      "}"
      "table.report th {"
      "  background-color: #f1f3f4;"
      "  color: #5f6368;"
      "  font-weight: 600;"
      "  font-size: 12px;"
      "  text-align: left;"
      "  padding: 7px 10px;"
      "  border: 1px solid #dadce0;"
      "}"
      "table.report td {"
      "  color: #202124;"
      "  font-size: 12px;"
      "  padding: 6px 10px;"
      "  border: 1px solid #e8eaed;"
      "}"
      "table.report td.num, table.report th.num {"
      "  text-align: right;"
      "  font-family: 'Ubuntu Mono', 'Consolas', 'Courier New', monospace;"
      "  white-space: nowrap;"
      "}"
      "table.report th.stub {"
      "  background-color: #f8f9fa;"
      "  color: #202124;"
      "  font-weight: 600;"
      "  text-align: left;"
      "  white-space: nowrap;"
      "}"
      "p.err {"
      "  color: #b3261e;"
      "  background-color: #fce8e6;"
      "  padding: 8px 10px;"
      "  border: 1px solid #f5c6c2;"
      "}"
      "p.muted, i.muted {"
      "  color: #5f6368;"
      "}"
      "</style>");
}

}  // namespace

namespace HtmlReport {

QString escape(const QString& s) {
  QString o = s;
  o.replace(QLatin1Char('&'), QStringLiteral("&amp;"));
  o.replace(QLatin1Char('<'), QStringLiteral("&lt;"));
  o.replace(QLatin1Char('>'), QStringLiteral("&gt;"));
  o.replace(QLatin1Char('"'), QStringLiteral("&quot;"));
  return o;
}

QString formatNumber(double v, int decimals) {
  return QString::number(v, 'f', decimals);
}

QString wrapDocument(const QString& body) {
  return QStringLiteral("<html><head>") + styleBlock() +
         QStringLiteral("</head><body>") + body +
         QStringLiteral("</body></html>");
}

QString errorParagraph(const QString& message) {
  return QStringLiteral("<p class='err'><b>Error:</b> ") + escape(message) +
         QStringLiteral("</p>");
}

QString tableBegin() {
  return QStringLiteral(
      "<table class='report' cellspacing='0' cellpadding='0'>");
}

QString tableEnd() { return QStringLiteral("</table>"); }

QString headerCell(const QString& text, bool empty) {
  if (empty) return QStringLiteral("<th></th>");
  return QStringLiteral("<th>") + escape(text) + QStringLiteral("</th>");
}

QString rowBegin(std::size_t /*rowIndex*/) { return QStringLiteral("<tr>"); }

QString rowEnd() { return QStringLiteral("</tr>"); }

QString stubCell(const QString& text, std::size_t rowIndex) {
  return QStringLiteral("<th class='stub' style='background-color:") +
         stripeBg(rowIndex) + QStringLiteral(";'>") + escape(text) +
         QStringLiteral("</th>");
}

QString dataCell(const QString& text, std::size_t rowIndex) {
  return QStringLiteral("<td style='background-color:") + stripeBg(rowIndex) +
         QStringLiteral(";'>") + escape(text) + QStringLiteral("</td>");
}

QString dataCell(double value, std::size_t rowIndex, int decimals) {
  return QStringLiteral("<td class='num' style='background-color:") +
         stripeBg(rowIndex) + QStringLiteral(";'>") +
         formatNumber(value, decimals) + QStringLiteral("</td>");
}

QString matrixTable(const anpcpp::Matrix& m,
                    const std::vector<std::string>& rowLabels,
                    const std::vector<std::string>& colLabels,
                    int decimals) {
  QString html = tableBegin();
  html += QStringLiteral("<tr>");
  html += headerCell(QString(), true);
  for (const auto& c : colLabels) {
    html += QStringLiteral("<th class='num'>") +
            escape(QString::fromStdString(c)) + QStringLiteral("</th>");
  }
  // If colLabels shorter than matrix, pad.
  for (std::size_t j = colLabels.size(); j < m.cols(); ++j) {
    html += QStringLiteral("<th class='num'>?</th>");
  }
  html += QStringLiteral("</tr>");

  for (std::size_t i = 0; i < m.rows(); ++i) {
    html += rowBegin(i);
    const QString label = QString::fromStdString(
        i < rowLabels.size() ? rowLabels[i] : "?");
    html += stubCell(label, i);
    for (std::size_t j = 0; j < m.cols(); ++j) {
      html += dataCell(m(i, j), i, decimals);
    }
    html += rowEnd();
  }
  html += tableEnd();
  return html;
}

QString vectorTable(const anpcpp::Vector& v,
                    const std::vector<std::string>& labels,
                    const QString& labelHeader,
                    const QString& valueHeader,
                    int decimals) {
  QString html = tableBegin();
  html += QStringLiteral("<tr>");
  html += headerCell(labelHeader);
  html += QStringLiteral("<th class='num'>") + escape(valueHeader) +
          QStringLiteral("</th>");
  html += QStringLiteral("</tr>");
  for (std::size_t i = 0; i < v.size(); ++i) {
    html += rowBegin(i);
    html += stubCell(
        QString::fromStdString(i < labels.size() ? labels[i] : "?"), i);
    html += dataCell(v[i], i, decimals);
    html += rowEnd();
  }
  html += tableEnd();
  return html;
}

QString nameList(const std::vector<std::string>& names,
                 const QString& heading) {
  QString html;
  if (!heading.isEmpty()) {
    html += QStringLiteral("<h3>") + escape(heading) + QStringLiteral("</h3>");
  }
  if (names.empty()) {
    html += QStringLiteral("<p class='muted'><i class='muted'>(none)</i></p>");
    return html;
  }
  html += QStringLiteral("<ol>");
  for (const auto& n : names) {
    html += QStringLiteral("<li>") + escape(QString::fromStdString(n)) +
            QStringLiteral("</li>");
  }
  html += QStringLiteral("</ol>");
  return html;
}

}  // namespace HtmlReport
