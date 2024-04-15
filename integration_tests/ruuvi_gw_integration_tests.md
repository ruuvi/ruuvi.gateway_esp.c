## TST-001: Tests after initial flashing firmware

### TST-001-AA001: Test Ethernet autoconfiguration after initial flashing firmware

#### Setup
- Connect Ethernet cable to the gateway.

#### Test steps
- **Action**: Erase flash and write firmware.

    ```shell
    python3 ruuvi_gw_flash.py v1.15.0 --erase_flash
    ```

    **Checks**:

    - Check that the gateway is immediately connected to Ethernet by LED: 'G'.

### TST-001-AA002: Test Wi-Fi hotspot activation after initial flashing firmware

#### Setup
- Disconnect Ethernet cable.

#### Test steps
- **Action**: Erase flash and write firmware.

    ```shell
    python3 ruuvi_gw_flash.py v1.15.0 --erase_flash
    ```

    **Checks**:

    - Check that Wi-Fi hotspot is activated and WPS is active. LED: 'RRRRRRRRGRGGGGGGGGGG'.
    - Use mobile phone to check that the hotspot 'Configure Ruuvi Gateway XXXX' is visible.

## TST-002: Test nRF52 firmware update

### TST-002-AB001: Test nRF52 firmware update

#### Setup
- Connect Ethernet cable to the gateway.
- Erase flash and write firmware v1.9.2.
    ```shell
    python3 ruuvi_gw_flash.py v1.9.2 --erase_flash
    ```


#### Test steps
- **Action**: Wait about 5 minutes until the flashing of v1.9.2 is completed.

    **Checks**:

    - LED: 'G'.

- **Action**: Write firmware v1.15.0.

    ```shell
    python3 ruuvi_gw_flash.py v1.15.0
    ```

    **Checks**:

    - Check that nRF52 flashing is started, LED: 'R---------'.
    - Wait about 5 minutes until the flashing is completed, LED: 'G'.

## TST-003: Tests after erasing configuration

### TST-003-AC001: Test configuration erase

#### Setup
- Connect Ethernet cable to the gateway.

#### Test steps
- **Action**: Press CONFIGURE button for 10 seconds.

    **Checks**:

    - LED: 'RR--RR--'.

- **Action**: Release CONFIGURE button and wait for 10 seconds.

    **Checks**:

    - Check that Wi-Fi hotspot is activated and WPS is active. LED: 'RRRRRRRRGRGGGGGGGGGG'.
    - Use mobile phone to check that the hotspot 'Configure Ruuvi Gateway XXXX' is visible.

### TST-003-AC002: Test Wi-Fi hotspot is active for 1 minute after erasing configuration when Ethernet is connected

#### Setup
- Connect Ethernet cable to the gateway.

#### Test steps
- **Action**: Erase configuration

    Press CONFIGURE button for 10 seconds, wait until LED 'RR--RR--', then release the button.

- **Action**: Wait 45 seconds after reboot.

    **Checks**:

    - LED: 'RRRRRRRRGRGGGGGGGGGG'.
    - Use mobile phone to check that the hotspot 'Configure Ruuvi Gateway XXXX' is visible.

- **Action**: Wait 1 minute and 15 seconds after reboot.

    **Checks**:

    - Check that the gateway is connected to the Ethernet, LED: 'G'.
    - Use mobile phone to check that the hotspot 'Configure Ruuvi Gateway XXXX' is not visible.

### TST-003-AC003: Test immediate Ethernet autoconfiguration after erasing configuration and rebooting

#### Setup
- Connect Ethernet cable to the gateway.

#### Test steps
- **Action**: Erase configuration

    Press CONFIGURE button for 10 seconds, wait until LED 'RR--RR--', then release the button.

- **Action**: Power off and on the gateway, wait 20 seconds.

    **Checks**:

    - Check that the gateway is connected to the Ethernet, LED: 'G'.

### TST-003-AC004: Test Wi-Fi WPS and check that WPS is active more than 3 minutes after erasing configuration

#### Setup
- Disconnect Ethernet cable.

#### Test steps
- **Action**: Erase configuration

    Press CONFIGURE button for 10 seconds, wait until LED 'RR--RR--', then release the button.

    **Checks**:

    - LED: 'RRRRRRRRGRGGGGGGGGGG'.

- **Action**: Wait 3 minutes

    **Checks**:

    - Check that Wi-Fi hotspot is active. LED: 'RRRRRRRRGRGGGGGGGGGG'.

- **Action**: Press WPS button on your router.

    **Checks**:

    - Check that the gateway is connected to the Wi-Fi, LED: 'G'.

### TST-003-AC005: Test Wi-Fi hotspot deactivation after 1 hour

#### Setup
- Disconnect Ethernet cable.

#### Test steps
- **Action**: Erase configuration

    Press CONFIGURE button for 10 seconds, wait until LED 'RR--RR--', then release the button.

    **Checks**:

    - LED: 'RRRRRRRRGRGGGGGGGGGG'.

- **Action**: Wait 1 hour

    **Checks**:

    - Check that Wi-Fi hotspot is deactivated. LED: 'R-R-R-R-R-'

## TST-004: Gateway configuration tests with Wi-Fi hotspot

### TST-004-AD001: Test connecting to the configuration hotspot via Wi-Fi and check automatic hotspot deactivation

#### Setup
- Connect Ethernet cable to the gateway.

#### Test steps
- **Action**: Erase configuration

    Press CONFIGURE button for 10 seconds, release the button, wait 10 seconds.

    **Checks**:

    - LED: 'RRRRRRRRGRGGGGGGGGGG'

- **Action**: Use your mobile phone to connect to the hotspot 'Configure Ruuvi Gateway XXXX'.

    If the configuration hotspot is not opened automatically, the open a browser and go to http://10.10.0.1/

    **Checks**:

    - LED: 'RRRRRRRRRRGGGGGGGGGG'

- **Action**: Wait 1 minute

    **Checks**:

    - Check that Wi-Fi hotspot is active and the browser is still open.
    - LED: 'RRRRRRRRRRGGGGGGGGGG'

- **Action**: Lock the phone and wait 1 minute

    **Checks**:

    - LED: 'R-R-R-R-R-' (no network connection)

- **Action**: Wait 15 seconds

    **Checks**:

    - LED: 'G' (automatically connected to Ethernet)

### TST-004-AD002: Test configuring the gateway using Wi-Fi hotspot to connect the gateway to Ethernet in DHCP mode

#### Setup
- Connect Ethernet cable to the gateway.

#### Test steps
- **Action**: Erase configuration

    Press CONFIGURE button for 10 seconds, release the button, wait 10 seconds.

    **Checks**:

    - LED: 'RRRRRRRRGRGGGGGGGGGG'

- **Action**: Use your mobile phone to connect to the hotspot 'Configure Ruuvi Gateway XXXX'.

    If the configuration hotspot is not opened automatically, the open a browser and go to http://10.10.0.1/

    **Checks**:

    - LED: 'RRRRRRRRRRGGGGGGGGGG'

- **Action**: Configure connection to Ethernet in DHCP mode.

    **Steps**:

    - Page **Setup Wizard**: Click **Let's get started**.
    - Page **Internet Connection**: Select **Ethernet**, click **Next**.
    - Page **IP address settings**: Select **Let your DHCP server assign an IP address automatically (default)**, click **Next**.
    - Wait until the popup message **Please connect Ethernet cable now if not already connected** is disappeared.

    **Checks**:

    - Check that the Ethernet connection has been successfully established.

- **Action**: Continue the configuration process.

    **Steps**:

    - Page **Software Update**: Click **Continue and skip update** or **Next**.
    - Page **Automatic Configuration Download**: Click **Next**.
    - Page **Automatic Updated**: Click **Next**.
    - Page **Remote Access Settings**: Click **Next**.
    - Page **Cloud Options**: Click **Next**.
    - Page **Finish**: Remember **Ruuvi Gateway's IP address** (it will be used on the next test)

    **Checks**:

    - Page **Finished!** must be opened.
    - Message **Ruuvi Gateway is now functioning** must be displayed.
    - Message: **Connected to**: Network: Ethernet
    - LED: 'G'

- **Action**: Check network connection with the gateway IP address.

    ```shell
    ping <gateway IP address>
    ```

    **Checks**:

    - Check that the gateway is answering to ping requests.

- **Action**: Check network connection with the gateway's hostname.

    ```shell
    ping ruuvigateway<XXXX>.local
    ```

    **Checks**:

    - Check that the gateway is answering to ping requests.

- **Action**: Use a browser to open the gateway's hostname ruuvigateway<XXXX>.local.

    **Checks**:

    - Check that the gateway's configuration wizard is opened in the browser.

### TST-004-AD003: Test failed attempt to configure the gateway to use Ethernet in DHCP mode

#### Setup
- Disconnect Ethernet cable.

