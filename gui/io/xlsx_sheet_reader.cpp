/**
 * @file xlsx_sheet_reader.cpp
 * @brief Minimal OOXML spreadsheet reader for judgment templates.
 */

#include "io/xlsx_sheet_reader.hpp"

#include <QFile>
#include <QHash>
#include <QRegularExpression>
#include <QXmlStreamReader>

#include <zlib.h>

#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

namespace {

bool inflateRaw(const QByteArray& src, quint32 uncompressedSize,
                QByteArray* out) {
  if (out == nullptr) return false;
  out->resize(static_cast<int>(uncompressedSize));
  z_stream strm;
  std::memset(&strm, 0, sizeof(strm));
  // ZIP uses raw deflate (-MAX_WBITS) for method 8.
  if (inflateInit2(&strm, -MAX_WBITS) != Z_OK) return false;
  strm.next_in =
      reinterpret_cast<Bytef*>(const_cast<char*>(src.data()));
  strm.avail_in = static_cast<uInt>(src.size());
  strm.next_out = reinterpret_cast<Bytef*>(out->data());
  strm.avail_out = static_cast<uInt>(out->size());
  const int rc = inflate(&strm, Z_FINISH);
  inflateEnd(&strm);
  if (rc != Z_STREAM_END && rc != Z_OK) return false;
  out->resize(static_cast<int>(strm.total_out));
  return true;
}

struct ZipEntry {
  QString name;
  QByteArray data;
};

bool readZipEntries(const QByteArray& zipBytes, QHash<QString, QByteArray>* out,
                    QString* error) {
  if (out == nullptr) return false;
  out->clear();
  int pos = 0;
  const auto need = [&](int n) -> bool {
    return pos >= 0 && pos + n <= zipBytes.size();
  };
  while (pos + 30 <= zipBytes.size()) {
    const auto u16 = [&](int off) -> quint16 {
      const uchar* p =
          reinterpret_cast<const uchar*>(zipBytes.constData() + off);
      return static_cast<quint16>(p[0] | (p[1] << 8));
    };
    const auto u32 = [&](int off) -> quint32 {
      const uchar* p =
          reinterpret_cast<const uchar*>(zipBytes.constData() + off);
      return static_cast<quint32>(p[0] | (p[1] << 8) | (p[2] << 16) |
                                  (p[3] << 24));
    };
    if (u32(pos) != 0x04034b50u) break;  // local file header
    const quint16 method = u16(pos + 8);
    const quint32 compSize = u32(pos + 18);
    const quint32 uncompSize = u32(pos + 22);
    const quint16 nameLen = u16(pos + 26);
    const quint16 extraLen = u16(pos + 28);
    if (!need(30 + nameLen + extraLen)) {
      if (error) *error = QStringLiteral("Truncated ZIP local header");
      return false;
    }
    const QString name =
        QString::fromUtf8(zipBytes.constData() + pos + 30, nameLen);
    pos += 30 + nameLen + extraLen;
    if (!need(static_cast<int>(compSize))) {
      if (error) *error = QStringLiteral("Truncated ZIP entry: %1").arg(name);
      return false;
    }
    const QByteArray payload = zipBytes.mid(pos, static_cast<int>(compSize));
    pos += static_cast<int>(compSize);

    QByteArray data;
    if (method == 0) {
      data = payload;
    } else if (method == 8) {
      if (!inflateRaw(payload, uncompSize, &data)) {
        if (error)
          *error = QStringLiteral("Failed to inflate ZIP entry: %1").arg(name);
        return false;
      }
    } else {
      // Skip unsupported methods.
      continue;
    }
    out->insert(name, data);
  }
  return true;
}

int colLettersToIndex(const QString& cellRef) {
  // "AB12" → column index 28 (1-based letters → 0-based index)
  int col = 0;
  for (const QChar c : cellRef) {
    if (!c.isLetter()) break;
    col = col * 26 + (c.toUpper().unicode() - 'A' + 1);
  }
  return col - 1;
}

QStringList parseSharedStrings(const QByteArray& xml) {
  QStringList strings;
  QXmlStreamReader reader(xml);
  QString current;
  bool inSi = false;
  bool inT = false;
  while (!reader.atEnd()) {
    reader.readNext();
    if (reader.isStartElement()) {
      if (reader.name() == QLatin1String("si")) {
        inSi = true;
        current.clear();
      } else if (inSi && reader.name() == QLatin1String("t")) {
        inT = true;
      }
    } else if (reader.isCharacters() && inT) {
      current += reader.text();
    } else if (reader.isEndElement()) {
      if (reader.name() == QLatin1String("t")) {
        inT = false;
      } else if (reader.name() == QLatin1String("si")) {
        strings << current;
        inSi = false;
      }
    }
  }
  return strings;
}

QVector<QStringList> parseSheetRows(const QByteArray& xml,
                                    const QStringList& shared) {
  // Map (row, col) → value, then emit dense rows.
  QHash<qint64, QString> cells;
  int maxRow = -1;
  int maxCol = -1;

  QXmlStreamReader reader(xml);
  QString cellRef;
  QString cellType;
  QString valueText;
  bool inV = false;
  bool inIsT = false;

  while (!reader.atEnd()) {
    reader.readNext();
    if (reader.isStartElement()) {
      if (reader.name() == QLatin1String("c")) {
        cellRef = reader.attributes().value(QLatin1String("r")).toString();
        cellType = reader.attributes().value(QLatin1String("t")).toString();
        valueText.clear();
      } else if (reader.name() == QLatin1String("v")) {
        inV = true;
      } else if (reader.name() == QLatin1String("t")) {
        // inlineStr
        inIsT = true;
      }
    } else if (reader.isCharacters()) {
      if (inV || inIsT) valueText += reader.text();
    } else if (reader.isEndElement()) {
      if (reader.name() == QLatin1String("v")) {
        inV = false;
      } else if (reader.name() == QLatin1String("t")) {
        inIsT = false;
      } else if (reader.name() == QLatin1String("c") && !cellRef.isEmpty()) {
        QString val = valueText.trimmed();
        if (cellType == QLatin1String("s")) {
          bool ok = false;
          const int idx = val.toInt(&ok);
          if (ok && idx >= 0 && idx < shared.size()) val = shared[idx];
        }
        // Parse row number from cellRef (letters then digits).
        int i = 0;
        while (i < cellRef.size() && cellRef[i].isLetter()) ++i;
        const int row = cellRef.mid(i).toInt() - 1;
        const int col = colLettersToIndex(cellRef);
        if (row >= 0 && col >= 0) {
          cells.insert(static_cast<qint64>(row) << 32 | col, val);
          if (row > maxRow) maxRow = row;
          if (col > maxCol) maxCol = col;
        }
        cellRef.clear();
        cellType.clear();
        valueText.clear();
      }
    }
  }

  QVector<QStringList> rows;
  if (maxRow < 0) return rows;
  rows.resize(maxRow + 1);
  for (int r = 0; r <= maxRow; ++r) {
    QStringList line;
    line.reserve(maxCol + 1);
    for (int c = 0; c <= maxCol; ++c) {
      line << cells.value(static_cast<qint64>(r) << 32 | c);
    }
    rows[r] = std::move(line);
  }
  return rows;
}

QList<QPair<QString, QString>> parseWorkbookSheets(const QByteArray& workbook,
                                                   const QByteArray& rels) {
  // sheet name → Target path from rels (e.g. worksheets/sheet1.xml)
  QHash<QString, QString> ridToTarget;
  {
    QXmlStreamReader reader(rels);
    while (!reader.atEnd()) {
      reader.readNext();
      if (reader.isStartElement() && reader.name() == QLatin1String("Relationship")) {
        const auto attrs = reader.attributes();
        const QString id = attrs.value(QLatin1String("Id")).toString();
        QString target = attrs.value(QLatin1String("Target")).toString();
        if (target.startsWith(QLatin1String("/"))) target.remove(0, 1);
        if (target.startsWith(QLatin1String("xl/"))) {
          // ok
        } else {
          target = QStringLiteral("xl/") + target;
        }
        ridToTarget.insert(id, target);
      }
    }
  }

  QList<QPair<QString, QString>> sheets;
  QXmlStreamReader reader(workbook);
  while (!reader.atEnd()) {
    reader.readNext();
    if (reader.isStartElement() && reader.name() == QLatin1String("sheet")) {
      const auto attrs = reader.attributes();
      const QString name = attrs.value(QLatin1String("name")).toString();
      const QString rid =
          attrs.value(QLatin1String("r:id")).toString().isEmpty()
              ? attrs.value(QLatin1String("id")).toString()
              : attrs.value(QLatin1String("r:id")).toString();
      // OOXML uses r:id in the r namespace.
      QString id = attrs.value(QStringLiteral("http://schemas.openxmlformats.org/officeDocument/2006/relationships"),
                               QLatin1String("id")).toString();
      if (id.isEmpty()) id = rid;
      if (id.isEmpty()) {
        // Fallback: try any attribute ending with id
        for (const QXmlStreamAttribute& a : attrs) {
          if (a.name() == QLatin1String("id")) {
            id = a.value().toString();
            break;
          }
        }
      }
      const QString target = ridToTarget.value(id);
      if (!name.isEmpty() && !target.isEmpty()) {
        sheets.append({name, target});
      }
    }
  }
  return sheets;
}

}  // namespace

