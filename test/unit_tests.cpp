#define _ENABLE_STL_INTERNAL_CHECK
#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <ranges>
#include <sph/ranges/views/hash.h>
#include <sph/ranges/views/hash_verify.h>
#include <vector>
#include <daw/json/daw_json_link.h>
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-W#warnings"
#endif
#include <doctest/doctest.h>
#ifdef __clang__
#pragma clang diagnostic pop
#endif
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
                    return static_cast<uint8_t>(std::stoul(std::string(v.begin(), v.end()), nullptr, 16));
                }) | std::ranges::to<std::vector<uint8_t>>(),
                .input = input | std::views::chunk(2) | std::views::transform([](auto&& v) -> uint8_t
                {
                    return static_cast<uint8_t>(std::stoul(std::string(v.begin(), v.end()), nullptr, 16));
                }) | std::ranges::to<std::vector<uint8_t>>(),
                .key = key | std::views::chunk(2) | std::views::transform([](auto&& v) -> uint8_t
                {
                    return static_cast<uint8_t>(std::stoul(std::string(v.begin(), v.end()), nullptr, 16));
                }) | std::ranges::to<std::vector<uint8_t>>(),
                .salt = salt | std::views::chunk(2) | std::views::transform([](auto&& v) -> uint8_t
                {
                    return static_cast<uint8_t>(std::stoul(std::string(v.begin(), v.end()), nullptr, 16));
                }) | std::ranges::to<std::vector<uint8_t>>(),
                .personal = personal | std::views::chunk(2) | std::views::transform([](auto&& v) -> uint8_t
                {
                    return static_cast<uint8_t>(std::stoul(std::string(v.begin(), v.end()), nullptr, 16));
                }) | std::ranges::to<std::vector<uint8_t>>()
            };
        }
    };
}

namespace daw::json {
    template<>
    struct json_data_contract<test_vector_json> {
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

} // namespace daw::json

namespace
{
    auto get_test_vector(std::string_view file_name) -> std::vector<test_vector>
    {
        auto file_path{ absolute(std::filesystem::path{file_name}) };
        if (!exists(file_path))
        {
            throw std::runtime_error(fmt::format("JSON file {} does not exist.", file_path.string()));
        }

        std::ifstream f(file_path);
        std::string json_string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

        try
        {
            std::vector<test_vector_json> test_vectors_json = daw::json::from_json_array<test_vector_json>(json_string);
            return test_vectors_json | std::views::transform([](test_vector_json const& v) -> test_vector { return v.test_vector(); }) | std::ranges::to<std::vector>();
        }
        catch (daw::json::json_exception const& ex)
        {
            throw std::runtime_error(fmt::format("Failed to read {} test vector: {}", file_path.string(), ex.what()));
        }
    }

    std::vector<test_vector> const blake2b_test_vectors {get_test_vector("blake2b.json")};
    std::vector<test_vector> const sha256_test_vectors {get_test_vector("sha256.json")};
    std::vector<test_vector> const sha512_test_vectors {get_test_vector("sha512.json")};
}

// namespace
// {
//     template <class _Left, class _Right>
//         requires std::ranges::_Pipe::_Range_adaptor_closure_object<_Left>&& std::ranges::_Pipe::_Range_adaptor_closure_object<_Right>
//     && std::constructible_from<std::remove_cvref_t<_Left>, _Left>
//         && std::constructible_from<std::remove_cvref_t<_Right>, _Right>
//         auto foo(_Left&& left, _Right&& right) -> void;
//     {
//         (void)left;
//         (void)right;
//     }
// }
// 
// TEST_CASE("foo")
// {
//     std::vector<uint8_t> r{};
//     sph::ranges::views::detail::hash_view<std::ranges::ref_view<const std::vector<uint8_t, std::allocator<uint8_t>>>, uint8_t, sph::hash_algorithm::blake2b, sph::hash_style::separate> left(10, r);
//     std::ranges::_Range_closure<std::ranges::_To_template_fn<std::vector>> right;
//     foo(left, right);
// }

// TEST_CASE("hash.vectors")
// {
//     auto test_name{ get_current_test_name() };
//     for (auto const [index, test_vector] : std::views::enumerate(blake2b_test_vectors | std::views::filter([](auto &&x) { return x.key.empty() && x.salt.empty() && x.personal.empty();})))
//     {
//         auto hash = test_vector.input | sph::views::hash<sph::hash_algorithm::blake2b>(test_vector.outlen) | std::ranges::to<std::vector>();
//         CHECK_MESSAGE(hash == test_vector.out, fmt::format("{}: failed blake on test vector {}", test_name, index));
//     }
// 
//     for (auto const [index, test_vector] : std::views::enumerate(sha256_test_vectors))
//     {
//         auto hash = test_vector.input | sph::views::hash<sph::hash_algorithm::sha256>(test_vector.outlen) | std::ranges::to<std::vector>();
//         CHECK_MESSAGE(hash == test_vector.out, fmt::format("{}: failed sha256 on test vector {}", test_name, index));
//     }
// 
//     for (auto const [index, test_vector] : std::views::enumerate(sha512_test_vectors))
//     {
//         auto hash = test_vector.input | sph::views::hash<sph::hash_algorithm::sha512>(test_vector.outlen) | std::ranges::to<std::vector>();
//         CHECK_MESSAGE(hash == test_vector.out, fmt::format("{}: failed sha512 on test vector {}", test_name, index));
//     }
// 
// }

TEST_CASE("hash_verify.vectors")
{
    auto test_name{ get_current_test_name() };
    for (auto const [index, test_vector] : std::views::enumerate(blake2b_test_vectors | std::views::filter([](auto&& x) { return x.key.empty() && x.salt.empty() && x.personal.empty(); })))
    {
        auto verify_range = test_vector.input | sph::views::hash_verify<sph::hash_algorithm::blake2b>(test_vector.out, test_vector.outlen);
        std::vector<uint8_t> verify{ verify_range | std::views::transform([](auto&& v) -> uint8_t
            {
                return v ? 1 : 0;
            }) | std::ranges::to<std::vector>()};
        CHECK_MESSAGE(verify.size() == 1, fmt::format("{}: failed blake on test vector {}", test_name, index));
        CHECK_MESSAGE(verify.front() == true, fmt::format("{}: failed blake on test vector {}", test_name, index));
    }

    for (auto const [index, test_vector] : std::views::enumerate(sha256_test_vectors))
    {
        auto verify = test_vector.input | sph::views::hash_verify<sph::hash_algorithm::sha256>(test_vector.out, test_vector.outlen) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(verify.size() == 1, fmt::format("{}: failed blake on test vector {}", test_name, index));
        CHECK_MESSAGE(verify.front() == true, fmt::format("{}: failed blake on test vector {}", test_name, index));
    }

    for (auto const [index, test_vector] : std::views::enumerate(sha512_test_vectors))
    {
        auto verify = test_vector.input | sph::views::hash_verify<sph::hash_algorithm::sha512>(test_vector.out, test_vector.outlen) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(verify.size() == 1, fmt::format("{}: failed blake on test vector {}", test_name, index));
        CHECK_MESSAGE(verify.front() == true, fmt::format("{}: failed blake on test vector {}", test_name, index));
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
