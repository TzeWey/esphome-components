#include "esphome/core/log.h"

#include "mel_conn.h"

namespace esphome {
namespace mel {
namespace conn {

static const char *const TAG = "mel.conn";

/*******************************************************************************
 * MelConnectionManager
 ******************************************************************************/

/* COMMAND FORMAT
 * ┌────────┬────────┬─────────┬─────────┬─────────┬─────────┬──────────┐
 * │ START  │ FLAGS  │ VERSION │ VERSION │ LENGTH  │ PAYLOAD │ CHECKSUM │
 * │        │        │  MAJOR  │  MINOR  │         │         │          │
 * ├────────┼────────┼─────────┼─────────┼─────────┼─────────┼──────────┤
 * │ 1 Byte │ 1 Byte │ 1 Byte  │ 1 Byte  │ 1 Byte  │ LENGTH  │ 1 Byte   │
 * │        │        │         │         │         │ Bytes   │          │
 * └────────┴────────┴─────────┴─────────┴─────────┴─────────┴──────────┘
 */

/* PAYLOAD FORMAT
 * ┌────────┬─────────┐
 * │ TYPE   │ DATA    │
 * ├────────┼─────────┤
 * │ 1 Byte │ n Bytes │
 * └────────┴─────────┘
 */

std::string MelConnectionManager::hex2str(const uint8_t *buffer, size_t length) {
  constexpr char hexmap[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
  std::string str(length * 3, ' ');
  for (int i = 0; i < length; ++i) {
    str[3 * i] = hexmap[(buffer[i] & 0xF0) >> 4];
    str[3 * i + 1] = hexmap[buffer[i] & 0x0F];
  }
  return str;
}

/**
 * Calculates the checksum of the provided buffer.
 */
uint8_t MelConnectionManager::calculate_checksum(const uint8_t *buffer, size_t length) {
  uint8_t sum = 0;
  for (int i = 0; i < length; i++) {
    sum += buffer[i];
  }
  return (MEL_COMMAND_START - sum);
}

void MelConnectionManager::receiver_check_timeout(const uint32_t now) {
  // Clear the waiting response state if the last request was a long time ago
  if (this->state_waiting_response_) {
    const uint32_t elapsed = (now - this->tx_timestamp_);
    if (elapsed > MEL_COMMAND_RECV_TIMEOUT) {
      ESP_LOGVV(TAG, "RX> Response timeout after %d ms", elapsed);
      this->state_waiting_response_ = false;
      this->state_response_timeout_ = true;
    }
  }
}

void MelConnectionManager::receiver_reset_states(void) {
  this->rx_index_ = 0;
  this->rx_timestamp_ = millis();
  this->state_response_timeout_ = false;
  this->state_response_pending_ = false;
}

void MelConnectionManager::receiver_process_byte(uint8_t byte) {
  ESP_LOGVV(TAG, "RX> Received 0x%02X", byte);
  const uint32_t now = millis();

  // Reset the receiver logic if the last byte received was a long time ago
  const uint32_t elapsed = (now - this->rx_timestamp_);
  if (elapsed > MEL_COMMAND_RECV_TIMEOUT) {
    ESP_LOGVV(TAG, "RX> Inter-byte timeout after %d ms", elapsed);
    this->receiver_reset_states();
  } else {
    // Update receive timestamp
    this->rx_timestamp_ = now;
  }

  // Check whether the first byte is START
  if ((this->rx_index_ == 0) && (byte != MEL_COMMAND_START)) {
    return;
  }

  // Store the received byte into the buffer
  this->rx_buffer_[this->rx_index_++] = byte;

  // Check whether we have received enough bytes to begin processing:
  // START, FLAGS, VERSION MAJOR, VERSION MINOR and LENGTH
  if (this->rx_index_ < sizeof(struct MelCommand)) {
    return;
  }

  // Map receive buffer to MelCommand
  struct MelCommand *cmd = (struct MelCommand *) this->rx_buffer_;

  // Verify Version
  if ((cmd->version_major != MEL_COMMAND_VERSION_MAJOR) && (cmd->version_minor != MEL_COMMAND_VERSION_MINOR)) {
    ESP_LOGW(TAG, "RX> Invalid version: got=%d.%d, exp=%d.%d", cmd->version_major, cmd->version_minor,
             MEL_COMMAND_VERSION_MAJOR, MEL_COMMAND_VERSION_MINOR);
    this->receiver_reset_states();
    return;
  }

  // Wait for all bytes to be received
  uint8_t expected_length = sizeof(struct MelCommand) + cmd->length + MEL_COMMAND_CHECKSUM_SIZE;
  if (this->rx_index_ < expected_length) {
    return;
  }

  // Verify checksum
  size_t command_length = sizeof(struct MelCommand) + cmd->length;
  uint8_t checksum_exp = this->calculate_checksum(this->rx_buffer_, command_length);
  uint8_t checksum = cmd->payload[cmd->length];
  if (checksum != checksum_exp) {
    ESP_LOGW(TAG, "RX> Invalid checksum: got=0x%02X, exp=0x%02X", checksum, checksum_exp);
    this->receiver_reset_states();
    return;
  }

  ESP_LOGVV(TAG, "RX> Response:");
  ESP_LOGVV(TAG, "RX>   version: %d.%d", cmd->version_major, cmd->version_minor);
  ESP_LOGVV(TAG, "RX>   flags: %02X", cmd->flags);
  ESP_LOGVV(TAG, "RX>   length: %d", cmd->length);
  ESP_LOGVV(TAG, "RX>   payload: %s", this->hex2str(cmd->payload, cmd->length).c_str());
  ESP_LOGVV(TAG, "RX>   checksum: %02X", checksum);

  // A valid command was received, reset the receiver states
  this->receiver_reset_states();
  this->state_waiting_response_ = false;
  this->state_response_pending_ = true;
}

void MelConnectionManager::send_command(uint8_t flags, uint8_t *payload, uint8_t length) {
  struct MelCommand *cmd = (struct MelCommand *) this->tx_buffer_;

  cmd->start = MEL_COMMAND_START;
  cmd->flags = MEL_COMMAND_FLAGS_OUT | flags;
  cmd->version_major = MEL_COMMAND_VERSION_MAJOR;
  cmd->version_minor = MEL_COMMAND_VERSION_MINOR;
  cmd->length = length;
  memcpy(cmd->payload, payload, length);

  size_t command_length = sizeof(struct MelCommand) + cmd->length;
  uint8_t checksum = this->calculate_checksum(this->tx_buffer_, command_length);
  this->tx_buffer_[command_length++] = checksum;
  this->uart_->write_array(this->tx_buffer_, command_length);

  ESP_LOGVV(TAG, "TX> Transmit:");
  ESP_LOGVV(TAG, "TX>   version: %d.%d", cmd->version_major, cmd->version_minor);
  ESP_LOGVV(TAG, "TX>   flags: %02X", cmd->flags);
  ESP_LOGVV(TAG, "TX>   length: %d", cmd->length);
  ESP_LOGVV(TAG, "TX>   payload: %s", this->hex2str(cmd->payload, cmd->length).c_str());
  ESP_LOGVV(TAG, "TX>   checksum: %02X", checksum);

  this->tx_timestamp_ = millis();
  this->receiver_reset_states();
  this->state_waiting_response_ = true;
}

void MelConnectionManager::tick(void) {
  const uint32_t now = millis();

  // Check response timeout
  this->receiver_check_timeout(now);

  // Process received bytes
  while (this->uart_->available()) {
    uint8_t byte;
    this->uart_->read_byte(&byte);
    this->receiver_process_byte(byte);
  }
}

}  // namespace conn
}  // namespace mel
}  // namespace esphome
