#include "chunkMesher.hpp"

void ChunkMesher::MeshContext::analyzeTopology() {
   constexpr int nOffsets[8][2] = {{-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}};
   constexpr int edgeOffsets[4][2] = {{0, -1}, {-1, 0}, {1, 0}, {0, 1}};       // N, W, E, S
   constexpr int cornerOffsets[4][2] = {{-1, -1}, {1, -1}, {-1, 1}, {1, 1}};   // NW, NE, SW, SE

   for (int y = -1; y <= CHUNK_SIZE; ++y) {
      for (int x = -1; x <= CHUNK_SIZE; ++x) {
         CachedTile& current = buffer[(y + 1) * PADDED_SIZE + (x + 1)];

         for (const auto& off : nOffsets) {
            const int nx = std::clamp(x + off[0] + 1, 0, PADDED_SIZE - 1);
            const int ny = std::clamp(y + off[1] + 1, 0, PADDED_SIZE - 1);

            const CachedTile& neighbor = buffer[ny * PADDED_SIZE + nx];

            if (neighbor.height < current.height - EPSILON) {
               current.set(AnalysisFlag::HasLowerNeighbor);
            }

            if (std::abs(neighbor.height - current.height) > EPSILON || neighbor.id != current.id) {
               current.set(AnalysisFlag::HasVariance);
            }
         }
      }
   }

   for (int y = 0; y < CHUNK_SIZE; ++y) {
      for (int x = 0; x < CHUNK_SIZE; ++x) {
         CachedTile& current = buffer[(y + 1) * PADDED_SIZE + (x + 1)];

         bool shouldSkip = false;

         if (current.softness <= EPSILON) {
            shouldSkip = true;
         } else {
            bool surroundedBySameOrHigherHard = true;
            for (const auto& off : nOffsets) {
               const CachedTile& n = get(x + off[0], y + off[1]);
               const bool isSameHeight = std::abs(n.height - current.height) < EPSILON;
               const bool isHigherAndHard = (n.height > current.height) && (n.softness < EPSILON);

               if (!isSameHeight && !isHigherAndHard) {
                  surroundedBySameOrHigherHard = false;
                  break;
               }
            }
            if (surroundedBySameOrHigherHard) {
               shouldSkip = true;
            }
         }
         if (shouldSkip) {
            current.set(RenderFlag::SkipRaymarching);
         }

         int lowerEdgeCount = 0;
         for (const auto& off : edgeOffsets) {
            if (get(x + off[0], y + off[1]).height < current.height - EPSILON) {
               lowerEdgeCount++;
            }
         }

         int lowerCornerCount = 0;
         if (lowerEdgeCount == 1) {
            for (const auto& off : cornerOffsets) {
               if (get(x + off[0], y + off[1]).height < current.height - EPSILON) {
                  lowerCornerCount++;
               }
            }
         }

         if (lowerEdgeCount > 1 || lowerCornerCount != 0) {
            current.set(RenderFlag::AdvancedRaymarching);
         }

         if (current.has(AnalysisFlag::HasVariance)) {
            current.set(RenderFlag::Blending);

            bool isSharedSlope = false;
            if (current.has(AnalysisFlag::HasLowerNeighbor)) {
               for (const auto& off : nOffsets) {
                  if (get(x + off[0], y + off[1]).has(AnalysisFlag::HasLowerNeighbor)) {
                     isSharedSlope = true;
                     break;
                  }
               }
            }

            if (isSharedSlope || current.softness < 0.1f) {
               current.set(RenderFlag::Triplanar);
            }
         }
      }
   }
}
