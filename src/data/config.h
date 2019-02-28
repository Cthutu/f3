//----------------------------------------------------------------------------------------------------------------------
// Ini file loader and saver.
//----------------------------------------------------------------------------------------------------------------------

#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

//----------------------------------------------------------------------------------------------------------------------
// Configuration

class CmdLine;

class Config
{
public:
    Config() {};

    func addSection(std::string name) -> void;

    func set(std::string&& key, std::string&& value) -> void;
    func get(std::string key, std::string&& defaultValue) const -> std::string;
    func get(std::string key) const -> std::string;
    func tryGet(std::string key) const -> std::optional<std::string>;
    func comment(std::string&& key) -> void;

    func writeIni(const CmdLine& cmdLine, const std::filesystem::path& path) const -> bool;
    func readIni(const CmdLine& cmdLine, const std::filesystem::path& path) -> bool;

private:
    struct Section
    {
        using ConfigMap = std::map<std::string, std::string>;
        using SectionKeys = std::vector<std::string>;

        std::string name;       // Name of section.
        ConfigMap map;          // Look up.
        SectionKeys keys;       // To maintain order of insertion.

        Section() = default;
        Section(std::string&& name) : name(std::move(name)) {}

        Section(const Section&) = delete;
        Section(Section&& o)
            : name(std::move(o.name))
            , map(std::move(o.map))
            , keys(std::move(o.keys))
        {}
        Section& operator=(const Section&) = delete;
        Section& operator=(Section&&) = delete;
    };

    using Sections = std::vector<Section>;

    func findSection(const std::string& name) const -> std::optional<Sections::const_iterator>;
    func getSection(std::string&& key) const -> std::tuple<Sections::const_iterator, std::string>;
    func ensureSection(std::string&& key) -> std::tuple<Sections::iterator, std::string>;

private:
    Sections m_sections;
};

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
