-- =============================================================================
-- Order Management System — schema
-- =============================================================================
-- Safe to re-run: drops existing tables (in dependency order) and recreates them.
-- Engine is InnoDB throughout: InnoDB is the only MySQL engine that supports
-- FOREIGN KEYs and TRANSACTIONS, both of which this project depends on.
-- =============================================================================

USE ordersdb;

-- Drop in reverse dependency order so foreign keys never block the drop:
-- order_items references orders & products; orders references customers.
DROP TABLE IF EXISTS order_items;
DROP TABLE IF EXISTS orders;
DROP TABLE IF EXISTS products;
DROP TABLE IF EXISTS customers;

-- -----------------------------------------------------------------------------
-- customers
-- -----------------------------------------------------------------------------
CREATE TABLE customers (
    id         INT AUTO_INCREMENT PRIMARY KEY,
    name       VARCHAR(100)  NOT NULL,
    email      VARCHAR(255)  NOT NULL UNIQUE,        -- UNIQUE: no two customers share an email
    created_at TIMESTAMP     NOT NULL DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB;

-- -----------------------------------------------------------------------------
-- products
-- -----------------------------------------------------------------------------
CREATE TABLE products (
    id             INT AUTO_INCREMENT PRIMARY KEY,
    name           VARCHAR(150)   NOT NULL,
    price          DECIMAL(10,2)  NOT NULL,          -- DECIMAL (not FLOAT) for exact money
    stock_quantity INT            NOT NULL DEFAULT 0,
    created_at     TIMESTAMP      NOT NULL DEFAULT CURRENT_TIMESTAMP,
    -- Stock can never go negative. This is the database-level safety net that
    -- backs up the application's stock check during order placement (Phase 4):
    -- even if the app logic were wrong, a decrement below zero is rejected here.
    CONSTRAINT chk_products_stock_nonneg CHECK (stock_quantity >= 0)
) ENGINE=InnoDB;

-- -----------------------------------------------------------------------------
-- orders
-- -----------------------------------------------------------------------------
CREATE TABLE orders (
    id           INT AUTO_INCREMENT PRIMARY KEY,
    customer_id  INT            NOT NULL,
    total_amount DECIMAL(10,2)  NOT NULL DEFAULT 0.00,
    status       ENUM('pending','confirmed','cancelled') NOT NULL DEFAULT 'pending',
    created_at   TIMESTAMP      NOT NULL DEFAULT CURRENT_TIMESTAMP,
    -- ON DELETE RESTRICT: you cannot delete a customer who still has orders.
    -- This protects order history from being orphaned or silently lost.
    CONSTRAINT fk_orders_customer
        FOREIGN KEY (customer_id) REFERENCES customers(id)
        ON DELETE RESTRICT
) ENGINE=InnoDB;

-- -----------------------------------------------------------------------------
-- order_items  (junction table: resolves the many-to-many between orders & products)
-- -----------------------------------------------------------------------------
CREATE TABLE order_items (
    id          INT AUTO_INCREMENT PRIMARY KEY,
    order_id    INT            NOT NULL,
    product_id  INT            NOT NULL,
    quantity    INT            NOT NULL,
    -- unit_price is SNAPSHOTTED at order time. We copy the product's price into
    -- the line item so the order is a historical record: if the product's price
    -- changes later, past orders still show what the customer actually paid.
    unit_price  DECIMAL(10,2)  NOT NULL,
    -- ON DELETE CASCADE: deleting an order removes its line items automatically;
    -- a line item has no meaning without its parent order.
    CONSTRAINT fk_items_order
        FOREIGN KEY (order_id) REFERENCES orders(id)
        ON DELETE CASCADE,
    -- ON DELETE RESTRICT: cannot delete a product that appears in an order,
    -- so historical line items keep pointing at a real product row.
    CONSTRAINT fk_items_product
        FOREIGN KEY (product_id) REFERENCES products(id)
        ON DELETE RESTRICT,
    CONSTRAINT chk_items_qty_positive CHECK (quantity > 0)
) ENGINE=InnoDB;
