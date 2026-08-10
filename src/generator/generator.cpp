/**
 * @file generator.hpp
 * @author Jaroslav Hucel (xhucel00@vutbr.cz)
 * @brief
 * @date Created: 12. 11. 2025
 * @date Modified: 10. 08. 2026
 *
 * @copyright Copyright (c) 2025 -> Public Domain, for more information see LICENSE
 */


#include "generator.hpp"

#include "../arena.hpp"
#include "../xml/xml.hpp"
#include "boilerplate.hpp"

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstring>
#include <format>
#include <fstream>
#include <iostream>
#include <ranges>
#include <sstream>
#include <string_view>


using vkgen::xml::Attribute;
using vkgen::xml::Element;
using vkgen::xml::Node;

using namespace vkgen::Generator;

using sv = std::string_view;

// Enum items that must be excluded from generated enum bodies and from to_cstr
// switches. They either shadow a corrected value (duplicate case labels) or are
// aliases that map to the same translated name as another item.
static const std::unordered_set<sv> skip_enum_items = {
    // Shadow correct values (Normal enums)
    "VK_COLORSPACE_SRGB_NONLINEAR_KHR",            // should be COLOR_SPACE
    "VK_STENCIL_FRONT_AND_BACK",                   // missing _FACE_
    "VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES2_EXT", // alias collides with _CAPABILITIES_2_EXT

    // Missing "_BIT", aliased to corrected bitmask values — would duplicate
    "VK_PIPELINE_CREATE_DISPATCH_BASE",
    "VK_HOST_IMAGE_COPY_MEMCPY",
    "VK_SURFACE_COUNTER_VBLANK",
    "VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_DATA_ACCESS",
    "VK_PERFORMANCE_COUNTER_DESCRIPTION_PERFORMANCE_IMPACTING",
    "VK_PERFORMANCE_COUNTER_DESCRIPTION_CONCURRENTLY_IMPACTED",
    "VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_OPACITY_MICROMAP_UPDATE",
    "VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_DISABLE_OPACITY_MICROMAPS",
    "VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_OPACITY_MICROMAP_DATA_UPDATE",
    "VK_GEOMETRY_INSTANCE_FORCE_OPACITY_MICROMAP_2_STATE",
    "VK_GEOMETRY_INSTANCE_DISABLE_OPACITY_MICROMAPS",
    "VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_DISPLACEMENT_MICROMAP_UPDATE",
    "VK_RESOLVE_MODE_EXTERNAL_FORMAT_DOWNSAMPLE",
};

// TASK: 090126_01
#ifdef CONCEPT_FILTERING
template <typename Func>
concept NodePredicate = requires(const Func & f, const Node * n) {
    { f(n) } -> std::convertible_to<bool>;
};

template <NodePredicate L, NodePredicate R>
struct And {
    L lhs;
    R rhs;
    inline constexpr bool operator()(const Node* n) const noexcept {
        return lhs(n) && rhs(n);
    }
};

template <NodePredicate L, NodePredicate R>
struct Or {
    L lhs;
    R rhs;
    inline constexpr bool operator()(const Node* n) const noexcept {
        return lhs(n) || rhs(n);
    }
};

template <NodePredicate P>
struct Not {
    P pred;
    inline constexpr bool operator()(const Node* n) const noexcept {
        return !pred(n);
    }
};

template <NodePredicate L, NodePredicate R>
inline constexpr auto operator&&(L lhs, R rhs) noexcept {
    return And<L, R>{lhs, rhs};
}

template <NodePredicate L, NodePredicate R>
inline constexpr auto operator||(L lhs, R rhs) noexcept {
    return Or<L, R>{lhs, rhs};
}

template <NodePredicate P>
inline constexpr auto operator!(P pred) noexcept {
    return Not<P>{pred};
}

struct HasTag {
    std::string_view tag;
    inline constexpr bool operator()(const Element& e) const noexcept {
        return e.tag == tag;
    }
    inline bool operator()(const Node* n) const noexcept {
        return n->isElement() && n->asElement().tag == tag;
    }
};

struct HasAttr {
    std::string_view name;
    std::string_view value;

    inline constexpr bool operator()(const Element& e) const noexcept {
        return std::ranges::any_of(e.attrs, [&](const auto& a) {
            return a.name == name && a.value == value;
            });
    }

    inline bool operator()(const Node* n) const noexcept {
        if (!n->isElement()) return false;
        return (*this)(n->asElement());
    }

};

struct HasAttrName {
    std::string_view name;

    constexpr inline bool operator()(const Element& e) const noexcept {
        return std::ranges::contains(e.attrs, name, &Attribute::name);
    }
    inline bool operator()(const Node* n) const noexcept {
        if (!n->isElement()) return false;
        return (*this)(n->asElement());
    }
};

struct HasAttrValue {
    std::string_view value;
    constexpr inline bool operator()(const Element& e) const noexcept {
        return std::ranges::contains(e.attrs, value, &Attribute::value);
    }
    inline bool operator()(const Node* n) const noexcept {
        if (!n->isElement()) return false;
        return (*this)(n->asElement());
    }
};


inline constexpr auto has_tag(std::string_view t) noexcept { return HasTag{ t }; }
inline constexpr auto has_attr(std::string_view n, std::string_view v) noexcept { return HasAttr{ n, v }; }
inline constexpr auto has_attr_name(std::string_view n) noexcept { return HasAttrName{ n }; }
inline constexpr auto has_attr_value(std::string_view v) noexcept { return HasAttrValue{ v }; }

#endif // CONCEPT_FILTERING

namespace vkgen::xml {

    // TASK: 090126_03
    std::ostream& operator<<(std::ostream& os, const Element& elem) {
        os << "<" << elem.tag;
        for (auto& attr : elem.attrs) {
            os << " " << attr.name << "=\"" << attr.value << '"';
        }
        return os << ">";
    }
}

// TASK: 090126_03
std::ostream& helper_test(std::ostream& os, const vkgen::xml::Node* node) {
    if (!node) return os << "(NULL)";
    if (!node->isElement())
        return os << "(TEXT: " << node->asText() << ")";

    return os << node->asElement();
}

// Helper for emitting grouped #if defined() / #endif guards during output TODO: concider moving the declaration into header file
struct ProtectGuard {
    static inline bool FilterBetaExtensions = true;
    sv current = {};
    std::ofstream& file;

    void transition(sv protect) {
        if (FilterBetaExtensions && protect == "VK_ENABLE_BETA_EXTENSIONS") protect = {};
        if (protect == current) return;
        if (!current.empty()) file << "#endif // " << current << "\n";
        if (!protect.empty()) file << "#if defined(" << protect << ")\n";
        current = protect;
    }

    void close() {
        if (!current.empty()) file << "#endif // " << current << "\n";
        current = {};
    }

    ~ProtectGuard() { close(); }
};

std::string_view to_string(Extension ext) {
    switch (ext) {
    case Extension::Img: return "IMG";
    case Extension::Amd: return "AMD";
    case Extension::Amdx: return "AMDX";
    case Extension::Arm: return "ARM";
    case Extension::Fsl: return "FSL";
    case Extension::Brcm: return "BRCM";
    case Extension::Nxp: return "NXP";
    case Extension::Nv: return "NV";
    case Extension::Nvx: return "NVX";
    case Extension::Viv: return "VIV";
    case Extension::Vsi: return "VSI";
    case Extension::Kdab: return "KDAB";
    case Extension::Android: return "ANDROID";
    case Extension::Chromium: return "CHROMIUM";
    case Extension::Fuchsia: return "FUCHSIA";
    case Extension::Ggp: return "GGP";
    case Extension::Google: return "GOOGLE";
    case Extension::Qcom: return "QCOM";
    case Extension::Lunarg: return "LUNARG";
    case Extension::Nzxt: return "NZXT";
    case Extension::Samsung: return "SAMSUNG";
    case Extension::Sec: return "SEC";
    case Extension::Tizen: return "TIZEN";
    case Extension::Renderdoc: return "RENDERDOC";
    case Extension::Nn: return "NN";
    case Extension::Mvk: return "MVK";
    case Extension::Khr: return "KHR";
    case Extension::Khx: return "KHX";
    case Extension::Ext: return "EXT";
    case Extension::Mesa: return "MESA";
    case Extension::Intel: return "INTEL";
    case Extension::Huawei: return "HUAWEI";
    case Extension::Ohos: return "OHOS";
    case Extension::Valve: return "VALVE";
    case Extension::Qnx: return "QNX";
    case Extension::Juice: return "JUICE";
    case Extension::Fb: return "FB";
    case Extension::Rastergrid: return "RASTERGRID";
    case Extension::Msft: return "MSFT";
    case Extension::Shady: return "SHADY";
    case Extension::Fredemmott: return "FREDEMMOTT";
    case Extension::Mtk: return "MTK";
    case Extension::None: break;
    }
    UNREACHABLE_QUIET();
    return "Unknown";
}

bool vkgen::Generator::bool_from_string(std::string_view s) {
    if (s == "true")
        return true;
    if (s == "false")
        return false;

    if (s.find(",") != std::string_view::npos) {
        std::cout << "FIXME: boolean value '" << s << "' not supported!" << std::endl;
        return false;
    }

    throw my_error("Invalid boolean value: '" + std::string(s) + "'");
}

Command::Scope vkgen::Generator::scope_from_string(std::string_view s) {
    if (s == "inside") return Command::Scope::Inside;
    if (s == "outside") return Command::Scope::Outside;
    if (s == "both") return Command::Scope::Both;
    throw my_error("Invalid scope: '" + std::string(s) + "'");
}

Member::ExternSync externsync_from_string(std::string_view s) {
    if (s == "true")
        return Member::ExternSync::True;
    if (s == "false")
        return Member::ExternSync::False;
    if (s == "maybe")
        return Member::ExternSync::Maybe;

    throw my_error("Invalid externsync value: '" + std::string(s) + "'");
}


Type::Category Type::category_from_string(std::string_view s) const {
    if (s == "bitmask") return Category::Bitmask;
    if (s == "basetype") return Category::Basetype;
    if (s == "define") return Category::Define;
    if (s == "enum") return Category::Enum;
    if (s == "handle") return Category::Handle;
    if (s == "funcpointer") return Category::Funcpointer;
    if (s == "struct") return Category::Struct;
    if (s == "include") return Category::Include;
    if (s == "union") return Category::Union;

    std::stringstream err;
    err << "Invalid category: '" << s << "' {";
    for (auto c : s)
        err << '\'' << c << "\', ";
    err << "}";

    throw my_error(err);
}

void Type::parse_struct(const vkgen::xml::Element& elem, vkgen::Arena& arena) {
    for (Node* child : elem.children) {
        if (child->isText()) {
            std::cout << "- TASK: 090126_04 - Skipping TEXT: " << child->asText() << std::endl;
            continue;
        }

        auto& ch = child->asElement();

        if (ch.tag == "comment")
            struct_->members.emplace_back(Member(ch, arena, Member::ParentType::Struct, true));
        else if (ch.tag == "member")
            struct_->members.emplace_back(Member(ch, arena, Member::ParentType::Struct));
        else {
            throw my_error{ "Unknown child element: " + std::string(ch.tag) };
        }
    }

};


void Type::parse_union(const vkgen::xml::Element& elem, vkgen::Arena& arena) {
    for (Node* child : elem.children) {
        if (child->isText()) {
            std::cout << "- TASK: 090126_04 - Skipping TEXT: " << child->asText() << std::endl;
            continue;
        }

        auto& ch = child->asElement();

        if (ch.tag == "comment")
            union_->members.emplace_back(Member(ch, arena, Member::ParentType::Union, true));
        else if (ch.tag == "member")
            union_->members.emplace_back(Member(ch, arena, Member::ParentType::Union));
        else {
            throw my_error{ "Unknown child element: " + std::string(ch.tag) };
        }
    }
};

Platform Platform::from_xml(const xml::Element& elem) {
    Platform p;
    for (auto& attr : elem.attrs) {
        if (attr.name == "name")
            p.name = attr.value;
        else if (attr.name == "protect")
            p.protect = attr.value;
        else if (attr.name == "comment")
            p.comment = attr.value;
        else
            throw my_error{ "Unknown attribute on <platform>: " + std::string(attr.name) };
    }
    if (p.name.empty())
        throw my_error{ "Platform must have a name" };
    if (p.protect.empty())
        throw my_error{ "Platform must have a protect" };
    return p;
}

Platforms Platforms::from_xml(const xml::Element& elem) {
    Platforms p;
    p.comment = elem.get_attr_value("comment");
    for (Node* child : elem.children) {
        if (child->isText()) continue;
        auto& ch = child->asElement();
        if (ch.tag == "platform")
            p.platforms.emplace_back(Platform::from_xml(ch));
        else
            throw my_error{ "Unknown child of <platforms>: " + std::string(ch.tag) };
    }
    return p;
}

Type::Type(const vkgen::xml::Element& elem, vkgen::Arena& arena) : elem(elem) {
    Category specific_attr = Category::None;

    for (const Attribute& attr : elem.attrs) {
        if (attr.name == "category") {
            category = category_from_string(attr.value);

            if (specific_attr == Category::None) {
                specific_attr = category;
                switch (category) {
                case Category::Handle:
                    handle = arena.make<TypeHandle>();
                    break;
                case Category::Struct:
                    struct_ = arena.make<TypeStruct>();
                    break;
                case Category::Union:
                    union_ = arena.make<TypeUnion>();
                    break;
                case Category::Funcpointer:
                    funcptr = arena.make<TypeFuncpointer>();
                    // Safe here: funcpointer has no attributes parsed later that affect parsing,
                    // and it sets Type::name from <name> child (must run before the name fallback below).
                    parse_funcpointer(elem, arena);
                    break;
                default:
                    break;
                }
            } else if (specific_attr != category)
                throw my_error("Invalid combination of attributes and category on <type>");

        } else if (attr.name == "name")
            name = attr.value;

        else if (attr.name == "api")
            api = ApiType::from_string(attr.value);

        else if (attr.name == "alias")
            alias = attr.value;

        else if (attr.name == "requires") {
            if (!requires_.empty()) {      // Set by bitvalues attribute
                assert(specific_attr == Category::Bitmask);
                throw my_error("Cannot have both requires and bitvalues attributes on <type>");
            }

            requires_ = attr.value;

        } else if (attr.name == "comment")
            comment = attr.value;

        else if (attr.name == "deprecated")
            deprecated = attr.value;

        else if (attr.name == "parent" || attr.name == "objtypeenum") {
            if (specific_attr == Category::None) {
                handle = arena.make<TypeHandle>();
            } else if (specific_attr != Category::Handle) {
                throw my_error("unexpected attribute on <type>");
            }

            if (attr.name == "parent")
                handle->parent = attr.value;
            else
                handle->objtypeenum = attr.value;
        } else if (attr.name == "returnedonly") {
            if (specific_attr == Category::None) {
                // TODO: this is a hack, and duplicates code
                auto it = std::ranges::find(elem.attrs, "category", &Attribute::name);
                if (it == elem.attrs.end())
                    throw my_error("unexpected attribute on <type>");
                specific_attr = category_from_string(it->value);

                if (specific_attr == Category::Struct) {
                    struct_ = arena.make<TypeStruct>();
                    struct_->returned_only = bool_from_string(attr.value);

                } else if (specific_attr == Category::Union) {
                    union_ = arena.make<TypeUnion>();
                    union_->returned_only = bool_from_string(attr.value);

                } else {
                    throw my_error("unexpected attribute on <type>");
                }


            } else if (specific_attr == Category::Struct) {
                struct_->returned_only = bool_from_string(attr.value);
            } else if (specific_attr == Category::Union) {
                union_->returned_only = bool_from_string(attr.value);
            } else {
                throw my_error("unexpected attribute on <type>");
            }



        } else if (attr.name == "structextends" || attr.name == "allowduplicate") {
            if (specific_attr == Category::None) {
                struct_ = arena.make<TypeStruct>();
            } else if (specific_attr != Category::Struct && specific_attr != Category::Union) {
                throw my_error("unexpected attribute on <type>");
            }

            if (attr.name == "structextends")
                struct_->struct_extends = attr.value;
            else if (attr.name == "allowduplicate")
                struct_->allow_duplicate = bool_from_string(attr.value);
            else
                struct_->returned_only = bool_from_string(attr.value);
        } else if (attr.name == "bitvalues") {
            if (specific_attr != Category::None && specific_attr != Category::Bitmask)
                throw my_error("unexpected attribute 'bitvalues' on non-bitmask <type>");
            if (!requires_.empty())
                throw my_error("bitvalues and requires cannot both be present on bitmask <type>");

            specific_attr = Category::Bitmask;
            bitvalues = attr.value;
        } else {
            throw my_error("unknown attribute on <type>: " + std::string(attr.name));
        }

    }


    if (specific_attr == Category::Handle && handle->objtypeenum.empty() && alias.empty())
        throw my_error("objtypeenum attribute is required on <type category=\"handle\"> if alias attribute is not present");

    // Reserved bitmask types have no FlagBits enum — initialize to avoid uninitialized union read.
    if (specific_attr == Category::Bitmask && bitvalues.empty() && alias.empty())
        bitvalues = {};

    // Struct/union parsing runs after the attribute loop because attributes like
    // returnedonly, structextends, allowduplicate may appear before category in the XML.
    if (specific_attr == Category::Struct)
        parse_struct(elem, arena);

    else if (specific_attr == Category::Union)
        parse_union(elem, arena);

    // TODO: check which types can have arbitrary C code
    if (name.empty()) {
        auto it = std::ranges::find_if(elem.children, has_tag("name"));
        if (it != elem.children.end()) {
            name = (*it)->asElement().children[0]->asText();
        } else {
            throw my_error("name attribute or <name> tag is required in <type>");
        }
    };
}

Type::Type(Type&& other) noexcept
    : name(other.name), api(other.api), alias(other.alias),
    comment(other.comment), deprecated(other.deprecated), protect(other.protect),
    category(other.category), state(other.state), elem(other.elem) {
    requires_ = other.requires_;
    handle = other.handle;
    other.category = Category::None; // prevent double destruction
}

// Manually destroy arena-allocated sub-objects that contain std::vectors
Type::~Type() {
    switch (category) {
    case Category::Struct:  struct_->~TypeStruct(); break;
    case Category::Union:   union_->~TypeUnion(); break;
    case Category::Funcpointer: funcptr->~TypeFuncpointer(); break;
    default: break;
    }
}

// TASK: 090126_03, prototype helper function, when othe code fails
void _gen_arbitrary_C_code_in_type(const vkgen::xml::Element& elem, std::ofstream& file) {
    bool last_was_element = false;
    for (Node* child : elem.children) {
        if (child->isText()) {
            // FIXME:
            if (child->asText() != ";" && last_was_element)
                file << ' ';
            file << child->asText();
            last_was_element = false;
            continue;
        }

        if (last_was_element)
            file << ' ';

        auto& ch = child->asElement();
        if (ch.tag == "comment") {
            file << "/*" << ch.children[0]->asText() << "*/"; // TODO: check_comment
        } else if (ch.tag == "name") {
            // TODO: register it
            sv name = ch.children[0]->asText();
            if (name.starts_with("PFN_vk"))
                file << name;
            else
                // TODO: find if defined or at least declared; use NameTranslator
                if (name.starts_with("Vk"))
                    file << name.substr(2);
                else
                    file << name;
        } else if (ch.tag == "type") {
            // TODO: find if defined or at least declared; use NameTranslator
            sv type_name = ch.children[0]->asText();
            if (type_name.starts_with("Vk"))
                type_name = type_name.substr(2);
            file << type_name; // TODO: check_type
        }


        last_was_element = true;
    }
    file << '\n';
}


TypeEnum::Type TypeEnum::type_from_string(std::string_view s) {
    if (s == "bitmask") return Type::Bitmask;
    if (s == "enum") return Type::Normal;
    if (s == "constants") return Type::Constants;
    throw my_error{ "Unknown type of enum: " + std::string(s) };
}

TypeEnum::Bitwidth TypeEnum::bitwidth_from_string(std::string_view s) {
    if (s == "8") return Bitwidth::_8;
    if (s == "16") return Bitwidth::_16;
    if (s == "32") return Bitwidth::_32;
    if (s == "64") return Bitwidth::_64;
    throw my_error{ "Unknown bitwidth: " + std::string(s) };
}

TypeEnum::TypeEnum(const vkgen::xml::Element& e, vkgen::Arena& arena) {
    for (auto& attr : e.attrs) {
        if (attr.name == "name") {
            name = attr.value;
        } else if (attr.name == "comment") {
            comment = attr.value;
        } else if (attr.name == "type") {
            type = type_from_string(e.get_attr_value("type"));
        } else if (attr.name == "bitwidth") {
            bitwidth = bitwidth_from_string(attr.value);
        } else {
            throw my_error{ "Unknown attribute: " + std::string(attr.name) };
        }
    };

    if (type == Type::Bitmask && bitwidth == Bitwidth::None)
        bitwidth = Bitwidth::_32;


    if (name.empty())
        throw my_error{ "Enum must have a name" };

    if (type == Type::None)
        throw my_error{ "Enum must have a type" };

    if (bitwidth != Bitwidth::None && type == Type::Constants)
        throw my_error{ "Constants cannot have a bitwidth, they specify the type instead" };


    for (Node* child : e.children) {
        if (!child->isElement()) {
            std::cout << "skipping non-element child" << std::endl;
            continue;
        }

        auto& ch = child->asElement();

        if (ch.tag == "comment") {
            items.emplace_back(EnumItem::from_xml(ch, arena, this, true));
            continue;
        }

        if (ch.tag == "unused") {
            std::cout << "skipping unused" << std::endl;
            continue;
        }

        if (ch.tag != "enum")
            throw my_error("unknown child of <enum>: " + std::string(ch.tag));

        items.emplace_back(EnumItem::from_xml(ch, arena, this));
    };
};

std::string_view to_string_type(TypeEnum::Bitwidth b) {
    switch (b) {
    case TypeEnum::Bitwidth::_8: return "uint8_t";
    case TypeEnum::Bitwidth::_16: return "uint16_t";
    case TypeEnum::Bitwidth::_32: return "uint32_t";
    case TypeEnum::Bitwidth::_64: return "uint64_t";
    case TypeEnum::Bitwidth::None: break;
    }
    UNREACHABLE_QUIET();
    return "Unknown";
}

std::string_view to_string(TypeEnum::Bitwidth b) {
    switch (b) {
    case TypeEnum::Bitwidth::_8: return "8";
    case TypeEnum::Bitwidth::_16: return "16";
    case TypeEnum::Bitwidth::_32: return "32";
    case TypeEnum::Bitwidth::_64: return "64";
    case TypeEnum::Bitwidth::None: return "None";
    }
    UNREACHABLE_QUIET();
    return "Unknown";
}
std::string_view to_string(TypeEnum::Type t) {
    switch (t) {
    case TypeEnum::Type::None: return "None";
    case TypeEnum::Type::Normal: return "enum";
    case TypeEnum::Type::Bitmask: return "bitmask";
    case TypeEnum::Type::Constants: return "constants";
    }
    UNREACHABLE_QUIET();
    return "Unknown";
}

uint8_t bitpos_from_string(std::string_view s) {
    int value = 0;
    for (char c : s) {
        if (c < '0' || c > '9')
            throw my_error{ "Invalid bitpos: " + std::string(s) };
        value = value * 10 + (c - '0');
        if (value > BITPOS_MAX)
            throw my_error{ "Invalid bitpos: " + std::string(s) };
    }
    return value;
}

Member::LimitType Member::limit_type_from_string(std::string_view s) {
    uint16_t value = 0;

#define LIMIT(x, y) (tok == x) { if (value & (uint16_t)LimitType::y) throw my_error{ "Duplicate limit of " x " in: " + std::string(s) } ; value |= (uint16_t)LimitType::y;}

    size_t start = 0;

    while (start < s.size()) {
        sv tok = s.substr(start, s.find(','));
        start += tok.size() + 1;

        if LIMIT("min", Min)
        else if LIMIT("max", Max)
        else if LIMIT("not", Not)
        else if LIMIT("pot", Pot)
        else if LIMIT("mul", Mul)
        else if LIMIT("bits", Bits)
        else if LIMIT("bitmask", Bitmask)
        else if LIMIT("range", Range)
        else if LIMIT("struct", Struct)
        else if LIMIT("exact", Exact)
        else if LIMIT("noauto", NoAuto)
        else
            throw my_error{ "Unknown limit type: " + std::string(s) };
    }
    return (Member::LimitType)value;
#undef LIMIT
}

