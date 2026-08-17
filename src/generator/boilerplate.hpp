/**
 * @file boilerplate.hpp
 * @author Jaroslav Hucel (xhucel00@vutbr.cz)
 * @brief Pregenerated boilerplate code for the generator.
 * @date Created: 23. 03. 2026
 * @date Modified: 17. 08. 2026
 *
 * @copyright Copyright (c) 2025 -> Public Domain, for more information see LICENSE
 */

#pragma once
#include <array>
#include <ostream>
#include <string_view>

namespace boilerplate {
    using namespace std::string_view_literals; // for ""sv string literals

    // author: PCJohn (peciva at fit.vut.cz)
    static constexpr std::array HANDLE_DEFINITION = {
        "template<typename Type> class UniqueHandle;\n\n"sv,

        "template<typename T>\n"sv,
        "class Handle {\n"sv,
        "    protected:\n"sv,
        "        T _handle;\n"sv,
        "    public:\n"sv,
        "        using HandleType = T;\n\n"sv,

        "        Handle() noexcept {}\n"sv,
        "        Handle(std::nullptr_t) noexcept : _handle(nullptr) {}\n"sv,
        "        Handle(T nativeHandle) noexcept : _handle(nativeHandle) {}\n"sv,
        "        Handle(const Handle& h) noexcept : _handle(h._handle) {}\n"sv,
        "        Handle(const UniqueHandle<Handle<T>>& u) noexcept : _handle(u.get().handle()) {}\n"sv,
        "        Handle& operator=(const Handle rhs) noexcept { _handle = rhs._handle; return *this; }\n"sv,
        "        Handle& operator=(const UniqueHandle<T>& rhs) noexcept { _handle = rhs._handle; return *this; }\n"sv,
        "        T handle() const noexcept { return _handle; }\n"sv,
        "        explicit operator bool() const noexcept { return _handle != nullptr; }\n"sv,
        "        bool operator==(const Handle rhs) const noexcept { return _handle == rhs._handle; }\n"sv,
        "        bool operator!=(const Handle rhs) const noexcept { return _handle != rhs._handle; }\n"sv,
        "};\n"sv,
    };

    // original author: PCJohn (peciva at fit.vut.cz) modified by Jaroslav Hucel
    static constexpr std::array FLAGS_DEFINITION = {
        "template <typename BitType>\n"sv,
        "class Flags {\n"sv,
        "    public:\n\n"sv,

        "    using ValueType = __underlying_type(BitType);\n"sv, // This is a change from the original code, this compiler intrinsics is supported by GCC, Clang and MSVC

        "   protected:\n"sv,
        "       ValueType _value;\n"sv,
        "   public:\n\n"sv,

        "   // constructors\n"sv,
        "   constexpr Flags() noexcept : _value(0) {}\n"sv,
        "   constexpr Flags(BitType bit) noexcept : _value(static_cast<ValueType>(bit)) {}\n"sv,
        "   constexpr Flags(const Flags<BitType>& other) noexcept = default;\n"sv,
        "   constexpr explicit Flags(ValueType flags) noexcept : _value(flags) {}\n\n"sv,

        "   // relational operators\n"sv,
        "   constexpr bool operator< (const Flags<BitType>& rhs) const noexcept { return _value < rhs._value; }\n"sv,
        "   constexpr bool operator<=(const Flags<BitType>& rhs) const noexcept { return _value <= rhs._value; }\n"sv,
        "   constexpr bool operator> (const Flags<BitType>& rhs) const noexcept { return _value > rhs._value; }\n"sv,
        "   constexpr bool operator>=(const Flags<BitType>& rhs) const noexcept { return _value >= rhs._value; }\n"sv,
        "   constexpr bool operator==(const Flags<BitType>& rhs) const noexcept { return _value == rhs._value; }\n"sv,
        "   constexpr bool operator!=(const Flags<BitType>& rhs) const noexcept { return _value != rhs._value; }\n\n"sv,

        "   // logical operator\n"sv,
        "   constexpr bool operator!() const noexcept { return !_value; }\n\n"sv,

        "   // bitwise operators\n"sv,
        "   constexpr Flags<BitType> operator&(const Flags<BitType>& rhs) const noexcept { return Flags<BitType>(_value & rhs._value); }\n"sv,
        "   constexpr Flags<BitType> operator|(const Flags<BitType>& rhs) const noexcept { return Flags<BitType>(_value | rhs._value); }\n"sv,
        "   constexpr Flags<BitType> operator^(const Flags<BitType>& rhs) const noexcept { return Flags<BitType>(_value ^ rhs._value); }\n\n"sv,

        "   // assignment operators\n"sv,
        "   constexpr Flags<BitType>& operator= (const Flags<BitType>& rhs) noexcept = default;\n"sv,
        "   constexpr Flags<BitType>& operator|=(const Flags<BitType>& rhs) noexcept { _value |= rhs._value; return *this; }\n"sv,
        "   constexpr Flags<BitType>& operator&=(const Flags<BitType>& rhs) noexcept { _value &= rhs._value; return *this; }\n"sv,
        "   constexpr Flags<BitType>& operator^=(const Flags<BitType>& rhs) noexcept { _value ^= rhs._value; return *this; }\n\n"sv,

        "   // cast operators\n"sv,
        "   explicit constexpr operator bool() const noexcept { return !!_value; }\n"sv,
        "   explicit constexpr operator ValueType() const noexcept { return _value; }\n"sv,
        "};\n\n"sv,

        // Code below is not from the original author
        "// These specializations are used for dummy flags, that don't have any bits set defined\n"sv,
        "template <>\n"sv,
        "class Flags<uint32_t> {\n"sv,
        "    uint32_t _value;\n"sv,
        "public:\n"sv,
        "    constexpr Flags(uint32_t v = 0) noexcept : _value(v) {}\n"sv,
        "    constexpr operator uint32_t() const noexcept { return _value; }\n"sv,
        "};\n\n"sv,

        "template <>\n"sv,
        "class Flags<uint64_t> {\n"sv,
        "    uint64_t _value;\n"sv,
        "public:\n"sv,
        "    constexpr Flags(uint64_t v = 0) noexcept : _value(v) {}\n"sv,
        "    constexpr operator uint64_t() const noexcept { return _value; }\n"sv,
        "};\n\n"sv,

        // Original author: PCJohn (peciva at fit.vut.cz)
        "template <typename BitType>\n"sv,
        "constexpr Flags<BitType> operator|(BitType lhs, BitType rhs) noexcept { return Flags<BitType>(lhs) | rhs; }\n"sv,
        "template <typename BitType>\n"sv,
        "constexpr Flags<BitType> operator|(BitType bit, const Flags<BitType>& flags) noexcept { return flags | bit; }\n"sv,
    };