#### Test steps
- **Action**: Erase configuration

    Press CONFIGURE button for 10 seconds, release the button, wait 10 seconds.

    **Checks**:

    - LED: 'RRRRRRRRGRGGGGGGGGGG'

- **Action**: Use your mobile phone to connect to the hotspot 'Configure Ruuvi Gateway XXXX'.

    If the configuration hotspot is not opened automatically, the open a browser and go to http://10.10.0.1/

    **Checks**:

    - LED: 'RRRRRRRRRRGGGGGGGGGG'

- **Action**: Configure connection to Ethernet in DHCP mode.

    **Steps**:

    - Page **Setup Wizard**: Click **Let's get started**.
    - Page **Internet Connection**: Select **Ethernet**, click **Next**.
    - Page **IP address settings**: Select **Let your DHCP server assign an IP address automatically (default)**, click **Next**.
    - Wait until the popup message **Please connect Ethernet cable now if not already connected** is disappeared.

    **Checks**:

    - Error message should be displayed '**Ethernet cable is not connected or there is some other problem with the connection**'.

### TST-004-AD004: Test configuring the gateway using Wi-Fi hotspot to connect the gateway to Ethernet in manual mode

#### Setup
- Connect Ethernet cable to the gateway.

#### Test steps
- **Action**: Erase configuration

    Press CONFIGURE button for 10 seconds, release the button, wait 10 seconds.

    **Checks**:

    - LED: 'RRRRRRRRGRGGGGGGGGGG'

- **Action**: Use your mobile phone to connect to the hotspot 'Configure Ruuvi Gateway XXXX'.

    If the configuration hotspot is not opened automatically, the open a browser and go to http://10.10.0.1/

    **Checks**:

    - LED: 'RRRRRRRRRRGGGGGGGGGG'

- **Action**: Configure connection to Ethernet in DHCP mode.

    **Steps**:

    - Page **Setup Wizard**: Click **Let's get started**.
    - Page **Internet Connection**: Select **Ethernet**, click **Next**.
    - Page **IP address settings**: Unselect **Let your DHCP server assign an IP address automatically (default)**.
    - Page **IP address settings**: **Static IP address**: Enter any available IP address from your local network.
    - Page **IP address settings**: **Subnet mask**: 255.255.255.0.
    - Page **IP address settings**: **Default Gateway**: Enter IP address of your router.
    - Page **IP address settings**: **Primary DNS IP address**: Enter IP address of your router.
    - Click **Next**
    - Wait until the popup message **Please connect Ethernet cable now if not already connected** is disappeared.

    **Checks**:

    - Check that the Ethernet connection has been successfully established.

- **Action**: Continue the configuration process.

    **Steps**:

    - Page **Software Update**: Click **Continue and skip update** or **Next**.
    - Page **Automatic Configuration Download**: Click **Next**.
    - Page **Automatic Updated**: Click **Next**.
    - Page **Remote Access Settings**: Click **Next**.
    - Page **Cloud Options**: Click **Next**.
    - Page **Finish**: Remember **Ruuvi Gateway's IP address** (it will be used on the next test)

    **Checks**:

    - Page **Finished!** must be opened.
    - Message **Ruuvi Gateway is now functioning** must be displayed.
    - Message: **Connected to**: Network: Ethernet
    - LED: 'G'

- **Action**: Check network connection with the gateway IP address.

    ```shell
    ping <gateway IP address>
    ```

    **Checks**:

    - Check that the gateway is answering to ping requests.

- **Action**: Check network connection with the gateway's hostname.

    ```shell
    ping ruuvigateway<XXXX>.local
    ```

    **Checks**:

    - Check that the gateway is answering to ping requests.

- **Action**: Use a browser to open the gateway's hostname ruuvigateway<XXXX>.local.

    **Checks**:

    - Check that the gateway's configuration wizard is opened in the browser.

### TST-004-AD005: Test configuring the gateway using Wi-Fi hotspot to connect the gateway to Wi-Fi using WPS

#### Setup
- Disconnect Ethernet cable.

#### Test steps
- **Action**: Erase configuration

    Press CONFIGURE button for 10 seconds, release the button, wait 10 seconds.

    **Checks**:

    - LED: 'RRRRRRRRGRGGGGGGGGGG'

- **Action**: Use your mobile phone to connect to the hotspot 'Configure Ruuvi Gateway XXXX'.

    If the configuration hotspot is not opened automatically, the open a browser and go to http://10.10.0.1/

    **Checks**:

    - LED: 'RRRRRRRRRRGGGGGGGGGG'

- **Action**: Configure connection to Wi-Fi using WPS.

    **Steps**:

    - Page **Setup Wizard**: Click **Let's get started**.
    - Page **Internet Connection**: Select **Wi-Fi**, click **Next**.
    - Page **Wi-Fi Network**: Enable checkbox **Use WPS to connect to Wi-Fi network**.
    - Click **Next**

    **Checks**:

    - Popup message should be displayed '**Please start WPS (Wi-Fi Protected Setup) on your router by pressing on the WPS button**'.
    - LED: 'RRRRRRRRGRGGGGGGGGGG'

- **Action**: Press WPS button on your router and wait about 15 seconds.

    **Checks**:

    - Check that the popup message is disappeared
    - Check that the Wi-Fi connection has been successfully established.

- **Action**: Continue the configuration process.

    **Steps**:

    - Page **Software Update**: Click **Continue and skip update** or **Next**.
    - Page **Automatic Configuration Download**: Click **Next**.
    - Page **Automatic Updated**: Click **Next**.
    - Page **Remote Access Settings**: Click **Next**.
    - Page **Cloud Options**: Click **Next**.

    **Checks**:

    - Page **Finished!** must be opened.
    - Message **Ruuvi Gateway is now functioning** must be displayed.
    - Message: **Connected to**: Network: Wi-Fi SSID
    - LED: 'G'

- **Action**: Check network connection with the gateway IP address.

    ```shell
    ping <gateway IP address>
    ```

    **Checks**:

    - Check that the gateway is answering to ping requests.

- **Action**: Check network connection with the gateway's hostname.

    ```shell
    ping ruuvigateway<XXXX>.local
    ```

    **Checks**:

    - Check that the gateway is answering to ping requests.

- **Action**: Use a browser to open the gateway's hostname ruuvigateway<XXXX>.local.

    **Checks**:

    - Check that the gateway's configuration wizard is opened in the browser.

### TST-004-AD006: Test configuring the gateway using Wi-Fi hotspot to connect the gateway to Wi-Fi using WPS - timeout

#### Setup
- Disconnect Ethernet cable.

#### Test steps
- **Action**: Erase configuration

    Press CONFIGURE button for 10 seconds, release the button, wait 10 seconds.

    **Checks**:

    - LED: 'RRRRRRRRGRGGGGGGGGGG'

- **Action**: Use your mobile phone to connect to the hotspot 'Configure Ruuvi Gateway XXXX'.

    If the configuration hotspot is not opened automatically, the open a browser and go to http://10.10.0.1/

    **Checks**:

    - LED: 'RRRRRRRRRRGGGGGGGGGG'

- **Action**: Configure connection to Wi-Fi using WPS.

    **Steps**:

    - Page **Setup Wizard**: Click **Let's get started**.
    - Page **Internet Connection**: Select **Wi-Fi**, click **Next**.
    - Page **Wi-Fi Network**: Enable checkbox **Use WPS to connect to Wi-Fi network**.
    - Click **Next**

    **Checks**:

    - Popup message should be displayed '**Please start WPS (Wi-Fi Protected Setup) on your router by pressing on the WPS button**'.
    - LED: 'RRRRRRRRGRGGGGGGGGGG'

- **Action**: Wait 1 minute.

    **Checks**:

    - Popup message should continue to be displayed '**Please start WPS (Wi-Fi Protected Setup) on your router by pressing on the WPS button**'.
    - LED: 'RRRRRRRRGRGGGGGGGGGG'

- **Action**: Click on the **Cancel** button.

    **Checks**:

    - Page **Wi-Fi Network** must be opened.

### TST-004-AD007: Test configuring the gateway using Wi-Fi hotspot to connect the gateway to Wi-Fi by choosing from the list of available networks

#### Setup
- Disconnect Ethernet cable.

#### Test steps
- **Action**: Erase configuration

    Press CONFIGURE button for 10 seconds, release the button, wait 10 seconds.

    **Checks**:

    - LED: 'RRRRRRRRGRGGGGGGGGGG'

- **Action**: Use your mobile phone to connect to the hotspot 'Configure Ruuvi Gateway XXXX'.

    If the configuration hotspot is not opened automatically, the open a browser and go to http://10.10.0.1/

    **Checks**:

    - LED: 'RRRRRRRRRRGGGGGGGGGG'

