/**
 * @file generator.hpp
 * @author Jaroslav Hucel (xhucel00@vutbr.cz)
 * @brief Vulkan registry data model and code generator driver.
 * @date Created: 02. 11. 2025
 * @date Modified: 15. 08. 2026
 *
 * @copyright Copyright (c) 2025 -> Public Domain, for more information see LICENSE
 */

#pragma once

#include "../config.hpp"
#include "../debug_macros.h"
#include "../xml/xml.hpp"

#include <map>
#include <unordered_map>
#include <unordered_set>

 // TASK: 090126_01
#define CONCEPT_FILTERING

namespace vkgen::Generator {
    constexpr int BITPOS_MAX = 64 - 1;

    struct Platform {
        std::string_view name; // Expected short C99 identifier compatible name
        std::string_view protect = {}; // C99 preprocessor token, starting with VK_USE_PLATFORM_
        std::string_view comment = {};

        static Platform from_xml(const xml::Element& elem);
    };

    struct Platforms {
        std::string_view comment = {};
        std::vector<Platform> platforms = {};

        static Platforms from_xml(const xml::Element& elem);
    };

    struct Tag {
        std::string_view name;
        std::string_view author;
        std::string_view contact;

        static Tag from_xml(const xml::Element& elem) { UNUSED(elem); NOT_IMPLEMENTED(); };
    };

    struct Tags {
        std::string_view comment = {};
        std::vector<Tag> tags = {};

        static Tags from_xml(const xml::Element& elem) { UNUSED(elem); NOT_IMPLEMENTED(); };
    };

    class ApiType {
    public:
        enum class Value : uint8_t {
            Any = 0xff,
            Vulkan = 1 << 0,
            VulkanSC = 1 << 1,
        };
        using enum Value;
        using underlying_type = std::underlying_type_t<Value>;

        constexpr ApiType() : value(Any) {}
        constexpr ApiType(Value val) : value(val) {}
        constexpr operator Value() const { return value; }
        explicit operator bool() const { return !!(underlying_type)value; }
        constexpr bool operator==(ApiType a) const { return value == a.value; }
        constexpr bool operator!=(ApiType a) const { return value != a.value; }
        constexpr ApiType operator&(ApiType b) const { return ApiType((Value)((underlying_type)value & (underlying_type)b.value)); }
        constexpr ApiType operator|(ApiType b) const { return ApiType((Value)((underlying_type)value | (underlying_type)b.value)); }
        constexpr ApiType operator&(Value b) const { return ApiType((Value)((underlying_type)value & (underlying_type)b)); }
        constexpr ApiType operator|(Value b) const { return ApiType((Value)((underlying_type)value | (underlying_type)b)); }

        bool is_vulkan() const { return (underlying_type)value & (underlying_type)Vulkan; }

        static ApiType from_string(std::string_view str);
    private:
        Value value;
    };

    struct HandleInfo;
    struct TypeHandle {
        // Name of the enum class containing VK_OBJECT_TYPE_*
        static constexpr std::string_view obj_enum_name = "VkObjectType";

        std::string_view parent; // Notes another type with the 'handle'
        // category that acts as a parent object for
        // this type.

        std::string_view objtypeenum; // name of VK_OBJECT_TYPE_* API enumerant which
        // corresponds to this type. Should be present

        HandleInfo* info = nullptr; // FIXME: TASK: 310326_01 Currently we create this info in the cache_handles function into array, that then is cleared, THIS IS A BIG HACK and needs refactoring
    };

    struct TypeEnum {

        struct ValueNormal {
            std::string_view value;
        };

        struct ValueBitmask {
            bool is_bitfield = false;
            union {
                uint8_t bitpos = 0;
                std::string_view value;
            };
        };

        struct ValueConstant {
            std::string_view value;
            std::string_view type;
        };

        struct EnumItem {
            std::string_view name;
            std::string_view comment;

            union {
                ValueNormal normal = {};
                ValueBitmask bitmask;
                ValueConstant constant;
                std::string_view alias;
            };

            std::string_view protect;
            std::string_view deprecated;
            ApiType api;
            bool is_alias = false;
            bool is_standalone_comment = false;
            uint16_t ext_number = 0;

            static EnumItem from_xml(const vkgen::xml::Element& elem, vkgen::Arena& arena, TypeEnum* parent, bool is_standalone_comment = false, bool extend_parent = false, uint16_t ext_number = 0);
        };


