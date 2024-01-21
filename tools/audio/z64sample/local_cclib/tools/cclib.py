#!/usr/bin/python3

import os, sys, re

if __name__ == "__main__":
    exit(1)

def path() -> str:
    ccpath = os.path.dirname(os.path.abspath(__file__))
    return ccpath.replace('/tools', '')

def valid_name(name:str):
	return re.match(r'^[a-zA-Z][a-zA-Z0-9]+$', name) is not None

def get_enum_name(name:str) -> str:
    return re.sub(r'(?<!^)(?=[A-Z])', '_', name).upper()

def get_snake_name(name:str) -> str:
    return re.sub(r'(?<!^)(?=[A-Z])', '_', name).lower()
