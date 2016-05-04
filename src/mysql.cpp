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

  mysql.cpp - Library for communicating with a MySQL Server over Ethernet.

  This code module implements the Connector class for connecting to and
  issuing queries against a MySQL database. You can issue any command
  using SQL statements for inserting or retrieving data.

  Version 1.0.0a Created by Dr. Charles A. Bell, April 2012.
  Version 1.0.0b Updated by Dr. Charles A. Bell, October 2013.
  Version 1.0.1b Updated by Dr. Charles A. Bell, February 2014.
  Version 1.0.2b Updated by Dr. Charles A. Bell, April 2014.
  Version 1.0.3rc Updated by Dr. Charles A. Bell, March 2015.
  Version 1.0.4ga Updated by Dr. Charles A. Bell, July 2015.
  Version 1.5.0-alpha Updated by Brent Pell, November 2015.
*/
#include "Arduino.h"
#include "mysql.h"

#if defined ESP8266

	#include <Hash.h>
	
#else
	
	#include <sha1.h>
	#include <avr/pgmspace.h>
	
#endif

#define MAX_CONNECT_ATTEMPTS 3
#define MAX_TIMEOUT          10
#define MIN_BYTES_NETWORK    8

const char CONNECTED[] PROGMEM = "Connected to server version ";
const char DISCONNECTED[] PROGMEM = "Disconnected.";
const char MEMORY_ERROR[] PROGMEM = "Memory error.";
const char PACKET_ERROR[] PROGMEM = "Packet error.";
const char BAD_MOJO[] PROGMEM = "Bad mojo. EOF found reading column header.";
const char ROWS[] PROGMEM = " rows in result.";
const char READ_COLS[] PROGMEM = "ERROR: You must read the columns first!";


// Begin public methods

Connector::Connector() {
  buffer = NULL;
#if defined WITH_SELECT
  columns.num_fields = 0;
  for (int_fast8_t f = 0; f < MAX_FIELDS; f++) {
    columns.fields[f] = NULL;
    row.values[f] = NULL;
  }
  columns_read = false;
#endif
}

/**
 * mysql_connect - Connect to a MySQL server.
 *
 * This method is used to connect to a MySQL server. It will attempt to
 * connect to the server as a client retrying up to MAX_CONNECT_ATTEMPTS.
 * This permits the possibility of longer than normal network lag times
 * for wireless networks. You can adjust MAX_CONNECT_ATTEMPTS to suit
 * your environment.
 *
 * server[in]      IP address of the server as IPAddress type
 * port[in]        port number of the server
 * user[in]        user name
 * password[in]    (optional) user password
 *
 * Returns bool - True = connection succeeded
*/
bool Connector::mysql_connect(const char *server, int_fast16_t port, char *user, char *password)
{
  int_fast8_t connected = 0;
  int_fast8_t i = -1;

  // Retry up to MAX_CONNECT_ATTEMPTS times 1 second apart.
  do {
    connected = client.connect(server, port);
	#if defined DEBUG
		if(!connected)
			Serial.println("Failed :(");
	#endif
	delay(1000);
	i++;
  } while (i < MAX_CONNECT_ATTEMPTS && !connected);

  if (connected) {
    read_packet();
    parse_handshake_packet();
    send_authentication_packet(user, password);
    read_packet();
    if (check_ok_packet() != 0) {
      parse_error_packet();
      return false;
    }
	#if defined DEBUG
		print_message(CONNECTED);
		Serial.println(server_version);
		free(server_version); // don't need it anymore
	#endif
    return true;
  }
  return false;
}