        enum class Type : uint8_t {
            None,
            Bitmask,
            Normal,
            Constants
        };

        enum class Bitwidth : uint8_t {
            None,
            _8,
            _16,
            _32,
            _64
        };

        std::string_view name;
        std::string_view comment;
        std::string_view deprecated;
        std::string_view protect; // platform protection macro (e.g., VK_USE_PLATFORM_WIN32_KHR), can contain complex expressions
        Type type = Type::None;
        Bitwidth bitwidth = Bitwidth::None;
        std::vector<EnumItem> items; // TASK: 180426_02

        TypeEnum(const vkgen::xml::Element& elem, vkgen::Arena& arena);

    private:
        Type type_from_string(std::string_view s);
        Bitwidth bitwidth_from_string(std::string_view s);
    };


    class TypeParam {
    public:
        enum class PreQualifier : uint8_t {
            None = 0,
            Const = 1 << 0,
            Struct = 1 << 1,
            Union = 1 << 2,
            Enum = 1 << 3,
            ConstStruct = Const | Struct,
            ConstUnion = Const | Union,
            ConstEnum = Const | Enum
        };

        enum class PostQualifier : uint8_t {
            None = 0,
            Const = 1 << 0,
            Pointer = 1 << 1,
            ConstPointer = Const | Pointer
        };


        struct ArrayExtension {
            enum class Type : uint8_t {
                PlainSize,
                EnumItem,
                BitSelect,
            };
            Type type;
            union {
                TypeEnum::EnumItem* enum_item;
                uint64_t size;
                uint8_t bitpos;
            };
            ArrayExtension(uint64_t s) : type(Type::PlainSize) { size = s; }
            ArrayExtension(TypeEnum::EnumItem* e) : type(Type::EnumItem) { enum_item = e; }
            ArrayExtension(uint8_t b) : type(Type::BitSelect) { bitpos = b; }
        };

        std::string_view type;
        std::string_view name;
        PreQualifier pre_qual = PreQualifier::None;
        std::string_view comment = {}; // TODO: this comment is in the moment not generated
        std::vector<PostQualifier> post_quals;
        std::vector<ArrayExtension> array_extensions;

        enum class State : uint8_t {
            AtStart,
            BeforeType,
            BeforeName,
            AfterName,
            InsideArray,
            AfterBitSelect
        };

        static TypeParam from_xml(const xml::Element& elem, vkgen::Arena& arena);
        std::string stringify() const;
        bool is_const() const;

    private:
        static uint64_t get_size(std::string_view str);
        State parse_string(std::string_view text, State state);
    };

    void trim_leading_ws(std::string_view& s);
    void trim_trailing_ws(std::string_view& s);
    void trim_ws(std::string_view& s);

    class Type;

    struct Member {
        enum class LimitType : uint16_t {
            None = 0,
            Min = 1,
            Max = 1 << 1,
            Not = 1 << 2,
            Pot = 1 << 3,
            Mul = 1 << 4,
            Bits = 1 << 5,
            Bitmask = 1 << 6,
            Range = 1 << 7,
            Struct = 1 << 8,
            Exact = 1 << 9,
            NoAuto = 1 << 10
        };

        enum class ExternSync : uint16_t {
            False,
            True,
            Maybe
        };

        TypeParam type_param;

        ApiType api;

        // if the member is an array, len may be one or more of the following
        // things, separated by commas (one for each array indirection):
        // another member of that struct, 'null-terminated' for a string,
        // '1' to indicate it is just a pointer (used for nested pointers),
        // or a latex equation (prefixed with 'latexmath:')
        std::string_view len;

        // if len has latexmath equations, this contains equivalent C99
        // expressions separated by commas.
        std::string_view altlen;

        // if the member is an array, stride specifies the name of
        // another member containing the byte stride between consecutive
        // elements in the array.The array is assumed to be tightly packed
        // if omitted.
        std::string_view stride;
        std::string_view deprecated; // denotes that this member is deprecated, and why

        // denotes that this member should be externally synchronized when accessed by Vulkan
        ExternSync externsync;
        bool optional; // denotes whether this value can be omitted by providing NULL

        // for a union member, identifies a separate enum member that
        // selects which of the union's members are valid
        std::string_view selector;


        // for a member of a union, identifies one or more commad-separated
        // enum values indicating that member is valid
        std::string_view selection;
        bool noautovalidity; // tag stating that no automatic validity language should be generated
        std::string_view values; // comma-separated list of legal values, usually used only for sType enums

