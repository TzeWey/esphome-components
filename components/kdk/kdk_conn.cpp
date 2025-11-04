#include "esphome/core/log.h"

#include "kdk_conn.h"

namespace esphome {
namespace kdk {

static const char *const TAG = "kdk.conn";

// All responses have bit 15 of the command set
#define IS_RESPONSE_MESSAGE(cmd) ((cmd & 0x8000) != 0)
#define SET_RESPONSE_MESSAGE(cmd) (cmd | 0x8000)

#define GET_COMMAND(cmd) (cmd & 0x7FFF)

/*******************************************************************************
 * KdkConnectionManager
 ******************************************************************************/

/* FRAME FORMAT
 * ┌────────┬─────────┬─────────┬─────────┬─────────┬──────────┐
 * │ START  │ TX/RX   │ COMMAND │ LENGTH  │ PAYLOAD │ CHECKSUM │
 * │        │ COUNTER │         │         │         │          │
 * ├────────┼─────────┼─────────┼─────────┼─────────┼──────────┤
 * │ 1 Byte │ 1 Byte  │ 2 Bytes │ 2 Bytes │ LENGTH  │ 1 Byte   │
 * │        │         │         │         │ Bytes   │          │
 * └────────┴─────────┴─────────┴─────────┴─────────┴──────────┘
 */

/*******************************************************************************
 * PROTECTED - UTILITIES
 ******************************************************************************/

std::string KdkConnectionManager::hex2str(const uint8_t *buffer, size_t length) {
  if (buffer == NULL) {
    return "[NULL]";
  }

  if (length == 0) {
    return "[EMPTY]";
  }

  constexpr char hexmap[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
  std::string str(length * 3, ' ');
  for (int i = 0; i < length; ++i) {
    str[3 * i] = hexmap[(buffer[i] & 0xF0) >> 4];
    str[3 * i + 1] = hexmap[buffer[i] & 0x0F];
  }
  return str;
}

std::string KdkConnectionManager::hex2str(std::vector<uint8_t> v) { return this->hex2str(v.data(), v.size()); }

/**
 * Calculates the sum of the provided buffer.
 */
uint8_t KdkConnectionManager::calculate_sum(const uint8_t *buffer, size_t length) {
  uint8_t sum = 0;
  for (int i = 0; i < length; i++) {
    sum += buffer[i];
  }
  return ((0 - sum) & 0xFF);
}

/*******************************************************************************
 * PROTECTED - INTERNAL
 ******************************************************************************/

void KdkConnectionManager::notify_clients_on_parameter_update(void) {
  for (auto &client : this->state_.clients) {
    client->on_parameter_update();
  }
}

void KdkConnectionManager::receiver_reset_states(void) {
  this->rx_.index = 0;
  this->rx_.sum = 0;
  this->rx_.timestamp = millis();
}

void KdkConnectionManager::receiver_process_byte(uint8_t byte) {
  const uint32_t now = millis();

  // Reset the receiver logic if the last byte received was a long time ago
  const uint32_t elapsed = (now - this->rx_.timestamp);
  if (elapsed > this->cfg_.byte_timeout) {
    ESP_LOGV(TAG, "RX> Inter-byte timeout after %d ms", elapsed);
    this->receiver_reset_states();
  } else {
    // Update receive timestamp
    this->rx_.timestamp = now;
  }

  ESP_LOGVV(TAG, "RX> Received %02X", byte);

  // Check whether the first byte is START
  if ((this->rx_.index == 0) && ((byte != KDK_MESSAGE_START) && (byte != KDK_MESSAGE_SYNC))) {
    ESP_LOGV(TAG, "RX> Not a valid START");
    return;
  }

  // Store the received byte into the buffer
  this->rx_.buffer[this->rx_.index++] = byte;
  this->rx_.sum += byte;

  // Check whether we have received enough bytes to begin processing:
  // START, COUNTER, COMMAND and LENGTH
  if (this->rx_.index < sizeof(struct KdkMsg)) {
    return;
  }

  // Map receive buffer to KdkMsg
  struct KdkMsg *cmd = (struct KdkMsg *) this->rx_.buffer;

  // Special handling for SYNC frame, reinitialize comm states
  if (cmd->start == KDK_MESSAGE_SYNC) {
    ESP_LOGI(TAG, "RX> Received SYNC frame, re-initializing module");
    this->receiver_reset_states();
    this->fsm_push_event(KDK_COMM_FSM_EVENT_SYNC_RECEIVED);
    return;
  }

  uint16_t expected_length = sizeof(struct KdkMsg) + cmd->length + KDK_MESSAGE_CHECKSUM_SIZE;

  // Verify that the frame will fit in our receive buffer
  if (expected_length > KDK_MESSAGE_BUFFER_SIZE) {
    ESP_LOGW(TAG, "RX> Length %d is larger than the receive buffer of %d", expected_length, KDK_MESSAGE_BUFFER_SIZE);
    this->receiver_reset_states();
    return;
  }

  // Wait for all bytes to be received
  if (this->rx_.index < expected_length) {
    return;
  }

  // Verify checksum
  size_t command_length = sizeof(struct KdkMsg) + cmd->length;
  uint8_t sum = this->calculate_sum(this->rx_.buffer, command_length);
  uint8_t checksum = cmd->payload[cmd->length];
  if (checksum != sum) {
    ESP_LOGW(TAG, "RX> Invalid checksum: got=%02X, exp=%02X", checksum, sum);
    this->receiver_reset_states();
    return;
  }

  ESP_LOGV(TAG, "RX> Received:");
  ESP_LOGV(TAG, "RX>   counter: %d", cmd->counter);
  ESP_LOGV(TAG, "RX>   command: %04X", cmd->command);
  ESP_LOGV(TAG, "RX>   length: %d", cmd->length);
  ESP_LOGV(TAG, "RX>   payload: %s", this->hex2str(cmd->payload, cmd->length).c_str());
  ESP_LOGV(TAG, "RX>   checksum: %02X", checksum);

  // A valid command was received, reset the receiver states
  this->receiver_reset_states();
  this->state_.message_pending = true;

  // Clear flag if message is a command response
  if (IS_RESPONSE_MESSAGE(cmd->command)) {
    this->clear_waiting_response();
  }
}

void KdkConnectionManager::send_request(uint16_t command, const uint8_t *payload, uint16_t length, bool posted) {
  struct KdkMsg *cmd = (struct KdkMsg *) this->tx_.buffer;

  this->tx_.counter++;  // Advance TX counter before transmitting, value is reset to 0xFF on SYNC

  cmd->start = KDK_MESSAGE_START;
  cmd->dummy = 0;
  cmd->counter = this->tx_.counter;
  cmd->command = command;
  cmd->length = length;

  if ((payload != NULL) && (length > 0)) {
    memcpy(cmd->payload, payload, length);
  }

  size_t buf_length = sizeof(struct KdkMsg) + cmd->length;  // Length of bytes to compute the checksum
  uint8_t checksum = this->calculate_sum(this->tx_.buffer, buf_length);
  this->tx_.buffer[buf_length++] = checksum;  // Add checksum to the final buffer length
  this->tx_.buffer_length = buf_length;
  this->transmit_message();

  // Response is expected for a non-posted message
  if (!posted) {
    this->state_.waiting_response = true;
  }

  ESP_LOGV(TAG, "TX> Send Request:");
  ESP_LOGV(TAG, "TX>   counter: %d", cmd->counter);
  ESP_LOGV(TAG, "TX>   command: %04X", cmd->command);
  ESP_LOGV(TAG, "TX>   length: %d", cmd->length);
  ESP_LOGV(TAG, "TX>   payload: %s", this->hex2str(cmd->payload, cmd->length).c_str());
  ESP_LOGV(TAG, "TX>   checksum: %02X", checksum);
  ESP_LOGV(TAG, "TX>   posted: %s", TRUEFALSE(posted));
}

void KdkConnectionManager::send_response(uint8_t counter, uint16_t command, const uint8_t *payload, uint16_t length) {
  struct KdkMsg *cmd = (struct KdkMsg *) this->tx_.buffer;

  cmd->start = KDK_MESSAGE_START;
  cmd->dummy = 0;
  cmd->counter = counter;  // Reply with RX counter
  cmd->command = command;
  cmd->length = length;

  if ((payload != NULL) && (length > 0)) {
    memcpy(cmd->payload, payload, length);
  }

  size_t buf_length = sizeof(struct KdkMsg) + cmd->length;  // Length of bytes to compute the checksum
  uint8_t checksum = this->calculate_sum(this->tx_.buffer, buf_length);
  this->tx_.buffer[buf_length++] = checksum;  // Add checksum to the final buffer length
  this->tx_.buffer_length = buf_length;
  this->transmit_message();

  ESP_LOGV(TAG, "TX> Send Response:");
  ESP_LOGV(TAG, "TX>   counter: %d", cmd->counter);
  ESP_LOGV(TAG, "TX>   command: %04X", cmd->command);
  ESP_LOGV(TAG, "TX>   length: %d", cmd->length);
  ESP_LOGV(TAG, "TX>   payload: %s", this->hex2str(cmd->payload, cmd->length).c_str());
  ESP_LOGV(TAG, "TX>   checksum: %02X", checksum);
}

void KdkConnectionManager::transmit_message(void) {
  this->write_array(this->tx_.buffer, this->tx_.buffer_length);
  this->tx_.timestamp = millis();
}

void KdkConnectionManager::check_response_timeout(const uint32_t now) {
  // Clear the waiting response state if the last request was a long time ago
  if (!this->state_.waiting_response) {
    return;
  }

  const uint32_t elapsed = (now - this->tx_.timestamp);
  if (elapsed < this->cfg_.receive_timeout) {
    return;
  }

  ESP_LOGW(TAG, "RX> Response timeout after %d ms", elapsed);

  if (this->tx_.retry_count < KDK_SEND_MAX_RETRY) {
    this->tx_.retry_count++;
    ESP_LOGW(TAG, "TX> Retransmit message, retry #%d", this->tx_.retry_count);
    this->transmit_message();
  } else {
    ESP_LOGE(TAG, "TX> Retransmit retries exhausted, attempt recovery");
    this->state_.waiting_response = false;
    this->fsm_push_event(KdkCommFsmEvent::KDK_COMM_FSM_EVENT_SYNC_RECOVERY);
  }
}

/*******************************************************************************
 * PROTECTED - Message Helpers
 ******************************************************************************/

bool KdkConnectionManager::is_message_response(uint16_t command) {
  if (!this->is_message_pending()) {
    return false;
  }

  const struct KdkMsg *msg = this->message();

  if (!IS_RESPONSE_MESSAGE(msg->command)) {
    return false;
  }

  if (GET_COMMAND(msg->command) != command) {
    return false;
  }

  if (msg->counter != this->tx_.counter) {
    ESP_LOGW(TAG, "MSG> Received expected message but wrong counter: got=%d, exp=%d", msg->counter, this->tx_.counter);
    return false;
  }

  return true;
}

void KdkConnectionManager::save_parameter_table_id(const uint8_t *buffer) {
  // 3-bytes in little-endian byte order
  this->state_.parameter_table_id = buffer[0] | (buffer[1] << 8) | (buffer[2] << 16);
}

void KdkConnectionManager::fill_parameter_table_id(uint8_t *buffer) {
  // 3-bytes in little-endian byte order
  auto id = this->state_.parameter_table_id;
  buffer[0] = (id >> 0) & 0xFF;
  buffer[1] = (id >> 8) & 0xFF;
  buffer[2] = (id >> 16) & 0xFF;
}

void KdkConnectionManager::fill_parameter_requests(uint8_t *buffer, std::vector<uint16_t> id_list) {
  if (id_list.size() > 80) {
    ESP_LOGE(TAG, "CMD> fill_parameter_requests list size %d is larger than 80", id_list.size());
    return;
  }

  this->fill_parameter_table_id(&buffer[0]);  // 3-bytes
  buffer[3] = id_list.size();

  for (int i = 0; i < id_list.size(); i++) {
    auto id = id_list[i];
    buffer[4 + ((i * KDK_MSG_PARAM_ID_REQ_SIZE) + 0)] = ((id >> 0) & 0xFF);
    buffer[4 + ((i * KDK_MSG_PARAM_ID_REQ_SIZE) + 1)] = ((id >> 8) & 0xFF);
    buffer[4 + ((i * KDK_MSG_PARAM_ID_REQ_SIZE) + 2)] = 0;  // Appears to be always 0
  }
}

/**
 * Parse parameter response list from the provided buffer.
 * Expect the buffer to start from 'count'.
 */
void KdkConnectionManager::parse_parameter_response(const uint8_t *buffer) {
  uint16_t index = 0;  // Buffer index, assumes 'buffer' starts from 'count'
  uint8_t count = buffer[index++];
  if (count == 0) {
    return;  // Invalid buffer
  }

  // Loop through each param
  for (int i = 0; i < count; i++) {
    // Parse parameter entry
    uint16_t id_lsb = buffer[index++];
    uint16_t id_msb = buffer[index++];
    uint16_t id = (uint16_t) (id_lsb | (id_msb << 8));
    uint8_t length = buffer[index++];

    auto &parameters = this->state_.parameters;
    auto it = parameters.find(id);
    if (it == parameters.end()) {
      ESP_LOGW(TAG, "PARAM> Failed to find parameter ID %04X", id);
      index += length;  // Advance the buffer index by the data length to process the next entry
      continue;
    }

    auto &param = it->second;

    if (param.data.size() != length) {
      ESP_LOGW(TAG, "PARAM> Parameter %04X size mismatch : got=%d, exp=%d", id, length, param.data.size());
      index += length;  // Advance the buffer index by the data length to process the next entry
      continue;
    }

    for (int j = 0; j < length; j++) {
      param.data[j] = buffer[index++];
    }

    ESP_LOGD(TAG, "PARAM> GET ID=%04X, SIZE=%d, DATA=%s", id, length, this->hex2str(param.data).c_str());
  }
}

/*******************************************************************************
 * PROTECTED - RESPONSE HANDLERS
 ******************************************************************************/

void KdkConnectionManager::process_response_default(uint16_t command) {
  if (!this->is_message_response(command)) {
    return;
  }

  // No payload processing, continue as long as a valid response for the command is received.
  this->fsm_push_event(KDK_COMM_FSM_EVENT_RESPONSE_RECEIVED);
  this->clear_message_pending();
}

void KdkConnectionManager::process_response_1100(void) {
  if (!this->is_message_response(0x1100)) {
    return;
  }

  /* Captured response from FAN to MOD:
   * 5A 03 00 91 00 29 // Header (not part of 'payload')
   * 00                // Status?
   * 00 01 00 01 0A 16 // Entry 1 Header? Last byte is the length
   * 4B 31 32 55 43 2B 56 42 48 48 2D 47 59 32 34 32 32 30 30 31 32 36 // Model + Serial Number
   * 0B 02 01 2C 0C 06 // Entry 2 Header? Last byte is the length
   * 46 4D 31 32 47 43 // Panasonic variant of the fan
   * D2                // Checksum
   */

  auto &payload = this->message()->payload;

  std::string info_string(reinterpret_cast<char const *>(&payload[7]), payload[6]);
  ESP_LOGV(TAG, "CMD1100> info_string=%s", info_string.c_str());

  int pos = info_string.find("+");

  this->state_.product_model = info_string.substr(0, pos);
  ESP_LOGV(TAG, "CMD1100> product_model=%s", this->state_.product_model.c_str());

  this->state_.product_serial = info_string.substr(pos + 1);
  ESP_LOGV(TAG, "CMD1100> product_serial=%s", this->state_.product_serial.c_str());

  this->fsm_push_event(KDK_COMM_FSM_EVENT_RESPONSE_RECEIVED);
  this->clear_message_pending();
}

void KdkConnectionManager::process_response_0010(void) {
  if (!this->is_message_response(0x0010)) {
    return;
  }

  /* Captured response from FAN to MOD:
   * 5A 07 10 80 00 05 // Header (not part of 'payload')
   * 00                // Status?
   * 01                // ???
   * 01 3A 01          // Parameter Table ID
   * CD                // Checksum
   */

  auto &payload = this->message()->payload;

  // Not sure what these 3-bytes mean, but they are sent with all subsequent requests.
  // Store all 3-bytes in little-endian byte order and use them as is.
  this->save_parameter_table_id(&payload[2]);
  ESP_LOGV(TAG, "CMD0010> parameter_table_id=0x%06X", this->state_.parameter_table_id);

  this->fsm_push_event(KDK_COMM_FSM_EVENT_RESPONSE_RECEIVED);
  this->clear_message_pending();
}

void KdkConnectionManager::process_response_0110(void) {
  if (!this->is_message_response(0x0110)) {
    return;
  }

  /* Captured response from FAN to MOD (truncated):
   * 5A 08 10 81 00 8A // Header (not part of 'payload')
   * 00                // Status?
   * 01 3A 01          // Parameter Table ID
   * 00 01             // ???
   * 00 01             // ???
   * 00 20             // Count
   * 00 80 E2 01       // Each entry is 4 bytes, refer to `struct KdkParam`
   * 00 81 E2 01       //  - 2-byte : id
   * 00 82 40 04       //  - 1-byte : some kind of metadata
   * 00 86 62 2E       //  - 1-byte : data size
   * . . . . (truncated, showing only 4 entries out of 32)
   * 62                // Checksum
   */

  auto &payload = this->message()->payload;

  uint16_t count = (uint16_t) ((payload[8] << 8) | payload[9]);
  ESP_LOGV(TAG, "CMD0110> count=%d", count);

  this->state_.parameters.clear();

  for (int i = 0; i < count; i++) {
    auto param_buffer = &payload[10 + (i * 4)];
    auto param_id = (uint16_t) (param_buffer[0] | (param_buffer[1] << 8));  // 2-bytes
    auto param_metadata = param_buffer[2];                                  // 1-byte
    auto param_size = param_buffer[3];

    struct KdkParam param = {
        .id = param_id,                                  // 2-bytes
        .metadata = param_metadata,                      // 1-byte
        .size = param_size,                              // 1-byte
        .data = std::vector<uint8_t>(param_size, 0x55),  // Data size is defined by the 1-byte size
    };

    this->state_.parameters.insert({param.id, param});
    ESP_LOGV(TAG, "CMD0110> PARAM %-2d : ID=%04X, SIZE=%-3d, META=%02X", i, param.id, param.size, param.metadata);
  }

  this->fsm_push_event(KDK_COMM_FSM_EVENT_RESPONSE_RECEIVED);
  this->clear_message_pending();
}

void KdkConnectionManager::process_response_0210(void) {
  if (!this->is_message_response(0x0210)) {
    return;
  }

  /* Captured response from FAN to MOD:
   * 5A 09 10 82 00 43 // Header (not part of 'payload')
   * 00                // Status?
   * 01 3A 01          // Parameter Table ID
   * 05                // Count
   * 00 82 04 00 00 4C 00 // Entry 1 - 2-byte ID, 1-byte length, followed by data
   * 00 8A 03 00 00 FE    // Entry 2
   * 00 9D 06 05 80 81 86 88 F3
   * 00 9E 11 10 81 81 80 82 80 80 80 80 80 00 80 00 80 80 80 00
   * 00 9F 11 1A 81 81 81 82 80 80 81 80 81 80 81 80 81 82 82 02
   * E4 // Checksum
   */

  auto &payload = this->message()->payload;

  this->parse_parameter_response(&payload[4]);

  this->fsm_push_event(KDK_COMM_FSM_EVENT_RESPONSE_RECEIVED);
  this->clear_message_pending();
}

void KdkConnectionManager::process_response_0910(void) {
  if (!this->is_message_response(0x0910)) {
    return;
  }

  /* Captured response from FAN to MOD:
   * 5A 23 10 89 00 76 // Header (not part of 'payload')
   * 00                // Status?
   * 01 3A 01          // Parameter Table ID
   * 0F                // Count
   * 00 80 01 30       // Entry 1 - 2-byte ID, 1-byte length, followed by data
   * 00 F0 01 32       // Entry 2
   * . . . (truncated, showing 2 out of 15 entries)
   * F6                // Checksum
   */

  auto &payload = this->message()->payload;

  this->parse_parameter_response(&payload[4]);

  this->fsm_push_event(KDK_COMM_FSM_EVENT_RESPONSE_RECEIVED);
  this->clear_message_pending();
}

/*******************************************************************************
 * PROTECTED - MESSAGE BUILDER
 ******************************************************************************/

void KdkConnectionManager::send_message_0810(std::vector<struct KdkParamUpdate> values) {
  /* Captured request from MOD to FAN:
   * 5A 21 10 08 00 2C // Header (not part of 'payload')
   * 02                // Type??? (not sure what this means, seems to always be 0x2)
   * 01 3A 01          // Parameter Table ID
   * 09                // Count
   * 00 FD 01 03       // Entry 1
   * 00 FC 01 30       // Entry 2
   * . . . (truncated, showing only 2 out of 9 entries)
   * E1                // Checksum
   */

  std::map<uint16_t, std::vector<uint8_t>> param_update_map;  // Unique parameter updates to be sent

  // Validate parameters
  auto &parameters = this->state_.parameters;

  for (auto &value : values) {
    auto id = value.id;
    auto it = parameters.find(id);
    if (it == parameters.end()) {
      ESP_LOGW(TAG, "0810> Failed to find parameter ID %04X", id);
      return;
    }
    auto size = it->second.size;
    if (size != value.data.size()) {
      ESP_LOGW(TAG, "0810> Failed to update parameter %04X, data size mismatch: got=%d, exp=%d", id, value.data.size(),
               size);
      return;
    }

    // Update if duplicate, else insert
    auto param_it = param_update_map.find(id);
    if (param_it != param_update_map.end()) {
      param_it->second = value.data;  // Overwrite existing data
    } else {
      param_update_map.insert({id, value.data});  // Insert new parameter update
    }
  }

  std::vector<uint8_t> payload(KDK_MSG_TYPE_SIZE + KDK_MSG_TABLE_ID_SIZE + KDK_MSG_PARAM_COUNT_SIZE);

  payload[0] = 0x02;
  this->fill_parameter_table_id(&payload[1]);  // 3-bytes
  payload[4] = values.size();

  for (auto values_it = param_update_map.begin(); values_it != param_update_map.end(); values_it++) {
    auto id = values_it->first;
    auto &value = values_it->second;
    payload.push_back((id >> 0) & 0xFF);
    payload.push_back((id >> 8) & 0xFF);
    payload.push_back((uint8_t) (value.size() & 0xFF));
    for (auto b : value) {
      payload.push_back(b);
    }

    ESP_LOGD(TAG, "PARAM> SET ID=%04X, SIZE=%d, DATA=%s", id, value.size(), this->hex2str(value).c_str());
  }

  this->send_request(0x0810, payload.data(), payload.size());
}

void KdkConnectionManager::send_message_0910(std::vector<uint16_t> id_list) {
  /* Captured request from MOD to FAN:
   * 5A 20 10 09 00 32 // Header (not part of 'payload')
   * 02                // Type??? (not sure what this means, seems to always be 0x2)
   * 01 3A 01          // Parameter Table ID
   * 0F                // Count
   * 00 80 00          // Entry 1
   * 00 F0 00          // Entry 2
   * . . . (truncated, showing only 2 out of 15 entries)
   * DE                // Checksum
   */

  std::vector<uint8_t> payload(KDK_MSG_TYPE_SIZE + KDK_MSG_TABLE_ID_SIZE + KDK_MSG_PARAM_COUNT_SIZE +
                               (KDK_MSG_PARAM_ID_REQ_SIZE * id_list.size()));

  payload[0] = 0x02;
  this->fill_parameter_requests(&payload[1], id_list);
  this->send_request(0x0910, payload.data(), payload.size());
}

void KdkConnectionManager::send_message_0210_init(void) {
  // Read? parameters with metadata = 0x40

  /* Captured request from MOD to FAN:
   * 5A 09 10 02 00 13 // Header (not part of 'payload')
   * 01 3A 01          // Parameter Table ID
   * 05                // Count
   * 00 82 00          // Entry 1
   * 00 8A 00          // Entry 2
   * 00 9D 00          // Entry 3
   * 00 9E 00          // Entry 4
   * 00 9F 00          // Entry 5
   * 51                // Checksum
   */

  // Get list of IDs with metadata that is 0x40
  std::vector<uint16_t> id_list;
  for (auto i : this->state_.parameters) {
    auto param = i.second;
    if (param.metadata == 0x40) {
      id_list.push_back(param.id);
    }
  }

  std::vector<uint8_t> payload(KDK_MSG_TABLE_ID_SIZE + KDK_MSG_PARAM_COUNT_SIZE +
                               (KDK_MSG_PARAM_ID_REQ_SIZE * id_list.size()));

  this->fill_parameter_requests(&payload[0], id_list);
  this->send_request(0x0210, payload.data(), payload.size());
}

void KdkConnectionManager::send_message_0910_init(void) {
  // List of IDs that are not polled periodically
  std::vector<uint16_t> id_list{
      0x8100,  //
      0x8600,  //
      0x8C00,  //
      0x9300,  //
      0xFC00,  //
      0xFD00,  //
      0xFE00,  //
      0xF001,  //
      0xF101,  //
      0xF201,  //
      0xF301,  //
      0xF401,  //
      0xF501,  //
  };

  this->send_message_0910(id_list);
}

void KdkConnectionManager::send_message_0910_poll(void) {
  // List of IDs to poll
  std::vector<uint16_t> id_list{
      0x8000,  // Fan ON/OFF
      0xF000,  // Fan Speed
      // 0x8600,  // UNKNOWN, large 46-bytes parameter
      0x8800,  // UNKNOWN, always 0x42
      0xF800,  // Some kind of parameter mask, sent when changing the fan states
      0xF200,  // Fan Yuragi
      0xF100,  // Fan Direction
      0xF900,  // UNKNOWN, always 0x00 0x00
      0xFA00,  // Some kind of parameter mask, sent when changing the fan states
      0xFB00,  // UNKNOWN, always 0x00 0x00
      0xF300,  // Light ON/OFF
      0xF500,  // Light Brightness
      0xF400,  // Light Mode
      0xF700,  // NightLight Brightness
      0xF600,  // Light Color
  };

  this->send_message_0910(id_list);
}

/*******************************************************************************
 * PROTECTED - Message Handler
 ******************************************************************************/

/**
 * @brief Command 0101 message handler.
 * This message is received when the device want to know the module's status.
 * Observed to be every 600s. Return a status that it is happy with.
 */
void KdkConnectionManager::process_message_0101(const KdkMsg *msg) {
  const uint8_t counter = msg->counter;
  const uint16_t command = SET_RESPONSE_MESSAGE(msg->command);
  uint8_t payload[] = {0x00, 0x11, 0x13};
  this->send_response(counter, command, payload, sizeof(payload));
  this->state_.last_ping_timestamp = millis();
}

/**
 * @brief Request 0A10 message handler.
 * This message is received when the device want to notify the module that a
 * parameter value has changed. This is only received when major states have
 * changed, for example, when the fan/light is turned on/off. It does not notify
 * on minor changes such as fan speed change or light brightness change.
 */
void KdkConnectionManager::process_message_0A10(const KdkMsg *msg) {
  /* Captured request from MOD to FAN:
   * 5A 0C 10 0A 00 09 // Header (not part of 'payload')
   * 00                // Status?
   * 01 3A 01          // Parameter Table ID
   * 01                // Count
   * 00 F3 01 30       // Entry 1: 2-Byte ID, 1-Byte size and 1-Byte data
   * 16                // Checksum
   */

  auto &payload = msg->payload;

  // Parse new state(s)
  this->parse_parameter_response(&payload[4]);

  // Send response
  const uint8_t counter = msg->counter;
  const uint16_t command = SET_RESPONSE_MESSAGE(msg->command);
  this->send_response(counter, command, &payload[0], 4);  // Return the Status? and Parameter Table ID

  // Pull the other states
  this->fsm_push_event(KdkCommFsmEvent::KDK_COMM_FSM_EVENT_PULL_STATES);
}

void KdkConnectionManager::process_message(void) {
  if (!this->is_message_pending()) {
    return;
  }

  auto msg = this->message();

  if (IS_RESPONSE_MESSAGE(msg->command) && !this->is_waiting_response()) {
    // Log warning if we are seeing a RESPONSE message,
    // they should always be handled by the FSM earlier.
    ESP_LOGW(TAG, "MSG> Unhandled RESPONSE CMD=%04X", msg->command);
    return;
  }

  // Always clear to unblock new message reception
  this->clear_message_pending();

  switch (msg->command) {
    case 0x0101:
      return this->process_message_0101(msg);
    case 0x0A10:
      return this->process_message_0A10(msg);
    default:
      ESP_LOGW(TAG, "MSG> Unhandled MESSAGE CMD=%04X", msg->command);
  }
}

/*******************************************************************************
 * PROTECTED - FSM
 ******************************************************************************/

void KdkConnectionManager::fsm_push_event(KdkCommFsmEvent event) {
  // Push event to the queue
  this->fsm_.event_queue.push(event);
}

void KdkConnectionManager::fsm_run(void) {
  // FSM cannot transition while waiting for a response
  if (this->is_waiting_response()) {
    return;
  }

  while (!this->fsm_.event_queue.empty()) {
    KdkCommFsmState curr_state = this->fsm_.state;
    KdkCommFsmEvent event = this->fsm_.event_queue.front();
    KdkCommFsmState next_state = this->fsm_next_state(curr_state, event);
    this->fsm_.event_queue.pop();

    if (next_state == KdkCommFsmState::KDK_COMM_STATE_NONE) {
      continue;
    }

    if (next_state != curr_state) {
      ESP_LOGD(TAG, "FSM> EVENT: %s", this->get_event_name(event).c_str());
      this->fsm_state_handlers(KdkCommFsmMethod::KDK_COMM_FSM_EXIT);
      this->fsm_.state = next_state;
    }

    this->fsm_state_handlers(KdkCommFsmMethod::KDK_COMM_FSM_ENTRY);
  }

  this->fsm_state_handlers(KdkCommFsmMethod::KDK_COMM_FSM_LOOP);
}

KdkCommFsmState KdkConnectionManager::fsm_next_state(KdkCommFsmState state, KdkCommFsmEvent event) {
  // Special handling for SYNC
  if ((event == KdkCommFsmEvent::KDK_COMM_FSM_EVENT_SYNC_RECEIVED) ||
      (event == KdkCommFsmEvent::KDK_COMM_FSM_EVENT_SYNC_RECOVERY)) {
    return KdkCommFsmState::KDK_COMM_STATE_INIT_SYNC;
  }

  switch (state) {
    case KdkCommFsmState::KDK_COMM_STATE_NONE: {
      ESP_LOGE(TAG, "FSM> KDK_COMM_STATE_NONE should never be entered!");
    } break;

    case KdkCommFsmState::KDK_COMM_STATE_UNINITIALIZED: {
      if (event == KdkCommFsmEvent::KDK_COMM_FSM_EVENT_SYNC_TIMEOUT) {
        return KdkCommFsmState::KDK_COMM_STATE_INIT_SYNC;
      }
    } break;

    case KdkCommFsmState::KDK_COMM_STATE_INIT_SYNC: {
      if (event == KdkCommFsmEvent::KDK_COMM_FSM_EVENT_SYNC_OK) {
        return KdkCommFsmState::KDK_COMM_STATE_INIT_0C00;
      }
    } break;

    case KdkCommFsmState::KDK_COMM_STATE_INIT_0C00: {
      if (event == KdkCommFsmEvent::KDK_COMM_FSM_EVENT_RESPONSE_RECEIVED) {
        return KdkCommFsmState::KDK_COMM_STATE_INIT_1000;
      }
    } break;

    case KdkCommFsmState::KDK_COMM_STATE_INIT_1000: {
      if (event == KdkCommFsmEvent::KDK_COMM_FSM_EVENT_RESPONSE_RECEIVED) {
        return KdkCommFsmState::KDK_COMM_STATE_INIT_1100;
      }
    } break;

    case KdkCommFsmState::KDK_COMM_STATE_INIT_1100: {
      if (event == KdkCommFsmEvent::KDK_COMM_FSM_EVENT_RESPONSE_RECEIVED) {
        return KdkCommFsmState::KDK_COMM_STATE_INIT_1200;
      }
    } break;

    case KdkCommFsmState::KDK_COMM_STATE_INIT_1200: {
      if (event == KdkCommFsmEvent::KDK_COMM_FSM_EVENT_RESPONSE_RECEIVED) {
        return KdkCommFsmState::KDK_COMM_STATE_INIT_4100;
      }
    } break;

    case KdkCommFsmState::KDK_COMM_STATE_INIT_4100: {
      if (event == KdkCommFsmEvent::KDK_COMM_FSM_EVENT_RESPONSE_RECEIVED) {
        return KdkCommFsmState::KDK_COMM_STATE_INIT_4C01;
      }
    } break;

    case KdkCommFsmState::KDK_COMM_STATE_INIT_4C01: {
      if (event == KdkCommFsmEvent::KDK_COMM_FSM_EVENT_RESPONSE_RECEIVED) {
        return KdkCommFsmState::KDK_COMM_STATE_INIT_0010;
      }
    } break;

    case KdkCommFsmState::KDK_COMM_STATE_INIT_0010: {
      if (event == KdkCommFsmEvent::KDK_COMM_FSM_EVENT_RESPONSE_RECEIVED) {
        return KdkCommFsmState::KDK_COMM_STATE_INIT_0110;
      }
    } break;

    case KdkCommFsmState::KDK_COMM_STATE_INIT_0110: {
      if (event == KdkCommFsmEvent::KDK_COMM_FSM_EVENT_RESPONSE_RECEIVED) {
        return KdkCommFsmState::KDK_COMM_STATE_INIT_0210;
      }
    } break;

    case KdkCommFsmState::KDK_COMM_STATE_INIT_0210: {
      if (event == KdkCommFsmEvent::KDK_COMM_FSM_EVENT_RESPONSE_RECEIVED) {
        return KdkCommFsmState::KDK_COMM_STATE_INIT_1800;
      }
    } break;

    case KdkCommFsmState::KDK_COMM_STATE_INIT_1800: {
      if (event == KdkCommFsmEvent::KDK_COMM_FSM_EVENT_RESPONSE_RECEIVED) {
        return KdkCommFsmState::KDK_COMM_STATE_INIT_0001_10;
      }
    } break;

    case KdkCommFsmState::KDK_COMM_STATE_INIT_0001_10: {
      if (event == KdkCommFsmEvent::KDK_COMM_FSM_EVENT_RESPONSE_RECEIVED) {
        return KdkCommFsmState::KDK_COMM_STATE_INIT_0001_11;
      }
    } break;

    case KdkCommFsmState::KDK_COMM_STATE_INIT_0001_11: {
      if (event == KdkCommFsmEvent::KDK_COMM_FSM_EVENT_RESPONSE_RECEIVED) {
        return KdkCommFsmState::KDK_COMM_STATE_INIT_0910;
      }
    } break;

    case KdkCommFsmState::KDK_COMM_STATE_INIT_0910: {
      if (event == KdkCommFsmEvent::KDK_COMM_FSM_EVENT_RESPONSE_RECEIVED) {
        return KdkCommFsmState::KDK_COMM_STATE_INIT_DONE;
      }
    } break;

    case KdkCommFsmState::KDK_COMM_STATE_INIT_DONE: {
      if (event == KdkCommFsmEvent::KDK_COMM_FSM_EVENT_INIT_DONE) {
        return KdkCommFsmState::KDK_COMM_STATE_PULL_STATES_0910; /* Pull initial state */
      }
    } break;

    case KdkCommFsmState::KDK_COMM_STATE_IDLE: {
      if (event == KdkCommFsmEvent::KDK_COMM_FSM_EVENT_PULL_STATES) {
        return KdkCommFsmState::KDK_COMM_STATE_PULL_STATES_0910;
      }
      if (event == KdkCommFsmEvent::KDK_COMM_FSM_EVENT_PUSH_STATES) {
        return KdkCommFsmState::KDK_COMM_STATE_PUSH_STATES_0810;
      }
    } break;

    case KdkCommFsmState::KDK_COMM_STATE_PULL_STATES_0910: {
      if (event == KdkCommFsmEvent::KDK_COMM_FSM_EVENT_RESPONSE_RECEIVED) {
        return KdkCommFsmState::KDK_COMM_STATE_IDLE;
      }
    } break;

    case KdkCommFsmState::KDK_COMM_STATE_PUSH_STATES_0810: {
      if (event == KdkCommFsmEvent::KDK_COMM_FSM_EVENT_RESPONSE_RECEIVED) {
        return KdkCommFsmState::KDK_COMM_STATE_IDLE;
      }
    } break;
  }

  return KDK_COMM_STATE_NONE;
}

void KdkConnectionManager::fsm_state_handlers(KdkCommFsmMethod method) {
  const uint32_t now = millis();
  KdkCommFsmState state = this->fsm_.state;

  if (method != KdkCommFsmMethod::KDK_COMM_FSM_LOOP) {
    ESP_LOGD(TAG, "FSM>  %s: %s", this->get_state_name(state).c_str(), this->get_method_name(method).c_str());
  }

  switch (state) {
    case KdkCommFsmState::KDK_COMM_STATE_NONE: {
      ESP_LOGE(TAG, "FSM> KDK_COMM_STATE_NONE should never be entered!");
    } break;
    case KdkCommFsmState::KDK_COMM_STATE_UNINITIALIZED: {
      // Waiting for SYNC message
      switch (method) {
        case KdkCommFsmMethod::KDK_COMM_FSM_ENTRY: {
        } break;
        case KdkCommFsmMethod::KDK_COMM_FSM_LOOP: {
          if (now > KDK_WAIT_SYNC_TIMEOUT) {
            ESP_LOGW(TAG, "FSM> Wait SYNC timeout after %d ms", now);
            this->fsm_push_event(KdkCommFsmEvent::KDK_COMM_FSM_EVENT_SYNC_TIMEOUT);
          }
        } break;
        case KdkCommFsmMethod::KDK_COMM_FSM_EXIT: {
        } break;
      }

    } break;

    case KdkCommFsmState::KDK_COMM_STATE_INIT_SYNC: {
      switch (method) {
        case KdkCommFsmMethod::KDK_COMM_FSM_ENTRY: {
          this->tx_.counter = 0xFF;
          this->tx_.retry_pending = false;
          this->tx_.retry_count = 0;
          this->tx_.buffer_length = 0;

          this->receiver_reset_states();
          this->rx_.counter = 0;

          this->state_.waiting_response = false;
          this->state_.message_pending = false;

          this->send_request(0x0600, NULL, 0, true);  // No response expected
        } break;
        case KdkCommFsmMethod::KDK_COMM_FSM_LOOP: {
          this->fsm_push_event(KDK_COMM_FSM_EVENT_SYNC_OK);
        } break;
        case KdkCommFsmMethod::KDK_COMM_FSM_EXIT: {
        } break;
      }
    } break;

    case KdkCommFsmState::KDK_COMM_STATE_INIT_0C00: {
      switch (method) {
        case KdkCommFsmMethod::KDK_COMM_FSM_ENTRY: {
          this->send_request(0x0C00, NULL, 0);
        } break;
        case KdkCommFsmMethod::KDK_COMM_FSM_LOOP: {
          this->process_response_default(0x0C00);
        } break;
        case KdkCommFsmMethod::KDK_COMM_FSM_EXIT: {
        } break;
      }
    } break;

    case KdkCommFsmState::KDK_COMM_STATE_INIT_1000: {
      switch (method) {
        case KdkCommFsmMethod::KDK_COMM_FSM_ENTRY: {
          uint8_t payload[] = {0x20};
          this->send_request(0x1000, payload, sizeof(payload));
        } break;
        case KdkCommFsmMethod::KDK_COMM_FSM_LOOP: {
          this->process_response_default(0x1000);
        } break;
        case KdkCommFsmMethod::KDK_COMM_FSM_EXIT: {
        } break;
      }
    } break;

    case KdkCommFsmState::KDK_COMM_STATE_INIT_1100: {  // Device Info
      switch (method) {
        case KdkCommFsmMethod::KDK_COMM_FSM_ENTRY: {
          uint8_t payload[] = {0x00, 0x01};
          this->send_request(0x1100, payload, sizeof(payload));
        } break;
        case KdkCommFsmMethod::KDK_COMM_FSM_LOOP: {
          this->process_response_1100();
        } break;
        case KdkCommFsmMethod::KDK_COMM_FSM_EXIT: {
        } break;
      }
    } break;

    case KdkCommFsmState::KDK_COMM_STATE_INIT_1200: {
      switch (method) {
        case KdkCommFsmMethod::KDK_COMM_FSM_ENTRY: {
          uint8_t payload[] = {0x01, 0x10, 0x11, 0x12, 0x13, 0x14};
          this->send_request(0x1200, payload, sizeof(payload));
        } break;
        case KdkCommFsmMethod::KDK_COMM_FSM_LOOP: {
          this->process_response_default(0x1200);
        } break;
        case KdkCommFsmMethod::KDK_COMM_FSM_EXIT: {
        } break;
      }
    } break;

    case KdkCommFsmState::KDK_COMM_STATE_INIT_4100: {
      switch (method) {
        case KdkCommFsmMethod::KDK_COMM_FSM_ENTRY: {
          this->send_request(0x4100, NULL, 0);
        } break;
        case KdkCommFsmMethod::KDK_COMM_FSM_LOOP: {
          this->process_response_default(0x4100);
        } break;
        case KdkCommFsmMethod::KDK_COMM_FSM_EXIT: {
        } break;
      }
    } break;

    case KdkCommFsmState::KDK_COMM_STATE_INIT_4C01: {
      switch (method) {
        case KdkCommFsmMethod::KDK_COMM_FSM_ENTRY: {
          this->send_request(0x4C01, NULL, 0);
        } break;
        case KdkCommFsmMethod::KDK_COMM_FSM_LOOP: {
          this->process_response_default(0x4C01);
        } break;
        case KdkCommFsmMethod::KDK_COMM_FSM_EXIT: {
        } break;
      }
    } break;

    case KdkCommFsmState::KDK_COMM_STATE_INIT_0010: {
      // Get parameter table ID?
      switch (method) {
        case KdkCommFsmMethod::KDK_COMM_FSM_ENTRY: {
          this->send_request(0x0010, NULL, 0);
        } break;
        case KdkCommFsmMethod::KDK_COMM_FSM_LOOP: {
          this->process_response_0010();
        } break;
        case KdkCommFsmMethod::KDK_COMM_FSM_EXIT: {
        } break;
      }
    } break;

    case KdkCommFsmState::KDK_COMM_STATE_INIT_0110: {  // Get list of supported parameters from table?
      switch (method) {
        case KdkCommFsmMethod::KDK_COMM_FSM_ENTRY: {
          uint8_t payload[] = {0x00, 0x00, 0x00, 0x00, 0x01};
          this->fill_parameter_table_id(&payload[0]);
          this->send_request(0x0110, payload, sizeof(payload));
        } break;
        case KdkCommFsmMethod::KDK_COMM_FSM_LOOP: {
          this->process_response_0110();
        } break;
        case KdkCommFsmMethod::KDK_COMM_FSM_EXIT: {
        } break;
      }
    } break;

    case KdkCommFsmState::KDK_COMM_STATE_INIT_0210: {  // Read? parameters with metadata = 0x40
      switch (method) {
        case KdkCommFsmMethod::KDK_COMM_FSM_ENTRY: {
          this->send_message_0210_init();
        } break;
        case KdkCommFsmMethod::KDK_COMM_FSM_LOOP: {
          this->process_response_0210();
        } break;
        case KdkCommFsmMethod::KDK_COMM_FSM_EXIT: {
        } break;
      }
    } break;

    case KdkCommFsmState::KDK_COMM_STATE_INIT_1800: {
      switch (method) {
        case KdkCommFsmMethod::KDK_COMM_FSM_ENTRY: {
          this->send_request(0x1800, NULL, 0);
        } break;
        case KdkCommFsmMethod::KDK_COMM_FSM_LOOP: {
          this->process_response_default(0x1800);
        } break;
        case KdkCommFsmMethod::KDK_COMM_FSM_EXIT: {
        } break;
      }
    } break;

    case KdkCommFsmState::KDK_COMM_STATE_INIT_0001_10: {  // Publish module status? 0x10
      switch (method) {
        case KdkCommFsmMethod::KDK_COMM_FSM_ENTRY: {
          uint8_t payload[] = {0x10};
          this->send_request(0x0001, payload, sizeof(payload));
        } break;
        case KdkCommFsmMethod::KDK_COMM_FSM_LOOP: {
          this->process_response_default(0x0001);
        } break;
        case KdkCommFsmMethod::KDK_COMM_FSM_EXIT: {
        } break;
      }
    } break;

    case KdkCommFsmState::KDK_COMM_STATE_INIT_0001_11: {  // Publish module status? 0x11
      switch (method) {
        case KdkCommFsmMethod::KDK_COMM_FSM_ENTRY: {
          uint8_t payload[] = {0x11};
          this->send_request(0x0001, payload, sizeof(payload));
        } break;
        case KdkCommFsmMethod::KDK_COMM_FSM_LOOP: {
          this->process_response_default(0x0001);
        } break;
        case KdkCommFsmMethod::KDK_COMM_FSM_EXIT: {
        } break;
      }
    } break;

    case KdkCommFsmState::KDK_COMM_STATE_INIT_0910: {
      switch (method) {
        case KdkCommFsmMethod::KDK_COMM_FSM_ENTRY: {
          this->send_message_0910_init();
        } break;
        case KdkCommFsmMethod::KDK_COMM_FSM_LOOP: {
          this->process_response_0910();
        } break;
        case KdkCommFsmMethod::KDK_COMM_FSM_EXIT: {
        } break;
      }
    } break;

    case KdkCommFsmState::KDK_COMM_STATE_INIT_DONE: {
      switch (method) {
        case KdkCommFsmMethod::KDK_COMM_FSM_ENTRY: {
          ESP_LOGI(TAG, "KDK> Module Initialized!");
          this->fsm_push_event(KdkCommFsmEvent::KDK_COMM_FSM_EVENT_INIT_DONE);
        } break;
        case KdkCommFsmMethod::KDK_COMM_FSM_LOOP: {
        } break;
        case KdkCommFsmMethod::KDK_COMM_FSM_EXIT: {
        } break;
      }
    } break;

    case KdkCommFsmState::KDK_COMM_STATE_IDLE: {
      switch (method) {
        case KdkCommFsmMethod::KDK_COMM_FSM_ENTRY: {
        } break;
        case KdkCommFsmMethod::KDK_COMM_FSM_LOOP: {
          if (this->is_update_pending()) {
            this->fsm_push_event(KdkCommFsmEvent::KDK_COMM_FSM_EVENT_PUSH_STATES);
            return;
          }

          const uint32_t elapsed = (now - this->state_.last_update_timestamp);
          if (elapsed > this->cfg_.poll_interval) {
            this->fsm_push_event(KdkCommFsmEvent::KDK_COMM_FSM_EVENT_PULL_STATES);
            return;
          }
        } break;
        case KdkCommFsmMethod::KDK_COMM_FSM_EXIT: {
        } break;
      }
    } break;

    case KdkCommFsmState::KDK_COMM_STATE_PULL_STATES_0910: {
      switch (method) {
        case KdkCommFsmMethod::KDK_COMM_FSM_ENTRY: {
          this->send_message_0910_poll();
        } break;
        case KdkCommFsmMethod::KDK_COMM_FSM_LOOP: {
          this->process_response_0910();
        } break;
        case KdkCommFsmMethod::KDK_COMM_FSM_EXIT: {
          this->state_.last_update_timestamp = now;
          this->notify_clients_on_parameter_update();
        } break;
      }
    } break;

    case KdkCommFsmState::KDK_COMM_STATE_PUSH_STATES_0810: {
      switch (method) {
        case KdkCommFsmMethod::KDK_COMM_FSM_ENTRY: {
          auto &list = this->state_.pending_parameter_update_list;
          this->send_message_0810(list);
          list.clear();
        } break;
        case KdkCommFsmMethod::KDK_COMM_FSM_LOOP: {
          this->process_response_default(0x0810);
        } break;
        case KdkCommFsmMethod::KDK_COMM_FSM_EXIT: {
          this->fsm_push_event(KdkCommFsmEvent::KDK_COMM_FSM_EVENT_PULL_STATES);
        } break;
      }
    } break;
  }
}

/*******************************************************************************
 * PUBLIC
 ******************************************************************************/

void KdkConnectionManager::dump_config() {
  const uint32_t now = millis();

  ESP_LOGCONFIG(TAG, "KdkConnectionManager:");

  LOG_UPDATE_INTERVAL(this);

  ESP_LOGCONFIG(TAG, "  Last Ping: %ds ago", ((uint) (now - this->state_.last_ping_timestamp) / 1000U));

  ESP_LOGCONFIG(TAG, "  FSM State: %s", this->get_state_name(this->fsm_.state).c_str());

  ESP_LOGCONFIG(TAG, "  Clients (%d):", this->state_.clients.size());
  for (auto *client : this->state_.clients) {
    ESP_LOGCONFIG(TAG, "  - %s", client->name().c_str());
  }

  ESP_LOGCONFIG(TAG, "  Product_Model: %s", this->state_.product_model.c_str());
  ESP_LOGCONFIG(TAG, "  Product Serial: %s", this->state_.product_serial.c_str());
  ESP_LOGCONFIG(TAG, "  Parameter Table ID: 0x%06X", this->state_.parameter_table_id);

  ESP_LOGCONFIG(TAG, "  Parameter Count: %d", this->state_.parameters.size());
  auto i = 0;
  for (auto const &x : this->state_.parameters) {
    auto &param = x.second;
    ESP_LOGCONFIG(TAG, "  [%2d] ID=%04X, SIZE=%-2d, META=%02X, DATA=%s", i++, param.id, param.size, param.metadata,
                  this->hex2str(param.data).c_str());
  }

  this->check_uart_settings(KDK_SUPPORTED_BAUD_RATE, 1, uart::UART_CONFIG_PARITY_EVEN, 8);
}

void KdkConnectionManager::update() {
  const uint32_t now = millis();

  // Process received bytes
  while (this->available() && (!this->is_message_pending())) {
    uint8_t byte;
    this->read_byte(&byte);
    this->receiver_process_byte(byte);
  }

  // Check response timeout
  this->check_response_timeout(now);

  this->fsm_run();

  this->process_message();
}

void KdkConnectionManager::register_client(KdkConnectionClient *client) {
  client->set_parent(this);
  this->state_.clients.push_back(client);
}

const std::vector<uint8_t> KdkConnectionManager::get_parameter_data(uint16_t id) const {
  auto &parameters = this->state_.parameters;
  auto it = parameters.find(id);
  if (it == parameters.end()) {
    ESP_LOGW(TAG, "PARAM> Failed to find parameter ID %04X", id);
    return {};
  }
  return it->second.data;
}

void KdkConnectionManager::update_parameter_data(std::vector<struct KdkParamUpdate> parameters) {
  auto &update_list = this->state_.pending_parameter_update_list;
  update_list.clear();  // Only single batch update is supported
  update_list.insert(update_list.end(), parameters.begin(), parameters.end());
}

}  // namespace kdk
}  // namespace esphome