    // author: PCJohn (peciva at fit.vut.cz)
    static constexpr std::string_view CPP_INCLUDES = ""
        "#include <cassert>\n"
        "#include <cstdlib>\n"
        "#include <string>\n"
        "#include <filesystem>\n"
        "#ifdef _WIN32\n"
        "# define WIN32_LEAN_AND_MEAN  // this reduces win32 headers default namespace pollution\n"
        "# include <windows.h>\n"
        "#else\n"
        "# include <dlfcn.h>\n"
        "#endif\n"sv;


    // author: PCJohn (peciva at fit.vut.cz)
    static constexpr std::string_view UNIQUE_HANDLE_DEFINITION =
        "template<typename Type>\n"
        "class UniqueHandle {\n"
        "protected:\n"
        "    Type _value;\n"
        "public:\n"
        "    UniqueHandle() : _value(nullptr) {}\n"
        "    explicit UniqueHandle(Type value) noexcept : _value(value) {}\n"
        "    UniqueHandle(const UniqueHandle&) = delete;\n"
        "    UniqueHandle(UniqueHandle&& other) noexcept : _value(other._value) { other._value = nullptr; }\n"
        "    inline ~UniqueHandle() { destroy(_value); }\n"
        "    UniqueHandle& operator=(const UniqueHandle&) = delete;\n"
        "    UniqueHandle& operator=(UniqueHandle&& other) noexcept { reset(other._value); other._value = nullptr; return *this; }\n"
        "    Type get() const noexcept { return _value; }\n"
        "    void reset(Type value = nullptr) noexcept { if (value == _value) return; destroy(_value); _value = value; }\n"
        "    Type release() noexcept { Type r = _value; _value = nullptr; return r; }\n"
        "    void swap(UniqueHandle& other) { Type tmp = _value; _value = other._value; other._value = tmp; }\n"
        "    explicit operator Type() const noexcept { return _value; }\n"
        "    explicit operator bool() const noexcept { return _value != nullptr; }\n"
        "};\n\n"sv;


    static constexpr std::array CUSTOM_SPAN_DEFINITION = {
        "// Helper class for eliminating unnecessary overloads, does not own the data\n"sv,
        "// Type is expected to be primitive type, vulkan struct or custom handle class\n"sv,
        "template<typename Type>\n"sv,
        "class span {\n"sv,
        "    const Type* _data;\n"sv,
        "    uint32_t _size;\n\n"sv,

        "public:\n"sv,
        "    constexpr span(const Type* data, size_t size) : _data(data), _size(uint32_t(size)) {}\n"sv,
        "    constexpr span(const Type& data) : _data(&data), _size(1) {}\n\n"sv,

        "    // Any contiguous container of Type: vk::vector, std::array, std::vector, std::span, ...\n"sv,
        "    // The span does not own the data, so the container must outlive it\n"sv,
        "    template<typename Container>\n"sv,
        "        requires requires(const Container& c) {\n"sv,
        "            static_cast<const Type*>(c.data());\n"sv,
        "            static_cast<size_t>(c.size());\n"sv,
        "        }\n"sv,
        "    constexpr span(const Container& data) noexcept : _data(data.data()), _size(uint32_t(data.size())) {}\n\n"sv,

        "    // If Type is handle, add UniqueHandle overloads\n"sv,
        "    template <typename U = Type, typename = detail::enable_if_t<detail::is_handle<U>::value>>\n"sv,
        "    constexpr span(const UniqueHandle<U>& data) noexcept : _data(reinterpret_cast<const Type*>(&data)), _size(1) {}\n"sv,
        "    template <typename U = Type, typename Container>\n"sv,
        "        requires detail::is_handle<U>::value && requires(const Container& c) {\n"sv,
        "            static_cast<const UniqueHandle<U>*>(c.data());\n"sv,
        "            static_cast<size_t>(c.size());\n"sv,
        "        }\n"sv,
        "    constexpr span(const Container& data) noexcept : _data(reinterpret_cast<const Type*>(data.data())), _size(uint32_t(data.size())) {}\n\n"sv,

        "    const typename detail::native_type<Type>::type* data() const noexcept {\n"sv,
        "        return reinterpret_cast<const typename detail::native_type<Type>::type*>(_data);\n"sv,
        "    }\n\n"sv,

        "    constexpr uint32_t size() const noexcept { return _size; }\n"sv,
        "};\n"sv
    };


    // author: PCJohn (peciva at fit.vut.cz)
    static constexpr std::string_view DETAIL_NAMESPACE =
        "namespace detail {\n"
        "    extern void* _library;\n"
        "    extern Instance _instance;\n"
        "    extern PhysicalDevice _physicalDevice;\n"
        "    extern Device _device;\n"
        "    extern uint32_t _instanceVersion;\n"
        "}\n\n"