- **Action**: Configure connection to Wi-Fi from the list of available networks.

    **Steps**:

    - Page **Setup Wizard**: Click **Let's get started**.
    - Page **Internet Connection**: Select **Wi-Fi**, click **Next**.
    - Page **Wi-Fi Network**: Select Wi-Fi SSID from the list of available networks, enter password.
    - Click **Next**
    - Wait about 15 seconds until the Wi-Fi connection is established.

    **Checks**:

    - LED: 'RRRRRRRRRRGGGGGGGGGG'
    - Check that the Wi-Fi connection has been successfully established.

- **Action**: Continue the configuration process.

    **Steps**:

    - Page **Software Update**: Click **Continue and skip update** or **Next**.
    - Page **Automatic Configuration Download**: Click **Next**.
    - Page **Automatic Updated**: Click **Next**.
    - Page **Remote Access Settings**: Click **Next**.
    - Page **Cloud Options**: Click **Next**.

    **Checks**:

    - Page **Finished!** must be opened.
    - Message **Ruuvi Gateway is now functioning** must be displayed.
    - Message: **Connected to**: Network: Wi-Fi SSID
    - LED: 'G'

- **Action**: Check network connection with the gateway IP address.

    ```shell
    ping <gateway IP address>
    ```

    **Checks**:

    - Check that the gateway is answering to ping requests.

- **Action**: Check network connection with the gateway's hostname.

    ```shell
    ping ruuvigateway<XXXX>.local
    ```

    **Checks**:

    - Check that the gateway is answering to ping requests.

- **Action**: Use a browser to open the gateway's hostname ruuvigateway<XXXX>.local.

    **Checks**:

    - Check that the gateway's configuration wizard is opened in the browser.

### TST-004-AD008: Test configuring the gateway using Wi-Fi hotspot to connect the gateway to Wi-Fi by choosing from the list of available networks - wrong password

#### Setup
- Disconnect Ethernet cable.

#### Test steps
- **Action**: Erase configuration

    Press CONFIGURE button for 10 seconds, release the button, wait 10 seconds.

    **Checks**:

    - LED: 'RRRRRRRRGRGGGGGGGGGG'

- **Action**: Use your mobile phone to connect to the hotspot 'Configure Ruuvi Gateway XXXX'.

    If the configuration hotspot is not opened automatically, the open a browser and go to http://10.10.0.1/

    **Checks**:

    - LED: 'RRRRRRRRRRGGGGGGGGGG'

- **Action**: Configure connection to Wi-Fi from the list of available networks and enter wrong password.

    **Steps**:

    - Page **Setup Wizard**: Click **Let's get started**.
    - Page **Internet Connection**: Select **Wi-Fi**, click **Next**.
    - Page **Wi-Fi Network**: Select Wi-Fi SSID from the list of available networks.
    - Page **Wi-Fi Network**: Enter wrong password.
    - Click **Next**

    **Checks**:

    - LED: 'RRRRRRRRRRGGGGGGGGGG'
    - Check that the error message is displayed: '**Failed to connect. Please try again.**'.

- **Action**: Enter correct password.

    **Steps**:

    - Page **Wi-Fi Network**: Enter correct password.
    - Click **Next**
    - Wait about 15 seconds until the Wi-Fi connection is established.

    **Checks**:

    - LED: 'RRRRRRRRRRGGGGGGGGGG'
    - Check that the Wi-Fi connection has been successfully established.

- **Action**: Continue the configuration process.

    **Steps**:

    - Page **Software Update**: Click **Continue and skip update** or **Next**.
    - Page **Automatic Configuration Download**: Click **Next**.
    - Page **Automatic Updated**: Click **Next**.
    - Page **Remote Access Settings**: Click **Next**.
    - Page **Cloud Options**: Click **Next**.

    **Checks**:

    - Page **Finished!** must be opened.
    - Message **Ruuvi Gateway is now functioning** must be displayed.
    - Message: **Connected to**: Network: Wi-Fi SSID
    - LED: 'G'

- **Action**: Check network connection with the gateway IP address.

    ```shell
    ping <gateway IP address>
    ```

    **Checks**:

    - Check that the gateway is answering to ping requests.

- **Action**: Check network connection with the gateway's hostname.

    ```shell
    ping ruuvigateway<XXXX>.local
    ```

    **Checks**:

    - Check that the gateway is answering to ping requests.

- **Action**: Use a browser to open the gateway's hostname ruuvigateway<XXXX>.local.

    **Checks**:

    - Check that the gateway's configuration wizard is opened in the browser.

### TST-004-AD009: Test configuring the gateway using Wi-Fi hotspot to connect the gateway to Wi-Fi by manually entering SSID and password

#### Setup
- Disconnect Ethernet cable.

#### Test steps
- **Action**: Erase configuration

    Press CONFIGURE button for 10 seconds, release the button, wait 10 seconds.

    **Checks**:

    - LED: 'RRRRRRRRGRGGGGGGGGGG'

- **Action**: Use your mobile phone to connect to the hotspot 'Configure Ruuvi Gateway XXXX'.

    If the configuration hotspot is not opened automatically, the open a browser and go to http://10.10.0.1/

    **Checks**:

    - LED: 'RRRRRRRRRRGGGGGGGGGG'

- **Action**: Configure connection to Wi-Fi from the list of available networks.

    **Steps**:

    - Page **Setup Wizard**: Click **Let's get started**.
    - Page **Internet Connection**: Select **Wi-Fi**, click **Next**.
    - Page **Wi-Fi Network**: Open '**Advanced settings**'.
    - Page **Wi-Fi Network**: Enable checkbox '**Connect manually**'.
    - Page **Wi-Fi Network**: Enter '**SSID (network name)**' and '**Password**'.
    - Click **Next**
    - Wait about 15 seconds until the Wi-Fi connection is established.

    **Checks**:

    - LED: 'RRRRRRRRRRGGGGGGGGGG'
    - Check that the Wi-Fi connection has been successfully established.

- **Action**: Continue the configuration process.

    **Steps**:

    - Page **Software Update**: Click **Continue and skip update** or **Next**.
    - Page **Automatic Configuration Download**: Click **Next**.
    - Page **Automatic Updated**: Click **Next**.
    - Page **Remote Access Settings**: Click **Next**.
    - Page **Cloud Options**: Click **Next**.

    **Checks**:

    - Page **Finished!** must be opened.
    - Message **Ruuvi Gateway is now functioning** must be displayed.
    - Message: **Connected to**: Network: Wi-Fi SSID
    - LED: 'G'

- **Action**: Check network connection with the gateway IP address.

    ```shell
    ping <gateway IP address>
    ```

    **Checks**:

    - Check that the gateway is answering to ping requests.

- **Action**: Check network connection with the gateway's hostname.

    ```shell
    ping ruuvigateway<XXXX>.local
    ```

    **Checks**:

    - Check that the gateway is answering to ping requests.

- **Action**: Use a browser to open the gateway's hostname ruuvigateway<XXXX>.local.

    **Checks**:

    - Check that the gateway's configuration wizard is opened in the browser.

### TST-004-AD010: Test activating Wi-Fi hotspot by the CONFIGURE button

#### Setup
- Connect Ethernet cable to the gateway.

#### Test steps
- **Action**: Erase configuration

    Press CONFIGURE button for 10 seconds, release the button, wait 10 seconds.

    **Checks**:

    - LED: 'RRRRRRRRGRGGGGGGGGGG'

- **Action**: Wait 1 minute or power off/on the gateway to automatically configure the gateway to use Ethernet.

    **Checks**:

    - LED: 'G'

- **Action**: Press CONFIGURE button.

    **Checks**:

    - LED: 'RRRRRRRRRRGGGGGGGGGG'
    - Use your mobile phone to check that the hotspot 'Configure Ruuvi Gateway XXXX' is visible.

- **Action**: Wait 1 minute.

    **Checks**:

    - LED: 'G'

- **Action**: Check network connection with the gateway's hostname.

    ```shell
    ping ruuvigateway<XXXX>.local
    ```

    **Checks**:

    - Check that the gateway is answering to ping requests.

- **Action**: Use a browser to open the gateway's hostname ruuvigateway<XXXX>.local.

    **Checks**:

    - Check that the gateway's configuration wizard is opened in the browser.

## TST-005: Remote access settings tests

### TST-005-AE001: Test default access settings

#### Setup
- Connect Ethernet cable to the gateway.

#### Test steps
- **Action**: Erase configuration

    Press CONFIGURE button for 10 seconds, release the button, wait 10 seconds.

    **Checks**:

    - LED: 'RRRRRRRRGRGGGGGGGGGG'

- **Action**: Turn off/on the gateway to activate autoconfiguration using Ethernet.

    **Checks**:

    - LED: 'G'

- **Action**: Use a browser to open the gateway's hostname ruuvigateway<XXXX>.local.

    **Checks**:

    - Check that the '**Login to Settings Page**' page is opened in the browser.

