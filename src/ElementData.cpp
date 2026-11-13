#include "ElementData.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace {

// ---------- 极简 CSV 解析（支持引号字段、BOM、CRLF） ----------
std::vector<std::vector<std::string>> parseCsv(const std::string& path, std::string& err) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        err = "无法打开文件: " + path;
        return {};
    }
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    // 去掉 UTF-8 BOM
    if (content.size() >= 3 && (unsigned char)content[0] == 0xEF &&
        (unsigned char)content[1] == 0xBB && (unsigned char)content[2] == 0xBF) {
        content.erase(0, 3);
    }

    std::vector<std::vector<std::string>> rows;
    std::vector<std::string> row;
    std::string field;
    bool inQuotes = false;
    size_t i = 0, n = content.size();
    while (i < n) {
        char c = content[i];
        if (inQuotes) {
            if (c == '"') {
                if (i + 1 < n && content[i + 1] == '"') { field += '"'; i += 2; continue; }
                inQuotes = false; ++i; continue;
            }
            field += c; ++i; continue;
        }
        if (c == '"') { inQuotes = true; ++i; continue; }
        if (c == ',') { row.push_back(field); field.clear(); ++i; continue; }
        if (c == '\r') { ++i; continue; }
        if (c == '\n') {
            row.push_back(field); field.clear();
            rows.push_back(row); row.clear();
            ++i; continue;
        }
        field += c; ++i;
    }
    if (!field.empty() || !row.empty()) {
        row.push_back(field);
        rows.push_back(row);
    }
    if (rows.empty()) {
        err = "CSV 为空: " + path;
        return {};
    }
    return rows;
}

float toFloat(const std::string& s) {
    if (s.empty()) return 0.0f;
    return (float)atof(s.c_str());
}

int toInt(const std::string& s) {
    if (s.empty()) return 0;
    return atoi(s.c_str());
}

std::string trimBrackets(const std::string& s) {
    std::string t = s;
    if (!t.empty() && t.front() == '[') t.erase(0, 1);
    if (!t.empty() && t.back() == ']') t.pop_back();
    return t;
}

} // namespace

ElemCategory categoryFromString(const std::string& s) {
    // 新 CSV 用英文值
    if (s == "alkali_metal")           return ElemCategory::AlkaliMetal;
    if (s == "alkaline_earth_metal")   return ElemCategory::AlkalineEarth;
    if (s == "transition_metal")       return ElemCategory::TransitionMetal;
    if (s == "post_transition_metal")  return ElemCategory::PostTransition;
    if (s == "metalloid")              return ElemCategory::Metalloid;
    if (s == "nonmetal")               return ElemCategory::Nonmetal;
    if (s == "halogen")                return ElemCategory::Halogen;
    if (s == "noble_gas")              return ElemCategory::NobleGas;
    if (s == "lanthanide")             return ElemCategory::Lanthanide;
    if (s == "actinide")               return ElemCategory::Actinide;
    // 兼容旧中文值
    if (s == "碱金属")     return ElemCategory::AlkaliMetal;
    if (s == "碱土金属")   return ElemCategory::AlkalineEarth;
    if (s == "过渡金属")   return ElemCategory::TransitionMetal;
    if (s == "后过渡金属") return ElemCategory::PostTransition;
    if (s == "准金属" || s == "类金属") return ElemCategory::Metalloid;
    if (s == "非金属")     return ElemCategory::Nonmetal;
    if (s == "卤素")       return ElemCategory::Halogen;
    if (s == "稀有气体")   return ElemCategory::NobleGas;
    if (s == "镧系元素" || s == "镧系") return ElemCategory::Lanthanide;
    if (s == "锕系元素" || s == "锕系") return ElemCategory::Actinide;
    return ElemCategory::Unknown;
}

const char* categoryChinese(ElemCategory c) {
    switch (c) {
        case ElemCategory::AlkaliMetal:     return "碱金属";
        case ElemCategory::AlkalineEarth:   return "碱土金属";
        case ElemCategory::TransitionMetal: return "过渡金属";
        case ElemCategory::PostTransition:  return "后过渡金属";
        case ElemCategory::Metalloid:       return "准金属";
        case ElemCategory::Nonmetal:        return "非金属";
        case ElemCategory::Halogen:         return "卤素";
        case ElemCategory::NobleGas:        return "稀有气体";
        case ElemCategory::Lanthanide:      return "镧系元素";
        case ElemCategory::Actinide:        return "锕系元素";
        default:                            return "未知";
    }
}

void categoryColor(ElemCategory c, float rgb[3]) {
    switch (c) {
        case ElemCategory::AlkaliMetal:     rgb[0]=0.90f; rgb[1]=0.36f; rgb[2]=0.30f; break;
        case ElemCategory::AlkalineEarth:   rgb[0]=0.95f; rgb[1]=0.64f; rgb[2]=0.31f; break;
        case ElemCategory::TransitionMetal: rgb[0]=0.94f; rgb[1]=0.78f; rgb[2]=0.29f; break;
        case ElemCategory::PostTransition:  rgb[0]=0.39f; rgb[1]=0.68f; rgb[2]=0.65f; break;
        case ElemCategory::Metalloid:       rgb[0]=0.41f; rgb[1]=0.71f; rgb[2]=0.42f; break;
        case ElemCategory::Nonmetal:        rgb[0]=0.33f; rgb[1]=0.64f; rgb[2]=0.80f; break;
        case ElemCategory::Halogen:         rgb[0]=0.26f; rgb[1]=0.72f; rgb[2]=0.78f; break;
        case ElemCategory::NobleGas:        rgb[0]=0.40f; rgb[1]=0.53f; rgb[2]=0.85f; break;
        case ElemCategory::Lanthanide:      rgb[0]=0.64f; rgb[1]=0.45f; rgb[2]=0.77f; break;
        case ElemCategory::Actinide:        rgb[0]=0.80f; rgb[1]=0.46f; rgb[2]=0.69f; break;
        default:                            rgb[0]=0.55f; rgb[1]=0.56f; rgb[2]=0.58f; break;
    }
}

