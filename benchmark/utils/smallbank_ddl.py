#!/usr/bin/python3

import json

ddl = [
"""
CREATE TABLE accounts (
    custid number(12,0) NOT NULL,
    name varchar2(64) NOT NULL,
    CONSTRAINT pk_accounts PRIMARY KEY (custid)
);
""",
"""
CREATE TABLE savings (
    custid number(12,0) NOT NULL,
    bal number(10,2) NOT NULL,
    CONSTRAINT pk_savings PRIMARY KEY (custid)
);
""",
"""
CREATE TABLE checking (
    custid number(12,0) NOT NULL,
    bal number(10,2) NOT NULL,
    CONSTRAINT pk_checking PRIMARY KEY (custid)
);
"""
]

with open('smallbank.json', 'w') as outfile:
    json.dump(ddl, outfile, sort_keys=True, indent=4)