- **Action**: Use wrong password to log in.

    **Checks**:

    - Check that the error message '**Wrong username or password.**' is displayed.

- **Action**: Use default credentials to log in (use Device ID (XX:XX:XX:XX:XX:XX:XX:XX) as the password).

    **Checks**:

    - Check that the '**Setup Wizard**' page is opened in the browser.

- **Action**: Power off/on the gateway, refresh the browser.

    **Checks**:

    - Check that the '**Login to Settings Page**' page is opened in the browser.

- **Action**: Use default credentials to log in (use Device ID (XX:XX:XX:XX:XX:XX:XX:XX) as the password).

    **Checks**:

    - Check that the '**Setup Wizard**' page is opened in the browser.

- **Action**: Check access to "/history" with curl

    ```shell
    curl -v http://ruuvigateway<XXXX>.local/history
    ```

    **Checks**:

    - Check that the response is 'HTTP/1.0 302 Found'.

- **Action**: Try HTTP GET "/ruuvi.json" with curl

    ```shell
    curl -v http://ruuvigateway<XXXX>.local/ruuvi.json
    ```

    **Checks**:

    - Check that the response is 'HTTP/1.0 302 Found'.

- **Action**: Try HTTP POST "/ruuvi.json" with curl

    ```shell
    curl -v -X POST http://ruuvigateway<XXXX>.local/ruuvi.json -d '{}'
    ```

    **Checks**:

    - Check that the response is 'HTTP/1.0 401 Unauthorized'.

### TST-005-AE002: Test access with a custom username/password

#### Setup
- Continue previous test.

#### Test steps
- **Action**: Configure **Remote Access Settings**

    **Steps**:

    - Page **Setup Wizard**: Click **Let's get started**.
    - Page **Internet Connection**: click **Skip**.
    - Page **Software Update**: Click **Continue and skip update** or **Next**.
    - Page **Automatic Configuration Download**: Click **Next**.
    - Page **Automatic Updated**: Click **Next**.
    - Page **Remote Access Settings**: Click **Advanced Settings**.
    - Page **Remote Access Settings**: Select **Protected with a custom password (safe with a strong password)**.
    - Page **Remote Access Settings**: Enter **Username**: user123
    - Page **Remote Access Settings**: Enter **Password**: pass1234
    - Page **Remote Access Settings**: Click **Next**.
    - Page **Cloud Options**: Click **Next**.

    **Checks**:

    - Page **Finished!** must be opened.
    - LED: 'G'

- **Action**: Refresh browser page with Configuration Wizard.

    **Checks**:

    - Check that the '**Login to Settings Page**' page is opened in the browser.
    - Check that the '**Username**' and '**Password**' fields are displayed.

- **Action**: Enter username/password.

    **Steps**:

    - Enter **Username**: user123
    - Enter **Password**: pass1234
    - Click **Login**.

    **Checks**:

    - Check that the '**Setup Wizard**' page is opened in the browser.

- **Action**: Check access to "/history" with curl

    ```shell
    curl -v http://ruuvigateway<XXXX>.local/history
    ```

    **Checks**:

    - Check that the response is 'HTTP/1.0 302 Found'.

- **Action**: Try HTTP GET "/ruuvi.json" with curl

    ```shell
    curl -v http://ruuvigateway<XXXX>.local/ruuvi.json
    ```

    **Checks**:

    - Check that the response is 'HTTP/1.0 302 Found'.

- **Action**: Try HTTP POST "/ruuvi.json" with curl

    ```shell
    curl -v -X POST http://ruuvigateway<XXXX>.local/ruuvi.json -d '{}'
    ```

    **Checks**:

    - Check that the response is 'HTTP/1.0 401 Unauthorized'.

### TST-005-AE003: Test access without a password

#### Setup
- Continue previous test.

#### Test steps
- **Action**: Configure **Remote Access Settings**

    **Steps**:

    - Page **Setup Wizard**: Click **Let's get started**.
    - Page **Internet Connection**: click **Skip**.
    - Page **Software Update**: Click **Continue and skip update** or **Next**.
    - Page **Automatic Configuration Download**: Click **Next**.
    - Page **Automatic Updated**: Click **Next**.
    - Page **Remote Access Settings**: Click **Advanced Settings**.
    - Page **Remote Access Settings**: Select **Remote configurable without a password (unsafe, not recommended)**.
    - Page **Remote Access Settings**: Click **Next**.
    - Page **Cloud Options**: Click **Next**.

    **Checks**:

    - Page **Finished!** must be opened.
    - LED: 'G'

- **Action**: Refresh browser page with Configuration Wizard.

    **Checks**:

    - Check that the '**Setup Wizard**' page is opened in the browser.

- **Action**: Check access to "/history" with curl

    ```shell
    curl -v http://ruuvigateway<XXXX>.local/history
    ```

    **Checks**:

    - Check that the response is 'HTTP/1.0 200 OK'.

- **Action**: Try HTTP GET "/ruuvi.json" with curl

    ```shell
    curl -v http://ruuvigateway<XXXX>.local/ruuvi.json
    ```

    **Checks**:

    - Check that the response is 'HTTP/1.0 200 OK'.

- **Action**: Try HTTP POST "/ruuvi.json" with curl

    ```shell
    curl -v -X POST http://ruuvigateway<XXXX>.local/ruuvi.json -d '{}'
    ```

    **Checks**:

    - Check that the response is 'HTTP/1.0 200 OK'.

### TST-005-AE004: Test disabled remote access

#### Setup
- Continue previous test.

#### Test steps
- **Action**: Configure **Remote Access Settings**

    **Steps**:

    - Page **Setup Wizard**: Click **Let's get started**.
    - Page **Internet Connection**: click **Skip**.
    - Page **Software Update**: Click **Continue and skip update** or **Next**.
    - Page **Automatic Configuration Download**: Click **Next**.
    - Page **Automatic Updated**: Click **Next**.
    - Page **Remote Access Settings**: Click **Advanced Settings**.
    - Page **Remote Access Settings**: Select **Not configurable via remote connection (extremely safe)**.
    - Page **Remote Access Settings**: Click **Next**.
    - Page **Cloud Options**: Click **Next**.

    **Checks**:

    - Page **Finished!** must be opened.
    - LED: 'G'

- **Action**: Refresh browser page with Configuration Wizard.

    **Checks**:

    - Check that the '**Login to Settings Page**' page is opened in the browser.
    - Check that no '**Username**' or '**Password**' fields are displayed.

- **Action**: Check access to "/history" with curl

    ```shell
    curl -v http://ruuvigateway<XXXX>.local/history
    ```

    **Checks**:

    - Check that the response is 'HTTP/1.0 403 Forbidden'.

- **Action**: Try HTTP GET "/ruuvi.json" with curl

    ```shell
    curl -v http://ruuvigateway<XXXX>.local/ruuvi.json
    ```

    **Checks**:

    - Check that the response is 'HTTP/1.0 403 Forbidden'.

- **Action**: Try HTTP POST "/ruuvi.json" with curl

    ```shell
    curl -v -X POST http://ruuvigateway<XXXX>.local/ruuvi.json -d '{}'
    ```

    **Checks**:

    - Check that the response is 'HTTP/1.0 403 Forbidden'.

### TST-005-AE005: Test disabled remote access

#### Setup
- Continue previous test.

#### Test steps
- **Action**: Enable Wi-Fi hotspot by pressing on the CONFIGURE button.

    **Checks**:

    - LED: 'RRRRRRRRRRGGGGGGGGGG'

- **Action**: Use your mobile phone to connect to the hotspot 'Configure Ruuvi Gateway XXXX'.

    If the configuration hotspot is not opened automatically, the open a browser and go to http://10.10.0.1

    **Checks**:

    - LED: 'RRRRRRRRRRGGGGGGGGGG'

- **Action**: Configure default **Remote Access Settings**

    **Steps**:

    - Page **Setup Wizard**: Click **Let's get started**.
    - Page **Internet Connection**: click **Next**.
    - Page **IP address settings**: click **Next**.
    - Page **Software Update**: Click **Continue and skip update** or **Next**.
    - Page **Automatic Configuration Download**: Click **Next**.
    - Page **Automatic Updated**: Click **Next**.
    - Page **Remote Access Settings**: Select **Password protected with the default password (default, safe)**.
    - Page **Remote Access Settings**: Click **Next**.
    - Page **Cloud Options**: Click **Next**.

    **Checks**:

    - Page **Finished!** must be opened.
    - LED: 'G'

- **Action**: Use a browser to open the gateway's hostname ruuvigateway<XXXX>.local.

    **Checks**:

    - Check that the '**Login to Settings Page**' page is opened in the browser.

- **Action**: Use default credentials to log in (use Device ID (XX:XX:XX:XX:XX:XX:XX:XX) as the password).

    **Checks**:

    - Check that the '**Setup Wizard**' page is opened in the browser.