bool Connector::mysql_connect(IPAddress server, int_fast16_t port, char *user, char *password)
{
  int_fast8_t connected = 0;
  int_fast8_t i = -1;

  // Retry up to MAX_CONNECT_ATTEMPTS times 1 second apart.
  do {
    connected = client.connect(server, port);
	#if defined DEBUG
		if(!connected)
			Serial.println("Failed :(");
	#endif
	delay(1000);
	i++;
  } while (i < MAX_CONNECT_ATTEMPTS && !connected);

  if (connected) {
    read_packet();
    parse_handshake_packet();
    send_authentication_packet(user, password);
    read_packet();
    if (check_ok_packet() != 0) {
      parse_error_packet();
      return false;
    }
	#if defined DEBUG
		print_message(CONNECTED);
		Serial.println(server_version);
		free(server_version); // don't need it anymore
	#endif
    return true;
  }
  return false;
}


/**
 * Disconnect from the server.
 *
 * Terminates connection with the server. You must call mysql_connect()
 * to reconnect.
*/
void Connector::disconnect()
{
  if (is_connected())
  {
    client.flush();
    client.stop();
	#if defined DEBUG
		print_message(DISCONNECTED, true);
	#endif
  }
}

/**
 * cmd_query - Execute a SQL statement
 *
 * This method executes the query specified as a character array that is
 * located in data memory. It copies the query to the local buffer then
 * calls the run_query() method to execute the query.
 *
 * If a result set is available after the query executes, the field
 * packets and rows can be read separately using the get_field() and
 * get_row() methods.
 *
 * query[in]       SQL statement (using normal memory access)
 *
 * Returns bool - True = a result set is available for reading
*/
bool Connector::cmd_query(const char *query)
{
  int_fast8_t query_len = (int_fast8_t)strlen(query);

  if (buffer != NULL)
    free(buffer);

  buffer = (byte *)malloc(query_len+5);

  // Write query to packet
  memcpy(&buffer[5], query, query_len);

  // Send the query
  return run_query(query_len);
}

/**
 * cmd_query_P - Execute a SQL statement
 *
 * This method executes the query specified as a character array that is
 * located in program memory. It copies the query to the local buffer then
 * calls the run_query() method to execute the query.
 *
 * If a result set is available after the query executes, the field
 * packets and rows can be read separately using the get_field() and
 * get_row() methods.
 *
 * query[in]       SQL statement (using PROGMEM)
 *
 * Returns bool - True = a result set is available for reading
*/
bool Connector::cmd_query_P(const char *query)
{
  int_fast8_t query_len = (int_fast8_t)strlen_P(query);
  if (buffer != NULL)
    free(buffer);
  buffer = (byte *)malloc(query_len+5);
  // Write query to packet
  for (int_fast8_t c = 0; c < query_len; c++)
    buffer[c+5] = pgm_read_byte_near(query+c);
  // Send the query
  return run_query(query_len);
}

#if defined WITH_SELECT

/**
 * show_results - Show a result set from the server via Serial.print
 *
 * This method reads a result from the server and displays it via the
 * via the Serial.print methods. It can be used in cases where
 * you may want to issue a SELECT or SHOW and see the results on your
 * computer from the Arduino.
 *
 * It is also a good example of how to read a result set from the
 * because it uses the public methods designed to return result
 * sets from the server.
*/
void Connector::show_results() {
  column_names *cols;
  int_fast8_t rows = 0;

  // Get the columns
  cols = get_columns();
  if (cols == NULL) {
    return;
  }

  for (int_fast8_t f = 0; f < columns.num_fields; f++) {
    Serial.print(columns.fields[f]->name);
    if (f < columns.num_fields-1)
      Serial.print(',');
  }
  Serial.println();

  // Read the rows
  while (get_next_row()) {
	#if defined WITH_DEBUG
		print_packet();
	#endif
    rows++;
    for (int_fast8_t f = 0; f < columns.num_fields; f++) {
      Serial.print(row.values[f]);
      if (f < columns.num_fields-1)
        Serial.print(',');
    }
    free_row_buffer();
    Serial.println();
  }

  // Report how many rows were read
  Serial.print(rows);
  print_message(ROWS, true);
  free_columns_buffer();

  // Free any post-query messages in queue for stored procedures
  clear_ok_packet();
}


