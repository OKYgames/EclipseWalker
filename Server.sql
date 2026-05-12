CREATE DATABASE IF NOT EXISTS GameDB
CHARACTER SET utf8mb4
COLLATE utf8mb4_unicode_ci;

USE GameDB;

CREATE TABLE IF NOT EXISTS PlayerAccount (
    uid INT AUTO_INCREMENT PRIMARY KEY,
    account_id VARCHAR(50) NOT NULL UNIQUE,
    password VARCHAR(255) NOT NULL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO PlayerAccount (account_id, password)
VALUES
('test1', '1234'),
('test2', '1234'),
('test3', '1234')
ON DUPLICATE KEY UPDATE password = VALUES(password);

SELECT * FROM PlayerAccount;
