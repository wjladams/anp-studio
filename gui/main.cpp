#include "mainwindow.hpp"

#include <QApplication>
#include <QIcon>

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

}  // namespace

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  QApplication::setApplicationName("ANP Studio");
  QApplication::setOrganizationName("ANP Studio");
  QApplication::setApplicationVersion("0.1.0");
  QApplication::setWindowIcon(loadAppIcon());

  MainWindow window;
  window.show();
  return app.exec();
}