const char* phaseChinese(Phase p) {
    switch (p) {
        case Phase::Solid:  return "固态";
        case Phase::Liquid: return "液态";
        case Phase::Gas:    return "气态";
        default:            return "未知";
    }
}

bool ElementDB::load(const std::string& cnCsvPath, const std::string& enCsvPath, std::string& err) {
    auto cn = parseCsv(cnCsvPath, err);
    if (cn.empty()) return false;

    std::unordered_map<int, std::string> enNames;
    if (!enCsvPath.empty()) {
        std::string e2;
        auto en = parseCsv(enCsvPath, e2);
        if (!en.empty() && en[0].size() > 2) {
            size_t numCol = (size_t)-1, nameCol = (size_t)-1;
            for (size_t i = 0; i < en[0].size(); ++i) {
                if (en[0][i] == "atomic_number") numCol = i;
                if (en[0][i] == "name")          nameCol = i;
            }
            if (numCol != (size_t)-1 && nameCol != (size_t)-1) {
                for (size_t r = 1; r < en.size(); ++r) {
                    if (en[r].size() > std::max(numCol, nameCol))
                        enNames[toInt(en[r][numCol])] = en[r][nameCol];
                }
            }
        }
    }

    // 表头 -> 列号
    std::unordered_map<std::string, size_t> col;
    for (size_t i = 0; i < cn[0].size(); ++i) col[cn[0][i]] = i;
    for (const char* req : {"原子序数", "符号", "中文元素名", "原子量", "分类", "周期"}) {
        if (!col.count(req)) {
            err = std::string("中文表缺少列: ") + req;
            return false;
        }
    }

    auto get = [&](const std::vector<std::string>& r, const char* name) -> std::string {
        auto it = col.find(name);
        if (it == col.end() || it->second >= r.size()) return {};
        return r[it->second];
    };

    elements.clear();
    elements.reserve(118);

    for (size_t r = 1; r < cn.size(); ++r) {
        const auto& row = cn[r];
        if (row.size() < 3) continue;
        Element e;
        e.number = toInt(get(row, "原子序数"));
        if (e.number <= 0) continue;

        e.symbol = get(row, "符号");
        e.nameCn = get(row, "中文元素名");
        e.mass   = get(row, "原子量");
        e.category = categoryFromString(get(row, "分类"));
        e.period = toInt(get(row, "周期"));
        e.group  = toInt(get(row, "族号"));
        e.block  = get(row, "区块");
        e.electronConfig          = get(row, "电子排布");
        e.electronConfigSemantic  = get(row, "电子排布简式");
        e.shells                  = trimBrackets(get(row, "每层电子数"));
        e.oxidationStates         = get(row, "氧化态");
        e.electronegativity = toFloat(get(row, "电负性"));
        e.ionizationEnergy  = toFloat(get(row, "电离能"));
        e.electronAffinity  = toFloat(get(row, "电子亲和能"));
        e.atomicRadius      = toFloat(get(row, "原子半径"));
        e.density           = toFloat(get(row, "密度_g_cm3"));
        e.meltingK          = toFloat(get(row, "熔点_K"));
        e.boilingK          = toFloat(get(row, "沸点_K"));

        // 拼音（新版列名：拼音(带声调)，兼容拼音）
        e.pinyin = get(row, "拼音(带声调)");
        if (e.pinyin.empty()) e.pinyin = get(row, "拼音");

        // 英文名：优先中文表的 英文名 列，回退英文表
        e.nameEn = get(row, "英文名");
        if (e.nameEn.empty()) {
            auto it = enNames.find(e.number);
            if (it != enNames.end()) e.nameEn = it->second;
        }

        // 中子数 ≈ round(原子量) - 原子序数
        float massF = toFloat(e.mass);
        e.neutrons = (int)(massF + 0.5f) - e.number;
        if (e.neutrons < 0) e.neutrons = 0;

        // 常温状态：兼容中英文
        std::string st = get(row, "常温状态");
        if (st == "固态" || st == "solid") e.phase = Phase::Solid;
        else if (st == "液态" || st == "liquid") e.phase = Phase::Liquid;
        else if (st == "气态" || st == "gas") e.phase = Phase::Gas;
        else e.phase = Phase::Unknown;

        // 布尔值：兼容 是/否 和 True/False
        std::string rd = get(row, "是否放射性");
        e.radioactive = (rd == "是" || rd == "True" || rd == "true");
        std::string sy = get(row, "是否人造");
        e.synthetic   = (sy == "是" || sy == "True" || sy == "true");

        e.discoveryYear = toInt(get(row, "发现年份"));
        e.discoveredBy  = get(row, "发现人");

        elements.push_back(std::move(e));
    }

    if (elements.empty()) {
        err = "未能从 CSV 解析出任何元素";
        return false;
    }
    // 按原子序数排序，保证 get(n) 可按下标访问
    std::sort(elements.begin(), elements.end(),
              [](const Element& a, const Element& b) { return a.number < b.number; });
    return true;
}
