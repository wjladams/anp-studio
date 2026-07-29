#include "panels/session_stub_panel.hpp"

#include <QLabel>
#include <QVBoxLayout>

SessionStubPanel::SessionStubPanel(QWidget* parent) : QWidget(parent) {
  auto* layout = new QVBoxLayout(this);
  auto* title = new QLabel(QStringLiteral("Session"), this);
  title->setStyleSheet(QStringLiteral("font-weight: bold;"));
  layout->addWidget(title);
  layout->addWidget(new QLabel(
      QStringLiteral(
          "Multi-user participants and judgment aggregation are not "
          "available yet."),
      this));
  layout->addStretch();
}