/**
 * clear_ok_packet - clear last Ok packet (if present)
 *
 * This method reads the header and status to see if this is an Ok packet.
 * If it is, it reads the packet and discards it. This is useful for
 * processing result sets from stored procedures.
 *
 * Returns False if the packet was not an Ok packet.
*/
bool Connector::clear_ok_packet() {
  int_fast8_t num = 0;

  do {
    num = client.available();
    if (num > 0) {
      read_packet();
      if (check_ok_packet() != 0) {
        parse_error_packet();
        return false;
      }
    }
  } while (num > 0);
  return true;
}


/**
 * get_next_row - Iterator for reading rows from a result set
 *
 * This method returns an instance of a structure (row_values)
 * that contains an array of strings representing the row
 * values returned from the server.
 *
 * The caller can use the values however needed - by first
 * converting them to a specific type or as a string.
*/
row_values *Connector::get_next_row() {
  int_fast8_t res = 0;

  // Read the rows
  res = get_row_values();
  if (res != EOF_PACKET) {
    return &row;
  }
  return NULL;
}

/**
 * free_columns_buffer - Free memory allocated for column names
 *
 * This method frees the memory allocated during the get_columns()
 * method.
 *
 * NOTICE: Failing to call this method after calling get_columns()
 *         and consuming the column names, types, etc. will result
 *         in a memory leak. The size of the leak will depend on
 *         the size of the combined column names (bytes).
*/
void Connector::free_columns_buffer() {
  // clear the columns
  for (int_fast8_t f = 0; f < MAX_FIELDS; f++) {
    if (columns.fields[f] != NULL) {
      free(columns.fields[f]->db);
      free(columns.fields[f]->table);
      free(columns.fields[f]->name);
      free(columns.fields[f]);
    }
    columns.fields[f] = NULL;
  }
  num_cols = 0;
#if defined WITH_SELECT
  columns_read = false;
#endif
}


/**
 * free_row_buffer - Free memory allocated for row values
 *
 * This method frees the memory allocated during the get_next_row()
 * method.
 *
 * NOTICE: You must call this method at least once after you
 *         have consumed the values you wish to process. Failing
 *         to do will result in a memory leak equal to the sum
 *         of the length of values and one byte for each max cols.
*/
void Connector::free_row_buffer() {
  // clear the row
  for (int_fast8_t f = 0; f < MAX_FIELDS; f++) {
    if (row.values[f] != NULL) {
      free(row.values[f]);
    }
    row.values[f] = NULL;
  }
}


/**
 * get_columns - Get a list of the columns (fields)
 *
 * This method returns an instance of the column_names structure
 * that contains an array of fields.
 *
 * Note: you should call free_columns_buffer() after consuming
 *       the field data to free memory.
*/
column_names *Connector::get_columns() {
  free_columns_buffer();
  free_row_buffer();
  num_cols = 0;
  if (get_fields()) {
    columns_read = true;
    return &columns;
  }
  else {
    return NULL;
  }
}

#endif

// Begin private methods

/**
 * run_query - execute a query
 *
 * This method sends the query string to the server and waits for a
 * response. If the result is a result set, it returns true, if it is
 * an error, it processes the error packet and prints the error via
 * Serial.print(). If it is an Ok packet, it parses the packet and
 * returns false.
 *
 * query_len[in]   Number of bytes in the query string
 *
 * Returns bool - true = result set available,
 *                   false = no result set returned.
*/
bool Connector::run_query(int_fast8_t query_len)
{
  store_int(&buffer[0], query_len+1, 3);
  // TODO: Abort if query larger than sizeof(buffer);
  buffer[3] = byte(0x00);
  buffer[4] = byte(0x03);  // command packet

  // Send the query
  for (int_fast8_t c = 0; c < query_len+5; c++)
    client.write(buffer[c]);

  // Read a response packet and check it for Ok or Error.
  read_packet();
  int_fast8_t res = check_ok_packet();
  if (res == ERROR_PACKET) {
    parse_error_packet();
    return false;
  } else if (!res) {
    return false;
  }
  // Not an Ok packet, so we now have the result set to process.
#if defined WITH_SELECT
  columns_read = false;
#endif
  return true;
}


