/**
 * @file settings_dialog.hpp
 * @brief App settings dialog with a left category list and detail pane.
 */

#pragma once

#include <QDialog>

class GoogleOAuth;
class QListWidget;
class QStackedWidget;

/**
 * @brief Standard settings shell: categories on the left, page content on the right.
 *
 * Currently hosts Connected accounts; more categories can be added to the list
 * and stack without changing the File → Settings entry point.
 */
class SettingsDialog : public QDialog {
  Q_OBJECT
public:
  enum class Page { ConnectedAccounts = 0 };

  explicit SettingsDialog(GoogleOAuth* oauth, QWidget* parent = nullptr);

  /** @brief Selects a category page (e.g. when opened from a deep link). */
  void setCurrentPage(Page page);

private:
  QListWidget* nav_ = nullptr;
  QStackedWidget* pages_ = nullptr;
};
