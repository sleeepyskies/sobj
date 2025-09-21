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
namespace detail
{
constexpr char DELIMITER                = '/';
constexpr char SPACE                    = ' ';
constexpr std::string ON                = "on";
constexpr std::string OFF               = "off";
constexpr std::string GROUP_NAME_PREFIX = "group";
} // namespace detail

//--------------------------------------------------
// MARK: Data Classes
//--------------------------------------------------

struct Vec3 {
    float x, y, z;
    void pushOnto(std::vector<float>& vector) const
    {
        vector.push_back(x);
        vector.push_back(y);
        vector.push_back(z);
    }
};

struct Vec2 {
    float x, y;
    void pushOnto(std::vector<float>& vector) const
    {
        vector.push_back(x);
        vector.push_back(y);
    }
};

struct Face {
    std::vector<uint32_t> positionIndices{};
    std::vector<uint32_t> normalIndices{};
    std::vector<uint32_t> uvIndices{};
    std::vector<uint32_t> colorIndices{};

    size_t numVertices() const
    {
        return positionIndices.size();
    }
};

struct Mesh {
    std::vector<Face> faces;
    std::string name{};
};

struct OBJData {
    std::vector<float> positions{};
    std::vector<float> normals{};
    std::vector<float> textureUVs{};
    std::vector<float> colors{};
    std::vector<Mesh> meshes{};
    std::string name{};
};

//--------------------------------------------------
// MARK: Utilities
//--------------------------------------------------

namespace detail
{
inline void trimLeft(std::string& str)
{
    str.erase(str.begin(),
              std::ranges::find_if(str, [](const unsigned char c) { return !std::isspace(c); }));
}

inline void trimRight(std::string& str)
{
    str.erase(std::find_if(
                  str.rbegin(), str.rend(), [](const unsigned char c) { return !std::isspace(c); })
                  .base(),
              str.end());
}

inline void trim(std::string& str)
{
    trimLeft(str);
    trimRight(str);
}

inline std::string fileNameFromPath(const std::string& path)
{
    return path.substr(path.find_last_of("/\\") + 1);
}

template <typename K, typename V> std::vector<V> values(const std::unordered_map<K, V>& map)
{
    std::vector<V> vec{};
    vec.reserve(map.size());
    for (const auto& pair : map) {
        vec.push_back(pair.second);
    }
    return vec;
}

template <typename K, typename V> std::vector<V> stealValues(std::unordered_map<K, V>& map)
{
    std::vector<V> vec{};
    vec.reserve(map.size());
    for (auto& pair : map) {
        vec.push_back(std::move(pair.second));
    }
    return vec;
}

} // namespace detail
//--------------------------------------------------
// MARK: OBJ Class Definition
//--------------------------------------------------
class OBJLoader
{
public:
    OBJLoader()  = default;
    ~OBJLoader() = default;

    bool load(const std::string& filePath);

    void setShouldTriangulate(bool b);

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
    enum class IndexType {
        POSITION, // m_positions
        NORMAL,   // m_normals
        UV,       // m_textureUVs
        COLOR,    // m_colors
        FACE      // ???
    };

    struct Config {
        // TODO
        enum TriangulationAlgorithm {
            NONE,
        };
        bool triangulate = true;
    };

    Config m_config{};

    uint32_t m_line = 0;
    std::string m_currentMeshName{};
    bool m_smoothShadingEnabled = false;

    std::vector<float> m_positions{};
    std::vector<float> m_normals{};
    std::vector<float> m_textureUVs{};
    std::vector<float> m_colors{}; // TODO
    std::unordered_map<std::string, Mesh> m_meshes{};

    std::string m_error;
    std::string m_warning;
    std::string m_filePath{};

    std::optional<Vec3> parseVec3(const std::string& str);
    std::optional<Vec2> parseVec2(const std::string& str);
    std::optional<Face> parseFace(const std::string& str);
    void parseSmoothShading(const std::string& str);
    void parseGroup(const std::string& str);