/**
 * wait_for_client - Wait until data is available for reading
 *
 * This method is used to permit the connector to respond to servers
 * that have high latency or execute long queries. The timeout is
 * set by MAX_TIMEOUT. Adjust this value to match the performance of
 * your server and network.
 *
 * It is also used to read how many bytes in total are available from the
 * server. Thus, it can be used to know how large a data burst is from
 * the server.
 *
 * Returns integer - Number of bytes available to read.
*/
int_fast8_t Connector::wait_for_client() {
  int_fast8_t num = 0;
  int_fast8_t timeout = 0;
  do {
    num = client.available();
    timeout++;
    if (num < MIN_BYTES_NETWORK and timeout < MAX_TIMEOUT) {
      delay(100);  // adjust for network latency
    }
  } while (num < MIN_BYTES_NETWORK and timeout < MAX_TIMEOUT);
  return num;
}


/**
 * send_authentication_packet - Send the response to the server's challenge
 *
 * This method builds a response packet used to respond to the server's
 * challenge packet (called the handshake packet). It includes the user
 * name and password scrambled using the SHA1 seed from the handshake
 * packet. It also sets the character set (default is 8 which you can
 * change to meet your needs).
 *
 * Note: you can also set the default database in this packet. See
 *       the code before for a comment on where this happens.
 *
 * The authentication packet is defined as follows.
 *
 * Bytes                        Name
 * -----                        ----
 * 4                            client_flags
 * 4                            max_packet_size
 * 1                            charset_number
 * 23                           (filler) always 0x00...
 * n (Null-Terminated String)   user
 * n (Length Coded Binary)      scramble_buff (1 + x bytes)
 * n (Null-Terminated String)   databasename (optional
 *
 * user[in]        User name
 * password[in]    password
*/
void Connector::send_authentication_packet(char *user, char *password)
{
  if (buffer != NULL)
    free(buffer);

  buffer = (byte *)malloc(256);

  int_fast8_t size_send = 4;

  // client flags
  buffer[size_send] = byte(0x85);
  buffer[size_send+1] = byte(0xa6);
  buffer[size_send+2] = byte(0x03);
  buffer[size_send+3] = byte(0x00);
  size_send += 4;

  // max_allowed_packet
  buffer[size_send] = 0;
  buffer[size_send+1] = 0;
  buffer[size_send+2] = 0;
  buffer[size_send+3] = 1;
  size_send += 4;

  // charset - default is 8
  buffer[size_send] = byte(0x08);
  size_send += 1;
  for(int_fast8_t i = 0; i < 24; i++)
    buffer[size_send+i] = 0x00;
  size_send += 23;

  // user name
  memcpy((char *)&buffer[size_send], user, strlen(user));
  size_send += strlen(user) + 1;
  buffer[size_send-1] = 0x00;

  // password - see scramble password
  byte *scramble = (uint_least8_t *)malloc(20);
  if (scramble_password(password, scramble)) {
    buffer[size_send] = 0x14;
    size_send += 1;
    for (int_fast8_t i = 0; i < 20; i++)
      buffer[i+size_send] = scramble[i];
    size_send += 20;
    buffer[size_send] = 0x00;
  }
  free(scramble);

  // terminate password response
  buffer[size_send] = 0x00;
  size_send += 1;

  // database
  buffer[size_send+1] = 0x00;
  size_send += 1;

  // Write packet size
  int_fast8_t p_size = size_send - 4;
  store_int(&buffer[0], p_size, 3);
  buffer[3] = byte(0x01);

  // Write the packet
  for (int_fast8_t i = 0; i < size_send; i++)
    client.write(buffer[i]);
}


