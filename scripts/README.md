# A collection of frequently used scripts

## Pre-requisites:

* python 3.10

### Install python 3.10

* Install python 3.10 on Ubuntu 22.04:

  `sudo apt install python3.10 python3.10-venv`

You can install python dependencies globally or use a virtual environment.

### Install python dependencies globally

* Install dependencies:

  ```shell
  pip3 install --upgrade pip
  pip3 install -r requirements.txt
  ```
  or just run the `install_python_requirements.sh` script.

### Setup venv and install dependencies

* Create venv:

  `python3 -m venv .venv`

* Activate venv:

  `source scripts/.venv/bin/activate`

* Upgrade pip:

  `pip3 install --upgrade pip`

* Install dependencies:

  `pip3 install -r requirements.txt`
