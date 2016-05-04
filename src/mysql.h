/*
  Copyright (c) 2012, 2015 Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

  mysql.h - Library for communicating with a MySQL Server over Ethernet.

  This header file defines the Connector class for connecting to and
  issuing queries against a MySQL database. You can issue any command
  using SQL statements for inserting or retrieving data.

  Dependencies:

    - requires the SHA1 code from google code repository. See README.txt
      for more details.

  Version 1.0.0a Created by Dr. Charles A. Bell, April 2012.
  Version 1.0.0b Updated by Dr. Charles A. Bell, October 2013.
  Version 1.0.1b Updated by Dr. Charles A. Bell, February 2014.
  Version 1.0.2b Updated by Dr. Charles A. Bell, April 2014.
  Version 1.0.3rc Updated by Dr. Charles A. Bell, March 2015.
  Version 1.0.4ga Updated by Dr. Charles A. Bell, July 2015.
  Version 1.5.0-alpha Updated by Brent Pell, November 2015.
*/
#ifndef mysql_h
#define mysql_h

#include "Arduino.h"

 
// Don't quite have this working yet. Will experiment with this more later.

// If not using Arduino AVR, SAM, or ESP8266, Show error to explain compile failure.
// ----------------------------------------------------------------------------------------
#if (not defined ARDUINO_ARCH_AVR) && (not defined ARDUINO_ARCH_SAM) && (not defined ARDUINO_ARCH_SAMD) && (not defined ESP8266)
	#error “This library only supports boards with an AVR, SAM, SAMD, or ESP8266 processor.”
#endif
// ----------------------------------------------------------------------------------------


#if defined ESP8266
	#include <ESP8266WiFi.h>
#else
	#include <SPI.h>
	#include <Ethernet.h>
	//#define WIFI         // Uncomment this for use with the WiFi shield
	//#include <WiFi.h>    // Uncomment this for use with the WiFi shield
#endif

#define WITH_SELECT  // Uncomment this to use SELECT queries

//#define WITH_DEBUG   // Uncomment this for enabling debugging of messages

#define OK_PACKET     0x00
#define EOF_PACKET    0xfe
#define ERROR_PACKET  0xff
#define MAX_FIELDS    0x20   // Maximum number of fields. Reduce to save memory. Default=32
#define VERSION_STR   "1.5.0-alpha"

#if defined WITH_SELECT

// Structure for retrieving a field (minimal implementation).
typedef struct {
  char *db;
  char *table;
  char *name;
} field_struct;

// Structure for storing result set metadata.
typedef struct {
  int_fast8_t num_fields;     // actual number of fields
  field_struct *fields[MAX_FIELDS];
} column_names;

// Structure for storing row data.
typedef struct {
  char *values[MAX_FIELDS];
} row_values;

#endif

/**
 * Connector class
 *
 * The connector class permits users to connect to and issue queries
 * against a MySQL database. It is a lightweight connector with the
 * following features.
 *
 *  - Connect and authenticate with a MySQL server (using 'new' 4.1+
 *    protocol).
 *  - Issue simple commands like INSERT, UPDATE, DELETE, SHOW, etc.
 *  - Run queries that return result sets.
 *
 *  There are some strict limitations:
 *
 *  - Queries must fit into memory. This is because the class uses an
 *    internal buffer for building data packets to send to the server.
 *    It is suggested long strings be stored in program memory using
 *    PROGMEM (see cmd_query_P).
 *  - Result sets are read one row-at-a-time.
 *  - The combined length of a row in a result set must fit into
 *    memory. The connector reads one packet-at-a-time and since the
 *    Arduino has a limited data size, the combined length of all fields
 *    must be less than available memory.
 *  - Server error responses are processed immediately with the error
 *    code and text written via Serial.print.
 */
class Connector
{
  public:
    Connector();
	bool mysql_connect(const char *server, int_fast16_t port, char *user, char *password);
    bool mysql_connect(IPAddress server, int_fast16_t port, char *user, char *password);
    void disconnect();
    bool cmd_query(const char *query);
    bool cmd_query_P(const char *query);
    int_fast8_t is_connected () { return client.connected(); }
    const char *version() { return VERSION_STR; }
#if defined WITH_SELECT
    column_names *get_columns();
    row_values *get_next_row();
    void free_columns_buffer();
    void free_row_buffer();
    void show_results();
    bool clear_ok_packet();
#endif
  private:
    byte *buffer;
    char *server_version;
    byte seed[20];
    int_fast8_t packet_len;
#if defined WITH_SELECT
    column_names columns;
    bool columns_read;
    int_fast8_t num_cols;
    row_values row;
#endif

    // Determine if WiFi shield is used
    #if (defined WIFI) || (defined ESP8266)
      WiFiClient client;
    #else
      EthernetClient client;
    #endif

    // Methods for handling packets
    int_fast8_t wait_for_client();
    void send_authentication_packet(char *user, char *password);
    void read_packet();
    void parse_handshake_packet();
    int_fast8_t check_ok_packet();
    void parse_error_packet();
    bool run_query(int_fast8_t query_len);

    // Utility methods
    bool scramble_password(char *password, byte *pwd_hash);
    int_fast8_t get_lcb_len(int_fast8_t offset);
    int_fast8_t read_int(int_fast8_t offset, int_fast8_t size=0);
    void store_int(byte *buff, int_fast32_t value, int_fast8_t size);
#if defined WITH_SELECT
    char *read_string(int_fast8_t *offset);
    int_fast8_t get_field(field_struct *fs);
    int_fast8_t get_row();
    bool get_fields();
    int_fast8_t get_row_values();
    column_names *query_result();
#endif

    // diagnostic methods
#if defined WITH_DEBUG
    void print_packet();
#endif

    void print_message(const char *msg, bool EOL = false) {
      char pos;
      while ((pos = pgm_read_byte(msg))) {
        Serial.print(pos);
        msg++;
      }
      if (EOL)
        Serial.println();
    }
};

#endif
