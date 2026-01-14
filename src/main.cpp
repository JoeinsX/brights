#define WEBGPU_CPP_IMPLEMENTATION
#define GLM_FORCE_DEFAULT_PACKED_GENTYPES ;
#define GLM_ENABLE_EXPERIMENTAL
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
