#pragma once

#include <set>
#include <string>

struct RenderSettings {
   float perspectiveStrength = 0.002f;

   float simpleModeThreshold = 3.0f;
   int raymarchMaxTiles = 10;
   int raymarchBinarySteps = 6;

   bool enableRaymarching = true;
   bool enableSoftMarch = true;
   bool enableTriplanar = true;
   bool enableBlending = true;

   int debugView = 0;   // 0 off, 1 normals, 2 height

   static constexpr const char* key = "render";

   template<typename Self, typename Fn>
   static void forEachField(Self& self, Fn&& fn) {
      fn("perspectiveStrength", self.perspectiveStrength);
      fn("simpleModeThreshold", self.simpleModeThreshold);
      fn("raymarchMaxTiles", self.raymarchMaxTiles);
      fn("raymarchBinarySteps", self.raymarchBinarySteps);
      fn("enableRaymarching", self.enableRaymarching);
      fn("enableSoftMarch", self.enableSoftMarch);
      fn("enableTriplanar", self.enableTriplanar);
      fn("enableBlending", self.enableBlending);
      fn("debugView", self.debugView);
   }

   [[nodiscard]] std::set<std::string> getDefines() const {
      std::set<std::string> defines;
      if (enableRaymarching) {
         defines.insert("ENABLE_RAYMARCHING");
      }
      if (enableSoftMarch) {
         defines.insert("ENABLE_SOFT_MARCH");
      }
      if (enableTriplanar) {
         defines.insert("ENABLE_TRIPLANAR");
      }
      if (enableBlending) {
         defines.insert("ENABLE_BLENDING");
      }
      if (debugView == 1) {
         defines.insert("DEBUG_NORMAL");
      } else if (debugView == 2) {
         defines.insert("DEBUG_HEIGHT");
      }
      return defines;
   }
};