        // Only applicable for members of VkPhysicalDeviceProperties and
        // VkPhysicalDeviceProperties2, their substructures, and extensions.
        // Specifies the type of a device limit.
        LimitType limittype;

        // Only applicable for members representing a handle as
        // a uint64_t value. Specifies the name of another member which is
        // a VkObjectType or VkDebugReportObjectTypeEXT value specifying
        // the type of object the handle references.
        std::string_view objecttype;

        // Only applicable for members representing a Boolean API
        // feature. Specifies that the feature has a link in the
        // specification that does not match the name of the feature.
        // Typically for features in extensions that were later promoted but
        // with changes.
        std::string_view featurelink;

        std::string_view comment;
        bool is_standalone_comment = false;

        Member(const vkgen::xml::Element& elem, vkgen::Arena& arena, Type& parent, bool is_standalone_comment = false);

        LimitType limit_type_from_string(std::string_view s);
    };


    struct TypeStruct {

        bool returned_only = false; // Notes that this struct is going to be filled in
        // by the API, rather than an application filling
        // it out and passing it to the API.

        bool allow_duplicate = false; // pNext can include multiple structures of this type.
        bool has_pNext = false; // pNext is present in this structure
        bool pNext_is_const = false; // pNext is a const pointer, invalid if has_pNext is false

        std::string_view struct_extends; // Lists parent structures which this structure may extend
        // via the pNext chain of the parent. When present it
        // suppresses generation of automatic validity for the
        // pNext member of that structure, and instead the
        // structure is added to pNext chain validity for the
        // parent structures it extends.

        std::vector<Member> members;
    };

    // TASK: 180426_04
    using TypeUnion = TypeStruct;

    struct TypeRef {
        bool is_const = false;
        std::string_view type;
        std::vector<TypeParam::PostQualifier> post_quals;

        static TypeRef from_string(std::string_view text);
        std::string stringify() const;
    };

    struct TypeFuncpointer {
        TypeRef return_type;
        std::vector<TypeParam> parameters;
    };

    class Type {
    public:
        enum class State : uint8_t {
            NotUsed,
            Declared,
            Defined
        };

        enum class Category : uint8_t {
            Basetype,
            Bitmask,
            Define,
            Enum, // content should be empty, only declaration
            Handle,
            Funcpointer,
            Struct,
            Union,
            Include,
            None // content may include types <apientry />, <name>...</name>,
                 // <type>...</type>, tag <type> may be multiple imbedded <type> tags
        };

        std::string_view name; // name attribute or present it <name>...</name>, always must be present
        ApiType api; // api attribute, matches a <feature> api attribute, if present
        std::string_view alias; // alias attribute
        union {
            std::string_view requires_ = {}; // requires attribute, pointing to another type
            std::string_view bitvalues; // Bitmask only: FlagBits enum name. Populated from XML `requires` (old-style) or `bitvalues` (new-style) attr.
        };
        std::string_view comment; // comment attribute
        std::string_view deprecated; // reason for deprecation if present, can also have values "true" or "false"
        std::string_view protect; // platform protection macro, set internally (not from XML)
        Category category = Category::None;
        State state = State::NotUsed;
        union {
            TypeHandle* handle;
            TypeStruct* struct_;
            TypeUnion* union_;
            TypeFuncpointer* funcptr;
        };

        const xml::Element& elem;

        Type(const xml::Element& elem, vkgen::Arena& arena);
        Type(Type&& other) noexcept;
        Type& operator=(Type&&) = delete;
        Type(const Type&) = delete;
        Type& operator=(const Type&) = delete;
        ~Type();

    private:
        Category category_from_string(std::string_view s) const;
        void parse_struct(const xml::Element& elem, Arena& arena);
        void parse_union(const xml::Element& elem, Arena& arena);
        void parse_funcpointer(const xml::Element& elem, Arena& arena);
    };

    class CommandParameter {
    public:
        ApiType api = ApiType::Any;
        // TASK: 090126_07
        std::string_view len = {};
        std::string_view altlen = {};
        std::string_view stride = {};
        std::string_view optional = {}; // can be comma separated, if not present, it is false, value should be provided for every indirection
        std::string_view selector = {}; // only if parameter is union
        std::string_view noautovalidity = {}; // optional
        std::string_view externsync = {}; // optional
        std::string_view objecttype = {}; // optional
        std::string_view validstructs = {}; // optional

        TypeParam type_param;

