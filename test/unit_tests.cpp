// ReSharper disable once CppInconsistentNaming
#define _ENABLE_STL_INTERNAL_CHECK  // NOLINT(clang-diagnostic-reserved-macro-identifier)
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

// ReSharper disable once CppRedundantNamespaceDefinition
namespace daw::json
{
    template<>
    struct json_data_contract<test_vector_json> {
        static constexpr char outlen[] = "outlen";
        static constexpr char out[] = "out";
        static constexpr char input[] = "input";
        static constexpr char key[] = "key";
        static constexpr char salt[] = "salt";
        static constexpr char personal[] = "personal";
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

    template <sph::hash_algorithm A>
    auto hash_overloads() -> void
    {
        using sph::hash_format;
        using sph::hash_site;
        using sph::views::hash;

        std::vector<uint8_t> hello_world{ 'h','e','l','l','o',' ','w','o','r','l','d' };
        size_t hash_size = 32;

        // Fully specified
        auto ref = hello_world
            | hash<A, uint8_t, hash_format::raw, hash_site::separate>(hash_size)
            | std::ranges::to<std::vector>();

        {
            // All hash_overloads with explicit types, all should match ref
            auto o1 = hello_world | hash<A, uint8_t, hash_format::raw, hash_site::separate>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o1 == ref, "Overload: <A, T, F, S> failed");

            auto o2 = hello_world | hash<A, uint8_t, hash_site::separate, hash_format::raw>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o2 == ref, "Overload: <A, T, S, F> failed");

            auto o3 = hello_world | hash<A, hash_site::separate, uint8_t, hash_format::raw>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o3 == ref, "Overload: <A, S, T, F> failed");

