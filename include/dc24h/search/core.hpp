#pragma once

#include "dc24h/search/query.hpp"
#include "dc24h/users/session_manager.hpp"

#include <functional>
#include <memory>
#include <string_view>
#include <utility>

namespace dc24h::search {
inline constexpr std::size_t max_selectors = 32;
struct Selection {
    std::vector<std::string> required_features, excluded_features;
    friend bool operator==(const Selection&, const Selection&) = default;
};
enum class Error { missing_sender, sender_not_ready, wrong_source, invalid_selection, foreign_plan, missing_sink };
namespace detail { struct Domain final {}; }
class Plan final {
public:
    [[nodiscard]] users::SessionId sender() const noexcept { return sender_; }
    [[nodiscard]] const std::string& source() const noexcept { return source_; }
    [[nodiscard]] const Query& query() const noexcept { return query_; }
    [[nodiscard]] const Selection& selection() const noexcept { return selection_; }
    [[nodiscard]] const std::vector<users::SessionId>& recipients() const noexcept { return recipients_; }
private:
    friend class Core;
    Plan(std::shared_ptr<const detail::Domain> domain, users::SessionId sender, std::string_view source,
         Query query, Selection selection, std::vector<users::SessionId> recipients)
        : domain_(std::move(domain)), sender_(sender), source_(source), query_(std::move(query)),
          selection_(std::move(selection)), recipients_(std::move(recipients)) {}
    std::shared_ptr<const detail::Domain> domain_;
    users::SessionId sender_;
    std::string source_;
    Query query_;
    Selection selection_;
    std::vector<users::SessionId> recipients_;
};
struct DispatchReport {
    std::size_t queued{}, unavailable{}, refused{}, canceled{};
    friend bool operator==(const DispatchReport&, const DispatchReport&) = default;
};
using Preparation = std::variant<Plan, Error, QueryError>;
// Single owner-thread, session-bound hub search coordination. Files remain owned
// by clients. No file index, SCH/RES parser, sockets, timers, history or logger I/O.
// SessionManager must outlive this Core and its dispatch calls.
class Core final {
public:
    using Sink = std::function<bool(users::SessionId, const Plan&)>;
    explicit Core(const users::SessionManager& sessions) : sessions_(sessions) {}
    Core(const Core&) = delete;
    Core& operator=(const Core&) = delete;
    [[nodiscard]] std::optional<Error> validate_sender(users::SessionId sender, std::string_view source) const noexcept;
    // Ready lifetime IDs in increasing order, including the sender when selected.
    // No user-profile copies. Allocation failure leaves all models unchanged.
    [[nodiscard]] Preparation prepare(users::SessionId sender, std::string_view source,
                                      const QueryData& query, const Selection& selection = {}) const;
    // One dispatch attempt, acceptance means queued, never delivered/read. No
    // allocation after sink construction. Rechecks source and each lifetime and
    // current feature set. Sender closure during a sink cancels the remainder.
    // Trusted sinks may close sessions, but must not reenter dispatch or throw.
    // Exceptions propagate after possible partial dispatch: never blindly retry.
    [[nodiscard]] std::variant<DispatchReport, Error> dispatch(const Plan& plan, const Sink& sink) const;
private:
    const users::SessionManager& sessions_;
    const std::shared_ptr<const detail::Domain> domain_{std::make_shared<const detail::Domain>()};
};
} // namespace dc24h::search
