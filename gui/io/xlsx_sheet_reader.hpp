/**
 * @file xlsx_sheet_reader.hpp
 * @brief Minimal .xlsx sheet reader (ZIP + OOXML) for judgment template import.
 */

#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

struct XlsxSheet {
  QString name;
  QVector<QStringList> rows;
};

/**
 * @brief Loads all worksheets from an .xlsx workbook into string grids.
 * @return Empty on failure; @p error receives a message when non-null.
 */
[[nodiscard]] QVector<XlsxSheet> readXlsxSheets(const QString& path,
                                                QString* error = nullptr);