        static CommandParameter from_xml(const xml::Element& elem, vkgen::Arena& arena);

    };

    // Bitset of which overload kinds a command emits. Cached on Command after classification
    // so module export emits only the names that actually exist, without re-running the
    // classifier or duplicating its naming logic.
    enum class Overloads : uint8_t {
        None = 0,
        ThrowNoThrow = 1, // emits _throw / _noThrow (and a default forwarder when configured)
        Unique = 2, // emits the Unique<T> form
        Singular = 4, // emits the singular convenience overloads (createGraphicsPipeline*)
    };
    constexpr Overloads operator|(Overloads a, Overloads b) {
        return static_cast<Overloads>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
    }
    constexpr Overloads operator&(Overloads a, Overloads b) {
        return static_cast<Overloads>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
    }
    constexpr Overloads& operator|=(Overloads& a, Overloads b) { return a = a | b; }
    constexpr bool has(Overloads set, Overloads bit) { return (set & bit) != Overloads::None; }

    struct CommandClassification {
        enum class Pattern : uint8_t {
            Void,              // void return, no error checking
            ResultVoid,        // VkResult return, no output data
            ResultCreate,      // VkResult return, creates handle via out-param
            ResultCreateArray, // VkResult return, creates N handles; output array's len references a uint32_t count param or a count member of a const struct param
            VoidOutParam,      // void return, fills struct via out-param
            Enumerate,         // two-call enumerate pattern
            ResultOutParam,    // VkResult + non-handle out-param
            Other,             // non-standard return type — pass-through
        };
        Pattern pattern;

        int implicit_param_idx = -1;      // device/instance to substitute with global
        std::string_view implicit_global; // implicit parameter replacement: "detail::_device" or "detail::_instance"
        int output_param_idx = -1;        // handle or struct output
        int allocator_param_idx = -1;     // VkAllocationCallbacks*
        int count_param_idx = -1;         // for enumerate
        int array_param_idx = -1;         // for enumerate
        bool output_has_destroy = false;  // ResultCreate: output handle has destroy() overload

        // ResultCreateArray with a struct-member count (len="pAllocateInfo->commandBufferCount").
        // Mutually exclusive with count_param_idx: there is no count parameter to drop from the
        // wrapper signature, the count is read from the info struct the caller already passes in.
        int count_struct_param_idx = -1;  // const struct param owning the count member
        std::string_view count_member;    // member name, e.g. "commandBufferCount"

        // Input array pairs: non-pointer uint32_t count + const T* with len referencing it (1:1 only)
        static constexpr int max_input_arrays = 8;
        struct InputArrayPair { int count_idx; int array_idx; };
        InputArrayPair input_arrays[max_input_arrays] = {};
        int input_array_count = 0;

        bool is_input_count(int idx) const {
            for (int k = 0; k < input_array_count; ++k)
                if (input_arrays[k].count_idx == idx) return true;
            return false;
        }
        bool is_input_array(int idx) const {
            for (int k = 0; k < input_array_count; ++k)
                if (input_arrays[k].array_idx == idx) return true;
            return false;
        }
    };

    struct  CommandAlias {
        std::string_view name;
        std::string_view alias;
        ApiType api = ApiType::Any; // comma separated list, optional
        std::string_view comment = {}; // optional

        static CommandAlias from_xml(const xml::Element& elem, vkgen::Arena& arena);
    };

    class Command {
    public:
        enum class Scope : uint8_t {
            None,
            Inside,
            Outside,
            Both
        };

        // TASK: 090126_06
        std::string_view tasks = {}; // comma separated list, optional
        std::string_view queues = {}; // comma separated list, optional
        std::string_view success_codes = {}; // comma separated list, optional
        std::string_view error_codes = {}; // comma separated list, optional
        Scope render_pass = Scope::None; // optional
        Scope video_encoding = Scope::None; // optional
        std::string_view cmd_buffer_level = {}; // comma separated list, optional
        std::string_view conditional_rendering = {}; // required for vkCmd* commands.Not allowed for other commands. Values true/false
        bool allow_no_queues = false; // optional, default false
        std::string_view export_ = {}; // comma separated list, optional
        ApiType api = {}; // comma separated list, optional
        std::string_view description = {};

        xml::Element* implicit_extern_sync_params = nullptr;

        std::string_view comment = {};
        std::string_view protect; // platform protection macro, set internally (not from XML)

        std::string_view name;
        std::string_view type;

