#!/bin/bash

# pip3 install -r requirements.txt
# sudo apt install pandoc

python3 ruuvi_gw_integration_tests_yaml2md.py ruuvi_gw_integration_tests.yaml
pandoc -f markdown -t html5 -o ruuvi_gw_integration_tests.html ruuvi_gw_integration_tests.md
