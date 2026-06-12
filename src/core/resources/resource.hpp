#pragma once

#include "util/hash.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>

class ResourceName {
public:
   // NOLINTBEGIN
   constexpr ResourceName(std::string_view name): key(fnv1a(name)) {}
   constexpr ResourceName(const char* name): ResourceName(std::string_view(name)) {}
   // NOLINTEND
   bool operator ==(const ResourceName& other) const { return key == other.key; }

   [[nodiscard]] constexpr size_t hash() const noexcept { return key; }

private:
   uint64_t key{};
};

class Resource {
public:
   // NOLINTNEXTLINE
   const ResourceName resourceName;

   explicit Resource(const ResourceName& name): resourceName(name) {}
   virtual ~Resource() = default;

   Resource(const Resource&) = delete;
   Resource(Resource&&) = delete;
   Resource& operator =(const Resource&) = delete;
   Resource& operator =(Resource&&) = delete;
};

template<>
struct std::hash<ResourceName> {
   size_t operator ()(const ResourceName& resName) const noexcept { return resName.hash(); }
};

class ResourceManager {
public:
   class ImageResource* loadImage(const ResourceName& name, const std::filesystem::path& path);
   class ResourceCreationToken;

private:
   std::unordered_map<ResourceName, ImageResource> resources;
};

class ImageResource: public Resource {
public:
   ImageResource(const ResourceManager::ResourceCreationToken& token, const ResourceName& name, const std::filesystem::path& path);
   ~ImageResource();

   ImageResource(const ImageResource&) = delete;
   ImageResource(ImageResource&&) = delete;
   ImageResource& operator =(const ImageResource&) = delete;
   ImageResource& operator =(ImageResource&&) = delete;

   [[nodiscard]] bool isValid() const { return data != nullptr; }

   [[nodiscard]] int getWidth() const { return width; }
   [[nodiscard]] int getHeight() const { return height; }

   [[nodiscard]] const uint8_t* getData() const { return data; }

private:
   std::uint8_t* data = nullptr;
   uint16_t width = 0;
   uint16_t height = 0;
   uint16_t channels = 0;
};
