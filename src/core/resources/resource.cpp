#include "resource.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "util/metaUtil.hpp"

class ResourceManager::ResourceCreationToken {};
const static ResourceManager::ResourceCreationToken resourceCreationToken;

ImageResource* ResourceManager::loadImage(const ResourceName& name, const std::filesystem::path& path) {
   return &resources.try_emplace(name, resourceCreationToken, name, path).first->second;
}

ImageResource::ImageResource(const ResourceManager::ResourceCreationToken& /*token*/, const ResourceName& name, const std::filesystem::path& path): Resource(name) {
   int dataWidth = 0, dataHeight = 0, dataChannels = 0;
   data = stbi_load(path.string().c_str(), &dataWidth, &dataHeight, &dataChannels, 4);
   width = dataWidth;
   height = dataHeight;
   channels = dataChannels;
}

ImageResource::~ImageResource() {
   if (data) {
      stbi_image_free(data);
   }
}

// really want to make sure construction is constexpr
ASSERT_EXPRESSION_CONSTEVALABILITY(ResourceName{"sus"})
