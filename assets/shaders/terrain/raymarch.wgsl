#include "constants.wgsl"
#include "tile.wgsl"
#include "tilemap.wgsl"
#include "heightfield.wgsl"

struct TraceResult {
    hit: bool,
    rayPos: vec3f,
    nh: TileNeighborhood,
    nhValid: bool,
};

// march the ray against the height field. Below simpleModeScaleThreshold the surface is
// flat enough to skip raymarching and just sample the smoothed height once
fn traceTerrain(rayStart: vec3f, rayDir: vec3f, bias: f32, simpleModeActive: bool) -> TraceResult {
    var rayPos = rayStart;
    var hit = false;
    var nh: TileNeighborhood;
    var nhValid = false;

    if (simpleModeActive) {
        nh = fetchTileNeighborhood(rayPos.xy);
        nhValid = true;
        rayPos.z = getSmoothedHeightNeighborhood(rayPos.xy, nh);
        hit = true;
        return TraceResult(hit, rayPos, nh, nhValid);
    }

    let gridStepDir = sign(rayDir.xy);
    let gridBorderOffset = step(vec2f(0.0), gridStepDir);

    var center = fetchTileData(rayPos.xy);
    let invRayDir = 1.0 / rayDir;
    for(var i=0; i<10; i+=1)
    {
        let tileMaxHeight = center.height;

        let tileSoftness = center.softness;

        var gridBorder = floor(rayPos.xy) + gridBorderOffset;

        var borderDistance = vec3f(gridBorder - rayPos.xy, min(0.0, tileMaxHeight - rayPos.z));

        var borderTime = borderDistance * invRayDir;

        var exitTime2 = min(borderTime.x, borderTime.y);
        let exitTime3 = min(exitTime2, borderTime.z);

        if(borderTime.z <= exitTime2)
        {
            if(borderTime.z <=0.0 && (tileSoftness <= 0.05 || center.skipRaymarching))
            {
                hit = true;
                break;
            }

            rayPos += rayDir * (exitTime3 + bias);

            borderDistance = vec3f(gridBorder - rayPos.xy, tileMaxHeight - rayPos.z);
            borderTime = abs(borderDistance * invRayDir);

            exitTime2 = min(borderTime.x, borderTime.y);

            if(tileSoftness <= 0.05 || center.skipRaymarching)
            {
                hit = true;
                break;
            }
            else
            {
                let exitTime = exitTime2 - bias;
                var exitRayPos = rayPos + rayDir * exitTime;

                nh = fetchTileNeighborhood(rayPos.xy);
                nhValid = true;

                let enterHeight = getSmoothedHeightNeighborhood(rayPos.xy, nh);

                if(enterHeight>=rayPos.z)
                {
                    hit = true;
                    break;
                }

                let exitHeight = getSmoothedHeightNeighborhood(exitRayPos.xy, nh);

                let steps = i32(sqrt(160.0 * max(0.0, 0.5 - tileSoftness)) * f32(center.advancedRaymarching));

                let stepSize = exitTime / f32(steps+1);
                var marchedT = 0.0;
                var foundHit = false;

                for(var s = 0; s < steps; s++) {
                    marchedT += stepSize;
                    let testPos = rayPos + rayDir * marchedT;
                    let testPosC = clamp(testPos, min(rayPos, exitRayPos), max(rayPos, exitRayPos));
                    let h = getSmoothedHeightNeighborhood(testPosC.xy, nh);

                    if(testPos.z <= h) {
                        exitRayPos = testPos;
                        foundHit = true;
                        break;
                    }
                }

                if(exitHeight < exitRayPos.z && !foundHit) //no intersection
                {
                    rayPos += rayDir * (exitTime2 + bias);
                    center = fetchTileData(rayPos.xy);
                    nhValid = false;
                }
                else
                {
                    var currentRayPos = (rayPos+exitRayPos)/2.0;
                    for(var j=0; j<raymarchBinarySearchSteps; j+=1)
                    {
                        let currentHeight = getSmoothedHeightNeighborhood(currentRayPos.xy, nh);
                        let prevCurrentRayPos = currentRayPos;
                        if(currentRayPos.z > currentHeight)
                        {
                            currentRayPos = (currentRayPos + exitRayPos)/2.0;
                            rayPos = prevCurrentRayPos;
                        }
                        else
                        {
                            currentRayPos = (rayPos + currentRayPos)/2.0;
                            exitRayPos = prevCurrentRayPos;
                        }
                    }
                    rayPos = currentRayPos;
                    hit = true;
                    break;
                }
            }
        }
        else {
            rayPos += rayDir * (exitTime2 + bias);
            center = fetchTileData(rayPos.xy);
        }
    }

    return TraceResult(hit, rayPos, nh, nhValid);
}
