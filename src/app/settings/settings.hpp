#pragma once

#include "core/graphics/renderSettings.hpp"
#include "platform/window.hpp"
#include "util/logger.hpp"
#include "yamlConversions.hpp"

#include <concepts>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <yaml-cpp/yaml.h>

template<typename T>
concept SettingsSection = requires(T& section) {
   { T::key } -> std::convertible_to<std::string_view>;
   T::forEachField(section, [](const char*, auto&) {});
};

class AnySettingsSection {
public:
   virtual void load(const YAML::Node& root) = 0;
   virtual void save(YAML::Node& root) const = 0;

   virtual ~AnySettingsSection() = default;
};

template<SettingsSection S>
struct SettingsSectionKey {
   static constexpr const char* keyString = S::key;
   static constexpr uint64_t keyUint = fnv1a(std::string_view(keyString));
};

template<SettingsSection S>
class AnySettingsSectionImpl: public AnySettingsSection {
public:
   void load(const YAML::Node& root) override {
      const YAML::Node node = root[S::key];
      if (!node) {
         return;
      }
      S::forEachField(section, [&](const char* key, auto& ref) { ref = node[key].as<std::remove_reference_t<decltype(ref)>>(ref); });
   }

   void save(YAML::Node& root) const override {
      YAML::Node node;
      S::forEachField(section, [&](const char* key, const auto& ref) { node[key] = ref; });
      root[S::key] = node;
   }

   S& accessSection() { return section; }
   const S& getSection() const { return section; }

   S section;
};

class Settings {
public:
   template<SettingsSection S>
   S& accessSection() {
      return static_cast<AnySettingsSectionImpl<S>*>(sections[SettingsSectionKey<S>::keyUint].get())->accessSection();
   }

   template<SettingsSection S>
   const S& getSection() {
      return static_cast<AnySettingsSectionImpl<S>*>(sections[SettingsSectionKey<S>::keyUint].get())->getSection();
   }

   template<SettingsSection S>
   void addSection() {
      sections.insert(std::make_pair(SettingsSectionKey<S>::keyUint, std::make_unique<AnySettingsSectionImpl<S>>()));
   }

   void load(const std::filesystem::path& path = "config.yaml") {
      configPath = path;

      YAML::Node root;
      try {
         root = YAML::LoadFile(path.string());
      } catch (const std::exception& e) {
         Logger::warn("config: could not load '{}' ({}); using defaults", path.string(), e.what());
         return;
      }

      for (auto& it : sections) {
         it.second->load(root);
      }
   }

   void save() const { save(configPath); }

   void save(const std::filesystem::path& path) const {
      YAML::Node root;
      try {
         root = YAML::LoadFile(path.string());
      } catch (const std::exception& e) {
         Logger::error("config: could not open '{}' for saving ({})", path.string(), e.what());
         return;
      }

      for (auto& it : sections) {
         it.second->save(root);
      }

      std::ofstream out(path);
      if (out.is_open()) {
         out << root;
      }
   }

private:
   std::unordered_map<uint64_t, std::unique_ptr<AnySettingsSection>> sections;
   std::filesystem::path configPath = "config.yaml";
};