QVector<XlsxSheet> readXlsxSheets(const QString& path, QString* error) {
  QVector<XlsxSheet> out;
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    if (error) *error = QStringLiteral("Could not open %1").arg(path);
    return out;
  }
  const QByteArray zipBytes = file.readAll();
  QHash<QString, QByteArray> entries;
  if (!readZipEntries(zipBytes, &entries, error)) return out;

  const QByteArray sharedXml =
      entries.value(QStringLiteral("xl/sharedStrings.xml"));
  const QStringList shared = sharedXml.isEmpty()
                                 ? QStringList{}
                                 : parseSharedStrings(sharedXml);

  const QByteArray workbook = entries.value(QStringLiteral("xl/workbook.xml"));
  const QByteArray rels =
      entries.value(QStringLiteral("xl/_rels/workbook.xml.rels"));
  if (workbook.isEmpty() || rels.isEmpty()) {
    if (error) *error = QStringLiteral("Not a valid .xlsx workbook: %1").arg(path);
    return out;
  }

  const auto sheets = parseWorkbookSheets(workbook, rels);
  for (const auto& sheet : sheets) {
    const QByteArray sheetXml = entries.value(sheet.second);
    if (sheetXml.isEmpty()) continue;
    XlsxSheet s;
    s.name = sheet.first;
    s.rows = parseSheetRows(sheetXml, shared);
    out.push_back(std::move(s));
  }
  if (out.isEmpty() && error) {
    *error = QStringLiteral("No worksheets found in %1").arg(path);
  }
  return out;
}