Member::Member(const vkgen::xml::Element& e, vkgen::Arena& arena, ParentType parent_type, bool is_standalone_comment) :
    is_standalone_comment(is_standalone_comment) {
    UNUSED(parent_type);
    if (is_standalone_comment) {
        assert(e.tag == "comment");
        comment = e.children[0]->asText();
        return;
    }

    assert(e.tag == "member");

    for (auto& attr : e.attrs) {
        if (attr.name == "api") {
            api = ApiType::from_string(attr.value);
        } else if (attr.name == "comment") {
            comment = attr.value;
        } else if (attr.name == "len") {
            len = attr.value;
        } else if (attr.name == "altlen") {
            altlen = attr.value;
        } else if (attr.name == "stride") {
            stride = attr.value;
        } else if (attr.name == "deprecated") {
            deprecated = attr.value;
        } else if (attr.name == "externsync") {
            externsync = externsync_from_string(attr.value);
        } else if (attr.name == "optional") {
            optional = bool_from_string(attr.value);
        } else if (attr.name == "selector") {
            selector = attr.value;
        } else if (attr.name == "selection") {
            selection = attr.value;
        } else if (attr.name == "noautovalidity") {
            noautovalidity = bool_from_string(attr.value);
        } else if (attr.name == "values") {
            values = attr.value;
        } else if (attr.name == "limittype") {
            limittype = limit_type_from_string(attr.value);
        } else if (attr.name == "objecttype") {
            objecttype = attr.value;
        } else if (attr.name == "featurelink") {
            featurelink = attr.value;
        } else {
            throw my_error{ "Unknown attribute for member: " + std::string(attr.name) };
        }
    }

    if (e.children.empty())
        throw my_error{ "Member must have a type and name specified!" };

    type_param = TypeParam::from_xml(e, arena);

    if (type_param.name.empty())
        throw my_error{ "Member must have a name!" };

    // CHECK: in vulkan spec it's said to be optional
    if (type_param.type.empty())
        throw my_error{ "Member must have a type!" };

}

TypeEnum::EnumItem TypeEnum::EnumItem::from_xml(const vkgen::xml::Element& elem, vkgen::Arena& arena, TypeEnum* parent, bool is_standalone_comment, bool extend_parent, uint16_t block_ext_number) {
    EnumItem item{ .name = {}, .is_standalone_comment = is_standalone_comment, .ext_number = block_ext_number };

    assert(!(is_standalone_comment && extend_parent));

    if (is_standalone_comment) {
        assert(elem.tag == "comment");
        item.comment = elem.children[0]->asText();
        return item;
    }

    assert(elem.tag == "enum");

    bool value_specified = false;
    bool offset_specified = false;

    sv dir;
    sv offset;

    for (auto& attr : elem.attrs) {
        if (attr.name == "name") {
            item.name = attr.value;
        } else if (attr.name == "comment") {
            item.comment = attr.value;
        } else if (attr.name == "api") {
            item.api = ApiType::from_string(attr.value);
        } else if (attr.name == "protect") {
            item.protect = attr.value;
        } else if (attr.name == "deprecated") {
            item.deprecated = attr.value;
        } else if (attr.name == "alias") {
            if (value_specified)
                throw my_error{ "Enum item cannot have both 'value/bitpos' and 'alias'" };
            if (offset_specified)
                throw my_error{ "Enum item cannot have both 'alias' and 'extnumber/dir/offset'" };

            item.alias = attr.value;
            item.is_alias = true;
        } else if (attr.name == "value") {
            if (item.is_alias)
                throw my_error{ "Enum item cannot have both 'value' and 'alias'" };
            if (value_specified)
                throw my_error{ "Enum item cannot have both 'value' and 'bitpos'" };
            if (offset_specified)
                throw my_error{ "Enum item cannot have both 'value' and 'extnumber/dir/offset'" };

            if (parent->type == Type::Constants)
                item.constant.value = attr.value;
            else if (parent->type == Type::Normal)
                item.normal.value = attr.value;
            else if (parent->type == Type::Bitmask)
                item.bitmask.value = attr.value;
            else
                assert(false);

            value_specified = true;

        } else if (attr.name == "bitpos") {
            if (item.is_alias)
                throw my_error{ "Enum item cannot have both 'bitpos' and 'alias'" };
            if (value_specified)
                throw my_error{ "Enum item cannot have both 'bitpos' and 'value'" };
            if (offset_specified)
                throw my_error{ "Enum item cannot have both 'bitpos' and 'extnumber/dir/offset'" };

            if (parent->type != Type::Bitmask)
                throw my_error{ "Invalid 'bitpos' attribute for enum type: " + std::string(to_string(parent->type)) };
            item.bitmask.bitpos = bitpos_from_string(attr.value);
            item.bitmask.is_bitfield = true;
            // TODO: check range with bitwidth
            // TODO: remove from_chars and write own parser
            value_specified = true;

        } else if (attr.name == "type") {
            if (parent->type != Type::Constants)
                throw my_error{ "Invalid 'type' attribute for enum type: " + std::string(to_string(parent->type)) };
            assert(parent->bitwidth == Bitwidth::None);
            item.constant.type = attr.value; // TODO: parse
        } else if (extend_parent && (attr.name == "extends" || attr.name == "extnumber" || attr.name == "dir" || attr.name == "offset")) {
            if (attr.name != "extends") {
                offset_specified = true;
                if (value_specified)
                    throw my_error{ "Enum item cannot have both 'value/bitpos' and 'extnumber/dir/offset'" };
                if (item.is_alias)
                    throw my_error{ "Enum item cannot have both 'alias' and 'extnumber/dir/offset'" };
            }

            if (attr.name == "extnumber") {
                auto num = std::stoul(std::string(attr.value)); // TODO: check if the extension number is valid and should be generated
                //if (ext_number != 0 && num != ext_number) // TODO: handle this
                item.ext_number = num;
            }

            else if (attr.name == "dir")
                dir = attr.value;
            else if (attr.name == "offset")
                offset = attr.value;
        } else {
            throw my_error{ "Unknown attribute: " + std::string(attr.name) };

        }
    };

    if (item.name.empty())
        throw my_error{ "Enum item must have a name" };



    if (extend_parent) {
        if (!value_specified && !item.is_alias && !offset_specified)
            // TODO: add information on where wre we and some enum info
            throw my_error{ "Enum item must have'value'/'bitpos', 'alias' or 'offset' attribute when extending parent." };
    } else {
        if (!value_specified && !item.is_alias)
            throw my_error{ "Enum item must have 'value'/'bitpos' or 'alias' attribute" };
    }

    if (offset_specified) {
        if (parent->type != Type::Normal)
            throw my_error{ "Offset/extnumber/dir can only be used with 'normal' enum type, not bitmask or constant." };

        if (item.ext_number == 0)
            throw my_error{ "Cannot infer extnumber when extending enum with offset! (Move this enum to <extension> tag or add 'extnumber' attribute)" };

        int offset_i = std::stoi(std::string(offset));
        if (offset_i > 1000) // 3 digits
            throw my_error{ "Offset must be less than 1000" };

        // Vulkan spec:
        long normal_value = 1000'000'000 + (item.ext_number - 1) * 1000 + offset_i;
        item.normal.value = arena.storeString(std::string(dir) + std::to_string(normal_value));
    }

    return item;
}



void vkgen::Generator::Generator::parse_platforms(vkgen::xml::Dom& dom) {
    auto& registry = dom.root->asElement();
    assert(registry.tag == "registry");

    // TODO: there should be only one
    auto platforms_range = registry.children | std::views::filter(has_tag("platforms"));
    for (Node* node : platforms_range) {
        platforms = Platforms::from_xml(node->asElement());
        for (auto& p : platforms.platforms) {
            platform_to_protect[p.name] = p.protect;
        }
    }
}

void Generator::parse_types(vkgen::xml::Dom& dom) {
    auto& registry = dom.root->asElement();
    assert(registry.tag == "registry");

    // TODO: tags

    // TODO: there should be only one
    auto all_types = registry.children | std::views::filter(has_tag("types"));
    for (Node* types : all_types) {
        for (Node* child : types->asElement().children) {
            // TODO: grouping?? and comments tag
            if (child->isText()) {
                std::cout << "- Skipping TEXT: " << child->asText() << std::endl;
                continue;
            }

            auto& type = child->asElement();
            if (type.tag == "comment") {
                std::cout << "Skipping <comment> when parsing types.\n";
                continue;
            }

            if (type.tag != "type")
                throw my_error{ "Unknown tag when parsing types: " + std::string(type.tag) };

            Type t(type, dom.arena);
            //std::cout << "- " << t.name << std::endl;
            this->types.emplace(t.name, std::move(t));

        }
    }

}

void Generator::parse_enums(vkgen::xml::Dom& dom) {
    auto& registry = dom.root->asElement();
    assert(registry.tag == "registry");

    for (Node* enum_ : registry.children | std::views::filter(has_tag("enums"))) {
        TypeEnum e(enum_->asElement(), dom.arena);
        //std::cout << "- " << e.name << std::endl;
        this->enums.emplace(e.name, std::move(e));
    }
}

void vkgen::Generator::Generator::parse_commands(vkgen::xml::Dom& dom) {
    auto& registry = dom.root->asElement();
    assert(registry.tag == "registry");

    auto all_commands = registry.children | std::views::filter(has_tag("commands"));
    for (Node* cmds : all_commands) {
        for (Node* child : cmds->asElement().children) {
            if (child->isText()) {
                std::cout << " TASK: 090126_04 - Skipping TEXT: " << child->asText() << std::endl;
                continue;
            }

            auto& cmd = child->asElement();
            if (cmd.tag != "command") {
                std::cout << "- FIXME: ";
                helper_test(std::cout, child) << std::endl;
                continue;
            };
            // TASK: 180426_08 — revisit aliased-command path below.
            if (std::ranges::find(cmd.attrs, "alias", &Attribute::name) != cmd.attrs.end()) {
                CommandAlias ca = CommandAlias::from_xml(cmd, dom.arena);
                //std::cout << "- " << ca.name << std::endl;
                this->command_aliases.emplace(ca.name, std::move(ca));
                continue;
            }

            Command c = Command::from_xml(cmd, dom.arena);
            //std::cout << "- " << c.name << std::endl;
            this->commands.emplace(c.name, std::move(c));
        }
    }
}

bool vkgen::Generator::Generator::is_handle(sv type) {
    auto it = types.find(type);
    if (it == types.end())
        return false;
    return it->second.category == Type::Category::Handle;
}

void vkgen::Generator::Generator::cache_handles(std::vector<HandleInfo>& handles) {
    const auto get_cmd = [&](std::string name) -> const Command* {
        auto it = commands.find(name);
        if (it == commands.end())
            return nullptr;
        return &it->second;
        };

    // Reserve to prevent reallocation — pointers into handles are stored in Type::handle->info
    size_t handle_count = 0;
    for (Type* p : required_types.get())
        if (p && p->category == Type::Category::Handle && p->alias.empty()) ++handle_count;
    handles.reserve(handle_count);

    for (Type* p : required_types.get()) {
        if (p == nullptr || p->category != Type::Category::Handle || !p->alias.empty())
            continue;

        HandleInfo h(*p);

        // Match each handle to its destroy/free/release command by scanning command parameters.
        // Pattern: vkDestroy/Free/Release commands take (parent, handle, [allocator]) or (handle, [allocator]) for self-destroy.
        // We match on the handle appearing as param[0] (self-destroy) or param[1] (owned), non-pointer (not batch).

        assert(h.type.name.starts_with("Vk"));
        sv vk_name = h.type.name.substr(2); // skip "Vk"

        if (auto cmd = get_cmd(std::string("vkDestroy").append(vk_name))) {
            h.destroy_command = cmd;
            h.destroy_behavior = HandleInfo::DestroyBehavior::Destroy;
        } else if (auto cmd = get_cmd(std::string("vkFree").append(vk_name))) {
            h.destroy_command = cmd;
            h.destroy_behavior = HandleInfo::DestroyBehavior::Free;
        } else if (auto cmd = get_cmd(std::string("vkRelease").append(vk_name))) {
            h.destroy_command = cmd;
            h.destroy_behavior = HandleInfo::DestroyBehavior::Release;
        } else {
            // TODO: Fallback: scan commands by parameter type (handles like VkDeviceMemory → vkFreeMemory)
            for (auto& [cmd_name, cmd] : commands) {
                bool is_destroy = cmd_name.starts_with("vkDestroy");
                bool is_free = cmd_name.starts_with("vkFree");
                bool is_release = cmd_name.starts_with("vkRelease");
                if (!is_destroy && !is_free && !is_release) continue;
                if (cmd.parameters.size() < 2) continue;
                if (cmd.parameters[1].type_param.type != h.type.name) continue;
                if (!cmd.parameters[1].type_param.post_quals.empty()) continue; // TODO: skip batch ops

                h.destroy_command = &cmd;
                h.destroy_behavior = is_destroy ? HandleInfo::DestroyBehavior::Destroy
                    : is_free ? HandleInfo::DestroyBehavior::Free
                    : HandleInfo::DestroyBehavior::Release;
                break;
            }
        }

        auto cmd = h.destroy_command;
        if (vk_name == "Instance" || vk_name == "Device") {
            assert(cmd != nullptr);
            assert(cmd->parameters.size() <= 2);
            assert(cmd->parameters[0].type_param.type == std::string("Vk").append(vk_name));

            h.kind = (vk_name == "Instance") ? HandleInfo::Kind::InstanceItself : HandleInfo::Kind::DeviceItself;
        } else if (cmd) {
            assert(cmd->parameters.size() >= 2);
            assert(cmd->parameters[1].type_param.type == std::string("Vk").append(vk_name));
            assert(cmd->parameters[1].type_param.post_quals.empty());

            sv parent = cmd->parameters[0].type_param.type;
            if (parent != "VkInstance" && parent != "VkDevice") { // TODO: this is not supported by UniqueHandle class, may be needed in future
                h.destroy_behavior = HandleInfo::DestroyBehavior::None;
                h.destroy_command = nullptr;
            } else {
                h.kind = (parent == "VkInstance") ? HandleInfo::Kind::Instance : HandleInfo::Kind::Device;
            }
        }
        handles.push_back(std::move(h));
        p->handle->info = &handles.back(); // FIXME: TASK: 310326_01 Currently we create this info in the cache_handles function into array, that then is cleared, THIS IS A BIG HACK and needs refactoring
    }
}

constexpr std::string_view bitwidth_to_str_type(TypeEnum::Bitwidth w) {
    switch (w) {
    case TypeEnum::Bitwidth::None:
        return "";
    case TypeEnum::Bitwidth::_8:
        return ": uint8_t ";
    case TypeEnum::Bitwidth::_16:
        return ": uint16_t ";
    case TypeEnum::Bitwidth::_32:
        return ": uint32_t ";
    case TypeEnum::Bitwidth::_64:
        return ": uint64_t ";
    }
    UNREACHABLE();
};

void Generator::generate_enum_alias(const Type& e, std::ofstream& file) {
    if (e.alias.empty())
        return;

    file << LineComment{ e.comment };
    file << "using " << NameTranslator::from_type_name(e.name) << Deprecate{ e.deprecated } << "= "
        << NameTranslator::from_type_name(e.alias) << ";\n\n";
}

// Ensures aliases come after the items they reference.
// Sorts by extension number
// Keeps XML order otherwise, still an bit hacky function, because we dont handle enum items dependencies
void order_enum_aliases(std::vector<TypeEnum::EnumItem>& items) {
    std::ranges::stable_sort(items, [](const TypeEnum::EnumItem& a, const TypeEnum::EnumItem& b) {
        return std::tie(a.is_alias, a.ext_number) < std::tie(b.is_alias, b.ext_number);
        });
    // TODO: possible edge case: alias of alias, then if A = 1; B = A; C = B; they must be generated in that order, but this guarantees only that A will be generated first
}

void Generator::generate_enum(TypeEnum& e, std::ofstream& file) {
    file << LineComment{ e.comment };

    // TODO: if (config.verbose)
    // std::cout << "Generating enum: " << e.name << std::endl;

    if (e.type == TypeEnum::Type::Constants) {
        for (auto& item : e.items) {
            file << "constexpr " << get_translated_type_name(item.constant.type) << " "
                << NameTranslator::from_constexpr_value(item.name) << " = " << item.constant.value << ";"
                << LineComment{ item.comment, false } << '\n';
        }
        file << '\n';
        return;
    }

    file << "enum" << (config.generate_enums_classes ? " class" : "") << Deprecate{ e.deprecated } << NameTranslator::from_type_name(e.name) <<
        " " << bitwidth_to_str_type(e.bitwidth) << " {\n";

    order_enum_aliases(e.items);

    // Move protected items to end, grouped by protect value
    std::stable_partition(e.items.begin(), e.items.end(),
        [](const TypeEnum::EnumItem& item) { return item.protect.empty(); });

    auto enum_name_transformed = NameTranslator::transform_enum_name(e.name, e.type == TypeEnum::Type::Bitmask);

    ProtectGuard guard{ .file = file };
    for (const TypeEnum::EnumItem& item : e.items) {
        if (item.is_standalone_comment) {
            file << StandaloneComment{ item.comment };
            continue;
        }

        if (skip_enum_items.contains(item.name)) {
            continue;
        }

        if (!item.api.is_vulkan())
            continue;

        guard.transition(item.protect);

        bool is_bitfield_value = false;
        if (e.type == TypeEnum::Type::Bitmask) {
            auto* target = &item;
            while (target->is_alias)
                target = &*std::ranges::find(e.items, target->alias, &TypeEnum::EnumItem::name);
            is_bitfield_value = target->bitmask.is_bitfield;
        }

        file << "    " << NameTranslator::from_enum_value(item.name, enum_name_transformed, is_bitfield_value) << Deprecate{ item.deprecated } << "= ";

        if (item.is_alias) {
            file << NameTranslator::from_enum_value(item.alias, enum_name_transformed, is_bitfield_value);
        } else if (e.type == TypeEnum::Type::Normal) {
            file << item.normal.value;
        } else if (e.type == TypeEnum::Type::Bitmask) {
            if (item.bitmask.is_bitfield) {
                if (item.bitmask.bitpos == 31)
                    file << "1U";
                else if (item.bitmask.bitpos > 31)
                    file << "1ULL";
                else
                    file << "1";

                file << " << " << (int)item.bitmask.bitpos;
            } else {
                file << item.bitmask.value;
            }

        } else {
            throw my_error("TODO: internal error");
        }

        file << "," << LineComment{ item.comment, false } << '\n';
    }
    guard.close();
    file << "};\n\n";
}

void Generator::generate_to_cstr_decl(TypeEnum& e, std::ofstream& file) {
    auto type_name = NameTranslator::from_type_name(e.name);
    file << "const char* to_cstr(" << type_name.new_name << " v);\n";
}

void Generator::generate_to_cstr_def(TypeEnum& e, std::ofstream& file) {
    auto type_name = NameTranslator::from_type_name(e.name);
    auto enum_name_transformed = NameTranslator::transform_enum_name(e.name, false);

    file << "const char* to_cstr(" << type_name.new_name << " v) {\n"
        << "    switch (v) {\n";

    ProtectGuard guard{ .file = file };
    for (const TypeEnum::EnumItem& item : e.items) {
        if (item.is_standalone_comment) continue;
        if (item.is_alias) continue;
        if (skip_enum_items.contains(item.name)) continue;
        if (!item.api.is_vulkan()) continue;

        guard.transition(item.protect);

        auto value = NameTranslator::from_enum_value(item.name, enum_name_transformed, false);
        sv str_name = value.new_name;
        if (!str_name.empty() && str_name.front() == 'e')
            str_name.remove_prefix(1);

        file << "    case " << type_name.new_name << "::" << value.new_name
            << ": return \"" << str_name << "\";\n";
    }
    guard.close();
    file << "    default: return \"Unknown\";\n"
        << "    }\n}\n\n";
}

void vkgen::Generator::Generator::generate_member(Member& member, std::ofstream& file, sv struct_union, sv parent_name) {
    if (member.is_standalone_comment) {
        file << StandaloneComment{ member.comment };
        return;
    }

    if (!member.api.is_vulkan()) {
        std::cout << " TODO: Skipping member '" << member.type_param.stringify() << "' of " << struct_union << " '" << parent_name << "', because it does not contain 'vulkan' api.\n";
        return;
    }

    // TODO: this is a hack, because the flag class is cant be used with bitselect, C++ does not allow it
    if (config.generate_flags_class && member.type_param.type.starts_with("Vk") && std::ranges::contains(member.type_param.array_extensions, TypeParam::ArrayExtension::Type::BitSelect, &TypeParam::ArrayExtension::type)) {
        TypeEnum::Bitwidth width = enums.find(types.find(member.type_param.type)->second.bitvalues)->second.bitwidth;
        sv old_type = member.type_param.type;
        member.type_param.type = to_string_type(width);
        file << "    " << member.type_param.stringify() << ';' << LineComment{ member.type_param.comment, false } << "// class Flags<> can't be subscribted with ':' \n";
        member.type_param.type = old_type;
    } else {
        file << "    " << member.type_param.stringify() << ';' << LineComment{ member.type_param.comment, false } << '\n';
    }

}

void Generator::generate_struct_union(const Type& type, std::ofstream& file, sv struct_union) {
    assert(struct_union == "struct" || struct_union == "union");
    assert(type.category == (struct_union == "struct" ? Type::Category::Struct : Type::Category::Union));

    // TODO: if (config.verbose)
    // std::cout << "Generating " << struct_union << ": " << type.name << std::endl;

    file << LineComment{ type.comment, true };

    if (!type.alias.empty()) {
        file << "using " << NameTranslator::from_type_name(type.name) << Deprecate{ type.deprecated } << "= "
            << NameTranslator::from_type_name(type.alias) << ";\n";
        return;
    }

    file << struct_union << Deprecate{ type.deprecated } << NameTranslator::from_type_name(type.name) << " {\n";

    static_assert(std::is_same_v<decltype(type.struct_->members), decltype(type.union_->members)>);
    for (auto& member : type.struct_->members) {
        generate_member(member, file, struct_union, type.name);
    }
    file << "};\n\n";
}

void Generator::generate_struct(const Type& s, std::ofstream& file) {
    generate_struct_union(s, file, "struct");
}

void Generator::generate_union(const Type& s, std::ofstream& file) {
    generate_struct_union(s, file, "union");
}

void Generator::generate_bitmask(const Type& bitmask, std::ofstream& file) {
    assert(bitmask.category == Type::Category::Bitmask);

    file << LineComment{ bitmask.comment, true };

    file << "using " << NameTranslator::from_type_name(bitmask.name) << Deprecate{ bitmask.deprecated } << "= ";
    if (!bitmask.alias.empty()) {
        file << NameTranslator::from_type_name(bitmask.alias) << ";\n";

    } else if (config.generate_flags_class) {
        // TASK: 230326_02 empty bitmask
        if (!bitmask.bitvalues.empty())
            file << "Flags<" << NameTranslator::from_type_name(bitmask.bitvalues) << ">;\n";
        else {
            auto it = std::ranges::find_if(bitmask.elem.children, has_tag("type"));
            if (it == bitmask.elem.children.end())
                throw my_error{ "bitmask type not found" };
            sv type = (*it)->asElement().children[0]->asText();

            if (type != "VkFlags" && type != "VkFlags64")
                throw my_error{ "bitmask type must be VkFlags or VkFlags64" };

            file << "Flags<uint" << (type == "VkFlags" ? "32" : "64") << "_t>; //  Bitmask does have undefined bits\n";
        }

    } else {
        auto it = std::ranges::find_if(bitmask.elem.children, has_tag("type"));
        if (it == bitmask.elem.children.end())
            throw my_error{ "bitmask type not found" };

        file << NameTranslator::from_type_name((*it)->asElement().children[0]->asText()) << ";\n"; // TODO: check children
    }

    // TODO: if (config.verbose)
    // std::cout << "Generating bitmask: " << bitmask.name << std::endl;

};



