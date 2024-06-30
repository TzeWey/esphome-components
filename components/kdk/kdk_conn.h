#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/uart/uart.h"

#include <map>
#include <queue>
#include <vector>

#include "kdk_conn_client.h"

namespace esphome {
namespace kdk {

static const uint32_t KDK_SUPPORTED_BAUD_RATE = 9600;

static const uint16_t KDK_MESSAGE_BUFFER_SIZE = 256 + 6;  // Largest packet captured is 145 bytes [CMD 0110]
static const uint8_t KDK_MESSAGE_CHECKSUM_SIZE = 1;

static const uint32_t KDK_BYTE_TIMEOUT = 100;  // Inter-byte timeout in ms
static const uint32_t KDK_RECV_TIMEOUT = 300;  // Message response timeout in ms
static const uint32_t KDK_SEND_MAX_RETRY = 5;  // Number of retry attempts when a response is not received

static const uint32_t KDK_DEFAULT_POLL_INTERVAL = 5000;
static const uint32_t KDK_WAIT_SYNC_TIMEOUT = 10000;

// KDK Frame Constants
static const uint8_t KDK_MESSAGE_SYNC = 0x66;   // First byte sent by the device on power up
static const uint8_t KDK_MESSAGE_START = 0x5A;  // First byte sent on every normal command

static const uint8_t KDK_MSG_TYPE_SIZE = 1;
static const uint8_t KDK_MSG_TABLE_ID_SIZE = 3;
static const uint8_t KDK_MSG_PARAM_COUNT_SIZE = 1;
static const uint8_t KDK_MSG_PARAM_ID_REQ_SIZE = 3;

struct KdkMsg {
  uint8_t start;
  uint8_t counter;
  uint16_t command;
  uint8_t dummy;  // always 0 for now
  uint8_t length;
  uint8_t payload[];
} PACKED;

struct KdkParam {
  uint16_t id;       // ID
  uint8_t metadata;  // Some kind of metadata, not sure what it means
  uint8_t size;      // Data Size
  std::vector<uint8_t> data;
};

struct KdkParamUpdate {
  uint16_t id;
  std::vector<uint8_t> data;
};

enum KdkCommFsmState {
  KDK_COMM_STATE_NONE = 0,
  KDK_COMM_STATE_UNINITIALIZED,
  KDK_COMM_STATE_INIT_SYNC,
  KDK_COMM_STATE_INIT_0C00,
  KDK_COMM_STATE_INIT_1000,
  KDK_COMM_STATE_INIT_1100,  // Device Info
  KDK_COMM_STATE_INIT_1200,
  KDK_COMM_STATE_INIT_4100,
  KDK_COMM_STATE_INIT_4C01,
  KDK_COMM_STATE_INIT_0010,
  KDK_COMM_STATE_INIT_0110,  // List of supported parameters
  KDK_COMM_STATE_INIT_0210,
  KDK_COMM_STATE_INIT_1800,
  KDK_COMM_STATE_INIT_0001_10,  // Publish module status? 0x10
  KDK_COMM_STATE_INIT_0001_11,  // Publish module status? 0x11
  KDK_COMM_STATE_INIT_0910,     // Initial state poll batch 1
  KDK_COMM_STATE_INIT_DONE,     // Final INIT state, module is considered INITIALIZED after this state
  KDK_COMM_STATE_IDLE,
  KDK_COMM_STATE_PULL_STATES_0910,
  KDK_COMM_STATE_PUSH_STATES_0810,
};

enum KdkCommFsmMethod {
  KDK_COMM_FSM_ENTRY,
  KDK_COMM_FSM_LOOP,
  KDK_COMM_FSM_EXIT,
};
enum KdkCommFsmEvent {
  KDK_COMM_FSM_EVENT_SYNC_RECEIVED,
  KDK_COMM_FSM_EVENT_SYNC_OK,
  KDK_COMM_FSM_EVENT_SYNC_TIMEOUT,
  KDK_COMM_FSM_EVENT_SYNC_RECOVERY,
  KDK_COMM_FSM_EVENT_RESPONSE_RECEIVED,
  KDK_COMM_FSM_EVENT_INIT_DONE,
  KDK_COMM_FSM_EVENT_PULL_STATES,
  KDK_COMM_FSM_EVENT_PUSH_STATES,
};

template<typename T> using EnumStrMap = std::map<T, std::string>;

static const EnumStrMap<enum KdkCommFsmState> KDK_COMM_FSM_STATE_STR_MAP = {
    {KDK_COMM_STATE_NONE, "NONE"},                          // Should never enter this state
    {KDK_COMM_STATE_UNINITIALIZED, "UNINITIALIZED"},        //
    {KDK_COMM_STATE_INIT_SYNC, "INIT_SYNC"},                //
    {KDK_COMM_STATE_INIT_0C00, "INIT_0C00"},                //
    {KDK_COMM_STATE_INIT_1000, "INIT_1000"},                //
    {KDK_COMM_STATE_INIT_1100, "INIT_1100"},                //
    {KDK_COMM_STATE_INIT_1200, "INIT_1200"},                //
    {KDK_COMM_STATE_INIT_4100, "INIT_4100"},                //
    {KDK_COMM_STATE_INIT_4C01, "INIT_4C01"},                //
    {KDK_COMM_STATE_INIT_0010, "INIT_0010"},                //
    {KDK_COMM_STATE_INIT_0110, "INIT_0110"},                //
    {KDK_COMM_STATE_INIT_0210, "INIT_0210"},                //
    {KDK_COMM_STATE_INIT_1800, "INIT_1800"},                //
    {KDK_COMM_STATE_INIT_0001_10, "INIT_0001_10"},          //
    {KDK_COMM_STATE_INIT_0001_11, "INIT_0001_11"},          //
    {KDK_COMM_STATE_INIT_0910, "INIT_0910"},                //
    {KDK_COMM_STATE_INIT_DONE, "INIT_DONE"},                //
    {KDK_COMM_STATE_IDLE, "IDLE"},                          //
    {KDK_COMM_STATE_PULL_STATES_0910, "PULL_STATES_0910"},  //
    {KDK_COMM_STATE_PUSH_STATES_0810, "PUSH_STATES_0810"},  //
};

static const EnumStrMap<enum KdkCommFsmMethod> KDK_COMM_FSM_METHOD_STR_MAP = {
    {KDK_COMM_FSM_ENTRY, "ENTRY"},
    {KDK_COMM_FSM_LOOP, "LOOP"},
    {KDK_COMM_FSM_EXIT, "EXIT"},
};

static const EnumStrMap<enum KdkCommFsmEvent> KDK_COMM_FSM_EVENT_STR_MAP = {
    {KDK_COMM_FSM_EVENT_SYNC_RECEIVED, "SYNC_RECEIVED"},          //
    {KDK_COMM_FSM_EVENT_SYNC_OK, "SYNC_OK"},                      //
    {KDK_COMM_FSM_EVENT_SYNC_TIMEOUT, "SYNC_TIMEOUT"},            //
    {KDK_COMM_FSM_EVENT_SYNC_RECOVERY, "SYNC_RECOVERY"},          //
    {KDK_COMM_FSM_EVENT_RESPONSE_RECEIVED, "RESPONSE_RECEIVED"},  //
    {KDK_COMM_FSM_EVENT_INIT_DONE, "INIT_DONE"},                  //
    {KDK_COMM_FSM_EVENT_PULL_STATES, "PULL_STATES"},              //
    {KDK_COMM_FSM_EVENT_PUSH_STATES, "PUSH_STATES"},              //
};

class KdkConnectionManager : public PollingComponent, public uart::UARTDevice {
 protected:
  struct {
    uint32_t byte_timeout = KDK_BYTE_TIMEOUT;            // Time in ms to wait between bytes
    uint32_t receive_timeout = KDK_RECV_TIMEOUT;         // Time in ms to wait for a response before retrying
    uint32_t poll_interval = KDK_DEFAULT_POLL_INTERVAL;  // Time in ms between poll intervals
  } cfg_;