        "inline void* library() { return detail::_library; }\n"
        "inline Instance instance() { return detail::_instance; }\n"
        "inline PhysicalDevice physicalDevice() { return detail::_physicalDevice; }\n"
        "inline Device device() { return detail::_device; }\n"
        "inline uint32_t enumerateInstanceVersion() { return detail::_instanceVersion; }\n\n"sv;

    // author: PCJohn (peciva at fit.vut.cz)
    static constexpr std::string_view IS_EXTENSION_SUPPORTED_DEF =
        "inline bool isExtensionSupported(const vector<ExtensionProperties>& extensionList, const char* extensionName) noexcept {\n"
        "    for (size_t i = 0, c = extensionList.size(); i < c; i++)\n"
        "        if (strcmp(extensionList[i].extensionName, extensionName) == 0)\n"
        "            return true;\n"
        "    return false;\n"
        "}\n"sv;


    // author: PCJohn (peciva at fit.vut.cz)
    static constexpr std::string_view INIT_DECLARATIONS =
        "void loadLib_throw();\n"
        "Result loadLib_noThrow() noexcept;\n"
        "inline void loadLib() { loadLib_throw(); }\n\n" // TODO: respect config with the throw_nothrow behavior
        "void loadLib_throw(const char* libPath);\n"
        "Result loadLib_noThrow(const char* libPath) noexcept;\n"
        "inline void loadLib(const char* libPath) { loadLib_throw(libPath); }\n\n"
        "void initInstance_throw(const InstanceCreateInfo& createInfo);\n"
        "Result initInstance_noThrow(const InstanceCreateInfo& createInfo) noexcept;\n"
        "inline void initInstance(const InstanceCreateInfo& createInfo) { initInstance_throw(createInfo); }\n"
        "void initInstance(Instance instance) noexcept;\n\n"
        "void initDevice_throw(PhysicalDevice pd, const DeviceCreateInfo& createInfo);\n"
        "Result initDevice_noThrow(PhysicalDevice pd, const DeviceCreateInfo& createInfo) noexcept;\n"
        "inline void initDevice(PhysicalDevice pd, const DeviceCreateInfo& createInfo) { initDevice_throw(pd, createInfo); }\n"
        "void initDevice(PhysicalDevice physicalDevice, Device device) noexcept;\n\n"
        "template<typename Qual> inline Qual getInstanceProcAddr(const char* name) noexcept;\n"
        "template<typename Qual> inline Qual getDeviceProcAddr(const char* name) noexcept;\n"
        "void destroyDevice() noexcept;\n"
        "void destroyInstance() noexcept;\n"
        "void unloadLib() noexcept;\n"
        "void cleanUp() noexcept;\n\n"sv;

    // original author: PCJohn (peciva at fit.vut.cz), added inline for modules support
    static constexpr std::string_view VERSION_HELPERS =
        "constexpr uint32_t makeApiVersion(uint32_t variant, uint32_t major, uint32_t minor, uint32_t patch) { return (variant << 29) | (major << 22) | (minor << 12) | patch; }\n"
        "constexpr uint32_t makeApiVersion(uint32_t major, uint32_t minor, uint32_t patch) { return (major << 22) | (minor << 12) | patch; }\n"
        "inline constexpr const uint32_t ApiVersion10 = makeApiVersion(0, 1, 0, 0);\n"
        "inline constexpr const uint32_t ApiVersion11 = makeApiVersion(0, 1, 1, 0);\n"
        "inline constexpr const uint32_t ApiVersion12 = makeApiVersion(0, 1, 2, 0);\n"
        "inline constexpr const uint32_t ApiVersion13 = makeApiVersion(0, 1, 3, 0);\n"
        "inline constexpr const uint32_t ApiVersion14 = makeApiVersion(0, 1, 4, 0);\n"
        "constexpr uint32_t apiVersionVariant(uint32_t version) { return version >> 29; }\n"
        "constexpr uint32_t apiVersionMajor(uint32_t version) { return (version >> 22) & 0x7f; }\n"
        "constexpr uint32_t apiVersionMinor(uint32_t version) { return (version >> 12) & 0x3ff; }\n"
        "constexpr uint32_t apiVersionPatch(uint32_t version) { return version & 0xfff; }\n\n"sv;

    // author: PCJohn (peciva at fit.vut.cz)
    static constexpr std::string_view ERROR_BASE_CLASS =
        "void throwResultExceptionWithMessage(Result result, const char* message);\n"
        "void throwResultException(Result result, const char* funcName);\n"
        "inline void checkForSuccessValue(Result result, const char* funcName) { if (result != Result::eSuccess) throwResultException(result, funcName); }\n"
        "inline void checkSuccess(Result result, const char* funcName) { if (int32_t(result) < 0) throwResultException(result, funcName); }\n\n"
        "class Error {\n"
        "protected:\n"
        "    char* _msg = nullptr;\n"
        "public:\n"
        "    Error() noexcept = default;\n"
        "    Error(const char* message) noexcept { if (message == nullptr) { _msg = nullptr; return; } size_t n = strlen(message) + 1; _msg = reinterpret_cast<char*>(malloc(n)); if (_msg) strncpy(_msg, message, n); }\n"
        "    Error(const char* msgHeader, const char* msgBody) noexcept;\n"
        "    Error(const char* funcName, Result result) noexcept;\n"
        "    Error(const Error& other) noexcept { delete[] _msg; if (other._msg == nullptr) { _msg = nullptr; return; } size_t n = strlen(other._msg) + 1; _msg = reinterpret_cast<char*>(malloc(n)); if (_msg) strncpy(_msg, other._msg, n); }\n"
        "    Error& operator=(const Error& rhs) noexcept { if (rhs._msg == nullptr) { _msg = nullptr; return *this; } size_t n = strlen(rhs._msg) + 1; _msg = reinterpret_cast<char*>(malloc(n)); if (_msg) strncpy(_msg, rhs._msg, n); return *this; }\n"
        "    virtual ~Error() noexcept { delete[] _msg; }\n"
        "    virtual const char* what() const noexcept { return _msg ? _msg : \"Unknown exception\"; }\n"
        "};\n\n"
        "class VkgError : public Error { public: using Error::Error; };\n\n"sv;