- **Action**: Configure default **Remote Access Settings** with bearer tokens

    **Steps**:

    - Page **Setup Wizard**: Click **Let's get started**.
    - Page **Internet Connection**: click **Skip**.
    - Page **Software Update**: Click **Continue and skip update** or **Next**.
    - Page **Automatic Configuration Download**: Click **Next**.
    - Page **Automatic Updated**: Click **Next**.
    - Page **Remote Access Settings**: Select **Password protected with the default password (default, safe)**.
    - Page **Remote Access Settings**: Click **Advanced settings**.
    - Page **Remote Access Settings**: Enable checkbox **Enable read-only access to '/history' using API key**.
    - Page **Remote Access Settings**: Enter **API key (bearer token)**: Uj+4tj24unVekco/lTLTRyxUfv1J8M6U+sbNsKTWRr0=
    - Page **Remote Access Settings**: Enable checkbox **Enable full (read/write) access to the Ruuvi Gateway router using API key**.
    - Page **Remote Access Settings**: Enter **API key (bearer token)**: 1SDrQH1FkH+pON0GsSjt2gYeMSP02uYqfuu7LWdaBvY=
    - Page **Remote Access Settings**: Click **Next**.
    - Page **Cloud Options**: Click **Next**.

    **Checks**:

    - Page **Finished!** must be opened.
    - LED: 'G'

- **Action**: Check access to "/history" with curl without API key

    ```shell
    curl -v http://ruuvigateway<XXXX>.local/history
    ```

    **Checks**:

    - Check that the response is 'HTTP/1.0 302 Found'.

- **Action**: Check access to "/history" with curl with the read-only API key

    ```shell
    curl -v http://ruuvigateway<XXXX>.local/history -H "Authorization: Bearer Uj+4tj24unVekco/lTLTRyxUfv1J8M6U+sbNsKTWRr0="
    ```

    **Checks**:

    - Check that the response is 'HTTP/1.0 200 OK'.

- **Action**: Check access to "/history" with curl with the read-write API key

    ```shell
    curl -v http://ruuvigateway<XXXX>.local/history -H "Authorization: Bearer 1SDrQH1FkH+pON0GsSjt2gYeMSP02uYqfuu7LWdaBvY="
    ```

    **Checks**:

    - Check that the response is 'HTTP/1.0 200 OK'.

- **Action**: Try HTTP GET "/ruuvi.json" with curl without API key

    ```shell
    curl -v http://ruuvigateway<XXXX>.local/ruuvi.json
    ```

    **Checks**:

    - Check that the response is 'HTTP/1.0 302 Found'.

- **Action**: Try HTTP GET "/ruuvi.json" with curl with the read-only API key

    ```shell
    curl -v http://ruuvigateway<XXXX>.local/ruuvi.json -H "Authorization: Bearer Uj+4tj24unVekco/lTLTRyxUfv1J8M6U+sbNsKTWRr0="
    ```

    **Checks**:

    - Check that the response is 'HTTP/1.0 200 OK'.

- **Action**: Try HTTP GET "/ruuvi.json" with curl with the read-write API key

    ```shell
    curl -v http://ruuvigateway<XXXX>.local/ruuvi.json -H "Authorization: Bearer 1SDrQH1FkH+pON0GsSjt2gYeMSP02uYqfuu7LWdaBvY="
    ```

    **Checks**:

    - Check that the response is 'HTTP/1.0 200 OK'.

- **Action**: Try HTTP POST "/ruuvi.json" with curl without API key

    ```shell
    curl -v -X POST http://ruuvigateway<XXXX>.local/ruuvi.json -d '{}'
    ```

    **Checks**:

    - Check that the response is 'HTTP/1.0 401 Unauthorized'.

- **Action**: Try HTTP POST "/ruuvi.json" with curl with the read-only API key

    ```shell
    curl -v -X POST http://ruuvigateway<XXXX>.local/ruuvi.json -d '{}' -H "Authorization: Bearer Uj+4tj24unVekco/lTLTRyxUfv1J8M6U+sbNsKTWRr0="
    ```

    **Checks**:

    - Check that the response is 'HTTP/1.0 401 Unauthorized'.

- **Action**: Try HTTP POST "/ruuvi.json" with curl with the read-write API key

    ```shell
    curl -v -X POST http://ruuvigateway<XXXX>.local/ruuvi.json -d '{}' -H "Authorization: Bearer 1SDrQH1FkH+pON0GsSjt2gYeMSP02uYqfuu7LWdaBvY="
    ```

    **Checks**:

    - Check that the response is 'HTTP/1.0 200 OK'.

## TST-006: Software update tests

### TST-006-AF001: Test installing firmware version v1.14.3 from the manually entered URL pointed to Ruuvi server

#### Setup
- Connect Ethernet cable to the gateway.

#### Test steps
- **Action**: Erase flash and write firmware.

    ```shell
    python3 ruuvi_gw_flash.py v1.15.0 --erase_flash
    ```

    **Checks**:

    - LED: 'G'

- **Action**: Use a browser to open the gateway's hostname ruuvigateway<XXXX>.local.

    **Checks**:

    - Check that the '**Login to Settings Page**' page is opened in the browser.

- **Action**: Use default credentials to log in (use Device ID (XX:XX:XX:XX:XX:XX:XX:XX) as the password).

    **Checks**:

    - Check that the '**Setup Wizard**' page is opened in the browser.

- **Action**: Update firmware on **Software Update** page

    **Steps**:

    - Page **Setup Wizard**: Click **Let's get started**.
    - Page **Internet Connection**: click **Skip**.
    - Page **Software Update**: Click **Advanced Settings**.
    - Page **Software Update**: Enable checkbox **Don't use the software update provided by Ruuvi but download binary files from URL address instead.**.
    - Page **Software Update**: Enter URL: https://fwupdate.ruuvi.com/v1.14.3
    - Page **Software Update**: Click **Update**.
    - Wait until updating is finished.

    **Checks**:

    - Page **Finished!**: message '**All good! Update was successful.**' must be displayed.
    - LED: 'G'

- **Action**: Check firmware version in UI - refresh the page in the browser.

    **Checks**:

    - Check that the firmware version at the bottom of the page is v1.14.3.

### TST-006-AF002: Test installing firmware version v1.14.2 from the manually entered URL pointed to GitHub

#### Setup
- Connect Ethernet cable to the gateway.

#### Test steps
- **Action**: Erase flash and write firmware.

    ```shell
    python3 ruuvi_gw_flash.py v1.15.0 --erase_flash
    ```

    **Checks**:

    - LED: 'G'

- **Action**: Use a browser to open the gateway's hostname ruuvigateway<XXXX>.local.

    **Checks**:

    - Check that the '**Login to Settings Page**' page is opened in the browser.

- **Action**: Use default credentials to log in (use Device ID (XX:XX:XX:XX:XX:XX:XX:XX) as the password).

    **Checks**:

    - Check that the '**Setup Wizard**' page is opened in the browser.

- **Action**: Update firmware on **Software Update** page

    **Steps**:

    - Page **Setup Wizard**: Click **Let's get started**.
    - Page **Internet Connection**: click **Skip**.
    - Page **Software Update**: Click **Advanced Settings**.
    - Page **Software Update**: Enable checkbox **Don't use the software update provided by Ruuvi but download binary files from URL address instead.**.
    - Page **Software Update**: Enter URL: https://github.com/ruuvi/ruuvi.gateway_esp.c/releases/download/v1.14.2
    - Page **Software Update**: Click **Update**.
    - Wait until updating is finished.

    **Checks**:

    - Page **Finished!**: message '**All good! Update was successful.**' must be displayed.
    - LED: 'G'

- **Action**: Check firmware version in UI - refresh the page in the browser.

    **Checks**:

    - Check that the firmware version at the bottom of the page is v1.14.2.

### TST-006-AF003: Test installing firmware from the manually entered URL pointed to Jenkins server

#### Setup
- Connect Ethernet cable to the gateway.

#### Test steps
- **Action**: Erase flash and write firmware.

    ```shell
    python3 ruuvi_gw_flash.py v1.15.0 --erase_flash
    ```

    **Checks**:

    - LED: 'G'

- **Action**: Use a browser to open the gateway's hostname ruuvigateway<XXXX>.local.

    **Checks**:

    - Check that the '**Login to Settings Page**' page is opened in the browser.

- **Action**: Use default credentials to log in (use Device ID (XX:XX:XX:XX:XX:XX:XX:XX) as the password).

    **Checks**:

    - Check that the '**Setup Wizard**' page is opened in the browser.

