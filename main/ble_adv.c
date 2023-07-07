/**
 * @file ble_adv.c
 * @author TheSomeMan
 * @date 2023-07-04
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include <esp_bt.h>
#include <services/gap/ble_svc_gap.h>
#include "ble_adv.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"
#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "host/ble_uuid.h"
#include "os_task.h"
#include "gw_cfg.h"

static const char TAG[] = "BLE_ADV";

#define BLE_ADV_DEVICE_INFO_MODEL_NUMBER      "RuuviAirQ"
#define BLE_ADV_DEVICE_INFO_HARDWARE_REVISION "Check PCB"
#define BLE_ADV_DEVICE_INFO_MANUFACTURER_NAME "Ruuvi Innovations Ltd"

#define BLE_ADV_OFFSET_MANUFACTURER_ID (5U)

static const ble_uuid16_t g_gatt_svr_svc_generic_access = BLE_UUID16_INIT(0x1800);
static const ble_uuid16_t g_gatt_svr_chr_device_name    = BLE_UUID16_INIT(0x2A00);

static const ble_uuid16_t g_gatt_svr_svc_device_information       = BLE_UUID16_INIT(0x180A);
static const ble_uuid16_t g_gatt_svr_chr_model_number_string      = BLE_UUID16_INIT(0x2A24);
static const ble_uuid16_t g_gatt_svr_chr_firmware_revision_string = BLE_UUID16_INIT(0x2A26);
static const ble_uuid16_t g_gatt_svr_chr_hardware_revision_string = BLE_UUID16_INIT(0x2A27);
static const ble_uuid16_t g_gatt_svr_chr_manufacturer_name_string = BLE_UUID16_INIT(0x2A29);

#define BLE_ADV_DEVICE_INFO_STRING_SIZE (32U)
#define BLE_ADV_DEVICE_NAME_SUFFIX_LEN  (5U)

static bool g_ble_adv_ready = false;
static char g_ble_adv_device_name[sizeof(BLE_ADV_DEVICE_INFO_MODEL_NUMBER) + 1 + BLE_ADV_DEVICE_NAME_SUFFIX_LEN];

static int
ble_adv_gatt_svr_chr_access(
    uint16_t                           conn_handle,
    uint16_t                           attr_handle,
    struct ble_gatt_access_ctxt* const p_ctxt,
    void* const                        p_arg);

static const struct ble_gatt_svc_def g_ble_gatt_services[] = {
    {
        // Service: Generic Access
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &g_gatt_svr_svc_generic_access.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                // Characteristic: Device Name
                .uuid = &g_gatt_svr_chr_device_name.u,
                .access_cb = &ble_adv_gatt_svr_chr_access,
                .flags = BLE_GATT_CHR_F_READ,
                .arg = NULL,
            },
            {
                0, // No more characteristics in this service
            },
        },
    },
    {
        // Service: Device Information
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &g_gatt_svr_svc_device_information.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                // Characteristic: Model Number String
                .uuid = &g_gatt_svr_chr_model_number_string.u,
                .access_cb = &ble_adv_gatt_svr_chr_access,
                .flags = BLE_GATT_CHR_F_READ,
                .arg = NULL,
            },
            {
                // Characteristic: Firmware Revision String
                .uuid = &g_gatt_svr_chr_firmware_revision_string.u,
                .access_cb = &ble_adv_gatt_svr_chr_access,
                .flags = BLE_GATT_CHR_F_READ,
                .arg = NULL,
            },
            {
                // Characteristic: Hardware Revision String
                .uuid = &g_gatt_svr_chr_hardware_revision_string.u,
                .access_cb = &ble_adv_gatt_svr_chr_access,
                .flags = BLE_GATT_CHR_F_READ,
                .arg = NULL,
            },
            {
                // Characteristic: Manufacturer Name String
                .uuid = &g_gatt_svr_chr_manufacturer_name_string.u,
                .access_cb = &ble_adv_gatt_svr_chr_access,
                .flags = BLE_GATT_CHR_F_READ,
                .arg = NULL,
            },
            {
                0, // No more characteristics in this service
            },
        },
    },
    {
        0, // No more services
    },
};

static int
ble_adv_alloc_mbuf_string(struct ble_gatt_access_ctxt* const p_ctxt, const char* const p_str)
{
    const char* info_string = (const char*)p_str;
    const int   res         = os_mbuf_append(p_ctxt->om, info_string, strlen(info_string));
    if (0 != res)
    {
        LOG_ERR("os_mbuf_append failed, res=%d", res);
        return BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    return 0;
}

static void
ble_adv_generate_device_name(char* const p_buf, const size_t buf_size)
{
    const mac_address_str_t* const p_mac_addr   = gw_cfg_get_esp32_mac_addr_bluetooth();
    const size_t                   mac_addr_len = strlen(p_mac_addr->str_buf);

    memset(p_buf, 0, buf_size);

    if (mac_addr_len > BLE_ADV_DEVICE_NAME_SUFFIX_LEN)
    {
        (void)snprintf(
            p_buf,
            buf_size,
            "%s %.2s%.2s",
            BLE_ADV_DEVICE_INFO_MODEL_NUMBER,
            &p_mac_addr->str_buf[mac_addr_len - BLE_ADV_DEVICE_NAME_SUFFIX_LEN],
            &p_mac_addr->str_buf[mac_addr_len - BLE_ADV_DEVICE_NAME_SUFFIX_LEN + 3]);
    }
    else
    {
        (void)snprintf(p_buf, buf_size, "%s %.5s", BLE_ADV_DEVICE_INFO_MODEL_NUMBER, p_mac_addr->str_buf);
    }
}

static int
ble_adv_gatt_svr_chr_access(
    uint16_t                           conn_handle,
    uint16_t                           attr_handle,
    struct ble_gatt_access_ctxt* const p_ctxt,
    void* const                        p_arg)
{
    (void)conn_handle;
    (void)attr_handle;
    (void)p_arg;

    if (p_ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR)
    {
        if (0 == ble_uuid_cmp(p_ctxt->chr->uuid, &g_gatt_svr_chr_device_name.u))
        {
            LOG_INFO("Requested GATT attribute: Device Name: %s", g_ble_adv_device_name);

            return ble_adv_alloc_mbuf_string(p_ctxt, g_ble_adv_device_name);
        }
        if (0 == ble_uuid_cmp(p_ctxt->chr->uuid, &g_gatt_svr_chr_model_number_string.u))
        {
            LOG_INFO("Requested GATT attribute: Model number: %s", BLE_ADV_DEVICE_INFO_MODEL_NUMBER);
            return ble_adv_alloc_mbuf_string(p_ctxt, BLE_ADV_DEVICE_INFO_MODEL_NUMBER);
        }
        if (0 == ble_uuid_cmp(p_ctxt->chr->uuid, &g_gatt_svr_chr_firmware_revision_string.u))
        {
            const ruuvi_esp32_fw_ver_str_t* const p_fw_ver = gw_cfg_get_esp32_fw_ver();

            char buf[BLE_ADV_DEVICE_INFO_STRING_SIZE];
            (void)snprintf(buf, sizeof(buf), "%s", p_fw_ver->buf);

            LOG_INFO("Requested GATT attribute: Firmware version: %s", buf);

            return ble_adv_alloc_mbuf_string(p_ctxt, buf);
        }
        if (0 == ble_uuid_cmp(p_ctxt->chr->uuid, &g_gatt_svr_chr_hardware_revision_string.u))
        {
            LOG_INFO("Requested GATT attribute: Hardware revision: %s", BLE_ADV_DEVICE_INFO_HARDWARE_REVISION);
            return ble_adv_alloc_mbuf_string(p_ctxt, BLE_ADV_DEVICE_INFO_HARDWARE_REVISION);
        }
        if (0 == ble_uuid_cmp(p_ctxt->chr->uuid, &g_gatt_svr_chr_manufacturer_name_string.u))
        {
            LOG_INFO("Requested GATT attribute: Manufacturer name: %s", BLE_ADV_DEVICE_INFO_MANUFACTURER_NAME);
            return ble_adv_alloc_mbuf_string(p_ctxt, BLE_ADV_DEVICE_INFO_MANUFACTURER_NAME);
        }
    }
    return BLE_ATT_ERR_ATTR_NOT_FOUND;
}

static void
ble_adv_on_sync(void)
{
    LOG_INFO("ble_adv_on_sync: BLE stack is ready");

    g_ble_adv_ready = true;
}

static ATTR_NORETURN void
ble_adv_task(void)
{
    // This task initializes the NimBLE host
    LOG_INFO("ble_adv_task: nimble_port_run");
    nimble_port_run();
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

bool
ble_adv_init(void)
{
    LOG_INFO("ble_adv_init");

    ble_adv_generate_device_name(g_ble_adv_device_name, sizeof(g_ble_adv_device_name));

    // Initialize the NimBLE stack
    esp_nimble_hci_and_controller_init();
    nimble_port_init();

    // Register GATT services
    int res = ble_gatts_count_cfg(g_ble_gatt_services);
    if (0 != res)
    {
        LOG_ERR("ble_gatts_count_cfg failed, res=%d", res);
    }
    res = ble_gatts_add_svcs(g_ble_gatt_services);
    if (0 != res)
    {
        LOG_ERR("ble_gatts_add_svcs failed, res=%d", res);
    }

    esp_err_t err = esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9);
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "%s failed", "esp_ble_tx_power_set");
    }

    res = ble_svc_gap_device_name_set("Ruuvi Gateway");
    if (0 != res)
    {
        LOG_ERR("ble_svc_gap_device_name_set failed, res=%d", res);
    }

    // Set the sync callback
    ble_hs_cfg.sync_cb = &ble_adv_on_sync;

    // Initialize the NimBLE host configuration
    ble_hs_cfg.sm_bonding        = 1;
    ble_hs_cfg.sm_sc             = 1;
    ble_hs_cfg.sm_mitm           = 1;
    ble_hs_cfg.sm_our_key_dist   = 1;
    ble_hs_cfg.sm_their_key_dist = 1;

    const uint32_t           stack_size = 4096;
    const os_task_priority_t priority   = configMAX_PRIORITIES - 2;
    os_task_handle_t         h_task     = NULL;
    if (!os_task_create_without_param(&ble_adv_task, "ble_adv", stack_size, priority, &h_task))
    {
        LOG_ERR("%s failed", "Can't create ble_adv task");
        return false;
    }
    return true;
}

void
ble_adv_send(uint8_t buf[RE_CA_UART_ADV_BYTES])
{
    if (!g_ble_adv_ready)
    {
        return;
    }

    int res = ble_gap_adv_stop();
    if ((0 != res) && (BLE_HS_EALREADY != res))
    {
        LOG_ERR("ble_gap_adv_stop failed, res=%d", res);
    }

    const size_t   offset   = BLE_ADV_OFFSET_MANUFACTURER_ID;
    uint8_t* const p_data   = &buf[offset];
    const size_t   data_len = RE_CA_UART_ADV_BYTES - offset;

    LOG_DUMP_DBG(p_data, data_len, "ble_adv_send");

    struct ble_hs_adv_fields fields = { 0 };

    // Set the flags - 02 01 06
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    fields.mfg_data     = p_data;
    fields.mfg_data_len = data_len;

    ble_gap_adv_set_fields(&fields);

    struct ble_gap_adv_params adv_params;
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    // Set advertising interval (in 0.625 ms units)
#if 1
    adv_params.itvl_min = 338; // 211.25 ms
    adv_params.itvl_max = 338; // 211.25 ms
#else
    adv_params.itvl_min = 800; // 500 ms
    adv_params.itvl_max = 800; // 500 ms
#endif

    res = ble_gap_adv_start(0, NULL, BLE_HS_FOREVER, &adv_params, NULL, NULL);
    if (0 != res)
    {
        LOG_ERR("ble_gap_adv_start failed, err=%d", res);
    }
}

void
ble_adv_send_device_name(void)
{
    if (!g_ble_adv_ready)
    {
        return;
    }

    int res = ble_gap_adv_stop();
    if ((0 != res) && (BLE_HS_EALREADY != res))
    {
        LOG_ERR("ble_gap_adv_stop failed, res=%d", res);
    }

    LOG_INFO("BLE Send: Device Name: %s", g_ble_adv_device_name);

    struct ble_hs_adv_fields fields = { 0 };

    // Set the flags - 02 01 06
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    // Set the device name
    fields.name             = (uint8_t*)g_ble_adv_device_name;
    fields.name_len         = strlen((char*)fields.name);
    fields.name_is_complete = 1;

    ble_gap_adv_set_fields(&fields);

    struct ble_gap_adv_params adv_params;
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    // Set advertising interval (in 0.625 ms units)
    adv_params.itvl_min = 32; // 20 ms
    adv_params.itvl_max = 32; // 20 ms

    res = ble_gap_adv_start(0, NULL, BLE_HS_FOREVER, &adv_params, NULL, NULL);
    if (0 != res)
    {
        LOG_ERR("ble_gap_adv_start failed, err=%d", res);
    }
}
