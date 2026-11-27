#include "dc24h/users/nick_manager.hpp"
#include "dc24h/users/session_manager.hpp"

#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
#include <utility>

namespace {
// Deterministically exercise every C++ allocation in reservation and rename.
// utf8proc uses malloc internally; checked values keep it outside these scopes.
long allocation_budget = -1;
void* allocate(std::size_t bytes) {
    if (allocation_budget == 0) { throw std::bad_alloc(); }
    if (allocation_budget > 0) { --allocation_budget; }
    if (auto* result = std::malloc(bytes == 0 ? 1 : bytes)) { return result; }
    throw std::bad_alloc();
}
}
void* operator new(std::size_t bytes) { return allocate(bytes); }
void* operator new[](std::size_t bytes) { return allocate(bytes); }
void operator delete(void* value) noexcept { std::free(value); }
void operator delete[](void* value) noexcept { std::free(value); }
void operator delete(void* value, std::size_t) noexcept { std::free(value); }
void operator delete[](void* value, std::size_t) noexcept { std::free(value); }

namespace {
using namespace dc24h::users;
using namespace std::chrono_literals;
void require(bool value, const char* reason) { if (!value) { throw std::runtime_error(reason); } }
SessionId id(std::uint64_t value) { return *SessionId::from_value(value); }
void names_and_ownership() {
    for (const auto capacity : {std::size_t{0}, NickManager::max_capacity + 1}) {
        bool rejected = false;
        try { NickManager invalid(capacity); } catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "Invalid capacity accepted");
    }
    NickManager names(2);
    const std::string maximum(max_nickname_bytes, 'N');
    for (const auto& invalid : {std::string{}, std::string("a b"), std::string("a\0b",3),
            std::string("x\n"), std::string("\xc0\xaf"), std::string("e\xcc\x81"),
            std::string("x\xe2\x80\x8b"), maximum + 'N'}) {
        require(names.reserve(id(1), invalid) == NickError::invalid_nickname && names.size() == 0,
            "Invalid UTF-8/NFC name reserved");
    }
    require(!names.reserve(id(1), "Żółw") && names.owner("Żółw") == id(1), "Polish name rejected");
    require(names.reserve(id(1), "Other") == NickError::owner_exists && !names.owner("Other"), "Owner reserved twice");
    require(names.reserve(id(2), "Żółw") == NickError::duplicate_nickname, "Name reserved twice");
    require(!names.reserve(id(2), "żółw"), "Unexpected case folding");
    require(names.reserve(id(3), "Third") == NickError::capacity_reached, "Registry exceeded capacity");
    require(names.rename(id(1), "żółw") == NickError::duplicate_nickname && names.owner("Żółw") == id(1),
        "Collision lost old name");
    require(names.rename(id(3), "Other") == NickError::missing && !names.owner("Other"), "Missing owner renamed");
    require(names.rename(id(1), "Bad Nick") == NickError::invalid_nickname, "Invalid rename accepted");
    require(!names.rename(id(1), "Żółw") && names.size() == 2, "Self rename failed at capacity");
    require(!names.rename(id(1), maximum) && names.owner(maximum) == id(1) && !names.owner("Żółw"),
        "Rename at capacity lost registry consistency");
    require(!names.owner(maximum + 'N') && !names.owner("") && !names.owner(std::string_view("N\0",2)),
        "Invalid lookup accepted");
    require(names.release(id(1)) && !names.release(id(1)) && !names.release(id(99)), "Cleanup was not owner specific");
    require(!names.reserve(id(3), maximum) && !names.release(id(1)) && names.owner(maximum) == id(3),
        "Old lifetime released a reused name");
    require(names.release(id(2)) && names.release(id(3)) && names.size() == 0, "Cleanup leaked names");
    for (std::uint64_t value = 4; value < 1004; ++value) {
        require(!names.reserve(id(value), "Reuse") && names.owner("Reuse") == id(value) && names.release(id(value)),
            "Repeated lifetime reuse failed");
    }
}
void allocation_failures() {
    const auto name = *Nickname::create(std::string(120,'A'));
    const auto changed = *Nickname::create(std::string(120,'B'));
    bool succeeded = false;
    for (long budget = 0; budget < 12 && !succeeded; ++budget) {
        NickManager names(1);
        allocation_budget = budget;
        try { const auto error = names.reserve(id(1),name); allocation_budget = -1;
            require(!error, "Reservation failed without an exception"); succeeded = true;
        } catch (const std::bad_alloc&) { allocation_budget = -1;
            require(names.size() == 0 && !names.owner(name.text()) && !names.release(id(1)), "Reservation allocation leaked an index");
            require(!names.reserve(id(1),name), "Failed reservation blocked retry");
        }
        require(names.owner(name.text()) == id(1), "Reservation retry has incorrect owner");
    }
    require(succeeded, "Reservation never completed");
    succeeded = false;
    for (long budget = 0; budget < 12 && !succeeded; ++budget) {
        NickManager names(1); require(!names.reserve(id(1),name), "Setup failed");
        allocation_budget = budget;
        try { const auto error = names.rename(id(1),changed); allocation_budget = -1;
            require(!error, "Rename failed without an exception"); succeeded = true;
        } catch (const std::bad_alloc&) { allocation_budget = -1;
            require(names.size() == 1 && names.owner(name.text()) == id(1) && !names.owner(changed.text()),
                "Rename allocation released the old name");
            require(!names.rename(id(1),changed), "Failed rename blocked retry");
        }
        require(names.owner(changed.text()) == id(1) && !names.owner(name.text()), "Rename retry corrupted owner index");
    }
    require(succeeded, "Rename never completed");

    // Admission spans SID, CID, nickname, reverse owner and Session state.
    succeeded = false;
    for (long budget = 0; budget < 20 && !succeeded; ++budget) {
        SessionManager sessions(1);
        (void)sessions.add(id(1),Transport::adc,{});
        require(!sessions.start_negotiation(id(1),{}), "Negotiation setup failed");
        auto user = *User::create(*Identity::create("SID","CID",name));
        allocation_budget = budget;
        try { const auto error = sessions.identify(id(1),std::move(user),TimePoint{}+1ms); allocation_budget = -1;
            require(!error, "Admission failed without an exception"); succeeded = true;
        } catch (const std::bad_alloc&) { allocation_budget = -1;
            require(sessions.find(id(1))->phase() == Phase::negotiating && sessions.identified_users() == 0 &&
                !sessions.by_sid("SID") && !sessions.by_cid("CID") && !sessions.by_nickname(name.text()),
                "Admission allocation left a partial identity");
        }
    }
    require(succeeded, "Admission never completed");
}
}
int main() {
    try { names_and_ownership(); allocation_failures();
        std::cout << "PASS: nickname validation, bounded ownership, rename, reuse and allocation rollback\n";
    } catch (const std::exception& error) { allocation_budget = -1; std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