void Generator::generate_handle(const Type& h, std::ofstream& file, TypeEnum& obj_enum) {
    assert(h.category == Type::Category::Handle);

    if (!h.alias.empty()) {
        file << "using " << NameTranslator::from_type_name(h.name) << Deprecate{ h.deprecated } << "= "
            << NameTranslator::from_type_name(h.alias) << ";\n";
        return;
    };

    auto it = std::ranges::find(obj_enum.items, h.handle->objtypeenum, &TypeEnum::EnumItem::name);
    if (it == obj_enum.items.end())
        throw my_error{ std::format("Handle '{}' did not found matching objtypeenum '{}' in enum '{}'", h.name, h.handle->objtypeenum, obj_enum.name) };

    if (!config.generate_handle_class)
        file << "VK_DEFINE_HANDLE(" << h.name << ")\n";
    else {
        // Use the same struct name as the non-vkg Vulkan APIs would use
        file << "using " << NameTranslator::from_type_name(h.name) << Deprecate{ h.deprecated } << "= "
            << "Handle<struct " << h.name << "_T*>" << ";" << LineComment{ h.comment, false } << '\n';
    }
}

// Generate error classes from Result enum values
void vkgen::Generator::Generator::generate_error_classes(std::ofstream& file) {
    file << boilerplate::ERROR_BASE_CLASS;
    auto VkResult = enums.at("VkResult");
    auto enum_name = NameTranslator::transform_enum_name("VkResult", false);

    for (auto& item : VkResult.items) {
        if (item.is_alias || item.is_standalone_comment) continue;

        auto translated = NameTranslator::from_enum_value(item.name, enum_name, false);
        sv class_name = translated.new_name;
        // eErrorOutOfHostMemory -> OutOfHostMemoryError, eSuccess -> SuccessResult
        if (class_name.starts_with("eError")) {
            class_name = class_name.substr(6); // skip "eError"
            file << "class " << class_name << "Error : public Error { public: using Error::Error; };\n";
        } else {
            assert(class_name.starts_with("e"));
            class_name = class_name.substr(1); // skip "e"
            file << "class " << class_name << "Result : public Error { public: using Error::Error; };\n";
        }
    }
    file << "\n";
}

void vkgen::Generator::Generator::generate_throw_result_exception(std::ofstream& source) {
    const TypeEnum& VkResult = enums.at("VkResult");
    auto enum_name = NameTranslator::transform_enum_name("VkResult", false);
    ProtectGuard guard{ .file = source };

    const auto emit_switch = [&](sv signature, sv throw_args) {
        source << signature << " {\n    switch (result) {\n";
        for (auto& item : VkResult.items) {
            if (item.is_alias || item.is_standalone_comment) continue;
            auto translated = NameTranslator::from_enum_value(item.name, enum_name, false);
            sv new_name = translated.new_name;
            guard.transition(item.protect);
            sv class_suffix = new_name.starts_with("eError") ? "Error" : "Result";
            sv class_name = new_name.substr(new_name.starts_with("eError") ? 6 : 1); // skip "eError" or "e"
            source << "    case Result::" << new_name <<
                ": throw " << class_name << class_suffix << "(" << throw_args << ");\n";
        }
        guard.close();
        source << "    default: throw VkgError(\"throwResultException\", result);\n"
            << "    }\n}\n\n";
        };

    emit_switch("void throwResultException(Result result, const char* funcName)", "funcName, result");
    emit_switch("void throwResultExceptionWithMessage(Result result, const char* message)", "message");

}


NameTranslator vkgen::Generator::Generator::get_translated_type_name(sv name) {
    Type& t = types.at(name);
    switch (t.category) {
    case Type::Category::Struct:
    case Type::Category::Union:
    case Type::Category::Enum:
        return NameTranslator::from_type_name(t.name);
    case Type::Category::Basetype:
        return NameTranslator(std::string(t.name));
    case Type::Category::Bitmask:
    case Type::Category::Handle:
    case Type::Category::Funcpointer:
    case Type::Category::Define:
    case Type::Category::Include:
        // FIXME:
        return NameTranslator(std::string(t.name));
    case Type::Category::None:
        // FIXME:
        return NameTranslator(std::string(t.name));
    }

    UNREACHABLE();
}


std::ofstream& vkgen::Generator::Generator::generate_command_params(Command& cmd, std::ofstream& file, bool is_end_of_line) {
    file << "(";
    bool first = true;
    for (auto& param : cmd.parameters) {
        if (!param.api.is_vulkan()) {
            std::cout << " TODO: Skipping parameter '" << param.type_param.stringify() << "' of command '" << cmd.name << "', because it does not contain 'vulkan' api.\n";
            continue;
        }

        if (first)
            first = false;
        else
            file << ", ";
        if (config.generate_handle_class && is_handle(param.type_param.type)) {
            std::string new_type = param.type_param.stringify();
            sv type = param.type_param.type; // TODO: name translator
            if (type.starts_with("Vk")) {
                type.remove_prefix(2);
            }
            new_type.insert(new_type.find(type, 0) + type.size(), "::HandleType");
            file << new_type;
        } else {
            file << param.type_param.stringify();
        }
    }

    file << ")";
    if (is_end_of_line)
        file << ";\n";
    return file;
}

void Generator::generate_command(Command& cmd, std::ofstream& file) {
    file << LineComment{ cmd.comment };
    // FIXME: cmd can have any arbitraty C code
    sv type = cmd.type;
    if (type.starts_with("Vk")) {
        type.remove_prefix(2);
    }
    file << "VKAPI_ATTR " << type << " VKAPI_CALL " << cmd.name;
    generate_command_params(cmd, file);

}

void vkgen::Generator::Generator::generate_command_PFN(Command& cmd, std::ofstream& file) {
    // FIXME: cmd can have any arbitraty C code
    sv type = cmd.type;
    if (type.starts_with("Vk")) {
        type.remove_prefix(2);
    }
    file << "typedef " << type << " (VKAPI_PTR* PFN_" << cmd.name << ")";
    generate_command_params(cmd, file);
}

void vkgen::Generator::Generator::generate_funcpointer(Type& type, std::ofstream& file) {
    auto& fp = *type.funcptr;
    file << "typedef " << fp.return_type.stringify();
    file << " (VKAPI_PTR* " << type.name << ")(";

    bool first = true;
    for (auto& param : fp.parameters) {
        if (first)
            first = false;
        else
            file << ", ";
        // FIXME: hotfix — stringify + string insert for handles is fragile,
        // and duplicates logic from generate_command_params. Unify later.
        if (config.generate_handle_class && is_handle(param.type)) {
            std::string new_type = param.stringify();
            sv param_type = param.type;
            if (param_type.starts_with("Vk"))
                param_type.remove_prefix(2);
            new_type.insert(new_type.find(param_type, 0) + param_type.size(), "::HandleType");
            file << new_type;
        } else {
            file << param.stringify();
        }
    }

    file << ");\n";
}

void vkgen::Generator::Generator::generate_command_wrapper(Command& cmd, std::ofstream& file) {
    sv type = cmd.type;
    if (type.starts_with("Vk")) {
        type.remove_prefix(2);
    }
    file << "inline " << type << " " << NameTranslator::from_command_name(cmd.name);
    generate_command_params(cmd, file, false);
    file << "{ return funcs." << cmd.name << "(";

    bool first = true;
    for (auto& param : cmd.parameters) {
        if (!param.api.is_vulkan()) {
            std::cout << " TODO: Skipping parameter '" << param.type_param.stringify() << "' of command '" << cmd.name << "', because it does not contain 'vulkan' api.\n";
            continue;
        }

        if (first)
            first = false;
        else
            file << ", ";
        // TODO: nameTranslator
        file << param.type_param.name;
    };
    file << "); }\n";

}

CommandClassification Generator::classify_command(const Command& cmd) {
    CommandClassification cc;

    // Non-standard return type → pass-through
    if (cmd.type != "void" && cmd.type != "VkResult") {
        cc.pattern = CommandClassification::Pattern::Other;
        return cc;
    }

    // Detect implicit first param (VkDevice or VkInstance)
    if (!cmd.parameters.empty()) {
        sv first_type = cmd.parameters[0].type_param.type;
        if (first_type == "VkDevice") {
            cc.implicit_param_idx = 0;
            cc.implicit_global = "detail::_device";
        } else if (first_type == "VkInstance") {
            cc.implicit_param_idx = 0;
            cc.implicit_global = "detail::_instance";
        }
    }

    // Detect allocator param (const VkAllocationCallbacks*)
    for (int i = 0; i < (int)cmd.parameters.size(); ++i) {
        if (cmd.parameters[i].type_param.type == "VkAllocationCallbacks"
            && cmd.parameters[i].type_param.is_const()
            && !cmd.parameters[i].type_param.post_quals.empty()
            && bool(cmd.parameters[i].type_param.post_quals[0] & TypeParam::PostQualifier::Pointer)) {
            cc.allocator_param_idx = i;
            break;
        }
    }

    // Detect enumerate pattern: a count param (uint32_t*) + array param with len referencing it
    for (int i = 0; i < (int)cmd.parameters.size(); ++i) {
        auto& p = cmd.parameters[i];
        if (p.type_param.type == "uint32_t"
            && !p.type_param.post_quals.empty()
            && bool(p.type_param.post_quals[0] & TypeParam::PostQualifier::Pointer)
            && p.optional == "false,true") {
            // Look for an array param that references this count param
            for (int j = 0; j < (int)cmd.parameters.size(); ++j) {
                if (i == j) continue;
                if (cmd.parameters[j].len == p.type_param.name) {
                    cc.count_param_idx = i;
                    cc.array_param_idx = j;
                    cc.pattern = CommandClassification::Pattern::Enumerate;
                    return cc;
                }
            }
        }
    }

    // Check last param for output detection
    if (!cmd.parameters.empty()) {
        int last_idx = (int)cmd.parameters.size() - 1;
        auto& last = cmd.parameters[last_idx];
        bool is_pointer = !last.type_param.post_quals.empty()
            && bool(last.type_param.post_quals[0] & TypeParam::PostQualifier::Pointer);
        bool is_non_const = !last.type_param.is_const();

        if (is_pointer && is_non_const && last_idx != cc.allocator_param_idx) {
            bool last_is_handle = is_handle(last.type_param.type);

            if (last_is_handle) {
                cc.output_param_idx = last_idx;
                if (cmd.type == "VkResult") {
                    cc.pattern = CommandClassification::Pattern::ResultCreate;
                    // Detect array-create: output's len references a uint32_t count param in same command
                    sv len = last.len;
                    if (!len.empty() && !len.starts_with("latexmath:") && len.find(',') == sv::npos) {
                        int cnt_idx = -1;
                        for (int i = 0; i < (int)cmd.parameters.size(); ++i) {
                            auto& p = cmd.parameters[i];
                            if (p.type_param.name == len
                                && p.type_param.type == "uint32_t"
                                && p.type_param.post_quals.empty()) {
                                cnt_idx = i;
                                break;
                            }
                        }
                        if (cnt_idx >= 0) {
                            // Find the matching input array (const T* with same len) to render as span
                            int in_arr_idx = -1;
                            for (int j = 0; j < (int)cmd.parameters.size(); ++j) {
                                if (j == last_idx) continue;
                                auto& p = cmd.parameters[j];
                                if (p.len != len) continue;
                                if (!p.type_param.is_const()) continue;
                                if (p.type_param.post_quals.empty()) continue;
                                if (!bool(p.type_param.post_quals[0] & TypeParam::PostQualifier::Pointer)) continue;
                                if (p.type_param.type == "void") continue;
                                in_arr_idx = j;
                                break;
                            }
                            if (in_arr_idx >= 0
                                && cc.input_array_count < CommandClassification::max_input_arrays) {
                                cc.input_arrays[cc.input_array_count++] = { cnt_idx, in_arr_idx };
                            }
                            cc.count_param_idx = cnt_idx;
                            cc.pattern = CommandClassification::Pattern::ResultCreateArray;
                        }
                    }
                } else {
                    assert(cmd.type == "void");
                    cc.pattern = CommandClassification::Pattern::VoidOutParam;
                }
            } else if (last.type_param.type != "void" && last.type_param.type != "uint32_t") {
                cc.output_param_idx = last_idx;
                if (cmd.type == "void") {
                    cc.pattern = CommandClassification::Pattern::VoidOutParam;
                } else {
                    assert(cmd.type == "VkResult");
                    cc.pattern = CommandClassification::Pattern::ResultOutParam;
                }
            }
        }
    }

    // No output data — set pattern if not already determined by output detection
    if (cc.output_param_idx == -1) {
        if (cmd.type == "void") {
            cc.pattern = CommandClassification::Pattern::Void;
        } else {
            assert(cmd.type == "VkResult");
            cc.pattern = CommandClassification::Pattern::ResultVoid;
        }
    }

    // Detect 1:1 input array pairs, can be combined with other patterns
    detect_input_arrays(cmd, cc);

    return cc;
}

// Detects 1:1 input array pairs: non-pointer uint32_t count + const T* with len referencing it.
// Shared counts (one count → multiple arrays) and complex len expressions are skipped.
void Generator::detect_input_arrays(const Command& cmd, CommandClassification& cc) {
    for (int j = 0; j < (int)cmd.parameters.size(); ++j) {
        auto& arr = cmd.parameters[j];
        if (arr.len.empty()) continue;
        // Skip complex len expressions (commas, latexmath)
        if (arr.len.find(',') != sv::npos) continue;
        if (arr.len.starts_with("latexmath:")) continue;
        // Must be a const pointer
        if (!arr.type_param.is_const()) continue;
        if (arr.type_param.post_quals.empty()) continue;
        if (!bool(arr.type_param.post_quals[0] & TypeParam::PostQualifier::Pointer)) continue;
        // Skip void* (e.g. vkCmdPushConstants) — span<void> is invalid
        if (arr.type_param.type == "void") continue;

        // Find the count param by name
        int count_idx = -1;
        for (int i = 0; i < (int)cmd.parameters.size(); ++i) {
            if (cmd.parameters[i].type_param.name == arr.len
                && cmd.parameters[i].type_param.type == "uint32_t"
                && cmd.parameters[i].type_param.post_quals.empty()) {
                count_idx = i;
                break;
            }
        }
        if (count_idx < 0) continue;

        // Check 1:1: this count must not be referenced by any other array param
        bool shared = false;
        for (int k = 0; k < (int)cmd.parameters.size(); ++k) {
            if (k == j) continue;
            if (cmd.parameters[k].len == arr.len) { shared = true; break; }
        }
        if (shared) continue;

        if (cc.input_array_count < CommandClassification::max_input_arrays) {
            cc.input_arrays[cc.input_array_count++] = { count_idx, j };
        }
    }
}

// Everything else is instance-level. Global commands are handled separately.
bool Generator::is_device_level_command(const Command& cmd) const {
    if (cmd.parameters.empty()) return false;
    auto& type = types.at(cmd.parameters[0].type_param.type);
    if (type.category != Type::Category::Handle) return false;
    return type.handle->info->kind == HandleInfo::Kind::Device || type.handle->info->kind == HandleInfo::Kind::DeviceItself;
}

bool Generator::has_pnext(sv struct_type_name) {
    auto it = types.find(struct_type_name);
    assert(it != types.end());
    auto& t = it->second;
    if (t.category != Type::Category::Struct && t.category != Type::Category::Union) return false;
    for (auto& m : t.struct_->members) {
        if (m.type_param.name == "pNext") return true;
    }
    return false;
}

// Emits a single parameter in C++ wrapper style:
// - const VkXxxInfo* (struct/union pointer) → const XxxInfo& (reference)
// - All other cases (handles, primitives, etc.) fall through to stringify(),
//   which already strips the Vk prefix and handles pointers/const
void Generator::emit_wrapper_param(const CommandParameter& param, std::ofstream& file) {
    auto [param_is_handle, is_const_struct_ptr] = is_handle_or_const_ptr_struct_argument(param);

    if (is_const_struct_ptr) {
        sv type = param.type_param.type;
        if (type.starts_with("Vk")) type.remove_prefix(2);
        file << "const " << type << "& " << param.type_param.name;
    } else {
        file << param.type_param.stringify();
    }
}

// Emits comma-separated parameter names for forwarding calls.
void Generator::emit_forward_param_names(const Command& cmd, const CommandClassification& cc,
    std::ofstream& file, bool include_nothrow_output, int skip_idx) {
    WrapperEmitOptions opts;
    opts.output = include_nothrow_output ? WrapperEmitOptions::OutputMode::InPlace
        : WrapperEmitOptions::OutputMode::Skip;
    opts.skip_idx = skip_idx;
    emit_forward_param_names(cmd, cc, file, opts);
}

void Generator::emit_forward_param_names(const Command& cmd, const CommandClassification& cc,
    std::ofstream& file, const WrapperEmitOptions& opts) {
    using OM = WrapperEmitOptions::OutputMode;
    using AM = WrapperEmitOptions::ArrayMode;

    bool first = true;
    auto sep = [&]() { if (first) first = false; else file << ", "; };

    for (int i = 0; i < (int)cmd.parameters.size(); ++i) {
        if (i == cc.implicit_param_idx) continue;
        if (i == cc.output_param_idx && opts.output != OM::InPlace) continue;
        if (i == cc.count_param_idx || i == cc.array_param_idx) continue;
        if (i == opts.skip_idx) continue;
        if (i == cc.allocator_param_idx) continue;
        if (cc.is_input_count(i)) continue;
        auto& param = cmd.parameters[i];
        if (!param.api.is_vulkan()) continue;
        sep();
        if (cc.is_input_array(i)) {
            if (opts.array == AM::Singular)
                file << opts.singular_param_name;
            else
                file << NameTranslator::from_input_array_name(param.type_param.name).new_name;
        } else {
            file << param.type_param.name;
        }
    }
    if (opts.output == OM::Trailing && !opts.output_trailing_name.empty()) {
        sep();
        file << opts.output_trailing_name;
    }
}

// Emits "(params, vector<ElemType>& v)" for enumerate _noThrow signatures.
void Generator::emit_enumerate_nothrow_params(const Command& cmd, const CommandClassification& cc,
    std::ofstream& file, int skip_idx) {
    auto& arr_param = cmd.parameters[cc.array_param_idx];
    sv elem_type = arr_param.type_param.type;
    if (elem_type.starts_with("Vk")) elem_type.remove_prefix(2);

    file << "(";
    bool first = true;
    for (int i = 0; i < (int)cmd.parameters.size(); ++i) {
        if (i == cc.implicit_param_idx || i == cc.count_param_idx || i == cc.array_param_idx) continue;
        if (i == skip_idx) continue;
        if (i == cc.allocator_param_idx) continue;
        auto& param = cmd.parameters[i];
        if (!param.api.is_vulkan()) continue;
        if (first) first = false;
        else file << ", ";
        emit_wrapper_param(param, file);
    }
    if (!first) file << ", ";
    file << "vector<" << elem_type << ">& v)";
}

// Writes wrapper parameter list for C++ command wrappers.
void Generator::generate_wrapper_params(const Command& cmd, const CommandClassification& cc,
    std::ofstream& file, bool for_nothrow_output, int skip_idx) {
    WrapperEmitOptions opts;
    opts.output = for_nothrow_output ? WrapperEmitOptions::OutputMode::InPlace
        : WrapperEmitOptions::OutputMode::Skip;
    opts.skip_idx = skip_idx;
    generate_wrapper_params(cmd, cc, file, opts);
}

void Generator::generate_wrapper_params(const Command& cmd, const CommandClassification& cc,
    std::ofstream& file, const WrapperEmitOptions& opts) {
    using OM = WrapperEmitOptions::OutputMode;
    using AM = WrapperEmitOptions::ArrayMode;

    file << "(";
    bool first = true;
    auto sep = [&]() { if (first) first = false; else file << ", "; };

    for (int i = 0; i < (int)cmd.parameters.size(); ++i) {
        if (i == cc.implicit_param_idx || i == opts.skip_idx) continue;
        if (i == cc.output_param_idx && opts.output != OM::InPlace) continue;
        if (i == cc.count_param_idx || i == cc.array_param_idx) continue;
        if (cc.is_input_count(i)) continue;
        if (i == cc.allocator_param_idx) continue;

        auto& param = cmd.parameters[i];
        if (!param.api.is_vulkan()) continue;

        sep();

        // Output param InPlace (_noThrow for ResultCreate / ResultOutParam)
        if (i == cc.output_param_idx && opts.output == OM::InPlace) {
            sv type = param.type_param.type;
            if (type.starts_with("Vk")) type.remove_prefix(2);
            file << type << "& " << param.type_param.name;
            continue;
        }

        // Input array param: span<T> pName → const T& name (Singular) or const span<T> name (Span)
        if (cc.is_input_array(i)) {
            sv type = param.type_param.type;
            if (type.starts_with("Vk")) type.remove_prefix(2);
            if (opts.array == AM::Singular)
                file << "const " << type << "& " << opts.singular_param_name;
            else
                file << "const span<" << type << "> " << NameTranslator::from_input_array_name(param.type_param.name).new_name;
            continue;
        }

        emit_wrapper_param(param, file);
    }
    if (opts.output == OM::Trailing && !opts.output_trailing_decl.empty()) {
        sep();
        file << opts.output_trailing_decl;
    }
    file << ")";
}

// Writes call arguments for the funcs.vkXxx(...) invocation.
void Generator::generate_call_args(const Command& cmd, const CommandClassification& cc,
    std::ofstream& file, bool for_nothrow) {
    WrapperEmitOptions opts;
    opts.output = for_nothrow ? WrapperEmitOptions::OutputMode::InPlace
        : WrapperEmitOptions::OutputMode::Skip;
    generate_call_args(cmd, cc, file, opts);
}

void Generator::generate_call_args(const Command& cmd, const CommandClassification& cc,
    std::ofstream& file, const WrapperEmitOptions& opts) {
    using OM = WrapperEmitOptions::OutputMode;
    using AM = WrapperEmitOptions::ArrayMode;

    file << "(";
    bool first = true;
    auto sep = [&]() { if (first) first = false; else file << ", "; };

    for (int i = 0; i < (int)cmd.parameters.size(); ++i) {
        auto& param = cmd.parameters[i];
        if (!param.api.is_vulkan()) continue;

        sep();

        if (i == cc.implicit_param_idx) { file << cc.implicit_global << ".handle()"; continue; }
        if (i == cc.allocator_param_idx) { file << "detail::_allocator"; continue; }

        // Input array pairs
        bool handled_input_array = false;
        for (int k = 0; k < cc.input_array_count; ++k) {
            if (i == cc.input_arrays[k].count_idx) {
                if (opts.array == AM::Singular) file << "1";
                else {
                    auto vec_name = NameTranslator::from_input_array_name(cmd.parameters[cc.input_arrays[k].array_idx].type_param.name).new_name;
                    file << vec_name << ".size()";
                }
                handled_input_array = true; break;
            }
            if (i == cc.input_arrays[k].array_idx) {
                if (opts.array == AM::Singular) file << "&" << opts.singular_param_name;
                else {
                    auto vec_name = NameTranslator::from_input_array_name(cmd.parameters[cc.input_arrays[k].array_idx].type_param.name).new_name;
                    file << vec_name << ".data()";
                }
                handled_input_array = true; break;
            }
        }
        if (handled_input_array) continue;

        if (i == cc.output_param_idx) {
            if (!opts.output_expr.empty()) {
                file << opts.output_expr;
            } else if (opts.output == OM::InPlace) {
                // _noThrow: output through reinterpret_cast or &name
                bool out_is_handle = is_handle(param.type_param.type);
                if (out_is_handle) {
                    sv type = param.type_param.type;
                    if (type.starts_with("Vk")) type.remove_prefix(2);
                    file << "reinterpret_cast<" << type << "::HandleType*>(&" << param.type_param.name << ")";
                } else {
                    file << "&" << param.type_param.name;
                }
            } else {
                // Default _throw: local var `h`
                file << "&h";
            }
            continue;
        }

        auto [param_is_handle, is_const_struct_ptr] = is_handle_or_const_ptr_struct_argument(param);

        if (is_const_struct_ptr) {
            file << "&" << param.type_param.name;
        } else if (param_is_handle) {
            bool is_pointer = !param.type_param.post_quals.empty()
                && bool(param.type_param.post_quals[0] & TypeParam::PostQualifier::Pointer);
            if (is_pointer && param.type_param.is_const()) {
                sv type = param.type_param.type;
                if (type.starts_with("Vk")) type.remove_prefix(2);
                file << "reinterpret_cast<const " << type << "::HandleType*>(" << param.type_param.name << ")";
            } else if (is_pointer) {
                sv type = param.type_param.type;
                if (type.starts_with("Vk")) type.remove_prefix(2);
                file << "reinterpret_cast<" << type << "::HandleType*>(" << param.type_param.name << ")";
            } else {
                file << param.type_param.name << ".handle()";
            }
        } else {
            file << param.type_param.name;
        }
    }
    file << ")";
}

