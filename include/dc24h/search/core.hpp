#pragma once

#include "dc24h/search/query.hpp"
#include "dc24h/search/connectivity.hpp"
#include "dc24h/search/features.hpp"
#include "dc24h/search/tokens.hpp"
#include "dc24h/users/session_manager.hpp"

#include <functional>
#include <memory>
#include <string_view>
#include <utility>

namespace dc24h::search {
enum class Scope { broadcast, direct, echo };
struct Audience {
    Scope scope{Scope::broadcast};
    std::optional<users::SessionId> target{};
};
enum class Error { missing_sender, sender_not_ready, wrong_source, invalid_selection, foreign_plan, missing_sink,
                   invalid_audience, recipient_unavailable };
namespace detail { struct Domain final {}; }
class Plan final {
public:
    [[nodiscard]] users::SessionId sender() const noexcept { return sender_; }
    [[nodiscard]] const std::string& source() const noexcept { return source_; }
    [[nodiscard]] const Query& query() const noexcept { return query_; }
    [[nodiscard]] const ReplyPaths& reply_paths() const noexcept { return reply_paths_; }
    [[nodiscard]] const Selection& selection() const noexcept { return features_.selection(); }
    [[nodiscard]] const std::vector<users::SessionId>& recipients() const noexcept { return recipients_; }
private:
    friend class Core;
    Plan(std::shared_ptr<const detail::Domain> domain, users::SessionId sender, std::string_view source,
         Query query, FeatureFilter features, std::vector<users::SessionId> recipients, ReplyPaths paths)
        : domain_(std::move(domain)), sender_(sender), source_(source), query_(std::move(query)),
          features_(std::move(features)), recipients_(std::move(recipients)), reply_paths_(paths) {}
    std::shared_ptr<const detail::Domain> domain_;
    users::SessionId sender_;
    std::string source_;
    Query query_;
    FeatureFilter features_;
    std::vector<users::SessionId> recipients_;
    ReplyPaths reply_paths_;
};
struct DispatchReport {
    std::size_t queued{}, unavailable{}, refused{}, canceled{};
    friend bool operator==(const DispatchReport&, const DispatchReport&) = default;
};
using Preparation = std::variant<Plan, Error, QueryError>;
class ActivePlan final {
public:
    [[nodiscard]] const Plan& plan() const noexcept { return plan_; }
    [[nodiscard]] const TokenHandle& handle() const noexcept { return handle_; }
private:
    friend class Core;
    ActivePlan(Plan plan, TokenHandle handle) : plan_(std::move(plan)), handle_(std::move(handle)) {}
    Plan plan_;
    TokenHandle handle_;
};
using StartResult = std::variant<ActivePlan, Error, QueryError, TokenError>;
// Single owner-thread, session-bound hub search coordination. Files remain owned
// by clients. No file index, SCH/RES parser, sockets, timers, history or logger I/O.
// SessionManager must outlive this Core and its dispatch calls.
class Core final {
public:
    using Sink = std::function<bool(users::SessionId, const Plan&)>;
    explicit Core(const users::SessionManager& sessions, TokenLimits limits = {}) : sessions_(sessions), tokens_(sessions,limits) {}
    Core(const Core&) = delete;
    Core& operator=(const Core&) = delete;
    [[nodiscard]] std::optional<Error> validate_sender(users::SessionId sender, std::string_view source) const noexcept;
    [[nodiscard]] std::optional<users::SessionId> resolve_target(std::string_view source) const noexcept;
    // Current ready-owner snapshot. This predicts available reply paths only;
    // clients use current public INF and exchange UDP results directly.
    [[nodiscard]] std::optional<ReplyPaths> reply_paths(users::SessionId owner) const noexcept;
    [[nodiscard]] bool owns(const Plan& plan) const noexcept { return plan.domain_ == domain_; }
    // Ready lifetime IDs in increasing order, including the sender when selected.
    // No user-profile copies. Allocation failure leaves all models unchanged.
    [[nodiscard]] Preparation prepare(users::SessionId sender, std::string_view source,
                                      const QueryData& query, const Selection& selection = {}, const Audience& audience = {}) const;
    // One dispatch attempt, acceptance means queued, never delivered/read. No
    // allocation after sink construction. Rechecks source and each lifetime and
    // current feature set. Sender closure during a sink cancels the remainder.
    // Trusted sinks may close sessions, but must not reenter dispatch or throw.
    // Exceptions propagate after possible partial dispatch: never blindly retry.
    [[nodiscard]] std::variant<DispatchReport, Error> dispatch(const Plan& plan, const Sink& sink) const;
    // Explicit accepted-search lifecycle. Stateless prepare/dispatch above does
    // not register tokens. Accepted live SCH with TO uses start().
    [[nodiscard]] StartResult start(users::SessionId sender, std::string_view source, const QueryData& query,
                                    users::TimePoint now, const Selection& selection = {}, const Audience& audience = {});
    [[nodiscard]] std::variant<DispatchReport, Error, TokenError> dispatch(const ActivePlan& search, const Sink& sink, users::TimePoint now);
    [[nodiscard]] bool complete(const ActivePlan& search) noexcept;
    [[nodiscard]] std::optional<ActiveSearch> find_search(users::SessionId owner, std::string_view token, users::TimePoint now) noexcept {
        return tokens_.find(owner,token,now);
    }
    [[nodiscard]] std::size_t tracked_searches() const noexcept { return tokens_.size(); }
    std::size_t prune_searches(users::TimePoint now) noexcept { return tokens_.prune(now); }
    std::size_t forget_searches(users::SessionId owner) noexcept { return tokens_.forget(owner); }
private:
    const users::SessionManager& sessions_;
    Tokens tokens_;
    const std::shared_ptr<const detail::Domain> domain_{std::make_shared<const detail::Domain>()};
};
} // namespace dc24h::search
