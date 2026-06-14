#include "ledis/app/application.h"

int main(int argc, char** argv) {
  ledis::Application app;
  if (!app.init(argc, argv)) {
    return app.helpRequested() ? 0 : app.exitCode();
  }
  return app.run();
}
