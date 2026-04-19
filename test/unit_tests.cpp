// ReSharper disable once CppInconsistentNaming
#define _ENABLE_STL_INTERNAL_CHECK  // NOLINT(clang-diagnostic-reserved-macro-identifier)
#include <array>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <magic_enum/magic_enum.hpp>
#include <ranges>
#include <span>
#include <sph/ranges/views/detail/blake2b.h>
#include <sph/ranges/views/detail/blake3.h>
#include <sph/ranges/views/detail/sha3_256.h>
#include <sph/ranges/views/detail/sha3_512.h>
#include <sph/ranges/views/detail/sha256.h>
#include <sph/ranges/views/detail/sha512.h>
#include <sph/ranges/views/hash.h>
#include <sph/ranges/views/hash_verify.h>
#include <sstream>
#include <string_view>
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
        size_t input_repeat_count;
        std::vector<uint8_t> out;
        std::vector<uint8_t> input;
        std::vector<uint8_t> key;
        std::vector<uint8_t> salt;
        std::vector<uint8_t> personal;
    };

    struct test_vector_json
    {
        size_t outlen;
        size_t input_repeat_count;
        std::string out;
        std::string input;
        std::string key;
        std::string salt;
        std::string personal;
        auto test_vector() const -> test_vector
        {
            auto hexstring_to_bytes = [](std::string const& value) -> std::vector<uint8_t>
            {
                return value
                    | std::views::chunk(2)
                    | std::views::transform([](auto&& v) -> uint8_t
                    {
                        return static_cast<uint8_t>(std::stoul(std::string(v.begin(), v.end()), nullptr, 16));
                    })
                    | std::ranges::to<std::vector<uint8_t>>();
            };
            return ::test_vector{
                .outlen = outlen,
                .input_repeat_count = input_repeat_count,
                .out = hexstring_to_bytes(out),
                .input = hexstring_to_bytes(input),
                .key = hexstring_to_bytes(key),
                .salt = hexstring_to_bytes(salt),
                .personal = hexstring_to_bytes(personal)
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
        static constexpr char input_repeat_count[] = "input_repeat_count";
        static constexpr char out[] = "out";
        static constexpr char input[] = "input";
        static constexpr char key[] = "key";
        static constexpr char salt[] = "salt";
        static constexpr char personal[] = "personal";
        using type = json_member_list<
            json_number<outlen, size_t>,
            json_number<input_repeat_count, size_t>,
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
    std::vector<test_vector> const blake3_test_vectors {get_test_vector("blake3.json")};
    std::vector<test_vector> const sha256_test_vectors {get_test_vector("sha256.json")};
    std::vector<test_vector> const sha3_256_test_vectors {get_test_vector("sha3_256.json")};
    std::vector<test_vector> const sha3_512_test_vectors {get_test_vector("sha3_512.json")};
    std::vector<test_vector> const sha512_test_vectors {get_test_vector("sha512.json")};

    auto blake2b_parameters_from_test_vector(test_vector const& v) -> sph::blake2b_parameters
    {
        return sph::blake2b_parameters{
            .key = v.key,
            .salt = v.salt,
            .personal = v.personal
        };
    }

    template<typename H>
    constexpr auto hash_to_byte_vector(H&& hash) -> std::vector<uint8_t>
    {
        using value_t = std::remove_cvref_t<std::ranges::range_value_t<H>>;
         return std::forward<H>(hash)
            | std::views::transform([](value_t v) -> std::array<uint8_t, sizeof(value_t)>
            {
                std::array<uint8_t, sizeof(value_t)> tmp{};
                std::copy_n(reinterpret_cast<uint8_t*>(&v), sizeof(value_t), tmp.data());
                return tmp;
            })
            | std::views::join
            | std::ranges::to<std::vector>();
    }

    template<typename T>
    auto byte_vector_to_hash_vector(std::span<uint8_t const> bytes) -> std::vector<T>
    {
        if (bytes.size() % sizeof(T) != 0)
        {
            throw std::runtime_error(std::format("Byte count {} is not divisible by element size {}", bytes.size(), sizeof(T)));
        }

        std::vector<T> result(bytes.size() / sizeof(T));
        for (size_t index = 0; index < result.size(); ++index)
        {
            std::memcpy(&result[index], bytes.data() + (index * sizeof(T)), sizeof(T));
        }
        return result;
    }

    template <sph::hash_algorithm A>
    struct detail_hash_type;

    template <>
    struct detail_hash_type<sph::hash_algorithm::blake2b> { using type = sph::ranges::views::detail::blake2b; };

    template <>
    struct detail_hash_type<sph::hash_algorithm::blake3> { using type = sph::ranges::views::detail::blake3; };

    template <>
    struct detail_hash_type<sph::hash_algorithm::sha256> { using type = sph::ranges::views::detail::sha256; };

    template <>
    struct detail_hash_type<sph::hash_algorithm::sha3_256> { using type = sph::ranges::views::detail::sha3_256; };

    template <>
    struct detail_hash_type<sph::hash_algorithm::sha3_512> { using type = sph::ranges::views::detail::sha3_512; };

    template <>
    struct detail_hash_type<sph::hash_algorithm::sha512> { using type = sph::ranges::views::detail::sha512; };

    template <sph::hash_algorithm A>
    using detail_hash_t = typename detail_hash_type<A>::type;

    template <typename H>
    auto feed_test_vector(H& hasher, test_vector const& test_vector) -> void
    {
        std::array<uint8_t, H::chunk_size> chunk{};
        size_t filled{};

        auto append_bytes = [&](std::span<uint8_t const> bytes) -> void
        {
            while (!bytes.empty())
            {
                auto const bytes_needed{ H::chunk_size - filled };
                auto const copy_count{ std::min(bytes_needed, bytes.size()) };
                std::memcpy(chunk.data() + filled, bytes.data(), copy_count);
                filled += copy_count;
                bytes = bytes.subspan(copy_count);
                if (filled == H::chunk_size)
                {
                    hasher.update(std::span<uint8_t const, H::chunk_size>{ chunk });
                    filled = 0;
                }
            }
        };

        for (size_t repeat = 0; repeat < test_vector.input_repeat_count; ++repeat)
        {
            append_bytes(test_vector.input);
        }

        hasher.final(std::span<uint8_t const>{ chunk.data(), filled });
    }

    template <sph::hash_algorithm A>
    auto hash_test_vector(test_vector const& test_vector) -> std::vector<uint8_t>
    {
        if constexpr (A == sph::hash_algorithm::blake2b)
        {
            auto hasher{ detail_hash_t<A>{ test_vector.outlen, blake2b_parameters_from_test_vector(test_vector) } };
            feed_test_vector(hasher, test_vector);
            auto hash{ hasher.hash() };
            return { hash.begin(), hash.end() };
        }
        else
        {
            auto hasher{ detail_hash_t<A>{ test_vector.outlen } };
            feed_test_vector(hasher, test_vector);
            auto hash{ hasher.hash() };
            return { hash.begin(), hash.end() };
        }
    }

    template <sph::hash_algorithm A>
    auto verify_test_vector(test_vector const& test_vector) -> std::vector<bool>
    {
        return { hash_test_vector<A>(test_vector) == test_vector.out };
    }

    auto find_padding_terminator(std::vector<uint8_t>& bytes) -> std::vector<uint8_t>::iterator
    {
        size_t current{ bytes.size() };
        while (current > 0 && bytes[current - 1] == 0x00)
        {
            --current;
        }

        if (current == 0 || bytes[current - 1] != 0x80)
        {
            return bytes.end();
        }

        return bytes.begin() + static_cast<ptrdiff_t>(current - 1);
    }

    class single_pass_byte_view : public std::ranges::view_interface<single_pass_byte_view>
    {
        std::string_view data_;

        struct iterator
        {
            using iterator_concept = std::input_iterator_tag;
            using iterator_category = std::input_iterator_tag;
            using value_type = char;
            using difference_type = std::ptrdiff_t;

            std::string_view data{};
            size_t index{};

            auto operator*() const -> char
            {
                return data[index];
            }

            auto operator++() -> iterator&
            {
                ++index;
                return *this;
            }

            auto operator++(int) -> iterator
            {
                auto tmp{ *this };
                ++(*this);
                return tmp;
            }

            auto operator==(std::default_sentinel_t) const -> bool
            {
                return index >= data.size();
            }
        };

    public:
        explicit single_pass_byte_view(std::string_view data) : data_{ data } {}

        auto begin() const -> iterator
        {
            return iterator{ data_, 0 };
        }

        auto end() const -> std::default_sentinel_t
        {
            return {};
        }
    };

    template <sph::hash_algorithm A, typename MakeRange>
    auto check_range_category(std::string_view detail, MakeRange make_range) -> void
    {
        constexpr std::string_view payload{ "hello world" };
        auto const expected{
            payload
            | sph::views::hash<A>(24)
            | std::ranges::to<std::vector>()
        };
        auto const hashed{
            make_range()
            | sph::views::hash<A>(24)
            | std::ranges::to<std::vector>()
        };
        CHECK_MESSAGE(hashed == expected, std::format("{}: hash mismatch", detail));

        auto const verified{
            make_range()
            | sph::views::hash_verify<A>(expected)
            | std::ranges::to<std::vector>()
        };
        CHECK_MESSAGE(verified.size() == 1, std::format("{}: verify result size mismatch", detail));
        CHECK_MESSAGE(verified.front(), std::format("{}: verify failed", detail));
    }

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
        auto const detail{ std::format("{}: {}, {}", magic_enum::enum_name(A), magic_enum::enum_name(F),  typeid(T).name()) };
        constexpr auto separate{ sph::hash_site::separate };
        using sph::views::hash;
        using sph::views::hash_verify;
        T x{};
        (void)x;
        std::array<unsigned char, 8> to_hash{ {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 }};
        auto ref{ to_hash  | hash<A, F, T, separate>(24) };
        if constexpr (F == sph::hash_format::padded)
        {
            // verify padded hash created correctly
            auto ref_vec{ hash_to_byte_vector(ref) };
            bool found{false};
            size_t hash_length{ ref_vec.size() };
            for (uint8_t v : ref_vec | std::views::reverse)
            {
                bool const valid{ v == 0x00 || v == 0x80 };
                --hash_length;
                CHECK_MESSAGE(valid, std::format("{}: Found invalid pad value", detail));
                if (v == 0x80)
                {
                    found = true;
                    break;
                }
            }
            CHECK_MESSAGE(found, std::format("{}: hash all zeros", detail));
            CHECK_MESSAGE(hash_length == 24, std::format("{}: expected 24 byte hash, got {}", detail, hash_length));
            CHECK_MESSAGE(*std::ranges::begin(to_hash | hash_verify<A, F>(ref)), std::format("{}: failed verify", detail));
        }
        else
        {
            auto ref_vec{ hash_to_byte_vector(ref) };
            (void)ref_vec;
            CHECK_MESSAGE(*std::ranges::begin(to_hash | hash_verify<A, F>(ref)), std::format("{}: failed verify", detail));
        }

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
        // Append mode preserves the original input bytes and then appends the hash bytes.
        // For padded hashes, the suffix may be longer than the raw hash due to padding.
        constexpr auto append{ sph::hash_site::append };
        using sph::views::hash;
        std::array<unsigned char, 24> const to_hash
        {
            {
                0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
                0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
            }
        };
        auto hashed_range
        {
            to_hash
            | hash<A, F, T, append>(24)
            | std::ranges::to<std::vector>()
        };
        auto const bytes{ hash_to_byte_vector(hashed_range) };
        auto const expected_raw_hash{
            to_hash
            | hash<A, uint8_t, sph::hash_format::raw>(24)
            | std::ranges::to<std::vector>()
        };

        CHECK(std::ranges::equal(bytes | std::views::take(to_hash.size()), to_hash));

        std::vector<uint8_t> const suffix(bytes.begin() + static_cast<ptrdiff_t>(to_hash.size()), bytes.end());
        if constexpr (F == sph::hash_format::raw)
        {
            CHECK(suffix == expected_raw_hash);
        }
        else
        {
            auto padded_suffix{ suffix };
            auto terminator = find_padding_terminator(padded_suffix);
            REQUIRE(terminator != padded_suffix.end());
            CHECK(static_cast<size_t>(std::distance(padded_suffix.begin(), terminator)) == expected_raw_hash.size());
            CHECK(std::ranges::equal(expected_raw_hash, padded_suffix | std::views::take(expected_raw_hash.size())));
            CHECK(std::ranges::all_of(std::next(terminator), padded_suffix.end(), [](uint8_t value) { return value == 0x00; }));
        }
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
    for (auto const [index, test_vector] : std::views::enumerate(blake2b_test_vectors))
    {
        auto hash = hash_test_vector<sph::hash_algorithm::blake2b>(test_vector);
        CHECK_MESSAGE(hash == test_vector.out, fmt::format("{}: failed blake on test vector {}", test_name, index));
    }

    for (auto const [index, test_vector] : std::views::enumerate(blake3_test_vectors))
    {
        auto hash = hash_test_vector<sph::hash_algorithm::blake3>(test_vector);
        CHECK_MESSAGE(hash == test_vector.out, fmt::format("{}: failed blake3 on test vector {}", test_name, index));
    }

    for (auto const [index, test_vector] : std::views::enumerate(sha256_test_vectors))
    {
        auto hash = hash_test_vector<sph::hash_algorithm::sha256>(test_vector);
        CHECK_MESSAGE(hash == test_vector.out, fmt::format("{}: failed sha256 on test vector {}", test_name, index));
    }

    for (auto const [index, test_vector] : std::views::enumerate(sha3_256_test_vectors))
    {
        auto hash = hash_test_vector<sph::hash_algorithm::sha3_256>(test_vector);
        CHECK_MESSAGE(hash == test_vector.out, fmt::format("{}: failed sha3/256 on test vector {}", test_name, index));
    }

    for (auto const [index, test_vector] : std::views::enumerate(sha3_512_test_vectors))
    {
        auto hash = hash_test_vector<sph::hash_algorithm::sha3_512>(test_vector);
        CHECK_MESSAGE(hash == test_vector.out, fmt::format("{}: failed sha3/512 on test vector {}", test_name, index));
    }

    for (auto const [index, test_vector] : std::views::enumerate(sha512_test_vectors))
    {
        auto hash = hash_test_vector<sph::hash_algorithm::sha512>(test_vector);
        CHECK_MESSAGE(hash == test_vector.out, fmt::format("{}: failed sha512 on test vector {}", test_name, index));
    }
}

TEST_CASE("hash.hash_overloads")
{
    SUBCASE("blake2b")
    {
        hash_overloads<sph::hash_algorithm::blake2b>();
    }
    SUBCASE("blake3")
    {
        hash_overloads<sph::hash_algorithm::blake3>();
    }
    SUBCASE("sha256")
    {
        hash_overloads<sph::hash_algorithm::sha256>();
    }
    SUBCASE("sha3_256")
    {
        hash_overloads<sph::hash_algorithm::sha3_256>();
    }
    SUBCASE("sha3_512")
    {
        hash_overloads<sph::hash_algorithm::sha3_512>();
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
    SUBCASE("blake3")
    {
        hash_to_size_t<sph::hash_algorithm::blake3>();
    }
    SUBCASE("sha256")
    {
        hash_to_size_t<sph::hash_algorithm::sha256>();
    }
    SUBCASE("sha3_256")
    {
        hash_to_size_t<sph::hash_algorithm::sha3_256>();
    }
    SUBCASE("sha3_512")
    {
        hash_to_size_t<sph::hash_algorithm::sha3_512>();
    }
    SUBCASE("sha512")
    {
        hash_to_size_t<sph::hash_algorithm::sha512>();
    }
}

TEST_CASE("hash_verify.vectors")
{
    auto test_name{ get_current_test_name() };
    for (auto const [index, test_vector] : std::views::enumerate(blake2b_test_vectors))
    {
        auto verify { verify_test_vector<sph::hash_algorithm::blake2b>(test_vector) };
        CHECK_MESSAGE(verify.size() == 1, fmt::format("{}: failed blake on test vector {}", test_name, index));
        CHECK_MESSAGE(verify.front() == true, fmt::format("{}: failed blake on test vector {}", test_name, index));
    }

    for (auto const [index, test_vector] : std::views::enumerate(blake3_test_vectors))
    {
        auto verify { verify_test_vector<sph::hash_algorithm::blake3>(test_vector) };
        CHECK_MESSAGE(verify.size() == 1, fmt::format("{}: failed blake3 on test vector {}", test_name, index));
        CHECK_MESSAGE(verify.front() == true, fmt::format("{}: failed blake3 on test vector {}", test_name, index));
    }

    for (auto const [index, test_vector] : std::views::enumerate(sha256_test_vectors))
    {
        auto verify = verify_test_vector<sph::hash_algorithm::sha256>(test_vector);
        CHECK_MESSAGE(verify.size() == 1, fmt::format("{}: failed blake on test vector {}", test_name, index));
        CHECK_MESSAGE(verify.front() == true, fmt::format("{}: failed blake on test vector {}", test_name, index));
    }

    for (auto const [index, test_vector] : std::views::enumerate(sha3_256_test_vectors))
    {
        auto verify = verify_test_vector<sph::hash_algorithm::sha3_256>(test_vector);
        CHECK_MESSAGE(verify.size() == 1, fmt::format("{}: failed sha3/256 on test vector {}", test_name, index));
        CHECK_MESSAGE(verify.front() == true, fmt::format("{}: failed sha3/256 on test vector {}", test_name, index));
    }

    for (auto const [index, test_vector] : std::views::enumerate(sha3_512_test_vectors))
    {
        auto verify = verify_test_vector<sph::hash_algorithm::sha3_512>(test_vector);
        CHECK_MESSAGE(verify.size() == 1, fmt::format("{}: failed sha3/512 on test vector {}", test_name, index));
        CHECK_MESSAGE(verify.front() == true, fmt::format("{}: failed sha3/512 on test vector {}", test_name, index));
    }

    for (auto const [index, test_vector] : std::views::enumerate(sha512_test_vectors))
    {
        auto verify = verify_test_vector<sph::hash_algorithm::sha512>(test_vector);
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
    SUBCASE("blake3.uint8_t")
    {
        hash_verify_overloads_separate<sph::hash_algorithm::blake3, uint8_t>();
    }
    SUBCASE("sha256.uint8_t")
    {
        hash_verify_overloads_separate<sph::hash_algorithm::sha256, uint8_t>();
    }
    SUBCASE("sha3_256.uint8_t")
    {
        hash_verify_overloads_separate<sph::hash_algorithm::sha3_256, uint8_t>();
    }
    SUBCASE("sha3_512.uint8_t")
    {
        hash_verify_overloads_separate<sph::hash_algorithm::sha3_512, uint8_t>();
    }
    SUBCASE("sha512.uint8_t")
    {
        hash_verify_overloads_separate<sph::hash_algorithm::sha512, uint8_t>();
    }
    SUBCASE("blake2b.size_t")
    {
        hash_verify_overloads_separate<sph::hash_algorithm::blake2b, size_t>();
    }
    SUBCASE("blake3.size_t")
    {
        hash_verify_overloads_separate<sph::hash_algorithm::blake3, size_t>();
    }
    SUBCASE("sha256.size_t")
    {
        hash_verify_overloads_separate<sph::hash_algorithm::sha256, size_t>();
    }
    SUBCASE("sha3_256.size_t")
    {
        hash_verify_overloads_separate<sph::hash_algorithm::sha3_256, size_t>();
    }
    SUBCASE("sha3_512.size_t")
    {
        hash_verify_overloads_separate<sph::hash_algorithm::sha3_512, size_t>();
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
    SUBCASE("blake3.separate")
    {
        hash_roundtrip_separate_types_format<sph::hash_algorithm::blake3>();
    }
    SUBCASE("sha256.separate")
    {
        hash_roundtrip_separate_types_format<sph::hash_algorithm::sha256>();
    }
    SUBCASE("sha3_256.separate")
    {
        hash_roundtrip_separate_types_format<sph::hash_algorithm::sha3_256>();
    }
    SUBCASE("sha3_512.separate")
    {
        hash_roundtrip_separate_types_format<sph::hash_algorithm::sha3_512>();
    }
    SUBCASE("sha512.separate")
    {
        hash_roundtrip_separate_types_format<sph::hash_algorithm::sha512>();
    }
    SUBCASE("blake2b.append")
    {
        hash_roundtrip_append_types_format<sph::hash_algorithm::blake2b>();
    }
    SUBCASE("blake3.append")
    {
        hash_roundtrip_append_types_format<sph::hash_algorithm::blake3>();
    }
    SUBCASE("sha256.append")
    {
        hash_roundtrip_append_types_format<sph::hash_algorithm::sha256>();
    }
    SUBCASE("sha3_256.append")
    {
        hash_roundtrip_append_types_format<sph::hash_algorithm::sha3_256>();
    }
    SUBCASE("sha3_512.append")
    {
        hash_roundtrip_append_types_format<sph::hash_algorithm::sha3_512>();
    }
    SUBCASE("sha512.append")
    {
        hash_roundtrip_append_types_format<sph::hash_algorithm::sha512>();
    }
}

TEST_CASE("hash_verify.negative")
{
    std::vector<uint8_t> const input{ 'h', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd' };

    SUBCASE("blake2b.corrupted")
    {
        auto hash = input | sph::views::hash<sph::hash_algorithm::blake2b>(24) | std::ranges::to<std::vector>();
        hash.front() ^= 0xFF;
        auto verify = input | sph::views::hash_verify<sph::hash_algorithm::blake2b>(hash) | std::ranges::to<std::vector>();
        CHECK_FALSE(verify.front());
    }

    SUBCASE("blake2b.truncated")
    {
        auto hash = input | sph::views::hash<sph::hash_algorithm::blake2b>(24) | std::ranges::to<std::vector>();
        hash.pop_back();
        auto verify = input | sph::views::hash_verify<sph::hash_algorithm::blake2b>(hash) | std::ranges::to<std::vector>();
        CHECK_FALSE(verify.front());
    }

    SUBCASE("blake2b.extra.trailing.byte")
    {
        auto hash = input | sph::views::hash<sph::hash_algorithm::blake2b>(24) | std::ranges::to<std::vector>();
        hash.push_back(0x00);
        auto verify = input | sph::views::hash_verify<sph::hash_algorithm::blake2b>(hash) | std::ranges::to<std::vector>();
        CHECK_FALSE(verify.front());
    }

    SUBCASE("wrong.algorithm")
    {
        auto hash = input | sph::views::hash<sph::hash_algorithm::sha256>(24) | std::ranges::to<std::vector>();
        auto verify = input | sph::views::hash_verify<sph::hash_algorithm::blake2b>(hash) | std::ranges::to<std::vector>();
        CHECK_FALSE(verify.front());
    }

    SUBCASE("append.corrupted")
    {
        auto appended = input
            | sph::views::hash<sph::hash_algorithm::sha256, uint8_t, sph::hash_format::raw, sph::hash_site::append>(24)
            | std::ranges::to<std::vector>();
        appended.back() ^= 0xFF;
        auto const expected_hash = input | sph::views::hash<sph::hash_algorithm::sha256>(24) | std::ranges::to<std::vector>();
        CHECK_FALSE(std::ranges::equal(appended | std::views::drop(input.size()), expected_hash));
    }
}

TEST_CASE("hash.boundary_values")
{
    std::vector<uint8_t> const empty_input{};
    std::vector<uint8_t> const some_input{ 0x01, 0x02, 0x03 };

    SUBCASE("blake2b.zero.uses.maximum")
    {
        auto hash = empty_input | sph::views::hash<sph::hash_algorithm::blake2b>() | std::ranges::to<std::vector>();
        CHECK(hash.size() == sph::hash_param<sph::hash_algorithm::blake2b>::hash_byte_count());
    }

    SUBCASE("sha256.zero.uses.maximum")
    {
        auto hash = empty_input | sph::views::hash<sph::hash_algorithm::sha256>() | std::ranges::to<std::vector>();
        CHECK(hash.size() == sph::hash_param<sph::hash_algorithm::sha256>::hash_byte_count());
    }

    SUBCASE("sha3_256.zero.uses.maximum")
    {
        auto hash = empty_input | sph::views::hash<sph::hash_algorithm::sha3_256>() | std::ranges::to<std::vector>();
        CHECK(hash.size() == sph::hash_param<sph::hash_algorithm::sha3_256>::hash_byte_count());
    }

    SUBCASE("sha3_512.zero.uses.maximum")
    {
        auto hash = empty_input | sph::views::hash<sph::hash_algorithm::sha3_512>() | std::ranges::to<std::vector>();
        CHECK(hash.size() == sph::hash_param<sph::hash_algorithm::sha3_512>::hash_byte_count());
    }

    SUBCASE("sha512.zero.uses.maximum")
    {
        auto hash = empty_input | sph::views::hash<sph::hash_algorithm::sha512>() | std::ranges::to<std::vector>();
        CHECK(hash.size() == sph::hash_param<sph::hash_algorithm::sha512>::hash_byte_count());
    }

    SUBCASE("blake3.zero.uses.maximum")
    {
        auto hash = empty_input | sph::views::hash<sph::hash_algorithm::blake3>() | std::ranges::to<std::vector>();
        CHECK(hash.size() == sph::hash_param<sph::hash_algorithm::blake3>::hash_byte_count());
    }

    SUBCASE("maximum.size.accepted")
    {
        auto hash = some_input | sph::views::hash<sph::hash_algorithm::sha256>(sph::hash_param<sph::hash_algorithm::sha256>::hash_byte_count()) | std::ranges::to<std::vector>();
        CHECK(hash.size() == sph::hash_param<sph::hash_algorithm::sha256>::hash_byte_count());
    }

    SUBCASE("oversized.blake2b.rejected")
    {
        bool threw{ false };
        try
        {
            auto unused = some_input | sph::views::hash<sph::hash_algorithm::blake2b>(sph::hash_param<sph::hash_algorithm::blake2b>::hash_byte_count() + 1) | std::ranges::to<std::vector>();
            (void)unused;
        }
        catch (std::invalid_argument const&)
        {
            threw = true;
        }
        CHECK(threw);
    }

    SUBCASE("oversized.sha256.rejected")
    {
        bool threw{ false };
        try
        {
            auto unused = some_input | sph::views::hash<sph::hash_algorithm::sha256>(sph::hash_param<sph::hash_algorithm::sha256>::hash_byte_count() + 1) | std::ranges::to<std::vector>();
            (void)unused;
        }
        catch (std::invalid_argument const&)
        {
            threw = true;
        }
        CHECK(threw);
    }

    SUBCASE("oversized.sha3_256.rejected")
    {
        bool threw{ false };
        try
        {
            auto unused = some_input | sph::views::hash<sph::hash_algorithm::sha3_256>(sph::hash_param<sph::hash_algorithm::sha3_256>::hash_byte_count() + 1) | std::ranges::to<std::vector>();
            (void)unused;
        }
        catch (std::invalid_argument const&)
        {
            threw = true;
        }
        CHECK(threw);
    }

    SUBCASE("oversized.sha3_512.rejected")
    {
        bool threw{ false };
        try
        {
            auto unused = some_input | sph::views::hash<sph::hash_algorithm::sha3_512>(sph::hash_param<sph::hash_algorithm::sha3_512>::hash_byte_count() + 1) | std::ranges::to<std::vector>();
            (void)unused;
        }
        catch (std::invalid_argument const&)
        {
            threw = true;
        }
        CHECK(threw);
    }

    SUBCASE("oversized.sha512.rejected")
    {
        bool threw{ false };
        try
        {
            auto unused = some_input | sph::views::hash<sph::hash_algorithm::sha512>(sph::hash_param<sph::hash_algorithm::sha512>::hash_byte_count() + 1) | std::ranges::to<std::vector>();
            (void)unused;
        }
        catch (std::invalid_argument const&)
        {
            threw = true;
        }
        CHECK(threw);
    }

    SUBCASE("oversized.blake3.rejected")
    {
        bool threw{ false };
        try
        {
            auto unused = some_input | sph::views::hash<sph::hash_algorithm::blake3>(sph::hash_param<sph::hash_algorithm::blake3>::hash_byte_count() + 1) | std::ranges::to<std::vector>();
            (void)unused;
        }
        catch (std::invalid_argument const&)
        {
            threw = true;
        }
        CHECK(threw);
    }
}

TEST_CASE("hash.blake2b.parameter_validation")
{
    std::vector<uint8_t> const input{ 0x01, 0x02, 0x03 };

    SUBCASE("oversized.key.rejected")
    {
        std::vector<uint8_t> key(65, 0x01);
        auto hasher = sph::views::hash<sph::hash_algorithm::blake2b>(24)
            .with_blake2b_parameters(sph::blake2b_parameters{ .key = key });
        bool threw{ false };
        try
        {
            auto unused = input | hasher | std::ranges::to<std::vector>();
            (void)unused;
        }
        catch (std::invalid_argument const&)
        {
            threw = true;
        }
        CHECK(threw);
    }

    SUBCASE("bad.salt.size.rejected")
    {
        std::array<uint8_t, 15> salt{};
        auto hasher = sph::views::hash<sph::hash_algorithm::blake2b>(24)
            .with_blake2b_parameters(sph::blake2b_parameters{ .salt = salt });
        bool threw{ false };
        try
        {
            auto unused = input | hasher | std::ranges::to<std::vector>();
            (void)unused;
        }
        catch (std::invalid_argument const&)
        {
            threw = true;
        }
        CHECK(threw);
    }

    SUBCASE("bad.personal.size.rejected")
    {
        std::array<uint8_t, 15> personal{};
        auto hasher = sph::views::hash<sph::hash_algorithm::blake2b>(24)
            .with_blake2b_parameters(sph::blake2b_parameters{ .personal = personal });
        bool threw{ false };
        try
        {
            auto unused = input | hasher | std::ranges::to<std::vector>();
            (void)unused;
        }
        catch (std::invalid_argument const&)
        {
            threw = true;
        }
        CHECK(threw);
    }
}

TEST_CASE("hash_verify.padding_rejection")
{
    std::vector<uint8_t> const input{ 'p', 'a', 'd', 'd', 'e', 'd' };

    SUBCASE("sha256.missing.terminator")
    {
        auto padded = input | sph::views::hash<sph::hash_algorithm::sha256, size_t, sph::hash_format::padded>(24) | std::ranges::to<std::vector>();
        auto bytes = hash_to_byte_vector(padded);
        auto terminator = find_padding_terminator(bytes);
        REQUIRE(terminator != bytes.end());
        *terminator = 0x00;
        auto corrupted = byte_vector_to_hash_vector<size_t>(bytes);
        auto verify = input | sph::views::hash_verify<sph::hash_algorithm::sha256, sph::hash_format::padded>(corrupted) | std::ranges::to<std::vector>();
        CHECK_FALSE(verify.front());
    }

    SUBCASE("sha512.nonzero.trailing.padding")
    {
        auto padded = input | sph::views::hash<sph::hash_algorithm::sha512, size_t, sph::hash_format::padded>(24) | std::ranges::to<std::vector>();
        auto bytes = hash_to_byte_vector(padded);
        auto terminator = find_padding_terminator(bytes);
        REQUIRE(terminator != bytes.end());
        REQUIRE(std::next(terminator) != bytes.end());
        *std::next(terminator) = 0x01;
        auto corrupted = byte_vector_to_hash_vector<size_t>(bytes);
        auto verify = input | sph::views::hash_verify<sph::hash_algorithm::sha512, sph::hash_format::padded>(corrupted) | std::ranges::to<std::vector>();
        CHECK_FALSE(verify.front());
    }

    SUBCASE("blake2b.appended.missing.terminator")
    {
        auto appended = input
            | sph::views::hash<sph::hash_algorithm::blake2b, size_t, sph::hash_format::padded, sph::hash_site::append>(24)
            | std::ranges::to<std::vector>();
        auto bytes = hash_to_byte_vector(appended);
        auto terminator = find_padding_terminator(bytes);
        REQUIRE(terminator != bytes.end());
        *terminator = 0x00;
        auto corrupted = byte_vector_to_hash_vector<size_t>(bytes);
        auto verify = corrupted | sph::views::hash_verify<sph::hash_algorithm::blake2b, sph::hash_format::padded>(24) | std::ranges::to<std::vector>();
        CHECK_FALSE(verify.front());
    }
}

TEST_CASE("hash.range_categories")
{
    std::array<uint8_t, 11> const span_payload{ { 'h', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd' } };
    std::vector<uint8_t> const const_payload(span_payload.begin(), span_payload.end());

    SUBCASE("string_view")
    {
        check_range_category<sph::hash_algorithm::sha256>("string_view", [] { return std::string_view{ "hello world" }; });
    }

    SUBCASE("span")
    {
        check_range_category<sph::hash_algorithm::sha256>("span", [&] { return std::span<uint8_t const>{ span_payload }; });
    }

    SUBCASE("temporary.range")
    {
        check_range_category<sph::hash_algorithm::sha256>("temporary.range", [] { return std::vector<uint8_t>{ 'h', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd' }; });
    }

    SUBCASE("const.range")
    {
        check_range_category<sph::hash_algorithm::sha256>("const.range", [&] { return std::views::all(const_payload); });
    }

    SUBCASE("single.pass.input.range")
    {
        check_range_category<sph::hash_algorithm::sha256>("single.pass.input.range", [] { return single_pass_byte_view{ "hello world" }; });
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
