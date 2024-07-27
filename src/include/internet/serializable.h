#include <iostream>
#include <string>
#include <vector>
#include <tuple>

#ifndef __Z_SERIALIZABLE
#define __Z_SERIALIZABLE

// contraints
template <typename T>
concept is_serializable = std::same_as<decltype(std::declval<T>().serialize((size_t*)0)), char*>;
template <typename T>
concept is_deserializable = std::same_as<decltype(std::declval<T>().deserialize((char*)0)), T>;

/**
 * Serializable type T
 */
template <typename T>
requires is_serializable<T> && is_deserializable<T>
class Serializable
{
     
public:
     constexpr char* serialize(size_t* len) noexcept
     { return ((T*)this)->serialize(std::forward<decltype(len)>(len)); }

     constexpr T deserialize(char* s) noexcept
     { return ((T*)this)->deserialize(s); }
};

// aligend with 8 byte
class Main
{
public:
     constexpr char* serialize(size_t*) noexcept;
     constexpr Main deserialize(char*) noexcept;
     // construct in compile time
     constexpr explicit Main() {}
private:
};

inline constexpr char *Main::serialize(size_t*) noexcept
{
     printf("serialize...\n");
     return nullptr;
}

inline constexpr Main Main::deserialize(char*) noexcept
{
     printf("deserialize...\n");
     return Main();
};

#endif