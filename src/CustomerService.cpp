#include "CustomerService.h"

#include <stdexcept>

#include "Database.h"

int CustomerService::addCustomer(const std::string& name, const std::string& email) {
    // Friendly pre-check for the common "email already registered" case, so the
    // user gets a clear message instead of a raw driver error. This is a UX
    // convenience only — the UNIQUE(email) constraint is still the real guarantee
    // (same app-check + database-constraint pattern used for stock in placeOrder).
    {
        auto existing = db_.run("SELECT 1 FROM customers WHERE email = ?", email);
        if (existing.fetchOne()) {
            throw std::runtime_error(
                "A customer with email '" + email + "' already exists.");
        }
    }

    // '?' placeholders + bound values: name/email are sent as data, never spliced
    // into the SQL text.
    auto result = db_.run(
        "INSERT INTO customers (name, email) VALUES (?, ?)", name, email);
    return static_cast<int>(result.getAutoIncrementValue());
}

std::vector<Customer> CustomerService::listCustomers() {
    auto result = db_.run("SELECT id, name, email FROM customers ORDER BY id");
    std::vector<Customer> out;
    for (mysqlx::Row row = result.fetchOne(); row; row = result.fetchOne()) {
        out.push_back(Customer{
            row[0].get<int>(),
            row[1].get<std::string>(),
            row[2].get<std::string>()});
    }
    return out;
}
