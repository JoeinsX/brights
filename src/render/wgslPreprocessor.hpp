#pragma once

#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

class WGSLPreprocessor {
public:
   void addDefine(const std::string& name) { defines.insert(name); }

   // Register a directory searched for includes that are not found next to the including file.
   void addIncludeRoot(const std::filesystem::path& root) { includeRoots.push_back(root); }

   [[nodiscard]] std::string load(const std::filesystem::path& path) const {
      std::set<std::filesystem::path> included{path.lexically_normal()};
      return parseFile(path, included);
   }

private:
   std::set<std::string> defines;
   std::vector<std::filesystem::path> includeRoots;

   // Resolve an #include relative to the including file first, then each registered root.
   [[nodiscard]] std::filesystem::path resolveInclude(const std::filesystem::path& fromDir, const std::string& rel) const {
      const std::filesystem::path local = fromDir / rel;
      if (std::filesystem::exists(local)) {
         return local;
      }
      for (const std::filesystem::path& root : includeRoots) {
         std::filesystem::path candidate = root / rel;
         if (std::filesystem::exists(candidate)) {
            return candidate;
         }
      }
      return local;
   }

   [[nodiscard]] std::string parseFile(const std::filesystem::path& path, std::set<std::filesystem::path>& included) const {
      std::ifstream file(path);
      if (!file.is_open()) {
         std::cerr << "WGSL Error: Could not open file " << path << '\n';
         return "";
      }

      std::stringstream output;
      std::string line;

      std::vector<bool> ifStack;
      ifStack.push_back(true);

      const std::regex includeRegex(R"(\s*#include\s+\"(.+)\"\s*)");
      const std::regex ifdefRegex(R"(\s*#ifdef\s+(\w+)\s*)");
      const std::regex ifndefRegex(R"(\s*#ifndef\s+(\w+)\s*)");
      const std::regex elseRegex(R"(\s*#else\s*)");
      const std::regex endifRegex(R"(\s*#endif\s*)");
      std::smatch match;

      while (std::getline(file, line)) {
         const bool processing = ifStack.back();

         if (std::regex_match(line, match, ifdefRegex)) {
            const bool condition = defines.contains(match[1].str());
            ifStack.push_back(processing && condition);
            continue;
         }

         if (std::regex_match(line, match, ifndefRegex)) {
            const bool condition = !defines.contains(match[1].str());
            ifStack.push_back(processing && condition);
            continue;
         }

         if (std::regex_match(line, match, elseRegex)) {
            if (ifStack.size() > 1) {
               const bool current = ifStack.back();
               ifStack.pop_back();
               const bool parent = ifStack.back();
               ifStack.push_back(parent && !current);
            }
            continue;
         }

         if (std::regex_match(line, match, endifRegex)) {
            if (ifStack.size() > 1) {
               ifStack.pop_back();
            }
            continue;
         }

         if (!processing) {
            continue;
         }

         if (std::regex_match(line, match, includeRegex)) {
            const std::filesystem::path includePath = resolveInclude(path.parent_path(), match[1].str());
            if (included.insert(includePath.lexically_normal()).second) {
               output << parseFile(includePath, included) << "\n";
            }
            continue;
         }

         output << line << "\n";
      }

      return output.str();
   }
};
