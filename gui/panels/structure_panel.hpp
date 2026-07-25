#pragma once

#include <QWidget>

class Document;
class QTreeWidget;
class QPushButton;
class QLabel;

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
  Document* doc_ = nullptr;
  QTreeWidget* tree_ = nullptr;
  QLabel* crumb_ = nullptr;
};
