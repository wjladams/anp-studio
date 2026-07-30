/**
 * @file inspector_panel.hpp
 * @brief Property inspector for the selected network, cluster, or node.
 */

#pragma once

#include <QWidget>

class Document;
class QLineEdit;
class QTextEdit;
class QCheckBox;
class QLabel;
class QPushButton;
class QComboBox;
class QTreeWidget;
class QTreeWidgetItem;
class QTableWidget;
class QEvent;

/**
 * @brief Property inspector for the selected network, cluster, or node.
 */
class InspectorPanel : public QWidget {
  Q_OBJECT
public:
  explicit InspectorPanel(Document* doc, QWidget* parent = nullptr);

public slots:
  void refresh();

protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
  void onInvertToggled(bool checked);
  void onSetAlternatives();
  void onOpenSubnet();
  void onSynthesisKindChanged(int index);
  void onCustomExprEdited();
  void onNameEdited();
  void onDescriptionEdited();
  void onSubnetItemActivated(QTreeWidgetItem* item, int column);

private:
  enum class Mode { Network, Cluster, Node };

  void refreshSynthesisControls();
  void setModeWidgets(Mode mode);
  void fillSubnetTree();
  void fillConnections();
  void fillMatrixColumns(const QString& nodeName);
  [[nodiscard]] QString displayNetworkName() const;

  Document* doc_ = nullptr;
  QLabel* title_ = nullptr;
  QLabel* typeLabel_ = nullptr;
  QLineEdit* nameEdit_ = nullptr;
  QTextEdit* descriptionEdit_ = nullptr;

  QCheckBox* invert_ = nullptr;
  QPushButton* setAlts_ = nullptr;
  QPushButton* openSubnet_ = nullptr;

  QLabel* formulaLabel_ = nullptr;
  QComboBox* synthKind_ = nullptr;
  QLineEdit* customExpr_ = nullptr;

  QLabel* subnetLabel_ = nullptr;
  QTreeWidget* subnetTree_ = nullptr;

  QLabel* connectionsLabel_ = nullptr;
  QTreeWidget* connectionsTree_ = nullptr;

  QLabel* matricesLabel_ = nullptr;
  QLabel* unscaledLabel_ = nullptr;
  QLabel* scaledLabel_ = nullptr;
  QLabel* limitLabel_ = nullptr;
  QTableWidget* unscaledCol_ = nullptr;
  QTableWidget* scaledCol_ = nullptr;
  QTableWidget* limitCol_ = nullptr;

  bool updating_ = false;
};
