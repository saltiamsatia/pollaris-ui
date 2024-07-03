#pragma once
/**
 * @file Infrastructure/reflectors.hpp
 *
 * @brief Defines templates and macros for reflecting simple structs
 *
 * Reflection is the declaration of the fields and/or methods of a type so that generic code can operate on specific
 * types. Our reflection is done at compile-time so that the compiler can be made to automatically generate code to
 * operate on specific types. Reflection here covers only public data members, not methods, and does not support
 * inheritance, although inherited fields can be reflected the same as native ones.
 *
 * Our reflection is performed using two main templates: the `field_reflector` and the `reflector`. The field
 * reflector stores compile-time information about a field of a struct, and the reflector stores compile-time
 * information about the struct itself including the list of fields. Also of interest is the `member_names` template
 * which stores a C-string name of a field.
 */

#include <boost/preprocessor/seq/for_each_i.hpp>
#include <boost/preprocessor/stringize.hpp>

#include <Infrastructure/typelist.hpp>

namespace infra {

template<class Container>
struct reflector;

/**
 * @brief A template to store the name of a field in a struct or class
 *
 * @tparam Container The struct the field to be named belongs to
 * @tparam Index The index of the field in the struct, as per @ref reflector<Class>::members
 */
template<class Container, std::size_t Index>
struct member_name {
    constexpr static const char* value = "Unknown member";
};
template<class Container, std::size_t Index>
constexpr static const char* member_name_v = member_name<Container, Index>::value;

/**
 *  @brief A template to store compile-time information about a field in a reflected struct
 *
 *  @tparam Container The type of the struct or class containing the field
 *  @tparam Member The type of the field
 *  @tparam field A pointer-to-member for the reflected field
 */
template<std::size_t Index, class Container, class Member, Member Container::*field>
struct field_reflector {
    using container = Container;
    using type = Member;
    using reflector = infra::reflector<type>;
    constexpr static std::size_t index = Index;
    constexpr static bool is_derived = false;
    constexpr static type container::*pointer = field;

    /// @brief Given a reference to the container type, get a reference to the field
    static type& get(container& c) { return c.*field; }
    static const type& get(const container& c) { return c.*field; }
    /// @brief Get the name of the field
    static const char* get_name() { return member_name<container, index>::value; }
};

/**
 * @brief A template to store compile-time reflection information about structs or classes
 *
 * @tparam Container The type being reflected
 */
template<class Container>
struct reflector {
    /// The type being reflected
    using type = Container;
    /// Whether the reflector is defined or not
    using is_defined = std::false_type;
    /// A list of the members of the reflected type
    using members = typelist::list<>;
    /// The name of the Container type
    constexpr static const char* type_name = "Unreflected type";
};

} // namespace infra

// Macros to aid in creating reflectors
/// For use with BOOST_PP_SEQ_FOR_EACH_I, to add a member to a typelist builder
#define IMPL_REFLECT_ADD_MEMBER(R, Struct, Index, Member) \
    ::add<field_reflector<Index, Struct, decltype(Struct::Member), &Struct::Member>>
/// For use with BOOST_PP_SEQ_FOR_EACH_I, to create a member_name record for a member
#define IMPL_REFLECT_MEMBER_NAME(R, Struct, Index, Member) \
    template<> struct member_name<Struct, Index> { constexpr static const char* value = BOOST_PP_STRINGIZE(Member); };

/// @macro Reflect a struct and its members, as in REFLECT_STRUCT(MyStruct, (memberA)(memberB))
/// @note Must be used within the global namespace
#define REFLECT_STRUCT(Struct, Members) \
    namespace infra { \
    BOOST_PP_SEQ_FOR_EACH_I(IMPL_REFLECT_MEMBER_NAME, Struct, Members) \
    template<> struct reflector<Struct> { \
        using type = Struct; using is_defined = std::true_type; \
        using members = typelist::builder_t<> \
                        BOOST_PP_SEQ_FOR_EACH_I(IMPL_REFLECT_ADD_MEMBER, Struct, Members) ::finalize; \
        constexpr static const char* type_name = BOOST_PP_STRINGIZE(Struct); \
    }; }
