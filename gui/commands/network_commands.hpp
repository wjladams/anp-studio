#pragma once

#include <QUndoCommand>
#include <QString>

#include "document.hpp"

class AddClusterCmd : public QUndoCommand {
public:
  AddClusterCmd(Document* doc, QString name);
  void redo() override;
  void undo() override;

private:
  Document* doc_;
  QString name_;
};

class AddNodeCmd : public QUndoCommand {
public:
  AddNodeCmd(Document* doc, QString cluster, QString name);
  void redo() override;
  void undo() override;

private:
  Document* doc_;
  QString cluster_;
  QString name_;
};

class ConnectNodesCmd : public QUndoCommand {
public:
  ConnectNodesCmd(Document* doc, QString src, QString dest);
  void redo() override;
  void undo() override;

private:
  Document* doc_;
  QString src_;
  QString dest_;
};

class DisconnectNodesCmd : public QUndoCommand {
public:
  DisconnectNodesCmd(Document* doc, QString src, QString dest);
  void redo() override;
  void undo() override;

private:
  Document* doc_;
  QString src_;
  QString dest_;
};

class SetNodeComparisonCmd : public QUndoCommand {
public:
  SetNodeComparisonCmd(Document* doc,
                       QString wrt,
                       QString a,
                       QString b,
                       double value,
                       double oldValue);
  void redo() override;
  void undo() override;

private:
  Document* doc_;
  QString wrt_, a_, b_;
  double value_;
  double oldValue_;
};

class SetClusterComparisonCmd : public QUndoCommand {
public:
  SetClusterComparisonCmd(Document* doc,
                          QString wrt,
                          QString a,
                          QString b,
                          double value,
                          double oldValue);
  void redo() override;
  void undo() override;

private:
  Document* doc_;
  QString wrt_, a_, b_;
  double value_;
  double oldValue_;
};

class SetSynthesisOptionsCmd : public QUndoCommand {
public:
  SetSynthesisOptionsCmd(Document* doc,
                         cppanp::SynthesisOptions neu,
                         cppanp::SynthesisOptions old);
  void redo() override;
  void undo() override;

private:
  Document* doc_;
  cppanp::SynthesisOptions neu_;
  cppanp::SynthesisOptions old_;
};

class SetInvertCmd : public QUndoCommand {
public:
  SetInvertCmd(Document* doc, QString node, bool value);
  void redo() override;
  void undo() override;

private:
  Document* doc_;
  QString node_;
  bool value_;
  bool oldValue_ = false;
};

class SetAlternativesClusterCmd : public QUndoCommand {
public:
  SetAlternativesClusterCmd(Document* doc, QString name, QString oldName);
  void redo() override;
  void undo() override;

private:
  Document* doc_;
  QString name_;
  QString oldName_;
};

class EnsureSubnetCmd : public QUndoCommand {
public:
  EnsureSubnetCmd(Document* doc, QString node);
  void redo() override;
  void undo() override;

private:
  Document* doc_;
  QString node_;
  bool created_ = false;
};

class ReorderNodeCmd : public QUndoCommand {
public:
  ReorderNodeCmd(Document* doc, QString node, int fromIndex, int toIndex);
  void redo() override;
  void undo() override;

private:
  Document* doc_;
  QString node_;
  int fromIndex_;
  int toIndex_;
};
