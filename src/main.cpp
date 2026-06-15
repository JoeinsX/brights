#define WEBGPU_CPP_IMPLEMENTATION
#include "app/application.hpp"

int main() {
   Application app;
   if (!app.initialize()) {
      return 1;
   }
   while (app.isRunning()) {
      app.mainLoop();
   }
   app.terminate();
   return 0;
}
