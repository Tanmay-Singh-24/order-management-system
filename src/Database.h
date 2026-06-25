#pragma once

#include <mysqlx/xdevapi.h>
#include <string>
#include <utility>

// Thin RAII wrapper around a MySQL X DevAPI session.
//
// RAII = "Resource Acquisition Is Initialization": the connection is opened in
// the constructor and released automatically when the Database object is
// destroyed (mysqlx::Session closes itself in its own destructor). Callers never
// have to remember to disconnect, and the connection is closed even if an
// exception unwinds the stack.
class Database {
public:
    // Opens the session. Connection settings live in one place (Database.cpp).
    Database();

    // A live database connection must not be copied (two owners closing one
    // connection is a bug). Deleting copy makes Database move-only-ish and the
    // intent explicit.
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    // Run a parameterized SQL statement and return its result.
    //
    // Any '?' placeholders in `sql` are filled, in order, by the bound arguments.
    // Crucially, the values travel to the server SEPARATELY from the SQL text, so
    // user input can never be interpreted as SQL — this is our SQL-injection
    // defense. (sizeof...(Args)==0 means a query with no parameters.)
    template <typename... Args>
    mysqlx::SqlResult run(const std::string& sql, Args&&... args) {
        if constexpr (sizeof...(Args) == 0) {
            return session_.sql(sql).execute();
        } else {
            return session_.sql(sql).bind(std::forward<Args>(args)...).execute();
        }
    }

    // Transaction control. Used by OrderService::placeOrder to make order
    // placement atomic (all-or-nothing).
    void beginTransaction();
    void commit();
    void rollback();

private:
    mysqlx::Session session_;
};
