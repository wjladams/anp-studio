#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QUndoStack>
#include <memory>
#include <vector>

#include "cppanp/network.hpp"

// Owns the root AnpNetwork, undo stack, file path, and dirty flag.
// Subnet navigation pushes AnpNetwork* frames (owned by parent nodes).
class Document : public QObject {
  Q_OBJECT
public:
  explicit Document(QObject* parent = nullptr);

  [[nodiscard]] cppanp::AnpNetwork& network();
  [[nodiscard]] const cppanp::AnpNetwork& network() const;
  [[nodiscard]] cppanp::AnpNetwork& root();
  [[nodiscard]] const cppanp::AnpNetwork& root() const;

  [[nodiscard]] QUndoStack* undoStack() { return &undo_; }

  [[nodiscard]] QString path() const { return path_; }
  [[nodiscard]] bool isDirty() const { return dirty_; }
  void setDirty(bool dirty);

  void newNetwork(bool create_alts = true);
  bool loadFromFile(const QString& path, QString* error = nullptr);
  bool saveToFile(const QString& path, QString* error = nullptr);

  void pushSubnet(const QString& nodeName);
  void popSubnet();
  void popToRoot();
  [[nodiscard]] int subnetDepth() const;
  [[nodiscard]] QStringList breadcrumb() const;

  void notifyChanged();

signals:
  void modelChanged();
  void dirtyChanged(bool dirty);
  void pathChanged(const QString& path);
  void viewNetworkChanged();

private:
  struct Frame {
    cppanp::AnpNetwork* net = nullptr;
    QString hostNode;
  };

  void replaceRoot(std::unique_ptr<cppanp::AnpNetwork> net);

  std::unique_ptr<cppanp::AnpNetwork> root_;
  std::vector<Frame> stack_;
  QUndoStack undo_;
  QString path_;
  bool dirty_ = false;
};