    // author: PCJohn (peciva at fit.vut.cz)
    static constexpr std::string_view PROC_ADDR_HELPERS =
        "template<typename Qual> inline Qual getGlobalProcAddr(const char* name) noexcept { return reinterpret_cast<Qual>(funcs.vkGetInstanceProcAddr(nullptr, name)); }\n"
        "template<typename Qual> inline Qual getInstanceProcAddr(const char* name) noexcept { return reinterpret_cast<Qual>(funcs.vkGetInstanceProcAddr(detail::_instance.handle(), name)); }\n"
        "template<typename Qual> inline Qual getDeviceProcAddr(const char* name) noexcept { return reinterpret_cast<Qual>(funcs.vkGetDeviceProcAddr(detail::_device.handle(), name)); }\n\n"sv;

    // author: PCJohn (peciva at fit.vut.cz)
    static constexpr std::string_view PROCESS_RESULT_TEMPLATE =
        "namespace detail {\n"
        "    template<typename Qual> void processResult(Result r, Qual& handle, const char* functionName) {"
        " if (r > Result::eSuccess) { destroy(handle); handle = nullptr; }"
        " if (r != Result::eSuccess) throwResultException(r, functionName); }\n"
        "}\n\n"sv;

    static constexpr std::string_view CPP_IMPL =
        "void* detail::_library = nullptr;\n"
        "Instance detail::_instance = nullptr;\n"
        "PhysicalDevice detail::_physicalDevice = nullptr;\n"
        "Device detail::_device = nullptr;\n"
        "uint32_t detail::_instanceVersion = 0;\n"
        "const AllocationCallbacks* detail::_allocator = nullptr;\n"
        "Funcs detail::_funcs;\n\n"

        // author: PCJohn (peciva at fit.vut.cz)
        "Error::Error(const char* msgHeader, const char* msgBody) noexcept {\n"
        "    size_t l1 = strlen(msgHeader); size_t l2 = strlen(msgBody);\n"
        "    _msg = reinterpret_cast<char*>(malloc(l1 + l2 + 1));\n"
        "    if (_msg) { memcpy(_msg, msgHeader, l1); memcpy(_msg + l1, msgBody, l2 + 1); }\n"
        "}\n"
        "Error::Error(const char* funcName, Result) noexcept {\n"
        "    if (funcName) { size_t n = strlen(funcName) + 1; _msg = reinterpret_cast<char*>(malloc(n)); if (_msg) strncpy(_msg, funcName, n); }\n"
        "}\n\n"sv;


