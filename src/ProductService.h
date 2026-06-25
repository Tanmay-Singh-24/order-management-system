#pragma once

#include <string>
#include <vector>

class Database;

// Plain data carrier for a product row. Price is read as a double for display
// and transient math; the authoritative value is stored as DECIMAL in the DB.
struct Product {
    int id;
    std::string name;
    double price;
    int stock;
};

// Data-access for the `products` table. All SQL is parameterized.
class ProductService {
public:
    explicit ProductService(Database& db) : db_(db) {}

    int addProduct(const std::string& name, double price, int stock);
    std::vector<Product> listProducts();

private:
    Database& db_;
};