        // TODO: the return type and parameters can contain arbitrary C code
        std::string declaration;

        std::vector<CommandParameter> parameters = {};
        Overloads overloads = Overloads::None;

        static Command from_xml(const xml::Element& elem, vkgen::Arena& arena);
    };

    struct DefineExt {
        std::string_view name;
        std::string_view value;
        xml::Element& elem;
    };

    enum class Extension {
        None,
        Img,
        Amd,
        Amdx,
        Arm,
        Fsl,
        Brcm,
        Nxp,
        Nv,
        Nvx,
        Viv,
        Vsi,
        Kdab,
        Android,
        Chromium,
        Fuchsia,
        Ggp,
        Google,
        Qcom,
        Lunarg,
        Nzxt,
        Samsung,
        Sec,
        Tizen,
        Renderdoc,
        Nn,
        Mvk,
        Khr,
        Khx,
        Ext,
        Mesa,
        Intel,
        Huawei,
        Ohos,
        Valve,
        Qnx,
        Juice,
        Fb,
        Rastergrid,
        Msft,
        Shady,
        Fredemmott,
        Mtk,
    };

    // Options controlling how generate_wrapper_params / generate_call_args /
    // emit_forward_param_names treat the output slot and input-array pairs.
    // Used to share one implementation between the ResultCreate plain/vector/singular wrapper patterns.
    struct WrapperEmitOptions {
        enum class OutputMode {
            Skip,       // output absent from signature; caller uses a local var ('_throw' bodies)
            InPlace,    // output at its index as `Type& name` (existing _noThrow for ResultCreate)
            Trailing,   // output appended at end of signature via output_trailing_decl (vector / singular _noThrow)
        };
        enum class ArrayMode {
            Span,       // span<T> param; at call: count → .size(), array → .data()
            Singular,   // const T& param; at call: count → "1", array → "&singular_param_name"
        };
        OutputMode output = OutputMode::Skip;
        ArrayMode array = ArrayMode::Span;
        std::string_view output_trailing_decl; // full decl, e.g. "vector<Pipeline>& pPipelines" (Trailing only)
        std::string_view output_trailing_name; // forwarded in emit_forward_param_names (Trailing only)
        std::string_view output_expr;          // call_args: expression at output slot (overrides default &h / reinterpret_cast)
        std::string_view singular_param_name;  // ArrayMode::Singular: replaces the plural array name
        int skip_idx = -1;
    };

    struct HandleInfo {
        enum class Kind : uint8_t {
            InstanceItself, // VkInstance
            DeviceItself,   // VkDevice
            Instance,       // Owned by Instance (e.g. VkSurfaceKHR)
            Device,         // Owned by Device (e.g. VkBuffer)
        };

        enum class DestroyBehavior : uint8_t {
            None,       // No cleanup command (e.g. PhysicalDevice, Queue)
            Destroy,    // vkDestroy* — takes (parent, handle, alloc)
            Free,       // vkFree* — simple free like vkFreeMemory(device, mem, alloc)
            Release,    // vkRelease* — vkReleasePerformanceConfigurationINTEL
        };

        const Type& type;
        Kind kind;
        DestroyBehavior destroy_behavior = DestroyBehavior::None;
        const Command* destroy_command = nullptr; // the vkDestroy*/vkFree* command, if any

        bool is_destroyable() const { return destroy_command != nullptr; }
    };

    struct NameTranslator {
        static inline bool keep_av1_vp9 = true;

        struct TransformedEnumName {
            const std::string name;
            const Extension ext = Extension::None;
        };

        std::string new_name;

        NameTranslator(std::string&& new_name) : new_name(std::move(new_name)) {}

        static TransformedEnumName transform_enum_name(std::string_view name, bool is_bitmask);
        static NameTranslator from_enum_value(std::string_view value, const TransformedEnumName& enum_class_transformed, bool is_bitmask);
        static NameTranslator from_type_name(std::string_view name); // enums, structs, unions
        static NameTranslator from_constexpr_value(std::string_view value_name);
        static NameTranslator from_command_name(std::string_view name); // vkCreateBuffer -> createBuffer
        static NameTranslator from_input_array_name(std::string_view name); // pViewports -> viewports
        // vkCreateBuffer -> createBufferUnique, createBuffer; vkCreateSwapchainKHR -> createSwapchainKHRUnique, createSwapchainKHR
        static std::pair<std::string, std::string> unique_command_name(std::string_view name);
        // Returns the singular form, preserving any trailing all-uppercase extension suffix
        static std::string singularize(std::string_view name);

