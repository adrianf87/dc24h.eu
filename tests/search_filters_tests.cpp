#include "dc24h/search/filters.hpp"
#include "dc24h/adc/sch.hpp"

#include <array>
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
void require(bool value, const char* why) { if (!value) { throw std::runtime_error(why); } }
Filter filter(QueryData data) { return std::get<Filter>(Filter::create(data)); }
Item item(std::string name, ItemType type = ItemType::file, std::optional<std::int64_t> size = 42) {
    return std::get<Item>(Item::create({std::move(name),type,size}));
}
adc::SchRequest sch(std::string_view fields) {
    return std::get<adc::SchRequest>(adc::parse_sch("BSCH AAAB " + std::string(fields)));
}
void names_and_unicode() {
    QueryData data; data.include = {"ubuntu","LINUX"}; data.exclude = {"gentoo","BETA"};
    const auto predicate = filter(data);
    require(predicate.matches(item("/Linux/UBUNTU.iso")), "AN did not match across the full virtual path");
    require(!predicate.matches(item("/Linux/Fedora.iso")), "AN used OR semantics");
    require(!predicate.matches(item("/Linux/gentoo/ubuntu.iso")), "NO failed to reject a parent component");
    require(!predicate.matches(item("/Linux/ubuntu-Beta.iso")), "NO did not use OR semantics");
    data.include = {"ŻÓŁW", "STRASSE", "Σ"}; data.exclude.clear();
    const auto unicode = filter(data);
    require(unicode.matches(item("/żółw/Straße/ς.txt")), "Full Unicode case folding failed");
    require(!unicode.matches(item("/zolw/Strasse/σ.txt")), "Case folding removed accents");
    data.include = {"  Two Words  ", "*?.[x]"};
    const auto literal = filter(data);
    require(literal.matches(item("/  two words  /*?.[x].txt")), "Literal punctuation or whitespace changed");
    require(!literal.matches(item("/two words/anything.txt")), "Whitespace trimmed or wildcard executed");
    data.include = {"é"};
    require(filter(data).matches(item("/CAFÉ.txt")), "Accented case pair did not match");
    data.include.clear(); data.exclude = {"TMP"};
    require(filter(data).matches(item("/source.cpp")) && !filter(data).matches(item("/tmp/source.cpp")), "Exclusion-only filter differs");
}
void extensions() {
    QueryData data; data.extensions = {"ISO", "IMG"}; const auto predicate = filter(data);
    for (const auto& name : {"/a.iso", "/a.ImG", "/d.iso/a.ISO", "disk.iso"}) {
        require(predicate.matches(item(name)), "EX alternatives/case matching failed");
    }
    for (const auto& name : {"/notiso", "/a.iso.bak", "/dir.iso/a", "/.iso", "/a.iso ", "/a.iso/x.txt"}) {
        require(!predicate.matches(item(name)), "EX matched a partial suffix, parent or empty stem");
    }
    require(!predicate.matches(item("/images.iso/",ItemType::directory)), "EX matched a directory");
    data.extensions = {"tar.gz", "ŻÓŁW"}; const auto compound = filter(data);
    require(compound.matches(item("/backup.TAR.GZ")) && compound.matches(item("/file.żółw")), "Compound/Unicode extension failed");
    require(!compound.matches(item("/backup.tar/gz")) && !compound.matches(item("/backup.gzip")), "Compound suffix crossed a component");
    data.extensions = {"iso", "iso"}; require(filter(data).matches(item("/a.iso")), "Repeated EX changed semantics");
}
void sizes_and_types() {
    QueryData data; data.min_bytes = 10; data.max_bytes = 20; const auto range = filter(data);
    for (std::int64_t size = 0; size <= 30; ++size) {
        require(range.matches(item("/file",ItemType::file,size)) == (size >= 10 && size <= 20), "Inclusive size endpoints differ");
    }
    data.exact_bytes = 15; const auto exact = filter(data);
    require(exact.matches(item("/file",ItemType::file,15)) && !exact.matches(item("/file",ItemType::file,14)) &&
        !exact.matches(item("/file",ItemType::file,16)), "EQ did not intersect bounds");
    require(!exact.matches(item("/file",ItemType::file,{})), "Unknown size matched a size predicate");
    require(exact.matches(item("/dir/",ItemType::directory,15)) && !exact.matches(item("/dir/",ItemType::directory,{})),
        "Directory size was guessed or size predicates ignored");
    for (const auto size : {std::int64_t{0}, std::int64_t{INT64_MAX}}) {
        data = {}; data.min_bytes = size; data.max_bytes = size; data.exact_bytes = size;
        require(filter(data).matches(item("/file",ItemType::file,size)), "Zero/INT64_MAX size refused or overflowed");
    }
    for (const auto type : {ItemType::file,ItemType::directory}) {
        data = {}; data.type = type; const auto predicate = filter(data);
        require(predicate.matches(item(type == ItemType::file ? "/file" : "/dir/",type,{})), "Type-only query required name or size");
        require(!predicate.matches(item(type == ItemType::file ? "/dir/" : "/file",
            type == ItemType::file ? ItemType::directory : ItemType::file)), "TY returned the wrong kind");
    }
    data = {}; data.extensions = {"iso"}; data.type = ItemType::directory;
    require(!filter(data).matches(item("/images.iso/",ItemType::directory)), "Impossible EX/TY conjunction broadened");
}
void validation_and_ownership() {
    require(std::get<QueryError>(Filter::create({})) == QueryError::no_criteria, "Empty filter became match-all");
    QueryData data; data.token = "Token only";
    require(std::get<QueryError>(Filter::create(data)) == QueryError::no_criteria, "TO became a search criterion");
    data.type = ItemType::file; data.token = "Żółw opaque token";
    require(filter(data).matches(item("/anything",ItemType::file,{})), "Opaque token restricted matching");
    for (const auto& name : std::vector<std::string>{"", "   ", "bad\n", "bad\t", std::string("bad\0x",5),
        std::string(1,char(0xff)), "e\xcc\x81", "bad\xe2\x80\xae", std::string(max_item_name_bytes+1,'x'), "/file/"}) {
        require(std::get<ItemError>(Item::create({name,ItemType::file,0})) == ItemError::invalid_name, "Invalid candidate name accepted");
    }
    require(std::get<ItemError>(Item::create({"/x",ItemType::any,0})) == ItemError::invalid_type, "Candidate accepted wildcard type");
    require(std::get<ItemError>(Item::create({"/x",static_cast<ItemType>(99),0})) == ItemError::invalid_type, "Candidate accepted forged enum");
    require(std::get<ItemError>(Item::create({"/x",ItemType::file,-1})) == ItemError::invalid_size, "Candidate accepted negative size");
    const auto boundary = item(std::string(max_item_name_bytes,'x'));
    data = {}; data.include = {std::string(max_term_bytes,'X')};
    require(filter(data).matches(boundary), "Exact candidate/term limit refused");
    auto oversized = ItemData{std::string(max_item_name_bytes+1,'x'),ItemType::file,0};
    allocation_budget = 0; const auto rejected = Item::create(oversized); allocation_budget = -1;
    require(std::get<ItemError>(rejected) == ItemError::invalid_name, "Name limit allocated before refusal");
    QueryData original; original.include = {"Original"}; auto owning = filter(original);
    ItemData source{"/ORIGINAL.iso",ItemType::file,0}; auto candidate = std::get<Item>(Item::create(source));
    original.include[0] = "changed"; source.name = "/changed";
    auto copy = owning; auto moved = std::move(owning); auto item_copy = candidate; auto item_moved = std::move(candidate);
    require(copy.matches(item_copy) && moved.matches(item_moved) && !owning.matches(item_moved), "Filter borrowed input or lost move/copy ownership");
    static_assert(noexcept(copy.matches(item_copy)));
    allocation_budget = 0; const bool matched = copy.matches(item_copy); allocation_budget = -1;
    require(matched, "Matching allocated or mutated criteria");
    const auto long_name = std::string(max_item_name_bytes,'x'); auto long_item = item(long_name);
    QueryData many; many.include = std::vector<std::string>(16,std::string(max_term_bytes,'X'));
    const auto large = filter(many);
    allocation_budget = 0; const bool bounded_match = large.matches(long_item); allocation_budget = -1;
    require(bounded_match, "Exact aggregate query bound failed");
    many.include.push_back("y");
    require(std::get<QueryError>(Filter::create(many)) == QueryError::too_large, "Aggregate limit bypassed by compiler");
}
void bridge_and_core() {
    const auto request = sch("ANubuntu ANlinux NOgentoo EXISO EXIMG GE10 LE20 EQ15 TY1 TOŻółw\\stoken");
    auto predicate = std::get<Filter>(request.to_filter());
    require(predicate.matches(item("/Linux/Ubuntu.iso",ItemType::file,15)), "SCH fields were not compiled together");
    require(!predicate.matches(item("/Linux/Ubuntu.iso",ItemType::file,14)), "SCH EQ was discarded");
    require(std::get<Query>(request.to_core_query()).data() == request.criteria(), "SCH projection changed original typed values");
    require(std::get<Filter>(sch("EXiso").to_filter()).matches(item("/a.iso")), "Filter-only SCH refused");
    require(std::get<Filter>(sch("NOtmp").to_filter()).matches(item("/a.iso")), "Exclusion-only SCH refused");
    const auto hash = sch("TR" + std::string(39,'A') + " EXiso");
    require(!std::get<Filter>(hash.to_filter()).matches(item("/a.iso")), "Missing descriptor TTH silently became text-only matching");
    users::SessionManager sessions(1); const auto id = *users::SessionId::from_value(1);
    require(std::holds_alternative<const users::Session*>(sessions.add(id,users::Transport::adc,{})), "Session add failed");
    require(!sessions.start_negotiation(id,{}), "Session negotiation failed");
    require(!sessions.identify(id,*users::User::create(*users::Identity::create("AAAB","CID","Nick")),{}), "Session identify failed");
    require(!sessions.mark_ready(id,{}), "Session ready failed");
    Core core(sessions); const auto filters = sch("EXiso GE0 TY1 TOtoken\\sŻółw");
    const auto plan = std::get<Plan>(core.prepare(id,"AAAB",filters.criteria()));
    require(plan.query().data() == filters.criteria() && plan.recipients().size() == 1, "Core rejected or rewrote filter-only query");
}
void fault_campaign() {
    QueryData data; data.include = {std::string(128,'X'),"Straße"}; data.extensions = {"ISO"};
    const auto before = data; bool failed = false, success = false;
    for (long budget = 0; budget < 100 && !success; ++budget) {
        allocation_budget = budget;
        try { const auto result = Filter::create(data); allocation_budget = -1;
            require(std::holds_alternative<Filter>(result), "Compiler fault returned a semantic error"); success = true;
        } catch (const std::bad_alloc&) { allocation_budget = -1; failed = true; }
        require(data == before, "Compiler fault mutated input");
    }
    require(failed && success, "Filter allocation campaign missed failure/success");
    ItemData source{std::string(256,'X'),ItemType::file,INT64_MAX}; failed = success = false;
    for (long budget = 0; budget < 100 && !success; ++budget) {
        allocation_budget = budget;
        try { const auto result = Item::create(source); allocation_budget = -1;
            require(std::holds_alternative<Item>(result), "Item fault returned a semantic error"); success = true;
        } catch (const std::bad_alloc&) { allocation_budget = -1; failed = true; }
        require(source.name == std::string(256,'X') && source.bytes == INT64_MAX, "Item fault mutated source");
    }
    require(failed && success, "Item allocation campaign missed failure/success");
}
} // namespace
int main() {
    try { names_and_unicode(); extensions(); sizes_and_types(); validation_and_ownership(); bridge_and_core(); fault_campaign();
        std::cout << "PASS: Unicode name, extension, size and type filters, SCH/Core bridge and allocation isolation\n";
    } catch (const std::exception& e) { allocation_budget = -1; std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
