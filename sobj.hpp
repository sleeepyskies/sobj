// ReSharper disable CppNonInlineFunctionDefinitionInHeaderFile
#pragma once

#include <format>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>

// sobj can optionally use the logging library slog which can be found at
// https://github.com/sleeepyskies/slog
#ifdef SOBJ_USE_SLOG
#include "slog.hpp"
#else
#define nfo(fmt, ...)
#define wrn(fmt, ...)
#define err(fmt, ...)
#endif

namespace sobj
{
//--------------------------------------------------
// MARK: Constants
//--------------------------------------------------
constexpr char DELIMITER = '/';
constexpr char SPACE     = ' ';

//--------------------------------------------------
// MARK: Data Classes
//--------------------------------------------------

struct Vec3 {
    float x, y, z;
    void pushOnto(std::vector<float> &vector) const
    {
        vector.push_back(x);
        vector.push_back(y);
        vector.push_back(z);
    }
};

struct Vec2 {
    float x, y;
    void pushOnto(std::vector<float> &vector) const
    {
        vector.push_back(x);
        vector.push_back(y);
    }
};

struct Face {
    std::vector<uint32_t> positionIndices;
    std::vector<uint32_t> normalIndices;
    std::vector<uint32_t> uvIndices;
    std::vector<uint32_t> colorIndices;

    size_t numVertices() const { return positionIndices.size(); }
};

struct Mesh {
    std::vector<Face> faces;
};

struct OBJData {
    std::vector<float> positions{};
    std::vector<float> normals{};
    std::vector<float> textureUVs{};
    std::vector<float> colors{};
    std::vector<Face> faces{};
};

//--------------------------------------------------
// MARK: String Utilities
//--------------------------------------------------

inline void trimLeft(std::string &str)
{
    str.erase(str.begin(),
              std::ranges::find_if(str, [](const unsigned char c) { return !std::isspace(c); }));
}

inline void trimRight(std::string &str)
{
    str.erase(std::find_if(
                  str.rbegin(), str.rend(), [](const unsigned char c) { return !std::isspace(c); })
                  .base(),
              str.end());
}

inline void trim(std::string &str)
{
    trimLeft(str);
    trimRight(str);
}

//--------------------------------------------------
// MARK: OBJ Class Definition
//--------------------------------------------------
class OBJLoader
{
public:
    OBJLoader()  = default;
    ~OBJLoader() = default;

    bool load(const std::string &filePath);

    OBJData steal();
    OBJData share() const;

    bool warning() const;
    bool error() const;
    std::string getWarning();
    std::string getError();

private:
    /// @brief Indicates what the type of the line in the obj file is.
    enum class Identifier {
        POSITION,       // v
        NORMAL,         // vn
        UV,             // vt
        FACE,           // f
        GROUP,          // g
        NAMED_OBJECT,   // o
        LINE_ELEMENT,   // l
        SMOOTH_SHADING, // s
        COMMENT,        // #
        BLANK,          // empty line
        UNKNOWN,        // ????
    };

    /// @brief Used when calculating index to determine which vector is relevant
    enum class IndexType { POSITION, NORMAL, UV, COLOR, FACE };

    uint32_t m_line = 0;
    std::string m_filePath{};

    std::vector<float> m_positions{};
    std::vector<float> m_normals{};
    std::vector<float> m_textureUVs{};
    std::vector<float> m_colors{}; // TODO
    std::vector<Face> m_faces{};

    std::string m_error;
    std::string m_warning;

    std::optional<Vec3> parseVec3(const std::string &str);
    std::optional<Vec2> parseVec2(const std::string &str);
    std::optional<Face> parseFace(const std::string &str);

    Identifier identifier(std::string_view str) const;
    std::string toString(Identifier id) const;
    size_t calculateIndex(int index, IndexType type) const;
    size_t index(int index) const;

    void reset();