// Pattern 1: void commands — single noexcept inline wrapper
void Generator::generate_wrapper_void(const Command& cmd, const CommandClassification& cc, std::ofstream& file) {
    file << "inline void " << NameTranslator::from_command_name(cmd.name);
    generate_wrapper_params(cmd, cc, file);
    file << " noexcept { funcs." << cmd.name;
    generate_call_args(cmd, cc, file);
    file << "; }\n";
}

// Pattern 2: VkResult → void — _throw, _noThrow, default
void Generator::generate_wrapper_result_void(const Command& cmd, const CommandClassification& cc, std::ofstream& file) {
    auto name = NameTranslator::from_command_name(cmd.name);

    if (should_emit_throw()) {
        sv suffix = should_emit_default() ? "_throw" : "";
        file << "inline void " << name << suffix;
        generate_wrapper_params(cmd, cc, file);
        file << " { Result r = funcs." << cmd.name;
        generate_call_args(cmd, cc, file);
        file << "; checkForSuccessValue(r, \"" << cmd.name << "\"); }\n";
    }

    if (should_emit_nothrow()) {
        sv suffix = should_emit_default() ? "_noThrow" : "";
        file << "inline Result " << name << suffix;
        generate_wrapper_params(cmd, cc, file);
        file << " noexcept { return funcs." << cmd.name;
        generate_call_args(cmd, cc, file);
        file << "; }\n";
    }

    if (should_emit_default()) {
        file << "inline " << (default_is_throw() ? "void " : "Result ") << name;
        generate_wrapper_params(cmd, cc, file);
        if (!default_is_throw()) file << " noexcept";
        file << " { ";
        if (!default_is_throw()) file << "return ";
        file << name << (default_is_throw() ? "_throw" : "_noThrow");
        // Forward just the param names
        file << "(";
        emit_forward_param_names(cmd, cc, file);
        file << "); }\n";
    }
}

// Pattern 3: VkResult → creates handle — _throw, _noThrow, default
void Generator::generate_wrapper_result_create(const Command& cmd, const CommandClassification& cc, std::ofstream& file) {
    auto name = NameTranslator::from_command_name(cmd.name);
    auto& out_param = cmd.parameters[cc.output_param_idx];
    sv out_type = out_param.type_param.type;
    if (out_type.starts_with("Vk")) out_type.remove_prefix(2);

    if (should_emit_throw()) {
        sv suffix = should_emit_default() ? "_throw" : "";
        file << "inline " << out_type << " " << name << suffix;
        generate_wrapper_params(cmd, cc, file); // skips output param
        file << " { " << out_type << "::HandleType h; Result r = funcs." << cmd.name;
        generate_call_args(cmd, cc, file, false);
        if (cc.output_has_destroy)
            file << "; detail::processResult(r, h, \"" << cmd.name << "\"); return h; }\n";
        else
            file << "; checkForSuccessValue(r, \"" << cmd.name << "\"); return h; }\n";
    }

    if (should_emit_nothrow()) {
        sv suffix = should_emit_default() ? "_noThrow" : "";
        file << "inline Result " << name << suffix;
        generate_wrapper_params(cmd, cc, file, true); // includes output param as Handle&
        file << " noexcept { return funcs." << cmd.name;
        generate_call_args(cmd, cc, file, true);
        file << "; }\n";
    }

    if (should_emit_default()) {
        sv target = default_is_throw() ? "_throw" : "_noThrow";
        if (default_is_throw()) {
            file << "inline " << out_type << " " << name;
            generate_wrapper_params(cmd, cc, file);
            file << " { return " << name << target << "(";
            emit_forward_param_names(cmd, cc, file);
        } else {
            file << "inline Result " << name;
            generate_wrapper_params(cmd, cc, file, true);
            file << " noexcept { return " << name << target << "(";
            emit_forward_param_names(cmd, cc, file, true);
        }
        file << "); }\n";
    }
}

// Pattern 3b: VkResult → creates N handles into output array sharing a count param.
// Emits vector<T> and vector<UniqueT> wrappers (throw/noThrow/default gated), plus
// singular convenience overloads (createGraphicsPipelines → createGraphicsPipeline) when the
// command has an input array of create-infos. Input array is rendered as `const span<T>`
// (plural) or `const T&` (singular); count param is dropped.
void Generator::generate_wrapper_result_create_array(const Command& cmd, const CommandClassification& cc, std::ofstream& file) {
    using OM = WrapperEmitOptions::OutputMode;
    using AM = WrapperEmitOptions::ArrayMode;
    assert(cc.pattern == CommandClassification::Pattern::ResultCreateArray);
    assert(cc.output_param_idx >= 0 && cc.count_param_idx >= 0);

    const auto [unique_name, name] = NameTranslator::unique_command_name(cmd.name);
    auto& out_param = cmd.parameters[cc.output_param_idx];
    sv out_type_sv = out_param.type_param.type;
    if (out_type_sv.starts_with("Vk")) out_type_sv.remove_prefix(2);

    std::string out_type(out_type_sv);
    std::string unique_type = "Unique" + out_type;
    std::string out_name(out_param.type_param.name);

    std::string size_expr;
    if (cc.input_array_count > 0) {
        auto& arr_param = cmd.parameters[cc.input_arrays[0].array_idx];
        size_expr = std::string(NameTranslator::from_input_array_name(arr_param.type_param.name).new_name) + ".size()";
    } else {
        size_expr = std::string(cmd.parameters[cc.count_param_idx].type_param.name);
    }

    sv destroy_cmd_name;
    if (auto it = types.find(out_param.type_param.type); it != types.end()
        && it->second.handle && it->second.handle->info
        && it->second.handle->info->destroy_command)
        destroy_cmd_name = it->second.handle->info->destroy_command->name;

    // Helper struct for output variants
    struct Variant {
        sv fn_name;           // e.g. "createGraphicsPipelines" or "createGraphicsPipeline"
        sv return_throw;      // e.g. "vector<Pipeline>" or "Pipeline"
        sv trailing_decl;     // noThrow output decl, e.g. "vector<Pipeline>& pPipelines"
        sv trailing_name;     // noThrow forward name, e.g. "pPipelines" or "pipeline"
        sv call_output_expr;  // expression written at output slot (empty → default "&h")
        AM array_mode;        // Span (plural) or Singular
        sv singular;          // singular_param_name (Singular only)
    };

    // Emits the _throw/_noThrow-shorthand default wrapper.
    auto emit_default_forward = [&](const Variant& v) {
        sv target = default_is_throw() ? "_throw" : "_noThrow";
        if (default_is_throw()) {
            file << "inline " << v.return_throw << " " << v.fn_name;
            generate_wrapper_params(cmd, cc, file,
                WrapperEmitOptions{ .array = v.array_mode, .singular_param_name = v.singular });
            file << " { return " << v.fn_name << target << "(";
            emit_forward_param_names(cmd, cc, file,
                WrapperEmitOptions{ .array = v.array_mode, .singular_param_name = v.singular });
        } else {
            file << "inline Result " << v.fn_name;
            generate_wrapper_params(cmd, cc, file,
                WrapperEmitOptions{ .output = OM::Trailing, .array = v.array_mode,
                                   .output_trailing_decl = v.trailing_decl, .singular_param_name = v.singular });
            file << " noexcept { return " << v.fn_name << target << "(";
            emit_forward_param_names(cmd, cc, file,
                WrapperEmitOptions{ .output = OM::Trailing, .array = v.array_mode,
                                   .output_trailing_name = v.trailing_name, .singular_param_name = v.singular });
        }
        file << "); }\n";
        };

    std::string plural_out_expr = "reinterpret_cast<" + out_type + "::HandleType*>(" + out_name + ".data())";

    // ---- vector<T> ----
    {
        std::string ret = "vector<" + out_type + ">";
        std::string trailing = "vector<" + out_type + ">& " + out_name;
        Variant v{
            .fn_name = name,
            .return_throw = ret,
            .trailing_decl = trailing,
            .trailing_name = out_name,
            .call_output_expr = plural_out_expr,
            .array_mode = AM::Span,
            .singular = {},
        };
        if (should_emit_throw()) {
            sv suffix = should_emit_default() ? "_throw" : "";
            file << "inline " << v.return_throw << " " << v.fn_name << suffix;
            generate_wrapper_params(cmd, cc, file, WrapperEmitOptions{ .array = v.array_mode });
            file << " { " << v.return_throw << " " << out_name << "; if (!" << out_name << ".alloc_noThrow(" << size_expr
                << ")) throw OutOfHostMemoryError(\"" << v.fn_name << suffix << "(): Out of host memory.\"); Result r = funcs." << cmd.name;
            generate_call_args(cmd, cc, file, WrapperEmitOptions{ .array = v.array_mode, .output_expr = v.call_output_expr });
            file << "; if (r != Result::eSuccess) { ";
            if (!destroy_cmd_name.empty())
                file << "for (size_t i = 0; i < " << size_expr << "; i++) funcs." << destroy_cmd_name << "("
                << cc.implicit_global << ".handle(), " << out_name << "[i].handle(), detail::_allocator); ";
            file << "throwResultException(r, \"" << cmd.name << "\"); } return " << out_name << "; }\n";
        }
        if (should_emit_nothrow()) {
            sv suffix = should_emit_default() ? "_noThrow" : "";
            file << "inline Result " << v.fn_name << suffix;
            generate_wrapper_params(cmd, cc, file,
                WrapperEmitOptions{ .output = OM::Trailing, .array = v.array_mode, .output_trailing_decl = v.trailing_decl });
            file << " noexcept { if (!" << out_name << ".alloc_noThrow(" << size_expr
                << ")) return Result::eErrorOutOfHostMemory; return funcs." << cmd.name;
            generate_call_args(cmd, cc, file, WrapperEmitOptions{ .array = v.array_mode, .output_expr = v.call_output_expr });
            file << "; }\n";
        }
        if (should_emit_default()) emit_default_forward(v);
    }

    // ---- vector<UniqueT> ----
    {
        std::string ret = "vector<" + unique_type + ">";
        std::string trailing = "vector<" + unique_type + ">& " + out_name;
        Variant v{
            .fn_name = unique_name,
            .return_throw = ret,
            .trailing_decl = trailing,
            .trailing_name = out_name,
            .call_output_expr = plural_out_expr,
            .array_mode = AM::Span,
            .singular = {},
        };
        if (should_emit_throw()) {
            sv suffix = should_emit_default() ? "_throw" : "";
            file << "inline " << v.return_throw << " " << v.fn_name << suffix;
            generate_wrapper_params(cmd, cc, file, WrapperEmitOptions{ .array = v.array_mode });
            file << " { " << v.return_throw << " " << out_name << "; if (!" << out_name << ".alloc_noThrow(" << size_expr
                << ")) throw OutOfHostMemoryError(\"" << v.fn_name << suffix << "(): Out of host memory.\"); Result r = funcs." << cmd.name;
            generate_call_args(cmd, cc, file, WrapperEmitOptions{ .array = v.array_mode, .output_expr = v.call_output_expr });
            file << "; if (r != Result::eSuccess) throwResultException(r, \"" << cmd.name << "\"); return " << out_name << "; }\n";
        }
        if (should_emit_nothrow()) {
            sv suffix = should_emit_default() ? "_noThrow" : "";
            file << "inline Result " << v.fn_name << suffix;
            generate_wrapper_params(cmd, cc, file,
                WrapperEmitOptions{ .output = OM::Trailing, .array = v.array_mode, .output_trailing_decl = v.trailing_decl });
            file << " noexcept { for (uint32_t i = 0; i < " << size_expr << "; i++) " << out_name
                << "[i].reset(); if (!" << out_name << ".alloc_noThrow(" << size_expr
                << ")) return Result::eErrorOutOfHostMemory; return funcs." << cmd.name;
            generate_call_args(cmd, cc, file, WrapperEmitOptions{ .array = v.array_mode, .output_expr = v.call_output_expr });
            file << "; }\n";
        }
        if (should_emit_default()) emit_default_forward(v);
    }

    // ---- Singular convenience overloads ----
    if (cc.input_array_count == 0) return;
    std::string singular_name = NameTranslator::singularize(name);
    if (singular_name == name) return;

    std::string singular_unique_name = singular_name + "Unique";
    auto& arr_param = cmd.parameters[cc.input_arrays[0].array_idx];
    std::string singular_param_name = NameTranslator::singularize(
        NameTranslator::from_input_array_name(arr_param.type_param.name).new_name);
    std::string out_ref_name = NameTranslator::singularize(
        NameTranslator::from_input_array_name(out_name).new_name);
    std::string singular_out_expr = "reinterpret_cast<" + out_type + "::HandleType*>(&" + out_ref_name + ")";

    // ---- Singular T ----
    {
        std::string trailing = out_type + "& " + out_ref_name;
        Variant v{
            .fn_name = singular_name,
            .return_throw = out_type,
            .trailing_decl = trailing,
            .trailing_name = out_ref_name,
            .call_output_expr = singular_out_expr, // only used in the noThrow body; throw uses default "&h"
            .array_mode = AM::Singular,
            .singular = singular_param_name,
        };
        if (should_emit_throw()) {
            sv suffix = should_emit_default() ? "_throw" : "";
            file << "inline " << v.return_throw << " " << v.fn_name << suffix;
            generate_wrapper_params(cmd, cc, file,
                WrapperEmitOptions{ .array = v.array_mode, .singular_param_name = v.singular });
            file << " { " << v.return_throw << "::HandleType h; Result r = funcs." << cmd.name;
            generate_call_args(cmd, cc, file,
                WrapperEmitOptions{ .array = v.array_mode, .singular_param_name = v.singular });
            file << "; detail::processResult(r, h, \"" << cmd.name << "\"); return h; }\n";
        }
        if (should_emit_nothrow()) {
            sv suffix = should_emit_default() ? "_noThrow" : "";
            file << "inline Result " << v.fn_name << suffix;
            generate_wrapper_params(cmd, cc, file,
                WrapperEmitOptions{ .output = OM::Trailing, .array = v.array_mode,
                                   .output_trailing_decl = v.trailing_decl, .singular_param_name = v.singular });
            file << " noexcept { return funcs." << cmd.name;
            generate_call_args(cmd, cc, file,
                WrapperEmitOptions{ .array = v.array_mode, .output_expr = v.call_output_expr, .singular_param_name = v.singular });
            file << "; }\n";
        }
        if (should_emit_default()) emit_default_forward(v);
    }

    // ---- Singular UniqueT ----
    {
        std::string trailing = unique_type + "& " + out_ref_name;
        Variant v{
            .fn_name = singular_unique_name,
            .return_throw = unique_type,
            .trailing_decl = trailing,
            .trailing_name = out_ref_name,
            .call_output_expr = singular_out_expr,
            .array_mode = AM::Singular,
            .singular = singular_param_name,
        };
        if (should_emit_throw()) {
            sv suffix = should_emit_default() ? "_throw" : "";
            file << "inline " << v.return_throw << " " << v.fn_name << suffix;
            generate_wrapper_params(cmd, cc, file,
                WrapperEmitOptions{ .array = v.array_mode, .singular_param_name = v.singular });
            file << " { return " << v.return_throw << "(" << singular_name << suffix << "(";
            emit_forward_param_names(cmd, cc, file,
                WrapperEmitOptions{ .array = v.array_mode, .singular_param_name = v.singular });
            file << ")); }\n";
        }
        if (should_emit_nothrow()) {
            sv suffix = should_emit_default() ? "_noThrow" : "";
            file << "inline Result " << v.fn_name << suffix;
            generate_wrapper_params(cmd, cc, file,
                WrapperEmitOptions{ .output = OM::Trailing, .array = v.array_mode,
                                   .output_trailing_decl = v.trailing_decl, .singular_param_name = v.singular });
            file << " noexcept { " << out_ref_name << ".reset(); return funcs." << cmd.name;
            generate_call_args(cmd, cc, file,
                WrapperEmitOptions{ .array = v.array_mode, .output_expr = v.call_output_expr, .singular_param_name = v.singular });
            file << "; }\n";
        }
        if (should_emit_default()) emit_default_forward(v);
    }
}

// Pattern 4: void → out-param — single noexcept wrapper
void Generator::generate_wrapper_void_outparam(const Command& cmd, const CommandClassification& cc, std::ofstream& file) {
    auto name = NameTranslator::from_command_name(cmd.name);
    auto& out_param = cmd.parameters[cc.output_param_idx];
    sv out_type = out_param.type_param.type;
    if (out_type.starts_with("Vk")) out_type.remove_prefix(2);

    bool out_is_handle = is_handle(out_param.type_param.type);
    // Handles always return by value. Structs with pNext return by reference (caller sets chain).
    bool by_ref = !out_is_handle && has_pnext(out_param.type_param.type);

    if (by_ref) {
        // void getPhysicalDeviceProperties2(PhysicalDevice pd, PhysicalDeviceProperties2& p) noexcept
        file << "inline void " << name;
        generate_wrapper_params(cmd, cc, file, true); // include output as ref
        file << " noexcept { funcs." << cmd.name;
        generate_call_args(cmd, cc, file, true);
        file << "; }\n";
    } else {
        // Return by value: Queue getDeviceQueue(...), PhysicalDeviceProperties getPhysicalDeviceProperties(...)
        sv local_type = out_is_handle ? sv("::HandleType") : sv("");
        file << "inline " << out_type << " " << name;
        generate_wrapper_params(cmd, cc, file); // skip output
        file << " noexcept { " << out_type << local_type << " h; funcs." << cmd.name;
        generate_call_args(cmd, cc, file, false);
        file << "; return h; }\n";
    }
}

// Pattern 6: VkResult + non-handle out-param — _throw, _noThrow, default
void Generator::generate_wrapper_result_outparam(const Command& cmd, const CommandClassification& cc, std::ofstream& file) {
    auto name = NameTranslator::from_command_name(cmd.name);
    auto& out_param = cmd.parameters[cc.output_param_idx];
    sv out_type = out_param.type_param.type;
    if (out_type.starts_with("Vk")) out_type.remove_prefix(2);

    bool by_ref = has_pnext(out_param.type_param.type);

    if (should_emit_throw()) {
        sv suffix = should_emit_default() ? "_throw" : "";
        if (by_ref) {
            // _throw takes output by ref and returns void
            file << "inline void " << name << suffix;
            generate_wrapper_params(cmd, cc, file, true);
            file << " { Result r = funcs." << cmd.name;
            generate_call_args(cmd, cc, file, true);
            file << "; checkForSuccessValue(r, \"" << cmd.name << "\"); }\n";
        } else {
            // _throw returns struct by value
            file << "inline " << out_type << " " << name << suffix;
            generate_wrapper_params(cmd, cc, file);
            file << " { " << out_type << " h; Result r = funcs." << cmd.name;
            generate_call_args(cmd, cc, file, false);
            file << "; checkForSuccessValue(r, \"" << cmd.name << "\"); return h; }\n";
        }
    }

    if (should_emit_nothrow()) {
        sv suffix = should_emit_default() ? "_noThrow" : "";
        file << "inline Result " << name << suffix;
        generate_wrapper_params(cmd, cc, file, true);
        file << " noexcept { return funcs." << cmd.name;
        generate_call_args(cmd, cc, file, true);
        file << "; }\n";
    }

    if (should_emit_default()) {
        sv target = default_is_throw() ? "_throw" : "_noThrow";
        if (default_is_throw()) {
            if (by_ref) {
                file << "inline void " << name;
                generate_wrapper_params(cmd, cc, file, true);
                file << " { " << name << target << "(";
                emit_forward_param_names(cmd, cc, file, true);
            } else {
                file << "inline " << out_type << " " << name;
                generate_wrapper_params(cmd, cc, file);
                file << " { return " << name << target << "(";
                emit_forward_param_names(cmd, cc, file);
            }
        } else {
            // _noThrow always takes output by ref and returns Result
            file << "inline Result " << name;
            generate_wrapper_params(cmd, cc, file, true);
            file << " noexcept { return " << name << target << "(";
            emit_forward_param_names(cmd, cc, file, true);
        }
        file << "); }\n";
    }
}

// Emits funcs.vkXxx(...) call for enumerate pattern.
// second_call: false emits nullptr for array param, true emits v.data()
void Generator::generate_enumerate_call(const Command& cmd, const CommandClassification& cc,
    std::ofstream& file, bool second_call) {
    file << "funcs." << cmd.name << "(";
    bool first = true;
    for (int i = 0; i < (int)cmd.parameters.size(); ++i) {
        auto& param = cmd.parameters[i];
        if (!param.api.is_vulkan()) continue;
        if (first) first = false;
        else file << ", ";

        if (i == cc.implicit_param_idx) {
            file << cc.implicit_global << ".handle()";
        } else if (i == cc.count_param_idx) {
            file << "&n";
        } else if (i == cc.array_param_idx) {
            if (second_call)
                if (config.generate_handle_class && is_handle(param.type_param.type)) {
                    sv type = param.type_param.type;
                    if (type.starts_with("Vk")) type.remove_prefix(2);
                    file << "reinterpret_cast<" << type << "::HandleType*>(v.data())";
                } else
                    file << "v.data()";
            else
                file << "nullptr";
        } else if (i == cc.allocator_param_idx) {
            file << "detail::_allocator";
        } else {
            auto [param_is_handle, is_const_struct_ptr] = is_handle_or_const_ptr_struct_argument(param);

            if (is_const_struct_ptr) {
                file << "&" << param.type_param.name;
            } else if (param_is_handle) {
                file << param.type_param.name << ".handle()";
            } else {
                file << param.type_param.name;
            }
        }
    }
    file << ")";
}

// Header: forward declarations for enumerate commands
void Generator::generate_wrapper_enumerate_header(const Command& cmd, const CommandClassification& cc, std::ofstream& file) {
    auto name = NameTranslator::from_command_name(cmd.name);
    auto& arr_param = cmd.parameters[cc.array_param_idx];
    sv elem_type = arr_param.type_param.type;
    if (elem_type.starts_with("Vk")) elem_type.remove_prefix(2);

    if (should_emit_throw()) {
        sv suffix = should_emit_default() ? "_throw" : "";
        file << "vector<" << elem_type << "> " << name << suffix;
        generate_wrapper_params(cmd, cc, file);
        file << ";\n";
    }

    if (should_emit_nothrow()) {
        sv suffix = should_emit_default() ? "_noThrow" : "";
        file << "Result " << name << suffix;
        emit_enumerate_nothrow_params(cmd, cc, file);
        file << " noexcept;\n";
    }

    if (should_emit_default()) {
        sv target = default_is_throw() ? "_throw" : "_noThrow";
        if (default_is_throw()) {
            file << "inline vector<" << elem_type << "> " << name;
            generate_wrapper_params(cmd, cc, file);
            file << " { return " << name << target << "(";
            emit_forward_param_names(cmd, cc, file);
            file << "); }\n";
        } else {
            file << "inline Result " << name;
            emit_enumerate_nothrow_params(cmd, cc, file);
            file << " noexcept { return " << name << target << "(";
            emit_forward_param_names(cmd, cc, file);
            file << ", v); }\n";
        }
    }
}

