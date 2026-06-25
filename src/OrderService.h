#pragma once

#include <string>
#include <utility>
#include <vector>

class Database;

// One line of an order, as shown to the user.
struct OrderLine {
    std::string productName;
    int quantity;
    double unitPrice;  // the price snapshotted at order time
};

// An order plus its joined customer name and line items, for display.
struct OrderView {
    int id;
    std::string customerName;
    double totalAmount;
    std::string status;
    std::string createdAt;
    std::vector<OrderLine> items;
};

// Data-access for orders, including the atomic placeOrder transaction.
class OrderService {
public:
    explicit OrderService(Database& db) : db_(db) {}

    // Lists every order with its customer name and line items (via JOINs).
    std::vector<OrderView> listOrders();

    // Places an order ATOMICALLY. `items` is a list of (productId, quantity).
    // Returns the new order id on success. On any problem (missing product,
    // insufficient stock, DB error) it rolls back so the database is left exactly
    // as it was, then throws std::runtime_error with a clear message.
    int placeOrder(int customerId,
                   const std::vector<std::pair<int, int>>& items);

private:
    Database& db_;
};
