#include "ProductService.h"

#include "Database.h"

int ProductService::addProduct(const std::string& name, double price, int stock) {
    auto result = db_.run(
        "INSERT INTO products (name, price, stock_quantity) VALUES (?, ?, ?)",
        name, price, stock);
    return static_cast<int>(result.getAutoIncrementValue());
}

std::vector<Product> ProductService::listProducts() {
    auto result =
        db_.run("SELECT id, name, price, stock_quantity FROM products ORDER BY id");
    std::vector<Product> out;
    for (mysqlx::Row row = result.fetchOne(); row; row = result.fetchOne()) {
        out.push_back(Product{
            row[0].get<int>(),
            row[1].get<std::string>(),
            row[2].get<double>(),
            row[3].get<int>()});
    }
    return out;
}