/**
 * scramble_password - Build a SHA1 scramble of the user password
 *
 * This method uses the password hash seed sent from the server to
 * form a SHA1 hash of the password. This is used to send back to
 * the server to complete the challenge and response step in the
 * authentication handshake.
 *
 * password[in]    User's password in clear text
 * pwd_hash[in]    Seed from the server
 *
 * Returns bool - True = scramble succeeded
*/
#if defined ESP8266
	
	bool Connector::scramble_password(char *password, byte *pwd_hash) {
		byte *digest;
		byte hash1[20];
		byte hash2[20];
		byte hash3[20];
		byte pwd_buffer[40];

		if (strlen(password) == 0)
		return false;

		// hash1
		sha1(password, &hash1[0]);

		// hash2
		sha1(hash1, 20, &hash2[0]);

		// hash3 of seed + hash2
		memcpy(pwd_buffer, &seed, 20);
		memcpy(pwd_buffer+20, hash2, 20);
		sha1(pwd_buffer, 40, &hash3[0]);

		// XOR for hash4
		for (int_fast8_t i = 0; i < 20; i++)
		pwd_hash[i] = hash1[i] ^ hash3[i];

		return true;
	}
	
#else
	
	bool Connector::scramble_password(char *password, byte *pwd_hash) {
		byte *digest;
		byte hash1[20];
		byte hash2[20];
		byte hash3[20];
		byte pwd_buffer[40];

		if (strlen(password) == 0)
			return false;

		// hash1
		Sha1.init();
		Sha1.print(password);
		digest = Sha1.result();
		memcpy(hash1, digest, 20);

		// hash2
		Sha1.init();
		Sha1.write(hash1, 20);
		digest = Sha1.result();
		memcpy(hash2, digest, 20);

		// hash3 of seed + hash2
		Sha1.init();
		memcpy(pwd_buffer, &seed, 20);
		memcpy(pwd_buffer+20, hash2, 20);
		Sha1.write(pwd_buffer, 40);
		digest = Sha1.result();
		memcpy(hash3, digest, 20);

		// XOR for hash4
		for (int_fast8_t i = 0; i < 20; i++)
			pwd_hash[i] = hash1[i] ^ hash3[i];

		return true;
	}

#endif


/**
 * read_packet - Read a packet from the server and store it in the buffer
 *
 * This method reads the bytes sent by the server as a packet. All packets
 * have a packet header defined as follows.
 *
 * Bytes                 Name
 * -----                 ----
 * 3                     Packet Length
 * 1                     Packet Number
 *
 * Thus, the length of the packet (not including the packet header) can
 * be found by reading the first 4 bytes from the server then reading
 * N bytes for the packet payload.
*/
void Connector::read_packet() {
  byte local[4];

  if (buffer != NULL)
    free(buffer);

#if (not defined WIFI) || (not defined ESP8266)
  // Wait for client (the server) to send data
  wait_for_client();
#endif

  int_fast8_t avail_bytes = wait_for_client();
  while (avail_bytes < 4) {
    avail_bytes = wait_for_client();
  }

  // Read packet header
  for (int_fast8_t i = 0; i < 4; i++) {
#if (defined WIFI) || (defined ESP8266) 
    while (!client.available());
#endif
    local[i] = client.read();
  }

  // Get packet length
  packet_len = local[0];
  packet_len += (local[1] << 8);
  packet_len += ((int_fast32_t)local[2] << 16);
#if (not defined WIFI) || (not defined ESP8266)
  // We must wait for slow arriving packets for Ethernet shields only.
  avail_bytes = wait_for_client();
  while (avail_bytes < packet_len) {
    avail_bytes = wait_for_client();
  }
#endif
  // Check for valid packet.
  if (packet_len < 0) {
    print_message(PACKET_ERROR, true);
    packet_len = 0;
  }
  buffer = (byte *)malloc(packet_len+4);
  if (buffer == NULL) {
    print_message(MEMORY_ERROR, true);
    return;
  }
  for (int_fast8_t i = 0; i < 4; i++)
    buffer[i] = local[i];

  for (int_fast8_t i = 4; i < packet_len+4; i++) {
#if (defined WIFI) || (defined ESP8266)
    while (!client.available());
#endif
    buffer[i] = client.read();
  }
}


