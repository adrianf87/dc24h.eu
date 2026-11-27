#include "dc24h/database/database.hpp"

#include <array>
#include <atomic>
#include <barrier>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <limits>
#include <memory>
#include <new>
#include <source_location>
#include <thread>

namespace { int fail_after=-1; }
[[gnu::noinline]] void* operator new(std::size_t n) {
    if(fail_after==0){fail_after=-1;throw std::bad_alloc();}if(fail_after>0){--fail_after;}
    if(auto* p=std::malloc(n?n:1)){return p;}throw std::bad_alloc();
}
[[gnu::noinline]] void* operator new[](std::size_t n){return ::operator new(n);}
[[gnu::noinline]] void operator delete(void* p) noexcept{std::free(p);}
[[gnu::noinline]] void operator delete[](void* p) noexcept{std::free(p);}
[[gnu::noinline]] void operator delete(void* p,std::size_t) noexcept{std::free(p);}
[[gnu::noinline]] void operator delete[](void* p,std::size_t) noexcept{std::free(p);}
namespace {
using namespace dc24h::database;
void require(bool ok,std::source_location where=std::source_location::current()) {
    if(!ok){throw std::runtime_error("Database Core assertion failed at line "+std::to_string(where.line()));}
}
template<class F> Error rejects(ErrorKind kind,F&& function,std::source_location where=std::source_location::current()) {
    try{function();}catch(const Error& error){require(error.kind()==kind,where);return error;}
    throw std::runtime_error("Database Core accepted an invalid operation at line "+std::to_string(where.line()));
}
void unit() {
    validate(QueryLimits{});
    validate(QueryLimits{0,64,0,0});
    for(const auto limits:{QueryLimits{1025,1,1,1},QueryLimits{1,0,1,1},QueryLimits{1,65,1,1},QueryLimits{1,1,65537,1},QueryLimits{1,1,1,4194305}}) {
        (void)rejects(ErrorKind::limit,[&]{validate(limits);});
    }
    for(const auto kind:{ErrorKind::initialization,ErrorKind::credentials,ErrorKind::connection,ErrorKind::statement,
            ErrorKind::constraint,ErrorKind::conflict,ErrorKind::result_shape,ErrorKind::limit,ErrorKind::transaction,ErrorKind::wrong_thread}) {
        const Error error(kind,Operation::commit,1234,true);
        require(error.kind()==kind&&error.operation()==Operation::commit&&error.native_code()==1234&&error.outcome_unknown());
        require(!std::string(error.what()).empty()&&std::string(error.what()).size()<96);
    }
}
void concurrent_initialization(const Options& options) {
    std::array<std::exception_ptr,4> errors;
    std::array<std::jthread,4> workers;
    for(std::size_t n=0;n<workers.size();++n){workers[n]=std::jthread([&,n]{try{Connection c(options);require(c.scalar("SELECT 42")=="42");}catch(...){errors[n]=std::current_exception();}});}
    for(auto& worker:workers){worker.join();}
    for(const auto& error:errors){if(error){std::rethrow_exception(error);}}
}
void typed_values(Connection& connection) {
    const Bytes bytes{0,1,0,255,128,92,39};
    const std::string text="Zażółć ' OR 1=1; -- \\ 😺";
    const auto result=connection.query("SELECT ? AS text_value, ? AS binary_value, ? AS signed_value, ? AS unsigned_value, ? AS null_value, ? AS empty_text, ? AS empty_blob",
        {text,bytes,std::numeric_limits<std::int64_t>::min(),std::numeric_limits<std::uint64_t>::max(),nullptr,std::string{},Bytes{}});
    require(result.columns==std::vector<std::string>{"text_value","binary_value","signed_value","unsigned_value","null_value","empty_text","empty_blob"});
    require(result.rows.size()==1&&result.rows[0].size()==7);
    const auto& row=result.rows[0];
    require(row[0]==text&&row[1]==std::string(reinterpret_cast<const char*>(bytes.data()),bytes.size()));
    require(row[2]=="-9223372036854775808"&&row[3]=="18446744073709551615"&&!row[4]&&row[5]==""&&row[6]=="");
    require(connection.query("SELECT ? AS embedded",{std::string("x\0y",3)}).rows[0][0]==std::string("x\0y",3));
    auto owned=connection.query("SELECT 'owned' AS value");
    require(connection.scalar("SELECT 43")=="43"&&owned.rows[0][0]=="owned");
    require(connection.query("SELECT 1 AS empty WHERE FALSE").rows.empty());
    require(connection.query("SELECT 0 AS seq,NULL AS a,'' AS b UNION ALL SELECT 1,'x',NULL ORDER BY seq").rows==std::vector<Row>{
        Row{std::string("0"),std::nullopt,std::string{}},Row{std::string("1"),std::string("x"),std::nullopt}});

}
void boundaries(Connection& c) {
    (void)rejects(ErrorKind::limit,[&]{(void)c.query("SELECT 1 UNION ALL SELECT 2",{},QueryLimits{1,1,10,10});});
    (void)rejects(ErrorKind::limit,[&]{(void)c.query("SELECT 1,2",{},QueryLimits{1,1,10,10});});
    (void)rejects(ErrorKind::limit,[&]{(void)c.query("SELECT 'a','b'",{},QueryLimits{1,2,1,1});});
    (void)rejects(ErrorKind::limit,[&]{(void)c.query("SELECT 'x'",{},QueryLimits{1,1,0,0});});
    require(c.query("SELECT NULL, ''",{},QueryLimits{1,2,0,0}).rows[0]==Row{std::nullopt,std::string{}});
    require(c.query("SELECT 1 WHERE FALSE",{},QueryLimits{0,1,0,0}).rows.empty());
    require(c.query("SELECT REPEAT('a',65536)").rows[0][0]->size()==65536);
    (void)rejects(ErrorKind::limit,[&]{(void)c.query("SELECT REPEAT('a',65537)");});
    std::string sql="SELECT 1";sql.resize(max_sql_bytes,' ');
    require(c.query(sql).rows[0][0]=="1");
    (void)rejects(ErrorKind::limit,[&]{(void)c.query(sql+' ');});
    (void)rejects(ErrorKind::limit,[&]{(void)c.query(std::string("SELECT 1\0",9));});
    (void)rejects(ErrorKind::limit,[&]{(void)c.query("SELECT ?",{std::string(max_parameter_bytes+1,'x')});});
    (void)rejects(ErrorKind::limit,[&]{(void)c.query("SELECT ?",{Bytes(max_parameter_bytes+1,1)});});
    Parameters parameters(max_parameters,std::int64_t{1});sql="SELECT ?";
    for(std::size_t i=1;i<max_parameters;++i){sql+="+?";}
    require(c.query(sql,parameters).rows[0][0]=="256");
    parameters.emplace_back(std::int64_t{1});
    (void)rejects(ErrorKind::limit,[&]{(void)c.query(sql+"+?",parameters);});
    parameters.assign(16,std::string(65536,'x'));sql="SELECT LENGTH(CONCAT(?";
    for(int i=1;i<16;++i){sql+=",?";}sql+="))";
    require(c.query(sql,parameters).rows[0][0]=="1048576");
    parameters.emplace_back(std::string("x"));
    (void)rejects(ErrorKind::limit,[&]{(void)c.query(sql,parameters);});
    (void)rejects(ErrorKind::statement,[&]{(void)c.query("SELECT ?");});
    (void)rejects(ErrorKind::result_shape,[&]{c.execute("SELECT 1");});
    require(c.scalar("SELECT 42")=="42");
}
std::uint64_t insert(Connection& c,std::string value) {
    const auto result=c.execute_prepared("INSERT INTO dc24h_core_fixture(value,number_value) VALUES (?,?)",{std::move(value),std::int64_t{0}});
    require(result.affected_rows==1&&result.insert_id>0);return result.insert_id;
}
std::string count(Connection& c,std::string value) {
    return *c.scalar("SELECT COUNT(*) FROM dc24h_core_fixture WHERE value=?",{std::move(value)});
}
void transactions(Connection& c,const Options& options) {
    Connection observer(options);
    {auto tx=c.transaction();(void)insert(c,"RolledBackPrivate");require(c.in_transaction()&&count(observer,"RolledBackPrivate")=="0");}
    require(!c.in_transaction()&&count(c,"RolledBackPrivate")=="0");
    {auto tx=c.transaction();(void)insert(c,"CommittedPrivate");tx.commit();require(!c.in_transaction()&&count(observer,"CommittedPrivate")=="1");
     auto next=c.transaction();(void)insert(c,"LaterPrivate");(void)rejects(ErrorKind::transaction,[&]{tx.rollback();});next.rollback();}
    require(count(c,"LaterPrivate")=="0");
    {auto tx=c.transaction();
     (void)rejects(ErrorKind::limit,[&]{(void)c.query("INSERT INTO dc24h_core_fixture(value,number_value) VALUES (?,0) RETURNING id,value",{std::string("ReturningPrivate")},QueryLimits{0,2,65536,1048576});});
     (void)rejects(ErrorKind::transaction,[&]{tx.commit();});tx.rollback();require(count(observer,"ReturningPrivate")=="0");}
    (void)rejects(ErrorKind::limit,[&]{(void)c.query("INSERT INTO dc24h_core_fixture(value,number_value) VALUES (?,0) RETURNING id",{std::string("AutocommittedPrivate")},QueryLimits{0,1,65536,1048576});});
    require(count(observer,"AutocommittedPrivate")=="1"); // A result error does not undo an autocommitted write.

    {auto tx=c.transaction();auto moved=std::move(tx);(void)rejects(ErrorKind::transaction,[&]{(void)c.transaction();});moved.rollback();}
    {auto tx=c.transaction();const auto duplicate=rejects(ErrorKind::constraint,[&]{(void)insert(c,"CommittedPrivate");});
     require(duplicate.native_code()==1062&&!duplicate.outcome_unknown());
     (void)rejects(ErrorKind::transaction,[&]{tx.commit();});(void)rejects(ErrorKind::transaction,[&]{(void)c.query("SELECT 1");});tx.rollback();}
    for(const auto sql:{"CREATE TABLE ForbiddenPrivate(id INT)","COMMIT","SET autocommit=1","/* comment */ SELECT 1"}) {
        auto tx=c.transaction();(void)insert(c,"DdlGuardPrivate");
        (void)rejects(ErrorKind::transaction,[&]{c.execute(sql);});(void)rejects(ErrorKind::transaction,[&]{tx.commit();});tx.rollback();
        require(count(observer,"DdlGuardPrivate")=="0");
    }
    {auto tx=c.transaction();(void)rejects(ErrorKind::transaction,[&]{(void)migrate(c);});tx.rollback();}
    try{auto tx=c.transaction();(void)insert(c,"ExceptionPrivate");throw 7;}catch(int){}
    require(count(observer,"ExceptionPrivate")=="0");
    auto expired=[&]{Connection temporary(options);auto tx=temporary.transaction();(void)insert(temporary,"ExpiredPrivate");return tx;}();
    (void)rejects(ErrorKind::transaction,[&]{expired.commit();});require(count(observer,"ExpiredPrivate")=="0");
    require(c.query("WITH x AS (SELECT 7 AS n) SELECT n FROM x").rows[0][0]=="7");
}
void thread_ownership(Connection& c,const Options& options) {
    std::exception_ptr error;
    std::jthread worker([&]{try{(void)rejects(ErrorKind::wrong_thread,[&]{(void)c.query("SELECT 1");});}catch(...){error=std::current_exception();}});
    worker.join();if(error){std::rethrow_exception(error);}require(c.scalar("SELECT 42")=="42");
    auto tx=c.transaction();
    std::jthread committer([&]{try{(void)rejects(ErrorKind::wrong_thread,[&]{tx.commit();});}catch(...){error=std::current_exception();}});
    committer.join();if(error){std::rethrow_exception(error);}tx.rollback();
    Connection owned(options);
    auto foreign=owned.transaction();(void)insert(owned,"ForeignDestructorPrivate");
    std::jthread cleanup([scope=std::move(foreign)]{});cleanup.join();
    (void)rejects(ErrorKind::connection,[&]{owned.ping();});
    require(count(c,"ForeignDestructorPrivate")=="0");
}
void allocation_failures(Connection& c) {
    // Conversion in the legacy text API must also make the scope rollback-only.
    const std::vector<std::string> values{"ConversionPrivate"};
    {auto tx=c.transaction();bool bad=false;fail_after=0;
     try{c.execute("INSERT INTO dc24h_core_fixture(value,number_value) VALUES (?,0)",values);}
     catch(const std::bad_alloc&){bad=true;}fail_after=-1;
     require(bad);(void)rejects(ErrorKind::transaction,[&]{tx.commit();});tx.rollback();}
    bool failed=false,succeeded=false;
    for(int budget=0;budget<64;++budget) {
        auto tx=c.transaction();(void)insert(c,"AllocationPrivate");
        bool bad=false;fail_after=budget;
        try{(void)c.query("SELECT REPEAT('p',4096) AS DatabaseCorePrivateAliasThatIsLong,REPEAT('q',4096) AS b UNION ALL SELECT 'r','s'");}
        catch(const std::bad_alloc&){bad=true;}
        fail_after=-1;
        if(bad){failed=true;(void)rejects(ErrorKind::transaction,[&]{tx.commit();});}else{succeeded=true;}
        tx.rollback();require(count(c,"AllocationPrivate")=="0"&&c.scalar("SELECT 42")=="42");
    }
    require(failed&&succeeded);
}
void lock_conflicts(Connection& c,const Options& options) {
    const auto first=insert(c,"LockA"),second=insert(c,"LockB");
    const auto update=[](Connection& connection,std::uint64_t key){(void)connection.execute_prepared("UPDATE dc24h_core_fixture SET number_value=number_value+1 WHERE id=?",{key});};
    Connection rival(options);rival.execute("SET SESSION innodb_lock_wait_timeout=1");
    {auto held=c.transaction();update(c,first);auto contender=rival.transaction();
     const auto timeout=rejects(ErrorKind::conflict,[&]{update(rival,first);});require(timeout.native_code()==1205);
     (void)rejects(ErrorKind::transaction,[&]{contender.commit();});contender.rollback();held.rollback();}
    std::barrier barrier(2);std::array<std::exception_ptr,2> errors;std::atomic<unsigned> commits{},conflicts{};
    std::array<std::jthread,2> workers;
    for(unsigned n=0;n<2;++n){workers[n]=std::jthread([&,n]{bool arrived=false;try{
        Connection connection(options);auto tx=connection.transaction();update(connection,n?second:first);
        arrived=true;barrier.arrive_and_wait();
        try{update(connection,n?first:second);tx.commit();++commits;}
        catch(const Error& e){if(e.kind()!=ErrorKind::conflict||e.native_code()!=1213){throw;}++conflicts;tx.rollback();}
    }catch(...){if(!arrived){barrier.arrive_and_drop();}errors[n]=std::current_exception();}});}
    for(auto& worker:workers){worker.join();}for(const auto& error:errors){if(error){std::rethrow_exception(error);}}
    require(commits==1&&conflicts==1);
    require(c.scalar("SELECT SUM(number_value) FROM dc24h_core_fixture WHERE value IN ('LockA','LockB')")=="2");
}
void lost_connections(Connection& observer,const Options& options) {
    Connection victim(options);const auto key=*victim.scalar("SELECT CONNECTION_ID()");
    auto tx=victim.transaction();(void)insert(victim,"LostCommitPrivate");
    observer.execute("KILL CONNECTION "+key);
    const auto failure=rejects(ErrorKind::connection,[&]{tx.commit();});require(failure.outcome_unknown()&&failure.operation()==Operation::commit);
    (void)rejects(ErrorKind::connection,[&]{(void)victim.query("SELECT 1");});
    require(count(observer,"LostCommitPrivate")=="0");
    Connection read(options);const auto read_key=*read.scalar("SELECT CONNECTION_ID()");observer.execute("KILL CONNECTION "+read_key);
    (void)rejects(ErrorKind::connection,[&]{(void)read.query("SELECT 1");});
    (void)rejects(ErrorKind::connection,[&]{read.ping();});
}
void integration(const Options& options) {
    concurrent_initialization(options);
    Connection c(options);c.ping();
    c.execute("CREATE TABLE dc24h_core_fixture(id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,value VARCHAR(512) NOT NULL UNIQUE,number_value BIGINT NOT NULL) ENGINE=InnoDB");
    typed_values(c);boundaries(c);transactions(c,options);thread_ownership(c,options);allocation_failures(c);lock_conflicts(c,options);lost_connections(c,options);
    (void)rejects(ErrorKind::limit,[&]{(void)c.query("SELECT value FROM dc24h_core_fixture ORDER BY id",{},QueryLimits{1,1,65536,1048576});});
    require(c.scalar("SELECT 42")=="42");
    (void)rejects(ErrorKind::result_shape,[&]{(void)c.query("CALL dc24h_core_multiple_results()");});
    require(c.scalar("SELECT 42")=="42");
    Connection manual(options);
    (void)rejects(ErrorKind::result_shape,[&]{(void)manual.query("START TRANSACTION");});
    (void)rejects(ErrorKind::connection,[&]{manual.ping();});
    Connection autocommit(options);
    (void)rejects(ErrorKind::transaction,[&]{autocommit.execute("SET autocommit=0");});
    (void)rejects(ErrorKind::connection,[&]{autocommit.ping();});
}
void read_only(Options options) {
    options.user="runtime";Connection c(options);
    const auto error=rejects(ErrorKind::statement,[&]{(void)insert(c,"ReadOnlyPrivate");});
    require(!error.outcome_unknown()&&error.native_code()!=0);require(c.scalar("SELECT 42")=="42");
}
}
int main(int argc,char* argv[]){try{
    if(argc==1){unit();}
    else if(argc==3||argc==4){Options options;options.enabled=true;options.socket=argv[1];options.password_file=argv[2];options.timeout=3;
        if(argc==4){read_only(options);}else{integration(options);}}
    else{throw std::runtime_error("Invalid Database Core test arguments");}
    std::cout<<"Database Core checks passed\n";
}catch(const std::exception& error){fail_after=-1;std::cerr<<error.what()<<'\n';return 1;}}
