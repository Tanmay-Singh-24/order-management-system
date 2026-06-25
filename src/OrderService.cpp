#include "OrderService.h"

#include <stdexcept>
#include <string>

#include "Database.h"

std::vector<OrderView> OrderService::listOrders() {
    // One query joins orders -> customers (for the name) and LEFT JOINs the line
    // items -> products (for each item's product name). LEFT JOIN means an order
    // with no items still appears. Rows come back ordered by order then item, so
    // we can group consecutive rows belonging to the same order.
    auto result = db_.run(
        "SELECT o.id, c.name, o.total_amount, o.status, "
        "       DATE_FORMAT(o.created_at, '%Y-%m-%d %H:%i') AS created, "
        "       p.name, oi.quantity, oi.unit_price "
        "FROM orders o "
        "JOIN customers c ON c.id = o.customer_id "
        "LEFT JOIN order_items oi ON oi.order_id = o.id "
        "LEFT JOIN products p     ON p.id = oi.product_id "
        "ORDER BY o.id, oi.id");

    std::vector<OrderView> out;
    for (mysqlx::Row row = result.fetchOne(); row; row = result.fetchOne()) {
        const int orderId = row[0].get<int>();

        // Start a new OrderView whenever the order id changes.
        if (out.empty() || out.back().id != orderId) {
            OrderView ov;
            ov.id = orderId;
            ov.customerName = row[1].get<std::string>();
            ov.totalAmount = row[2].get<double>();
            ov.status = row[3].get<std::string>();
            ov.createdAt = row[4].get<std::string>();
            out.push_back(std::move(ov));
        }

        // Columns 5..7 are NULL for an order that has no items (the LEFT JOIN).
        if (!row[5].isNull()) {
            out.back().items.push_back(OrderLine{
                row[5].get<std::string>(),
                row[6].get<int>(),
                row[7].get<double>()});
        }
    }
    return out;
}

int OrderService::placeOrder(int customerId,
                             const std::vector<std::pair<int, int>>& items) {
    if (items.empty()) {
        throw std::runtime_error("Cannot place an empty order.");
    }

    // ---- ATOMICITY: everything from here to commit() is one unit of work. ----
    // If any step throws, the catch block rolls the whole thing back, so the DB
    // never ends up with a half-finished order (an order row but no items, or
    // stock decremented without a matching line item, etc.).
    db_.beginTransaction();
    try {
        // Friendly existence check. The orders.customer_id foreign key would also
        // reject a bad id, but this gives a clearer message than a raw FK error.
        {
            auto check = db_.run("SELECT 1 FROM customers WHERE id = ?", customerId);
            if (!check.fetchOne()) {
                throw std::runtime_error(
                    "Customer id " + std::to_string(customerId) + " does not exist.");
            }
        }

        // 1. Insert the order shell: pending, total 0 (we fill these in at the end).
        auto inserted = db_.run(
            "INSERT INTO orders (customer_id, total_amount, status) "
            "VALUES (?, 0, 'pending')",
            customerId);
        const int orderId = static_cast<int>(inserted.getAutoIncrementValue());

        double total = 0.0;

        // 2. Process each requested line item.
        for (const auto& [productId, quantity] : items) {
            if (quantity <= 0) {
                throw std::runtime_error("Quantity must be a positive number.");
            }

            // Read the product's current price + stock. Scoped in its own block so
            // the result set is fully consumed before we issue the next statement.
            std::string productName;
            double unitPrice = 0.0;
            int stock = 0;
            {
                auto pr = db_.run(
                    "SELECT name, price, stock_quantity FROM products WHERE id = ?",
                    productId);
                mysqlx::Row product = pr.fetchOne();
                if (!product) {
                    throw std::runtime_error(
                        "Product id " + std::to_string(productId) + " does not exist.");
                }
                productName = product[0].get<std::string>();
                unitPrice = product[1].get<double>();   // DECIMAL -> double
                stock = product[2].get<int>();
            }

            // Application-level stock check: gives a clear, friendly error message.
            if (stock < quantity) {
                throw std::runtime_error(
                    "Insufficient stock for '" + productName + "': have " +
                    std::to_string(stock) + ", need " + std::to_string(quantity) + ".");
            }

            // Insert the line item, SNAPSHOTTING unit_price from what we just read.
            // The order records what the customer actually paid, independent of any
            // future price change.
            db_.run(
                "INSERT INTO order_items (order_id, product_id, quantity, unit_price) "
                "VALUES (?, ?, ?, ?)",
                orderId, productId, quantity, unitPrice);

            // Decrement stock. The CHECK (stock_quantity >= 0) constraint is the
            // SECOND safety net: if the application check above were ever wrong (or
            // a concurrent order raced us), this UPDATE would violate the CHECK,
            // throw, and the whole transaction would roll back — the database can
            // never be left with negative stock.
            //
            // (Concurrency note: under real multi-user load we would additionally
            // "SELECT ... FOR UPDATE" to lock the product rows we read, preventing
            // two orders from both passing the stock check on the same units. For a
            // single-user CLI that machinery isn't warranted; the CHECK still keeps
            // the data correct in the worst case.)
            db_.run(
                "UPDATE products SET stock_quantity = stock_quantity - ? WHERE id = ?",
                quantity, productId);

            total += unitPrice * static_cast<double>(quantity);
        }

        // 3. Finalize: write the accumulated total and mark the order confirmed.
        db_.run(
            "UPDATE orders SET total_amount = ?, status = 'confirmed' WHERE id = ?",
            total, orderId);

        // 4. COMMIT: make every change above permanent and visible, all at once.
        db_.commit();
        return orderId;
    } catch (...) {
        // 5. ROLLBACK: any failure undoes the entire transaction. We rethrow so the
        // caller (the menu) can show the message; the database is untouched.
        db_.rollback();
        throw;
    }
}
