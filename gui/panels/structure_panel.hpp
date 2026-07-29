/**
 * @file structure_panel.hpp
 * @brief Tree view of clusters and nodes with subnet navigation.
 */

#pragma once

#include <QWidget>

class Document;
class QTreeWidget;
class QPushButton;
class QLabel;

/**
 * @brief Displays the network hierarchy and subnet breadcrumb trail.
 */
class StructurePanel : public QWidget {
  Q_OBJECT
public:
  /**
   * @param doc Document to display.
   * @param parent Optional parent widget.
   */
  explicit StructurePanel(Document* doc, QWidget* parent = nullptr);

public slots:
  /** @brief Rebuilds the tree from the current network view. */
  void refresh();

signals:
  /** @brief Emitted when the user selects a node in the tree. */
  void nodeSelected(const QString& name);
  /** @brief Emitted when the user selects a cluster in the tree. */
  void clusterSelected(const QString& name);

private:
  Document* doc_ = nullptr;
  QTreeWidget* tree_ = nullptr;
  QLabel* crumb_ = nullptr;
};
