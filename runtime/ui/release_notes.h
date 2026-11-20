#pragma once
// Plain-text softener for GitHub release bodies (Play/Settings What's new + --check-updates).
#include <string>
#include <string_view>

// Strip common Markdown noise (line-leading only) and cap length for UI / CLI.
inline std::string SoftenReleaseBody(std::string body, size_t maxChars = 900)
{
    for (char& c : body)
        if (c == '\r') c = '\n';
    // Drop HTML comments.
    std::string cleaned;
    cleaned.reserve(body.size());
    for (size_t i = 0; i < body.size();)
    {
        if (i + 3 < body.size() && body[i] == '<' && body[i + 1] == '!' && body[i + 2] == '-' && body[i + 3] == '-')
        {
            const auto end = body.find("-->", i + 4);
            i = end == std::string::npos ? body.size() : end + 3;
            continue;
        }
        cleaned.push_back(body[i++]);
    }
    std::string plain;
    plain.reserve(cleaned.size());
    size_t lineStart = 0;
    auto flushLine = [&](size_t end) {
        std::string_view line(cleaned.data() + lineStart, end - lineStart);
        size_t i = 0;
        while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
        // Line-leading markdown only — keep mid-line hyphens / asterisks.
        if (i < line.size() && (line[i] == '#' || line[i] == '>' || line[i] == '-' || line[i] == '*'))
        {
            char mark = line[i];
            if (mark == '-' || mark == '*')
            {
                // Require bullet form: "- " / "* " (not "well-known").
                if (i + 1 >= line.size() || line[i + 1] != ' ')
                {
                    plain.append(line.data() + i, line.size() - i);
                    plain.push_back('\n');
                    return;
                }
            }
            while (i < line.size() && (line[i] == '#' || line[i] == '>' || line[i] == '-' || line[i] == '*' ||
                                       line[i] == ' ' || line[i] == '\t'))
                ++i;
        }
        // [text](url) → text
        for (; i < line.size();)
        {
            if (line[i] == '[')
            {
                const auto close = line.find(']', i + 1);
                if (close != std::string_view::npos)
                {
                    plain.append(line.data() + i + 1, close - i - 1);
                    i = close + 1;
                    if (i < line.size() && line[i] == '(')
                    {
                        const auto endParen = line.find(')', i + 1);
                        i = endParen == std::string_view::npos ? line.size() : endParen + 1;
                    }
                    continue;
                }
            }
            plain.push_back(line[i++]);
        }
        plain.push_back('\n');
    };
    for (size_t i = 0; i < cleaned.size(); ++i)
    {
        if (cleaned[i] == '\n')
        {
            flushLine(i);
            lineStart = i + 1;
        }
    }
    if (lineStart < cleaned.size()) flushLine(cleaned.size());
    while (!plain.empty() && (plain.back() == '\n' || plain.back() == ' '))
        plain.pop_back();
    // Collapse runs of blank lines.
    std::string collapsed;
    collapsed.reserve(plain.size());
    int blankRun = 0;
    for (char c : plain)
    {
        if (c == '\n')
        {
            if (++blankRun <= 2) collapsed.push_back(c);
        }
        else
        {
            blankRun = 0;
            collapsed.push_back(c);
        }
    }
    if (collapsed.size() > maxChars)
    {
        collapsed.resize(maxChars);
        while (!collapsed.empty() && collapsed.back() != ' ' && collapsed.back() != '\n')
            collapsed.pop_back();
        collapsed += "…";
    }
    return collapsed;
}
