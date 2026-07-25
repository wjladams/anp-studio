#pragma once

#include <QWidget>

class Document;
class QComboBox;
class QTableWidget;
class QLabel;
class QRadioButton;

class PairwisePanel : public QWidget {
  Q_OBJECT
public:
  explicit PairwisePanel(Document* doc, QWidget* parent = nullptr);

public slots:
  void refresh();
  void selectNodeParent(const QString& name);
  void selectClusterParent(const QString& name);

private slots:
  void onParentModeChanged();
  void onParentChanged(int index);
  void onDestClusterChanged(int index);
  void onCellChanged(int row, int col);

private:
  void rebuildTable();
  [[nodiscard]] bool nodeMode() const;

  Document* doc_ = nullptr;
  QRadioButton* nodeMode_ = nullptr;
  QRadioButton* clusterMode_ = nullptr;
  QComboBox* parentBox_ = nullptr;
  QComboBox* destClusterBox_ = nullptr;
  QTableWidget* table_ = nullptr;
  QLabel* info_ = nullptr;
  bool updating_ = false;
};
