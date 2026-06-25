#pragma once

#include <string>
#include <vector>

class Database;  // forward declaration — we only need a reference here

// Plain data carrier for a customer row.
struct Customer {
    int id;
    std::string name;
    std::string email;
};

// Data-access for the `customers` table. All SQL is parameterized.
class CustomerService {
public:
    explicit CustomerService(Database& db) : db_(db) {}

    // Inserts a customer and returns the new auto-increment id.
    int addCustomer(const std::string& name, const std::string& email);

    // Returns all customers, ordered by id.
    std::vector<Customer> listCustomers();

private:
    Database& db_;  // not owned; the Database outlives this service
};
