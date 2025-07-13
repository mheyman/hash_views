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

TEST_CASE("hash.vectors")
{
    auto test_name{ get_current_test_name() };
    for (auto const [index, test_vector] : std::views::enumerate(blake2b_test_vectors | std::views::filter([](auto &&x) { return x.key.empty() && x.salt.empty() && x.personal.empty();})))
    {
        auto hash = test_vector.input | sph::views::hash<sph::hash_algorithm::blake2b>(test_vector.outlen) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(hash == test_vector.out, fmt::format("{}: failed blake on test vector {}", test_name, index));
    }

    for (auto const [index, test_vector] : std::views::enumerate(sha256_test_vectors))
    {
        auto hash = test_vector.input | sph::views::hash<sph::hash_algorithm::sha256>(test_vector.outlen) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(hash == test_vector.out, fmt::format("{}: failed sha256 on test vector {}", test_name, index));
    }

    for (auto const [index, test_vector] : std::views::enumerate(sha512_test_vectors))
    {
        auto hash = test_vector.input | sph::views::hash<sph::hash_algorithm::sha512>(test_vector.outlen) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(hash == test_vector.out, fmt::format("{}: failed sha512 on test vector {}", test_name, index));
    }
}

TEST_CASE("hash.overloads")
{
    using sph::hash_algorithm;
    using sph::hash_format;
    using sph::hash_site;
    using sph::views::hash;

    std::vector<uint8_t> hello_world{ 'h','e','l','l','o',' ','w','o','r','l','d' };
    size_t hash_size = 32;

    // Fully specified
    auto ref = hello_world
        | hash<hash_algorithm::blake2b, uint8_t, hash_format::raw, hash_site::separate>(hash_size)
        | std::ranges::to<std::vector>();

    {
        // All overloads with explicit types, all should match ref
        auto o1 = hello_world | hash<hash_algorithm::blake2b, uint8_t, hash_format::raw, hash_site::separate>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o1 == ref, "Overload: <A, T, F, S> failed");

        auto o2 = hello_world | hash<hash_algorithm::blake2b, uint8_t, hash_site::separate, hash_format::raw>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o2 == ref, "Overload: <A, T, S, F> failed");

        auto o3 = hello_world | hash<hash_algorithm::blake2b, hash_site::separate, uint8_t, hash_format::raw>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o3 == ref, "Overload: <A, S, T, F> failed");

        auto o4 = hello_world | hash<hash_algorithm::blake2b, hash_site::separate, hash_format::raw, uint8_t>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o4 == ref, "Overload: <A, S, F, T> failed");

        auto o5 = hello_world | hash<hash_algorithm::blake2b, hash_format::raw, uint8_t, hash_site::separate>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o5 == ref, "Overload: <A, F, T, S> failed");

        auto o6 = hello_world | hash<hash_algorithm::blake2b, hash_format::raw, hash_site::separate, uint8_t>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o6 == ref, "Overload: <A, F, S, T> failed");

        auto o7 = hello_world | hash<hash_site::separate, hash_algorithm::blake2b, uint8_t, hash_format::raw>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o7 == ref, "Overload: <S, A, T, F> failed");

        auto o8 = hello_world | hash<hash_site::separate, hash_algorithm::blake2b, hash_format::raw, uint8_t>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o8 == ref, "Overload: <S, A, F, T> failed");

        auto o9 = hello_world | hash<hash_site::separate, hash_format::raw, hash_algorithm::blake2b, uint8_t>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o9 == ref, "Overload: <S, F, A, T> failed");

        auto o10 = hello_world | hash<hash_site::separate, hash_format::raw, uint8_t, hash_algorithm::blake2b>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o10 == ref, "Overload: <S, F, T, A> failed");

        auto o11 = hello_world | hash<hash_site::separate, uint8_t, hash_algorithm::blake2b, hash_format::raw>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o11 == ref, "Overload: <S, T, A, F> failed");

        auto o12 = hello_world | hash<hash_site::separate, uint8_t, hash_format::raw, hash_algorithm::blake2b>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o12 == ref, "Overload: <S, T, F, A> failed");

        auto o13 = hello_world | hash<hash_format::raw, hash_algorithm::blake2b, uint8_t, hash_site::separate>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o13 == ref, "Overload: <F, A, T, S> failed");

        auto o14 = hello_world | hash<hash_format::raw, hash_algorithm::blake2b, hash_site::separate, uint8_t>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o14 == ref, "Overload: <F, A, S, T> failed");

        auto o15 = hello_world | hash<hash_format::raw, hash_site::separate, hash_algorithm::blake2b, uint8_t>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o15 == ref, "Overload: <F, S, A, T> failed");

        auto o16 = hello_world | hash<hash_format::raw, hash_site::separate, uint8_t, hash_algorithm::blake2b>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o16 == ref, "Overload: <F, S, T, A> failed");

        auto o17 = hello_world | hash<hash_format::raw, uint8_t, hash_algorithm::blake2b, hash_site::separate>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o17 == ref, "Overload: <F, T, A, S> failed");

        auto o18 = hello_world | hash<hash_format::raw, uint8_t, hash_site::separate, hash_algorithm::blake2b>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o18 == ref, "Overload: <F, T, S, A> failed");

        auto o19 = hello_world | hash<uint8_t, hash_algorithm::blake2b, hash_format::raw, hash_site::separate>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o19 == ref, "Overload: <T, A, F, S> failed");

        auto o20 = hello_world | hash<uint8_t, hash_algorithm::blake2b, hash_site::separate, hash_format::raw>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o20 == ref, "Overload: <T, A, S, F> failed");

        auto o21 = hello_world | hash<uint8_t, hash_site::separate, hash_algorithm::blake2b, hash_format::raw>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o21 == ref, "Overload: <T, S, A, F> failed");

        auto o22 = hello_world | hash<uint8_t, hash_site::separate, hash_format::raw, hash_algorithm::blake2b>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o22 == ref, "Overload: <T, S, F, A> failed");

        auto o23 = hello_world | hash<uint8_t, hash_format::raw, hash_algorithm::blake2b, hash_site::separate>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o23 == ref, "Overload: <T, F, A, S> failed");

        auto o24 = hello_world | hash<uint8_t, hash_format::raw, hash_site::separate, hash_algorithm::blake2b>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o24 == ref, "Overload: <T, F, S, A> failed");
    }
    {
        // uint8_t overloads with three explicit types, all should match ref
        auto o1 = hello_world | hash<hash_algorithm::blake2b, uint8_t, hash_format::raw>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o1 == ref, "Overload: <A, T, F> failed");

        auto o2 = hello_world | hash<hash_algorithm::blake2b, uint8_t, hash_site::separate>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o2 == ref, "Overload: <A, T, S> failed");

        auto o3 = hello_world | hash<hash_algorithm::blake2b, hash_site::separate, uint8_t>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o3 == ref, "Overload: <A, S, T> failed");

        auto o4 = hello_world | hash<hash_algorithm::blake2b, hash_site::separate, hash_format::raw>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o4 == ref, "Overload: <A, S, F> failed");

        auto o5 = hello_world | hash<hash_algorithm::blake2b, hash_format::raw, uint8_t>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o5 == ref, "Overload: <A, F, T> failed");

        auto o6 = hello_world | hash<hash_algorithm::blake2b, hash_format::raw, hash_site::separate>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o6 == ref, "Overload: <A, F, S> failed");

        auto o7 = hello_world | hash<hash_site::separate, hash_algorithm::blake2b, uint8_t>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o7 == ref, "Overload: <S, A, T> failed");

        auto o8 = hello_world | hash<hash_site::separate, hash_algorithm::blake2b, hash_format::raw>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o8 == ref, "Overload: <S, A, F> failed");

        auto o9 = hello_world | hash<hash_site::separate, hash_format::raw, hash_algorithm::blake2b>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o9 == ref, "Overload: <S, F, A> failed");

        auto o10 = hello_world | hash<hash_site::separate, hash_format::raw, uint8_t>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o10 == ref, "Overload: <S, F, T> failed");

        auto o11 = hello_world | hash<hash_site::separate, uint8_t, hash_algorithm::blake2b>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o11 == ref, "Overload: <S, T, A> failed");

        auto o12 = hello_world | hash<hash_site::separate, uint8_t, hash_format::raw>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o12 == ref, "Overload: <S, T, F> failed");

        auto o13 = hello_world | hash<hash_format::raw, hash_algorithm::blake2b, uint8_t>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o13 == ref, "Overload: <F, A, T> failed");

        auto o14 = hello_world | hash<hash_format::raw, hash_algorithm::blake2b, hash_site::separate>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o14 == ref, "Overload: <F, A, S> failed");

        auto o15 = hello_world | hash<hash_format::raw, hash_site::separate, hash_algorithm::blake2b>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o15 == ref, "Overload: <F, S, A> failed");

        auto o16 = hello_world | hash<hash_format::raw, hash_site::separate, uint8_t>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o16 == ref, "Overload: <F, S, T> failed");

        auto o17 = hello_world | hash<hash_format::raw, uint8_t, hash_algorithm::blake2b>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o17 == ref, "Overload: <F, T, A> failed");

        auto o18 = hello_world | hash<hash_format::raw, uint8_t, hash_site::separate>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o18 == ref, "Overload: <F, T, S> failed");

        auto o19 = hello_world | hash<uint8_t, hash_algorithm::blake2b, hash_format::raw>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o19 == ref, "Overload: <T, A, F> failed");

        auto o20 = hello_world | hash<uint8_t, hash_algorithm::blake2b, hash_site::separate>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o20 == ref, "Overload: <T, A, S> failed");

        auto o21 = hello_world | hash<uint8_t, hash_site::separate, hash_algorithm::blake2b>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o21 == ref, "Overload: <T, S, A> failed");

        auto o22 = hello_world | hash<uint8_t, hash_site::separate, hash_format::raw>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o22 == ref, "Overload: <T, S, F> failed");

        auto o23 = hello_world | hash<uint8_t, hash_format::raw, hash_algorithm::blake2b>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o23 == ref, "Overload: <T, F, A> failed");

        auto o24 = hello_world | hash<uint8_t, hash_format::raw, hash_site::separate>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o24 == ref, "Overload: <T, F, S> failed");
    }
    {
        // uint8_t overloads with two explicit types, all should match ref
        auto o1 = hello_world | hash<hash_algorithm::blake2b, uint8_t>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o1 == ref, "Overload: <A, T> failed");

        auto o2 = hello_world | hash<hash_algorithm::blake2b, hash_site::separate>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o2 == ref, "Overload: <A, S> failed");

        auto o3 = hello_world | hash<hash_algorithm::blake2b, hash_format::raw>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o3 == ref, "Overload: <A, F> failed");

        auto o4 = hello_world | hash<hash_site::separate, hash_algorithm::blake2b>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o4 == ref, "Overload: <S, A> failed");

        auto o5 = hello_world | hash<hash_site::separate, hash_format::raw>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o5 == ref, "Overload: <S, F> failed");

        auto o6 = hello_world | hash<hash_site::separate, uint8_t>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o6 == ref, "Overload: <S, T> failed");

        auto o7 = hello_world | hash<hash_format::raw, hash_algorithm::blake2b>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o7 == ref, "Overload: <F, A> failed");

        auto o8 = hello_world | hash<hash_format::raw, hash_site::separate>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o8 == ref, "Overload: <F, S> failed");

        auto o9 = hello_world | hash<hash_format::raw, uint8_t>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o9 == ref, "Overload: <F, T> failed");

        auto o10 = hello_world | hash<uint8_t, hash_algorithm::blake2b>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o10 == ref, "Overload: <T, A> failed");

        auto o11 = hello_world | hash<uint8_t, hash_site::separate>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o11 == ref, "Overload: <T, S> failed");

        auto o12 = hello_world | hash<uint8_t, hash_format::raw>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o12 == ref, "Overload: <T, F> failed");
    }
    {
        // uint8_t overloads with one explicit type, all should match ref
        auto o1 = hello_world | hash<hash_algorithm::blake2b>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o1 == ref, "Overload: <A> failed");

        auto o2 = hello_world | hash<hash_site::separate>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o2 == ref, "Overload: <S> failed");

        auto o3 = hello_world | hash<hash_format::raw>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o3 == ref, "Overload: <F> failed");

        auto o4 = hello_world | hash<uint8_t>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o4 == ref, "Overload: <T> failed");
    }

    auto o25 = hello_world | hash(hash_size) | std::ranges::to<std::vector>();
    CHECK_MESSAGE(o25 == ref, "Overload: <> failed");

}