// Source (.cpp): implementations for enumerate commands
void Generator::generate_wrapper_enumerate_source(const Command& cmd, const CommandClassification& cc, std::ofstream& file) {
    auto name = NameTranslator::from_command_name(cmd.name);
    auto& arr_param = cmd.parameters[cc.array_param_idx];
    sv elem_type = arr_param.type_param.type;
    if (elem_type.starts_with("Vk")) elem_type.remove_prefix(2);

    bool returns_result = (cmd.type == "VkResult");
    assert(returns_result || cmd.type == "void");

    // _throw variant
    if (should_emit_throw()) {
        sv suffix = should_emit_default() ? "_throw" : "";
        file << "vector<" << elem_type << "> " << name << suffix;
        generate_wrapper_params(cmd, cc, file);
        file << " {\n";
        file << "    vector<" << elem_type << "> v;\n";
        file << "    uint32_t n;\n";

        if (returns_result) {
            // do-while with eIncomplete
            file << "    Result r;\n"
                << "    do {\n"
                << "        r = "; generate_enumerate_call(cmd, cc, file, false); file << ";\n"
                << "        checkForSuccessValue(r, \"" << cmd.name << "\");\n"
                << "        v.alloc(n);\n"
                << "        r = "; generate_enumerate_call(cmd, cc, file, true); file << ";\n"
                << "        checkSuccess(r, \"" << cmd.name << "\");\n"
                << "    } while (r == Result::eIncomplete);\n"
                << "    if (n != v.size()) v.resize(n);\n";
        } else {
            // void return: single two-call, no loop
            file << "    "; generate_enumerate_call(cmd, cc, file, false); file << ";\n"
                << "    v.alloc(n);\n"
                << "    "; generate_enumerate_call(cmd, cc, file, true); file << ";\n";
        }
        file << "    return v;\n}\n\n";
    }

    // _noThrow variant
    if (should_emit_nothrow()) {
        sv suffix = should_emit_default() ? "_noThrow" : "";
        file << "Result " << name << suffix;
        emit_enumerate_nothrow_params(cmd, cc, file);
        file << " noexcept {\n";
        file << "    uint32_t n;\n";

        if (returns_result) {
            file << "    Result r;\n"
                << "    do {\n"
                << "        r = "; generate_enumerate_call(cmd, cc, file, false); file << ";\n"
                << "        if (r != Result::eSuccess) return r;\n"
                << "        if (!v.alloc_noThrow(n)) return Result::eErrorOutOfHostMemory;\n"
                << "        r = "; generate_enumerate_call(cmd, cc, file, true); file << ";\n"
                << "        if (int32_t(r) < 0) return r;\n"
                << "    } while (r == Result::eIncomplete);\n"
                << "    if (n != v.size())\n"
                << "        if (!v.resize_noThrow(n)) return Result::eErrorOutOfHostMemory;\n";
        } else {
            file << "    "; generate_enumerate_call(cmd, cc, file, false); file << ";\n"
                << "    if (!v.alloc_noThrow(n)) return Result::eErrorOutOfHostMemory;\n"
                << "    "; generate_enumerate_call(cmd, cc, file, true); file << ";\n";
        }
        file << "    return Result::eSuccess;\n}\n\n";
    }
}

// UniqueHandle create variants: createXxxUnique_throw/noThrow/default
void Generator::generate_unique_handle_create(const Command& cmd, const CommandClassification& cc, std::ofstream& file) {
    assert(cc.pattern == CommandClassification::Pattern::ResultCreate && cc.output_has_destroy);

    const auto [unique_name, name] = NameTranslator::unique_command_name(cmd.name);
    auto& out_param = cmd.parameters[cc.output_param_idx];
    sv out_type = out_param.type_param.type;
    if (out_type.starts_with("Vk")) out_type.remove_prefix(2);
    std::string unique_type = "Unique" + std::string(out_type);

    // _throw: return UniqueXxx(createXxx_throw(params))
    if (should_emit_throw()) {
        sv suffix = should_emit_default() ? "_throw" : "";
        file << "inline " << unique_type << " " << unique_name << suffix;
        generate_wrapper_params(cmd, cc, file);
        file << " { return " << unique_type << "(" << name << suffix << "(";
        emit_forward_param_names(cmd, cc, file);
        file << ")); }\n";
    }

    // _noThrow: reset + write directly into UniqueHandle via reinterpret_cast
    if (should_emit_nothrow()) {
        sv suffix = should_emit_default() ? "_noThrow" : "";
        file << "inline Result " << unique_name << suffix << "(";
        bool first = true;
        for (int i = 0; i < (int)cmd.parameters.size(); ++i) {
            if (i == cc.implicit_param_idx || i == cc.output_param_idx) continue;
            if (i == cc.allocator_param_idx) continue;
            auto& param = cmd.parameters[i];
            if (!param.api.is_vulkan()) continue;
            if (first) first = false;
            else file << ", ";
            emit_wrapper_param(param, file);
        }
        if (!first) file << ", ";
        file << unique_type << "& " << out_param.type_param.name << ") noexcept { "
            << out_param.type_param.name << ".reset(); return funcs." << cmd.name;
        generate_call_args(cmd, cc, file, true);
        file << "; }\n";
    }

    if (should_emit_default()) {
        sv target = default_is_throw() ? "_throw" : "_noThrow";
        if (default_is_throw()) {
            file << "inline " << unique_type << " " << unique_name;
            generate_wrapper_params(cmd, cc, file);
            file << " { return " << unique_name << target << "(";
            emit_forward_param_names(cmd, cc, file);
            file << "); }\n";
        } else {
            // Forward to _noThrow: takes UniqueXxx& as output, returns Result
            file << "inline Result " << unique_name << "(";
            bool first = true;
            for (int i = 0; i < (int)cmd.parameters.size(); ++i) {
                if (i == cc.implicit_param_idx || i == cc.output_param_idx) continue;
                if (i == cc.allocator_param_idx) continue;
                auto& param = cmd.parameters[i];
                if (!param.api.is_vulkan()) continue;
                if (first) first = false;
                else file << ", ";
                emit_wrapper_param(param, file);
            }
            if (!first) file << ", ";
            file << unique_type << "& " << out_param.type_param.name << ") noexcept { return "
                << unique_name << target << "(";
            emit_forward_param_names(cmd, cc, file);
            file << ", " << out_param.type_param.name << "); }\n";
        }
    }
}

// PhysicalDevice convenience overloads: drop the PhysicalDevice param, forward to physicalDevice()
void Generator::generate_physical_device_overloads(const Command& cmd, const CommandClassification& cc, std::ofstream& file) {
    assert(!cmd.parameters.empty() && cmd.parameters[0].type_param.type == "VkPhysicalDevice");
    constexpr int pd_idx = 0;

    auto name = NameTranslator::from_command_name(cmd.name);
    bool is_enumerate = (cc.pattern == CommandClassification::Pattern::Enumerate);


    // Helper: emit "physicalDevice(), remaining_args"
    auto emit_pd_forward = [&](bool include_nothrow_output) {
        file << "physicalDevice()";
        for (int i = 0; i < (int)cmd.parameters.size(); ++i) {
            if (i == pd_idx || i == cc.implicit_param_idx) continue;
            if (i == cc.count_param_idx || i == cc.array_param_idx) continue;
            if (i == cc.output_param_idx && !include_nothrow_output) continue;
            if (i == cc.allocator_param_idx) continue;
            if (cc.is_input_count(i)) continue;
            auto& param = cmd.parameters[i];
            if (!param.api.is_vulkan()) continue;
            if (cc.is_input_array(i))
                file << ", " << NameTranslator::from_input_array_name(param.type_param.name).new_name;
            else
                file << ", " << param.type_param.name;
        }
        };

    // Helper: emit a single PD overload line
    // inline RetType name[decl_suffix](params_no_pd) [noexcept] { [return] name[call_suffix](physicalDevice(), args); }
    auto emit_overload = [&](sv ret_type, sv decl_suffix, sv call_suffix,
        bool nothrow_output, bool is_noexcept, bool has_return) {
            file << "inline " << ret_type << " " << name << decl_suffix;
            generate_wrapper_params(cmd, cc, file, nothrow_output, pd_idx);
            if (is_noexcept) file << " noexcept";
            file << " { ";
            if (has_return) file << "return ";
            file << name << call_suffix << "(";
            emit_pd_forward(nothrow_output);
            file << "); }\n";
        };

    // Determine output behavior
    bool by_ref = false;
    if (cc.output_param_idx >= 0
        && (cc.pattern == CommandClassification::Pattern::VoidOutParam || cc.pattern == CommandClassification::Pattern::ResultOutParam)) {
        by_ref = !is_handle(cmd.parameters[cc.output_param_idx].type_param.type)
            && has_pnext(cmd.parameters[cc.output_param_idx].type_param.type);
    }

    // Determine return type
    std::string return_type;
    if (is_enumerate) {
        sv t = cmd.parameters[cc.array_param_idx].type_param.type;
        if (t.starts_with("Vk")) t.remove_prefix(2);
        return_type = "vector<" + std::string(t) + ">";
    } else if (cc.output_param_idx >= 0) {
        sv t = cmd.parameters[cc.output_param_idx].type_param.type;
        if (t.starts_with("Vk")) t.remove_prefix(2);
        return_type = std::string(t);
    } else {
        return_type = "void";
    }


    // Void and VoidOutParam have no _throw/_noThrow — just one overload
    if (cc.pattern == CommandClassification::Pattern::Void) {
        emit_overload("void", "", "", false, true, false);
        return;
    }
    if (cc.pattern == CommandClassification::Pattern::VoidOutParam) {
        if (by_ref)
            emit_overload("void", "", "", true, true, false);
        else
            emit_overload(return_type, "", "", false, true, true);
        return;
    }

    // Patterns with _throw/_noThrow variants
    if (should_emit_throw()) {
        sv suffix = should_emit_default() ? "_throw" : "";
        bool returns_void = (cc.pattern == CommandClassification::Pattern::ResultVoid) || by_ref;
        if (returns_void)
            emit_overload("void", suffix, suffix, by_ref, false, false);
        else
            emit_overload(return_type, suffix, suffix, false, false, true);
    }

    if (should_emit_nothrow()) {
        sv suffix = should_emit_default() ? "_noThrow" : "";
        if (is_enumerate) {
            file << "inline Result " << name << suffix;
            emit_enumerate_nothrow_params(cmd, cc, file, pd_idx);
            file << " noexcept { return " << name << suffix << "(";
            emit_pd_forward(false);
            file << ", v); }\n";
        } else {
            emit_overload("Result", suffix, suffix, true, true, true);
        }
    }

    if (should_emit_default()) {
        sv target = default_is_throw() ? "_throw" : "_noThrow";
        if (default_is_throw()) {
            bool returns_void = (cc.pattern == CommandClassification::Pattern::ResultVoid) || by_ref;
            if (is_enumerate) {
                emit_overload(return_type, "", target, false, false, true);
            } else if (returns_void) {
                emit_overload("void", "", target, by_ref, false, false);
            } else {
                emit_overload(return_type, "", target, false, false, true);
            }
        } else {
            if (is_enumerate) {
                file << "inline Result " << name;
                emit_enumerate_nothrow_params(cmd, cc, file, pd_idx);
                file << " noexcept { return " << name << target << "(";
                emit_pd_forward(false);
                file << ", v); }\n";
            } else {
                emit_overload("Result", "", target, true, true, true);
            }
        }
    }
}

void vkgen::Generator::Generator::generate_physical_device_overload_alias(const CommandAlias& alias, std::ofstream& file) {
    if (config.exception_behavior == ExceptionBehavior::BothWithoutDefault) {

        file << "template<typename... Args> inline auto " << NameTranslator::from_command_name(alias.name) << "_noThrow(Args&&... args) { return " << NameTranslator::from_command_name(alias.alias) << "_noThrow(detail::forward<Args>(args)...); }\n";
        file << "template<typename... Args> inline auto " << NameTranslator::from_command_name(alias.name) << "_throw(Args&&... args) { return " << NameTranslator::from_command_name(alias.alias) << "_throw(detail::forward<Args>(args)...); }\n";
    } else {
        file << "template<typename... Args> inline auto " << NameTranslator::from_command_name(alias.name) << "(Args&&... args) { return " << NameTranslator::from_command_name(alias.alias) << "(detail::forward<Args>(args)...); }\n";
    }
}

void Generator::add_required_type(sv name) {
    std::vector<Type*> required_types;
    add_required_type(name, required_types);
    assert(required_types.size() == 0);
}

void Generator::add_required_type(sv name, std::vector<Type*>& types_stack) {
    if (required_types.contains(name))
        return;

    if (!types.contains(name))
        std::cout << "FIXME: Required type not found: " << name << "\n";

    Type* type = &types.find(name)->second;
    if (std::ranges::find(types_stack, type) == types_stack.end())
        types_stack.push_back(type);
    else {
        std::cout << "FIXME: Returning from circular dependency: " << name << "\n";
        return;
    }


    // TODO: Write own split
    for (auto&& part : type->requires_ | std::views::split(',')) {
        sv req(&*part.begin(), std::ranges::distance(part));
        if (req.empty()) {
            std::cout << "FIXME: empty requires NOT HANDLED" << std::endl;
            break;
        }

        // FIXME: Circular dependency
        add_required_type(req, types_stack);


    }

    // CHECK: Im not sure about this logic. What if we have struct, that is defined and want to also be accessed by this alias?
    if (!type->alias.empty())
        add_required_type(type->alias, types_stack);
    else
        switch (type->category) {

            /// These types do not require any special handling
        case Type::Category::Basetype: // fallthrough
        case Type::Category::Bitmask:  // fallthrough
        case Type::Category::Define:   // fallthrough
            // CHECK: handle too?
        case Type::Category::Handle:   // fallthrough
            // TODO: currently generates all handles
        case Type::Category::Include:  // fallthrough
        case Type::Category::None:
            break;

        case Type::Category::Enum:
            add_required_enum(type->name);
            break;

        case Type::Category::Struct:   // fallthrough
        case Type::Category::Union:
            for (auto& member : type->struct_->members) {
                if (member.is_standalone_comment)
                    continue;

                add_required_type(member.type_param.type, types_stack);
                // TASK: 210326_02 — also track enum dependencies from array_extensions
            }
            break;

        case Type::Category::Funcpointer:
            add_required_type(type->funcptr->return_type.type, types_stack);
            for (auto& param : type->funcptr->parameters)
                add_required_type(param.type, types_stack);
            // TASK: 210326_02 — also track enum dependencies from array_extensions
            break;

        }

    types_stack.pop_back();
    required_types.add_without_check(type);
}

void vkgen::Generator::Generator::add_required_enum(sv name) {
    // FIXME: API constatnts
    static const std::unordered_set<sv> api_constants = { "VK_MAX_PHYSICAL_DEVICE_NAME_SIZE", "VK_UUID_SIZE", "VK_LUID_SIZE", "VK_MAX_EXTENSION_NAME_SIZE", "VK_MAX_DESCRIPTION_SIZE", "VK_MAX_MEMORY_TYPES", "VK_MAX_MEMORY_HEAPS", "VK_LOD_CLAMP_NONE", "VK_REMAINING_MIP_LEVELS", "VK_REMAINING_ARRAY_LAYERS", "VK_REMAINING_3D_SLICES_EXT", "VK_WHOLE_SIZE", "VK_ATTACHMENT_UNUSED", "VK_TRUE", "VK_FALSE", "VK_QUEUE_FAMILY_IGNORED", "VK_QUEUE_FAMILY_EXTERNAL", "VK_QUEUE_FAMILY_FOREIGN_EXT", "VK_SUBPASS_EXTERNAL", "VK_MAX_DEVICE_GROUP_SIZE", "VK_MAX_DRIVER_NAME_SIZE", "VK_MAX_DRIVER_INFO_SIZE", "VK_SHADER_UNUSED_KHR", "VK_MAX_GLOBAL_PRIORITY_SIZE", "VK_MAX_SHADER_MODULE_IDENTIFIER_SIZE_EXT", "VK_MAX_PIPELINE_BINARY_KEY_SIZE_KHR", "VK_MAX_VIDEO_AV1_REFERENCES_PER_FRAME_KHR", "VK_MAX_VIDEO_VP9_REFERENCES_PER_FRAME_KHR", "VK_SHADER_INDEX_UNUSED_AMDX", "VK_PARTITIONED_ACCELERATION_STRUCTURE_PARTITION_INDEX_GLOBAL_NV", "VK_MAX_PHYSICAL_DEVICE_DATA_GRAPH_OPERATION_SET_NAME_SIZE_ARM" };

    if (api_constants.contains(name))
        name = "API Constants";

    if (required_enums.contains(name) || required_types.contains(name))
        return;

    auto it = enums.find(name);
    if (it != enums.end()) {
        required_enums.add_without_check(&it->second);
        return;
    };

    auto it2 = types.find(name);
    if (it2 != types.end()) {
        required_types.add_without_check(&it2->second);
        add_required_enum(it2->second.alias);
        return;
    }

    // FIXME: New enum alias can be defined in extension require tag and then other extension can depend on it without redefining it
    // Also we're currently adding enum aliases ound in require tags into required_defines
    if (std::ranges::find(required_defines, name, &DefineExt::name) == required_defines.end())
        throw my_error{ std::format("Enum '{}' not found", name) };

}



void Generator::add_required_command(sv name) {
    // TASK: 040226_01 non-linkable commands
    static const std::unordered_set<sv> non_linkable_commands = { "vkGetRayTracingShaderGroupsKHR", "vkGetRayTracingCaptureReplayShaderGroupHandlesKHR", "vkGetRayTracingShaderGroupHandlesKHR", "vkGetPhysicalDeviceCalibrateableTimeDomainsKHR", "vkGetCalibratedTimestampsKHR", "vkReleaseSwapchainImagesKHR" };
    if (non_linkable_commands.contains(name))
        return;

    if (required_commands.contains(name) || required_commands_aliases.contains(name))
        return;

    auto it = commands.find(name);
    if (it != commands.end()) {
        Command* cmd = &(it->second);
        required_commands.add_without_check(cmd);

        // return type
        add_required_type(cmd->type);

        for (auto& param : cmd->parameters)
            add_required_type(param.type_param.type);
        // TASK: 210326_02 — also track enum dependencies from array_extensions

    } else {
        CommandAlias* ca = &command_aliases.find(name)->second;

        // TASK: 040226_01 non-linkable commands
        if (non_linkable_commands.contains(ca->alias))
            return;

        required_commands_aliases.add_without_check(ca);

        add_required_command(ca->alias);
    }
}

void Generator::add_required_version_feature(sv name, vkgen::xml::Dom& dom) {
    // TODO: some feature map is needed
    auto it = std::ranges::find_if(dom.root->asElement().children, has_tag("feature") && has_attr("name", name));
    if (it == dom.root->asElement().children.end()) {
        throw my_error{ std::format("Feature (version) '{}' not found", name) };
    }
    Element& feature = (*it)->asElement();

    // TODO: check arguments of the feature.
    // attributes:
    // api - comma separated list,
    // apitype - values: 'internal' or 'public' <- default
    // name: we wont protect the feature, because we will generate specific features
    // number: major.minor
    // depends: 😭 String containing a boolean expression of one or more API core version and extension names. The feature requires the expression in the string to be satisfied to use any functionality it defines. Supported operators include , for logical OR, ` for logical AND, and `(` `)` for grouping. `,` and ` are of equal precedence, and lower than ( ). Expressions must be evaluated left-to-right for operators of the same precedence. Terms which are core version names are true if the corresponding API version is supported. Terms which are extension names are true if the corresponding extension is enabled.
    // sortorder
    // comment

    sv depends = feature.get_attr_value("depends");

    if (!depends.empty() && (depends.contains(',') || depends.contains('+'))) {
        std::cout << "FIXME: Compound dependencies not supported yet" << std::endl;
    } else if (!depends.empty()) {
        add_required_version_feature(depends, dom);
    }

    for (Node* node : feature.children) {
        if (node->isText()) {
            std::cout << "- Skipping: " << node->asText() << std::endl;
            continue;
        }
        auto& elem = node->asElement();
        if (elem.tag == "require") {
            // FIXME: handle arguments: depends, profile, api
            for (Node* type : elem.children) {
                if (type->isText()) {
                    std::cout << "- Skipping TEXT: " << type->asText() << std::endl;
                    continue;
                }

                Element& elem = type->asElement();

                if (elem.tag == "type") {
                    add_required_type(elem.get_attr_value("name"));
                } else if (elem.tag == "enum") {
                    auto it = std::ranges::find(elem.attrs, "extends", &Attribute::name);
                    if (it != elem.attrs.end()) {
                        // Feature-block enums always have explicit extnumber in vk.xml, no block_ext_number needed
                        extend_enum(it->value, elem, dom.arena);
                        add_required_enum(it->value);
                    } else {
                        add_required_enum(elem.get_attr_value("name"));
                    }


                } else if (elem.tag == "command") {
                    add_required_command(elem.get_attr_value("name"));
                } else if (elem.tag == "feature") {
                    // Do nothing. Feature tag are only purely for documentation
                } else if (elem.tag == "comment") {
                    // CHECK: ignore standalone comments?
                    std::cout << "Skipping <comment> when parsing require.\n";
                } else {
                    throw my_error{ "Unknown tag '" + std::string(elem.tag) + "' in require tag." };
                }
            }
        } else if (elem.tag == "deprecate") {
            // TODO: handle profile and api attributes
            sv explanation = elem.get_attr_value("explanationlink");

            for (Node* type : elem.children) {
                if (type->isText()) {
                    std::cout << "- Skipping TEXT: " << type->asText() << std::endl;
                    continue;
                }

                Element& elem = type->asElement();

                if (elem.tag == "type") {
                    sv name = elem.get_attr_value("name");
                    types.find(name)->second.deprecated = explanation;
                } else if (elem.tag == "enum") {
                    // TODO: enum can have also api attribute
                    sv name = elem.get_attr_value("name");
                    enums.find(name)->second.deprecated = explanation;
                } else if (elem.tag == "command") {
                    // TODO: Currently unable to deprecate commands
                    //sv name = elem.get_attr_value("name");
                    //commands.find(name)->second.deprecated = explanation;
                } else if (elem.tag == "comment") {
                    // CHECK: ignore standalone comments?
                    std::cout << "Skipping <comment> when parsing deprecate.\n";
                } else {
                    throw my_error{ "Unknown tag '" + std::string(elem.tag) + "' in deprecate tag." };
                }
            }

        } else if (elem.tag == "remove") {
            // TODO: handle profile, api, reasonlink and comment attributes
            for (Node* type : elem.children) {
                if (type->isText()) {
                    std::cout << "- Skipping TEXT: " << type->asText() << std::endl;
                    continue;
                }

                Element& elem = type->asElement();

                if (elem.tag == "type") {
                    required_types.remove(elem.get_attr_value("name"));
                } else if (elem.tag == "enum") {
                    required_enums.remove(elem.get_attr_value("name"));
                } else if (elem.tag == "command") {
                    required_commands.remove(elem.get_attr_value("name"));
                } else if (elem.tag == "comment") {
                    // CHECK: ignore standalone comments?
                    std::cout << "Skipping <comment> when parsing remove.\n";
                } else {
                    throw my_error{ "Unknown tag '" + std::string(elem.tag) + "' in remove tag." };
                }
            }

        } else {
            std::cout << "- TODO: skipping: " << elem.tag << std::endl;
        }

    }
}

void Generator::extend_enum(sv extends, Element& elem, vkgen::Arena& arena, uint16_t block_ext_number, sv protect) {

    TypeEnum& enum_ = enums.find(extends)->second;
    auto item = TypeEnum::EnumItem::from_xml(elem, arena, &enum_, false, true, block_ext_number);

    // Propagate platform protection from the parent extension
    if (!protect.empty() && item.protect.empty())
        item.protect = protect;

    auto existing = std::ranges::find(enum_.items, item.name, &TypeEnum::EnumItem::name);
    if (existing != enum_.items.end()) {
        if (config.log_level == LogLevel::All)
            std::cout << "INFO: Duplicate enum item '" << item.name << "' when extending '" << extends << "'\n";

        // Check if the duplicate has the same value (expected for promoted extensions)
        bool same_value = false;
        if (item.is_alias && existing->is_alias)
            same_value = (item.alias == existing->alias);
        else if (!item.is_alias && !existing->is_alias) {
            if (enum_.type == TypeEnum::Type::Normal)
                same_value = (item.normal.value == existing->normal.value);
            else if (enum_.type == TypeEnum::Type::Bitmask)
                same_value = (item.bitmask.bitpos == existing->bitmask.bitpos);
        }

        if (!same_value && config.log_level == LogLevel::Warning)
            std::cout << "WARNING: conflicting duplicate enum item: '" << item.name << "' when extending '" << extends << "'\n";
    } else
        enum_.items.emplace_back(std::move(item));
}


int vk_version_cmp(sv token, VulkanVersion version) {
    VulkanVersion token_version;
    if (!parse_vulkan_version(token, token_version))
        throw my_error{ "Invalid version found in depends token: " + std::string(token) };
    return (int)token_version - (int)version;
}

