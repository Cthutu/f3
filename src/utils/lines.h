//----------------------------------------------------------------------------------------------------------------------
// Build a vector of line strings from a continuous feed.
//----------------------------------------------------------------------------------------------------------------------

#pragma once

#include <core.h>

//----------------------------------------------------------------------------------------------------------------------

class Lines
{
public:
    std::vector<std::string>::const_iterator begin() const { return m_lines.begin(); }
    std::vector<std::string>::const_iterator end() const { return m_lines.end(); }

    func clear() -> void { m_data.clear();  m_lines.clear(); }

    func feed(const char* buffer, size_t len) -> void
    {
        m_data += std::string(buffer, len);
    }

    func generate() -> std::vector<std::string>&
    {
        std::string line;

        for (const auto& ch : m_data)
        {
            if (ch == '\n')
            {
                m_lines.emplace_back(line);
                line.clear();
            }
            else if (ch != '\r')
            {
                line += ch;
            }
        }

        if (!line.empty()) m_lines.emplace_back(move(line));
        return m_lines;
    }

    func operator[](int index) const -> const std::string& { return m_lines[index]; }

private:
    std::string m_data;
    std::vector<std::string> m_lines;
};

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
