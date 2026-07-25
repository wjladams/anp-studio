#include "mainwindow.hpp"

#include <QApplication>

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  QApplication::setApplicationName("cppanp");
  QApplication::setOrganizationName("cppanp");
  QApplication::setApplicationVersion("0.1.0");

  MainWindow window;
  window.show();
  return app.exec();
}