- **Action**: Update firmware on **Software Update** page

    **Steps**:

    - Page **Setup Wizard**: Click **Let's get started**.
    - Page **Internet Connection**: click **Skip**.
    - Page **Software Update**: Click **Advanced Settings**.
    - Page **Software Update**: Enable checkbox **Don't use the software update provided by Ruuvi but download binary files from URL address instead.**.
    - Page **Software Update**: Enter URL: https://jenkins.ruuvi.com/job/ruuvi_gateway_esp-PR/1275/artifact/build/
    - Page **Software Update**: Click **Update**.
    - Wait until updating is finished.

    **Checks**:

    - Page **Finished!**: message '**All good! Update was successful.**' must be displayed.
    - LED: 'G'

- **Action**: Check firmware version in UI - refresh the page in the browser.

    **Checks**:

    - Check that the firmware version at the bottom of the page is v1.14.1-86-g70f9ebc.

### TST-006-AF003: Test installing firmware from the manually entered URL pointed to local server

#### Setup
- Connect Ethernet cable to the gateway.
- Configure mDNS on your computer, set host-name to 'my-https-server', set domain-name to 'local'.
- Run a local HTTP server.
    ```shell
    python3 ./ruuvi.gwui.html/scripts/http_server_auth.py --port 8000
    ```


#### Test steps
- **Action**: Erase flash and write firmware.

    ```shell
    python3 ruuvi_gw_flash.py v1.15.0 --erase_flash
    ```

    **Checks**:

    - LED: 'G'

- **Action**: Use a browser to open the gateway's hostname ruuvigateway<XXXX>.local.

    **Checks**:

    - Check that the '**Login to Settings Page**' page is opened in the browser.

- **Action**: Use default credentials to log in (use Device ID (XX:XX:XX:XX:XX:XX:XX:XX) as the password).

    **Checks**:

    - Check that the '**Setup Wizard**' page is opened in the browser.

- **Action**: Update firmware on **Software Update** page

    **Steps**:

    - Page **Setup Wizard**: Click **Let's get started**.
    - Page **Internet Connection**: click **Skip**.
    - Page **Software Update**: Click **Advanced Settings**.
    - Page **Software Update**: Enable checkbox **Don't use the software update provided by Ruuvi but download binary files from URL address instead.**.
    - Page **Software Update**: Enter URL: http://my-https-server.local:8000/.releases/v1.14.3/
    - Page **Software Update**: Click **Update**.
    - Wait until updating is finished.

    **Checks**:

    - Page **Finished!**: message '**All good! Update was successful.**' must be displayed.
    - LED: 'G'

- **Action**: Check firmware version in UI - refresh the page in the browser.

    **Checks**:

    - Check that the firmware version at the bottom of the page is v1.14.3.

### TST-006-AF004: Test updating firmware to the latest release

#### Setup
- Connect Ethernet cable to the gateway.
- Configure mDNS on your computer, set host-name to 'my-https-server', set domain-name to 'local'.
- Run a local HTTP server.
    ```shell
    python3 ./ruuvi.gwui.html/scripts/http_server_auth.py --port 8000
    ```


#### Test steps
- **Action**: Erase flash and write firmware.

    ```shell
    python3 ruuvi_gw_flash.py v1.15.0 --erase_flash
    ```

    **Checks**:

    - LED: 'G'

- **Action**: Use a browser to open the gateway's hostname ruuvigateway<XXXX>.local.

    **Checks**:

    - Check that the '**Login to Settings Page**' page is opened in the browser.

- **Action**: Use default credentials to log in (use Device ID (XX:XX:XX:XX:XX:XX:XX:XX) as the password).

    **Checks**:

    - Check that the '**Setup Wizard**' page is opened in the browser.

- **Action**: Configure firmware update server on **Software Update** page

    **Steps**:

    - Page **Setup Wizard**: Click **Let's get started**.
    - Page **Internet Connection**: click **Skip**.
    - Page **Software Update**: Click **Advanced Settings**.
    - Page **Software Update**: Enter URL for **Server for automatic firmware updates**: http://my-https-server.local:8000/firmwareupdate
    - Page **Software Update**: Click **Check**.
    - Page **Software Update**: Click **Save**.

    **Checks**:

    - Check that there is no errors during checking and saving the URL.

- **Action**: Update firmware on **Software Update** page

    **Steps**:

    - Refresh the page in the browser.
    - Page **Setup Wizard**: Click **Let's get started**.
    - Page **Internet Connection**: click **Skip**.

    **Checks**:

    - On page **Software Update**: Check displayed 'Current firmware version': v1.15.0
    - On page **Software Update**: Check displayed 'Latest available firmware version': v1.14.3
    - On page **Software Update**: Message '**An update is available! Press UPDATE to start the firmware update.**' must be displayed.
    - On page **Software Update**: Check that the button **UPDATE** is active.

- **Action**: Update firmware to v1.14.3.

    **Steps**:

    - Page **Software Update**: Click **Update**.
    - Wait until updating is finished.

    **Checks**:

    - Page **Finished!**: message '**All good! Update was successful.**' must be displayed.
    - LED: 'G'

- **Action**: Check firmware version in UI - refresh the page in the browser.

    **Checks**:

    - Check that the firmware version at the bottom of the page is v1.14.3.

### TST-006-AF005: Test auto updating firmware to the latest release v1.14.3

#### Setup
- Connect Ethernet cable to the gateway.
- Configure mDNS on your computer, set host-name to 'my-https-server', set domain-name to 'local'.
- Run a local HTTP server.
    ```shell
    python3 ./ruuvi.gwui.html/scripts/http_server_auth.py --port 8000
    ```


#### Test steps
- **Action**: Erase flash and write firmware.

    ```shell
    python3 ruuvi_gw_flash.py v1.15.0 --erase_flash
    ```

    **Checks**:

    - LED: 'G'

- **Action**: Use a browser to open the gateway's hostname ruuvigateway<XXXX>.local.

    **Checks**:

    - Check that the '**Login to Settings Page**' page is opened in the browser.

- **Action**: Use default credentials to log in (use Device ID (XX:XX:XX:XX:XX:XX:XX:XX) as the password).

    **Checks**:

    - Check that the '**Setup Wizard**' page is opened in the browser.

- **Action**: Configure firmware update server on **Software Update** page

    **Steps**:

    - Page **Setup Wizard**: Click **Let's get started**.
    - Page **Internet Connection**: click **Skip**.
    - Page **Software Update**: Click **Advanced Settings**.
    - Page **Software Update**: Enter URL for **Server for automatic firmware updates**: http://my-https-server.local:8000/firmwareupdate
    - Page **Software Update**: Click **Check**.
    - Page **Software Update**: Click **Save**.
    - Close browser

    **Checks**:

    - Check that there is no errors during checking and saving the URL.

- **Action**: Power off/on the gateway.

    **Checks**:

    - LED: 'G'

- **Action**: Wait 45 minutes.

    **Checks**:

    - LED: 'G'

- **Action**: Refresh the page in the browser

    **Checks**:

    - Check that the firmware version at the bottom of the page is v1.14.3.

### TST-006-AF006: Test auto updating firmware to the latest beta version v1.14.2

#### Setup
- Connect Ethernet cable to the gateway.
- Configure mDNS on your computer, set host-name to 'my-https-server', set domain-name to 'local'.
- Run a local HTTP server.
    ```shell
    python3 ./ruuvi.gwui.html/scripts/http_server_auth.py --port 8000
    ```


#### Test steps
- **Action**: Erase flash and write firmware.

    ```shell
    python3 ruuvi_gw_flash.py v1.15.0 --erase_flash
    ```

    **Checks**:

    - LED: 'G'

- **Action**: Use a browser to open the gateway's hostname ruuvigateway<XXXX>.local.

    **Checks**:

    - Check that the '**Login to Settings Page**' page is opened in the browser.

- **Action**: Use default credentials to log in (use Device ID (XX:XX:XX:XX:XX:XX:XX:XX) as the password).

    **Checks**:

    - Check that the '**Setup Wizard**' page is opened in the browser.

- **Action**: Configure firmware update server on **Software Update** page

    **Steps**:

    - Page **Setup Wizard**: Click **Let's get started**.
    - Page **Internet Connection**: click **Skip**.
    - Page **Software Update**: Click **Advanced Settings**.
    - Page **Software Update**: Enter URL for **Server for automatic firmware updates**: http://my-https-server.local:8000/firmwareupdate_beta
    - Page **Software Update**: Click **Check**.
    - Page **Software Update**: Click **Save**.
    - Page **Software Update**: Click **Continue and skip update** or **Next**.

    **Checks**:

    - Check that there is no errors during checking and saving the URL.

- **Action**: Configure firmware updating for beta testers on **Automatic Updates** page

    **Steps**:

    - Page **Automatic Configuration Download**: Click **Next**.
    - Page **Automatic Updates**: Click **Advanced Settings**.
    - Page **Automatic Updates**: Select **Auto update (for beta testers, accept all updates)**.
    - Page **Automatic Updates**: Click **Next**.
    - Page **Remote Access Settings**: Click **Next**.
    - Page **Cloud Options**: Click **Next**.

    **Checks**:

    - Page **Finished!** must be opened.
    - Message **Ruuvi Gateway is now functioning** must be displayed.
    - Message: **Connected to**: Network: Ethernet
    - LED: 'G'