/**
 * parse_handshake_packet - Decipher the server's challenge data
 *
 * This method reads the server version string and the seed from the
 * server. The handshake packet is defined as follows.
 *
 *  Bytes                        Name
 *  -----                        ----
 *  1                            protocol_version
 *  n (Null-Terminated String)   server_version
 *  4                            thread_id
 *  8                            scramble_buff
 *  1                            (filler) always 0x00
 *  2                            server_capabilities
 *  1                            server_language
 *  2                            server_status
 *  2                            server capabilities (two upper bytes)
 *  1                            length of the scramble seed
 * 10                            (filler)  always 0
 *  n                            rest of the plugin provided data
 *                               (at least 12 bytes)
 *  1                            \0 byte, terminating the second part of
 *                                a scramble seed
*/
void Connector::parse_handshake_packet() {

  int_fast8_t i = 5;
  do {
    i++;
  } while (buffer[i-1] != 0x00);

  server_version = (char *)malloc(i-5);
  strncpy(server_version, (char *)&buffer[5], i-5);

  // Capture the first 8 characters of seed
  i += 4; // Skip thread id
  for (int_fast8_t j = 0; j < 8; j++)
    seed[j] = buffer[i+j];

  // Capture rest of seed
  i += 27; // skip ahead
  for (int_fast8_t j = 0; j < 12; j++)
    seed[j+8] = buffer[i+j];
}

/**
 * parse_error_packet - Display the error returned from the server
 *
 * This method parses an error packet from the server and displays the
 * error code and text via Serial.print. The error packet is defined
 * as follows.
 *
 * Note: the error packet is already stored in the buffer since this
 *       packet is not an expected response.
 *
 * Bytes                       Name
 * -----                       ----
 * 1                           field_count, always = 0xff
 * 2                           errno
 * 1                           (sqlstate marker), always '#'
 * 5                           sqlstate (5 characters)
 * n                           message
*/
void Connector::parse_error_packet() {
  Serial.print("Error: ");
  Serial.print(read_int(5, 2));
  Serial.print(" = ");
  for (int_fast8_t i = 0; i < packet_len-9; i++)
    Serial.print((char)buffer[i+13]);
  Serial.println(".");
}


/**
 * check_ok_packet - Decipher an Ok packet from the server.
 *
 * This method attempts to parse an Ok packet. If the packet is not an
 * Ok, packet, it returns the packet type.
 *
 *  Bytes                       Name
 *  -----                       ----
 *  1   (Length Coded Binary)   field_count, always = 0
 *  1-9 (Length Coded Binary)   affected_rows
 *  1-9 (Length Coded Binary)   insert_id
 *  2                           server_status
 *  2                           warning_count
 *  n   (until end of packet)   message
 *
 * Returns integer - 0 = successful parse, packet type if not an Ok packet
*/
int_fast8_t Connector::check_ok_packet() {
  int_fast8_t type = buffer[4];
  if (type != OK_PACKET)
    return type;
  return 0;
}


/**
 * get_lcb_len - Retrieves the length of a length coded binary value
 *
 * This reads the first byte from the offset into the buffer and returns
 * the number of bytes (size) that the integer consumes. It is used in
 * conjunction with read_int() to read length coded binary integers
 * from the buffer.
 *
 * Returns integer - number of bytes integer consumes
*/
int_fast8_t Connector::get_lcb_len(int_fast8_t offset) {
  int_fast8_t read_len = buffer[offset];
  if (read_len > 250) {
    // read type:
    byte type = buffer[offset+1];
    if (type == 0xfc)
      read_len = 2;
    else if (type == 0xfd)
      read_len = 3;
    else if (type == 0xfe)
      read_len = 8;
  }
  return 1;
}


