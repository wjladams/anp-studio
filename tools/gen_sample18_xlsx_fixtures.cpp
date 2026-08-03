/**
 * @file gen_sample18_xlsx_fixtures.cpp
 * @brief Regenerates samples/import/single_user Excel templates via the real exporter.
 *
 * Usage:
 *   gen_sample18_xlsx_fixtures <18_multiuser_pairwise_ahp.anpstudio> <out_dir>
 */
#include <QCoreApplication>
#include <QDir>
#include <QString>
#include <cstdio>
#include <string>

#include "document.hpp"
#include "io/judgment_template_io.hpp"

namespace {

bool setTriple(anpcpp::AnpNetwork& root, const char* uid, double ab, double ac,
               double bc, QString* error) {
  try {
    root.set_node_comparison_for(uid, "Goal", "A", "B", ab);
    root.set_node_comparison_for(uid, "Goal", "A", "C", ac);
    root.set_node_comparison_for(uid, "Goal", "B", "C", bc);
    return true;
  } catch (const std::exception& ex) {
    if (error)
      *error = QString::fromUtf8(ex.what());
    return false;
  }
}

bool writeOne(const anpcpp::AnpNetwork& root, const char* id,
              const QString& outDir, QString* error) {
  const anpcpp::JudgmentParticipant* p = root.find_participant(id);
  if (p == nullptr) {
    if (error) *error = QStringLiteral("Missing participant %1").arg(id);
    return false;
  }
  JudgmentTemplateExportOptions opt;
  opt.includeExistingVotes = true;
  const QString path = QDir(outDir).filePath(judgmentTemplateFileName(*p));
  return writeJudgmentTemplateXlsx(root, *p, path, opt, error);
}

}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  if (argc != 3) {
    std::fprintf(stderr,
                 "Usage: %s <18_multiuser_pairwise_ahp.anpstudio> <out_dir>\n",
                 argv[0]);
    return 2;
  }
  const QString samplePath = QString::fromLocal8Bit(argv[1]);
  const QString outDir = QString::fromLocal8Bit(argv[2]);
  if (!QDir().mkpath(outDir)) {
    std::fprintf(stderr, "Could not create %s\n", argv[2]);
    return 1;
  }

  Document doc;
  QString err;
  if (!doc.loadFromFile(samplePath, &err)) {
    std::fprintf(stderr, "Load failed: %s\n", qPrintable(err));
    return 1;
  }

  anpcpp::AnpNetwork& root = doc.root();
  // New vote values (different from sample 18 built-ins).
  if (!setTriple(root, "alice", 5.0, 2.0, 3.0, &err) ||
      !setTriple(root, "bob", 1.0 / 3.0, 5.0, 2.0, &err) ||
      !setTriple(root, "carol", 7.0, 1.0 / 5.0, 4.0, &err)) {
    std::fprintf(stderr, "Set votes failed: %s\n", qPrintable(err));
    return 1;
  }
  root.add_participant("eve", "Eve Morales", "eve@example.com");
  if (!setTriple(root, "eve", 4.0, 0.5, 3.0, &err)) {
    std::fprintf(stderr, "Set Eve votes failed: %s\n", qPrintable(err));
    return 1;
  }

  for (const char* id : {"alice", "bob", "carol", "eve"}) {
    if (!writeOne(root, id, outDir, &err)) {
      std::fprintf(stderr, "Export %s failed: %s\n", id, qPrintable(err));
      return 1;
    }
  }
  std::printf("Wrote Excel fixtures to %s\n", qPrintable(outDir));
  return 0;
}