    // This code is modified. original author: PCJohn (peciva at fit.vut.cz)
    static constexpr std::string_view LOAD_UNLOAD_LIB_IMPL =
        "void loadLib_throw() {\n"
        "#ifdef _WIN32\n"
        "    loadLib_throw(\"vulkan-1.dll\");\n"
        "#else\n"
        "    loadLib_throw(\"libvulkan.so.1\");\n"
        "#endif\n"
        "}\n"
        "\n"
        "\n"
        "Result loadLib_noThrow() noexcept {\n"
        "#ifdef _WIN32\n"
        "    return loadLib_noThrow(\"vulkan-1.dll\");\n"
        "#else\n"
        "    return loadLib_noThrow(\"libvulkan.so.1\");\n"
        "#endif\n"
        "}\n"
        "\n"
        "\n"
        "void loadLib_throw(const char* libPath) {\n"
        "    // avoid multiple initialization attempts\n"
        "    if (detail::_library)\n"
        "        throw VkgError(\"Vulkan error: Multiple initialization attempts.\");\n"
        "\n"
        "    // load library\n"
        "    // and get vkGetInstanceProcAddr pointer\n"
        "    std::filesystem::path p = std::filesystem::path(libPath);\n"
        "#ifdef _WIN32\n"
        "    detail::_library = reinterpret_cast<void*>(LoadLibraryW(p.native().c_str()));\n"
        "    if (detail::_library == nullptr)\n"
        "        throw VkgError((std::string(\"Vulkan error: Can not open \\\"\") + p.string() + \"\\\".\").c_str());\n"
        "    funcs.vkGetInstanceProcAddr = PFN_vkGetInstanceProcAddr(\n"
        "        GetProcAddress(reinterpret_cast<HMODULE>(detail::_library), \"vkGetInstanceProcAddr\"));\n"
        "#else\n"
        "    detail::_library = dlopen(p.native().c_str(), RTLD_NOW);\n"
        "    if (detail::_library == nullptr)\n"
        "        throw VkgError((std::string(\"Vulkan error: Can not open \\\"\") + p.native() + \"\\\".\").c_str());\n"
        "    funcs.vkGetInstanceProcAddr = PFN_vkGetInstanceProcAddr(dlsym(detail::_library, \"vkGetInstanceProcAddr\"));\n"
        "#endif\n"
        "    if (funcs.vkGetInstanceProcAddr == nullptr) {\n"
        "        unloadLib();\n"
        "        throw VkgError((std::string(\"Vulkan error: Can not retrieve vkGetInstanceProcAddr function pointer out of \\\"\") + p.string() + \".\").c_str());\n"
        "    }\n"
        "\n"
        "    // function pointers available without vk::Instance\n"
        "    funcs.vkEnumerateInstanceExtensionProperties = getInstanceProcAddr<PFN_vkEnumerateInstanceExtensionProperties>(\"vkEnumerateInstanceExtensionProperties\");\n"
        "    funcs.vkEnumerateInstanceLayerProperties = getInstanceProcAddr<PFN_vkEnumerateInstanceLayerProperties>(\"vkEnumerateInstanceLayerProperties\");\n"
        "    funcs.vkCreateInstance = getInstanceProcAddr<PFN_vkCreateInstance>(\"vkCreateInstance\");\n"
        "    PFN_vkEnumerateInstanceVersion vkEnumerateInstanceVersion = getInstanceProcAddr<PFN_vkEnumerateInstanceVersion>(\"vkEnumerateInstanceVersion\");\n"
        "\n"
        "    // instance version\n"
        "    if (vkEnumerateInstanceVersion) {\n"
        "        uint32_t v;\n"
        "        Result r = vkEnumerateInstanceVersion(&v);\n"
        "        if (r != Result::eSuccess) {\n"
        "            unloadLib();\n"
        "            throwResultException(r, \"vkEnumerateInstanceVersion\");\n"
        "        }\n"
        "        detail::_instanceVersion = v;\n"
        "    } else\n"
        "        detail::_instanceVersion = ApiVersion10;\n"
        "}\n"
        "\n"
        "\n"
        "Result loadLib_noThrow(const char* libPath) noexcept {\n"
        "    // avoid multiple initialization attempts\n"
        "    if (detail::_library)\n"
        "        return Result::eErrorUnknown;\n"
        "\n"
        "    // load library\n"
        "    // and get vkGetInstanceProcAddr pointer\n"
        "    std::filesystem::path p = std::filesystem::path(libPath);\n"
        "#ifdef _WIN32\n"
        "    detail::_library = reinterpret_cast<void*>(LoadLibraryW(p.native().c_str()));\n"
        "    if (detail::_library == nullptr)\n"
        "        return Result::eErrorInitializationFailed;\n"
        "    funcs.vkGetInstanceProcAddr = PFN_vkGetInstanceProcAddr(\n"
        "        GetProcAddress(reinterpret_cast<HMODULE>(detail::_library), \"vkGetInstanceProcAddr\"));\n"
        "#else\n"
        "    detail::_library = dlopen(p.native().c_str(), RTLD_NOW);\n"
        "    if (detail::_library == nullptr)\n"
        "        return Result::eErrorInitializationFailed;\n"
        "    funcs.vkGetInstanceProcAddr = PFN_vkGetInstanceProcAddr(dlsym(detail::_library, \"vkGetInstanceProcAddr\"));\n"
        "#endif\n"
        "    if (funcs.vkGetInstanceProcAddr == nullptr) {\n"
        "        unloadLib();\n"
        "        return Result::eErrorIncompatibleDriver;\n"
        "    }\n"
        "\n"
        "    // function pointers available without vk::Instance\n"
        "    funcs.vkEnumerateInstanceExtensionProperties = getInstanceProcAddr<PFN_vkEnumerateInstanceExtensionProperties>(\"vkEnumerateInstanceExtensionProperties\");\n"
        "    funcs.vkEnumerateInstanceLayerProperties = getInstanceProcAddr<PFN_vkEnumerateInstanceLayerProperties>(\"vkEnumerateInstanceLayerProperties\");\n"
        "    funcs.vkCreateInstance = getInstanceProcAddr<PFN_vkCreateInstance>(\"vkCreateInstance\");\n"
        "    PFN_vkEnumerateInstanceVersion vkEnumerateInstanceVersion = getInstanceProcAddr<PFN_vkEnumerateInstanceVersion>(\"vkEnumerateInstanceVersion\");\n"
        "\n"
        "    // instance version\n"
        "    if (vkEnumerateInstanceVersion) {\n"
        "        uint32_t v;\n"
        "        Result r = vkEnumerateInstanceVersion(&v);\n"
        "        if (r != Result::eSuccess) {\n"
        "            unloadLib();\n"
        "            return r;\n"
        "        }\n"
        "        detail::_instanceVersion = v;\n"
        "    } else\n"
        "        detail::_instanceVersion = ApiVersion10;\n"
        "\n"
        "    return Result::eSuccess;\n"
        "}\n"
        "\n"
        "void unloadLib() noexcept {\n"
        "    if (detail::_library) {\n"
        "#ifdef _WIN32\n"
        "        FreeLibrary(reinterpret_cast<HMODULE>(detail::_library));\n"
        "#else\n"
        "        dlclose(detail::_library);\n"
        "#endif\n"
        "        detail::_library = nullptr;\n"
        "    }\n"
        "}\n"
        "\n"
        "void cleanUp() noexcept {\n"
        "    destroyDevice();\n"
        "    destroyInstance();\n"
        "    unloadLib();\n"
        "}\n\n"
        "void destroyDevice() noexcept {\n"
        "    if (detail::_device) {\n"
        "        funcs.vkDestroyDevice(detail::_device.handle(), detail::_allocator);\n"
        "        detail::_device = nullptr;\n"
        "    }\n"
        "}\n\n"
        "void destroyInstance() noexcept {\n"
        "    if (detail::_instance) {\n"
        "        funcs.vkDestroyInstance(detail::_instance.handle(), detail::_allocator);\n"
        "        detail::_instance = nullptr;\n"
        "    }\n"
        "}\n"sv;

