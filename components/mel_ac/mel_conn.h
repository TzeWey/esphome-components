#pragma once

#include "esphome/components/uart/uart.h"

namespace esphome {
namespace mel {
namespace conn {

// MEL Command Constants
static const uint8_t MEL_COMMAND_START = 0xFC;
static const uint8_t MEL_COMMAND_VERSION_MAJOR = 0x01;
static const uint8_t MEL_COMMAND_VERSION_MINOR = 0x30;
static const uint8_t MEL_COMMAND_FLAGS_IN = 0x20;
static const uint8_t MEL_COMMAND_FLAGS_OUT = 0x40;
static const uint8_t MEL_COMMAND_FLAGS_SET = 0x01;
static const uint8_t MEL_COMMAND_FLAGS_GET = 0x02;
static const uint8_t MEL_COMMAND_FLAGS_CONNECT = 0x18 | MEL_COMMAND_FLAGS_GET;

static const uint16_t MEL_COMMAND_SET_POWER = 0x0001;
static const uint16_t MEL_COMMAND_SET_MODE = 0x0002;
static const uint16_t MEL_COMMAND_SET_TEMPERATURE = 0x0004;
static const uint16_t MEL_COMMAND_SET_FAN = 0x0008;
static const uint16_t MEL_COMMAND_SET_VANE_VERT = 0x0010;
static const uint16_t MEL_COMMAND_SET_VANE_HORZ = 0x0100;

static const uint8_t MEL_COMMAND_BUFFER_SIZE = 32;  // Max 26 bytes of data
static const uint8_t MEL_COMMAND_CHECKSUM_SIZE = 1;

static const uint32_t MEL_COMMAND_RECV_TIMEOUT = 1000;  // Max time in ms to wait for a complete command

struct MelCommand {
  uint8_t start;
  uint8_t flags;
  uint8_t version_major;
  uint8_t version_minor;
  uint8_t length;
  uint8_t payload[];
} PACKED;

enum MelCommandType {
  MEL_COMMAND_TYPE_SET_PARAMS = 1,  // Set parameters
  MEL_COMMAND_TYPE_GET_PARAMS = 2,  // Get parameters
  MEL_COMMAND_TYPE_GET_TEMP = 3,    // Get room temperature
  MEL_COMMAND_TYPE_UNKNOWN = 4,     // Unknown
  MEL_COMMAND_TYPE_GET_TIMERS = 5,  // Request timers
  MEL_COMMAND_TYPE_GET_STATUS = 6,  // Request status
  MEL_COMMAND_TYPE_SET_TEMP = 7,    // Report room temp from external sensor
  MEL_COMMAND_TYPE_CONNECT = 0xCA   // Handshake packet
};

class MelConnectionManager {
 private:
  uart::UARTDevice *uart_;
  uint32_t tx_timestamp_ = 0;
  uint8_t tx_buffer_[MEL_COMMAND_BUFFER_SIZE];

  uint32_t rx_index_ = 0;
  uint32_t rx_timestamp_ = 0;
  uint8_t rx_buffer_[MEL_COMMAND_BUFFER_SIZE];

  bool state_waiting_response_ = false;
  bool state_response_timeout_ = false;
  bool state_response_pending_ = false;

  // Utilities
  std::string hex2str(const uint8_t *buffer, size_t length);
  uint8_t calculate_checksum(const uint8_t *buffer, size_t length);

  void receiver_check_timeout(const uint32_t now);
  void receiver_reset_states(void);
  void receiver_process_byte(uint8_t byte);

 public:
  bool is_busy() const { return this->state_waiting_response_; }
  bool is_response_timeout() const { return this->state_response_timeout_; }
  bool is_response_pending() const { return this->state_response_pending_; }

  const struct MelCommand *response() const { return (MelCommand *) this->rx_buffer_; }

  void send_command(uint8_t flags, uint8_t *payload, uint8_t length);

  bool check_response_flags(uint8_t flags) { return (this->response()->flags & flags) == flags; }
  void clear_response_pending() { this->state_response_pending_ = false; }

  void request_connect(void);
  void request_room_temperature(void);

  void process_byte(uint8_t byte) { this->receiver_process_byte(byte); }

  void tick(void);

  MelConnectionManager(uart::UARTDevice *uart) : uart_(uart){};
};

}  // namespace conn
}  // namespace mel
}  // namespace esphome
