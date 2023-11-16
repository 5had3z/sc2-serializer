// From a sequence of actions over a replay, generate an array whici is
// an encoding of the researched tech over the game based on when the
// research action is requested and when it is expected to complete
// based on the game data
//  | -------R1Action----R1Fin ---R2Action------------R2Fin--------------
//  | -----------|---------|---------|------------------|----------------
//  R1 | 0000000000000000001111111111111111111111111111111111111111111111
//  R2 | 0000000000000000000000000000000000000000000000011111111111111111
//  ..
//  RN | 0000000000000000000000000000000000000000000000000000000000000000

#include "data.hpp"

#include <spdlog/spdlog.h>
#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace cvt {

// Encapsulate into an API
class ResearchParser
{
  public:
    bool loadDelayMap(const std::string &dataFile, const std::string &version)
    {
        if (!std::filesystem::exists(dataFile)) {
            SPDLOG_ERROR("Data file does not exist: {}", dataFile);
            return false;
        }
        YAML::Node root = YAML::LoadFile(dataFile);
    }

    template<typename T>
        requires std::is_integral_v<T> || std::is_floating_point_v<T>
    auto parse(const std::vector<Action> &actions) -> std::vector<T>
    {
        if (id2delay_.empty()) { throw std::logic_error{ "Research info to delay not loaded" }; }
        return {};
    }

  private:
    std::unordered_map<int, int> id2delay_{};
};

}// namespace cvt
