// ReSharper disable CppNonInlineFunctionDefinitionInHeaderFile
#pragma once

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <sstream>
#include <stb_image.hpp>
#include <string>
#include <unordered_map>
#include <vector>

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
};

struct Vec2 {
    float x, y;
};

struct ImageData {
    std::string name{};
    std::vector<unsigned char> bytes{};
    int width    = 0;
    int height   = 0;
    int channels = 0;
};

struct Material {
    std::string name{};

    // texture map data

    std::unique_ptr<ImageData> ambientMap   = nullptr; // Ka
    std::unique_ptr<ImageData> diffuseMap   = nullptr; // Kd
    std::unique_ptr<ImageData> specularMap  = nullptr; // Ks
    std::unique_ptr<ImageData> roughnessMap = nullptr; // Ns
    std::unique_ptr<ImageData> alphaMap     = nullptr; // d

    Vec3 ambient{ -1 };
    Vec3 diffuse{ -1 };
    Vec3 specular{ -1 };
    float roughness = -1;
    float alpha     = -1;
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
    std::string name{};
    std::vector<Face> faces{};
    std::shared_ptr<Material> material = nullptr;
};

struct OBJData {
    std::vector<Vec3> positions{};
    std::vector<Vec3> normals{};
    std::vector<Vec2> textureUVs{};
    std::vector<Vec3> colors{};
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
// MARK: Class Definition
//--------------------------------------------------

class sobjLogger
{

public:
    bool existsError() const;
    bool existsWarning() const;
    void error(const std::string& msg);
    void warn(const std::string& msg);
    void info(const std::string& msg);
    std::vector<std::string> getErrors();
    std::vector<std::string> getWarnings();
    std::vector<std::string> getInfos();
    void clear();

private:
    std::vector<std::string> m_errors{};
    std::vector<std::string> m_warnings{};
    std::vector<std::string> m_infos{};
};

class MathParser
{
public:
    MathParser()  = default;
    ~MathParser() = default;

    std::optional<Vec3> parseVec3(const std::string& str) const;
    std::optional<Vec2> parseVec2(const std::string& str) const;
    std::optional<float> parseFloat(const std::string& str) const;
};

class MTLLoader
{
public:
    MTLLoader(const std::shared_ptr<sobjLogger>& logger) : m_logger(logger)
    {
    }
    ~MTLLoader() = default;

    bool loadMaterialFile(const std::string& filePath);
    void reset();

    std::unordered_map<std::string, std::shared_ptr<Material>> stealMaterials();

private:
    /// @brief Indicates what the type of the line in the mtl file is.
    enum class Identifier {
        NEW_MATERIAL,  // newmtl
                       //
        AMBIENT_MAP,   // map_Ka
        DIFFUSE_MAP,   // map_Kd
        SPECULAR_MAP,  // map_Ks
        ROUGHNESS_MAP, // map_Ns
        ALPHA_MAP,     // map_d
                       //
        AMBIENT,       // Ka
        DIFFUSE,       // Kd
        SPECULAR,      // Ks
        ROUGHNESS,     // Ns
        ALPHA,         // d
                       //
        COMMENT,       // #
        BLANK,         // empty line
        UNKNOWN,       // ????
    };

    MathParser m_mathParser{};

    std::unordered_map<std::string, std::shared_ptr<Material>> m_materials{};
    std::string m_currentMaterial{};

    std::string m_filePath{};
    std::string m_workingDirectory{};
    size_t m_line = 0;

    std::shared_ptr<sobjLogger> m_logger = nullptr;

    bool parseNewMaterial(const std::string& str);
    std::optional<ImageData> parseImage(const std::string& str) const;

    bool setImageMap(std::unique_ptr<ImageData>& imageMap, const std::string& line,
                     Identifier identifier);

    Identifier identifier(std::string_view str) const;
    std::string toString(Identifier identifier) const;
};

class OBJLoader
{
public:
    OBJLoader()  = default;
    ~OBJLoader() = default;

    bool load(const std::string& filePath);

    void setShouldTriangulate(bool b);

    OBJData steal();
    OBJData share() const;

