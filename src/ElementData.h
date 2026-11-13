#pragma once
#include <string>
#include <vector>

enum class ElemCategory : uint8_t {
    AlkaliMetal, AlkalineEarth, TransitionMetal, PostTransition,
    Metalloid, Nonmetal, Halogen, NobleGas, Lanthanide, Actinide, Unknown
};

enum class Phase : uint8_t { Solid, Liquid, Gas, Unknown };

struct Element {
    int         number = 0;
    std::string symbol;
    std::string nameEn;
    std::string nameCn;
    std::string pinyin;         // 拼音（带声调）
    std::string mass;
    ElemCategory category = ElemCategory::Unknown;
    int         period = 0;
    int         group = 0;          // 0 = f 区内层（无族号）
    std::string block;              // s区 / p区 / d区 / f区
    std::string electronConfig;
    std::string electronConfigSemantic;
    std::string shells;
    std::string oxidationStates;
    float electronegativity = 0.0f;
    float ionizationEnergy = 0.0f;  // kJ/mol
    float electronAffinity = 0.0f;  // kJ/mol
    float atomicRadius = 0.0f;      // pm
    float density = 0.0f;           // g/cm3
    float meltingK = 0.0f;
    float boilingK = 0.0f;
    Phase phase = Phase::Unknown;
    bool  radioactive = false;
    bool  synthetic = false;
    int   neutrons = 0;             // 中子数 ≈ round(原子量) - 原子序数
    int   discoveryYear = 0;        // 0 = 古代已知
    std::string discoveredBy;
};

// 从 CSV 加载元素数据（中文表为主表，英文表补充英文名）
struct ElementDB {
    std::vector<Element> elements;  // elements[i].number == i+1

    const Element* get(int atomicNumber) const {
        if (atomicNumber < 1 || atomicNumber > (int)elements.size()) return nullptr;
        return &elements[atomicNumber - 1];
    }

    // cnCsvPath: 中文数据表; enCsvPath: 英文表（可为空，仅补英文名）
    bool load(const std::string& cnCsvPath, const std::string& enCsvPath, std::string& err);
};

const char* categoryChinese(ElemCategory c);
void        categoryColor(ElemCategory c, float rgb[3]);
const char* phaseChinese(Phase p);
ElemCategory categoryFromString(const std::string& s);
