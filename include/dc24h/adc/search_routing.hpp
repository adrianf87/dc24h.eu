#pragma once

#include "dc24h/adc/sch.hpp"
#include <type_traits>

namespace dc24h::adc {
enum class SearchRoutingError { invalid_state, wrong_source, invalid_message, no_criteria, recipient_unavailable,
                                duplicate_token, search_limit, inactive_search, foreign_plan, already_dispatched, missing_sink };
class SearchFrame final {
public:
    class Passkey { friend class SearchRouter; Passkey() noexcept = default; };
    SearchFrame(const SearchFrame&) = delete;
    SearchFrame& operator=(const SearchFrame&) = delete;
    SearchFrame(SearchFrame&&) = delete;
    SearchFrame& operator=(SearchFrame&&) = delete;
    SearchFrame(Passkey, Message message, std::string wire) : message_(std::move(message)), wire_(std::move(wire)) {}
    [[nodiscard]] const Message& message() const noexcept { return message_; }
    [[nodiscard]] const std::string& wire() const noexcept { return wire_; }
private:
    Message message_;
    std::string wire_;
};
using SharedSearchFrame = std::shared_ptr<const SearchFrame>;
class RoutedSearch final {
public:
    [[nodiscard]] const search::Plan& plan() const noexcept {
        return std::visit([](const auto& value) -> const search::Plan& {
            if constexpr (std::is_same_v<std::decay_t<decltype(value)>,search::Plan>) { return value; }
            else { return value.plan(); }
        },admission_);
    }
    [[nodiscard]] const SharedSearchFrame& frame() const noexcept { return frame_; }
private:
    friend class SearchRouter;
    struct Attempt { bool started{false}; };
    RoutedSearch(std::variant<search::Plan,search::ActivePlan> admission, SharedSearchFrame frame, std::shared_ptr<Attempt> attempt)
        : admission_(std::move(admission)), frame_(std::move(frame)), attempt_(std::move(attempt)) {}
    std::variant<search::Plan,search::ActivePlan> admission_;
    SharedSearchFrame frame_;
    std::shared_ptr<Attempt> attempt_;
};
using SearchDispatch = std::variant<search::DispatchReport,SearchRoutingError>;
// B/F/D/E routing over the existing lifetime-bound Core. Copies of a routed
// publication share one dispatch attempt. No RES parser, sockets or queue I/O.
class SearchRouter final {
public:
    using Sink = std::function<bool(users::SessionId,const SharedSearchFrame&)>;
    explicit SearchRouter(search::Core& core) : core_(core) {}
    [[nodiscard]] std::variant<RoutedSearch,SearchRoutingError> prepare(users::SessionId sender, const Message& message, users::TimePoint now);
    [[nodiscard]] SearchDispatch dispatch(const RoutedSearch& message, const Sink& sink, users::TimePoint now);
    [[nodiscard]] bool cancel(const RoutedSearch& message) noexcept;
private:
    search::Core& core_;
};
} // namespace dc24h::adc