#if defined WITH_SELECT

/**
 * read_string - Retrieve a string from the buffer
 *
 * This reads a string from the buffer. It reads the length of the string
 * as the first byte.
 *
 * offset[in]      offset from start of buffer
 *
 * Returns string - String from the buffer
*/
char *Connector::read_string(int_fast8_t *offset) {
  int_fast8_t len_bytes = get_lcb_len(buffer[*offset]);
  int_fast8_t len = read_int(*offset, len_bytes);
  char *str = (char *)malloc(len+1);
  strncpy(str, (char *)&buffer[*offset+len_bytes], len);
  str[len] = 0x00;
  *offset += len_bytes+len;
  return str;
}

#endif

/**
 * read_int - Retrieve an integer from the buffer in size bytes.
 *
 * This reads an integer from the buffer at offset position indicated for
 * the number of bytes specified (size).
 *
 * offset[in]      offset from start of buffer
 * size[in]        number of bytes to use to store the integer
 *
 * Returns integer - integer from the buffer
*/
int_fast8_t Connector::read_int(int_fast8_t offset, int_fast8_t size) {
  int_fast8_t value = 0;
  int_fast8_t new_size = 0;
  if (size == 0)
     new_size = get_lcb_len(offset);
  if (size == 1)
     return buffer[offset];
  new_size = size;
  int_fast8_t shifter = (new_size - 1) * 8;
  for (int_fast8_t i = new_size; i > 0; i--) {
    value += (byte)(buffer[i-1] << shifter);
    shifter -= 8;
  }
  return value;
}


/**
 * store_int - Store an integer value into a byte array of size bytes.
 *
 * This writes an integer into the buffer at the current position of the
 * buffer. It will transform an integer of size to a length coded binary
 * form where 1-3 bytes are used to store the value (set by size).
 *
 * buff[in]        pointer to location in internal buffer where the
 *                 integer will be stored
 * value[in]       integer value to be stored
 * size[in]        number of bytes to use to store the integer
*/
void Connector::store_int(byte *buff, int_fast32_t value, int_fast8_t size) {
  memset(buff, 0, size);
  if (value < 0xff)
    buff[0] = (byte)value;
  else if (value < 0xffff) {
    buff[0] = (byte)value;
    buff[1] = (byte)(value << 8);
  } else if (value < 0xffffff) {
    buff[0] = (byte)value;
    buff[1] = (byte)(value << 8);
    buff[2] = (byte)(value << 16);
  } else if (value < 0xffffff) {
    buff[0] = (byte)value;
    buff[1] = (byte)(value << 8);
    buff[2] = (byte)(value << 16);
    buff[3] = (byte)(value << 24);
  }
}


#if defined WITH_DEBUG

/**
 * print_packet - Print the contents of a packet via Serial.print
 *
 * This method is a diagnostic method. It is best used to decipher a
 * packet from the server (or before being sent to the server). If you
 * are looking for additional program memory space, you can safely
 * delete this method.
*/
void Connector::print_packet() {
  Serial.print("Packet: ");
  Serial.print(buffer[3]);
  Serial.print(" contains ");
  Serial.print(packet_len+3);
  Serial.println(" bytes.");

  Serial.print("  HEX: ");
  for (int_fast8_t i = 0; i < packet_len+3; i++) {
    Serial.print(buffer[i], HEX);
    Serial.print(' ');
  }
  Serial.println();
  Serial.print("ASCII: ");
  for (int_fast8_t i = 0; i < packet_len+3; i++)
    Serial.print((char)buffer[i]);
  Serial.println();
}

