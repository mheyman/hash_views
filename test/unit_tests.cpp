#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <doctest/doctest.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <ranges>
#include <sph/ranges/views/hash.h>
#include <sph/ranges/views/hash_verify.h>
#include <vector>
#include <daw/json/daw_json_link.h>

#include "doctest_util.h"
namespace
{
    struct test_vector
    {
        size_t outlen;
        std::vector<uint8_t> out;
        std::vector<uint8_t> input;
        std::vector<uint8_t> key;
        std::vector<uint8_t> salt;
        std::vector<uint8_t> personal;
    };

    struct test_vector_json
    {
        size_t outlen;
        std::string out;
        std::string input;
        std::string key;
        std::string salt;
        std::string personal;
        auto test_vector() const -> test_vector
        {
            return ::test_vector{
                .outlen = outlen,
                .out = out | std::views::chunk(2) | std::views::transform([](auto&& v) -> uint8_t
                {
                    return static_cast<uint8_t>(std::stoul(std::string{v}, nullptr, 16));
                }) | std::ranges::to<std::vector<uint8_t>>(),
                .input = input | std::views::chunk(2) | std::views::transform([](auto&& v) -> uint8_t
                {
                    return static_cast<uint8_t>(std::stoul(std::string{v}, nullptr, 16));
                }) | std::ranges::to<std::vector<uint8_t>>(),
                .key = key | std::views::chunk(2) | std::views::transform([](auto&& v) -> uint8_t
                {
                    return static_cast<uint8_t>(std::stoul(std::string{v}, nullptr, 16));
                }) | std::ranges::to<std::vector<uint8_t>>(),
                .salt = salt | std::views::chunk(2) | std::views::transform([](auto&& v) -> uint8_t
                {
                    return static_cast<uint8_t>(std::stoul(std::string{v}, nullptr, 16));
                }) | std::ranges::to<std::vector<uint8_t>>(),
                .personal = personal | std::views::chunk(2) | std::views::transform([](auto&& v) -> uint8_t
                {
                    return static_cast<uint8_t>(std::stoul(std::string{v}, nullptr, 16));
                }) | std::ranges::to<std::vector<uint8_t>>()
            };
        }
    };
}

template<>
struct daw::json::json_data_contract<test_vector_json> {
    static constexpr char const outlen[] = "outlen";
    static constexpr char const out[] = "out";
    static constexpr char const input[] = "input";
    static constexpr char const key[] = "key";
    static constexpr char const salt[] = "salt";
    static constexpr char const personal[] = "personal";
    using type = json_member_list<
        json_number<outlen, size_t>,
        json_string<out, std::string>,
        json_string<input, std::string>,
        json_string<key, std::string>,
        json_string<salt, std::string>,
        json_string<personal, std::string>
    >;
}; 

namespace
{
    auto get_test_vector(std::string_view file_name) -> std::vector<test_vector>
    {
        std::ifstream f(std::string{file_name});
        std::string json_string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

        try {
            auto test_vector_jsons = daw::json::from_json_array<test_vector_json>(json_string);
            return test_vector_jsons | std::views::transform([](test_vector_json const& v) -> test_vector { return v.test_vector(); }) | std::ranges::to<std::vector>();
        } catch (daw::json::json_exception const& ex) {
            throw std::runtime_error(fmt::format("Failed to read blake2b test vector: {}", ex.what()));
        }
    }

    // ReSharper disable once CppInconsistentNaming
    std::vector<test_vector> const blake2b_test_vectors {get_test_vector("blake2b.json")};
    std::vector<test_vector> const sha256_test_vectors {get_test_vector("sha256.json")};
    std::vector<test_vector> const sha512_test_vectors {get_test_vector("sha512.json")};
}

TEST_CASE("hash.vectors")
{
    for (auto const& test_vector : blake2b_test_vectors)
    {
        auto hash = test_vector.input | sph::views::hash<sph::hash_algorithm::blake2b>(test_vector.outlen) | std::ranges::to<std::vector>();
        CHECK(hash == test_vector.out);
    }

    for (auto const& test_vector : sha256_test_vectors)
    {
        auto hash = test_vector.input | sph::views::hash<sph::hash_algorithm::sha256>(test_vector.outlen) | std::ranges::to<std::vector>();
        CHECK(hash == test_vector.out);
    }

    for (auto const& test_vector : sha512_test_vectors)
    {
        auto hash = test_vector.input | sph::views::hash<sph::hash_algorithm::sha512>(test_vector.outlen) | std::ranges::to<std::vector>();
        CHECK(hash == test_vector.out);
    }

}

TEST_CASE("foo")
{
    std::vector<uint8_t> foo {
        std::string_view{ "01A1" }
        | std::views::chunk(2)
        | std::views::transform([](auto&& v) -> uint8_t
            {
                std::array<char, 3> s{{v[0], v[1], '\0'}};
                char* endptr;
                return static_cast<uint8_t>(strtoul(s.data(), &endptr, 16));
            })
        | std::ranges::to<std::vector<uint8_t>>()};
    CHECK(foo == std::vector<uint8_t> { 0x01, 0xA1 });
}