- **Action**: Power off/on the gateway.

    **Checks**:

    - LED: 'G'

- **Action**: Wait 45 minutes.

    **Checks**:

    - LED: 'G'

- **Action**: Refresh the page in the browser

    **Checks**:

    - Check that the firmware version at the bottom of the page is v1.14.2.

### TST-006-AF007: Test manual firmware updating mode

#### Setup
- Connect Ethernet cable to the gateway.
- Configure mDNS on your computer, set host-name to 'my-https-server', set domain-name to 'local'.
- Run a local HTTP server.
    ```shell
    python3 ./ruuvi.gwui.html/scripts/http_server_auth.py --port 8000
    ```


#### Test steps
- **Action**: Erase flash and write firmware.

    ```shell
    python3 ruuvi_gw_flash.py v1.15.0 --erase_flash
    ```

    **Checks**:

    - LED: 'G'

- **Action**: Use a browser to open the gateway's hostname ruuvigateway<XXXX>.local.

    **Checks**:

    - Check that the '**Login to Settings Page**' page is opened in the browser.

- **Action**: Use default credentials to log in (use Device ID (XX:XX:XX:XX:XX:XX:XX:XX) as the password).

    **Checks**:

    - Check that the '**Setup Wizard**' page is opened in the browser.

- **Action**: Configure firmware update server on **Software Update** page

    **Steps**:

    - Page **Setup Wizard**: Click **Let's get started**.
    - Page **Internet Connection**: click **Skip**.
    - Page **Software Update**: Click **Advanced Settings**.
    - Page **Software Update**: Enter URL for **Server for automatic firmware updates**: http://my-https-server.local:8000/firmwareupdate
    - Page **Software Update**: Click **Check**.
    - Page **Software Update**: Click **Save**.
    - Page **Software Update**: Click **Continue and skip update** or **Next**.

    **Checks**:

    - Check that there is no errors during checking and saving the URL.

- **Action**: Configure firmware updating for beta testers on **Automatic Updates** page

    **Steps**:

    - Page **Automatic Configuration Download**: Click **Next**.
    - Page **Automatic Updates**: Click **Advanced Settings**.
    - Page **Automatic Updates**: Select **Manual updates only (not recommended)**.
    - Page **Automatic Updates**: Click **Next**.
    - Page **Remote Access Settings**: Click **Next**.
    - Page **Cloud Options**: Click **Next**.

    **Checks**:

    - Page **Finished!** must be opened.
    - Message **Ruuvi Gateway is now functioning** must be displayed.
    - Message: **Connected to**: Network: Ethernet
    - LED: 'G'

- **Action**: Power off/on the gateway.

    **Checks**:

    - LED: 'G'

- **Action**: Wait 45 minutes.

    **Checks**:

    - LED: 'G'

- **Action**: Refresh the page in the browser

    **Checks**:

    - Check that the firmware version at the bottom of the page is v1.15.0.

## TST-007: Custom HTTP(S) server tests

### Setup
- Create a directory for the tests.
    ```shell
    mkdir -p integration_tests/.test_results
    ```

- Prepare 'integration_tests/.test_results/secrets.json' with the configuration.
    ```json
    {
      "gw_mac": "XX:XX:XX:XX:9C:2C",
      "gw_id": "XX:XX:XX:XX:XX:XX:XX:XX",
      "url": "http://ruuvigateway9c2c.local",
      "http_server": "http://my-https-server.local:8000",
      "https_server": "https://my-https-server.local:8001",
    }
    ```
    - Set "gw_mac" to the MAC address of the gateway.
    - Set "gw_id" to the Device ID of the gateway (it is used as default password to access the gateway).
    - Set "url" to the hostname of the gateway, it has format "http://ruuvigatewayXXXX.local", replace 'XXXX' with last 4 digits of gw_mac.
    - Set "http_server" to the mDNS hostname of your computes.
    - Set "https_server" to the mDNS hostname of your computes.

- Generate self-signed certificates
    ```shell
    cd integration_tests/.test_results
    openssl genrsa -out server_key.pem 2048
    openssl req -new -key server_key.pem -out server_csr.pem -subj "/C=FI/ST=Uusimaa/L=Helsinki/O=Ruuvi/OU=/CN=my-https-server.local"
    openssl x509 -req -in server_csr.pem -signkey server_key.pem -out server_cert.pem -days 365
    openssl genrsa -out client_key.pem 2048
    openssl req -new -key client_key.pem -out client_csr.pem -subj "/C=FI/ST=Uusimaa/L=Helsinki/O=Ruuvi/OU=/CN=ruuvigateway9c2c.local"
    openssl x509 -req -in client_csr.pem -signkey client_key.pem -out client_cert.pem -days 365
    ```


### TST-007-AG001: Test sending data to HTTP server without authentication

#### Setup
- Connect Ethernet cable to the gateway.

#### Test steps
- **Action**: Erase flash and write firmware.

    ```shell
    python3 ruuvi_gw_flash.py v1.15.0 --erase_flash
    ```

    **Checks**:

    - LED: 'G'

- **Action**: Create a directory for the test and start the logging process.

    ```shell
    mkdir -p integration_tests/.test_results/TST-007-AG001
    cd integration_tests/.test_results/TST-007-AG001
    python3 ../../../scripts/ruuvi_gw_uart_log.py
    ```

- **Action**: Run HTTP server.

    ```shell
    cd integration_tests/.test_results/TST-007-AG001
    source ../../../ruuvi.gwui.html/scripts/.venv/bin/activate
    python3 ../../../ruuvi.gwui.html/scripts/http_server_auth.py --port 8000
    ```

- **Action**: Configure gateway to send data to HTTP server.

    ```shell
    cd ruuvi.gwui.html
    node scripts/ruuvi_gw_ui_tester.js --config tests/test_http.yaml --secrets ../integration_tests/.test_results/secrets.json 
    ```

    **Checks**:

    - The script ruuvi_gw_ui_tester.js must print message '**Finished successfully**' at the end.
    - LED: 'G'

### TST-007-AG002: Test sending data to HTTP server with authentication

#### Setup
- Connect Ethernet cable to the gateway.

#### Test steps
- **Action**: (Optionally) Erase flash and write firmware.

    ```shell
    python3 ruuvi_gw_flash.py v1.15.0 --erase_flash
    ```

    **Checks**:

    - LED: 'G'

- **Action**: Create a directory for the test and start the logging process.

    ```shell
    mkdir -p integration_tests/.test_results/TST-007-AG002
    cd integration_tests/.test_results/TST-007-AG002
    python3 ../../../scripts/ruuvi_gw_uart_log.py
    ```

- **Action**: Run HTTP server with authentication.

    ```shell
    cd integration_tests/.test_results/TST-007-AG002
    source ../../../ruuvi.gwui.html/scripts/.venv/bin/activate
    python3 ../../../ruuvi.gwui.html/scripts/http_server_auth.py --port 8000 -u user1 -p pass1
    ```

- **Action**: Configure gateway to send data to HTTP server.

    ```shell
    cd ruuvi.gwui.html
    node scripts/ruuvi_gw_ui_tester.js --config tests/test_http_with_auth.yaml --secrets ../integration_tests/.test_results/secrets.json 
    ```

    **Checks**:

    - The script ruuvi_gw_ui_tester.js must print message '**Finished successfully**' at the end.
    - LED: 'G'

### TST-007-AG003: Test sending data to local HTTPS server with the server certificate checking

#### Setup
- Connect Ethernet cable to the gateway.

#### Test steps
- **Action**: (Optionally) Erase flash and write firmware.

    ```shell
    python3 ruuvi_gw_flash.py v1.15.0 --erase_flash
    ```

    **Checks**:

    - LED: 'G'

- **Action**: Create a directory for the test and start the logging process.

    ```shell
    mkdir -p integration_tests/.test_results/TST-007-AG003
    cd integration_tests/.test_results/TST-007-AG003
    python3 ../../../scripts/ruuvi_gw_uart_log.py
    ```

- **Action**: Run HTTPS server with self-signed certificate.

    ```shell
    cd integration_tests/.test_results/TST-007-AG003
    source ../../../ruuvi.gwui.html/scripts/.venv/bin/activate
    python3 ../../../ruuvi.gwui.html/scripts/http_server_auth.py --port 8001 \
      --ssl_cert=../../../integration_tests/.test_results/server_cert.pem \
      --ssl_key=../../../integration_tests/.test_results/server_key.pem
    ```

- **Action**: Configure gateway to send data to HTTPS server with self-signed server certificate.

    ```shell
    cd ruuvi.gwui.html
    node scripts/ruuvi_gw_ui_tester.js --config tests/test_https_with_server_cert_checking.yaml --secrets ../integration_tests/.test_results/secrets.json 
    ```

    **Checks**:

    - The script ruuvi_gw_ui_tester.js must print message '**Finished successfully**' at the end.
    - LED: 'G'