    std::vector<std::string> getErrors() const;
    std::vector<std::string> getWarnings() const;
    std::vector<std::string> getInfos() const;
    bool existsError() const;
    bool existsWarning() const;

private:
    /// @brief Indicates what the type of the line in the obj file is.
    enum class Identifier {
        POSITION,       // v
        NORMAL,         // vn
        UV,             // vt
        FACE,           // f
        GROUP,          // g
        NAMED_OBJECT,   // o
        SMOOTH_SHADING, // s
        MATERIAL_LIB,   // mtllib
        USE_MATERIAL,   // usemtl
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

    std::shared_ptr<sobjLogger> m_logger = std::make_shared<sobjLogger>();

    uint32_t m_line = 0;
    std::string m_currentMeshName{};
    bool m_smoothShadingEnabled = false;

    std::vector<Vec3> m_positions{};
    std::vector<Vec3> m_normals{};
    std::vector<Vec2> m_textureUVs{};
    std::vector<Vec3> m_colors{}; // TODO
    std::unordered_map<std::string, Mesh> m_meshes{};
    std::unordered_map<std::string, std::shared_ptr<Material>> m_materials{};

    std::string m_filePath{};
    std::string m_workingDirectory{};

    MathParser m_mathParser{};
    MTLLoader m_mtlLoader{ m_logger };