    // This code is modified. original author: PCJohn (peciva at fit.vut.cz)
    static constexpr std::string_view INIT_INSTANCE_DEVICE_IMPL =
        "void initInstance_throw(const InstanceCreateInfo& createInfo) {\n"
        "    assert(detail::_library && \"vk::loadLib() must be called before vk::initInstance().\");\n"
        "    destroyInstance();\n"
        "    Instance::HandleType instanceHandle;\n"
        "    Result r = funcs.vkCreateInstance(&createInfo, detail::_allocator, &instanceHandle);\n"
        "    checkForSuccessValue(r, \"vkCreateInstance\");\n"
        "    initInstance(instanceHandle);\n"
        "}\n\n"
        "Result initInstance_noThrow(const InstanceCreateInfo& createInfo) noexcept {\n"
        "    assert(detail::_library && \"vk::loadLib() must be called before vk::initInstance().\");\n"
        "    destroyInstance();\n"
        "    Instance::HandleType instanceHandle;\n"
        "    Result r = funcs.vkCreateInstance(&createInfo, detail::_allocator, &instanceHandle);\n"
        "    if (r != Result::eSuccess) return r;\n"
        "    initInstance(instanceHandle);\n"
        "    return Result::eSuccess;\n"
        "}\n\n"
        "void initInstance(Instance instance) noexcept {\n"
        "    assert(detail::_library && \"vk::loadLib() must be called before vk::initInstance().\");\n"
        "    destroyInstance();\n"
        "    detail::_instance = instance;\n"
        "    initInstancePFNs();\n"
        "}\n\n"
        "void initDevice_throw(PhysicalDevice pd, const DeviceCreateInfo& createInfo) {\n"
        "    assert(detail::_instance && \"vk::initInstance() must be called before vk::initDevice().\");\n"
        "    destroyDevice();\n"
        "    Device::HandleType deviceHandle;\n"
        "    Result r = funcs.vkCreateDevice(pd.handle(), &createInfo, detail::_allocator, &deviceHandle);\n"
        "    checkForSuccessValue(r, \"vkCreateDevice\");\n"
        "    initDevice(pd, deviceHandle);\n"
        "}\n\n"
        "Result initDevice_noThrow(PhysicalDevice pd, const DeviceCreateInfo& createInfo) noexcept {\n"
        "    assert(detail::_instance && \"vk::initInstance() must be called before vk::initDevice().\");\n"
        "    destroyDevice();\n"
        "    Device::HandleType deviceHandle;\n"
        "    Result r = funcs.vkCreateDevice(pd.handle(), &createInfo, detail::_allocator, &deviceHandle);\n"
        "    if (r != Result::eSuccess) return r;\n"
        "    initDevice(pd, deviceHandle);\n"
        "    return Result::eSuccess;\n"
        "}\n\n"
        "void initDevice(PhysicalDevice physicalDevice, Device device) noexcept {\n"
        "    assert(detail::_instance && \"vk::initInstance() must be called before vk::initDevice().\");\n"
        "    destroyDevice();\n"
        "    detail::_physicalDevice = physicalDevice;\n"
        "    detail::_device = device;\n"
        "    initDevicePFNs();\n"
        "}\n\n"sv;

    // author: PCJohn (peciva at fit.vut.cz)
    static constexpr std::string_view CUSTOM_VECTOR_DECL = ""
        "      template<typename Type>\n"
        "class iterator {\n"
        "public:\n"
        "    using pointer = Type*;\n"
        "    using reference = Type&;\n"
        "protected:\n"
        "    pointer _p;\n"
        "public:\n"
        "    iterator() : _p(nullptr) {}\n"
        "    iterator(pointer p) : _p(p) {}\n"
        "    iterator(reference r) : _p(&r) {}\n"
        "    pointer operator->() const { return _p; }\n"
        "    reference operator*() { return *_p; }\n"
        "    reference operator[](std::ptrdiff_t index) const { return *(_p + index); }\n"
        "    iterator& operator++() { _p++; return *this; }\n"
        "    iterator& operator--() { _p--; return *this; }\n"
        "    iterator  operator++(int) { iterator it(*this); _p++; return it; }\n"
        "    iterator  operator--(int) { iterator it(*this); _p--; return it; }\n"
        "    iterator& operator+=(std::ptrdiff_t n) { _p += n; return *this; }\n"
        "    iterator& operator-=(std::ptrdiff_t n) { _p -= n; return *this; }\n"
        "    iterator operator+(std::ptrdiff_t n) const { return iterator(_p + n); }\n"
        "    iterator operator-(std::ptrdiff_t n) const { return iterator(_p - n); }\n"
        "    std::ptrdiff_t operator-(iterator rhs) const { return _p - rhs._p; }\n"
        "    bool operator<(const iterator rhs) const { return _p < rhs._p; }\n"
        "    bool operator>(const iterator rhs) const { return _p > rhs._p; }\n"
        "    bool operator<=(const iterator rhs) const { return _p <= rhs._p; }\n"
        "    bool operator>=(const iterator rhs) const { return _p >= rhs._p; }\n"
        "    bool operator==(const iterator rhs) const { return _p == rhs._p; }\n"
        "    bool operator!=(const iterator rhs) const { return _p != rhs._p; }\n"
        "};\n"
        "\n"
        "// vector class\n"
        "template<typename Type>\n"
        "class vector {\n"
        "protected:\n"
        "    Type* _data;\n"
        "    size_t _size;\n"
        "public:\n"
        "    using value_type = Type;\n"
        "    using reference = Type&;\n"
        "    using const_reference = const Type&;\n"
        "    using iterator = vk::iterator<Type>;\n"
        "    using const_iterator = vk::iterator<Type>;\n"
        "\n"
        "    vector() noexcept : _data(nullptr), _size(0) {}\n"
        "    vector(size_t size) : _data(new Type[size]), _size(size) {}\n"
        "    vector(Type* data, size_t size) noexcept : _data(data), _size(size) {}\n"
        "    ~vector() noexcept { delete[] _data; }\n"
        "\n"
        "    vector(const vector& other);\n"
        "    vector(vector&& other) noexcept : _data(other._data), _size(other._size) { other._data = nullptr; other._size = 0; }\n"
        "    vector& operator=(const vector& rhs);\n"
        "    vector& operator=(vector&& rhs) noexcept { delete[] _data; _data = rhs._data; _size = rhs._size; rhs._data = nullptr; rhs._size = 0; return *this; }\n"
        "\n"
        "    Type& operator[](size_t index) { return _data[index]; }\n"
        "    const Type& operator[](size_t index) const { return _data[index]; }\n"
        "    Type* data() { return _data; }\n"
        "    const Type* data() const { return _data; }\n"
        "    size_t size() const { return _size; }\n"
        "    bool empty() const { return _size == 0; }\n"
        "\n"
        "    void clear() noexcept { if (_data == nullptr) return; delete[] _data; _data = nullptr; _size = 0; }\n"
        "    void alloc(size_t size) { if (size != _size) { delete[] _data; _data = nullptr; _size = 0; if (size) { _data = new Type[size]; _size = size; } } else { for (size_t i = 0; i < _size; i++) { _data[i].~Type(); new(&_data[i]) Type; } } }\n"
        "    bool alloc_noThrow(size_t size) noexcept { if (size != _size) { delete[] _data; if (size == 0) { _data = nullptr; _size = 0; return true; } _data = new(std::nothrow) Type[size]; if (!_data) { _size = 0; return false; } _size = size; return true; } else { for (size_t i = 0; i < _size; i++) { _data[i].~Type(); new(&_data[i]) Type; } return true; } }\n"
        "    void resize(size_t newSize);\n"
        "    bool resize_noThrow(size_t newSize) noexcept;\n"
        "\n"
        "    iterator begin() noexcept { return iterator(_data); }\n"
        "    iterator end() noexcept { return iterator(_data + _size); }\n"
        "    const_iterator begin() const noexcept { return const_iterator(_data); }\n"
        "    const_iterator end() const noexcept { return const_iterator(_data + _size); }\n"
        "    const_iterator cbegin() const noexcept { return const_iterator(_data); }\n"
        "    const_iterator cend() const noexcept { return const_iterator(_data + _size); }\n"
        "};\n";

