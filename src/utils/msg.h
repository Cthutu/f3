//----------------------------------------------------------------------------------------------------------------------
// Message construction
//----------------------------------------------------------------------------------------------------------------------

#pragma once

#include <cassert>
#include <sstream>
#include <string>
#include <utils/cmdline.h>

func msg(const CmdLine& cmdLine, std::string action, std::string info) -> void;
func error(const CmdLine& cmdLine, std::string message) -> bool;


namespace {

    inline func isDigit(char c) -> bool
    {
        return ('0' <= c) && (c <= '9');
    }

    inline func toDigit(char c) -> int
    {
        return c - '0';
    }

    template <int N>
    struct Counter
    {
        int m_count[N];
        int m_braceEscapes;
        int m_plainChars;

        Counter() : m_braceEscapes(0), m_plainChars(0)      { memset(m_count, 0, sizeof(int) * N); }

        func onMarker(int markerNumber) -> void             { ++m_count[markerNumber]; }

        func onEscapeLeft() -> void                         { ++m_braceEscapes; }
        func onEscapeRight() -> void                        { ++m_braceEscapes; }
        func onChar(char c) -> void                         { ++m_plainChars; }
    };

    template <int N>
    class Formatter
    {
    public:
        Formatter(std::string& dest, std::string* values)
            : m_dest(dest)
            , m_size(0)
            , m_values(values)
        {}

        func onMarker(int markerNumber) -> void
        {
            size_t len = m_values[markerNumber].length();
            m_dest.replace(m_size, len, m_values[markerNumber]);
            m_size += len;
        }

        func onEscapeLeft() -> void                         { m_dest[m_size++] = '{'; }
        func onEscapeRight() -> void                        { m_dest[m_size++] = '}'; }
        func onChar(char c) -> void                         { m_dest[m_size++] = c; }

    private:
        std::string & m_dest;
        size_t m_size;
        std::string* m_values;
    };

    template <int N, typename Handler>
    inline func traverse(const char* format, Handler& callback) -> void
    {
        const char* c = format;
        while (*c != 0)
        {
            if (*c == '{')
            {
                if (c[1] == '{')
                {
                    callback.onEscapeLeft();
                    c += 2;
                }
                else
                {
                    int number = 0;
                    ++c;

                    // Unexpected end of string
                    assert(*c != 0);

                    // Invalid brace contents: must be a positive integer
                    assert(isDigit(*c));

                    number = toDigit(*c++);
                    while (isDigit(*c))
                    {
                        number *= 10;
                        number += toDigit(*c++);
                    }

                    // Unexpected end of string
                    assert(*c != 0);
                    // Invalid brace contents: must be a positive integer
                    assert(*c == '}');
                    // Format value index is out of range
                    assert(number <= N);

                    callback.onMarker(number);
                    ++c;
                } // if (c[1] == '{')
            }
            else if (*c == '}')
            {
                if (c[1] == '}')
                {
                    callback.onEscapeRight();
                    c += 2;
                }
                else
                {
                    // Unescaped right brace
                    assert(0);
                }
            }
            else
            {
                callback.onChar(*c);
                ++c;
            }
        }
    } // traverse

    template <int N>
    inline func formattedTotal(std::string values[N], int counts[N]) -> uint
    {
        uint total = 0;
        for (int i = 0; i < N; ++i)
        {
            total += values[i].length() * counts[i];
        }
        return total;
    }

    template <int N>
    inline func formatArray(const char* format, std::string values[N]) -> std::string
    {
        Counter<N> counter;
        traverse<N>(format, counter);

        uint formatsSize = formattedTotal<N>(values, counter.m_count);
        uint outputTotal = formatsSize + counter.m_braceEscapes + counter.m_plainChars;

        std::string output(outputTotal, 0);
        Formatter<N> formatter(output, values);
        traverse<N>(format, formatter);

        return output;
    }

    template <int N, typename T>
    inline func internalStringFormat(int index, std::string values[N], T t) -> void
    {
        std::ostringstream ss;
        ss << t;
        values[index] = ss.str();
    }

    template <int N, typename T, typename ...Args>
    inline func internalStringFormat(int index, std::string values[N], T t, Args... args) -> void
    {
        std::ostringstream ss;
        ss << t;
        values[index] = ss.str();
        internalStringFormat<N, Args...>(index + 1, values, args...);
    }

    template <int N>
    inline func internalStringFormat(int index, std::string values[N]) -> void
    {
    }

} // namespace

inline func stringFormat(const char* format) -> std::string
{
    return std::string(format);
}

template <typename ...Args>
inline func stringFormat(const char* format, Args... args) -> std::string
{
    std::string values[sizeof...(args)];
    internalStringFormat<sizeof...(args), Args...>(0, values, args...);
    return formatArray<sizeof...(args)>(format, values);
}

template <typename... Args>
inline func pr(Args... args) -> void
{
    std::string s = stringFormat(args...);
#if OS_WIN32
    OutputDebugStringA(s.c_str());
#endif
}

template <typename... Args>
inline func prn(Args... args) -> void
{
    pr(args...);
#if OS_WIN32
    OutputDebugStringA("\n");
#endif
}