  struct {
    uint32_t timestamp = 0;     // Timestamp of last transmission
    uint8_t counter = 0;        // Transmit frame counter
    uint8_t buffer_length = 0;  // Number of valid bytes in the TX buffer, used when retransmitting the buffer.
    uint8_t buffer[KDK_MESSAGE_BUFFER_SIZE];

    bool retry_pending = 0;   // True to resend last message
    uint8_t retry_count = 0;  // Number of retry attempts
  } tx_;

  struct {
    uint32_t index = 0;      // Index to insert the next byte in the receive buffer
    uint32_t timestamp = 0;  // Timestamp of last received byte
    uint8_t counter = 0;     // Receive frame counter
    uint8_t sum = 0;         // Running sum of received bytes
    uint8_t buffer[KDK_MESSAGE_BUFFER_SIZE];
  } rx_;

  struct {
    std::vector<KdkConnectionClient *> clients;

    /* Message Flags */
    bool waiting_response = false;  // True when expecting a response

    bool message_pending = false;  // True when a valid message is received

    /* Product Info */
    std::string product_model = "";
    std::string product_serial = "";

    /* Parameter Info */
    uint32_t parameter_table_id = 0;
    std::map<uint16_t, struct KdkParam> parameters;

    std::vector<struct KdkParamUpdate> pending_parameter_update_list;

