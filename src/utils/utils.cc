//----------------------------------------------------------------------------------------------------------------------
// Utilities
//----------------------------------------------------------------------------------------------------------------------

#include <core.h>

#include <cctype>
#include <sstream>
#include <utils/msg.h>
#include <utils/utils.h>

using namespace std;

//----------------------------------------------------------------------------------------------------------------------

func split(const string& text, string&& delim) -> vector<string>
{
    vector<string> tokens;
    size_t prev = 0, pos = 0;
    do
    {
        pos = text.find(delim, prev);
        if (pos == string::npos) pos = text.length();
        string token = text.substr(prev, pos - prev);
        if (!token.empty()) tokens.push_back(token);
        prev = pos + delim.length();
    } while (pos < text.length() && prev < text.length());
    return tokens;
}

//----------------------------------------------------------------------------------------------------------------------

func join(const vector<string>& elems, string&& delim) -> string
{
    if (elems.empty()) return {};

    stringstream ss;
    ss << elems[0];
    for (size_t i = 1; i < elems.size(); ++i)
    {
        ss << delim << elems[i];
    }

    return ss.str();
}

//----------------------------------------------------------------------------------------------------------------------

func join(const vector<filesystem::path>& elems, string&& delim) -> string
{
    if (elems.empty()) return {};

    stringstream ss;
    ss << elems[0].string();
    for (size_t i = 1; i < elems.size(); ++i)
    {
        ss << delim << elems[i].string();
    }

    return ss.str();
}

//----------------------------------------------------------------------------------------------------------------------
// trim from start (in place)

func ltrim(string &s) -> void
{
    s.erase(s.begin(), find_if(s.begin(), s.end(), [](int ch)
    {
        return !isspace(ch) && (ch != '"');
    }));
}

//----------------------------------------------------------------------------------------------------------------------
// trim from end (in place)

func rtrim(string &s) -> void
{
    s.erase(find_if(s.rbegin(), s.rend(), [](int ch)
    {
        return !isspace(ch) && (ch != '"');
    }).base(), s.end());
}

//----------------------------------------------------------------------------------------------------------------------
// trim from both ends (in place)

func trim(string &s) -> void
{
    ltrim(s);
    rtrim(s);
}

//----------------------------------------------------------------------------------------------------------------------

func symbolise(const string& str) -> string
{
    string sym;
    for (auto c : str)
    {
        if ((c >= '0' && c <= '9') ||
            (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c == '_') ||
            (c == '$'))
        {
            sym += c;
        }
        else if ((c == ' ') || (c == '\\') || (c == '/') || (c == '.'))
        {
            sym += '_';
        }
    }

    if (sym[0] >= '0' && sym[0] <= '9')
    {
        sym = string("_") + sym;
    }

    return sym;
}

//----------------------------------------------------------------------------------------------------------------------

func byteHexStr(u8 byte) -> string
{
    u8 h = byte / 16;
    u8 l = byte % 16;

    static const char digits[] = "0123456789abcdef";
    char hex[3] = "??";
    hex[0] = digits[h];
    hex[1] = digits[l];
    return hex;
}

//----------------------------------------------------------------------------------------------------------------------

func extractSubStr(const string& str, char startDelim, char endDelim) -> string
{
    size_t start = str.find(startDelim);
    if (start == string::npos) return {};

    size_t end = find(str.begin() + start, str.end(), endDelim) - str.begin();

    if (end == str.size()) return {};

    return str.substr(start + 1, end - start - 1);
}

//----------------------------------------------------------------------------------------------------------------------

func validateFileName(const string& str) -> bool
{
    for (const auto& c : str)
    {
        if (c < ' ' ||
            c > 126 ||
            c == '/' ||
            c == '\\' ||
            c == '?' ||
            c == '%' ||
            c == '*' ||
            c == ':' ||
            c == '|' ||
            c == '"' ||
            c == '<' ||
            c == '>' ||
            c == '(' ||
            c == ')' ||
            c == '&' ||
            c == ';' ||
            c == '#' ||
            c == '\'')
        {
            return false;
        }
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

func ensurePath(const CmdLine& cmdLine, filesystem::path&& path) -> bool
{
    using namespace filesystem;

    // Check to see if the path already exists.
    if (exists(path) && is_directory(path)) return true;

    if (exists(path))
    {
        // Path is not a directory!
        error(cmdLine, stringFormat("`{0}` is not a directory!", path.string()));
        return false;
    }

    if (!ensurePath(cmdLine, path.parent_path())) return false;

    return create_directory(path);
}

//----------------------------------------------------------------------------------------------------------------------

func generateGuid() -> string
{
#if OS_WIN32
    GUID guid;
    CoCreateGuid(&guid);

    stringstream ss;
    ss << hex << uppercase
        << '{'
        << setfill('0') << setw(8) << (u32)(guid.Data1) << '-'
        << setfill('0') << setw(4) << (u16)(guid.Data2) << '-'
        << setfill('0') << setw(4) << (u16)(guid.Data3) << '-'
        << setfill('0') << setw(2) << (int)(guid.Data4[0])
        << setfill('0') << setw(2) << (int)(guid.Data4[1]) << '-'
        << setfill('0') << setw(2) << (int)(guid.Data4[2])
        << setfill('0') << setw(2) << (int)(guid.Data4[3])
        << setfill('0') << setw(2) << (int)(guid.Data4[4])
        << setfill('0') << setw(2) << (int)(guid.Data4[5])
        << setfill('0') << setw(2) << (int)(guid.Data4[6])
        << setfill('0') << setw(2) << (int)(guid.Data4[7]) << '}';

    return ss.str();
#else
#   error Define generateGuid() for your platform.
#endif
}

//----------------------------------------------------------------------------------------------------------------------

func hasEnding(const string& str, const string& ending) -> bool
{
    if (str.length() >= ending.length())
    {
        return (0 == str.compare(str.length() - ending.length(), ending.length(), ending));
    }
    else
    {
        return false;
    }
}

//----------------------------------------------------------------------------------------------------------------------

func ensureEnding(const string& str, const string& ending) -> string
{
    return hasEnding(str, ending) ? str : str + ending;
}

//----------------------------------------------------------------------------------------------------------------------

func expand(const std::string& text) -> string
{
    std::string out;

    for (auto it = text.begin(); it != text.end();)
    {
        if (*it == '$')
        {
            std::string macro;
            ++it;
            while ((*it >= 'a' && *it <= 'z') ||
                (*it >= 'A' && *it <= 'Z') ||
                (*it == '_'))
            {
                macro += *it;
                ++it;
            }
            if (macro.empty())
            {
                out += '$';
                ++it;
            }
            else
            {
                char* buffer;
                size_t size;
                if (!_dupenv_s(&buffer, &size, macro.c_str()))
                {
                    out += buffer;
                    free(buffer);
                }
            }
        }
        else
        {
            out += *it;
            ++it;
        }
    }

    return out;
}

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
