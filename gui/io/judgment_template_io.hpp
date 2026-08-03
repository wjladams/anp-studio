/**
 * @file judgment_template_io.hpp
 * @brief Per-participant judgment templates (respondent Excel + legacy CSV).
 *
 * Export writes one .xlsx per participant with a human "Your judgments" sheet
 * and a hidden "_meta" sheet. The filename may include the display name for
 * sharing, but import identifies the person from _meta cells — never the
 * filename. Legacy CSV and older flat .xlsx sheets still import.
 */

#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

class Document;
namespace anpcpp {
class AnpNetwork;
struct JudgmentParticipant;
}  // namespace anpcpp

struct JudgmentTemplateExportOptions {
  /** When false (default), rating cells are left blank for respondents to fill. */
  bool includeExistingVotes = false;
  /**
   * Empty = export every participant. Otherwise only participants whose id
   * appears in this list (unknown ids are skipped).
   */
  QStringList participantIds;
};

struct JudgmentTemplateExportResult {
  bool ok = false;
  QString error;
  QStringList writtenPaths;
  int filesWritten = 0;
};

struct JudgmentTemplateImportResult {
  bool ok = false;
  QString error;
  int filesProcessed = 0;
  int participantsCreated = 0;
  int judgmentsSet = 0;
  int judgmentsSkipped = 0;
  QStringList createdParticipantNames;
  QStringList notes;
};

/**
 * @brief Safe filename stem from a display name (e.g. "Alice Chen" → "Alice_Chen").
 */
[[nodiscard]] QString judgmentTemplateFileStem(const QString& displayName);

/**
 * @brief Suggested filename for one participant Excel template (includes .xlsx).
 */
[[nodiscard]] QString judgmentTemplateFileName(
    const anpcpp::JudgmentParticipant& participant);

/**
 * @brief Writes one respondent-friendly Excel template for @p participant.
 */
[[nodiscard]] bool writeJudgmentTemplateXlsx(
    const anpcpp::AnpNetwork& root,
    const anpcpp::JudgmentParticipant& participant,
    const QString& filePath,
    const JudgmentTemplateExportOptions& options,
    QString* error);

/**
 * @brief Writes one legacy CSV template (kept for power users / tests).
 */
[[nodiscard]] bool writeJudgmentTemplateCsv(
    const anpcpp::AnpNetwork& root,
    const anpcpp::JudgmentParticipant& participant,
    const QString& filePath,
    const JudgmentTemplateExportOptions& options,
    QString* error);

/**
 * @brief Exports one Excel template per participant into @p directory.
 */
[[nodiscard]] JudgmentTemplateExportResult exportJudgmentTemplates(
    const Document& doc,
    const QString& directory,
    const JudgmentTemplateExportOptions& options);

/**
 * @brief Imports one or more template CSV/XLSX files (multi-select).
 *
 * New Excel templates: sheets "Your judgments" + "_meta".
 * Legacy: CSV tables or flat sheets with participant_* + kind columns.
 */
[[nodiscard]] JudgmentTemplateImportResult importJudgmentTemplates(
    Document& doc,
    const QStringList& filePaths);
