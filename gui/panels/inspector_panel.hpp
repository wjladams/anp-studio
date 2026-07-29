/**
 * @file inspector_panel.hpp
 * @brief Property inspector for the selected cluster or node.
 */

#pragma once

#include <QWidget>

class Document;
class QLineEdit;
class QCheckBox;
class QLabel;
class QPushButton;

/**
 * @brief Property inspector for the selected cluster or node.
 */
class InspectorPanel : public QWidget {
  Q_OBJECT
public:
  explicit InspectorPanel(Document* doc, QWidget* parent = nullptr);

public slots:
  void refresh();

private:
  void onInvertToggled(bool checked);
  void onSetAlternatives();
  void onOpenSubnet();

  Document* doc_ = nullptr;
  QLabel* title_ = nullptr;
  QLabel* nameLabel_ = nullptr;
  QCheckBox* invert_ = nullptr;
  QPushButton* setAlts_ = nullptr;
  QPushButton* openSubnet_ = nullptr;
  bool updating_ = false;
};
