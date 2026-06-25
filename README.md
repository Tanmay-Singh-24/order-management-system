# Order Management System (OMS)

A command-line Order Management System written in **C++17**, backed by a **MySQL**
database. It models a small shop: customers, products with stock, and orders made
up of line items. The headline feature is **atomic order placement** — an order
either fully succeeds (items recorded, stock decremented, total computed) or, if
any product is short on stock, the entire operation rolls back and the database is
left exactly as it was.

> Built to demonstrate practical relational-database design and transaction
> handling from a native C++ client.

---

## Tech stack

| Layer        | Choice                                                      |
|--------------|-------------------------------------------------------------|
| Language     | C++17                                                       |
| Database     | MySQL 9.6 (InnoDB engine)                                   |
| DB access    | MySQL Connector/C++ 9.7 — **X DevAPI** (`mysqlx::Session`)  |
| Build        | CMake (find_package + imported targets)                    |
| Platform     | macOS / Homebrew (project-local MySQL instance)            |

## Architecture

The code is split into three layers, each with a single responsibility:

```
+--------------------------+
|  main.cpp                |   Presentation: text menu, input validation, output.
|  (menu loop + I/O)       |   Contains no SQL.
+------------+-------------+
             | calls
             v
+--------------------------+
|  Service classes         |   Data access, one per entity. Every query is
|   - CustomerService      |   parameterized (.bind) — never string-concatenated.
|   - ProductService       |
|   - OrderService         |   OrderService.placeOrder() runs as one transaction.
+------------+-------------+
             | uses
             v
+--------------------------+        X Protocol, TLS, port 33060
|  Database (RAII wrapper)  | <===========================================> MySQL
|  owns mysqlx::Session     |   parameterized SQL / transactions             (InnoDB)
+--------------------------+                                          ordersdb schema
```

**Schema** (3NF; `order_items` is the junction table resolving the many-to-many
between `orders` and `products`):

```
customers ──1:N──> orders ──1:N──> order_items <──N:1── products
   id                id  (FK)         order_id (FK)        id
   name              customer_id      product_id (FK)      name
   email (UNIQUE)    total_amount     quantity             price  DECIMAL(10,2)
   created_at        status (ENUM)    unit_price (snapshot)stock_quantity CHECK>=0
                     created_at
```

## Project layout

```
oms/
├── CMakeLists.txt          # build recipe (connector + keg-only OpenSSL fix + RPATH)
├── README.md
├── db/
│   ├── schema.sql          # tables, keys, constraints (re-runnable)
│   ├── seed.sql            # sample customers + products
│   ├── start-db.sh         # start the project-local MySQL instance
│   └── stop-db.sh          # stop it
└── src/
    ├── main.cpp            # menu loop + I/O
    ├── Database.{h,cpp}    # RAII session wrapper + parameterized run() + txn control
    ├── CustomerService.{h,cpp}
    ├── ProductService.{h,cpp}
    └── OrderService.{h,cpp}   # listOrders() + atomic placeOrder()
```

## Setup & run

Prerequisites (macOS): `brew install cmake mysql mysql-connector-c++`.
The database is a **project-local** MySQL instance (data lives in `db/data/`,
classic port `3307`, X Protocol port `33060`); it is started/stopped manually and
does **not** run as a system service.

```bash
# 1. Start the database
./db/start-db.sh

# 2. Apply schema + seed data (run as root over the local socket).
SOCK="$(pwd)/db/mysql.sock"
/opt/homebrew/opt/mysql/bin/mysql --socket="$SOCK" -u root ordersdb < db/schema.sql
/opt/homebrew/opt/mysql/bin/mysql --socket="$SOCK" -u root ordersdb < db/seed.sql

# 3. Build
cmake -S . -B build
cmake --build build

# 4. Run
./build/oms

# When finished:
./db/stop-db.sh
```

> First-time only: the `ordersdb` database and the least-privilege `orderapp` user
> must exist. They are created once as root:
> ```sql
> CREATE DATABASE IF NOT EXISTS ordersdb CHARACTER SET utf8mb4;
> CREATE USER IF NOT EXISTS 'orderapp'@'%' IDENTIFIED BY 'orderpass';
> GRANT SELECT, INSERT, UPDATE, DELETE ON ordersdb.* TO 'orderapp'@'%';
> ```

### Try the headline feature

1. **List products** (option 4) to see stock.
2. **Place order** (option 5): pick a customer, add items within stock → it's
   confirmed and stock drops.
3. **Place order** again, but request **more than is in stock** → you get
   `Insufficient stock for '…'` and **nothing changes** — re-list products to
   confirm the stock is untouched. That's the rollback working.

## Design decisions

- **`DECIMAL(10,2)` for money, never `FLOAT`.** Binary floating point can't
  represent values like `0.10` exactly, so sums drift by fractions of a cent.
  `DECIMAL` is exact base-10 fixed point — correct for currency.
- **Parameterized queries everywhere (`.bind()`).** User input is sent to the
  server separately from the SQL text, so it can never be parsed as SQL. This
  structurally eliminates SQL injection (vs. building queries by string
  concatenation).
- **Order placement is one transaction.** Inserting the order, inserting each line
  item, and decrementing stock must all happen together. Wrapping them in
  `startTransaction … commit`, with `rollback` on any failure, gives **atomicity**:
  the database is never left with a half-finished order.
- **Stock guarded twice.** The app checks stock before each decrement (for a clear
  message), and a `CHECK (stock_quantity >= 0)` constraint backs it up at the
  database level — even a logic bug or a race can't drive stock negative.
- **`unit_price` is snapshotted** into `order_items` at order time, so past orders
  remain an accurate historical record if a product's price later changes.
- **Least-privilege DB user.** The app connects as `orderapp`, which has only
  `SELECT/INSERT/UPDATE/DELETE` on `ordersdb` — no DDL, no access to other
  databases. Limits the blast radius if the app is ever compromised.
- **RAII connection wrapper.** `Database` opens the session in its constructor and
  lets the destructor close it, so the connection is always released — even when an
  exception unwinds the stack.
```
