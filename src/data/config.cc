//----------------------------------------------------------------------------------------------------------------------
// Configuration management, parsing and writing
//----------------------------------------------------------------------------------------------------------------------

#include <core.h>

#include <algorithm>
#include <data/config.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <utils/cmdline.h>
#include <utils/msg.h>
#include <utils/utils.h>

using namespace std;

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
// Section management
//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------

func Config::findSection(const string& name) const -> optional<Config::Sections::const_iterator>
{
    Sections::const_iterator section = find_if(m_sections.begin(), m_sections.end(), [&name](const Section& sect) -> bool {
        return sect.name == name;
    });
    if (section == m_sections.end())
    {
        return {};
    }
    else
    {
        return section;
    }
}

//----------------------------------------------------------------------------------------------------------------------

func Config::getSection(string&& key) const -> tuple<Config::Sections::const_iterator, string>
{
    auto paths = split(move(key), ".");

    auto p = paths.begin();
    auto section = findSection(*p);

    if (!section)
    {
        return make_tuple(m_sections.end(), string());
    }

    paths.erase(paths.begin());
    string subKey = join(paths, ".");

    return make_tuple(*section, subKey);
}

//----------------------------------------------------------------------------------------------------------------------

func Config::ensureSection(string&& key) -> tuple<Config::Sections::iterator, string>
{
    auto paths = split(move(key), ".");

    auto p = paths.begin();
    auto section = findSection(*p);

    if (!section)
    {
        string sectName = *p;
        addSection(move(*p++));
        section = findSection(sectName);
    }

    assert(section);
    Config::Sections::iterator it(m_sections.begin());
    advance(it, *section - m_sections.cbegin());

    paths.erase(paths.begin());
    string subKey = join(paths, ".");

    return make_tuple(it, subKey);
}

//----------------------------------------------------------------------------------------------------------------------

func Config::addSection(string name) -> void
{
    assert(!findSection(name));
    m_sections.emplace_back(move(name));
}

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
// Config value management
//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------

func Config::set(string&& key, string&& value) -> void
{
    auto[sectionIt, subKey] = ensureSection(move(key));
    Section& s = *sectionIt;

    if (find(s.keys.begin(), s.keys.end(), subKey) == s.keys.end())
    {
        s.keys.push_back(subKey);
    }

    s.map[subKey] = value;
}

//----------------------------------------------------------------------------------------------------------------------

func Config::get(string key, string&& defaultValue) const -> string
{
    auto[sectionIt, subKey] = getSection(move(key));

    if (sectionIt == m_sections.end())
    {
        // Not found!
        return defaultValue;
    }
    else
    {
        return sectionIt->map.at(subKey);
    }
}

//----------------------------------------------------------------------------------------------------------------------

func Config::get(string key) const -> string
{
    auto[sectionIt, subKey] = getSection(move(key));

    if (sectionIt == m_sections.end())
    {
        // Not found!
        return string();
    }
    else
    {
        auto it = sectionIt->map.find(subKey);
        return (it == sectionIt->map.end()) ? "" : it->second;
    }
}

//----------------------------------------------------------------------------------------------------------------------

func Config::tryGet(string key) const -> optional<string>
{
    auto[sectionIt, subKey] = getSection(move(key));

    if (sectionIt == m_sections.end())
    {
        // Not found!
        return {};
    }
    else
    {
        auto it = sectionIt->map.find(subKey);
        return (it == sectionIt->map.end()) ? optional<string>{} : it->second;
    }
}

//----------------------------------------------------------------------------------------------------------------------

func Config::writeIni(const CmdLine& cmdLine, const std::filesystem::path& path) const -> bool
{
    ofstream f;
    f.open(path, ios::trunc);
    if (f.is_open())
    {
        for (const auto& section : m_sections)
        {
            f << "[" << section.name << "]" << endl;

            for (const auto& key : section.keys)
            {
                f << key << " = \"" << section.map.at(key) << "\"" << endl;
            }

            f << endl;
        }

        return true;
    }
    else
    {
        error(cmdLine, stringFormat("Could not open `{0}`!", path.string()));
        return false;
    }
}

//----------------------------------------------------------------------------------------------------------------------

static const char* kCommentChar = "#";

func Config::readIni(const CmdLine& cmdLine, const std::filesystem::path& path) -> bool
{
    ifstream f;
    f.open(path);
    if (f.is_open())
    {
        string s;
        string lastSection;
        while (std::getline(f, s))
        {
            if (!s.empty())
            {
                if (s[0] == '[')
                {
                    // Section.
                    auto i = s.find(']');
                    string sectionName = s.substr(1, i - 1);
                    addSection(sectionName);
                    lastSection = sectionName;
                }
                else if (s[0] != kCommentChar[0])
                {
                    vector<string> kv = split(s, "=");
                    if (kv.size() != 2)
                    {
                        error(cmdLine, "Invalid `forge.ini` file.");
                        return false;
                    }
                    for (auto& s : kv) trim(s);
                    string key = lastSection.empty() ? kv[0] : lastSection + "." + kv[0];
                    set(move(key), move(kv[1]));
                }
            }
        }

        return true;
    }
    else
    {
        error(cmdLine, stringFormat("Could not open `{0}`!", path.string()));
        return false;
    }
}

//----------------------------------------------------------------------------------------------------------------------

func Config::comment(std::string&& key) -> void
{
    auto[sectionIt, subKey] = ensureSection(move(key));
    string commentedKey = string(kCommentChar) + subKey;

    // Section exists
    auto keyIt = sectionIt->map.find(subKey);
    auto commentIt = sectionIt->map.find(commentedKey);

    if ((keyIt == sectionIt->map.end()) && (commentIt == sectionIt->map.end()))
    {
        // Neither the key or a commented out version of it exists.  Create it from scratch.
        set(sectionIt->name + "." + kCommentChar + subKey, {});
    }
    else if (keyIt != sectionIt->map.end())
    {
        // Found the key to comment.
        if (commentIt == sectionIt->map.end())
        {
            // Commented out version doesn't exist so replace the key.
            // The map:
            auto nodeHandler = sectionIt->map.extract(subKey);
            nodeHandler.key() = string(kCommentChar) + subKey;
            sectionIt->map.insert(move(nodeHandler));

            // The keys:
            auto keyIt = find(sectionIt->keys.begin(), sectionIt->keys.end(), subKey);
            assert(keyIt != sectionIt->keys.end());
            *keyIt = string(kCommentChar) + subKey;
        }
        else
        {
            // There's a commented out version already.
            assert(0);
            return;
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