// TASK: 100126_01
// TODO: Refactor - this function currently iterates all extensions and adds all of them.
//       It should accept a parsed extension structure and add requirements for a single extension.
void Generator::add_extension_with_deps(Element& ext, xml::Dom& dom, sv required_by) {
    // Already processed? Skip (also prevents circular deps)
    if (processed_extensions.contains(&ext))
        return;
    processed_extensions.insert(&ext);

    sv ext_name = ext.get_attr_value("name");

    // Skip beta extensions if configured
    if (config.beta_extensions == BetaExtensions::DontGenerate && ext.get_attr_value("provisional") == "true")
        return;

    if (config.extension_filter_mode == ExtensionFilterMode::BlackList && config.extension_names.contains(std::string(ext_name))) {
        std::cout << "Warning: Generating blacklisted extension '" << ext_name << "' because it is required by '" << required_by << "'.\n";
    }

    sv depends = ext.get_attr_value("depends");
    if (!depends.empty()) {
        // Parse depends expression: split on + (AND) and , (OR), strip parens
        // TODO: right now we treat all tokens as required
        std::string deps_str(depends);
        for (char& c : deps_str) {
            if (c == '+' || c == ',' || c == '(' || c == ')')
                c = ' ';
        }

        std::string_view remaining(deps_str);
        while (!remaining.empty()) {
            auto start = remaining.find_first_not_of(' ');
            if (start == std::string_view::npos) break;
            remaining.remove_prefix(start);

            auto end = remaining.find(' ');
            auto token = remaining.substr(0, end);
            remaining = (end == std::string_view::npos) ? "" : remaining.substr(end);

            if (token.starts_with("VK_VERSION_")) {
                if (vk_version_cmp(token, config.target_version) < 0) {
                    throw my_error{ std::stringstream() << "Extension '" << ext_name << "' requires newer Vulkan version '" << token << "' than was provided '"
                        << vulkan_version_to_string(config.target_version) << "'." };
                }
                // (already handled by add_required_version_feature call in generate())
            } else {
                auto it = extension_name_to_element.find(token);
                if (it != extension_name_to_element.end()) {
                    Element& dep = *(it->second);
                    if (!processed_extensions.contains(&dep)) {
                        add_extension_with_deps(dep, dom, ext_name);
                    }
                } else {
                    throw my_error{ std::stringstream() << "Dependency '" << token << "' of extension " << ext_name << " not found in vk.xml\n" };
                }
            }
        }
    }

    add_extension_prototype(ext, dom);
}

void Generator::add_extension_prototype(Element& ext, xml::Dom& dom) {
    sv supported = ext.get_attr_value("supported");
    if (supported == "disabled" || !std::ranges::contains(std::views::split(supported, ','), "vulkan", [](auto&& rng) { return sv(rng); }))
        return;

    // TODO: refactor - resolve platform protect from a parsed Extension class instead of raw XML
    // Resolve platform protect macro
    sv ext_platform = ext.get_attr_value("platform");
    sv ext_protect;
    if (!ext_platform.empty()) {
        auto it = platform_to_protect.find(ext_platform);
        if (it == platform_to_protect.end())
            throw my_error{ "Unknown platform: " + std::string(ext_platform) };
        ext_protect = it->second;
    }

    // Skip beta/provisional extensions if configured
    if (config.beta_extensions == BetaExtensions::DontGenerate && ext.get_attr_value("provisional") == "true")
        return;

    uint16_t ext_number = std::stoul(std::string(ext.get_attr_value("number")));

    for (Node* node : ext.children) {
        if (node->isText()) {
            std::cout << "- Skipping: " << node->asText() << std::endl;
            continue;
        }

        auto& elem = node->asElement();
        if (elem.tag == "require") {
            sv protect = ext_protect;

            for (Node* type : elem.children) {
                if (type->isText()) {
                    std::cout << "- Skipping TEXT: " << type->asText() << std::endl;
                    continue;
                }

                Element& elem = type->asElement();

                if (elem.tag == "type") {
                    sv type_name = elem.get_attr_value("name");
                    add_required_type(type_name);
                    if (!protect.empty()) {
                        // Set protect on the type itself
                        auto type_it = types.find(type_name);
                        if (type_it != types.end() && type_it->second.protect.empty())
                            type_it->second.protect = protect;
                        // If this type is an enum, propagate protect to it too
                        auto enum_it = enums.find(type_name);
                        if (enum_it != enums.end() && enum_it->second.protect.empty())
                            enum_it->second.protect = protect;
                    }
                } else if (elem.tag == "enum") {
                    auto it = std::ranges::find(elem.attrs, "extends", &Attribute::name);
                    if (it != elem.attrs.end()) {
                        extend_enum(it->value, elem, dom.arena, ext_number, protect);
                        add_required_enum(it->value);
                        continue;
                    }

                    sv name = elem.get_attr_value("name");
                    it = std::ranges::find(elem.attrs, "value", &Attribute::name);
                    // TODO: generate_extension_defined_macro not handled
                    // TODO: This should be handled by adding it as name and version into the extension it self
                    bool skip_extention_macro =
                        (!config.generate_extension_name_macro && name.ends_with("_EXTENSION_NAME")) ||
                        (!config.generate_extension_version_macro && name.ends_with("_SPEC_VERSION"));
                    if (it != elem.attrs.end()) {
                        if (!skip_extention_macro)
                            required_defines.push_back({ name, it->value, elem });
                        continue;
                    }

                    it = std::ranges::find(elem.attrs, "alias", &Attribute::name);
                    if (it != elem.attrs.end()) {
                        // FIXME: should be enum alias? Check if it is referencing contexpr value
                        if (!skip_extention_macro) {
                            sv value = dom.arena.storeString(NameTranslator::from_constexpr_value(it->value).new_name);
                            required_defines.push_back({ name, value, elem });
                        }
                        continue;
                    }

                    if (!protect.empty()) {
                        auto enum_it = enums.find(name);
                        if (enum_it != enums.end() && enum_it->second.protect.empty())
                            enum_it->second.protect = protect;
                    }
                    add_required_enum(name);

                } else if (elem.tag == "command") {
                    sv cmd_name = elem.get_attr_value("name");
                    add_required_command(cmd_name);
                    if (!protect.empty()) {
                        auto cmd_it = commands.find(cmd_name);
                        if (cmd_it != commands.end() && cmd_it->second.protect.empty())
                            cmd_it->second.protect = protect;
                    }

                } else if (elem.tag == "feature") {
                    // Do nothing. Feature tag are only purely for documentation
                } else if (elem.tag == "comment") {
                    // CHECK: standalone comment?
                    std::cout << "Skipping <comment> when parsing require.\n";
                } else {
                    throw my_error{ "Unknown tag: " + std::string(elem.tag) };
                }
            }
        }
    }
}


void Generator::generate(xml::Dom& dom, std::ofstream& header, std::ofstream& source, Config& config) {
    // TASK: 250226_01 Handle config in generator properly
    this->config = config;
    Deprecate::enabled = config.deprecation_behavior == DeprecationBehavior::GenerateWithDeprecationWarning;
    LineComment::enabled = config.generate_comments;
    StandaloneComment::enabled = config.generate_comments;
    NameTranslator::keep_av1_vp9 = config.apply_av1_and_vp9_naming_exceptions;
    ProtectGuard::FilterBetaExtensions = config.beta_extensions == BetaExtensions::GenerateWithoutProtectMacro;

    // ==================== Parsing ====================
    parse_platforms(dom);
    parse_types(dom);
    parse_enums(dom);
    parse_commands(dom);

    add_required_version_feature(vulkan_version_to_string(config.target_version), dom);

    // Build extension name: TASK: 110426_01 Extensions should be parsed same as types, enums etc.
    auto& registry = dom.root->asElement();
    for (Node* ext_group : registry.children | std::views::filter(has_tag("extensions"))) {
        for (Node* ext : ext_group->asElement().children) {
            if (ext->isText() || ext->asElement().tag != "extension")
                continue;
            sv name = ext->asElement().get_attr_value("name");
            if (!name.empty())
                extension_name_to_element[name] = &ext->asElement();
        }
    }

    if (config.extension_filter_mode == ExtensionFilterMode::WhiteList) {
        for (const auto& ext_name : config.extension_names) {
            auto it = extension_name_to_element.find(ext_name);
            if (it == extension_name_to_element.end())
                throw my_error{ "Whitelisted extension not found in vk.xml: " + ext_name };
            add_extension_with_deps(*(it->second), dom);
        }
    } else {
        // BlackList: iterate in XML document order (ordering matters for enum dependencies)
        for (Node* ext_group : registry.children | std::views::filter(has_tag("extensions"))) {
            for (Node* ext : ext_group->asElement().children) {
                if (ext->isText() || ext->asElement().tag != "extension")
                    continue;
                sv name = ext->asElement().get_attr_value("name");
                if (config.extension_names.empty() || !config.extension_names.contains(std::string(name)))
                    add_extension_prototype(ext->asElement(), dom);
            }
        }
    }

    if (config.generate_flags_class) {
        required_types.remove("VkFlags");
        required_types.remove("VkFlags64");
    }

    // ==================== Header generation ====================
    header << "#pragma once\n";
    header << "#include <cstddef>\n";  // std::nullptr_t used in Handle class
    header << "#include <cstdint>\n";  // uint32_t, uint64_t ... used everywhere
    header << "#include <new>\n";      // needed for custom vector class
    header << "#include <stdlib.h>\n"; // needed for custom Error class - malloc/free
    header << "#include <string.h>\n"; // needed for custom Error class - strncpy/strlen
    header << boilerplate::VIDEO_INCLUDES << "\n";

    header << "#define VKAPI_PTR\n";
    header << "#define VKAPI_CALL\n";
    header << "#define VKAPI_ATTR\n";
    if (!config.namespace_name.empty())
        header << "\nnamespace " << config.namespace_name << " {\n\n";

    // --- Forward declarations ---
    header << "struct ExtensionProperties;\n";
    header << "struct InstanceCreateInfo;\n";
    header << "struct DeviceCreateInfo;\n";
    header << "enum class Result;\n\n";

    // --- Handle<T> template ---
    if (config.generate_handle_class)
        boilerplate::print(header, boilerplate::HANDLE_DEFINITION) << "\n";
    else
        header << "#define VK_DEFINE_HANDLE(object) typedef struct object##_T* object;\n";

    ProtectGuard header_guard{ .file = header };

    // --- Build HandleInfo for all concrete handles ---
    std::vector<HandleInfo> handles;
    if (config.generate_handle_class)
        cache_handles(handles);

    // --- Handle type aliases ---
    auto& obj_enum = enums.at(TypeHandle::obj_enum_name);
    for (HandleInfo& h : handles) {
        header_guard.transition(h.type.protect);
        generate_handle(h.type, header, obj_enum);
    }
    header_guard.close();
    header << "\n";

    // --- UniqueHandle<T> template + aliases (only for destroyable handles) ---
    header << boilerplate::UNIQUE_HANDLE_DEFINITION;
    for (auto& h : handles) {
        if (!h.is_destroyable()) continue;
        header_guard.transition(h.type.protect);
        auto name = NameTranslator::from_type_name(h.type.name);
        header << "using Unique" << name << " = UniqueHandle<" << name << ">;\n";
    }
    header_guard.close();
    header << "\n";


    // --- detail namespace, accessors, init declarations ---
    header << boilerplate::DETAIL_NAMESPACE;
    header << boilerplate::INIT_DECLARATIONS;
    header << "void initInstancePFNs() noexcept;\n"
        << "void initDevicePFNs() noexcept;\n\n";
    header << boilerplate::VERSION_HELPERS;
    header << boilerplate::CUSTOM_VECTOR_DECL << "\n";
    header << boilerplate::CUSTOM_VECTOR_IMPL << "\n";
    header << boilerplate::CUSTOM_UTILS_DECL << "\n";

    boilerplate::print(header, boilerplate::CUSTOM_SPAN_DEFINITION) << "\n";

    // --- Flags<BitType> template ---
    if (config.generate_flags_class)
        boilerplate::print(header, boilerplate::FLAGS_DEFINITION) << "\n";

    // --- Enums ---
    for (TypeEnum* enum_ : required_enums.get()) {
        if (enum_ == nullptr) continue;
        header_guard.transition(enum_->protect);
        generate_enum(*enum_, header);
    }
    header_guard.close();

    // --- to_cstr (declarations or inline definitions) ---
    if (config.to_cstr_behavior != ToCstrFunction::None) {
        header << "// conversions\n";
        for (TypeEnum* enum_ : required_enums.get()) {
            if (enum_ == nullptr) continue;
            if (enum_->type != TypeEnum::Type::Normal) continue;
            header_guard.transition(enum_->protect);
            if (config.to_cstr_behavior == ToCstrFunction::InCpp) {
                generate_to_cstr_decl(*enum_, header);
            } else {
                header << "inline ";
                generate_to_cstr_def(*enum_, header);
            }
        }
        header_guard.close();
        header << '\n';
    }

    // TASK: 100126_01
    for (auto& define : required_defines) {
        header << "#define " << define.name << " " << define.value << "\n";
    }

    // --- Types (structs, unions, bitmasks, funcpointers, basetypes) ---
    for (Type* p : required_types.get()) {
        if (p == nullptr) continue;

        auto& type = *p;
        if (type.category == Type::Category::Handle) continue; // already emitted above
        header_guard.transition(type.protect);

        switch (type.category) {
        case Type::Category::Struct:
            generate_struct(type, header);
            break;
        case Type::Category::Union:
            generate_union(type, header);
            break;
        case Type::Category::Enum:
            generate_enum_alias(type, header);
            break;
        case Type::Category::Bitmask:
            generate_bitmask(type, header);
            break;
        case Type::Category::Basetype:
            _gen_arbitrary_C_code_in_type(type.elem, header);
            break;
        case Type::Category::Funcpointer:
            generate_funcpointer(type, header);
            break;
        default:
            break;
        }
    }
    header_guard.close();

    // This convienience function needs types
    header << boilerplate::IS_EXTENSION_SUPPORTED_DEF << "\n";

    // --- PFN typedefs ---
    for (Command* cmd : required_commands.get()) {
        if (cmd == nullptr) continue;
        header_guard.transition(cmd->protect);
        generate_command_PFN(*cmd, header);
    }
    header_guard.close();

    // --- Funcs struct ---
    header << "\nstruct Funcs {\n";
    for (Command* cmd : required_commands.get()) {
        if (cmd == nullptr) continue;
        header_guard.transition(cmd->protect);
        header << "    PFN_" << cmd->name << " " << cmd->name << " = nullptr;\n";
    }
    header_guard.close();
    header << "};\n";
    header << "namespace detail { extern Funcs _funcs; }\n";
    header << "inline Funcs& funcs = detail::_funcs;\n\n";

    header << "namespace detail { extern const AllocationCallbacks* _allocator; }\n";
    header << "inline const AllocationCallbacks* allocator() noexcept { return detail::_allocator; }\n";
    header << "void setAllocator(const AllocationCallbacks* a) noexcept;\n\n";

    // --- Error classes ---
    if (config.exception_behavior != ExceptionBehavior::NoThrowOnly)
        generate_error_classes(header);

    // --- Proc addr helpers (after Funcs is defined) ---
    header << boilerplate::PROC_ADDR_HELPERS;

    // --- PFN aliases ---
    if (config.generate_command_aliases) {
        for (CommandAlias* ca : required_commands_aliases.get()) {
            if (ca == nullptr) continue;
            header << "using " << "PFN_" << ca->name << " = PFN_" << ca->alias << ";\n";
        }
    }

    // --- destroy() overloads for UniqueHandle ---
    // Must come before command wrappers because processResult calls destroy()
    if (config.generate_handle_class) {
        header << "\n";
        for (const auto& h : handles) {
            if (!h.is_destroyable()) continue;
            auto name = NameTranslator::from_type_name(h.type.name);
            header_guard.transition(h.type.protect);

            bool has_allocator = (h.destroy_behavior == HandleInfo::DestroyBehavior::Destroy
                || h.destroy_behavior == HandleInfo::DestroyBehavior::Free);
            sv alloc_arg = has_allocator ? ", detail::_allocator" : "";
            switch (h.kind) {
            case HandleInfo::Kind::InstanceItself:
                header << "inline void destroy(Instance i) noexcept { funcs."
                    << h.destroy_command->name << "(i.handle()" << alloc_arg << "); }\n";
                break;
            case HandleInfo::Kind::DeviceItself:
                header << "inline void destroy(Device d) noexcept { funcs."
                    << h.destroy_command->name << "(d.handle()" << alloc_arg << "); }\n";
                break;
            case HandleInfo::Kind::Instance:
                header << "inline void destroy(" << name << " h) noexcept { funcs."
                    << h.destroy_command->name << "(detail::_instance.handle(), h.handle()"
                    << alloc_arg << "); }\n";
                break;
            case HandleInfo::Kind::Device:
                header << "inline void destroy(" << name << " h) noexcept { funcs."
                    << h.destroy_command->name << "(detail::_device.handle(), h.handle()"
                    << alloc_arg << "); }\n";
                break;
            }

        }
        header_guard.close();
    }

    if (config.exception_behavior != ExceptionBehavior::NoThrowOnly)
        header << boilerplate::PROCESS_RESULT_TEMPLATE;

    // Collect destroy/free/release commands that already have destroy() overloads
    std::unordered_set<sv> destroy_commands;
    for (const auto& h : handles) {
        if (h.is_destroyable() && h.destroy_command)
            destroy_commands.insert(h.destroy_command->name);
    }

    // --- Command wrappers ---
    for (Command* cmd : required_commands.get()) {
        if (cmd == nullptr) continue;
        // Skip commands that already have destroy() overloads
        if (destroy_commands.contains(cmd->name)) continue;
        header_guard.transition(cmd->protect);

        auto cc = classify_command(*cmd);
        // For ResultCreate: check if output handle has a destroy() overload
        if ((cc.pattern == CommandClassification::Pattern::ResultCreate
            || cc.pattern == CommandClassification::Pattern::ResultCreateArray)
            && cc.output_param_idx >= 0) {
            // Check if any destroy command targets this handle type
            // by looking at the handles vector
            sv out_type = cmd->parameters[cc.output_param_idx].type_param.type;
            for (const auto& h : handles) {
                if (h.type.name == out_type && h.is_destroyable()) {
                    cc.output_has_destroy = true;
                    break;
                }
            }
        }
        switch (cc.pattern) {
        case CommandClassification::Pattern::Void:
            generate_wrapper_void(*cmd, cc, header);
            break;
        case CommandClassification::Pattern::ResultVoid:
            generate_wrapper_result_void(*cmd, cc, header);
            break;
        case CommandClassification::Pattern::ResultCreate:
            generate_wrapper_result_create(*cmd, cc, header);
            if (cc.output_has_destroy)
                generate_unique_handle_create(*cmd, cc, header);
            break;
        case CommandClassification::Pattern::ResultCreateArray:
            generate_wrapper_result_create_array(*cmd, cc, header);
            break;
        case CommandClassification::Pattern::VoidOutParam:
            generate_wrapper_void_outparam(*cmd, cc, header);
            break;
        case CommandClassification::Pattern::ResultOutParam:
            generate_wrapper_result_outparam(*cmd, cc, header);
            break;
        case CommandClassification::Pattern::Enumerate:
            generate_wrapper_enumerate_header(*cmd, cc, header);
            break;
        case CommandClassification::Pattern::Other:
            generate_command_wrapper(*cmd, header);
            break;
        }

        // PhysicalDevice convenience overloads
        if (!cmd->parameters.empty() && cmd->parameters[0].type_param.type == "VkPhysicalDevice"
            && cc.pattern != CommandClassification::Pattern::Other) {
            generate_physical_device_overloads(*cmd, cc, header);
        }
    }
    header_guard.close();

    // --- Command alias wrappers ---
    if (config.generate_command_aliases) {
        for (CommandAlias* ca : required_commands_aliases.get()) {
            if (ca == nullptr) continue;
            // Skip aliases that point to destroy commands (already have destroy() overloads)
            if (destroy_commands.contains(ca->alias)) continue;

            std::string alias_base = NameTranslator::from_command_name(ca->name).new_name;
            std::string target_base = NameTranslator::from_command_name(ca->alias).new_name;

            auto emit_alias = [&](sv a_base, sv t_base, sv a_suffix, sv t_suffix) {
                header << "inline constexpr auto& " << a_base << a_suffix
                    << " = " << t_base << t_suffix << ";\n";
                };

            // Mirrors the gating in generate_wrapper_* emitters so every generated
            // target symbol gets a matching alias and none reference missing ones.
            auto emit_throw_nothrow_default = [&](sv a_base, sv t_base, sv a_pfx, sv t_pfx) {
                if (should_emit_throw())
                    emit_alias(a_base, t_base,
                        std::string(a_pfx) + (should_emit_default() ? "_throw" : ""),
                        std::string(t_pfx) + (should_emit_default() ? "_throw" : ""));
                if (should_emit_nothrow())
                    emit_alias(a_base, t_base,
                        std::string(a_pfx) + (should_emit_default() ? "_noThrow" : ""),
                        std::string(t_pfx) + (should_emit_default() ? "_noThrow" : ""));
                if (should_emit_default())
                    emit_alias(a_base, t_base, a_pfx, t_pfx);
                };

            // Fall back to shorthand-only alias when target command isn't in the map.
            auto it = commands.find(ca->alias);
            if (it == commands.end()) {
                emit_alias(alias_base, target_base, "", "");
                continue;
            }
            auto& target_cmd = it->second;

            // Cant generate aliases where target is overloaded (PhysicalDevice overloads make constexpr auto& ambiguous)
            if (!target_cmd.parameters.empty() && target_cmd.parameters[0].type_param.type == "VkPhysicalDevice") {
                generate_physical_device_overload_alias(*ca, header);
                continue;
            }

            CommandClassification cc = classify_command(target_cmd);
            if ((cc.pattern == CommandClassification::Pattern::ResultCreate
                || cc.pattern == CommandClassification::Pattern::ResultCreateArray)
                && cc.output_param_idx >= 0) {
                sv out_type = target_cmd.parameters[cc.output_param_idx].type_param.type;
                for (const auto& h : handles) {
                    if (h.type.name == out_type && h.is_destroyable()) {
                        cc.output_has_destroy = true;
                        break;
                    }
                }
            }

            using Pattern = CommandClassification::Pattern;
            switch (cc.pattern) {
            case Pattern::Void:
            case Pattern::VoidOutParam:
            case Pattern::Other:
                emit_alias(alias_base, target_base, "", "");
                break;
            case Pattern::ResultVoid:
            case Pattern::ResultOutParam:
            case Pattern::Enumerate:
                emit_throw_nothrow_default(alias_base, target_base, "", "");
                break;
            case Pattern::ResultCreate:
                emit_throw_nothrow_default(alias_base, target_base, "", "");
                if (cc.output_has_destroy)
                    emit_throw_nothrow_default(alias_base, target_base, "Unique", "Unique");
                break;
            case Pattern::ResultCreateArray: {
                    auto emit_both_forms = [&](sv a, sv t) {
                        emit_throw_nothrow_default(a, t, "", "");
                        emit_throw_nothrow_default(a, t, "Unique", "Unique");
                        };
                    emit_both_forms(alias_base, target_base);
                    // Singular convenience overloads: createGraphicsPipelines -> createGraphicsPipeline
                    if (cc.input_array_count > 0) {
                        std::string singular_alias = NameTranslator::singularize(alias_base);
                        std::string singular_target = NameTranslator::singularize(target_base);
                        if (singular_alias != alias_base && singular_target != target_base)
                            emit_both_forms(singular_alias, singular_target);
                    }
                    break;
                }
            }
        }
    }

    if (!config.namespace_name.empty())
        header << "\n} // namespace " << config.namespace_name << "\n";

    source << "#include \"" << config.header_path << "\"\n";

    if (!config.namespace_name.empty())
        source << "namespace " << config.namespace_name << " {\n\n";
    source << boilerplate::CPP_IMPL;
    generate_throw_result_exception(source);

    source << boilerplate::LOAD_UNLOAD_LIB_IMPL << "\n";

    // Global commands loaded in loadLib — skip in PFN loading
    static const std::unordered_set<std::string_view> global_commands = {
        "vkGetInstanceProcAddr",
        "vkCreateInstance",
        "vkEnumerateInstanceExtensionProperties",
        "vkEnumerateInstanceLayerProperties",
    };

    ProtectGuard source_guard{ .file = source };

    // --- initInstancePFNs: instance-level commands via vkGetInstanceProcAddr ---
    source << "void initInstancePFNs() noexcept {\n";
    // vkGetDeviceProcAddr must be loaded first (needed by initDevicePFNs)
    source << "    funcs.vkGetDeviceProcAddr = (PFN_vkGetDeviceProcAddr)funcs.vkGetInstanceProcAddr(detail::_instance.handle(), \"vkGetDeviceProcAddr\");\n";
    for (Command* cmd : required_commands.get()) {
        if (cmd == nullptr || global_commands.contains(cmd->name)) continue;
        if (cmd->name == "vkGetDeviceProcAddr") continue; // already loaded above
        if (is_device_level_command(*cmd)) continue;
        source_guard.transition(cmd->protect);
        source << "    funcs." << cmd->name << " = (PFN_" << cmd->name
            << ")funcs.vkGetInstanceProcAddr(detail::_instance.handle(), \"" << cmd->name << "\");\n";
    }
    source_guard.close();
    source << "}\n\n";

    // --- initDevicePFNs: device-level commands via vkGetDeviceProcAddr ---
    source << "void initDevicePFNs() noexcept {\n";
    for (Command* cmd : required_commands.get()) {
        if (cmd == nullptr) continue;
        if (!is_device_level_command(*cmd)) continue;
        if (cmd->name == "vkGetDeviceProcAddr") continue; // already loaded in initInstancePFNs
        source_guard.transition(cmd->protect);
        source << "    funcs." << cmd->name << " = (PFN_" << cmd->name
            << ")funcs.vkGetDeviceProcAddr(detail::_device.handle(), \"" << cmd->name << "\");\n";
    }
    source_guard.close();
    source << "}\n\n";

    source << boilerplate::INIT_INSTANCE_DEVICE_IMPL;

    source << "void setAllocator(const AllocationCallbacks* a) noexcept {\n"
        "    assert(!detail::_instance && \"vk::setAllocator() must be called before vk::initInstance().\");\n"
        "    detail::_allocator = a;\n"
        "}\n\n";

    // --- to_cstr definitions (.cpp) ---
    if (config.to_cstr_behavior == ToCstrFunction::InCpp) {
        for (TypeEnum* enum_ : required_enums.get()) {
            if (enum_ == nullptr) continue;
            if (enum_->type != TypeEnum::Type::Normal) continue;
            source_guard.transition(enum_->protect);
            generate_to_cstr_def(*enum_, source);
        }
        source_guard.close();
    }

    // --- Enumerate command implementations (.cpp) ---
    for (Command* cmd : required_commands.get()) {
        if (cmd == nullptr) continue;
        if (destroy_commands.contains(cmd->name)) continue;
        auto cc = classify_command(*cmd);
        if (cc.pattern != CommandClassification::Pattern::Enumerate) continue;
        source_guard.transition(cmd->protect);
        generate_wrapper_enumerate_source(*cmd, cc, source);
    }
    source_guard.close();

    if (!config.namespace_name.empty())
        source << "\n} // namespace " << config.namespace_name << "\n";
};

