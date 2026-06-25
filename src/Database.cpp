#include "Database.h"

namespace {
// All connection settings in ONE place. For a real deployment these would come
// from environment variables or a config file rather than being compiled in;
// for a local single-user demo, centralizing them here is clear and sufficient.
const char* kHost = "127.0.0.1";  // loopback only
const int   kPort = 33060;        // X Protocol port (the X DevAPI speaks this)
const char* kUser = "orderapp";   // least-privilege account (DML only on ordersdb)
const char* kPass = "orderpass";  // dev-only password
const char* kDb   = "ordersdb";
}  // namespace

// The member `session_` is constructed in the initializer list, which is what
// "acquires the resource" — opening and authenticating the connection happens
// here. If it fails, the constructor throws and no half-built Database escapes.
Database::Database()
    : session_(mysqlx::SessionSettings(
          mysqlx::SessionOption::HOST, kHost,
          mysqlx::SessionOption::PORT, kPort,
          mysqlx::SessionOption::USER, kUser,
          mysqlx::SessionOption::PWD,  kPass,
          mysqlx::SessionOption::DB,   kDb)) {}

void Database::beginTransaction() { session_.startTransaction(); }
void Database::commit()           { session_.commit(); }
void Database::rollback()         { session_.rollback(); }