    Identifier identifier(std::string_view str) const;
    std::string toString(Identifier id) const;
    size_t calculateIndex(int index, IndexType type) const;
    void pushFace(const Face& face);
    void pushFaces(const std::vector<Face>& faces);
    std::vector<Face> triangulate(const Face& face) const;
    void shrink();
    void makeGroup(const std::string& name);
    void makeGroupAnonym();

    void reset();

    void info(const std::string& msg) const;
    void warn(const std::string& msg);
    void error(const std::string& msg);
};

#ifdef SOBJ_IMPLEMENTATION
//--------------------------------------------------
// MARK: OBJLoader Parsing methods
//--------------------------------------------------
bool OBJLoader::load(const std::string& filePath)
{
    reset();
    m_filePath = filePath;

    // open file, TODO(Error handling here?)
    std::ifstream file;
    file.open(filePath);

    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line)) {
        detail::trim(line);

        switch (identifier(line)) {
        case Identifier::POSITION: {
            const auto result = parseVec3(line);
            if (!result) return false;
            result->pushOnto(m_positions);
            break;
        }
        case Identifier::NORMAL: {
            const auto result = parseVec3(line);
            if (!result) return false;
            result->pushOnto(m_normals);
            break;
        }
        case Identifier::UV: {
            const auto result = parseVec2(line);
            if (!result) return false;
            result->pushOnto(m_textureUVs);
            break;
        }
        case Identifier::FACE: {
            const auto result = parseFace(line);
            if (!result) return false;
            if (m_config.triangulate) {
                pushFaces(triangulate(*result));
            } else {
                pushFace(*result);
            }
            break;
        }
        case Identifier::SMOOTH_SHADING: {
            parseSmoothShading(line);
            break;
        }
        case Identifier::NAMED_OBJECT:
        case Identifier::GROUP: {
            parseGroup(line);
            break;
        }
        case Identifier::LINE_ELEMENT:
            throw std::runtime_error("This functionality is not yet supported.");
        case Identifier::BLANK:
        case Identifier::COMMENT:
            break;
        case Identifier::UNKNOWN:
            warn(std::format(
                "Encountered unknown line identifier in file {} at line {}.", m_filePath, m_line));
            break;
        }

        m_line++;
    }

    file.close();

    if (m_positions.empty()) {
        error(std::format(".obj file {} must include at least 1 position", m_filePath));
        return false;
    }

    info(std::format("Successfully parsed and loaded data from {}", m_filePath));

    shrink();

    return true;
}

std::optional<Vec3> OBJLoader::parseVec3(const std::string& str)
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

std::optional<Vec2> OBJLoader::parseVec2(const std::string& str)
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

std::optional<Face> OBJLoader::parseFace(const std::string& str)
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
            if (slash1 != detail::DELIMITER || slash2 != detail::DELIMITER) {
                error(
                    std::format("Invalid syntax encountered in file {} at line {} ({} or "
                                "{} is not \\)",
                                m_filePath,
                                m_line,
                                slash1,
                                slash2));
            }
            face.positionIndices.push_back(calculateIndex(v, IndexType::POSITION));
            face.normalIndices.push_back(calculateIndex(vn, IndexType::NORMAL));
        }

        return { face };
    }

    if (str.find("/") != std::string::npos) {
        int32_t v, vt;
        char slash1;
        stream >> v >> slash1 >> vt;

        // v/vt/vn syntax
        if (stream.peek() == detail::DELIMITER) {
            char slash2;
            int32_t vn;
            stream >> slash2 >> vn;
            do {
                if (slash1 != detail::DELIMITER || slash2 != detail::DELIMITER) {
                    error(
                        std::format("Invalid syntax encountered in file {} at line {} ({} "
                                    "or {} is not \\)",
                                    m_filePath,
                                    m_line,
                                    slash1,
                                    slash2));
                }
                face.positionIndices.push_back(calculateIndex(v, IndexType::POSITION));
                face.uvIndices.push_back(calculateIndex(vt, IndexType::UV));
                face.normalIndices.push_back(calculateIndex(vn, IndexType::NORMAL));
            } while (stream >> v >> slash1 >> vt >> slash2 >> vn);

            return { face };
        }

        // v/vt syntax
        do {
            if (slash1 != detail::DELIMITER) {
                error(
                    std::format("Invalid syntax encountered in file {} at line {} ({} is "
                                "not \\)",
                                m_filePath,
                                m_line,
                                slash1));
            }
            face.positionIndices.push_back(calculateIndex(v, IndexType::POSITION));
            face.uvIndices.push_back(calculateIndex(vt, IndexType::UV));
        } while (stream >> v >> slash1 >> vt);

        return { face };
    }

    // v1 v2 v3 syntax
    int32_t v;
    while (stream >> v) {
        face.positionIndices.push_back(calculateIndex(v, IndexType::POSITION));
    }

    return { face };
}

