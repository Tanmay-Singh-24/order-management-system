#include "Database.h"

#include <cstdlib>
#include <string>

namespace {
// Connection settings are read from environment variables so credentials are not
// baked into the binary or committed to source control. The fallbacks are the
// documented local-dev defaults (the project-local instance on the X Protocol
// port), so the app runs out of the box while a real deployment overrides them.
std::string envOr(const char* key, const char* fallback) {
    const char* value = std::getenv(key);
    return value ? std::string(value) : std::string(fallback);
}

int envPort() {
    const char* value = std::getenv("OMS_DB_PORT");
    return value ? std::stoi(value) : 33060;  // X Protocol port
}
}  // namespace

// The member `session_` is constructed in the initializer list, which is what
// "acquires the resource" — opening and authenticating the connection happens
// here. If it fails, the constructor throws and no half-built Database escapes.
Database::Database()
    : session_(mysqlx::SessionSettings(
          mysqlx::SessionOption::HOST, envOr("OMS_DB_HOST", "127.0.0.1"),
          mysqlx::SessionOption::PORT, envPort(),
          mysqlx::SessionOption::USER, envOr("OMS_DB_USER", "orderapp"),
          mysqlx::SessionOption::PWD,  envOr("OMS_DB_PASSWORD", "orderpass"),
          mysqlx::SessionOption::DB,   envOr("OMS_DB_NAME", "ordersdb"))) {}

void Database::beginTransaction() { session_.startTransaction(); }
void Database::commit()           { session_.commit(); }
void Database::rollback()         { session_.rollback(); }
