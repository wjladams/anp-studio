/**
 * @file structure_panel.hpp
 * @brief Tree view of clusters and nodes.
 */

#pragma once

#include <QWidget>

class Document;
class QTreeWidget;

/**
 * @brief Hierarchy browser for clusters and nodes (Structure stage).
 */
class StructurePanel : public QWidget {
  Q_OBJECT
public:
  explicit StructurePanel(Document* doc, QWidget* parent = nullptr);

public slots:
  void refresh();

signals:
  void nodeSelected(const QString& name);
  void clusterSelected(const QString& name);

private:
  void onItemClicked();
  void onContextMenu(const QPoint& pos);
  void syncSelectionFromDoc();

  Document* doc_ = nullptr;
  QTreeWidget* tree_ = nullptr;
  bool updating_ = false;
};
