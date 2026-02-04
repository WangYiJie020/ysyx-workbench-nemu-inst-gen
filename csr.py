import os
import sys
import yaml
import re

db_csr_path = os.path.expanduser("~/gitclones/riscv-unified-db/spec/std/isa/csr/")

def declare_field(name: str,field:str, location: str):
    loc = location.replace('-', ', ')
    print(f"_CSR_FIELD({name.upper()}, {field}, {loc});")

def load_csr_yaml(file_path):
    with open(file_path, 'r') as f:
        data = yaml.safe_load(f)

    name = data['name']

    for field, item in data.get('fields', {}).items():
        loc = item.get('location', item.get('location_rv32', None))
        assert loc is not None, f"Location not found for field {field} in {file_path}"
        declare_field(name,field, str(loc))



load_csr_yaml(os.path.join(db_csr_path, "mstatus.yaml"))
