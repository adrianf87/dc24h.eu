#include "dc24h/search/filters.hpp"
#include "dc24h/search/core.hpp"
#include "dc24h/adc/sch.hpp"
#include "dc24h/adc/serializer.hpp"

#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>

namespace { long allocation_budget = -1; }
[[gnu::noinline]] void* operator new(std::size_t size) {
    if (allocation_budget == 0) { throw std::bad_alloc(); }
    if (allocation_budget > 0) { --allocation_budget; }
    if (auto* value = std::malloc(size == 0 ? 1 : size)) { return value; }
    throw std::bad_alloc();
}
[[gnu::noinline]] void* operator new[](std::size_t size) { return ::operator new(size); }
[[gnu::noinline]] void operator delete(void* value) noexcept { std::free(value); }
[[gnu::noinline]] void operator delete[](void* value) noexcept { std::free(value); }
[[gnu::noinline]] void operator delete(void* value, std::size_t) noexcept { std::free(value); }
[[gnu::noinline]] void operator delete[](void* value, std::size_t) noexcept { std::free(value); }

namespace {
using namespace dc24h;
using namespace dc24h::search;
constexpr std::string_view encoded_root = "LWPNACQDBZRYXW3VHJVCJ64QBZNGHOHHHZWCLNQ";
void require(bool value, const char* why) { if (!value) { throw std::runtime_error(why); } }
TthRoot root() { return *adc::parse_tiger_base32(encoded_root); }
Filter filter(const QueryData& data) { return std::get<Filter>(Filter::create(data)); }
Item item(std::optional<TthRoot> hash, std::string name = "/file.iso", std::optional<std::int64_t> size = 0) {
    return std::get<Item>(Item::create({std::move(name),ItemType::file,size,hash}));
}
adc::SchRequest sch(std::string_view body) { return std::get<adc::SchRequest>(adc::parse_sch(body)); }
void exact_hashes() {
    const auto hash = root();
    // Empty-file TTH is Tiger(0x00), not Tiger(empty file bytes).
    const std::array<std::uint8_t,1> leaf_prefix{0};
    require(adc::tiger192(leaf_prefix) == hash && adc::tiger192({}) != hash, "TTH confused with a plain Tiger file digest");
    QueryData data; data.tth = hash;
    const auto query = std::get<Query>(Query::create(data));
    require(query.data() == data && query.data().include.empty(), "Hash-only query lost its root or invented a name");
    const auto predicate = filter(data);
    require(predicate.matches(item(hash)) && predicate.matches(item(hash,"/renamed.txt",{})), "Hash-only matching depended on filename or size");
    require(!predicate.matches(item({})), "Unknown root became a TTH match");
    for (std::size_t byte = 0; byte < hash.size(); ++byte) {
        for (unsigned bit = 0; bit < 8; ++bit) {
            auto changed = hash; changed[byte] ^= static_cast<std::uint8_t>(1U << bit);
            require(!predicate.matches(item(changed)), "A differing TTH bit was ignored");
        }
    }
    data.tth = TthRoot{}; const auto zero = filter(data);
    require(zero.matches(item(TthRoot{})) && !zero.matches(item({})) && !zero.matches(item(hash)), "Absent and all-zero roots were conflated");
    TthRoot upper; upper.fill(255); data.tth = upper;
    require(filter(data).matches(item(upper)), "A valid binary TTH value was treated as a sentinel");
    require(std::get<ItemError>(Item::create({"/dir/",ItemType::directory,0,hash})) == ItemError::invalid_hash,
        "Directory descriptor accepted a file TTH");
    const auto directory = std::get<Item>(Item::create({"/dir/",ItemType::directory,0,{}}));
    require(!predicate.matches(directory), "Hash-only filter matched a directory");
    data = {}; data.include = {"file"};
    require(filter(data).matches(item(hash)) && filter(data).matches(item({})), "Optional descriptor root broke text-only filters");
}
void conjunction_and_ownership() {
    QueryData data; data.tth = root(); data.include = {"UBUNTU"}; data.exclude = {"beta"}; data.extensions = {"iso"};
    data.min_bytes = 10; data.max_bytes = 20; data.exact_bytes = 15; data.type = ItemType::file; data.token = "Żółw token";
    const auto predicate = filter(data); const auto match = item(root(),"/Linux/ubuntu.ISO",15);
    require(predicate.matches(match), "Combined TTH/name/extension/size/type predicate refused");
    for (const auto& rejected : {item({},"/ubuntu.iso",15), item(root(),"/fedora.iso",15), item(root(),"/ubuntu-beta.iso",15),
        item(root(),"/ubuntu.img",15), item(root(),"/ubuntu.iso",14), item(root(),"/ubuntu.iso",{} )}) {
        require(!predicate.matches(rejected), "TTH shortcut discarded another criterion");
    }
    auto source = data; auto owning = filter(source); source.tth = TthRoot{}; source.include.clear();
    auto copy = owning; auto moved = std::move(owning);
    ItemData descriptor{"/ubuntu.iso",ItemType::file,15,root()}; auto checked = std::get<Item>(Item::create(descriptor));
    descriptor.tth.reset(); descriptor.name = "/changed";
    require(copy.matches(checked) && moved.matches(checked) && !owning.matches(checked), "TTH compiler/descriptor borrowed caller storage");
    static_assert(noexcept(predicate.matches(match)));
    allocation_budget = 0; const bool matched = predicate.matches(match); allocation_budget = -1;
    require(matched, "TTH matching allocated");
    data.type = ItemType::directory;
    const auto impossible = filter(data);
    require(!impossible.matches(match), "TR/TY directory conjunction broadened to a file match");
    QueryData boundary; boundary.tth = root(); boundary.include = std::vector<std::string>(16,std::string(max_term_bytes,'X'));
    require(std::holds_alternative<Query>(Query::create(boundary)), "Fixed-size root incorrectly charged as text");
    boundary.token = "x";
    require(std::get<QueryError>(Query::create(boundary)) == QueryError::too_large, "TTH bypassed aggregate text bounds");
}
void wire_and_projection() {
    const auto text = std::string(encoded_root);
    for (const auto& header : {"BSCH AAAB", "FSCH AAAB +TCP4-UDP4", "DSCH AAAB AAAC", "ESCH AAAB AAAC"}) {
        const auto body = std::string(header) + " TR" + text + " TOŻółw\\stoken XXfuture";
        const auto request = sch(body);
        require(request.root() == root() && request.criteria().tth == request.root(), "SCH and Core root diverged");
        require(std::get<Query>(request.to_core_query()).data() == request.criteria(), "Projection dropped TR or token");
        require(std::get<Filter>(request.to_filter()).matches(item(root())), "Hash-only SCH failed matching");
        require(std::get<std::string>(adc::serialize(request.message())) == body + "\n", "SCH projection changed original wire fields");
        auto copy = request; auto moved = std::move(copy);
        require(moved.root() == root() && moved.criteria().tth == root(), "SCH root getter lost ownership after moving");
    }
    for (const auto& bad : std::vector<std::string>{"", text.substr(1), text+"A", std::string(39,'a'), std::string(38,'A')+"B",
        "TTH/"+text, "urn:tree:tiger:"+text, text+"=", std::string(39,'0')}) {
        const auto result = adc::parse_sch("BSCH AAAB TR"+bad);
        require(std::get<adc::SchError>(result) == adc::SchError::invalid_hash, "Invalid/noncanonical TTH accepted");
    }
    const auto duplicate = adc::parse_sch("BSCH AAAB TR"+text+" TR"+text);
    require(std::get<adc::SchError>(duplicate) == adc::SchError::duplicate_field, "Duplicate TR accepted");
    const auto body = "BSCH AAAB TR"+text+" ANx EXiso GE0 TY1";
    const auto candidate = sch(body); const auto predicate = std::get<Filter>(candidate.to_filter());
    require(predicate.matches(item(root(),"/x.iso")) && !predicate.matches(item(root(),"/x.img")), "Combined SCH criteria lost during TTH projection");
}
void core_and_faults() {
    users::SessionManager sessions(2); const auto id = *users::SessionId::from_value(1);
    require(std::holds_alternative<const users::Session*>(sessions.add(id,users::Transport::adc,{})), "Add failed");
    require(!sessions.start_negotiation(id,{}), "Negotiation failed");
    require(!sessions.identify(id,*users::User::create(*users::Identity::create("AAAB","CID","Nick")),{}), "Identify failed");
    require(!sessions.mark_ready(id,{}), "Readiness failed");
    Core core(sessions); QueryData data; data.tth = root(); data.token = "opaque token";
    require(std::get<Error>(core.prepare(id,"AAAC",data)) == Error::wrong_source, "TTH bypassed sender ownership");
    auto plan = std::get<Plan>(core.prepare(id,"AAAB",data)); data.tth = TthRoot{};
    require(plan.query().data().tth == root() && plan.recipients().size() == 1, "Core plan lost owning TTH criteria");
    const Core::Sink sink = [&](users::SessionId recipient,const Plan& received) {
        return recipient == id && received.query().data().tth == root();
    };
    allocation_budget = 0; const auto report = core.dispatch(plan,sink); allocation_budget = -1;
    require(std::get<DispatchReport>(report) == DispatchReport{1,0,0,0}, "TTH dispatch allocated or lost the query");
    const auto request = sch("BSCH AAAB TR"+std::string(encoded_root)+" AN"+std::string(128,'x')+" TOtoken");
    bool failed = false, success = false;
    for (long budget = 0; budget < 100 && !success; ++budget) {
        allocation_budget = budget;
        try { const auto result = request.to_filter(); allocation_budget = -1;
            require(std::holds_alternative<Filter>(result), "Projection fault became a semantic error"); success = true;
        } catch (const std::bad_alloc&) { allocation_budget = -1; failed = true; }
        require(request.root() == root() && request.criteria().include[0] == std::string(128,'x'), "Projection fault changed the request");
    }
    require(failed && success, "TTH allocation campaign missed failure/success");
}
} // namespace
int main() {
    try { exact_hashes(); conjunction_and_ownership(); wire_and_projection(); core_and_faults();
        std::cout << "PASS: exact TTH matching, SCH/Core ownership, canonical roots and allocation isolation\n";
    } catch (const std::exception& error) { allocation_budget = -1; std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