    void info(const std::string &msg) const;
    void warn(const std::string &msg);
    void error(const std::string &msg);
};

// #ifdef SOBJ_IMPLEMENTATION
//--------------------------------------------------
// MARK: OBJLoader Parsing methods
//--------------------------------------------------
bool OBJLoader::load(const std::string &filePath)
{
    reset();
    m_filePath = filePath;

    // open file, TODO(Error handling here?)
    std::ifstream file;
    file.open(filePath);

    if (!file.is_open())
        return false;

    std::string line;
    while (std::getline(file, line)) {
        trim(line);

        switch (identifier(line)) {
        case Identifier::POSITION: {
            const auto result = parseVec3(line);
            if (!result)
                return false;
            result->pushOnto(m_positions);
            break;
        }
        case Identifier::NORMAL: {
            const auto result = parseVec3(line);
            if (!result)
                return false;
            result->pushOnto(m_normals);
            break;
        }
        case Identifier::UV: {
            const auto result = parseVec2(line);
            if (!result)
                return false;
            result->pushOnto(m_textureUVs);
            break;
        }
        case Identifier::FACE: {
            const auto result = parseFace(line);
            if (!result)
                return false;
            if (result->numVertices() != 3)
                throw std::runtime_error("More than 3 vertices per face not yet supported!");
            m_faces.push_back(*result);
            break;
        }
        case Identifier::GROUP:
        case Identifier::NAMED_OBJECT:
        case Identifier::LINE_ELEMENT:
            throw std::runtime_error("This functionality is not yet supported.");
        case Identifier::SMOOTH_SHADING: // TODO add this
        case Identifier::BLANK:
        case Identifier::COMMENT:
            break;
        case Identifier::UNKNOWN:
            warn(std::format(
                "Encountered unknown line identifier in file {} at line {}.", m_filePath, m_line));
            return false;
        }

        m_line++;
    }

    file.close();

    if (m_positions.empty()) {
        error(std::format(".obj file {} must include at least 1 position", m_filePath));
        return false;
    }

    info(std::format("Successfully parsed and loaded data from {}", m_filePath));

    return true;
}

std::optional<Vec3> OBJLoader::parseVec3(const std::string &str)
{
    // TODO: handle too many args? what about comments inline
    std::stringstream stream{ str };
    float x, y, z;
    std::string _;
    stream >> _ >> x >> y >> z;
    if (stream.fail()) {
        error(std::format("An error occurred when parsing {} at line {}", m_filePath, m_line));
        return std::nullopt;
    }

    return { { x, y, z } };
}

std::optional<Vec2> OBJLoader::parseVec2(const std::string &str)
{
    // TODO: handle too many args? what about comments inline
    std::stringstream stream{ str };
    float x, y;
    std::string _;
    stream >> _ >> x >> y;
    if (stream.fail()) {
        error(std::format("An error occurred when parsing {} at line {}", m_filePath, m_line));
        return std::nullopt;
    }

    return { { x, y } };
}

std::optional<Face> OBJLoader::parseFace(const std::string &str)
{
    std::stringstream stream{ str };
    Face face;
    std::string _;
    stream >> _;

    // v//vn syntax
    if (str.find("//") != std::string::npos) {
        int32_t v, vn;
        char slash1, slash2;

        while (stream >> v >> slash1 >> slash2 >> vn) {
            if (slash1 != DELIMITER || slash2 != DELIMITER) {
                error(
                    std::format("Invalid syntax encountered in file {} at line {} ({} or "
                                "{} is not \\)",
                                m_filePath,
                                m_line,
                                slash1,
                                slash2));
            }
            face.positionIndices.push_back(index(v));
            face.normalIndices.push_back(index(vn));
        }

        return { face };
    }

    if (str.find("/") != std::string::npos) {
        int32_t v, vt;
        char slash1;
        stream >> v >> slash1 >> vt;

        // v/vt/vn syntax
        if (stream.peek() == DELIMITER) {
            char slash2;
            int32_t vn;
            stream >> slash2 >> vn;
            do {
                if (slash1 != DELIMITER || slash2 != DELIMITER) {
                    error(
                        std::format("Invalid syntax encountered in file {} at line {} ({} "
                                    "or {} is not \\)",
                                    m_filePath,
                                    m_line,
                                    slash1,
                                    slash2));
                }
                face.positionIndices.push_back(index(v));
                face.uvIndices.push_back(index(vt));
                face.normalIndices.push_back(index(vn));
            } while (stream >> v >> slash1 >> vt >> slash2 >> vn);

            return { face };
        }

        // v/vt syntax
        do {
            if (slash1 != DELIMITER) {
                error(
                    std::format("Invalid syntax encountered in file {} at line {} ({} is "
                                "not \\)",
                                m_filePath,
                                m_line,
                                slash1));
            }
            face.positionIndices.push_back(index(v));
            face.uvIndices.push_back(index(vt));
        } while (stream >> v >> slash1 >> vt);

        return { face };
    }

    // v1 v2 v3 syntax
    int32_t v;
    while (stream >> v) {
        face.positionIndices.push_back(index(v));
    }

    return { face };
}

//--------------------------------------------------
// MARK: OBJLoader Non Parsing Methods
//--------------------------------------------------

OBJData OBJLoader::steal()
{
    OBJData pr;
    pr.positions  = std::move(m_positions);
    pr.normals    = std::move(m_normals);
    pr.textureUVs = std::move(m_textureUVs);
    pr.colors     = std::move(m_colors);
    pr.faces      = std::move(m_faces);

    reset();

    return pr;
}

OBJData OBJLoader::share() const
{
    OBJData pr;
    pr.positions  = m_positions;
    pr.normals    = m_normals;
    pr.textureUVs = m_textureUVs;
    pr.colors     = m_colors;
    pr.faces      = m_faces;

    return pr;
}

void OBJLoader::reset()
{
    m_line = 0;
    m_filePath.clear();
    m_positions.clear();
    m_normals.clear();
    m_textureUVs.clear();
    m_colors.clear();
    m_faces.clear();
    m_error.clear();
    m_warning.clear();
}

bool OBJLoader::warning() const { return m_warning.size() == 0; }

bool OBJLoader::error() const { return m_error.size() == 0; }

std::string OBJLoader::getWarning() { return m_warning; }

std::string OBJLoader::getError() { return m_error; }

//--------------------------------------------------
// MARK: Helper Methods
//--------------------------------------------------
OBJLoader::Identifier OBJLoader::identifier(const std::string_view str) const
{
    if (str.starts_with("v "))
        return Identifier::POSITION;
    if (str.starts_with("vn "))
        return Identifier::NORMAL;
    if (str.starts_with("vt "))
        return Identifier::UV;
    if (str.starts_with("f "))
        return Identifier::FACE;
    if (str.starts_with("g "))
        return Identifier::GROUP;
    if (str.starts_with("o "))
        return Identifier::NAMED_OBJECT;
    if (str.starts_with("l "))
        return Identifier::LINE_ELEMENT;
    if (str.starts_with("s "))
        return Identifier::SMOOTH_SHADING;
    if (str.starts_with("# "))
        return Identifier::COMMENT;
    if (str.empty())
        return Identifier::BLANK; // we trim before this call so it will always be empty

    return Identifier::UNKNOWN;
}

std::string OBJLoader::toString(const Identifier id) const
{
    switch (id) {
    case Identifier::POSITION:
        return "VERTEX";
    case Identifier::NORMAL:
        return "NORMAL";
    case Identifier::UV:
        return "UV";
    case Identifier::FACE:
        return "FACE";
    case Identifier::GROUP:
        return "GROUP";
    case Identifier::NAMED_OBJECT:
        return "NAMED_OBJECT";
    case Identifier::LINE_ELEMENT:
        return "LINE_ELEMENT";
    case Identifier::SMOOTH_SHADING:
        return "SMOOTH_SHADING";
    case Identifier::COMMENT:
        return "COMMENT";
    case Identifier::BLANK:
        return "BLANK";
    case Identifier::UNKNOWN:
    default:
        return "UNKNOWN";
    }
}

size_t OBJLoader::calculateIndex(const int index, const IndexType type) const
{
    // TODO: handle invalid negative indices here as well as positve indieces
    if (index > 0)
        return index - 1;

    switch (type) {
    case IndexType::POSITION:
        m_positions.size() - abs(index);
        break;
    case IndexType::NORMAL:
        m_normals.size() - abs(index);
        break;
    case IndexType::UV:
        m_textureUVs.size() - abs(index);
        break;
    case IndexType::COLOR:
        m_colors.size() - abs(index);
        break;
    case IndexType::FACE:
        m_positions.size() - abs(index);
        break;
    }
}

void OBJLoader::warn(const std::string &msg)
{
    wrn(msg);
    m_warning = msg;
}

void OBJLoader::error(const std::string &msg)
{
    nfo(msg);
    m_error = msg;
}
void OBJLoader::info(const std::string &msg) const { nfo(msg); }

// #endif

} // namespace sobj
