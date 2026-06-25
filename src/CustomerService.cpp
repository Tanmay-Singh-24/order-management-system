#include "CustomerService.h"

#include "Database.h"

int CustomerService::addCustomer(const std::string& name, const std::string& email) {
    // '?' placeholders + bound values: name/email are sent as data, never spliced
    // into the SQL text. The UNIQUE constraint on email enforces no duplicates
    // (a duplicate throws mysqlx::Error, which the caller reports).
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
