#ifndef INCLUDED_SRC_BUILDTOOL_BUILD_ENGINE_EXPRESSION_EXPRESSION_PTR_HPP
#define INCLUDED_SRC_BUILDTOOL_BUILD_ENGINE_EXPRESSION_EXPRESSION_PTR_HPP

#include <functional>
#include <memory>
#include <string>
#include <type_traits>

#include "nlohmann/json.hpp"
#include "src/buildtool/build_engine/expression/function_map.hpp"
#include "src/buildtool/build_engine/expression/linked_map.hpp"
#include "src/buildtool/logging/logger.hpp"

class Configuration;
class Expression;

class ExpressionPtr {
  public:
    // Initialize to nullptr
    explicit ExpressionPtr(std::nullptr_t /*ptr*/) noexcept : ptr_{nullptr} {}

    // Initialize from Expression's variant type or Expression
    template <class T>
    requires(not std::is_same_v<std::remove_cvref_t<T>, ExpressionPtr>)
        // NOLINTNEXTLINE(bugprone-forwarding-reference-overload)
        explicit ExpressionPtr(T&& data) noexcept
        : ptr_{std::make_shared<Expression>(std::forward<T>(data))} {}

    ExpressionPtr() noexcept;
    ExpressionPtr(ExpressionPtr const&) noexcept = default;
    ExpressionPtr(ExpressionPtr&&) noexcept = default;
    ~ExpressionPtr() noexcept = default;
    auto operator=(ExpressionPtr const&) noexcept -> ExpressionPtr& = default;
    auto operator=(ExpressionPtr&&) noexcept -> ExpressionPtr& = default;

    explicit operator bool() const { return static_cast<bool>(ptr_); }
    [[nodiscard]] auto operator*() & -> Expression& { return *ptr_; }
    [[nodiscard]] auto operator*() const& -> Expression const& { return *ptr_; }
    [[nodiscard]] auto operator*() && -> Expression = delete;
    [[nodiscard]] auto operator->() const& -> Expression const* {
        return ptr_.get();
    }
    [[nodiscard]] auto operator->() && -> Expression const* = delete;
    [[nodiscard]] auto operator[](
        std::string const& key) const& -> ExpressionPtr const&;
    [[nodiscard]] auto operator[](std::string const& key) && -> ExpressionPtr;
    [[nodiscard]] auto operator[](
        ExpressionPtr const& key) const& -> ExpressionPtr const&;
    [[nodiscard]] auto operator[](ExpressionPtr const& key) && -> ExpressionPtr;
    [[nodiscard]] auto operator[](size_t pos) const& -> ExpressionPtr const&;
    [[nodiscard]] auto operator[](size_t pos) && -> ExpressionPtr;
    [[nodiscard]] auto operator<(ExpressionPtr const& other) const -> bool;
    [[nodiscard]] auto operator==(ExpressionPtr const& other) const -> bool;
    template <class T>
    [[nodiscard]] auto operator==(T const& other) const -> bool {
        return ptr_ and *ptr_ == other;
    }
    template <class T>
    [[nodiscard]] auto operator!=(T const& other) const -> bool {
        return not(*this == other);
    }
    [[nodiscard]] auto Evaluate(
        Configuration const& env,
        FunctionMapPtr const& functions,
        std::function<void(std::string const&)> const& logger =
            [](std::string const& error) noexcept -> void {
            Logger::Log(LogLevel::Error, error);
        },
        std::function<void(void)> const& note_user_context =
            []() noexcept -> void {}) const noexcept -> ExpressionPtr;

    [[nodiscard]] auto IsCacheable() const noexcept -> bool;
    [[nodiscard]] auto ToIdentifier() const noexcept -> std::string;
    [[nodiscard]] auto ToJson() const noexcept -> nlohmann::json;

    using linked_map_t = LinkedMap<std::string, ExpressionPtr, ExpressionPtr>;
    [[nodiscard]] auto IsNotNull() const noexcept -> bool;
    [[nodiscard]] auto LinkedMap() const& -> linked_map_t const&;
    [[nodiscard]] static auto Make(linked_map_t&& map) -> ExpressionPtr;

  private:
    std::shared_ptr<Expression> ptr_;
};

namespace std {
template <>
struct hash<ExpressionPtr> {
    [[nodiscard]] auto operator()(ExpressionPtr const& p) const noexcept
        -> std::size_t;
};
}  // namespace std

#endif  // INCLUDED_SRC_BUILDTOOL_BUILD_ENGINE_EXPRESSION_EXPRESSION_PTR_HPP