        static Extension get_and_remove_extension_name(std::string_view& name);
    protected:
        static void transform_from_upper_constant(std::string& name, size_t start_pos, bool first_is_upper);
        static Extension get_and_remove_extension_constant(std::string_view& name);
    };

    template <typename Type>
    concept NameIndexable = requires (Type t) { { t.name } -> std::convertible_to<std::string_view>; };

    class Generator {
        template <NameIndexable Type>
        class NameIndex {
            std::vector<Type*> names;
            std::unordered_map<std::string_view, std::size_t> nameIndex;

        public:
            bool contains(std::string_view name) const { return nameIndex.find(name) != nameIndex.end(); }
            void add(Type* type);
            void add_without_check(Type* type);
            void remove(std::string_view name);
            const std::vector<Type*>& get() const { return names; };
        };

        // Currently expecting only one Platforms tag
        Platforms platforms;

        // Currently expecting only one Tags tag
        Tags tags;

        // CHECK: Consider using unordered map, for faster lookup. Maybe even std::flat_map? Currently it does not seem that we have some performance problems.
        // Index of all the types defined in the <types> tags (with the category="enum" included)
        std::map<std::string_view, Type> types = {};
        // Index of all the enums defined in the <enums> tags
        std::map<std::string_view, TypeEnum> enums = {};
        // Index of all the commands defined in the <commands> tags
        std::map<std::string_view, Command> commands = {};
        std::map<std::string_view, CommandAlias> command_aliases = {};

        NameIndex<Type> required_types;
        NameIndex<TypeEnum> required_enums;
        // TODO: concider splitting Alias and Commands
        NameIndex<Command> required_commands;
        NameIndex<CommandAlias> required_commands_aliases;

        std::unordered_map<std::string_view, std::string_view> platform_to_protect; // platform name → protect macro
        std::unordered_set<std::string_view> included_platforms;

        // Names from <enums name="API Constants"> — populated by parse_enums.
        std::unordered_set<std::string_view> api_constants;

        std::unordered_map<std::string_view, xml::Element*> extension_name_to_element; // TASK: 110426_01
        std::unordered_set<xml::Element*> processed_extensions;
        std::unordered_set<xml::Element*> skipped_extensions;

        Config config;

        // TASK: 100126_01
        std::vector<DefineExt> required_defines;

        void parse_platforms(vkgen::xml::Dom& dom);
        void parse_types(vkgen::xml::Dom& dom);
        void parse_enums(vkgen::xml::Dom& dom);
        void parse_commands(vkgen::xml::Dom& dom);
        bool is_handle(std::string_view type);
        void cache_handles(std::vector<HandleInfo>& handles);


        void generate_enum(TypeEnum& enum_, std::ofstream& file);
        void generate_enum_alias(const Type& enum_, std::ofstream& file);
        void generate_to_cstr_decl(TypeEnum& enum_, std::ofstream& file);
        void generate_to_cstr_def(TypeEnum& enum_, std::ofstream& file);
        void generate_member(Member& member, std::ofstream& file, std::string_view struct_union, std::string_view parent_name);
        void generate_struct_union(const Type& type, std::ofstream& file, std::string_view struct_union);
        void generate_struct(const Type& struct_, std::ofstream& file);
        void generate_union(const Type& union_, std::ofstream& file);
        void generate_bitmask(const Type& bitmask, std::ofstream& file);
        void generate_handle(const Type& handle, std::ofstream& file, TypeEnum& obj_enum);
        void generate_error_classes(std::ofstream& file);


        std::ofstream& generate_command_params(Command& cmd, std::ofstream& file, bool is_end_of_line = true);
        void generate_command(Command& cmd, std::ofstream& file);
        void generate_command_PFN(Command& cmd, std::ofstream& file);
        void generate_command_wrapper(Command& cmd, std::ofstream& file);
        void generate_funcpointer(Type& type, std::ofstream& file);

        CommandClassification classify_command(Command& cmd);
        void detect_input_arrays(const Command& cmd, CommandClassification& cc);
        bool detect_struct_member_count(const Command& cmd, std::string_view len, CommandClassification& cc);
        bool has_pnext(std::string_view struct_type_name);

        bool should_emit_throw() const;
        bool should_emit_nothrow() const;
        bool should_emit_default() const;
        bool default_is_throw() const;

