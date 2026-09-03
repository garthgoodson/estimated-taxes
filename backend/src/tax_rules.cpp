#include "estimated_taxes/tax_rules.hpp"
#include "estimated_taxes/sqlite.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <charconv>
#include <memory>
#include <sstream>
#include <string_view>

namespace estimated_taxes {
namespace {
constexpr int kRuleSchemaVersion = 3;

[[noreturn]] void storage_error(sqlite3* db, std::string_view op) { throw StorageError(std::string(op) + ": " + sqlite3_errmsg(db)); }
void exec(sqlite3* db, const char* sql) { char* error{}; if (sqlite3_exec(db, sql, nullptr, nullptr, &error) != SQLITE_OK) { std::string m = error ? error : sqlite3_errmsg(db); sqlite3_free(error); throw StorageError(m); } }
using Statement = sqlite::Statement;
class Transaction { public: explicit Transaction(sqlite3* db):db_(db){exec(db,"BEGIN IMMEDIATE");} ~Transaction(){if(!done_) sqlite3_exec(db_,"ROLLBACK",nullptr,nullptr,nullptr);} void commit(){exec(db_,"COMMIT");done_=true;} private:sqlite3* db_;bool done_{}; };
void bind(sqlite3_stmt* s,int i,std::int64_t v){if(sqlite3_bind_int64(s,i,v)!=SQLITE_OK)throw StorageError("bind integer failed");}
void bind(sqlite3_stmt* s,int i,const std::string& v){if(sqlite3_bind_text(s,i,v.c_str(),-1,SQLITE_TRANSIENT)!=SQLITE_OK)throw StorageError("bind text failed");}
void done(sqlite3* db,sqlite3_stmt* s,std::string_view op){if(sqlite3_step(s)!=SQLITE_DONE)storage_error(db,op);}
std::string jurisdiction_name(Jurisdiction j){return j==Jurisdiction::federal?"federal":"california";}
Jurisdiction jurisdiction_from(std::string_view s){if(s=="federal")return Jurisdiction::federal;if(s=="california")return Jurisdiction::california;throw StorageError("invalid stored jurisdiction");}
bool date_ok(const std::string& d){if(d.size()!=10||d[4]!='-'||d[7]!='-')return false;for(int i:{0,1,2,3,5,6,8,9})if(d[i]<'0'||d[i]>'9')return false;int y=std::stoi(d.substr(0,4)),m=std::stoi(d.substr(5,2)),day=std::stoi(d.substr(8,2));constexpr int days[]={31,28,31,30,31,30,31,31,30,31,30,31};return y>=2026&&y<=2027&&m>=1&&m<=12&&day>=1&&day<=days[m-1];}
void validate_brackets(const std::vector<Bracket>& b){if(b.empty()||b.front().lower_bound_cents!=0)throw RuleValidationError("brackets must begin at zero");for(size_t i=0;i<b.size();++i){if(b[i].lower_bound_cents<0||b[i].rate_ppm<0||b[i].rate_ppm>1'000'000)throw RuleValidationError("invalid bracket value");if(i+1==b.size()){if(b[i].upper_bound_cents)throw RuleValidationError("final bracket must be open-ended");}else{if(!b[i].upper_bound_cents||*b[i].upper_bound_cents<=b[i].lower_bound_cents||*b[i].upper_bound_cents!=b[i+1].lower_bound_cents)throw RuleValidationError("brackets must be contiguous");}}}
void validate_installments(const std::vector<Installment>& in){if(in.size()!=4)throw RuleValidationError("exactly four installments are required");int prior=0;std::string date;for(size_t i=0;i<in.size();++i){const auto& x=in[i];if(x.quarter!=int(i)+1||!date_ok(x.due_date)||(!date.empty()&&x.due_date<=date)||x.period_ppm<0||x.period_ppm>1'000'000||x.cumulative_ppm<prior||x.cumulative_ppm>1'000'000||x.period_ppm!=x.cumulative_ppm-prior)throw RuleValidationError("invalid installments");date=x.due_date;prior=x.cumulative_ppm;}if(prior!=1'000'000)throw RuleValidationError("installments must total 100 percent");}
void nonnegative(Cents n){if(n<0)throw RuleValidationError("rule amount cannot be negative");}
std::string encode(const std::vector<Bracket>& b,const std::vector<Installment>& i){std::ostringstream o;for(auto& x:b)o<<x.lower_bound_cents<<','<<(x.upper_bound_cents?std::to_string(*x.upper_bound_cents):"N")<<','<<x.rate_ppm<<';';o<<'|';if(i.empty())o<<'-';for(auto& x:i)o<<x.quarter<<','<<x.due_date<<','<<x.period_ppm<<','<<x.cumulative_ppm<<';';return o.str();}
std::vector<std::string> split(const std::string& text,char sep){std::vector<std::string> r;std::stringstream s(text);std::string x;while(std::getline(s,x,sep))if(!x.empty())r.push_back(x);return r;}
std::int64_t number(const std::string& s){std::int64_t v{};auto [p,e]=std::from_chars(s.data(),s.data()+s.size(),v);if(e!=std::errc{}||p!=s.data()+s.size())throw StorageError("invalid stored rule payload");return v;}
void decode(const std::string& text,std::vector<Bracket>& b,std::vector<Installment>& i){auto sections=split(text,'|');if(sections.size()!=2)throw StorageError("invalid stored rule payload");for(auto& e:split(sections[0],';')){auto f=split(e,',');if(f.size()!=3)throw StorageError("invalid bracket payload");b.push_back({number(f[0]),f[1]=="N"?std::nullopt:std::optional<Cents>{number(f[1])},int(number(f[2]))});}if(sections[1]=="-")return;for(auto& e:split(sections[1],';')){auto f=split(e,',');if(f.size()!=4)throw StorageError("invalid installment payload");i.push_back({int(number(f[0])),f[1],int(number(f[2])),int(number(f[3]))});}}
std::string sources_encode(const std::vector<Source>& s){std::ostringstream o;for(auto& x:s)o<<x.agency<<'\t'<<x.title<<'\t'<<x.url<<'\t'<<x.publication_date<<'\t'<<x.verification_date<<'\t'<<x.interpretation_note<<'\n';return o.str();}
std::vector<Source> sources_decode(const std::string& t){std::vector<Source> out;for(auto& line:split(t,'\n')){auto f=split(line,'\t');if(f.size()!=6)throw StorageError("invalid source payload");out.push_back({f[0],f[1],f[2],f[3],f[4],f[5]});}return out;}
std::string federal_payload(const FederalRules& r){return std::to_string(r.standard_deduction_cents)+"/"+std::to_string(r.age_or_blind_addition_cents)+"/"+std::to_string(r.capital_loss_limit_cents)+"/"+std::to_string(r.niit_threshold_cents)+"/"+std::to_string(r.niit_rate_ppm)+"/"+std::to_string(r.additional_medicare_threshold_cents)+"/"+std::to_string(r.additional_medicare_rate_ppm)+"/"+encode(r.ordinary_brackets,r.installments)+"/"+encode(r.preferential_brackets,{});}
std::string california_payload(const CaliforniaRules& r){return std::to_string(r.standard_deduction_cents)+"/"+std::to_string(r.joint_personal_exemption_credit_cents)+"/"+std::to_string(r.capital_loss_limit_cents)+"/"+std::to_string(r.behavioral_health_services_threshold_cents)+"/"+std::to_string(r.behavioral_health_services_rate_ppm)+"/"+encode(r.ordinary_brackets,r.installments);}
FederalRules federal_decode(const std::string& p){auto f=split(p,'/');if(f.size()!=9)throw StorageError("invalid federal payload");FederalRules r; r.standard_deduction_cents=number(f[0]);r.age_or_blind_addition_cents=number(f[1]);r.capital_loss_limit_cents=number(f[2]);r.niit_threshold_cents=number(f[3]);r.niit_rate_ppm=int(number(f[4]));r.additional_medicare_threshold_cents=number(f[5]);r.additional_medicare_rate_ppm=int(number(f[6]));decode(f[7],r.ordinary_brackets,r.installments);std::vector<Installment> unused;decode(f[8],r.preferential_brackets,unused);return r;}
CaliforniaRules california_decode(const std::string& p){auto f=split(p,'/');if(f.size()!=6)throw StorageError("invalid California payload");CaliforniaRules r;r.standard_deduction_cents=number(f[0]);r.joint_personal_exemption_credit_cents=number(f[1]);r.capital_loss_limit_cents=number(f[2]);r.behavioral_health_services_threshold_cents=number(f[3]);r.behavioral_health_services_rate_ppm=int(number(f[4]));decode(f[5],r.ordinary_brackets,r.installments);return r;}
std::vector<Installment> federal_installments(){return {{1,"2026-04-15",250000,250000},{2,"2026-06-15",250000,500000},{3,"2026-09-15",250000,750000},{4,"2027-01-15",250000,1000000}};}
std::vector<Installment> california_installments(){return {{1,"2026-04-15",300000,300000},{2,"2026-06-15",400000,700000},{3,"2026-09-15",0,700000},{4,"2027-01-15",300000,1000000}};}
} // namespace

void validate(const FederalRules& r){if(r.tax_year!=2026)throw RuleValidationError("federal tax year must be 2026");for(Cents v:{r.standard_deduction_cents,r.age_or_blind_addition_cents,r.capital_loss_limit_cents,r.niit_threshold_cents,r.additional_medicare_threshold_cents})nonnegative(v);if(r.niit_rate_ppm<0||r.niit_rate_ppm>1000000||r.additional_medicare_rate_ppm<0||r.additional_medicare_rate_ppm>1000000)throw RuleValidationError("invalid federal rate");validate_brackets(r.ordinary_brackets);validate_brackets(r.preferential_brackets);validate_installments(r.installments);}
void validate(const CaliforniaRules& r){if(r.tax_year!=2026)throw RuleValidationError("California tax year must be 2026");for(Cents v:{r.standard_deduction_cents,r.joint_personal_exemption_credit_cents,r.capital_loss_limit_cents,r.behavioral_health_services_threshold_cents})nonnegative(v);if(r.behavioral_health_services_rate_ppm<0||r.behavioral_health_services_rate_ppm>1000000)throw RuleValidationError("invalid California rate");validate_brackets(r.ordinary_brackets);validate_installments(r.installments);}
FederalRules official_federal_rules(){return {2026,3220000,165000,{{0,2480000,100000},{2480000,10080000,120000},{10080000,21140000,220000},{21140000,40355000,240000},{40355000,51245000,320000},{51245000,76870000,350000},{76870000,std::nullopt,370000}},{{0,9890000,0},{9890000,61370000,150000},{61370000,std::nullopt,200000}},300000,25000000,38000,25000000,9000,federal_installments()};}
CaliforniaRules official_california_rules(){return {2026,1141200,{{0,2215800,10000},{2215800,5252800,20000},{5252800,8290400,40000},{8290400,11508400,60000},{11508400,14544800,80000},{14544800,74295800,93000},{74295800,89154200,103000},{89154200,148590600,113000},{148590600,std::nullopt,123000}},30600,300000,100000000,10000,california_installments()};}
std::vector<Source> official_federal_sources(){return {{"IRS","2026 Form 1040-ES","https://www.irs.gov/pub/irs-pdf/f1040es.pdf","2026","2026-04-01","Standard deduction, Schedule Y-1, and payment dates."},{"IRS","Publication 505 (2026)","https://www.irs.gov/publications/p505","2026","2026-04-01","Preferential schedule, loss limit, and additional taxes."},{"IRS","Topic no. 559","https://www.irs.gov/taxtopics/tc559","not stated","2026-04-01","NIIT threshold and rate."},{"IRS","Topic no. 560","https://www.irs.gov/taxtopics/tc560","2019-06-04","2026-04-01","Additional Medicare threshold and rate."}};}
std::vector<Source> official_california_sources(){return {{"California FTB","2026 Form 540-ES instructions","https://www.ftb.ca.gov/forms/2026/2026-540-es-instructions.html","2026","2026-04-01","Standard deduction, installments, and Behavioral Health Services Tax."},{"California FTB","2025 Form 540 instructions","https://www.ftb.ca.gov/forms/2025/2025-540-booklet.html","2025","2026-04-01","Schedule Y, used as directed by the 2026 worksheet."},{"California FTB","2025 Form 540 2EZ booklet","https://www.ftb.ca.gov/forms/2025/2025-540-2ez-booklet.html","2025","2026-04-01","$306 joint credit derived from two published $153 personal exemptions."},{"California FTB","2025 Schedule D (540) instructions","https://www.ftb.ca.gov/forms/2025/2025-540-d-instructions.html","2025","2026-04-01","$3,000 capital-loss limit for MFJ."}};}
struct RuleStore::Connection { explicit Connection(const std::string& path) : sqlite(path) {} sqlite::Connection sqlite; sqlite3* db{sqlite.get()}; };
RuleStore::RuleStore(const std::string& path){InputStore bootstrap(path);connection_=std::make_unique<Connection>(path);Transaction tx(connection_->db);Statement q(connection_->db,"SELECT MAX(version) FROM schema_migrations");if(sqlite3_step(q.get())!=SQLITE_ROW)storage_error(connection_->db,"schema version");int v=sqlite3_column_int(q.get(),0);if(v>kRuleSchemaVersion)throw StorageError("database schema is newer than this application");if(v<2){exec(connection_->db,"CREATE TABLE rule_revisions (id INTEGER PRIMARY KEY, jurisdiction TEXT NOT NULL CHECK(jurisdiction IN ('federal','california')), official_baseline INTEGER NOT NULL CHECK(official_baseline IN(0,1)), modified INTEGER NOT NULL CHECK(modified IN(0,1)), payload TEXT NOT NULL, sources TEXT NOT NULL, created_order INTEGER NOT NULL UNIQUE, CHECK(official_baseline=0 OR modified=0)); CREATE TABLE active_rule_revisions (jurisdiction TEXT PRIMARY KEY CHECK(jurisdiction IN ('federal','california')), revision_id INTEGER NOT NULL REFERENCES rule_revisions(id)); CREATE TRIGGER prevent_official_rule_update BEFORE UPDATE ON rule_revisions WHEN OLD.official_baseline = 1 BEGIN SELECT RAISE(ABORT, 'official baselines are immutable'); END; CREATE TRIGGER prevent_official_rule_delete BEFORE DELETE ON rule_revisions WHEN OLD.official_baseline = 1 BEGIN SELECT RAISE(ABORT, 'official baselines are immutable'); END;");auto ins=[&](Jurisdiction j,const std::string&p,const std::vector<Source>&s,int order){ const std::string sources = sources_encode(s); Statement x(connection_->db,"INSERT INTO rule_revisions(jurisdiction,official_baseline,modified,payload,sources,created_order) VALUES(?,?,?,?,?,?)");if (sqlite3_bind_text(x.get(), 1, j == Jurisdiction::federal ? "federal" : "california", -1, SQLITE_STATIC) != SQLITE_OK) throw StorageError("bind jurisdiction failed"); bind(x.get(),2,std::int64_t{1});bind(x.get(),3,std::int64_t{0});bind(x.get(),4,p);bind(x.get(),5,sources);bind(x.get(),6,order);done(connection_->db,x.get(),"seed rules");return sqlite3_last_insert_rowid(connection_->db);};auto f=ins(Jurisdiction::federal,federal_payload(official_federal_rules()),official_federal_sources(),1);auto c=ins(Jurisdiction::california,california_payload(official_california_rules()),official_california_sources(),2);for(auto [j,id]:{std::pair{Jurisdiction::federal,f},std::pair{Jurisdiction::california,c}}){const std::string jurisdiction=jurisdiction_name(j);Statement x(connection_->db,"INSERT INTO active_rule_revisions VALUES(?,?)");bind(x.get(),1,jurisdiction);bind(x.get(),2,id);done(connection_->db,x.get(),"seed active rules");}exec(connection_->db,"INSERT INTO schema_migrations VALUES(2)");}tx.commit();}
RuleStore::~RuleStore()=default;

namespace {
RuleRevision read_revision(sqlite3* db, std::int64_t id) {
  Statement s(db,"SELECT jurisdiction,official_baseline,modified,payload,sources FROM rule_revisions WHERE id=?"); bind(s.get(),1,id);
  if(sqlite3_step(s.get())!=SQLITE_ROW) throw StorageError("rule revision not found");
  RuleRevision r; r.id=id; r.jurisdiction=jurisdiction_from(s.required_text(0, "rule jurisdiction")); r.official_baseline=sqlite3_column_int(s.get(),1)!=0; r.modified=sqlite3_column_int(s.get(),2)!=0;
  const std::string payload = s.required_text(3, "rule payload"); r.sources=sources_decode(s.required_text(4, "rule sources"));
  if(r.jurisdiction==Jurisdiction::federal) { r.federal=federal_decode(payload); validate(*r.federal); } else { r.california=california_decode(payload); validate(*r.california); }
  return r;
}
std::int64_t insert_revision(sqlite3* db, Jurisdiction j, const std::string& payload, const std::vector<Source>& sources, bool modified) {
  const std::string jurisdiction = jurisdiction_name(j);
  const std::string encoded_sources = sources_encode(sources);
  Statement order(db,"SELECT COALESCE(MAX(created_order),0)+1 FROM rule_revisions"); if(sqlite3_step(order.get())!=SQLITE_ROW)storage_error(db,"read revision order");
  Statement s(db,"INSERT INTO rule_revisions(jurisdiction,official_baseline,modified,payload,sources,created_order) VALUES(?,0,?,?,?,?)"); bind(s.get(),1,jurisdiction);bind(s.get(),2,modified?1:0);bind(s.get(),3,payload);bind(s.get(),4,encoded_sources);bind(s.get(),5,sqlite3_column_int64(order.get(),0));done(db,s.get(),"insert rule revision");return sqlite3_last_insert_rowid(db);
}
void move_active(sqlite3* db,Jurisdiction j,std::int64_t id){const std::string jurisdiction=jurisdiction_name(j);Statement s(db,"UPDATE active_rule_revisions SET revision_id=? WHERE jurisdiction=?");bind(s.get(),1,id);bind(s.get(),2,jurisdiction);done(db,s.get(),"move active rule");if(sqlite3_changes(db)!=1)throw StorageError("active rule reference missing");}
}

ActiveRules RuleStore::load_active() const {
  std::optional<std::int64_t> federal_id, california_id;
  { Statement s(connection_->db,"SELECT jurisdiction,revision_id FROM active_rule_revisions");
    while(sqlite3_step(s.get())==SQLITE_ROW) { if(jurisdiction_from(s.required_text(0, "active rule jurisdiction"))==Jurisdiction::federal) federal_id=sqlite3_column_int64(s.get(),1); else california_id=sqlite3_column_int64(s.get(),1); }
  }
  if(!federal_id||!california_id)throw StorageError("active rules are incomplete");
  RuleRevision f=read_revision(connection_->db,*federal_id); RuleRevision c=read_revision(connection_->db,*california_id);
  const bool federal_customized = f.federal != official_federal_rules();
  const bool california_customized = c.california != official_california_rules();
  return {std::move(f), std::move(c), federal_customized, california_customized};
}

void RuleStore::replace_active(const FederalRules& federal,const CaliforniaRules& california) {
  validate(federal);validate(california); Transaction tx(connection_->db); const bool f_modified=federal!=official_federal_rules(),c_modified=california!=official_california_rules();
  auto f=insert_revision(connection_->db,Jurisdiction::federal,federal_payload(federal),official_federal_sources(),f_modified);
  auto c=insert_revision(connection_->db,Jurisdiction::california,california_payload(california),official_california_sources(),c_modified);
  move_active(connection_->db,Jurisdiction::federal,f);move_active(connection_->db,Jurisdiction::california,c);tx.commit();
}

void RuleStore::restore_official(Jurisdiction jurisdiction) {
  Transaction tx(connection_->db); if(jurisdiction==Jurisdiction::federal){auto id=insert_revision(connection_->db,jurisdiction,federal_payload(official_federal_rules()),official_federal_sources(),false);move_active(connection_->db,jurisdiction,id);}else{auto id=insert_revision(connection_->db,jurisdiction,california_payload(official_california_rules()),official_california_sources(),false);move_active(connection_->db,jurisdiction,id);}tx.commit();
}
void RuleStore::restore_archived(Jurisdiction jurisdiction,std::int64_t revision_id) {
  RuleRevision source=read_revision(connection_->db,revision_id);if(source.jurisdiction!=jurisdiction||source.official_baseline)throw RuleValidationError("revision cannot be restored for this jurisdiction"); Transaction tx(connection_->db);auto id=insert_revision(connection_->db,jurisdiction,source.federal?federal_payload(*source.federal):california_payload(*source.california),source.sources,source.modified);move_active(connection_->db,jurisdiction,id);tx.commit();
}
std::vector<RuleRevision> RuleStore::archived_revisions(Jurisdiction jurisdiction) const {
  std::int64_t active_id{};
  const std::string jurisdiction_value = jurisdiction_name(jurisdiction);
  { Statement active(connection_->db,"SELECT revision_id FROM active_rule_revisions WHERE jurisdiction=?");bind(active.get(),1,jurisdiction_value);if(sqlite3_step(active.get())!=SQLITE_ROW)throw StorageError("active rule missing");active_id=sqlite3_column_int64(active.get(),0); }
  std::vector<std::int64_t> ids;
  { Statement s(connection_->db,"SELECT id FROM rule_revisions WHERE jurisdiction=? AND official_baseline=0 AND id<>? ORDER BY created_order");bind(s.get(),1,jurisdiction_value);bind(s.get(),2,active_id);while(sqlite3_step(s.get())==SQLITE_ROW) ids.push_back(sqlite3_column_int64(s.get(),0)); }
  std::vector<RuleRevision> out; for (const auto id : ids) out.push_back(read_revision(connection_->db, id)); return out;
}

}  // namespace estimated_taxes