### TST-007-AG004: Test sending data to local HTTPS server with the server and client certificates checking

#### Setup
- Connect Ethernet cable to the gateway.

#### Test steps
- **Action**: (Optionally) Erase flash and write firmware.

    ```shell
    python3 ruuvi_gw_flash.py v1.15.0 --erase_flash
    ```

    **Checks**:

    - LED: 'G'

- **Action**: Create a directory for the test and start the logging process.

    ```shell
    mkdir -p integration_tests/.test_results/TST-007-AG004
    cd integration_tests/.test_results/TST-007-AG004
    python3 ../../../scripts/ruuvi_gw_uart_log.py
    ```

- **Action**: Run HTTPS server with the server and client self-signed certificates.

    ```shell
    cd integration_tests/.test_results/TST-007-AG004
    source ../../../ruuvi.gwui.html/scripts/.venv/bin/activate
    python3 ../../../ruuvi.gwui.html/scripts/http_server_auth.py --port 8001 \
      --ssl_cert=../../../integration_tests/.test_results/server_cert.pem \
      --ssl_key=../../../integration_tests/.test_results/server_key.pem \
      --ca_cert=../../../integration_tests/.test_results/client_cert.pem
    ```

- **Action**: Configure gateway to send data to HTTPS server with the server and client certificates.

    ```shell
    cd ruuvi.gwui.html
    node scripts/ruuvi_gw_ui_tester.js --config tests/test_https_with_server_and_client_cert_checking.yaml --secrets ../integration_tests/.test_results/secrets.json 
    ```

    **Checks**:

    - The script ruuvi_gw_ui_tester.js must print message '**Finished successfully**' at the end.
    - LED: 'G'

## TST-008: MQTT server tests

### Setup
- Create a directory for the tests.
    ```shell
    mkdir -p integration_tests/.test_results
    ```

- Prepare 'integration_tests/.test_results/secrets.json' with the configuration.
    ```json
    {
      "gw_mac": "XX:XX:XX:XX:9C:2C",
      "gw_id": "XX:XX:XX:XX:XX:XX:XX:XX",
      "url": "http://ruuvigateway9c2c.local",
      "http_server": "http://my-https-server.local:8000",
      "https_server": "https://my-https-server.local:8001",
      "mqtt_client_cert": [
        "-----BEGIN CERTIFICATE-----",
        ... certificate content ...,
        "-----END CERTIFICATE-----"
      ],
      "mqtt_client_key": [
        "-----BEGIN RSA PRIVATE KEY-----",
        ... key content ...,
        "-----END RSA PRIVATE KEY-----"
      ],
      "mqtt_server_cert": [
        "-----BEGIN CERTIFICATE-----",
        ... certificate content ...,
        "-----END CERTIFICATE-----"
      ]
    }
    ```
    - Set "gw_mac" to the MAC address of the gateway.
    - Set "gw_id" to the Device ID of the gateway (it is used as default password to access the gateway).
    - Set "url" to the hostname of the gateway, it has format "http://ruuvigatewayXXXX.local", replace 'XXXX' with last 4 digits of gw_mac.
    - Set "http_server" to the mDNS hostname of your computes.
    - Set "https_server" to the mDNS hostname of your computes.
    - Set "mqtt_client_cert" to the content of the AWS MQTT client certificate.
    - Set "mqtt_client_key" to the content of the AWS MQTT client key.
    - Set "mqtt_server_cert" to the content of the AWS MQTT server certificate.


### TST-008-AE001: Test sending data to MQTT server via TCP

#### Setup
- Connect Ethernet cable to the gateway.

#### Test steps
- **Action**: Erase flash and write firmware.

    ```shell
    python3 ruuvi_gw_flash.py v1.15.0 --erase_flash
    ```

    **Checks**:

    - LED: 'G'

- **Action**: Create a directory for the test and start the logging process.

    ```shell
    mkdir -p integration_tests/.test_results/TST-008-AE001
    cd integration_tests/.test_results/TST-008-AE001
    python3 ../../../scripts/ruuvi_gw_uart_log.py
    ```

- **Action**: Configure gateway to send data to MQTT server via TCP.

    ```shell
    cd ruuvi.gwui.html
    node scripts/ruuvi_gw_ui_tester.js --config tests/test_mqtt_tcp.yaml --secrets ../integration_tests/.test_results/secrets.json 
    ```

    **Checks**:

    - The script ruuvi_gw_ui_tester.js must print message '**Finished successfully**' at the end.
    - LED: 'G'

### TST-008-AE002: Test sending data to MQTT server via SSL

#### Setup
- Connect Ethernet cable to the gateway.

#### Test steps
- **Action**: (Optionally) Erase flash and write firmware.

    ```shell
    python3 ruuvi_gw_flash.py v1.15.0 --erase_flash
    ```

    **Checks**:

    - LED: 'G'

- **Action**: Create a directory for the test and start the logging process.

    ```shell
    mkdir -p integration_tests/.test_results/TST-008-AE002
    cd integration_tests/.test_results/TST-008-AE002
    python3 ../../../scripts/ruuvi_gw_uart_log.py
    ```

- **Action**: Configure gateway to send data to MQTT server via SSL.

    ```shell
    cd ruuvi.gwui.html
    node scripts/ruuvi_gw_ui_tester.js --config tests/test_mqtt_ssl.yaml --secrets ../integration_tests/.test_results/secrets.json 
    ```

    **Checks**:

    - The script ruuvi_gw_ui_tester.js must print message '**Finished successfully**' at the end.
    - LED: 'G'

### TST-008-AE003: Test sending data to MQTT server via WebSocket

#### Setup
- Connect Ethernet cable to the gateway.

#### Test steps
- **Action**: (Optionally) Erase flash and write firmware.

    ```shell
    python3 ruuvi_gw_flash.py v1.15.0 --erase_flash
    ```

    **Checks**:

    - LED: 'G'

- **Action**: Create a directory for the test and start the logging process.

    ```shell
    mkdir -p integration_tests/.test_results/TST-008-AE003
    cd integration_tests/.test_results/TST-008-AE003
    python3 ../../../scripts/ruuvi_gw_uart_log.py
    ```

- **Action**: Configure gateway to send data to MQTT server via WebSocket.

    ```shell
    cd ruuvi.gwui.html
    node scripts/ruuvi_gw_ui_tester.js --config tests/test_mqtt_ws.yaml --secrets ../integration_tests/.test_results/secrets.json 
    ```

    **Checks**:

    - The script ruuvi_gw_ui_tester.js must print message '**Finished successfully**' at the end.
    - LED: 'G'

### TST-008-AE004: Test sending data to MQTT server via Secure WebSocket

#### Setup
- Connect Ethernet cable to the gateway.

#### Test steps
- **Action**: (Optionally) Erase flash and write firmware.

    ```shell
    python3 ruuvi_gw_flash.py v1.15.0 --erase_flash
    ```

    **Checks**:

    - LED: 'G'

- **Action**: Create a directory for the test and start the logging process.

    ```shell
    mkdir -p integration_tests/.test_results/TST-008-AE004
    cd integration_tests/.test_results/TST-008-AE004
    python3 ../../../scripts/ruuvi_gw_uart_log.py
    ```

- **Action**: Configure gateway to send data to MQTT server via Secure WebSocket.

    ```shell
    cd ruuvi.gwui.html
    node scripts/ruuvi_gw_ui_tester.js --config tests/test_mqtt_wss.yaml --secrets ../integration_tests/.test_results/secrets.json 
    ```

    **Checks**:

    - The script ruuvi_gw_ui_tester.js must print message '**Finished successfully**' at the end.
    - LED: 'G'

### TST-008-AE005: Test sending data to AWS MQTT server

#### Setup
- Connect Ethernet cable to the gateway.

#### Test steps
- **Action**: (Optionally) Erase flash and write firmware.

    ```shell
    python3 ruuvi_gw_flash.py v1.15.0 --erase_flash
    ```

    **Checks**:

    - LED: 'G'

- **Action**: Create a directory for the test and start the logging process.

    ```shell
    mkdir -p integration_tests/.test_results/TST-008-AE005
    cd integration_tests/.test_results/TST-008-AE005
    python3 ../../../scripts/ruuvi_gw_uart_log.py
    ```

- **Action**: Configure gateway to send data to AWS MQTT server.

    ```shell
    cd ruuvi.gwui.html
    node scripts/ruuvi_gw_ui_tester.js --config tests/test_mqtt_aws.yaml --secrets ../integration_tests/.test_results/secrets.json 
    ```

    **Checks**:

    - The script ruuvi_gw_ui_tester.js must print message '**Finished successfully**' at the end.
    - LED: 'G'