    static constexpr std::string_view CUSTOM_UTILS_DECL = ""
        "namespace detail {\n"
        "    template<typename _Tp> struct remove_reference { using type = _Tp; };\n"
        "    template<typename _Tp> struct remove_reference<_Tp&> { using type = _Tp; };\n"
        "    template<typename _Tp> struct remove_reference<_Tp&&> { using type = _Tp; };\n"
        "\n"
        "    template<typename _Tp>\n"
        "    constexpr _Tp&& forward(typename remove_reference<_Tp>::type&& __t) noexcept { return static_cast<_Tp&&>(__t); }\n"
        "\n"
        "    template<typename _Tp>\n"
        "    constexpr _Tp&& forward(typename remove_reference<_Tp>::type& __t) noexcept { return static_cast<_Tp&&>(__t); }\n"
        "    template<bool B, class T = void> struct enable_if {};\n"
        "    template<class T> struct enable_if<true, T> { using type = T; };\n"
        "    template<bool B, class T = void> using enable_if_t = typename enable_if<B, T>::type;\n"
        "\n"
        "    template <typename T> struct is_handle { static constexpr bool value = false; };\n"
        "    template <typename T> struct is_handle<Handle<T>> { static constexpr bool value = true; };\n"
        "\n"
        "    template <typename T, bool IsHandle = is_handle<T>::value>\n"
        "    struct native_type { using type = T; };\n"
        "\n"
        "    template <typename T>\n"
        "    struct native_type<T, true> { using type = typename T::HandleType; };\n"
        "};\n";

