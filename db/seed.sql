-- =============================================================================
-- Sample data so the app has something to show immediately.
-- Run AFTER schema.sql (which starts from empty tables).
-- =============================================================================

USE ordersdb;

INSERT INTO customers (name, email) VALUES
    ('Ada Lovelace',   'ada@example.com'),
    ('Alan Turing',    'alan@example.com'),
    ('Grace Hopper',   'grace@example.com');

-- Note the deliberately low stock on a couple of items: handy for demoing the
-- "insufficient stock -> transaction rollback" path in Phase 4.
INSERT INTO products (name, price, stock_quantity) VALUES
    ('USB-C Cable',        9.99,   100),
    ('Mechanical Keyboard', 79.50,  15),
    ('27in Monitor',       249.00,   5),
    ('Webcam 1080p',        45.25,   3),
    ('Laptop Stand',        29.99,   0);   -- out of stock on purpose
