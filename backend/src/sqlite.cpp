#include "estimated_taxes/sqlite.hpp"

#include <sqlite3.h>

namespace estimated_taxes::sqlite {
namespace {
[[noreturn]] void throw_error(sqlite3* database, std::string_view operation)
{
  throw StorageError(std::string(operation) + ": " + sqlite3_errmsg(database));
}
}  // namespace

Connection::Connection(const std::string& path)
{
  if (sqlite3_open_v2(path.c_str(), &database_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK) {
    const std::string message = database_ == nullptr ? "open database failed" : sqlite3_errmsg(database_);
    if (database_ != nullptr) sqlite3_close(database_);
    throw StorageError(message);
  }
  execute("PRAGMA foreign_keys = ON");
}

Connection::~Connection() { sqlite3_close(database_); }
sqlite3* Connection::get() const { return database_; }

void Connection::execute(const char* sql) const
{
  char* message = nullptr;
  if (sqlite3_exec(database_, sql, nullptr, nullptr, &message) != SQLITE_OK) {
    const std::string error = message == nullptr ? sqlite3_errmsg(database_) : message;
    sqlite3_free(message);
    throw StorageError(error);
  }
}

Statement::Statement(sqlite3* database, const char* sql) : database_(database)
{
  if (sqlite3_prepare_v2(database_, sql, -1, &statement_, nullptr) != SQLITE_OK) throw_error(database_, "prepare statement");
}
Statement::~Statement() { sqlite3_finalize(statement_); }
void Statement::bind_integer(int index, std::int64_t value) const { if (sqlite3_bind_int64(statement_, index, value) != SQLITE_OK) throw_error(database_, "bind integer"); }
void Statement::bind_text(int index, std::string_view value) const { if (sqlite3_bind_text(statement_, index, value.data(), static_cast<int>(value.size()), SQLITE_TRANSIENT) != SQLITE_OK) throw_error(database_, "bind text"); }
void Statement::bind_null(int index) const { if (sqlite3_bind_null(statement_, index) != SQLITE_OK) throw_error(database_, "bind null"); }
bool Statement::step_row() const { const int result=sqlite3_step(statement_); if(result==SQLITE_ROW)return true; if(result==SQLITE_DONE)return false; throw_error(database_,"read row"); }
void Statement::step_done(std::string_view operation) const { if(sqlite3_step(statement_)!=SQLITE_DONE)throw_error(database_,operation); }
void Statement::reset() const { sqlite3_reset(statement_); sqlite3_clear_bindings(statement_); }
std::int64_t Statement::integer(int column) const { return sqlite3_column_int64(statement_, column); }
std::string Statement::required_text(int column, std::string_view field) const { const auto* value=sqlite3_column_text(statement_,column);if(value==nullptr)throw StorageError("stored "+std::string(field)+" is missing");return reinterpret_cast<const char*>(value); }
std::optional<std::string> Statement::optional_text(int column) const { if(sqlite3_column_type(statement_,column)==SQLITE_NULL)return std::nullopt;return required_text(column,"text"); }
sqlite3_stmt* Statement::get() const{return statement_;}

Transaction::Transaction(const Connection& connection) : connection_(connection) { connection_.execute("BEGIN IMMEDIATE"); }
Transaction::~Transaction(){if(!committed_)sqlite3_exec(connection_.get(),"ROLLBACK",nullptr,nullptr,nullptr);}
void Transaction::commit(){connection_.execute("COMMIT");committed_=true;}
}  // namespace estimated_taxes::sqlite
