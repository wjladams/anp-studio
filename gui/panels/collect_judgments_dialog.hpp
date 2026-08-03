/**
 * @file collect_judgments_dialog.hpp
 * @brief Judgments-stage hub for Excel / Google Forms / CSV collection.
 */

#pragma once

#include <QDialog>
#include <QString>

class Document;
class GoogleOAuth;
class QComboBox;
class QLabel;
class QPushButton;
class QWidget;

/**
 * @brief "Collect judgments" hub — parallel channels into the same roster.
 *
 * Excel and CSV need no OAuth; Google Forms uses the app-level connection.
 * Buttons emit requests; MainWindow runs the existing import/export actions.
 */
class CollectJudgmentsDialog : public QDialog {
  Q_OBJECT
public:
  CollectJudgmentsDialog(Document* doc, GoogleOAuth* oauth,
                         QWidget* parent = nullptr);

  /** @return Selected participant id, or empty for "All". */
  [[nodiscard]] QString selectedParticipantId() const;

signals:
  void exportExcelRequested(const QString& participantId);
  void importExcelRequested();
  void importCsvRequested();
  void createGoogleFormRequested();
  void importGoogleFormRequested();
  void openLinkedFormRequested();
  void connectGoogleRequested();

public slots:
  void refresh();

private:
  Document* doc_ = nullptr;
  GoogleOAuth* oauth_ = nullptr;

  QComboBox* participantCombo_ = nullptr;

  QLabel* googleStatus_ = nullptr;
  QWidget* googleEnabledActions_ = nullptr;
  QWidget* googleConnectActions_ = nullptr;
  QPushButton* createFormBtn_ = nullptr;
  QPushButton* importFormBtn_ = nullptr;
  QPushButton* openFormBtn_ = nullptr;
};
