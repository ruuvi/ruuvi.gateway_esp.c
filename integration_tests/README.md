# Ruuvi Gateway Integration Tests

This directory contains description of integration tests for Ruuvi Gateway.

## Preparation of the environment

1. Install Ruuvi Gateway firmware on the Gateway.
2. Erase the configuration of the Gateway by long pressing on the Configure button.
3. Connect the Gateway to Ethernet or Wi-Fi.
4. Create directory '.test_results'.
5. Copy 'secrets_example.json' to '.test_results/secrets.json'.
6. Edit '.test_results/secrets.json' and set the correct values.
   * Set "gw_hostname" attribute in the '.test_results/secrets.json' file to the IP address or 
      name of the Gateway (for example "ruuvigateway9c2c.local").

## ruuvi_gw_integration_tests.yaml

`ruuvi_gw_integration_tests.yaml` contains the main description of the integration tests.

The script `ruuvi_gw_integration_tests_yaml2md.py` can be used 
to convert the YAML file to a Markdown file.

The script `ruuvi_gw_integration_tests_yaml2md_html.sh` uses the previous script 
and `pandoc` to convert the Markdown file to an HTML file.

To use these scripts you need to install `pyyaml` and  `pandoc`:

```bash
pip3 install pyyaml
sudo apt install pandoc
```

## Automated tests

There are scripts with automated tests in this directory with names `tst-XXX.sh`.

To run these tests you need to install some python packages:

```bash
pip3 install requests pyserial
```

### Running tests locally on Ubuntu or macOS

Each of `tst-XXX.sh` test scripts contains one set of tests and can be run separately.
Temporary files and logs will be saved to '.test_results/TST-**XXX**' directory.
During testing a browser window will be opened with Ruuvi Gateway web interface.

### Running tests in Docker

### Install Docker

To use Docker you need to install it into Ubuntu 22.04 or 24.04.
Connecting USB devices to Docker containers on Windows or macOS is more complex and not covered here.

#### Install Docker Engine on Ubuntu

The following instruction is based on: https://docs.docker.com/engine/install/ubuntu/

##### Prerequisites
* Ubuntu Noble 24.04 (LTS)
* Ubuntu Jammy 22.04 (LTS)

##### Uninstall old versions
```shell
for pkg in docker.io docker-doc docker-compose docker-compose-v2 podman-docker containerd runc; do sudo apt-get remove $pkg; done
```

##### Install using the apt repository
1. Set up Docker's apt repository.
```shell
# Add Docker's official GPG key:
sudo apt-get update
sudo apt-get install ca-certificates curl
sudo install -m 0755 -d /etc/apt/keyrings
sudo curl -fsSL https://download.docker.com/linux/ubuntu/gpg -o /etc/apt/keyrings/docker.asc
sudo chmod a+r /etc/apt/keyrings/docker.asc

# Add the repository to Apt sources:
echo \
  "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.asc] https://download.docker.com/linux/ubuntu \
  $(. /etc/os-release && echo "$VERSION_CODENAME") stable" | \
  sudo tee /etc/apt/sources.list.d/docker.list > /dev/null
sudo apt-get update
```
2. Install the Docker packages.
```shell
sudo apt-get install docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin
```
3. Verify that the Docker Engine installation is successful by running the `hello-world` image.
```shell
sudo docker run hello-world
```

If you want to free space on your computer, you can remove the Docker images,
containers, volumes, and networks all at once:
```shell
docker system prune -a
```

### Building Docker image

```shell
./build_docker_image.sh
```

If `build_docker_image.sh` fails with error "Could not resolve 'archive.ubuntu.com'", 
follow the instructions on [stackoverflow](https://stackoverflow.com/questions/24991136/docker-build-could-not-resolve-archive-ubuntu-com-apt-get-fails-to-install-a)
or [Fix Docker's networking DNS config](https://robinwinslow.uk/fix-docker-networking-dns)

Logs will be saved to '.test_results/build_docker_image.log'.

### Running tests in Docker container

```shell
./run_tests_in_docker.sh
```

Logs will be saved to '.test_results/YYYY-MM-DDTHH-mm-ss' folder and to 
the archive 'test_results/YYYY-MM-DDTHH-mm-ss.zip'.