CommandParameter CommandParameter::from_xml(const xml::Element& elem, vkgen::Arena& arena) {
    assert(elem.tag == "param");

    CommandParameter param;

    for (const Attribute& attr : elem.attrs) {
        if (attr.name == "api")
            param.api = ApiType::from_string(attr.value);
        else if (attr.name == "len")
            param.len = attr.value;
        else if (attr.name == "altlen")
            param.altlen = attr.value;
        else if (attr.name == "stride")
            param.stride = attr.value;
        else if (attr.name == "optional")
            param.optional = attr.value;
        else if (attr.name == "selector")
            param.selector = attr.value;
        else if (attr.name == "noautovalidity")
            param.noautovalidity = attr.value;
        else if (attr.name == "externsync")
            param.externsync = attr.value;
        else if (attr.name == "objecttype")
            param.objecttype = attr.value;
        else if (attr.name == "validstructs")
            param.validstructs = attr.value;
        else
            throw my_error{ "Unknown attribute for command parameter: " + std::string(attr.name) };
    };

    if (elem.children.empty())
        throw my_error{ "Command parameter must have a type and name specified!" };

    param.type_param = TypeParam::from_xml(elem, arena);

    if (param.type_param.name.empty())
        throw my_error{ "Command parameter must have a name!" };

    // CHECK: in vulkan spec it's said to be optional
    if (param.type_param.type.empty())
        throw my_error{ "Command parameter must have a type!" };
    return param;
}

Command Command::from_xml(const xml::Element& elem, vkgen::Arena& arena) {
    Command cmd;

    for (const Attribute& attr : elem.attrs) {
        if (attr.name == "tasks")
            cmd.tasks = attr.value;
        else if (attr.name == "queues")
            cmd.queues = attr.value;
        else if (attr.name == "successcodes")
            cmd.success_codes = attr.value;
        else if (attr.name == "errorcodes")
            cmd.error_codes = attr.value;
        else if (attr.name == "renderpass")
            cmd.render_pass = scope_from_string(attr.value);
        else if (attr.name == "videocoding")
            cmd.video_encoding = scope_from_string(attr.value);
        else if (attr.name == "cmdbufferlevel")
            cmd.cmd_buffer_level = attr.value;
        else if (attr.name == "conditionalrendering")
            cmd.conditional_rendering = attr.value;
        else if (attr.name == "allownoqueues")
            cmd.allow_no_queues = bool_from_string(attr.value); // TODO: no ',' allowed
        else if (attr.name == "export")
            cmd.export_ = attr.value;
        else if (attr.name == "api")
            cmd.api = ApiType::from_string(attr.value);
        else if (attr.name == "comment")
            cmd.comment = attr.value;
        else if (attr.name == "alias" || attr.name == "name")
            throw my_error{ "Command attribute '" + std::string(attr.name) + "' is not supported" };
        else {
            std::cerr << elem << std::endl;
            throw my_error{ "Unknown attribute for command: " + std::string(attr.name) };

        }
    };

    if (elem.children.size() == 0)
        throw my_error{ "Command must have at least <proto> element!" };

    Node* proto = elem.children[0];
    if (proto->isText())
        throw my_error{ "Command must start with <proto> element!" };

    if (proto->asElement().tag != "proto")
        throw my_error{ "Command must start with <proto> element!" };

    for (Node* child : proto->asElement().children) {
        if (child->isText()) {

            cmd.declaration += child->asText();
            continue;
        }
        auto& ch = child->asElement();
        if (ch.tag == "name") {
            if (!cmd.name.empty())
                throw my_error{ "Command can only have one name!" };
            cmd.name = ch.children[0]->asText();
            cmd.declaration += cmd.name;
        } else if (ch.tag == "type") {
            if (cmd.type.empty()) {
                cmd.type = ch.children[0]->asText();
            } else {
                // TASK: 090126_08 - THIS CAN HAPPEN
                std::cout << "TODO: Duplicate type in member: " << std::string(cmd.name) << std::endl;
            }
            cmd.declaration += ch.children[0]->asText();
        } else
            throw my_error{ "Unknown child element for command: " + std::string(child->asElement().tag) };
    }

    if (cmd.name.empty())
        throw my_error{ "Command must have a name!" };

    // CHECK: in vulkan spec it is said to be optional
    if (cmd.type.empty())
        throw my_error{ "Command must have a type!" };

    for (Node* child : elem.children | std::views::drop(1)) {
        if (child->isText()) {
            std::cout << "- TASK: 090126_04 - Skipping TEXT: " << child->asText() << std::endl;
            continue;
        }
        auto& ch = child->asElement();

        if (ch.tag == "param")
            cmd.parameters.emplace_back(CommandParameter::from_xml(child->asElement(), arena));
        else if (ch.tag == "implicitexternsyncparams")
            cmd.implicit_extern_sync_params = &child->asElement();
        else if (ch.tag == "description")
            cmd.description = ch.children[0]->asText();
        else
            throw my_error{ "Unknown child element for command: " + std::string(child->asElement().tag) };

    }

    return cmd;
}

CommandAlias vkgen::Generator::CommandAlias::from_xml(const xml::Element& elem, vkgen::Arena& arena) {
    UNUSED(arena);
    CommandAlias ca;

    for (const Attribute& attr : elem.attrs) {
        if (attr.name == "name")
            ca.name = attr.value;
        else if (attr.name == "alias")
            ca.alias = attr.value;
        else if (attr.name == "comment")
            ca.comment = attr.value;
        else if (attr.name == "api")
            ca.api = ApiType::from_string(attr.value);
        else {
            std::cerr << elem << std::endl;
            throw my_error{ "Unknown attribute for command: " + std::string(attr.name) };
        }
    };

    if (elem.children.size() != 0)
        throw my_error{ "Command alias must have no children!" };

    if (ca.name.empty())
        throw my_error{ "Command alias must have a name!" };

    assert(!ca.alias.empty());

    return ca;
}


vkgen::Generator::NameTranslator::TransformedEnumName vkgen::Generator::NameTranslator::transform_enum_name(sv name, bool is_bitmask) {
    assert(!name.empty());
    assert(name.starts_with("Vk"));

    Extension ext = get_and_remove_extension_name(name);

    int digit = -1;
    if (is_bitmask && std::isdigit(name.back())) {
        digit = name.back() - '0';
        name.remove_suffix(1);
    }

    assert(!is_bitmask || name.ends_with("FlagBits"));
    if (is_bitmask)
        name.remove_suffix(sizeof("FlagBits") - 1);

    std::string transformed;
    bool first = true;
    for (char c : name) {
        if (!first && (std::isupper(c)))
            transformed.push_back('_');
        transformed.push_back(std::toupper(c));
        first = false;
    }
    if (digit != -1) {
        transformed.push_back('_');
        transformed.push_back(digit + '0');
    }

    size_t idx = transformed.find("_A_V1_");
    if (idx != std::string::npos) { // TODO: how to convert StrABCStr? it should propably be STR_ABC_STR, but it breakes the rules
        transformed.replace(idx, sizeof("_A_V1_") - 1, "_AV1_");
    }

    return { std::move(transformed), ext };
}

NameTranslator vkgen::Generator::NameTranslator::from_enum_value(sv value, const TransformedEnumName& enum_, bool is_bitfield) {
    assert(!enum_.name.empty());
    assert(!value.empty());

    // These values do not obey the api rules
    const static std::unordered_map<sv, sv> exception_values = {
        {"VK_COLORSPACE_SRGB_NONLINEAR_KHR", "eSrgbNonlinearKHR2"}, // COLORSPACE should be COLOR_SPACE, and it also shadows the correct value, so we should not generate it at all
        {"VK_STENCIL_FRONT_AND_BACK", "eFrontAndBack2"}, // missing "_FACE_" and it also shadows the correct value, so whe should not generate it at all
        {"VK_QUERY_SCOPE_COMMAND_BUFFER_KHR", "queryScopeCommandBuffer"}, // aliased value for "VkPerformanceCounterScopeKHR" enum
        {"VK_QUERY_SCOPE_RENDER_PASS_KHR", "queryScopeRenderPass"}, // aliased value for "VkPerformanceCounterScopeKHR" enum
        {"VK_QUERY_SCOPE_COMMAND_KHR", "queryScopeCommand"}, // aliased value for "VkPerformanceCounterScopeKHR" enum
        {"VK_CLUSTER_ACCELERATION_STRUCTURE_INDEX_FORMAT_8BIT_NV", "e8BitNV"},  // its missing "_BIT" but it also propably should contain it anyway
        {"VK_CLUSTER_ACCELERATION_STRUCTURE_INDEX_FORMAT_16BIT_NV", "e16BitNV"}, // its missing "_BIT" but it also propably should contain it anyway
        {"VK_CLUSTER_ACCELERATION_STRUCTURE_INDEX_FORMAT_32BIT_NV", "e32BitNV"}, // its missing "_BIT" but it also propably should contain it anyway
        {"VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES2_EXT", "eSurfaceCapabilities2EXT2"}, // 2 aliased values will be translated as the same value, so we should not generate the other one
        {"VK_PIPELINE_RASTERIZATION_STATE_CREATE_FRAGMENT_DENSITY_MAP_ATTACHMENT_BIT_EXT", "eCreateFragmentDensityMapAttachmenEXT"}, // aliased value for "VK_PIPELINE_CREATE_RENDERING_FRAGMENT_DENSITY_MAP_ATTACHMENT_BIT_EXT", but it is named differently and does not start with the class name
        {"VK_PIPELINE_RASTERIZATION_STATE_CREATE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR", "eCreateFragmentShadingRateAttachmenKHR"}, // aliased value for "VK_PIPELINE_CREATE_RENDERING_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR", but it is named differently and does not start with the class name
    };

    // These values are aliased bitfields, that are missing "_BIT" and also they are shadowning the correct value, we should propably not generate them.
    const static std::unordered_set<sv> missing_bit_with_shadowed_values = {
        "VK_PIPELINE_CREATE_DISPATCH_BASE",
        "VK_HOST_IMAGE_COPY_MEMCPY",
        "VK_SURFACE_COUNTER_VBLANK",
        "VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_DATA_ACCESS",
        "VK_PERFORMANCE_COUNTER_DESCRIPTION_PERFORMANCE_IMPACTING",
        "VK_PERFORMANCE_COUNTER_DESCRIPTION_CONCURRENTLY_IMPACTED",
        "VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_OPACITY_MICROMAP_UPDATE",
        "VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_DISABLE_OPACITY_MICROMAPS",
        "VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_OPACITY_MICROMAP_DATA_UPDATE",
        "VK_GEOMETRY_INSTANCE_FORCE_OPACITY_MICROMAP_2_STATE",
        "VK_GEOMETRY_INSTANCE_DISABLE_OPACITY_MICROMAPS",
        "VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_DISPLACEMENT_MICROMAP_UPDATE",
        "VK_RESOLVE_MODE_EXTERNAL_FORMAT_DOWNSAMPLE",
    };

    // These values are just missing "_BIT" (not obeying the api rules) but should be generated
    const static std::unordered_set<sv> missing_bit_value = {
        "VK_PHYSICAL_DEVICE_SCHEDULING_CONTROLS_SHADER_CORE_COUNT",
        "VK_CLUSTER_ACCELERATION_STRUCTURE_CLUSTER_ALLOW_DISABLE_OPACITY_MICROMAPS",
        "VK_PARTITIONED_ACCELERATION_STRUCTURE_INSTANCE_FLAG_ENABLE_EXPLICIT_BOUNDING_BOX",
        "VK_INDIRECT_COMMANDS_INPUT_MODE_VULKAN_INDEX_BUFFER", // These 2 values also seem like they should not be bitfields, but they are in the XML
        "VK_INDIRECT_COMMANDS_INPUT_MODE_DXGI_INDEX_BUFFER",
    };

    auto it = exception_values.find(value);
    if (it != exception_values.end())
        return NameTranslator(std::string(it->second));

    assert(value.starts_with(enum_.name) || enum_.name == "VK_RESULT"); // VK_RESULT values do not contain "VK_RESULT_" but only "VK_"
    Extension ext = enum_.name == "VK_VENDOR_ID" ? Extension::None : get_and_remove_extension_constant(value);
    if (enum_.ext != Extension::None && ext == enum_.ext)
        ext = Extension::None; // Discard the extension from enum_value if it is already in enum_name

    // VkDebugReportObjectTypeEXT values can end with <EXT_SUFFIX>_EXT, e.g. SURFACE_KHR_EXT
    if (enum_.name == "VK_DEBUG_REPORT_OBJECT_TYPE") {
        Extension inner_ext = get_and_remove_extension_constant(value);
        assert(inner_ext == Extension::None || ext == Extension::None);
        if (inner_ext != Extension::None)
            ext = inner_ext;
    };

    assert(is_bitfield || !value.ends_with("_BIT"));

    size_t prefix = enum_.name == "VK_RESULT" ? 2 : enum_.name.size(); // leaving space for "e"
    std::string new_name(value.substr(prefix));
    new_name[0] = 'e';

    if (is_bitfield) {
        if (new_name == "eNONE")
            return NameTranslator(std::string("eNone"));
        if (new_name == "eALL")
            return NameTranslator(std::string("eAll"));

        bool add_2_for_shadowed_value = missing_bit_with_shadowed_values.contains(value);
        // All values in these enums are missing "_BIT"
        bool whole_enum_missing_bit = enum_.name == "VK_IMAGE_COMPRESSION" || enum_.name == "VK_IMAGE_CONSTRAINTS_INFO";
        bool skip_bit = add_2_for_shadowed_value || missing_bit_value.contains(value) || whole_enum_missing_bit;

        if (!skip_bit) {
            assert(new_name.ends_with("_BIT"));
            new_name.resize(new_name.size() - sizeof("_BIT") + 1);
        }
        if (add_2_for_shadowed_value)
            new_name.append("2");

    }

    transform_from_upper_constant(new_name, 1, true);
    if (ext != Extension::None)
        new_name.append(to_string(ext));
    return NameTranslator(std::move(new_name));
}

NameTranslator vkgen::Generator::NameTranslator::from_type_name(sv name) {
    assert(!name.empty());
    assert(name.starts_with("Vk"));

    return NameTranslator(std::string(name.substr(2)));
}

NameTranslator vkgen::Generator::NameTranslator::from_command_name(sv name) {
    assert(!name.empty());
    assert(name.starts_with("vk"));

    std::string new_name(name.substr(2));
    // Lowercase the first letter: CreateBuffer -> createBuffer
    assert(!new_name.empty());
    new_name[0] = std::tolower(new_name[0]);

    return NameTranslator(std::move(new_name));
}

// pViewports -> viewports, pCoverageModulationTable -> coverageModulationTable
NameTranslator vkgen::Generator::NameTranslator::from_input_array_name(sv name) {
    assert(!name.empty());
    if (name.size() > 1 && name[0] == 'p' && std::isupper(name[1])) {
        std::string new_name(name.substr(1));
        new_name[0] = std::tolower(new_name[0]);
        return NameTranslator(std::move(new_name));
    }
    return NameTranslator(std::string(name));
}

std::string vkgen::Generator::NameTranslator::singularize(sv name) {
    // Split off any trailing all-uppercase extension suffix (KHR/EXT/NV/...)
    size_t suffix_start = name.size();
    while (suffix_start > 0 && std::isupper(static_cast<unsigned char>(name[suffix_start - 1])))
        --suffix_start;
    sv stem = name.substr(0, suffix_start);
    sv suffix = name.substr(suffix_start);

    if (stem.size() >= 3 && stem.ends_with("ies")) {
        std::string out(stem.substr(0, stem.size() - 3));
        out += 'y';
        out += suffix;
        return out;
    }
    if (!stem.empty() && stem.back() == 's') {
        std::string out(stem.substr(0, stem.size() - 1));
        out += suffix;
        return out;
    }
    return std::string(name);
}

std::pair<std::string, std::string> vkgen::Generator::NameTranslator::unique_command_name(sv name) {
    assert(!name.empty());
    assert(name.starts_with("vk"));

    std::string not_unique_name(name.substr(2));
    assert(!not_unique_name.empty());
    not_unique_name[0] = std::tolower(not_unique_name[0]);
    name = not_unique_name;

    Extension ext = get_and_remove_extension_name(name);
    std::string unique_name(name);
    unique_name.append("Unique");
    if (ext != Extension::None)
        unique_name.append(to_string(ext));

    return std::make_pair(std::move(unique_name), std::move(not_unique_name));
}

NameTranslator vkgen::Generator::NameTranslator::from_constexpr_value(sv value_name) {
    assert(!value_name.empty());
    assert(value_name.starts_with("VK_"));

    Extension ext = get_and_remove_extension_constant(value_name);
    std::string new_name(value_name.substr(3));
    transform_from_upper_constant(new_name, 0, true);
    if (ext != Extension::None)
        new_name.append(to_string(ext));

    return NameTranslator(std::move(new_name));
}

void vkgen::Generator::NameTranslator::transform_from_upper_constant(std::string& name, size_t start_pos, bool first_is_upper) {
    size_t j = start_pos;
    bool next_is_upper = first_is_upper;
    bool prev_was_digit = false;
    for (size_t i = j; i < name.size(); ++i) {
        if (name[i] == '_') {
            next_is_upper = true;
            prev_was_digit = false;
            // Keep "AV1" and "VP9" as-is (would otherwise become "Av1" and "Vp9")
            if (keep_av1_vp9 &&
                ((i + 4 < name.size() && (std::memcmp(&name[i + 1], "AV1_", 4) == 0 || std::memcmp(&name[i + 1], "VP9_", 4) == 0))
                    || (i + 4 == name.size() && (std::memcmp(&name[i + 1], "AV1", 3) == 0 || std::memcmp(&name[i + 1], "VP9", 3) == 0)))) {
                name[j++] = name[i + 1];
                name[j++] = name[i + 2];
                name[j++] = name[i + 3];
                i += 4;
            }
            continue;
        }

        if (std::isdigit(name[i])) {
            name[j++] = name[i];
            prev_was_digit = true;
            continue; // digits dont affect next_is_upper
        }

        if (prev_was_digit && i + 1 < name.size() && std::isdigit(name[i + 1])) {
            name[j++] = name[i++]; // preserve original case between digits (also for 'X', because vk.xml should consistently use 'X' for 'x' for different meanings)
            name[j++] = name[i];   //copy the digit
            continue; // [0-9][xX][0-9] dont affect next_is_upper
        } else if (next_is_upper) {
            name[j] = std::toupper(name[i]);
        } else {
            name[j] = std::tolower(name[i]);
        }

        next_is_upper = false;
        prev_was_digit = false;
        ++j;
    }
    name.resize(j);
}

Extension vkgen::Generator::NameTranslator::get_and_remove_extension_constant(sv& name) {
    if (name.ends_with("_IMG")) { name.remove_suffix(sizeof("IMG")); return Extension::Img; }
    if (name.ends_with("_AMD")) { name.remove_suffix(sizeof("AMD")); return Extension::Amd; }
    if (name.ends_with("_AMDX")) { name.remove_suffix(sizeof("AMDX")); return Extension::Amdx; }
    if (name.ends_with("_ARM")) { name.remove_suffix(sizeof("ARM")); return Extension::Arm; }
    if (name.ends_with("_FSL")) { name.remove_suffix(sizeof("FSL")); return Extension::Fsl; }
    if (name.ends_with("_BRCM")) { name.remove_suffix(sizeof("BRCM")); return Extension::Brcm; }
    if (name.ends_with("_NXP")) { name.remove_suffix(sizeof("NXP")); return Extension::Nxp; }
    if (name.ends_with("_NV")) { name.remove_suffix(sizeof("NV")); return Extension::Nv; }
    if (name.ends_with("_NVX")) { name.remove_suffix(sizeof("NVX")); return Extension::Nvx; }
    if (name.ends_with("_VIV")) { name.remove_suffix(sizeof("VIV")); return Extension::Viv; }
    if (name.ends_with("_VSI")) { name.remove_suffix(sizeof("VSI")); return Extension::Vsi; }
    if (name.ends_with("_KDAB")) { name.remove_suffix(sizeof("KDAB")); return Extension::Kdab; }
    if (name.ends_with("_ANDROID")) { name.remove_suffix(sizeof("ANDROID")); return Extension::Android; }
    if (name.ends_with("_CHROMIUM")) { name.remove_suffix(sizeof("CHROMIUM")); return Extension::Chromium; }
    if (name.ends_with("_FUCHSIA")) { name.remove_suffix(sizeof("FUCHSIA")); return Extension::Fuchsia; }
    if (name.ends_with("_GGP")) { name.remove_suffix(sizeof("GGP")); return Extension::Ggp; }
    if (name.ends_with("_GOOGLE")) { name.remove_suffix(sizeof("GOOGLE")); return Extension::Google; }
    if (name.ends_with("_QCOM")) { name.remove_suffix(sizeof("QCOM")); return Extension::Qcom; }
    if (name.ends_with("_LUNARG")) { name.remove_suffix(sizeof("LUNARG")); return Extension::Lunarg; }
    if (name.ends_with("_NZXT")) { name.remove_suffix(sizeof("NZXT")); return Extension::Nzxt; }
    if (name.ends_with("_SAMSUNG")) { name.remove_suffix(sizeof("SAMSUNG")); return Extension::Samsung; }
    if (name.ends_with("_SEC")) { name.remove_suffix(sizeof("SEC")); return Extension::Sec; }
    if (name.ends_with("_TIZEN")) { name.remove_suffix(sizeof("TIZEN")); return Extension::Tizen; }
    if (name.ends_with("_RENDERDOC")) { name.remove_suffix(sizeof("RENDERDOC")); return Extension::Renderdoc; }
    if (name.ends_with("_NN")) { name.remove_suffix(sizeof("NN")); return Extension::Nn; }
    if (name.ends_with("_MVK")) { name.remove_suffix(sizeof("MVK")); return Extension::Mvk; }
    if (name.ends_with("_KHR")) { name.remove_suffix(sizeof("KHR")); return Extension::Khr; }
    if (name.ends_with("_KHX")) { name.remove_suffix(sizeof("KHX")); return Extension::Khx; }
    if (name.ends_with("_EXT")) { name.remove_suffix(sizeof("EXT")); return Extension::Ext; }
    if (name.ends_with("_MESA")) { name.remove_suffix(sizeof("MESA")); return Extension::Mesa; }
    if (name.ends_with("_INTEL")) { name.remove_suffix(sizeof("INTEL")); return Extension::Intel; }
    if (name.ends_with("_HUAWEI")) { name.remove_suffix(sizeof("HUAWEI")); return Extension::Huawei; }
    if (name.ends_with("_OHOS")) { name.remove_suffix(sizeof("OHOS")); return Extension::Ohos; }
    if (name.ends_with("_VALVE")) { name.remove_suffix(sizeof("VALVE")); return Extension::Valve; }
    if (name.ends_with("_QNX")) { name.remove_suffix(sizeof("QNX")); return Extension::Qnx; }
    if (name.ends_with("_JUICE")) { name.remove_suffix(sizeof("JUICE")); return Extension::Juice; }
    if (name.ends_with("_FB")) { name.remove_suffix(sizeof("FB")); return Extension::Fb; }
    if (name.ends_with("_RASTERGRID")) { name.remove_suffix(sizeof("RASTERGRID")); return Extension::Rastergrid; }
    if (name.ends_with("_MSFT")) { name.remove_suffix(sizeof("MSFT")); return Extension::Msft; }
    if (name.ends_with("_SHADY")) { name.remove_suffix(sizeof("SHADY")); return Extension::Shady; }
    if (name.ends_with("_FREDEMMOTT")) { name.remove_suffix(sizeof("FREDEMMOTT")); return Extension::Fredemmott; }
    if (name.ends_with("_MTK")) { name.remove_suffix(sizeof("MTK")); return Extension::Mtk; }
    return Extension::None;
}

