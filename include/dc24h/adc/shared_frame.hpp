#pragma once

#include <memory>
#include <utility>

namespace dc24h::adc {

// The frame and its control block share one allocation. Only the routing owner
// can mint Frame::Passkey, so this does not widen who may construct a frame.
template <class Frame, class... Args>
[[nodiscard]] std::shared_ptr<const Frame> share_frame(typename Frame::Passkey key, Args&&... args) {
    return std::make_shared<Frame>(key, std::forward<Args>(args)...);
}

} // namespace dc24h::adc