void OBJLoader::parseSmoothShading(const std::string& str)
{
    std::stringstream stream{ str };
    std::string _;
    stream >> _;

    // word syntax
    if (stream.peek() == 'o') {
        std::string toggle{};
        stream >> toggle;
        if (toggle == detail::ON) {
            if (m_smoothShadingEnabled) return;
            makeGroupAnonym();
            m_smoothShadingEnabled = true;
        } else if (toggle == detail::OFF) {
            if (!m_smoothShadingEnabled) return;
            makeGroupAnonym();
            m_smoothShadingEnabled = false;
        } else {
            warn(std::format("Could not parse file {} line {} due to unknown word {}",
                             m_filePath,
                             m_line,
                             toggle));
        }
    }

    // number syntax
    int toggle;
    stream >> toggle;
    if (toggle) {
        if (m_smoothShadingEnabled) return;
        makeGroupAnonym();
        m_smoothShadingEnabled = true;
    } else {
        if (!m_smoothShadingEnabled) return;
        makeGroupAnonym();
        m_smoothShadingEnabled = false;
    }
}

void OBJLoader::parseGroup(const std::string& str)
{
    std::stringstream stream{ str };
    std::string _;
    std::string line{};
    stream >> _;
    std::getline(stream, line);
    makeGroup(line);
}

//--------------------------------------------------
// MARK: OBJLoader Non Parsing Methods
//--------------------------------------------------

OBJData OBJLoader::steal()
{
    OBJData data;
    data.positions  = std::move(m_positions);
    data.normals    = std::move(m_normals);
    data.textureUVs = std::move(m_textureUVs);
    data.colors     = std::move(m_colors);
    data.meshes     = detail::stealValues(m_meshes);
    data.name       = detail::fileNameFromPath(m_filePath);

    reset();

    return data;
}

OBJData OBJLoader::share() const
{
    OBJData data;
    data.positions  = m_positions;
    data.normals    = m_normals;
    data.textureUVs = m_textureUVs;
    data.colors     = m_colors;
    data.meshes     = detail::values(m_meshes);
    data.name       = detail::fileNameFromPath(m_filePath);

    return data;
}

void OBJLoader::reset()
{
    m_line = 0;
    m_currentMeshName.clear();
    m_filePath.clear();
    m_positions.clear();
    m_normals.clear();
    m_textureUVs.clear();
    m_colors.clear();
    m_meshes.clear();
    m_error.clear();
    m_warning.clear();
}

bool OBJLoader::warning() const
{
    return m_warning.size() == 0;
}

bool OBJLoader::error() const
{
    return m_error.size() == 0;
}

std::string OBJLoader::getWarning()
{
    return m_warning;
}

std::string OBJLoader::getError()
{
    return m_error;
}

//--------------------------------------------------
// MARK: Helper Methods
//--------------------------------------------------
OBJLoader::Identifier OBJLoader::identifier(const std::string_view str) const
{
    if (str.starts_with("v ")) return Identifier::POSITION;
    if (str.starts_with("vn ")) return Identifier::NORMAL;
    if (str.starts_with("vt ")) return Identifier::UV;
    if (str.starts_with("f ")) return Identifier::FACE;
    if (str.starts_with("g ")) return Identifier::GROUP;
    if (str.starts_with("o ")) return Identifier::NAMED_OBJECT;
    if (str.starts_with("l ")) return Identifier::LINE_ELEMENT;
    if (str.starts_with("s ")) return Identifier::SMOOTH_SHADING;
    if (str.starts_with("# ")) return Identifier::COMMENT;
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
    if (index > 0) return index - 1;

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
    default:
        // can never happen
        assert(false);
    }
    // can never happen
    assert(false);
}

