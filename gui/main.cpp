#include "mainwindow.hpp"

#include <QApplication>
#include <QFile>
#include <QIcon>
#include <QPalette>
#include <QStyle>
#include <QStyleFactory>

namespace {

QIcon loadAppIcon() {
  QIcon icon;
  icon.addFile(QStringLiteral(":/icons/anpstudio-16.png"));
  icon.addFile(QStringLiteral(":/icons/anpstudio-32.png"));
  icon.addFile(QStringLiteral(":/icons/anpstudio-48.png"));
  icon.addFile(QStringLiteral(":/icons/anpstudio-64.png"));
  icon.addFile(QStringLiteral(":/icons/anpstudio-128.png"));
  icon.addFile(QStringLiteral(":/icons/anpstudio-256.png"));
  icon.addFile(QStringLiteral(":/icons/anpstudio.png"));
  return icon;
}

QPalette lightAppPalette() {
  // Gmail / Slack / Teams light: white surfaces, dark text, blue accent.
  const QColor window(0xffffff);
  const QColor base(0xffffff);
  const QColor altBase(0xf8f9fa);
  const QColor chrome(0xf6f8fc);
  const QColor text(0x202124);
  const QColor muted(0x5f6368);
  const QColor border(0xdadce0);
  const QColor accent(0x1a73e8);
  const QColor selected(0xe8f0fe);

  QPalette pal;
  pal.setColor(QPalette::Window, window);
  pal.setColor(QPalette::WindowText, text);
  pal.setColor(QPalette::Base, base);
  pal.setColor(QPalette::AlternateBase, altBase);
  pal.setColor(QPalette::ToolTipBase, text);
  pal.setColor(QPalette::ToolTipText, window);
  pal.setColor(QPalette::Text, text);
  pal.setColor(QPalette::Button, chrome);
  pal.setColor(QPalette::ButtonText, text);
  pal.setColor(QPalette::BrightText, QColor(0xffffff));
  pal.setColor(QPalette::Link, accent);
  pal.setColor(QPalette::LinkVisited, accent);
  pal.setColor(QPalette::Highlight, selected);
  pal.setColor(QPalette::HighlightedText, QColor(0x1967d2));
  pal.setColor(QPalette::PlaceholderText, muted);
  pal.setColor(QPalette::Light, window);
  pal.setColor(QPalette::Midlight, border);
  pal.setColor(QPalette::Mid, border);
  pal.setColor(QPalette::Dark, muted);
  pal.setColor(QPalette::Shadow, QColor(0x3c4043));

  pal.setColor(QPalette::Disabled, QPalette::WindowText, muted);
  pal.setColor(QPalette::Disabled, QPalette::Text, muted);
  pal.setColor(QPalette::Disabled, QPalette::ButtonText, muted);
  pal.setColor(QPalette::Disabled, QPalette::Highlight, altBase);
  pal.setColor(QPalette::Disabled, QPalette::HighlightedText, muted);
  return pal;
}

void applyAppStyle(QApplication& app) {
  if (QStyle* fusion = QStyleFactory::create(QStringLiteral("Fusion"))) {
    app.setStyle(fusion);
  }
  app.setPalette(lightAppPalette());
  QFile styleFile(QStringLiteral(":/styles/app.qss"));
  if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
    app.setStyleSheet(QString::fromUtf8(styleFile.readAll()));
  }
}

}  // namespace

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  QApplication::setApplicationName("ANP Studio");
  QApplication::setOrganizationName("ANP Studio");
  QApplication::setApplicationVersion("0.3.0");
  QApplication::setWindowIcon(loadAppIcon());
  applyAppStyle(app);

  MainWindow window;
  window.show();
  return app.exec();
}