        // Emits a single parameter in C++ wrapper style (const struct ptr → const ref, VkHandle → Handle, else raw stringify)
        void emit_wrapper_param(const CommandParameter& param, std::ofstream& file);
        // Emits comma-separated param names for forwarding calls. skip_idx: extra index to skip (PD param)
        void emit_forward_param_names(const Command& cmd, const CommandClassification& cc,
            std::ofstream& file, bool include_nothrow_output = false, int skip_idx = -1);
        void emit_forward_param_names(const Command& cmd, const CommandClassification& cc,
            std::ofstream& file, const WrapperEmitOptions& opts);
        // Emits "(params, vector<ElemType>& v)" for enumerate _noThrow signatures. skip_idx for PD overloads.
        void emit_enumerate_nothrow_params(const Command& cmd, const CommandClassification& cc,
            std::ofstream& file, int skip_idx = -1);
        // Generates entire parameter list
        void generate_wrapper_params(const Command& cmd, const CommandClassification& cc,
            std::ofstream& file, bool for_nothrow_output = false, int skip_idx = -1);
        void generate_wrapper_params(const Command& cmd, const CommandClassification& cc,
            std::ofstream& file, const WrapperEmitOptions& opts);
        // Generates the argument list for a wrapper function call
        void generate_call_args(const Command& cmd, const CommandClassification& cc,
            std::ofstream& file, bool for_nothrow = false);
        void generate_call_args(const Command& cmd, const CommandClassification& cc,
            std::ofstream& file, const WrapperEmitOptions& opts);

        void generate_wrapper_void(const Command& cmd, const CommandClassification& cc, std::ofstream& file);
        void generate_wrapper_result_void(const Command& cmd, const CommandClassification& cc, std::ofstream& file);
        void generate_wrapper_result_create(const Command& cmd, const CommandClassification& cc, std::ofstream& file);
        void generate_wrapper_result_create_array(const Command& cmd, const CommandClassification& cc, std::ofstream& file);
        void generate_wrapper_void_outparam(const Command& cmd, const CommandClassification& cc, std::ofstream& file);
        void generate_wrapper_result_outparam(const Command& cmd, const CommandClassification& cc, std::ofstream& file);

        void generate_wrapper_enumerate_header(const Command& cmd, const CommandClassification& cc, std::ofstream& file);
        void generate_wrapper_enumerate_source(const Command& cmd, const CommandClassification& cc, std::ofstream& file);
        void generate_enumerate_call(const Command& cmd, const CommandClassification& cc, std::ofstream& file, bool second_call);
        void generate_throw_result_exception(std::ofstream& source);
        void generate_physical_device_overloads(const Command& cmd, const CommandClassification& cc, std::ofstream& file);
        void generate_physical_device_overload_alias(const CommandAlias& cmd, std::ofstream& file);
        void generate_unique_handle_create(const Command& cmd, const CommandClassification& cc, std::ofstream& file);


        bool is_device_level_command(const Command& cmd) const;

        struct HandleOrConstPtrStructArgument {
            bool is_handle = false;
            bool is_const_ptr_struct = false;
        };

        HandleOrConstPtrStructArgument is_handle_or_const_ptr_struct_argument(const CommandParameter& param);

        NameTranslator get_translated_type_name(std::string_view name);

        // Wrapper around add_required_type
        void add_required_type(std::string_view name);
        void add_required_type(std::string_view name, std::vector<Type*>& required_types);
        void add_required_enum(std::string_view name);
        void add_required_command(std::string_view name);


        void extend_enum(std::string_view extends, vkgen::xml::Element& elem, vkgen::Arena& arena, uint16_t block_ext_number = 0, std::string_view protect = {});
        // Handles feature-feature and feature-type dependencies
        void add_required_version_feature(std::string_view name, vkgen::xml::Dom& dom);
        // Handles extension-type dependencies, adds all types, enums and commands from this extension
        void add_extension_prototype(xml::Element& ext, xml::Dom& dom);
        // Handles extension-extension dependencies and checks the "allowlist"
        void add_extension_with_deps(xml::Element& ext, xml::Dom& dom);

        void generate_modules(std::ofstream& modules, const std::vector<HandleInfo>& handles);
    public:
        Generator() {}
        void generate(vkgen::xml::Dom& dom, std::ofstream& header, std::ofstream& source, std::ofstream& modules, const Config& config);