            auto o4 = hello_world | hash<A, hash_site::separate, hash_format::raw, uint8_t>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o4 == ref, "Overload: <A, S, F, T> failed");

            auto o5 = hello_world | hash<A, hash_format::raw, uint8_t, hash_site::separate>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o5 == ref, "Overload: <A, F, T, S> failed");

            auto o6 = hello_world | hash<A, hash_format::raw, hash_site::separate, uint8_t>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o6 == ref, "Overload: <A, F, S, T> failed");

            auto o7 = hello_world | hash<hash_site::separate, A, uint8_t, hash_format::raw>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o7 == ref, "Overload: <S, A, T, F> failed");

            auto o8 = hello_world | hash<hash_site::separate, A, hash_format::raw, uint8_t>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o8 == ref, "Overload: <S, A, F, T> failed");

            auto o9 = hello_world | hash<hash_site::separate, hash_format::raw, A, uint8_t>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o9 == ref, "Overload: <S, F, A, T> failed");

            auto o10 = hello_world | hash<hash_site::separate, hash_format::raw, uint8_t, A>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o10 == ref, "Overload: <S, F, T, A> failed");

            auto o11 = hello_world | hash<hash_site::separate, uint8_t, A, hash_format::raw>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o11 == ref, "Overload: <S, T, A, F> failed");

            auto o12 = hello_world | hash<hash_site::separate, uint8_t, hash_format::raw, A>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o12 == ref, "Overload: <S, T, F, A> failed");

            auto o13 = hello_world | hash<hash_format::raw, A, uint8_t, hash_site::separate>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o13 == ref, "Overload: <F, A, T, S> failed");

            auto o14 = hello_world | hash<hash_format::raw, A, hash_site::separate, uint8_t>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o14 == ref, "Overload: <F, A, S, T> failed");

            auto o15 = hello_world | hash<hash_format::raw, hash_site::separate, A, uint8_t>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o15 == ref, "Overload: <F, S, A, T> failed");

            auto o16 = hello_world | hash<hash_format::raw, hash_site::separate, uint8_t, A>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o16 == ref, "Overload: <F, S, T, A> failed");

            auto o17 = hello_world | hash<hash_format::raw, uint8_t, A, hash_site::separate>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o17 == ref, "Overload: <F, T, A, S> failed");

            auto o18 = hello_world | hash<hash_format::raw, uint8_t, hash_site::separate, A>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o18 == ref, "Overload: <F, T, S, A> failed");

            auto o19 = hello_world | hash<uint8_t, A, hash_format::raw, hash_site::separate>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o19 == ref, "Overload: <T, A, F, S> failed");

            auto o20 = hello_world | hash<uint8_t, A, hash_site::separate, hash_format::raw>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o20 == ref, "Overload: <T, A, S, F> failed");

            auto o21 = hello_world | hash<uint8_t, hash_site::separate, A, hash_format::raw>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o21 == ref, "Overload: <T, S, A, F> failed");

            auto o22 = hello_world | hash<uint8_t, hash_site::separate, hash_format::raw, A>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o22 == ref, "Overload: <T, S, F, A> failed");

            auto o23 = hello_world | hash<uint8_t, hash_format::raw, A, hash_site::separate>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o23 == ref, "Overload: <T, F, A, S> failed");

            auto o24 = hello_world | hash<uint8_t, hash_format::raw, hash_site::separate, A>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o24 == ref, "Overload: <T, F, S, A> failed");
        }
        {
            // uint8_t hash_overloads with three explicit types, all should match ref
            auto o1 = hello_world | hash<A, uint8_t, hash_format::raw>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o1 == ref, "Overload: <A, T, F> failed");

            auto o2 = hello_world | hash<A, uint8_t, hash_site::separate>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o2 == ref, "Overload: <A, T, S> failed");

            auto o3 = hello_world | hash<A, hash_site::separate, uint8_t>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o3 == ref, "Overload: <A, S, T> failed");

            auto o4 = hello_world | hash<A, hash_site::separate, hash_format::raw>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o4 == ref, "Overload: <A, S, F> failed");

            auto o5 = hello_world | hash<A, hash_format::raw, uint8_t>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o5 == ref, "Overload: <A, F, T> failed");

            auto o6 = hello_world | hash<A, hash_format::raw, hash_site::separate>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o6 == ref, "Overload: <A, F, S> failed");

            auto o7 = hello_world | hash<hash_site::separate, A, uint8_t>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o7 == ref, "Overload: <S, A, T> failed");

            auto o8 = hello_world | hash<hash_site::separate, A, hash_format::raw>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o8 == ref, "Overload: <S, A, F> failed");

            auto o9 = hello_world | hash<hash_site::separate, hash_format::raw, A>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o9 == ref, "Overload: <S, F, A> failed");

            auto o10 = hello_world | hash<hash_site::separate, uint8_t, A>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o10 == ref, "Overload: <S, T, A> failed");

            auto o11 = hello_world | hash<hash_format::raw, A, uint8_t>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o11 == ref, "Overload: <F, A, T> failed");

            auto o12 = hello_world | hash<hash_format::raw, A, hash_site::separate>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o12 == ref, "Overload: <F, A, S> failed");

            auto o13 = hello_world | hash<hash_format::raw, hash_site::separate, A>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o13 == ref, "Overload: <F, S, A> failed");

            auto o14 = hello_world | hash<hash_format::raw, uint8_t, A>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o14 == ref, "Overload: <F, T, A> failed");

            auto o15 = hello_world | hash<uint8_t, A, hash_format::raw>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o15 == ref, "Overload: <T, A, F> failed");

            auto o16 = hello_world | hash<uint8_t, A, hash_site::separate>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o16 == ref, "Overload: <T, A, S> failed");

            auto o17 = hello_world | hash<uint8_t, hash_site::separate, A>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o17 == ref, "Overload: <T, S, A> failed");

            auto o18 = hello_world | hash<uint8_t, hash_format::raw, A>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o18 == ref, "Overload: <T, F, A> failed");

            if constexpr (A == sph::hash_algorithm::blake2b)
            {
                auto o19 = hello_world | hash<hash_site::separate, hash_format::raw, uint8_t>(hash_size) | std::ranges::to<std::vector>();
                CHECK_MESSAGE(o19 == ref, "Overload: <S, F, T> failed");

                auto o20 = hello_world | hash<hash_site::separate, uint8_t, hash_format::raw>(hash_size) | std::ranges::to<std::vector>();
                CHECK_MESSAGE(o20 == ref, "Overload: <S, T, F> failed");

                auto o21 = hello_world | hash<hash_format::raw, hash_site::separate, uint8_t>(hash_size) | std::ranges::to<std::vector>();
                CHECK_MESSAGE(o21 == ref, "Overload: <F, S, T> failed");

                auto o22 = hello_world | hash<hash_format::raw, uint8_t, hash_site::separate>(hash_size) | std::ranges::to<std::vector>();
                CHECK_MESSAGE(o22 == ref, "Overload: <F, T, S> failed");

                auto o23 = hello_world | hash<uint8_t, hash_site::separate, hash_format::raw>(hash_size) | std::ranges::to<std::vector>();
                CHECK_MESSAGE(o23 == ref, "Overload: <T, S, F> failed");

                auto o24 = hello_world | hash<uint8_t, hash_format::raw, hash_site::separate>(hash_size) | std::ranges::to<std::vector>();
                CHECK_MESSAGE(o24 == ref, "Overload: <T, F, S> failed");
            }
        }
        {
            // uint8_t hash_overloads with two explicit types, all should match ref
            auto o1 = hello_world | hash<A, uint8_t>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o1 == ref, "Overload: <A, T> failed");

            auto o2 = hello_world | hash<A, hash_site::separate>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o2 == ref, "Overload: <A, S> failed");

            auto o3 = hello_world | hash<A, hash_format::raw>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o3 == ref, "Overload: <A, F> failed");

            auto o4 = hello_world | hash<hash_site::separate, A>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o4 == ref, "Overload: <S, A> failed");

            auto o5 = hello_world | hash<hash_format::raw, A>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o5 == ref, "Overload: <F, A> failed");

            auto o6 = hello_world | hash<uint8_t, A>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o6 == ref, "Overload: <T, A> failed");

            if constexpr (A == sph::hash_algorithm::blake2b)
            {
                auto o7 = hello_world | hash<hash_site::separate, hash_format::raw>(hash_size) | std::ranges::to<std::vector>();
                CHECK_MESSAGE(o7 == ref, "Overload: <S, F> failed");

                auto o8 = hello_world | hash<hash_site::separate, uint8_t>(hash_size) | std::ranges::to<std::vector>();
                CHECK_MESSAGE(o8 == ref, "Overload: <S, T> failed");

                auto o9 = hello_world | hash<hash_format::raw, hash_site::separate>(hash_size) | std::ranges::to<std::vector>();
                CHECK_MESSAGE(o9 == ref, "Overload: <F, S> failed");

                auto o10 = hello_world | hash<hash_format::raw, uint8_t>(hash_size) | std::ranges::to<std::vector>();
                CHECK_MESSAGE(o10 == ref, "Overload: <F, T> failed");

                auto o11 = hello_world | hash<uint8_t, hash_site::separate>(hash_size) | std::ranges::to<std::vector>();
                CHECK_MESSAGE(o11 == ref, "Overload: <T, S> failed");

                auto o12 = hello_world | hash<uint8_t, hash_format::raw>(hash_size) | std::ranges::to<std::vector>();
                CHECK_MESSAGE(o12 == ref, "Overload: <T, F> failed");
            }
        }

        {
            // uint8_t hash_overloads with one explicit type, all should match ref
            auto o1 = hello_world | hash<A>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o1 == ref, "Overload: <A> failed");

            if constexpr(A == sph::hash_algorithm::blake2b)
            {
                auto o2 = hello_world | hash<hash_site::separate>(hash_size) | std::ranges::to<std::vector>();
                CHECK_MESSAGE(o2 == ref, "Overload: <S> failed");

                auto o3 = hello_world | hash<hash_format::raw>(hash_size) | std::ranges::to<std::vector>();
                CHECK_MESSAGE(o3 == ref, "Overload: <F> failed");

                auto o4 = hello_world | hash<uint8_t>(hash_size) | std::ranges::to<std::vector>();
                CHECK_MESSAGE(o4 == ref, "Overload: <T> failed");
            }
        }

        if constexpr (A == sph::hash_algorithm::blake2b)
        {
            auto o1 = hello_world | hash(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o1 == ref, "Overload: <> failed");
        }
    }

    template <sph::hash_algorithm A>
    auto hash_to_size_t() -> void
    {
        using sph::hash_format;
        using sph::hash_site;
        using sph::views::hash;

        std::vector<uint8_t> hello_world{ 'h','e','l','l','o',' ','w','o','r','l','d' };
        size_t hash_size = 32;
        static_assert(!sph::ranges::views::detail::is_one_byte<size_t>);

        // Fully specified reference: size_t output, padded format
        auto const ref = hello_world
            | hash<A, size_t, hash_format::padded, hash_site::separate>(hash_size)
            | std::ranges::to<std::vector>();

        {
            auto o1 = hello_world | hash<A, size_t, hash_format::padded, hash_site::separate>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o1 == ref, "Overload: <A, T, F, S> failed");

            auto o2 = hello_world | hash<A, size_t, hash_site::separate, hash_format::padded>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o2 == ref, "Overload: <A, T, S, F> failed");

            auto o3 = hello_world | hash<A, hash_site::separate, size_t, hash_format::padded>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o3 == ref, "Overload: <A, S, T, F> failed");

            auto o4 = hello_world | hash<A, hash_site::separate, hash_format::padded, size_t>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o4 == ref, "Overload: <A, S, F, T> failed");

            auto o5 = hello_world | hash<A, hash_format::padded, size_t, hash_site::separate>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o5 == ref, "Overload: <A, F, T, S> failed");

            auto o6 = hello_world | hash<A, hash_format::padded, hash_site::separate, size_t>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o6 == ref, "Overload: <A, F, S, T> failed");

            auto o7 = hello_world | hash<hash_site::separate, A, size_t, hash_format::padded>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o7 == ref, "Overload: <S, A, T, F> failed");

            auto o8 = hello_world | hash<hash_site::separate, A, hash_format::padded, size_t>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o8 == ref, "Overload: <S, A, F, T> failed");

            auto o9 = hello_world | hash<hash_site::separate, hash_format::padded, A, size_t>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o9 == ref, "Overload: <S, F, A, T> failed");

            auto o10 = hello_world | hash<hash_site::separate, hash_format::padded, size_t, A>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o10 == ref, "Overload: <S, F, T, A> failed");

            auto o11 = hello_world | hash<hash_site::separate, size_t, A, hash_format::padded>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o11 == ref, "Overload: <S, T, A, F> failed");

            auto o12 = hello_world | hash<hash_site::separate, size_t, hash_format::padded, A>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o12 == ref, "Overload: <S, T, F, A> failed");

            auto o13 = hello_world | hash<hash_format::padded, A, size_t, hash_site::separate>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o13 == ref, "Overload: <F, A, T, S> failed");

            auto o14 = hello_world | hash<hash_format::padded, A, hash_site::separate, size_t>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o14 == ref, "Overload: <F, A, S, T> failed");

            auto o15 = hello_world | hash<hash_format::padded, hash_site::separate, A, size_t>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o15 == ref, "Overload: <F, S, A, T> failed");

            auto o16 = hello_world | hash<hash_format::padded, hash_site::separate, size_t, A>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o16 == ref, "Overload: <F, S, T, A> failed");

            auto o17 = hello_world | hash<hash_format::padded, size_t, A, hash_site::separate>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o17 == ref, "Overload: <F, T, A, S> failed");

            auto o18 = hello_world | hash<hash_format::padded, size_t, hash_site::separate, A>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o18 == ref, "Overload: <F, T, S, A> failed");

            auto o19 = hello_world | hash<size_t, A, hash_format::padded, hash_site::separate>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o19 == ref, "Overload: <T, A, F, S> failed");

            auto o20 = hello_world | hash<size_t, A, hash_site::separate, hash_format::padded>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o20 == ref, "Overload: <T, A, S, F> failed");

            auto o21 = hello_world | hash<size_t, hash_site::separate, A, hash_format::padded>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o21 == ref, "Overload: <T, S, A, F> failed");

            auto o22 = hello_world | hash<size_t, hash_site::separate, hash_format::padded, A>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o22 == ref, "Overload: <T, S, F, A> failed");

            auto o23 = hello_world | hash<size_t, hash_format::padded, A, hash_site::separate>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o23 == ref, "Overload: <T, F, A, S> failed");

            auto o24 = hello_world | hash<size_t, hash_format::padded, hash_site::separate, A>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o24 == ref, "Overload: <T, F, S, A> failed");
        }

        {
            auto o1 = hello_world | hash<A, size_t, hash_format::padded>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o1 == ref, "Overload: <A, T, F> failed");

            auto o2 = hello_world | hash<A, size_t, hash_site::separate>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o2 == ref, "Overload: <A, T, S> failed");

            auto o3 = hello_world | hash<A, hash_site::separate, size_t>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o3 == ref, "Overload: <A, S, T> failed");

            auto o4 = hello_world | hash<A, hash_format::padded, size_t>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o4 == ref, "Overload: <A, F, T> failed");

            auto o5 = hello_world | hash<hash_site::separate, A, size_t>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o5 == ref, "Overload: <S, A, T> failed");

            auto o6 = hello_world | hash<hash_site::separate, size_t, A>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o6 == ref, "Overload: <S, T, A> failed");

            auto o7 = hello_world | hash<hash_format::padded, A, size_t>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o7 == ref, "Overload: <F, A, T> failed");

            auto o8 = hello_world | hash<hash_format::padded, size_t, A>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o8 == ref, "Overload: <F, T, A> failed");

            auto o9 = hello_world | hash<size_t, A, hash_format::padded>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o9 == ref, "Overload: <T, A, F> failed");

            auto o10 = hello_world | hash<size_t, A, hash_site::separate>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o10 == ref, "Overload: <T, A, S> failed");

            auto o11 = hello_world | hash<size_t, hash_site::separate, A>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o11 == ref, "Overload: <T, S, A> failed");

            auto o12 = hello_world | hash<size_t, hash_format::padded, A>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o12 == ref, "Overload: <T, F, A> failed");

            if constexpr (A == sph::hash_algorithm::blake2b)
            {
                auto o13 = hello_world | hash<hash_site::separate, hash_format::padded, size_t>(hash_size) | std::ranges::to<std::vector>();
                CHECK_MESSAGE(o13 == ref, "Overload: <S, F, T> failed");

                auto o14 = hello_world | hash<hash_site::separate, size_t, hash_format::padded>(hash_size) | std::ranges::to<std::vector>();
                CHECK_MESSAGE(o14 == ref, "Overload: <S, T, F> failed");

                auto o15 = hello_world | hash<hash_format::padded, hash_site::separate, size_t>(hash_size) | std::ranges::to<std::vector>();
                CHECK_MESSAGE(o15 == ref, "Overload: <F, S, T> failed");

                auto o16 = hello_world | hash<hash_format::padded, size_t, hash_site::separate>(hash_size) | std::ranges::to<std::vector>();
                CHECK_MESSAGE(o16 == ref, "Overload: <F, T, S> failed");

                auto o17 = hello_world | hash<size_t, hash_site::separate, hash_format::padded>(hash_size) | std::ranges::to<std::vector>();
                CHECK_MESSAGE(o17 == ref, "Overload: <T, S, F> failed");

                auto o18 = hello_world | hash<size_t, hash_format::padded, hash_site::separate>(hash_size) | std::ranges::to<std::vector>();
                CHECK_MESSAGE(o18 == ref, "Overload: <T, F, S> failed");
            }
        }

        {
            auto o1 = hello_world | hash<A, size_t>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o1 == ref, "Overload: <A, T> failed");

            auto o2 = hello_world | hash<size_t, A>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o2 == ref, "Overload: <T, A> failed");

            if constexpr (A == sph::hash_algorithm::blake2b)
            {
                auto o3 = hello_world | hash<hash_site::separate, size_t>(hash_size) | std::ranges::to<std::vector>();
                CHECK_MESSAGE(o3 == ref, "Overload: <S, T> failed");

                auto o4 = hello_world | hash<hash_format::padded, size_t>(hash_size) | std::ranges::to<std::vector>();
                CHECK_MESSAGE(o4 == ref, "Overload: <F, T> failed");

                auto o5 = hello_world | hash<size_t, hash_site::separate>(hash_size) | std::ranges::to<std::vector>();
                CHECK_MESSAGE(o5 == ref, "Overload: <T, S> failed");

                auto o6 = hello_world | hash<size_t, hash_format::padded>(hash_size) | std::ranges::to<std::vector>();
                CHECK_MESSAGE(o6 == ref, "Overload: <T, F> failed");
            }
        }

        if constexpr (A == sph::hash_algorithm::blake2b)
        {
            auto o1 = hello_world | hash<size_t>(hash_size) | std::ranges::to<std::vector>();
            CHECK_MESSAGE(o1 == ref, "Overload: <T> failed");
        }
    }

    template <sph::hash_algorithm A, sph::ranges::views::detail::hashable_type T>
    auto hash_verify_overloads_separate() -> void
    {
        using sph::hash_site;
        using sph::views::hash;

        std::vector<uint8_t> hello_world{ 'h','e','l','l','o',' ','w','o','r','l','d' };
        size_t hash_size = 32;
        constexpr sph::hash_format hash_format = sph::ranges::views::detail::is_one_byte<T> ? sph::hash_format::raw : sph::hash_format::padded;

        // Fully specified reference: size_t output, padded format
        std::vector<T> const ref
        {
            hello_world
            | hash<A, T, hash_site::separate>(hash_size)
            | std::ranges::to<std::vector>()
        };

        CHECK_EQ(*std::ranges::begin(hello_world | sph::views::hash_verify<A, hash_format>(ref)), true);
        CHECK_EQ(*std::ranges::begin(hello_world | sph::views::hash_verify<hash_format, A>(ref)), true);
        CHECK_EQ(*std::ranges::begin(hello_world | sph::views::hash_verify<A>(ref)), true);
        if constexpr (A == sph::hash_algorithm::blake2b)
        {
            CHECK_EQ(*std::ranges::begin(hello_world | sph::views::hash_verify<hash_format>(ref)), true);
            CHECK_EQ(*std::ranges::begin(hello_world | sph::views::hash_verify(ref)), true);
        }
    }

    template <sph::hash_algorithm A, sph::hash_format F, sph::ranges::views::detail::hashable_type T>
    auto hash_roundtrip_separate() -> void
    {
        // if F is raw, only works with T of size 1, 2, 3, 4, 6, 8, 12, 24
        constexpr auto separate{ sph::hash_site::separate };
        using sph::views::hash;
        using sph::views::hash_verify;
        std::array<unsigned char, 8> to_hash{ {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 }};
        auto ref{ to_hash  | hash<A, F, T, separate>(24) };
        CHECK_EQ(*std::ranges::begin(to_hash | hash_verify(ref)), true);
    }

    template <sph::hash_algorithm A, sph::hash_format F>
    auto hash_roundtrip_separate_types() -> void
    {
        SUBCASE("separate.1")
        {
            hash_roundtrip_separate<A, F, uint8_t>();
        }
        SUBCASE("separate.2")
        {
            hash_roundtrip_separate<A, F, uint16_t>();
        }
        SUBCASE("separate.3")
        {
            hash_roundtrip_separate<A, F, std::array<uint8_t, 3>>();
        }
        SUBCASE("separate.4")
        {
            hash_roundtrip_separate<A, F, uint32_t>();
        }
        SUBCASE("separate.6")
        {
            hash_roundtrip_separate<A, F, std::array<uint8_t, 6>>();
        }
        SUBCASE("separate.8")
        {
            hash_roundtrip_separate<A, F, uint64_t>();
        }
        SUBCASE("separate.12")
        {
            hash_roundtrip_separate<A, F, std::array<uint8_t, 12>>();
        }
        SUBCASE("separate.24")
        {
            hash_roundtrip_separate<A, F, std::array<uint8_t, 24>>();
        }
    }

    template <sph::hash_algorithm A>
    auto hash_roundtrip_separate_types_format() -> void
    {
        SUBCASE("raw")
        {
            hash_roundtrip_separate_types<A, sph::hash_format::raw>();
        }
        SUBCASE("padded")
        {
            hash_roundtrip_separate_types<A, sph::hash_format::padded>();
        }
    }

    template <sph::hash_algorithm A, sph::hash_format F, sph::ranges::views::detail::hashable_type T>
    auto hash_roundtrip_append() -> void
    {
        // if F is raw, only works with T of size 1, 2, 3, 4, 6, 8, 12, 16, 24, 48
        constexpr auto append{ sph::hash_site::append };
        using sph::views::hash;
        using sph::views::hash_verify;
        // hashed_range must be copyable or borrowed.
        auto hashed_range
        {
            std::array<unsigned char, 24>
            {
                {
                    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                    0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
                    0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
                }
            }
            | hash<A, F, T, append>(24) | std::ranges::to<std::vector>() };
        //auto verified_range{ hashed_range | std::ranges::to<std::vector> | hash_verify(24) };
        //CHECK_EQ(*std::ranges::begin(verified_range), true);
    }

    template <sph::hash_algorithm A, sph::hash_format F>
    auto hash_roundtrip_append_types() -> void
    {
        SUBCASE("append.1")
        {
            hash_roundtrip_append<A, F, uint8_t>();
        }
        SUBCASE("append.2")
        {
            hash_roundtrip_append<A, F, uint16_t>();
        }
        SUBCASE("append.3")
        {
            hash_roundtrip_append<A, F, std::array<uint8_t, 3>>();
        }
        SUBCASE("append.4")
        {
            hash_roundtrip_append<A, F, uint32_t>();
        }
        SUBCASE("append.6")
        {
            hash_roundtrip_append<A, F, std::array<uint8_t, 6>>();
        }
        SUBCASE("append.8")
        {
            hash_roundtrip_append<A, F, uint64_t>();
        }
        SUBCASE("append.12")
        {
            hash_roundtrip_append<A, F, std::array<uint8_t, 12>>();
        }
        SUBCASE("append.16")
        {
            hash_roundtrip_append<A, F, std::array<uint8_t, 16>>();
        }
        SUBCASE("append.24")
        {
            hash_roundtrip_append<A, F, std::array<uint8_t, 24>>();
        }
        SUBCASE("append.48")
        {
            hash_roundtrip_append<A, F, std::array<uint8_t, 48>>();
        }
    }
    template <sph::hash_algorithm A>
    auto hash_roundtrip_append_types_format() -> void
    {
        SUBCASE("raw")
        {
            hash_roundtrip_append_types<A, sph::hash_format::raw>();
        }
        SUBCASE("padded")
        {
            hash_roundtrip_append_types<A, sph::hash_format::padded>();
        }
    }
}


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

