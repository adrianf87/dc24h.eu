#pragma once

#include "dc24h/adc/protocol.hpp"

#include <functional>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace dc24h::adc {

enum class SessionState { protocol, identify, verify, normal };
enum class SessionAction { none, identified, verify_password, dispatch, close };

struct SessionResult {
    SessionAction action = SessionAction::none;
    std::vector<std::string> replies;
};

class Session {
  public:
    explicit Session(std::string_view assigned_sid);

    [[nodiscard]] SessionState state() const noexcept;
    [[nodiscard]] bool closed() const noexcept;
    [[nodiscard]] std::string_view cid() const noexcept;
    [[nodiscard]] bool peer_supports(std::string_view feature) const;

    // Consume a syntactically valid Message from the frame decoder.
    [[nodiscard]] SessionResult receive(const Message &message);

    // The caller chooses the login path after receiving the identified action.
    [[nodiscard]] std::optional<SessionResult>
    complete_identification(std::optional<std::string_view> challenge = std::nullopt);
    [[nodiscard]] std::optional<SessionResult> complete_verification(bool accepted);
    [[nodiscard]] SessionResult disconnect();

  private:
    [[nodiscard]] SessionResult receive_support(const Message &message);
    [[nodiscard]] SessionResult receive_info(const Message &message);
    [[nodiscard]] SessionResult receive_password(const Message &message);
    [[nodiscard]] SessionResult receive_status(const Message &message);
    [[nodiscard]] SessionResult fatal_status(std::string_view code, std::string_view description,
                                             std::string_view field = {});

    SessionState state_ = SessionState::protocol;
    std::string sid_;
    std::string cid_;
    std::set<std::string, std::less<>> peer_features_;
    bool password_pending_ = false;
    bool closed_ = false;
};

} // namespace dc24h::adc