TEST_CASE("hash.to_size_t")
{
    using sph::hash_algorithm;
    using sph::hash_format;
    using sph::hash_site;
    using sph::views::hash;

    std::vector<uint8_t> hello_world{ 'h','e','l','l','o',' ','w','o','r','l','d' };
    size_t hash_size = 32;

    // Fully specified reference: size_t output, padded format
    auto ref = hello_world
        | hash<hash_algorithm::blake2b, size_t, hash_format::padded, hash_site::separate>(hash_size)
        | std::ranges::to<std::vector>();

    {
        auto o1 = hello_world | hash<hash_algorithm::blake2b, size_t, hash_format::padded, hash_site::separate>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o1 == ref, "Overload: <A, T, F, S> failed");

        auto o2 = hello_world | hash<hash_algorithm::blake2b, size_t, hash_site::separate, hash_format::padded>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o2 == ref, "Overload: <A, T, S, F> failed");

        auto o3 = hello_world | hash<hash_algorithm::blake2b, hash_site::separate, size_t, hash_format::padded>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o3 == ref, "Overload: <A, S, T, F> failed");

        auto o4 = hello_world | hash<hash_algorithm::blake2b, hash_site::separate, hash_format::padded, size_t>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o4 == ref, "Overload: <A, S, F, T> failed");

        auto o5 = hello_world | hash<hash_algorithm::blake2b, hash_format::padded, size_t, hash_site::separate>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o5 == ref, "Overload: <A, F, T, S> failed");

        auto o6 = hello_world | hash<hash_algorithm::blake2b, hash_format::padded, hash_site::separate, size_t>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o6 == ref, "Overload: <A, F, S, T> failed");

        auto o7 = hello_world | hash<hash_site::separate, hash_algorithm::blake2b, size_t, hash_format::padded>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o7 == ref, "Overload: <S, A, T, F> failed");

        auto o8 = hello_world | hash<hash_site::separate, hash_algorithm::blake2b, hash_format::padded, size_t>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o8 == ref, "Overload: <S, A, F, T> failed");

        auto o9 = hello_world | hash<hash_site::separate, hash_format::padded, hash_algorithm::blake2b, size_t>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o9 == ref, "Overload: <S, F, A, T> failed");

        auto o10 = hello_world | hash<hash_site::separate, hash_format::padded, size_t, hash_algorithm::blake2b>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o10 == ref, "Overload: <S, F, T, A> failed");

        auto o11 = hello_world | hash<hash_site::separate, size_t, hash_algorithm::blake2b, hash_format::padded>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o11 == ref, "Overload: <S, T, A, F> failed");

        auto o12 = hello_world | hash<hash_site::separate, size_t, hash_format::padded, hash_algorithm::blake2b>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o12 == ref, "Overload: <S, T, F, A> failed");

        auto o13 = hello_world | hash<hash_format::padded, hash_algorithm::blake2b, size_t, hash_site::separate>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o13 == ref, "Overload: <F, A, T, S> failed");

        auto o14 = hello_world | hash<hash_format::padded, hash_algorithm::blake2b, hash_site::separate, size_t>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o14 == ref, "Overload: <F, A, S, T> failed");

        auto o15 = hello_world | hash<hash_format::padded, hash_site::separate, hash_algorithm::blake2b, size_t>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o15 == ref, "Overload: <F, S, A, T> failed");

        auto o16 = hello_world | hash<hash_format::padded, hash_site::separate, size_t, hash_algorithm::blake2b>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o16 == ref, "Overload: <F, S, T, A> failed");

        auto o17 = hello_world | hash<hash_format::padded, size_t, hash_algorithm::blake2b, hash_site::separate>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o17 == ref, "Overload: <F, T, A, S> failed");

        auto o18 = hello_world | hash<hash_format::padded, size_t, hash_site::separate, hash_algorithm::blake2b>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o18 == ref, "Overload: <F, T, S, A> failed");

        auto o19 = hello_world | hash<size_t, hash_algorithm::blake2b, hash_format::padded, hash_site::separate>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o19 == ref, "Overload: <T, A, F, S> failed");

        auto o20 = hello_world | hash<size_t, hash_algorithm::blake2b, hash_site::separate, hash_format::padded>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o20 == ref, "Overload: <T, A, S, F> failed");

        auto o21 = hello_world | hash<size_t, hash_site::separate, hash_algorithm::blake2b, hash_format::padded>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o21 == ref, "Overload: <T, S, A, F> failed");

        auto o22 = hello_world | hash<size_t, hash_site::separate, hash_format::padded, hash_algorithm::blake2b>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o22 == ref, "Overload: <T, S, F, A> failed");

        auto o23 = hello_world | hash<size_t, hash_format::padded, hash_algorithm::blake2b, hash_site::separate>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o23 == ref, "Overload: <T, F, A, S> failed");

        auto o24 = hello_world | hash<size_t, hash_format::padded, hash_site::separate, hash_algorithm::blake2b>(hash_size) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(o24 == ref, "Overload: <T, F, S, A> failed");
    }


}

TEST_CASE("hash_verify.vectors")
{
    auto test_name{ get_current_test_name() };
    for (auto const [index, test_vector] : std::views::enumerate(blake2b_test_vectors | std::views::filter([](auto&& x) { return x.key.empty() && x.salt.empty() && x.personal.empty(); })))
    {
        auto verify {test_vector.input | sph::views::hash_verify<sph::hash_algorithm::blake2b>(test_vector.out, test_vector.outlen) | std::ranges::to<std::vector>()};
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


TEST_CASE("hexstring_to_hex")
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
