#pragma once

#include "estimated_taxes/input_store.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

struct sqlite3;
struct sqlite3_stmt;

namespace estimated_taxes::sqlite {

class Connection {
public:
  explicit Connection(const std::string& path);
  ~Connection();

  Connection(const Connection&) = delete;
  Connection& operator=(const Connection&) = delete;

  [[nodiscard]] sqlite3* get() const;
  void execute(const char* sql) const;

private:
  sqlite3* database_{};
};

class Statement {
public:
  Statement(sqlite3* database, const char* sql);
  ~Statement();

  Statement(const Statement&) = delete;
  Statement& operator=(const Statement&) = delete;

  void bind_integer(int index, std::int64_t value) const;
  void bind_text(int index, std::string_view value) const;
  void bind_null(int index) const;
  [[nodiscard]] bool step_row() const;
  void step_done(std::string_view operation) const;
  void reset() const;

  [[nodiscard]] std::int64_t integer(int column) const;
  [[nodiscard]] std::string required_text(int column, std::string_view field) const;
  [[nodiscard]] std::optional<std::string> optional_text(int column) const;
  [[nodiscard]] sqlite3_stmt* get() const;

private:
  sqlite3* database_;
  sqlite3_stmt* statement_{};
};

class Transaction {
public:
  explicit Transaction(const Connection& connection);
  ~Transaction();

  Transaction(const Transaction&) = delete;
  Transaction& operator=(const Transaction&) = delete;

  void commit();

private:
  const Connection& connection_;
  bool committed_{};
};

}  // namespace estimated_taxes::sqlite