    std::optional<Face> parseFace(const std::string& str);
    void parseSmoothShading(const std::string& str);
    void parseGroup(const std::string& str);
    std::optional<std::string> parseMaterialFilePath(const std::string& str) const;
    bool parseUseMaterial(const std::string& str);

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
};

#ifdef SOBJ_IMPLEMENTATION
//--------------------------------------------------
// MARK: MTLLoader Parsing methods
//--------------------------------------------------
bool MTLLoader::loadMaterialFile(const std::string& filePath)
{
    m_filePath = filePath;
    detail::trim(m_filePath);

    if (!m_filePath.ends_with(".mtl")) { return false; }

    std::filesystem::path objPath = m_filePath;
    m_workingDirectory            = objPath.parent_path().string() + "/";

    std::ifstream file;
    file.open(filePath);

    if (!file.is_open()) { return false; }

    std::string line;
    while (std::getline(file, line)) {
        detail::trim(line);

        const Identifier id = identifier(line);
        switch (id) {
        case Identifier::NEW_MATERIAL: {
            if (!parseNewMaterial(line)) return false;
            break;
        }
        case Identifier::AMBIENT_MAP: {
            if (!setImageMap(m_materials[m_currentMaterial]->ambientMap, line, id)) {
                return false;
            }
            break;
        }
        case Identifier::DIFFUSE_MAP: {
            if (!setImageMap(m_materials[m_currentMaterial]->diffuseMap, line, id)) {
                return false;
            }
            break;
        }
        case Identifier::SPECULAR_MAP: {
            if (!setImageMap(m_materials[m_currentMaterial]->specularMap, line, id)) {
                return false;
            }
            break;
        }
        case Identifier::ROUGHNESS_MAP: {
            if (!setImageMap(m_materials[m_currentMaterial]->roughnessMap, line, id)) {
                return false;
            }
            break;
        }
        case Identifier::ALPHA_MAP: {
            if (!setImageMap(m_materials[m_currentMaterial]->ambientMap, line, id)) {
                return false;
            }
            break;
        }
        case Identifier::AMBIENT: {
            const auto result = m_mathParser.parseVec3(line);
            if (!result) {
                m_logger->error(std::format(
                    "An error occurred when parsing {} at line {}", m_filePath, m_line));
                return false;
            }
            m_materials[m_currentMaterial]->ambient = *result;
            break;
        }
        case Identifier::DIFFUSE: {
            const auto result = m_mathParser.parseVec3(line);
            if (!result) {
                m_logger->error(std::format(
                    "An error occurred when parsing {} at line {}", m_filePath, m_line));
                return false;
            }
            m_materials[m_currentMaterial]->diffuse = *result;
            break;
        }
        case Identifier::SPECULAR: {
            const auto result = m_mathParser.parseVec3(line);
            if (!result) {
                m_logger->error(std::format(
                    "An error occurred when parsing {} at line {}", m_filePath, m_line));
                return false;
            }
            m_materials[m_currentMaterial]->specular = *result;
            break;
        }
        case Identifier::ROUGHNESS: {
            const auto result = m_mathParser.parseFloat(line);
            if (!result) {
                m_logger->error(std::format(
                    "An error occurred when parsing {} at line {}", m_filePath, m_line));
                return false;
            }
            m_materials[m_currentMaterial]->roughness = *result;
            break;
        }
        case Identifier::ALPHA: {
            const auto result = m_mathParser.parseFloat(line);
            if (!result) {
                m_logger->error(std::format(
                    "An error occurred when parsing {} at line {}", m_filePath, m_line));
                return false;
            }
            m_materials[m_currentMaterial]->alpha = *result;
            break;
        }
        case Identifier::COMMENT:
        case Identifier::BLANK:
            break;
        case Identifier::UNKNOWN:
            m_logger->warn(
                std::format("Unknown identifier encountered in {} at line {}", m_filePath, m_line));
            break;
        }

        m_line++;
    }

    return true;
}

bool MTLLoader::parseNewMaterial(const std::string& str)
{
    std::stringstream stream{ str };
    std::string _;
    std::string name;

    stream >> _ >> name;

    if (stream.fail()) { return false; }
    assert(!m_materials.contains(name));

    m_materials[name] = std::make_shared<Material>(name);
    m_currentMaterial = name;

    return true;
}

std::optional<ImageData> MTLLoader::parseImage(const std::string& str) const
{
    std::stringstream stream{ str };
    std::string _;
    std::string path;

    stream >> _ >> path;

    detail::trim(path);
    const std::string name = detail::fileNameFromPath(path);

    if (stream.fail()) { return std::nullopt; }

    int x, y, channels;
    const std::string relativePath = m_workingDirectory + path;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* bytes = stbi_load(relativePath.c_str(), &x, &y, &channels, STBI_default);
    // TODO: error check here
    const size_t size = x * y * channels;

    ImageData data;
    data.name     = name;
    data.bytes    = std::vector(bytes, bytes + size);
    data.height   = x;
    data.width    = y;
    data.channels = channels;

    stbi_image_free(bytes);

    return data;
}

bool MTLLoader::setImageMap(std::unique_ptr<ImageData>& imageMap, const std::string& line,
                            const Identifier identifier)
{
    const auto result = parseImage(line);
    if (!result) return false;
    if (m_materials.empty()) return false;
    assert(m_materials.contains(m_currentMaterial));
    if (imageMap) {
        m_logger->warn(std::format("Defined two {} image maps in file {} at line {}",
                                   toString(identifier),
                                   m_filePath,
                                   m_line));
    }
    imageMap = std::make_unique<ImageData>(*result);

    return true;
}

//--------------------------------------------------
// MARK: OBJLoader Parsing methods
//--------------------------------------------------
bool OBJLoader::load(const std::string& filePath)
{
    reset();

    detail::trim(m_filePath);
    m_filePath = filePath;

    std::filesystem::path objPath = m_filePath;
    m_workingDirectory            = objPath.parent_path().string() + "/";

    if (!m_filePath.ends_with(".obj")) {
        m_logger->error(std::format("The file {} does not have the .obj extension", m_filePath));
        return false;
    }

    // open file, TODO(Error handling here?)
    std::ifstream file;
    file.open(filePath);

    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line)) {
        detail::trim(line);

        switch (identifier(line)) {
        case Identifier::POSITION: {
            const auto result = m_mathParser.parseVec3(line);
            if (!result) {
                m_logger->error(std::format(
                    "An error occurred when parsing {} at line {}", m_filePath, m_line));
                return false;
            }
            m_positions.push_back(*result);
            break;
        }
        case Identifier::NORMAL: {
            const auto result = m_mathParser.parseVec3(line);
            if (!result) {
                m_logger->error(std::format(
                    "An error occurred when parsing {} at line {}", m_filePath, m_line));
                return false;
            }
            m_normals.push_back(*result);
            break;
        }
        case Identifier::UV: {
            const auto result = m_mathParser.parseVec2(line);
            if (!result) {
                m_logger->error(std::format(
                    "An error occurred when parsing {} at line {}", m_filePath, m_line));
                return false;
            }
            m_textureUVs.push_back(*result);
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
        case Identifier::MATERIAL_LIB: {
            const auto result = parseMaterialFilePath(line);
            if (!result) return false;
            m_mtlLoader.loadMaterialFile(m_workingDirectory + *result); // look in this dir
            m_materials = m_mtlLoader.stealMaterials();
            break;
        }
        case Identifier::USE_MATERIAL: {
            parseUseMaterial(line);
            break;
        }
        case Identifier::BLANK:
        case Identifier::COMMENT:
            break;
        case Identifier::UNKNOWN:
            m_logger->warn(std::format(
                "Encountered unknown line identifier in file {} at line {}.", m_filePath, m_line));
            break;
        }

        m_line++;
    }

    file.close();

    if (m_positions.empty()) {
        m_logger->error(std::format(".obj file {} must include at least 1 position", m_filePath));
        return false;
    }

    m_logger->info(std::format("Successfully parsed and loaded data from {}", m_filePath));

    shrink();

    return true;
}

std::optional<Vec3> MathParser::parseVec3(const std::string& str) const
{
    // TODO: handle too many args? what about comments inline
    std::stringstream stream{ str };
    float x, y, z;
    std::string _;
    stream >> _ >> x >> y >> z;
    if (stream.fail()) { return std::nullopt; }

    return { { x, y, z } };
}

std::optional<Vec2> MathParser::parseVec2(const std::string& str) const
{
    // TODO: handle too many args? what about comments inline
    std::stringstream stream{ str };
    float x, y;
    std::string _;
    stream >> _ >> x >> y;
    if (stream.fail()) { return std::nullopt; }

    return { { x, y } };
}

std::optional<float> MathParser::parseFloat(const std::string& str) const
{
    // TODO: handle too many args? what about comments inline
    std::stringstream stream{ str };
    float x;
    std::string _;
    stream >> _ >> x;
    if (stream.fail()) { return std::nullopt; }

    return { x };
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
                m_logger->error(
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
                    m_logger->error(
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
                m_logger->error(
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
            m_logger->warn(std::format("Could not parse file {} line {} due to unknown word {}",
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

std::optional<std::string> OBJLoader::parseMaterialFilePath(const std::string& str) const
{
    std::stringstream stream{ str };
    std::string _;
    std::string path{};
    stream >> _;
    std::getline(stream, path);
    detail::trim(path);
    return { path };
}

bool OBJLoader::parseUseMaterial(const std::string& str)
{
    if (m_meshes.empty()) { return false; }

    std::stringstream stream{ str };
    std::string _;
    std::string name;
    stream >> _;
    std::getline(stream, name);
    detail::trim(name);

    if (stream.fail()) { return false; }

    assert(m_meshes.contains(m_currentMeshName));
    if (!m_materials.contains(name)) { return false; }

    m_meshes[m_currentMeshName].material = m_materials[name];

    return true;
}

//--------------------------------------------------
// MARK: MTLLoader Helper Methods
//--------------------------------------------------

std::string MTLLoader::toString(const Identifier identifier) const
{
    switch (identifier) {
    case Identifier::NEW_MATERIAL:
        return "newmtl";
    case Identifier::AMBIENT_MAP:
        return "map_Ka";
    case Identifier::DIFFUSE_MAP:
        return "map_Kd";
    case Identifier::SPECULAR_MAP:
        return "map_Ks";
    case Identifier::ROUGHNESS_MAP:
        return "map_Ns";
    case Identifier::ALPHA_MAP:
        return "map_d";
    case Identifier::AMBIENT:
        return "Ka";
    case Identifier::DIFFUSE:
        return "Kd";
    case Identifier::SPECULAR:
        return "Ks";
    case Identifier::ROUGHNESS:
        return "Ns";
    case Identifier::ALPHA:
        return "d";
    case Identifier::COMMENT:
        return "#";
    case Identifier::BLANK:
        return "";
    case Identifier::UNKNOWN:
        return "unknown";
    default:
        return "invalid";
    }
}

MTLLoader::Identifier MTLLoader::identifier(const std::string_view str) const
{
    if (str.starts_with("newmtl ")) return Identifier::NEW_MATERIAL;

    if (str.starts_with("map_Ka ")) return Identifier::AMBIENT_MAP;
    if (str.starts_with("map_Kd ")) return Identifier::DIFFUSE_MAP;
    if (str.starts_with("map_Ks ")) return Identifier::SPECULAR_MAP;
    if (str.starts_with("map_Ns ")) return Identifier::ROUGHNESS_MAP;
    if (str.starts_with("map_d ")) return Identifier::ALPHA_MAP;

    if (str.starts_with("Ka ")) return Identifier::AMBIENT;
    if (str.starts_with("Kd ")) return Identifier::DIFFUSE;
    if (str.starts_with("Ks ")) return Identifier::SPECULAR;
    if (str.starts_with("Ns ")) return Identifier::ROUGHNESS;
    if (str.starts_with("d ")) return Identifier::ALPHA;

    if (str.starts_with("# ")) return Identifier::COMMENT;
    if (str.empty()) return Identifier::BLANK;

    return Identifier::UNKNOWN;
}

std::unordered_map<std::string, std::shared_ptr<Material>> MTLLoader::stealMaterials()
{
    return std::move(m_materials);
}

void MTLLoader::reset()
{
    m_materials.clear();
    m_filePath.clear();
    m_line = 0;
}

//--------------------------------------------------
// MARK: OBJLoader Helper Methods
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
    m_logger->clear();
}

OBJLoader::Identifier OBJLoader::identifier(const std::string_view str) const
{
    if (str.starts_with("v ")) return Identifier::POSITION;
    if (str.starts_with("vn ")) return Identifier::NORMAL;
    if (str.starts_with("vt ")) return Identifier::UV;
    if (str.starts_with("f ")) return Identifier::FACE;
    if (str.starts_with("g ")) return Identifier::GROUP;
    if (str.starts_with("o ")) return Identifier::NAMED_OBJECT;
    if (str.starts_with("s ")) return Identifier::SMOOTH_SHADING;
    if (str.starts_with("mtllib ")) return Identifier::MATERIAL_LIB;
    if (str.starts_with("usemtl ")) return Identifier::USE_MATERIAL;
    if (str.starts_with("# ")) return Identifier::COMMENT;
    if (str.empty())
        return Identifier::BLANK; // we trim before this call so it will always be empty

    return Identifier::UNKNOWN;
}

std::string OBJLoader::toString(const Identifier id) const
{
    switch (id) {
    case Identifier::POSITION:
        return "v";
    case Identifier::NORMAL:
        return "vn";
    case Identifier::UV:
        return "vt";
    case Identifier::FACE:
        return "f";
    case Identifier::GROUP:
        return "g";
    case Identifier::NAMED_OBJECT:
        return "o";
    case Identifier::SMOOTH_SHADING:
        return "s";
    case Identifier::MATERIAL_LIB:
        return "mtllib";
    case Identifier::USE_MATERIAL:
        return "usemtl";
    case Identifier::COMMENT:
        return "#";
    case Identifier::BLANK:
        return "";
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

    return { f1, f2 };
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
    std::string name_ = name;
    detail::trim(name_);
    m_currentMeshName = name_;
    if (m_meshes.contains(name)) { return; }

    // always make a new group
    m_meshes[name_]      = {};
    m_meshes[name_].name = name_;
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

bool OBJLoader::existsError() const
{
    return m_logger->existsError();
}

bool OBJLoader::existsWarning() const
{
    return m_logger->existsWarning();
}

std::vector<std::string> OBJLoader::getInfos() const
{
    return m_logger->getInfos();
}

std::vector<std::string> OBJLoader::getErrors() const
{
    return m_logger->getErrors();
}

std::vector<std::string> OBJLoader::getWarnings() const
{
    return m_logger->getWarnings();
}

bool sobjLogger::existsError() const
{
    return !m_errors.empty();
}

bool sobjLogger::existsWarning() const
{
    return !m_warnings.empty();
}

void sobjLogger::error(const std::string& msg)
{
    err(msg);
    m_errors.push_back(msg);
}

void sobjLogger::warn(const std::string& msg)
{
    wrn(msg);
    m_warnings.push_back(msg);
}

void sobjLogger::info(const std::string& msg)
{
    nfo(msg);
    m_infos.push_back(msg);
}

std::vector<std::string> sobjLogger::getErrors()
{
    return m_errors;
}

std::vector<std::string> sobjLogger::getWarnings()
{
    return m_warnings;
}

std::vector<std::string> sobjLogger::getInfos()
{
    return m_infos;
}

void sobjLogger::clear()
{
    m_errors.clear();
    m_warnings.clear();
    m_infos.clear();
}

#endif

} // namespace sobj
