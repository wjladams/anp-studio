#pragma once

#include <QHash>
#include <QPointF>
#include <QString>
#include <QUndoCommand>
#include <vector>

#include "anpcpp/ratings.hpp"
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

/** @brief Sets one participant's node pairwise comparison, then rebuilds. */
class SetNodeComparisonForCmd : public QUndoCommand {
public:
  SetNodeComparisonForCmd(Document* doc,
                          QString userId,
                          QString wrt,
                          QString a,
                          QString b,
                          double value,
                          double oldValue);
  void redo() override;
  void undo() override;

private:
  Document* doc_;
  QString userId_, wrt_, a_, b_;
  double value_;
  double oldValue_;
};

/** @brief Sets one participant's cluster pairwise comparison, then rebuilds. */
class SetClusterComparisonForCmd : public QUndoCommand {
public:
  SetClusterComparisonForCmd(Document* doc,
                             QString userId,
                             QString wrt,
                             QString a,
                             QString b,
                             double value,
                             double oldValue);
  void redo() override;
  void undo() override;

private:
  Document* doc_;
  QString userId_, wrt_, a_, b_;
  double value_;
  double oldValue_;
};

/** @brief Sets (or clears) one participant's categorical rating vote. */
class SetRatingVoteForCmd : public QUndoCommand {
public:
  SetRatingVoteForCmd(Document* doc,
                      QString userId,
                      QString wrt,
                      QString alt,
                      QString categoryId,
                      QString oldCategoryId);
  void redo() override;
  void undo() override;

private:
  void apply(const QString& categoryId);
  Document* doc_;
  QString userId_, wrt_, alt_, categoryId_, oldCategoryId_;
};

/** @brief Sets (or clears) one participant's numeric rating value. */
class SetRatingValueForCmd : public QUndoCommand {
public:
  SetRatingValueForCmd(Document* doc,
                       QString userId,
                       QString wrt,
                       QString alt,
                       bool clear,
                       double value,
                       bool hadOld,
                       double oldValue);
  void redo() override;
  void undo() override;

private:
  void apply(bool clear, double value);
  Document* doc_;
  QString userId_, wrt_, alt_;
  bool clear_;
  double value_;
  bool hadOld_;
  double oldValue_;
};

class SetSynthesisOptionsCmd : public QUndoCommand {
public:
  SetSynthesisOptionsCmd(Document* doc,
                         anpcpp::SynthesisOptions neu,
                         anpcpp::SynthesisOptions old);
  void redo() override;
  void undo() override;

private:
  Document* doc_;
  anpcpp::SynthesisOptions neu_;
  anpcpp::SynthesisOptions old_;
};