TEST_CASE("hash.hash_overloads")
{
    SUBCASE("blake2b")
    {
        hash_overloads<sph::hash_algorithm::blake2b>();
    }
    SUBCASE("sha256")
    {
        hash_overloads<sph::hash_algorithm::sha256>();
    }
    SUBCASE("sha512")
    {
        hash_overloads<sph::hash_algorithm::sha512>();
    }
}

TEST_CASE("hash.hash_to_size_t")
{
    SUBCASE("blake2b")
    {
        hash_to_size_t<sph::hash_algorithm::blake2b>();
    }
    SUBCASE("sha256")
    {
        hash_to_size_t<sph::hash_algorithm::sha256>();
    }
    SUBCASE("sha512")
    {
        hash_to_size_t<sph::hash_algorithm::sha512>();
    }
}

TEST_CASE("hash_verify.vectors")
{
    auto test_name{ get_current_test_name() };
    for (auto const [index, test_vector] : std::views::enumerate(blake2b_test_vectors | std::views::filter([](auto&& x) { return x.key.empty() && x.salt.empty() && x.personal.empty(); })))
    {
        auto verify {test_vector.input | sph::views::hash_verify<sph::hash_algorithm::blake2b>(test_vector.out) | std::ranges::to<std::vector>()};
        CHECK_MESSAGE(verify.size() == 1, fmt::format("{}: failed blake on test vector {}", test_name, index));
        CHECK_MESSAGE(verify.front() == true, fmt::format("{}: failed blake on test vector {}", test_name, index));
    }

    for (auto const [index, test_vector] : std::views::enumerate(sha256_test_vectors))
    {
        auto verify = test_vector.input | sph::views::hash_verify<sph::hash_algorithm::sha256>(test_vector.out) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(verify.size() == 1, fmt::format("{}: failed blake on test vector {}", test_name, index));
        CHECK_MESSAGE(verify.front() == true, fmt::format("{}: failed blake on test vector {}", test_name, index));
    }

    for (auto const [index, test_vector] : std::views::enumerate(sha512_test_vectors))
    {
        auto verify = test_vector.input | sph::views::hash_verify<sph::hash_algorithm::sha512>(test_vector.out) | std::ranges::to<std::vector>();
        CHECK_MESSAGE(verify.size() == 1, fmt::format("{}: failed blake on test vector {}", test_name, index));
        CHECK_MESSAGE(verify.front() == true, fmt::format("{}: failed blake on test vector {}", test_name, index));
    }

}