Extension vkgen::Generator::NameTranslator::get_and_remove_extension_name(sv& name) {
    if (name.ends_with("IMG")) { name.remove_suffix(sizeof("IMG") - 1); return Extension::Img; }
    if (name.ends_with("AMD")) { name.remove_suffix(sizeof("AMD") - 1); return Extension::Amd; }
    if (name.ends_with("AMDX")) { name.remove_suffix(sizeof("AMDX") - 1); return Extension::Amdx; }
    if (name.ends_with("ARM")) { name.remove_suffix(sizeof("ARM") - 1); return Extension::Arm; }
    if (name.ends_with("FSL")) { name.remove_suffix(sizeof("FSL") - 1); return Extension::Fsl; }
    if (name.ends_with("BRCM")) { name.remove_suffix(sizeof("BRCM") - 1); return Extension::Brcm; }
    if (name.ends_with("NXP")) { name.remove_suffix(sizeof("NXP") - 1); return Extension::Nxp; }
    if (name.ends_with("NV")) { name.remove_suffix(sizeof("NV") - 1); return Extension::Nv; }
    if (name.ends_with("NVX")) { name.remove_suffix(sizeof("NVX") - 1); return Extension::Nvx; }
    if (name.ends_with("VIV")) { name.remove_suffix(sizeof("VIV") - 1); return Extension::Viv; }
    if (name.ends_with("VSI")) { name.remove_suffix(sizeof("VSI") - 1); return Extension::Vsi; }
    if (name.ends_with("KDAB")) { name.remove_suffix(sizeof("KDAB") - 1); return Extension::Kdab; }
    if (name.ends_with("ANDROID")) { name.remove_suffix(sizeof("ANDROID") - 1); return Extension::Android; }
    if (name.ends_with("CHROMIUM")) { name.remove_suffix(sizeof("CHROMIUM") - 1); return Extension::Chromium; }
    if (name.ends_with("FUCHSIA")) { name.remove_suffix(sizeof("FUCHSIA") - 1); return Extension::Fuchsia; }
    if (name.ends_with("GGP")) { name.remove_suffix(sizeof("GGP") - 1); return Extension::Ggp; }
    if (name.ends_with("GOOGLE")) { name.remove_suffix(sizeof("GOOGLE") - 1); return Extension::Google; }
    if (name.ends_with("QCOM")) { name.remove_suffix(sizeof("QCOM") - 1); return Extension::Qcom; }
    if (name.ends_with("LUNARG")) { name.remove_suffix(sizeof("LUNARG") - 1); return Extension::Lunarg; }
    if (name.ends_with("NZXT")) { name.remove_suffix(sizeof("NZXT") - 1); return Extension::Nzxt; }
    if (name.ends_with("SAMSUNG")) { name.remove_suffix(sizeof("SAMSUNG") - 1); return Extension::Samsung; }
    if (name.ends_with("SEC")) { name.remove_suffix(sizeof("SEC") - 1); return Extension::Sec; }
    if (name.ends_with("TIZEN")) { name.remove_suffix(sizeof("TIZEN") - 1); return Extension::Tizen; }
    if (name.ends_with("RENDERDOC")) { name.remove_suffix(sizeof("RENDERDOC") - 1); return Extension::Renderdoc; }
    if (name.ends_with("NN")) { name.remove_suffix(sizeof("NN") - 1); return Extension::Nn; }
    if (name.ends_with("MVK")) { name.remove_suffix(sizeof("MVK") - 1); return Extension::Mvk; }
    if (name.ends_with("KHR")) { name.remove_suffix(sizeof("KHR") - 1); return Extension::Khr; }
    if (name.ends_with("KHX")) { name.remove_suffix(sizeof("KHX") - 1); return Extension::Khx; }
    if (name.ends_with("EXT")) { name.remove_suffix(sizeof("EXT") - 1); return Extension::Ext; }
    if (name.ends_with("MESA")) { name.remove_suffix(sizeof("MESA") - 1); return Extension::Mesa; }
    if (name.ends_with("INTEL")) { name.remove_suffix(sizeof("INTEL") - 1); return Extension::Intel; }
    if (name.ends_with("HUAWEI")) { name.remove_suffix(sizeof("HUAWEI") - 1); return Extension::Huawei; }
    if (name.ends_with("OHOS")) { name.remove_suffix(sizeof("OHOS") - 1); return Extension::Ohos; }
    if (name.ends_with("VALVE")) { name.remove_suffix(sizeof("VALVE") - 1); return Extension::Valve; }
    if (name.ends_with("QNX")) { name.remove_suffix(sizeof("QNX") - 1); return Extension::Qnx; }
    if (name.ends_with("JUICE")) { name.remove_suffix(sizeof("JUICE") - 1); return Extension::Juice; }
    if (name.ends_with("FB")) { name.remove_suffix(sizeof("FB") - 1); return Extension::Fb; }
    if (name.ends_with("RASTERGRID")) { name.remove_suffix(sizeof("RASTERGRID") - 1); return Extension::Rastergrid; }
    if (name.ends_with("MSFT")) { name.remove_suffix(sizeof("MSFT") - 1); return Extension::Msft; }
    if (name.ends_with("SHADY")) { name.remove_suffix(sizeof("SHADY") - 1); return Extension::Shady; }
    if (name.ends_with("FREDEMMOTT")) { name.remove_suffix(sizeof("FREDEMMOTT") - 1); return Extension::Fredemmott; }
    if (name.ends_with("MTK")) { name.remove_suffix(sizeof("MTK") - 1); return Extension::Mtk; }
    return Extension::None;
}

TypeParam vkgen::Generator::TypeParam::from_xml(const xml::Element& elem, vkgen::Arena& arena) {
    State state = State::AtStart;
    TypeParam param;

    for (Node* child : elem.children) {
        if (child->isText()) {
            state = param.parse_string(child->asText(), state);
            continue;
        }

        auto& ch = child->asElement();
        if (ch.tag == "name") {
            switch (state) {
            case State::AtStart:
            case State::BeforeType:
                throw my_error{ "Command parameter is missing a type!" };
            case State::BeforeName:
                state = TypeParam::State::AfterName;
                param.name = ch.children[0]->asText();
                break;
            case State::AfterName:
            case State::AfterBitSelect:
                throw my_error{ "Command parameter can only have one name!" };
            case State::InsideArray:
                throw my_error{ "Excepted <enum> inside [] as an array parameter but found <name>!" };
            }

        } else if (ch.tag == "type") {

            switch (state) {
            case State::AtStart: // qualifiers before type could be empty
            case State::BeforeType:
                state = TypeParam::State::BeforeName;
                // TODO: search for the type
                param.type = ch.children[0]->asText();
                break;
            case State::BeforeName:
            case State::AfterName:
            case State::AfterBitSelect:
                throw my_error{ "Command parameter can only have one type!" };
            case State::InsideArray:
                throw my_error{ "Excepted <enum> inside [] as an array parameter but found <type>!" };
            }

        } else if (ch.tag == "enum") {
            if (state != State::InsideArray)
                throw my_error{ "Enum tag can only be inside [] as an array size parameter!" };

            // FIXME: search for the type, this is real hack
            TypeEnum::EnumItem enum_item;
            enum_item.name = ch.children[0]->asText();
            param.array_extensions.emplace_back(arena.make<TypeEnum::EnumItem>(std::move(enum_item)));

        } else if (ch.tag == "comment") {
            if (!param.comment.empty())
                throw my_error{ "Command parameter can only have one comment!" };
            param.comment = ch.children[0]->asText();
        } else {
            throw my_error{ "Unknown tag <" + std::string(ch.tag) + "> in command parameter! Expected <name>, <type> or <enum>" };
        }
    }


    return param;
}

void Type::parse_funcpointer(const xml::Element& elem, vkgen::Arena& arena) {
    UNUSED(arena);

    // The expected format:
    // typedef <return_type> (VKAPI_PTR* <name>func_name</name>)(<params>)

    if (elem.children.empty())
        throw my_error{ "Type Funcpointer: expected at least one child! (Expected format: typedef <return_type> (VKAPI_PTR* <name>func_name</name>)(<params>)" };

    auto it = elem.children.begin();

    if (!(*it)->isText())
        throw my_error{ "Type Funcpointer: expected first child to be text! (Expected format: typedef <return_type> (VKAPI_PTR* <name>func_name</name>)(<params>)" };
    sv first_text = (*it)->asText();
    trim_leading_ws(first_text);

    if (!first_text.starts_with("typedef"))
        throw my_error{ "Type Funcpointer: expected to start with 'typedef'! (Expected format: typedef <return_type> (VKAPI_PTR* <name>func_name</name>)(<params>)" };
    first_text.remove_prefix(7);
    trim_leading_ws(first_text);

    // Find "VKAPI_PTR" — everything before '(' VKAPI_PTR is the return type
    auto vkapi_pos = first_text.find("VKAPI_PTR");
    if (vkapi_pos == sv::npos)
        throw my_error{ "Type Funcpointer: did not find VKAPI_PTR! (Expected format: typedef <return_type> (VKAPI_PTR* <name>func_name</name>)(<params>)" };

    sv return_text = first_text.substr(0, vkapi_pos);
    trim_ws(return_text);
    if (return_text.empty() || return_text.back() != '(')
        throw my_error{ "Funcpointer: missing '(' before VKAPI_PTR! (Expected format: typedef <return_type> (VKAPI_PTR* <name>func_name</name>)(<params>)" };
    return_text.remove_suffix(1);
    trim_ws(return_text);

    funcptr->return_type = TypeRef::from_string(return_text);

    ++it;

    // Second child: <name> — set Type::name directly
    if (it == elem.children.end() || !(*it)->isElement() || (*it)->asElement().tag != "name")
        throw my_error{ "Funcpointer: expected <name> element as second child" };
    name = (*it)->asElement().children[0]->asText();
    ++it;

    // --- Phase 2: Parse parameters ---
    // Third child text: ")(\n    " or ")(void);"
    if (it == elem.children.end() || !(*it)->isText())
        throw my_error{ "Funcpointer: expected text node after <name>" };
    sv after_name = (*it)->asText();

    auto paren_pos = after_name.find(")(");
    if (paren_pos == sv::npos)
        throw my_error{ "Funcpointer: missing ')(' parameter list separator" };
    sv param_start_text = after_name.substr(paren_pos + 2);
    trim_leading_ws(param_start_text);

    // Check for "(void);" — no parameters
    if (param_start_text.starts_with("void")) {
        sv after_void = param_start_text.substr(4);
        trim_leading_ws(after_void);
        if (after_void.starts_with(");"))
            return;
    }

    ++it;

    // Iterate remaining children to build parameters
    sv pending_pre_text = param_start_text;

    while (it != elem.children.end()) {
        if ((*it)->isElement() && (*it)->asElement().tag == "type") {
            TypeParam param;

            // Check pending text for "const" qualifier
            sv pre = pending_pre_text;
            while (!pre.empty() && (std::isspace(pre[0]) || pre.front() == ','))
                pre.remove_prefix(1);

            if (pre.starts_with("const")) {
                sv after_const = pre.substr(5);
                if (after_const.empty() || std::isspace(static_cast<unsigned char>(after_const.front()))) {
                    param.pre_qual = TypeParam::PreQualifier::Const;
                }
            }

            param.type = (*it)->asElement().children[0]->asText();
            ++it;

            // Next: text with optional pointer, name, separator
            if (it != elem.children.end() && (*it)->isText()) {
                sv post = (*it)->asText();
                trim_leading_ws(post);

                // Parse post-qualifiers: *, const*, * const, etc.
                while (!post.empty() && (post.front() == '*' || (post.starts_with("const") &&
                    (post.size() == 5 || std::isspace(static_cast<unsigned char>(post[5])) || post[5] == '*')))) {
                    if (post.front() == '*') {
                        param.post_quals.push_back(TypeParam::PostQualifier::Pointer);
                        post.remove_prefix(1);
                        trim_leading_ws(post);
                    } else {
                        post.remove_prefix(5);
                        trim_leading_ws(post);
                        if (!post.empty() && post.front() == '*') {
                            param.post_quals.push_back(TypeParam::PostQualifier::ConstPointer);
                            post.remove_prefix(1);
                            trim_leading_ws(post);
                        } else {
                            param.post_quals.push_back(TypeParam::PostQualifier::Const);
                        }
                    }
                }

                // Extract parameter name
                size_t name_end = 0;
                while (name_end < post.size() &&
                    !std::isspace(static_cast<unsigned char>(post[name_end])) &&
                    post[name_end] != ',' &&
                    post[name_end] != ')' &&
                    post[name_end] != ';') {
                    name_end++;
                }
                param.name = post.substr(0, name_end);
                pending_pre_text = post.substr(name_end);
                ++it;
            }

            funcptr->parameters.push_back(std::move(param));

        } else if ((*it)->isText()) {
            pending_pre_text = (*it)->asText();
            ++it;
        } else {
            throw my_error{ "Funcpointer: unexpected element <" + std::string((*it)->asElement().tag) + "> in parameter list" };
        }
    }
}

std::string vkgen::Generator::TypeParam::stringify() const {
    std::stringstream ss;
    if (is_const())
        ss << "const ";

    // TODO: assert that if pre_qual is struct|union|enum, the type category is correct

    // TASK: 210326_01 add config for generating c keywords
    // if (config.generate_c_type_keywords)

    // TODO: name translator
    if (type.starts_with("Vk"))
        ss << type.substr(2);
    else
        ss << type;

    for (const auto& qualifier : post_quals) {
        if (bool(qualifier & PostQualifier::Const))
            ss << " const";
        if (bool(qualifier & PostQualifier::Pointer))
            ss << "*";
    }

    ss << ' ' << name;

    for (const auto& extension : array_extensions) {
        switch (extension.type) {
        case ArrayExtension::Type::PlainSize:
            ss << '[' << extension.size << ']';
            break;
        case ArrayExtension::Type::EnumItem:
            ss << '[' << NameTranslator::from_constexpr_value(extension.enum_item->name) << ']';
            break;
        case ArrayExtension::Type::BitSelect:
            ss << ':' << (int)extension.bitpos;
            break;
        }
    }
    return ss.str();
}

TypeRef vkgen::Generator::TypeRef::from_string(sv text) {
    TypeRef ref;
    trim_ws(text);

    if (text.starts_with("const") && (text.size() == 5 || std::isspace(text[5]))) {
        ref.is_const = true;
        text.remove_prefix(5);
        trim_leading_ws(text);
    }

    // Extract type name (up to first '*' or whitespace)
    size_t type_end = 0;
    while (type_end < text.size() && text[type_end] != '*' && !std::isspace(text[type_end])) {
        type_end++;
    }
    if (type_end == 0)
        throw my_error{ "TypeRef::from_string: empty type in '" + std::string(text) + "'" };

    ref.type = text.substr(0, type_end);
    sv quals = text.substr(type_end);
    trim_leading_ws(quals);

    while (!quals.empty()) {
        if (quals.front() == '*') {
            ref.post_quals.push_back(TypeParam::PostQualifier::Pointer);
            quals.remove_prefix(1);
            trim_leading_ws(quals);

        } else if (quals.starts_with("const") && (quals.size() == 5 || std::isspace(quals[5]) || quals[5] == '*')) {
            quals.remove_prefix(5);
            trim_leading_ws(quals);
            if (!quals.empty() && quals.front() == '*') {
                ref.post_quals.push_back(TypeParam::PostQualifier::ConstPointer);
                quals.remove_prefix(1);
                trim_leading_ws(quals);
            } else {
                ref.post_quals.push_back(TypeParam::PostQualifier::Const);
            }
        } else {
            throw my_error{ "TypeRef::from_string: unexpected token '" + std::string(quals) + "'" };
        }
    }
    return ref;
}

std::string vkgen::Generator::TypeRef::stringify() const {
    std::stringstream ss;
    if (is_const)
        ss << "const ";

    // TODO: name translator
    if (type.starts_with("Vk"))
        ss << type.substr(2);
    else
        ss << type;

    for (const auto& qualifier : post_quals) {
        if (bool(qualifier & TypeParam::PostQualifier::Const))
            ss << " const";
        if (bool(qualifier & TypeParam::PostQualifier::Pointer))
            ss << '*';
    }
    return ss.str();
}

void vkgen::Generator::trim_leading_ws(sv& s) {
    auto it = std::find_if_not(s.begin(), s.end(), [](unsigned char c) { return std::isspace(c); });
    s.remove_prefix(it - s.begin());
}

void vkgen::Generator::trim_trailing_ws(sv& s) {
    auto it = std::find_if_not(s.rbegin(), s.rend(), [](unsigned char c) { return std::isspace(c); });
    s.remove_suffix(it - s.rbegin());
}

void vkgen::Generator::trim_ws(sv& s) {
    trim_leading_ws(s);
    trim_trailing_ws(s);
}

uint64_t vkgen::Generator::TypeParam::get_size(sv str) {
    uint64_t size = 0;
    for (char c : str) {
        if (c < '0' || c > '9')
            throw my_error{ "Invalid size of array: " + std::string(str) };
        size *= 10, size += c - '0';
    }
    return size;
}

TypeParam::State vkgen::Generator::TypeParam::parse_string(sv text, State state) {
    sv original = text;
    const auto check_pre_qual = [&](sv keyword, PreQualifier flag) -> bool {
        // TODO: checking like this can be unsafe (does not check if the word ends - "const" in "constexpr")
        if (!text.starts_with(keyword)) return false;

        if (bool(pre_qual & flag)) {
            throw my_error{ "Found " + std::string(keyword) + " qualifier twice! '" + std::string(original) + "'" };
        }

        pre_qual |= flag;
        text.remove_prefix(keyword.length());
        return true;
        };

    switch (state) {
    case State::AtStart:
        while (true) {
            trim_leading_ws(text);
            if (text.empty())
                return is_const() ? State::BeforeType : State::AtStart;

            if (check_pre_qual("const", PreQualifier::Const)) continue;
            if (check_pre_qual("struct", PreQualifier::Struct)) continue;
            if (check_pre_qual("union", PreQualifier::Union)) continue;
            if (check_pre_qual("enum", PreQualifier::Enum)) continue;

            throw my_error{ "Unknown qualifier before type! '" + std::string(original) + "' Expected struct, union or enum!" };
        }

    case State::BeforeType:
        assert(is_const());
        while (true) {
            trim_leading_ws(text);
            if (text.empty())
                return State::BeforeName;
            if (text.starts_with("const"))
                throw my_error{ "Found const qualifier twice before type! '" + std::string(original) + "'" };

            if (check_pre_qual("struct", PreQualifier::Struct)) continue;
            if (check_pre_qual("union", PreQualifier::Union)) continue;
            if (check_pre_qual("enum", PreQualifier::Enum)) continue;

            throw my_error{ "Unknown qualifier before type! '" + std::string(original) + "' Expected struct, union or enum!" };
        }

    case State::BeforeName:
        while (true) {
            trim_leading_ws(text);
            if (text.empty())
                return State::BeforeName;
            if (text[0] == '*') {
                post_quals.push_back(PostQualifier::Pointer);
                text.remove_prefix(1);
            } else if (text.starts_with("const") &&
                (text.size() == 5 || std::isspace(text[5]) || text[5] == '*')) {
                text.remove_prefix(5);
                trim_leading_ws(text);
                if (!text.empty() && text[0] == '*') {
                    post_quals.push_back(PostQualifier::ConstPointer);
                    text.remove_prefix(1);
                } else {
                    post_quals.push_back(PostQualifier::Const);
                }
            } else {
                throw my_error{ "Unknown qualifier after type! '" + std::string(original) + "'" };
            }
        }
        return State::BeforeName;

    case State::AfterName:
        trim_ws(text);
        if (text.empty())
            return State::AfterName;
        if (text[0] != '[' && text[0] != ':')
            throw my_error{ "Excepted '[' or ':' after name but found '" + std::string(original) + "'" };
        if (text[0] == ':') {
            if (!array_extensions.empty())
                throw my_error{ "Can't combine bitselect (:<num>) and array extensions!" };
            uint64_t size = get_size(text.substr(1));
            if (size > 64)
                throw my_error{ "Bitselect can't be bigger than 64 bits! '" + std::string(original) + "'" };
            array_extensions.emplace_back((uint8_t)size);
            return State::AfterBitSelect;
        }
        // "[<size>]"
        text.remove_prefix(1);
        trim_leading_ws(text);
        if (text.empty())
            return State::InsideArray; // this text ends so some other element is coming

        if (!std::isdigit(text[0]))
            throw my_error{ "Excepted array size or <enum> after '[' but found '" + std::string(original) + "'" };

        {
            int i;
            for (i = 1; std::isdigit(text[i]); ++i);
            uint64_t size = get_size(text.substr(0, i));
            text.remove_prefix(i);
            trim_leading_ws(text);
            if (text.empty() || text[0] != ']')
                throw my_error{ "Excepted ']' after array size but found '" + std::string(original) + "'" };
            array_extensions.emplace_back(size);
        }
        return State::AfterName;

    case State::InsideArray:
        trim_leading_ws(text);
        if (text[0] != ']')
            throw my_error{ "Excepted ']' after array size but found '" + std::string(original) + "'" };
        if (text.size() > 1)
            return parse_string(text.substr(1), State::AfterName);
        return State::AfterName;

    case State::AfterBitSelect:
        trim_leading_ws(text);
        if (text.empty())
            return State::AfterBitSelect;
        throw my_error{ "Can't have anything after bitselect (:<num>)!" };
    }
    UNREACHABLE();
}

ApiType vkgen::Generator::ApiType::from_string(sv str) {
    if (str.empty())
        return ApiType::Any;

    ApiType type((Value)0);
    size_t split;
    do {
        split = str.find(',');
        sv part = str.substr(0, split);
        if (part == "vulkan")
            type = type | ApiType::Vulkan;
        else if (part == "vulkansc")
            type = type | ApiType::VulkanSC;
        else
            throw my_error{ "Unknown api type! '" + std::string(part) + "'" };

        if (split == sv::npos)
            break;

        str.remove_prefix(split + 1);
    } while (!str.empty()); // trailing comma check

    return type;
}