    // author: PCJohn (peciva at fit.vut.cz)
    static constexpr std::string_view CUSTOM_VECTOR_IMPL = ""
        "template<typename Type>\n"
        "vector<Type>::vector(const vector & other)\n"
        "    : _data(reinterpret_cast<Type*>(::operator new[](sizeof(Type)* other._size)))\n"
        "    , _size(other._size) {\n"
        "    size_t i = 0;\n"
        "    try {\n"
        "        for (size_t c = other._size; i < c; i++)\n"
        "            new(&_data[i]) Type(other._data[i]);\n"
        "    }\n"
        "    catch (...) {\n"
        "        while (i > 0) {\n"
        "            i--;\n"
        "            _data[i].~Type();\n"
        "        }\n"
        "        ::operator delete[](_data);\n"
        "        _data = nullptr;\n"
        "        _size = 0;\n"
        "        throw;\n"
        "    }\n"
        "}\n"
        "\n"
        "\n"
        "template<typename Type>\n"
        "vector<Type>& vector<Type>::operator=(const vector& rhs) {\n"
        "    if (_size == rhs._size)\n"
        "        for (size_t i = 0, c = _size; i < c; i++)\n"
        "            _data[i] = rhs._data[i];\n"
        "    else {\n"
        "        delete[] _data;\n"
        "        _data = nullptr;\n"
        "        _size = 0;\n"
        "        _data = ::operator new(sizeof(Type) * _size);\n"
        "        size_t i = 0;\n"
        "        try {\n"
        "            for (; i < rhs._size; i++)\n"
        "                new(&_data[i]) Type(rhs._data[i]);\n"
        "        }\n"
        "        catch (...) {\n"
        "            while (i > 0) {\n"
        "                i--;\n"
        "                _data[i].~Type();\n"
        "            }\n"
        "            ::operator delete[](_data);\n"
        "            _data = nullptr;\n"
        "            throw;\n"
        "        }\n"
        "        _size = rhs._size;\n"
        "    }\n"
        "    return *this;\n"
        "}\n"
        "\n"
        "\n"
        "template<typename Type>\n"
        "void vector<Type>::resize(size_t newSize) {\n"
        "    if (newSize > size()) {\n"
        "        Type* m = reinterpret_cast<Type*>(::operator new(sizeof(Type) * newSize));\n"
        "        size_t i = 0;\n"
        "        size_t s = size();\n"
        "        size_t j = s;\n"
        "        try {\n"
        "            for (; i < s; i++)\n"
        "                new(&m[i]) Type(static_cast<Type&&>(_data[i]));\n"
        "            for (; j < newSize; j++)\n"
        "                new(&m[j]) Type();\n"
        "        }\n"
        "        catch (...) {\n"
        "            while (j != s) {\n"
        "                j--;\n"
        "                m[j].~Type();\n"
        "            }\n"
        "            while (i != 0) {\n"
        "                i--;\n"
        "                m[i].~Type();\n"
        "            }\n"
        "            ::operator delete[](m);\n"
        "            throw;\n"
        "        }\n"
        "        Type* tmp = _data;\n"
        "        _data = m;\n"
        "        _size = newSize;\n"
        "        delete[] tmp;\n"
        "    } else if (newSize < size()) {\n"
        "        if (newSize == 0)\n"
        "            clear();\n"
        "        else {\n"
        "            Type* m = reinterpret_cast<Type*>(::operator new(sizeof(Type) * newSize));\n"
        "            size_t i = 0;\n"
        "            try {\n"
        "                for (size_t c = newSize; i < c; i++)\n"
        "                    new(&m[i]) Type(static_cast<Type&&>(_data[i]));\n"
        "            }\n"
        "            catch (...) {\n"
        "                while (i != 0) {\n"
        "                    i--;\n"
        "                    m[i].~Type();\n"
        "                }\n"
        "                ::operator delete[](m);\n"
        "                throw;\n"
        "            }\n"
        "            Type* tmp = _data;\n"
        "            _data = m;\n"
        "            _size = newSize;\n"
        "            delete[] tmp;\n"
        "        }\n"
        "    }\n"
        "}\n"
        "\n"
        "\n"
        "template<typename Type>\n"
        "bool vector<Type>::resize_noThrow(size_t newSize) noexcept {\n"
        "    if (newSize > size()) {\n"
        "        Type* m = reinterpret_cast<Type*>(::operator new(sizeof(Type) * newSize, std::nothrow));\n"
        "        if (!m)  return false;\n"
        "        size_t i = 0;\n"
        "        size_t s = size();\n"
        "        size_t j = s;\n"
        "        try {\n"
        "            for (; i < s; i++)\n"
        "                new(&m[i]) Type(static_cast<Type&&>(_data[i]));\n"
        "            for (; j < newSize; j++)\n"
        "                new(&m[j]) Type();\n"
        "        }\n"
        "        catch (...) {\n"
        "            while (j != s) {\n"
        "                j--;\n"
        "                m[j].~Type();\n"
        "            }\n"
        "            while (i != 0) {\n"
        "                i--;\n"
        "                m[i].~Type();\n"
        "            }\n"
        "            ::operator delete[](m);\n"
        "            clear();\n"
        "            return false;\n"
        "        }\n"
        "        Type* tmp = _data;\n"
        "        _data = m;\n"
        "        _size = newSize;\n"
        "        delete[] tmp;\n"
        "    } else if (newSize < size()) {\n"
        "        if (newSize == 0)\n"
        "            clear();\n"
        "        else {\n"
        "            Type* m = reinterpret_cast<Type*>(::operator new(sizeof(Type) * newSize, std::nothrow));\n"
        "            if (!m)  return false;\n"
        "            size_t i = 0;\n"
        "            try {\n"
        "                for (size_t c = newSize; i < c; i++)\n"
        "                    new(&m[i]) Type(static_cast<Type&&>(_data[i]));\n"
        "            }\n"
        "            catch (...) {\n"
        "                while (i != 0) {\n"
        "                    i--;\n"
        "                    m[i].~Type();\n"
        "                }\n"
        "                ::operator delete[](m);\n"
        "                clear();\n"
        "                return false;\n"
        "            }\n"
        "            Type* tmp = _data;\n"
        "            _data = m;\n"
        "            _size = newSize;\n"
        "            delete[] tmp;\n"
        "        }\n"
        "    }\n"
        "    return true;\n"
        "}\n";


    // TODO: handle dynamically
    static const std::array MODULE_EXPORTS = {
        "Flags"sv,
        "vector"sv,
        "iterator"sv,
        "span"sv,
        "loadLib"sv,
        "unloadLib"sv,
        "cleanUp"sv,
        "initInstance"sv,
        "initInstance_throw"sv,
        "initInstance_noThrow"sv,
        "initDevice"sv,
        "initDevice_throw"sv,
        "initDevice_noThrow"sv,
        "library"sv,
        "instance"sv,
        "physicalDevice"sv,
        "device"sv,
        "enumerateInstanceVersion"sv,
        "apiVersionVariant"sv,
        "apiVersionMajor"sv,
        "apiVersionMinor"sv,
        "apiVersionPatch"sv,
        "makeApiVersion"sv,
        "ApiVersion10"sv,
        "ApiVersion11"sv,
        "ApiVersion12"sv,
        "ApiVersion13"sv,
        "ApiVersion14"sv,
        "isExtensionSupported"sv,
        "destroyDevice"sv,
        "destroyInstance"sv,
        "getInstanceProcAddr"sv,
        "getDeviceProcAddr"sv,
        "getGlobalProcAddr"sv,
        "initInstancePFNs"sv,
        "initDevicePFNs"sv,
        "operator|"sv,
    };

    static const std::array ERROR_MODULE_EXPORTS = {
        "checkForSuccessValue"sv,
        "checkSuccess"sv,
        "throwResultExceptionWithMessage"sv,
        "throwResultException"sv,
        "Error"sv,
        "VkgError"sv,
    };

    inline std::ostream& print(std::ostream& os, std::string_view text, int indent = 0) {
        return os << std::string(indent * 4, ' ') << text;
    }

    template<size_t N>
    inline std::ostream& print(std::ostream& os, const std::array<std::string_view, N>& lines, int indent = 0) {
        std::string pad(indent * 4, ' ');
        for (auto& line : lines)
            os << pad << line;
        return os;
    }
}