TEST_CASE("hash_verify.overloads.separate")
{
    SUBCASE("blake2b.uint8_t")
    {
        hash_verify_overloads_separate<sph::hash_algorithm::blake2b, uint8_t>();
    }
    SUBCASE("sha256.uint8_t")
    {
        hash_verify_overloads_separate<sph::hash_algorithm::sha256, uint8_t>();
    }
    SUBCASE("sha512.uint8_t")
    {
        hash_verify_overloads_separate<sph::hash_algorithm::sha512, uint8_t>();
    }
    SUBCASE("blake2b.size_t")
    {
        hash_verify_overloads_separate<sph::hash_algorithm::blake2b, size_t>();
    }
    SUBCASE("sha256.size_t")
    {
        hash_verify_overloads_separate<sph::hash_algorithm::sha256, size_t>();
    }
    SUBCASE("sha512.size_t")
    {
        hash_verify_overloads_separate<sph::hash_algorithm::sha512, size_t>();
    }
}
TEST_CASE("roundtrip")
{
    SUBCASE("blake2b.separate")
    {
        hash_roundtrip_separate_types_format<sph::hash_algorithm::blake2b>();
    }
    SUBCASE("sha256.separate")
    {
        hash_roundtrip_separate_types_format<sph::hash_algorithm::sha256>();
    }
    SUBCASE("sha512.separate")
    {
        hash_roundtrip_separate_types_format<sph::hash_algorithm::sha512>();
    }
    SUBCASE("blake2b.append")
    {
        hash_roundtrip_append_types_format<sph::hash_algorithm::blake2b>();
    }
    SUBCASE("sha256.append")
    {
        hash_roundtrip_append_types_format<sph::hash_algorithm::sha256>();
    }
    SUBCASE("sha512.append")
    {
        hash_roundtrip_append_types_format<sph::hash_algorithm::sha512>();
    }
}



TEST_CASE("hexstring_to_hex")
{
    std::vector<uint8_t> const foo {
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