        // TODO: In future, these should be parsed to Generator structures and not used at all.
        //       Or they should be moved to some cache structure.
        xml::Node* Types;
        xml::Node* Enums;
        xml::Node* Commands;
        xml::Node* Feature;
        xml::Node* Extensions;
        xml::Node* Formats;
        xml::Node* Sync;
        xml::Node* VideoCodecs;
        xml::Node* SpirvExtensions;
        xml::Node* SpirvCapabilities;
    };

    struct Deprecate {
        static inline bool enabled;
        std::string_view msg;
    };
    struct LineComment {
        static inline bool enabled;
        std::string_view comment;
        bool alone = true;
    };

    struct StandaloneComment {
        static inline bool enabled;
        std::string_view comment;
    };

    template <typename Qual>
    concept IsTypeQualifier = std::same_as<Qual, TypeParam::PreQualifier> ||
        std::same_as<Qual, TypeParam::PostQualifier>;

    template <IsTypeQualifier Qual>
    constexpr Qual operator|(Qual a, Qual b) {
        return static_cast<Qual>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
    }

    template <IsTypeQualifier Qual>
    constexpr Qual operator&(Qual a, Qual b) {
        return static_cast<Qual>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
    }

    template <IsTypeQualifier Qual>
    constexpr Qual& operator|=(Qual& a, Qual b) {
        return a = a | b;
    }

    inline bool TypeParam::is_const() const { return bool(pre_qual & PreQualifier::Const); };

    inline std::ostream& operator<<(std::ostream& os, const NameTranslator& translator) {
        return os << translator.new_name;
    }

    inline std::ostream& operator<<(std::ostream& os, Deprecate d) {
        if (Deprecate::enabled && !d.msg.empty()) {
            return os << " [[deprecated(\"" << d.msg << "\")]] ";
        }
        return os << ' ';
    };

    inline std::ostream& operator<<(std::ostream& os, LineComment c) {
        if (LineComment::enabled && !c.comment.empty()) {
            if (c.alone)
                os << "// " << c.comment << '\n';
            else
                os << " // " << c.comment;
        }
        return os;
    };

    inline std::ostream& operator<<(std::ostream& os, StandaloneComment c) {
        if (StandaloneComment::enabled && !c.comment.empty()) {
            os << "/* " << c.comment << " */\n";
        }
        return os;
    };

    bool bool_from_string(std::string_view s);
    Command::Scope scope_from_string(std::string_view s);

    template<NameIndexable Type>
    inline void Generator::NameIndex<Type>::add(Type* type) {
        if (contains(type->name))
            return;

        add_without_check(type);
    }

    template<NameIndexable Type>
    inline void Generator::NameIndex<Type>::add_without_check(Type* type) {
        nameIndex[type->name] = names.size();
        names.push_back(type);
    }

    template<NameIndexable Type>
    inline void Generator::NameIndex<Type>::remove(std::string_view name) {
        auto it = nameIndex.find(name);
        if (it != nameIndex.end()) {
            names[it->second] = nullptr;
            nameIndex.erase(it);
        }
    }

    inline bool Generator::should_emit_throw() const {
        return config.exception_behavior != ExceptionBehavior::NoThrowOnly;
    }
    inline bool Generator::should_emit_nothrow() const {
        return config.exception_behavior != ExceptionBehavior::ThrowOnly;
    }
    inline bool Generator::should_emit_default() const {
        return config.exception_behavior == ExceptionBehavior::BothWithDefaultThrow
            || config.exception_behavior == ExceptionBehavior::BothWithDefaultNoThrow;
    }
    inline bool Generator::default_is_throw() const {
        return config.exception_behavior == ExceptionBehavior::BothWithDefaultThrow;
    }

    inline Generator::HandleOrConstPtrStructArgument Generator::is_handle_or_const_ptr_struct_argument(const CommandParameter& param) {
        Type::Category cat = types.at(param.type_param.type).category;
        bool param_is_handle = config.generate_handle_class && cat == Type::Category::Handle;
        bool is_const_struct_ptr = param.type_param.is_const()
            && param.type_param.post_quals.size() == 1
            && bool(param.type_param.post_quals[0] & TypeParam::PostQualifier::Pointer) // TODO: can't use PostQualifier::ConstPointer because the const qualifier is before type name (in pre-qualifiers)
            && !param_is_handle
            && (cat == Type::Category::Struct || cat == Type::Category::Union);
        return { param_is_handle, is_const_struct_ptr };
    }

} // namespace vkgen::Generator

