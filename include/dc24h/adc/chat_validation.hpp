#pragma once

#include "dc24h/adc/msg.hpp"
#include "dc24h/chat/core.hpp"

namespace dc24h::adc {
enum class ChatValidationError { invalid_message, invalid_text, invalid_sid, invalid_state, wrong_source, unsupported_type };
using ChatValidationResult = std::variant<MsgView, ChatValidationError>;

// Shared client-chat admission check for direct adapter callers and the router.
// Checks bounded text, ADC header/SID/flag/wire syntax, then current sender
// readiness and source ownership. Only B/D/E are supported; F source ownership
// is checked before unsupported_type. H/I are never client chat senders.
// Target syntax is checked here; ready target selection and lifetime binding
// remain in Core::prepare_direct, after the private conversation policy.
// The returned view borrows Message, is not an authorization token and must not
// be cached across Message or session changes. No registry/queue mutation.
// Allocation errors propagate before publication. Dispatch rechecks lifetimes.
[[nodiscard]] ChatValidationResult validate_chat(const chat::Core& core,
    users::SessionId sender, const Message& message);
} // namespace dc24h::adc