class SetLimitMatrixOptionsCmd : public QUndoCommand {
public:
  SetLimitMatrixOptionsCmd(Document* doc,
                           anpcpp::LimitMatrixOptions neu,
                           anpcpp::LimitMatrixOptions old);
  void redo() override;
  void undo() override;

private:
  Document* doc_;
  anpcpp::LimitMatrixOptions neu_;
  anpcpp::LimitMatrixOptions old_;
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

class RemoveNodeCmd : public QUndoCommand {
public:
  RemoveNodeCmd(Document* doc, QString name);
  void redo() override;
  void undo() override;

private:
  Document* doc_;
  QString name_;
  QString cluster_;
  bool hadNode_ = false;
};

class RemoveClusterCmd : public QUndoCommand {
public:
  RemoveClusterCmd(Document* doc, QString name);
  void redo() override;
  void undo() override;

private:
  Document* doc_;
  QString name_;
  bool hadCluster_ = false;
};

class RenameNodeCmd : public QUndoCommand {
public:
  RenameNodeCmd(Document* doc, QString oldName, QString newName);
  void redo() override;
  void undo() override;

private:
  Document* doc_;
  QString oldName_;
  QString newName_;
};

class RenameClusterCmd : public QUndoCommand {
public:
  RenameClusterCmd(Document* doc, QString oldName, QString newName);
  void redo() override;
  void undo() override;

private:
  Document* doc_;
  QString oldName_;
  QString newName_;
};

class SetNetworkNameCmd : public QUndoCommand {
public:
  SetNetworkNameCmd(Document* doc, QString name);
  void redo() override;
  void undo() override;

private:
  Document* doc_;
  QString name_;
  QString oldName_;
};

class SetNetworkDescriptionCmd : public QUndoCommand {
public:
  SetNetworkDescriptionCmd(Document* doc, QString description);
  void redo() override;
  void undo() override;

private:
  Document* doc_;
  QString description_;
  QString oldDescription_;
};

class SetNodeDescriptionCmd : public QUndoCommand {
public:
  SetNodeDescriptionCmd(Document* doc, QString node, QString description);
  void redo() override;
  void undo() override;

private:
  Document* doc_;
  QString node_;
  QString description_;
  QString oldDescription_;
};

class SetClusterDescriptionCmd : public QUndoCommand {
public:
  SetClusterDescriptionCmd(Document* doc, QString cluster, QString description);
  void redo() override;
  void undo() override;

private:
  Document* doc_;
  QString cluster_;
  QString description_;
  QString oldDescription_;
};

class SetPrioritizerKindCmd : public QUndoCommand {
public:
  SetPrioritizerKindCmd(Document* doc,
                        QString wrt,
                        QString destCluster,
                        anpcpp::NodePrioritizerKind kind);
  void redo() override;
  void undo() override;

private:
  Document* doc_;
  QString wrt_;
  QString destCluster_;
  anpcpp::NodePrioritizerKind kind_;
  anpcpp::NodePrioritizerKind oldKind_ = anpcpp::NodePrioritizerKind::Pairwise;
};

class SetRatingsModeCmd : public QUndoCommand {
public:
  SetRatingsModeCmd(Document* doc,
                    QString wrt,
                    QString destCluster,
                    anpcpp::RatingsPrioritizer::Mode mode);
  void redo() override;
  void undo() override;

private:
  Document* doc_;
  QString wrt_;
  QString destCluster_;
  anpcpp::RatingsPrioritizer::Mode mode_;
  anpcpp::RatingsPrioritizer::Mode oldMode_ =
      anpcpp::RatingsPrioritizer::Mode::Numeric;
};

class SetRatingsCategoriesCmd : public QUndoCommand {
public:
  SetRatingsCategoriesCmd(Document* doc,
                          QString wrt,
                          QString destCluster,
                          std::vector<anpcpp::RatingCategory> cats);
  void redo() override;
  void undo() override;

private:
  Document* doc_;
  QString wrt_;
  QString destCluster_;
  std::vector<anpcpp::RatingCategory> cats_;
  std::vector<anpcpp::RatingCategory> oldCats_;
};

class SetRatingVoteCmd : public QUndoCommand {
public:
  SetRatingVoteCmd(Document* doc,
                   QString wrt,
                   QString destCluster,
                   QString alt,
                   QString categoryId);
  void redo() override;
  void undo() override;

private:
  Document* doc_;
  QString wrt_;
  QString destCluster_;
  QString alt_;
  QString categoryId_;
  QString oldCategoryId_;
  bool hadOld_ = false;
};

class SetRatingValueCmd : public QUndoCommand {
public:
  SetRatingValueCmd(Document* doc,
                    QString wrt,
                    QString destCluster,
                    QString alt,
                    bool clear,
                    double value);
  void redo() override;
  void undo() override;

private:
  Document* doc_;
  QString wrt_;
  QString destCluster_;
  QString alt_;
  bool clear_ = false;
  double value_ = 0.0;
  bool hadOld_ = false;
  double oldValue_ = 0.0;
};

class SetRatingsInterpreterCmd : public QUndoCommand {
public:
  SetRatingsInterpreterCmd(Document* doc,
                           QString wrt,
                           QString destCluster,
                           anpcpp::ScoreInterpreter interpreter);
  void redo() override;
  void undo() override;

private:
  Document* doc_;
  QString wrt_;
  QString destCluster_;
  anpcpp::ScoreInterpreter interpreter_;
  anpcpp::ScoreInterpreter oldInterpreter_;
};

/** @brief Sets cluster window positions (scene top-left) as one undo step. */
class SetClusterPositionsCmd : public QUndoCommand {
public:
  SetClusterPositionsCmd(Document* doc,
                         QHash<QString, QPointF> oldPositions,
                         QHash<QString, QPointF> newPositions);
  void redo() override;
  void undo() override;

private:
  void apply(const QHash<QString, QPointF>& positions);

  Document* doc_;
  QHash<QString, QPointF> oldPositions_;
  QHash<QString, QPointF> newPositions_;
};