#endif

#if defined WITH_SELECT

/**
 * get_field - Read a field from the server
 *
 * This method reads a field packet from the server. Field packets are
 * defined as:
 *
 * Bytes                      Name
 * -----                      ----
 * n (Length Coded String)    catalog
 * n (Length Coded String)    db
 * n (Length Coded String)    table
 * n (Length Coded String)    org_table
 * n (Length Coded String)    name
 * n (Length Coded String)    org_name
 * 1                          (filler)
 * 2                          charsetnr
 * 4                          length
 * 1                          type
 * 2                          flags
 * 1                          decimals
 * 2                          (filler), always 0x00
 * n (Length Coded Binary)    default
 *
 * Note: the sum of all db, column, and field names must be < 255 in length
 *
*/
int_fast8_t Connector::get_field(field_struct *fs) {
  int_fast8_t len_bytes;
  int_fast8_t len;
  int_fast8_t offset;

  // Read field packets until EOF
  read_packet();
  if (buffer[4] != EOF_PACKET) {
    // calculate location of db
    len_bytes = get_lcb_len(4);
    len = read_int(4, len_bytes);
    offset = 4+len_bytes+len;
    fs->db = read_string(&offset);
    // get table
    fs->table = read_string(&offset);
    // calculate location of name
    len_bytes = get_lcb_len(offset);
    len = read_int(offset, len_bytes);
    offset += len_bytes+len;
    fs->name = read_string(&offset);
    return 0;
  }
  return EOF_PACKET;
}


/**
 * get_row - Read a row from the server and store it in the buffer
 *
 * This reads a single row and stores it in the buffer. If there are
 * no more rows, it returns EOF_PACKET. A row packet is defined as
 * follows.
 *
 * Bytes                   Name
 * -----                   ----
 * n (Length Coded String) (column value)
 * ...
 *
 * Note: each column is store as a length coded string concatenated
 *       as a single stream
 *
 * Returns integer - EOF_PACKET if no more rows, 0 if more rows available
*/
int_fast8_t Connector::get_row() {
  // Read row packets
  read_packet();
  if (buffer[4] != EOF_PACKET)
    return 0;
  return EOF_PACKET;
}


/**
 * get_fields - reads the fields from the read buffer
 *
 * This method is used to read the field names, types, etc.
 * from the read buffer and store them in the columns structure
 * in the class.
 *
*/
bool Connector::get_fields()
{
  int_fast8_t num_fields = 0;
  int_fast8_t res = 0;

  if (buffer == NULL) {
    return false;
  }
  num_fields = buffer[4]; // From result header packet
  columns.num_fields = num_fields;
  num_cols = num_fields; // Save this for later use
  for (int_fast8_t f = 0; f < num_fields; f++) {
    field_struct *field = (field_struct *)malloc(sizeof(field_struct));
    res = get_field(field);
    if (res == EOF_PACKET) {
		#if defined WITH_DEBUG
			print_message(BAD_MOJO, true);
		#endif
		return false;
    }
    columns.fields[f] = field;
  }
  read_packet(); // EOF packet
  return true;
}


/**
 * get_row_values - reads the row values from the read buffer
 *
 * This method is used to read the row column values
 * from the read buffer and store them in the row structure
 * in the class.
 *
*/
int_fast8_t Connector::get_row_values() {
  int_fast8_t res = 0;
  int_fast8_t offset = 0;

  // It is an error to try to read rows before columns
  // are read.
  if (!columns_read) {
    print_message(READ_COLS, true);
    return EOF_PACKET;
  }
  // Drop any row data already read
  free_row_buffer();

  // Read a row
  res = get_row();
  if (res != EOF_PACKET) {
    offset = 4;
    for (int_fast8_t f = 0; f < num_cols; f++) {
      row.values[f] = read_string(&offset);
    }
  }
  return res;
}

#endif