    uint32_t last_ping_timestamp = 0;
    uint32_t last_update_timestamp = 0;

  } state_;

  struct {
    KdkCommFsmState state = KdkCommFsmState::KDK_COMM_STATE_UNINITIALIZED;
    std::queue<KdkCommFsmEvent> event_queue;

  } fsm_;

  // Utilities
  std::string hex2str(const uint8_t *buffer, size_t length);
  std::string hex2str(std::vector<uint8_t> data);
  uint8_t calculate_sum(const uint8_t *buffer, size_t length);

  std::string get_state_name(enum KdkCommFsmState x) { return KDK_COMM_FSM_STATE_STR_MAP.find(x)->second; }
  std::string get_method_name(enum KdkCommFsmMethod x) { return KDK_COMM_FSM_METHOD_STR_MAP.find(x)->second; }
  std::string get_event_name(enum KdkCommFsmEvent x) { return KDK_COMM_FSM_EVENT_STR_MAP.find(x)->second; }

  bool is_update_pending(void) { return this->state_.pending_parameter_update_list.size() > 0; }

  // Internal
  void notify_clients_on_parameter_update(void);

  void receiver_reset_states(void);
  void receiver_process_byte(uint8_t byte);

  void send_request(uint16_t command, const uint8_t *payload, uint16_t length, bool posted = false);
  void send_response(uint8_t counter, uint16_t command, const uint8_t *payload, uint16_t length);

  void transmit_message(void);

  void check_response_timeout(const uint32_t now);

  // Message Helpers
  bool is_message_pending(void) { return this->state_.message_pending; };
  void clear_message_pending(void) { this->state_.message_pending = false; };

  bool is_waiting_response(void) { return this->state_.waiting_response; }

  bool is_message_response(uint16_t command);

  void save_parameter_table_id(const uint8_t *buffer);
  void fill_parameter_table_id(uint8_t *buffer);
  void fill_parameter_requests(uint8_t *buffer, std::vector<uint16_t> id_list);

  void parse_parameter_response(const uint8_t *buffer);

  // Response Handlers
  void process_response_default(uint16_t command);
  void process_response_1100(void);
  void process_response_0010(void);
  void process_response_0110(void);
  void process_response_0210(void);
  void process_response_0910(void);

  // Message builder
  void send_message_0810(std::vector<struct KdkParamUpdate> key_value_pairs);
  void send_message_0910(std::vector<uint16_t> id_list);

  void send_message_0210_init(void);
  void send_message_0910_init(void);

  void send_message_0910_poll(void);

  // Message Handlers
  void process_message_0101(const KdkMsg *msg);
  void process_message_0A10(const KdkMsg *msg);

  void process_message(void);

  // FSM
  void fsm_push_event(KdkCommFsmEvent event);
  KdkCommFsmState fsm_next_state(KdkCommFsmState state, KdkCommFsmEvent event);
  void fsm_run(void);
  void fsm_state_handlers(KdkCommFsmMethod method);

  const struct KdkMsg *message(void) const { return (KdkMsg *) this->rx_.buffer; }

 public:
  void dump_config() override;
  void update() override;

  void register_client(KdkConnectionClient *client);

  const std::vector<uint8_t> get_parameter_data(uint16_t id) const;
  void update_parameter_data(std::vector<struct KdkParamUpdate> parameters);

  void set_receive_timeout(uint32_t value_ms) { this->cfg_.receive_timeout = value_ms; }
  void set_poll_interval(uint32_t value_ms) { this->cfg_.poll_interval = value_ms; }

  bool is_ready(void) { return this->fsm_.state >= KDK_COMM_STATE_INIT_DONE; };

  KdkConnectionManager(){};
};

}  // namespace kdk
}  // namespace esphome
