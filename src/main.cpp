// Order Management System — CLI entry point.
//
// Responsibilities here: drive the text menu, read/validate user input, and print
// results. All database work is delegated to the service classes; this file does
// no SQL itself.

#include <cctype>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "CustomerService.h"
#include "Database.h"
#include "OrderService.h"
#include "ProductService.h"

namespace {

// ---- Input helpers: never crash on bad input; just re-prompt. --------------

// Reads one line. End-of-input (Ctrl-D) cleanly exits the program.
std::string readLine(const std::string& prompt) {
    std::cout << prompt;
    std::string line;
    if (!std::getline(std::cin, line)) {
        std::cout << "\n";
        std::exit(0);
    }
    return line;
}

// Reads a whole number, re-prompting until the entire line is a valid integer.
int readInt(const std::string& prompt) {
    while (true) {
        const std::string line = readLine(prompt);
        try {
            std::size_t pos = 0;
            const int value = std::stoi(line, &pos);
            while (pos < line.size() &&
                   std::isspace(static_cast<unsigned char>(line[pos]))) {
                ++pos;
            }
            if (pos == line.size()) return value;
        } catch (...) {
            // fall through to the error message
        }
        std::cout << "  Please enter a whole number.\n";
    }
}

// Reads a non-negative decimal, re-prompting until valid.
double readMoney(const std::string& prompt) {
    while (true) {
        const std::string line = readLine(prompt);
        try {
            std::size_t pos = 0;
            const double value = std::stod(line, &pos);
            while (pos < line.size() &&
                   std::isspace(static_cast<unsigned char>(line[pos]))) {
                ++pos;
            }
            if (pos == line.size() && value >= 0.0) return value;
        } catch (...) {
        }
        std::cout << "  Please enter a valid non-negative amount.\n";
    }
}

// Formats a money amount with two decimal places.
std::string money(double value) {
    std::ostringstream os;
    os << std::fixed << std::setprecision(2) << value;
    return os.str();
}

// ---- Menu actions ----------------------------------------------------------

void doAddCustomer(CustomerService& customers) {
    const std::string name = readLine("  Name: ");
    const std::string email = readLine("  Email: ");
    const int id = customers.addCustomer(name, email);
    std::cout << "  Added customer #" << id << ".\n";
}

void doListCustomers(CustomerService& customers) {
    const auto list = customers.listCustomers();
    if (list.empty()) {
        std::cout << "  (no customers)\n";
        return;
    }
    std::cout << "  " << std::left << std::setw(4) << "ID" << std::setw(22) << "NAME"
              << "EMAIL\n";
    for (const auto& c : list) {
        std::cout << "  " << std::left << std::setw(4) << c.id << std::setw(22)
                  << c.name << c.email << "\n";
    }
}

void doAddProduct(ProductService& products) {
    const std::string name = readLine("  Name: ");
    const double price = readMoney("  Price: ");
    const int stock = readInt("  Stock quantity: ");
    const int id = products.addProduct(name, price, stock);
    std::cout << "  Added product #" << id << ".\n";
}

void doListProducts(ProductService& products) {
    const auto list = products.listProducts();
    if (list.empty()) {
        std::cout << "  (no products)\n";
        return;
    }
    std::cout << "  " << std::left << std::setw(4) << "ID" << std::setw(24) << "NAME"
              << std::right << std::setw(10) << "PRICE" << std::setw(8) << "STOCK"
              << "\n";
    for (const auto& p : list) {
        std::cout << "  " << std::left << std::setw(4) << p.id << std::setw(24)
                  << p.name << std::right << std::setw(10) << money(p.price)
                  << std::setw(8) << p.stock << "\n";
    }
}

void doPlaceOrder(OrderService& orders) {
    const int customerId = readInt("  Customer id: ");

    std::vector<std::pair<int, int>> items;
    std::cout << "  Add line items (enter product id 0 to finish):\n";
    while (true) {
        const int productId = readInt("    Product id (0 to finish): ");
        if (productId == 0) break;
        const int quantity = readInt("    Quantity: ");
        items.emplace_back(productId, quantity);
    }

    if (items.empty()) {
        std::cout << "  No items entered; order cancelled.\n";
        return;
    }

    // If placeOrder throws (e.g. insufficient stock) it has already rolled back;
    // the menu loop's handler prints the message and the DB is unchanged.
    const int orderId = orders.placeOrder(customerId, items);
    std::cout << "  Order #" << orderId << " placed and confirmed.\n";
}

void doListOrders(OrderService& orders) {
    const auto list = orders.listOrders();
    if (list.empty()) {
        std::cout << "  (no orders)\n";
        return;
    }
    for (const auto& o : list) {
        std::cout << "  Order #" << o.id << "  customer=" << o.customerName
                  << "  status=" << o.status << "  total=" << money(o.totalAmount)
                  << "  (" << o.createdAt << ")\n";
        for (const auto& item : o.items) {
            std::cout << "      - " << item.quantity << " x " << item.productName
                      << " @ " << money(item.unitPrice) << "\n";
        }
    }
}

void printMenu() {
    std::cout << "\n=== Order Management System ===\n"
              << "1. Add customer\n"
              << "2. List customers\n"
              << "3. Add product\n"
              << "4. List products\n"
              << "5. Place order\n"
              << "6. List orders\n"
              << "0. Exit\n";
}

}  // namespace

int main() {
    try {
        // Opening the connection (RAII): if this fails, we report and exit.
        Database db;
        CustomerService customers(db);
        ProductService products(db);
        OrderService orders(db);

        std::cout << "Connected to ordersdb.\n";

        while (true) {
            printMenu();
            const int choice = readInt("Choose an option: ");

            // Each action is wrapped so a single failure (bad email, insufficient
            // stock, etc.) reports cleanly and returns to the menu instead of
            // crashing the whole program.
            try {
                switch (choice) {
                    case 1: doAddCustomer(customers); break;
                    case 2: doListCustomers(customers); break;
                    case 3: doAddProduct(products); break;
                    case 4: doListProducts(products); break;
                    case 5: doPlaceOrder(orders); break;
                    case 6: doListOrders(orders); break;
                    case 0: std::cout << "Goodbye.\n"; return 0;
                    default: std::cout << "  Unknown option.\n"; break;
                }
            } catch (const mysqlx::Error& err) {
                std::cerr << "  Database error: " << err.what() << "\n";
            } catch (const std::exception& ex) {
                std::cerr << "  Error: " << ex.what() << "\n";
            }
        }
    } catch (const mysqlx::Error& err) {
        std::cerr << "Fatal database error: " << err.what() << "\n";
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << "\n";
        return 1;
    }
}
