#!/bin/bash

# sudo apt install pandoc

pandoc -f markdown -t html5 -o gateway_configuration.html gateway_configuration.md -c gateway_configuration.css