void OBJLoader::pushFace(const Face& face)
{
    assert(m_meshes.contains(m_currentMeshName));
    m_meshes[m_currentMeshName].faces.push_back(face);
}

void OBJLoader::pushFaces(const std::vector<Face>& faces)
{
    assert(m_meshes.contains(m_currentMeshName));
    for (const auto& face : faces) {
        m_meshes[m_currentMeshName].faces.push_back(face);
    }
}

std::vector<Face> OBJLoader::triangulate(const Face& face) const
{
    // TODO: add support for more than 3 or 4 vertices. actual algorithm would be cool :D
    assert(m_config.triangulate);

    // already a tri
    if (face.numVertices() == 3) { return { face }; }
    if (face.numVertices() != 4) {
        throw std::runtime_error("Currently only quads and tris are supported");
    }

    std::vector<Face> faces{};

    Face f1{};
    Face f2{};
    // we turn p1 p2 p3 p4 into p1 p2 p3 + p1 p3 p4
    constexpr int indices1[] = { 0, 1, 2 };
    constexpr int indices2[] = { 0, 2, 3 };
    for (const int i : indices1) {
        f1.positionIndices.push_back(face.positionIndices[i]);
        if (!face.normalIndices.empty()) f1.normalIndices.push_back(face.normalIndices[i]);
        if (!face.colorIndices.empty()) f1.colorIndices.push_back(face.colorIndices[i]);
        if (!face.uvIndices.empty()) f1.uvIndices.push_back(face.uvIndices[i]);
    }
    for (const int i : indices2) {
        f2.positionIndices.push_back(face.positionIndices[i]);
        if (!face.normalIndices.empty()) f2.normalIndices.push_back(face.normalIndices[i]);
        if (!face.colorIndices.empty()) f2.colorIndices.push_back(face.colorIndices[i]);
        if (!face.uvIndices.empty()) f2.uvIndices.push_back(face.uvIndices[i]);
    }

    return faces;
}

void OBJLoader::shrink()
{
    m_positions.shrink_to_fit();
    m_normals.shrink_to_fit();
    m_textureUVs.shrink_to_fit();
    m_colors.shrink_to_fit();
    for (auto& [_, mesh] : m_meshes) {
        mesh.faces.shrink_to_fit();
    }
}

void OBJLoader::makeGroupAnonym()
{
    static size_t groupID = 0;
    assert(m_meshes.contains(m_currentMeshName));
    // only create new group if current group is not empty
    if (m_meshes[m_currentMeshName].faces.empty()) return;

    std::string name{};
    do {
        name = detail::GROUP_NAME_PREFIX + std::to_string(groupID);
    } while (m_meshes.contains(name));

    m_meshes[name]      = {};
    m_meshes[name].name = name;
    m_meshes[name].name = name;

    m_currentMeshName = name;
}

void OBJLoader::makeGroup(const std::string& name)
{
    m_currentMeshName = name;
    if (m_meshes.contains(name)) { return; }

    // always make a new group
    m_meshes[name]      = {};
    m_meshes[name].name = name;
    m_meshes[name].name = name;

    m_currentMeshName = name;
}

//--------------------------------------------------
// MARK: Configuration Methods
//--------------------------------------------------
void OBJLoader::setShouldTriangulate(const bool b)
{
    m_config.triangulate = b;
}

//--------------------------------------------------
// MARK: Logging
//--------------------------------------------------

void OBJLoader::warn(const std::string& msg)
{
    wrn(msg);
    m_warning += msg + '\n';
}

void OBJLoader::error(const std::string& msg)
{
    nfo(msg);
    m_error += msg + '\n';
}
void OBJLoader::info(const std::string& msg) const
{
    nfo(msg);
}

#endif

} // namespace sobj
