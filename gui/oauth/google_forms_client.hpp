/**
 * @file google_forms_client.hpp
 * @brief Create / import Google Forms for ANP multi-user judgments.
 */

#pragma once

#include <QString>
#include <QStringList>

class Document;
class GoogleOAuth;
namespace anpcpp {
class AnpNetwork;
}

struct GoogleFormCreateResult {
  bool ok = false;
  QString formId;
  QString editUrl;
  QString responderUrl;
  QString error;
  int questionCount = 0;
  /** Fingerprint of judgment structure at create time (also stored on link). */
  QString structureFingerprint;
  /** Judgment [anp:] tags only (no respondent fields). */
  QStringList questionTags;
  /** Forms questionIds for identity + judgment items (create order). */
  QStringList questionIds;
  /** Tags parallel to @c questionIds (includes respondent / email). */
  QStringList mappedTags;
};

struct GoogleFormImportResult {
  bool ok = false;
  QString error;
  int responsesProcessed = 0;
  int participantsCreated = 0;
  int judgmentsSet = 0;
  int judgmentsSkipped = 0;
  QStringList createdParticipantNames;
  QStringList skippedNotes;
};

/**
 * @brief [anp:…] tags that would be emitted for @p net (pairwise + ratings).
 *
 * Order is stable for a given tree walk; used for fingerprints and matching.
 */
[[nodiscard]] QStringList collectGoogleFormJudgmentTags(
    const anpcpp::AnpNetwork& net);

/**
 * @brief SHA-256 (hex) of sorted judgment tags — independent of judgment values.
 */
[[nodiscard]] QString googleFormStructureFingerprint(
    const anpcpp::AnpNetwork& net);

/**
 * @brief True if @p fingerprint matches the current model structure.
 *
 * Empty fingerprint (legacy links) is treated as not matching.
 */
[[nodiscard]] bool googleFormFingerprintMatches(
    const QString& fingerprint,
    const anpcpp::AnpNetwork& net);

/**
 * @brief Builds a Forms survey for pairwise + ratings on @p net (and subnets).
 *
 * Titles are plain English (same copy as Excel templates). [anp:…] tags are
 * stored via questionId mapping for import; legacy title-embedded tags still
 * work. Pairwise uses Saaty radios; categorical ratings use labels; numeric
 * ratings use short text.
 */
[[nodiscard]] GoogleFormCreateResult createGoogleFormForNetwork(
    GoogleOAuth& oauth,
    const anpcpp::AnpNetwork& net,
    const QString& formTitle);

/**
 * @brief Imports all responses from @p formId into @p doc.
 *
 * Matches respondents to participants by email and/or name (case-insensitive).
 * Creates missing participants automatically, then writes pairwise/ratings via
 * per-user setters and rebuilds effective judgments.
 *
 * When @p matchingOnly is true, only answers whose [anp:] tags still exist in
 * the current model structure are applied; others are skipped with notes.
 */
[[nodiscard]] GoogleFormImportResult importGoogleFormResponses(
    GoogleOAuth& oauth,
    Document& doc,
    const QString& formId,
    bool matchingOnly = false);
