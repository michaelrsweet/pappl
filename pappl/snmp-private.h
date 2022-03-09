//
// Private SNMP definitions for the Printer Application Framework.
//
// Copyright © 2020-2022 by Michael R Sweet
// Copyright © 2007-2014 by Apple Inc.
// Copyright © 2006-2007 by Easy Software Products, all rights reserved.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _PAPPL_SNMP_PRIVATE_H_
#  define _PAPPL_SNMP_PRIVATE_H_
#  include "base-private.h"


//
// Constants...
//

#define _PAPPL_SNMP_COMMUNITY	"public"// SNMP default community name
#define _PAPPL_SNMP_PORT	161	// SNMP well-known port
#define _PAPPL_SNMP_MAX_COMMUNITY 512	// Maximum size of community name
#define _PAPPL_SNMP_MAX_OID	128	// Maximum number of OID numbers
#define _PAPPL_SNMP_MAX_PACKET	1472	// Maximum size of SNMP packet
#define _PAPPL_SNMP_MAX_STRING	1024	// Maximum size of string
#define _PAPPL_SNMP_VERSION_1	0	// SNMPv1


//
// Types...
//

enum _pappl_asn1_e			// ASN1 request/object types
{
  _PAPPL_ASN1_END_OF_CONTENTS = 0x00,		// End-of-contents
  _PAPPL_ASN1_BOOLEAN = 0x01,			// BOOLEAN
  _PAPPL_ASN1_INTEGER = 0x02,			// INTEGER or ENUMERATION
  _PAPPL_ASN1_BIT_STRING = 0x03,		// BIT STRING
  _PAPPL_ASN1_OCTET_STRING = 0x04,		// OCTET STRING
  _PAPPL_ASN1_NULL_VALUE = 0x05,		// NULL VALUE
  _PAPPL_ASN1_OID = 0x06,			// OBJECT IDENTIFIER
  _PAPPL_ASN1_SEQUENCE = 0x30,			// SEQUENCE
  _PAPPL_ASN1_HEX_STRING = 0x40,		// Binary string aka Hex-STRING
  _PAPPL_ASN1_COUNTER = 0x41,			// 32-bit unsigned aka Counter32
  _PAPPL_ASN1_GAUGE = 0x42,			// 32-bit unsigned aka Gauge32
  _PAPPL_ASN1_TIMETICKS = 0x43,			// 32-bit unsigned aka Timeticks32
  _PAPPL_ASN1_GET_REQUEST = 0xa0,		// GetRequest-PDU
  _PAPPL_ASN1_GET_NEXT_REQUEST = 0xa1,		// GetNextRequest-PDU
  _PAPPL_ASN1_GET_RESPONSE = 0xa2		// GetResponse-PDU
};
typedef enum _pappl_asn1_e _pappl_asn1_t;// ASN1 request/object types

typedef struct _pappl_snmp_string_s	// String value
{
  unsigned char	bytes[_PAPPL_SNMP_MAX_STRING];
						// Bytes in string
  unsigned	num_bytes;			// Number of bytes
} _pappl_snmp_string_t;

union _pappl_snmp_value_u		// Object value
{
  int		boolean;			// Boolean value
  int		integer;			// Integer value
  int		counter;			// Counter value
  unsigned	gauge;				// Gauge value
  unsigned	timeticks;			// Timeticks  value
  int		oid[_PAPPL_SNMP_MAX_OID];	// OID value
  _pappl_snmp_string_t string;			// String value
};

typedef struct _pappl_snmp_s		// SNMP data packet
{
  const char	*error;				// Encode/decode error
  http_addr_t	address;			// Source address
  int		version;			// Version number
  char		community[_PAPPL_SNMP_MAX_COMMUNITY];
						// Community name
  _pappl_asn1_t	request_type;			// Request type
  unsigned	request_id;			// request-id value
  int		error_status;			// error-status value
  int		error_index;			// error-index value
  int		object_name[_PAPPL_SNMP_MAX_OID];
						// object-name value
  _pappl_asn1_t	object_type;			// object-value type
  union _pappl_snmp_value_u object_value;	// object-value value
} _pappl_snmp_t;

typedef void (*_pappl_snmp_cb_t)(_pappl_snmp_t *packet, void *data);
					// SNMP callback


//
// Prototypes...
//

extern void		_papplSNMPClose(int fd) _PAPPL_PRIVATE;
extern int		*_papplSNMPCopyOID(int *dst, const int *src, int dstsize) _PAPPL_PRIVATE;
extern int		_papplSNMPIsOID(_pappl_snmp_t *packet, const int *oid) _PAPPL_PRIVATE;
extern int		_papplSNMPIsOIDPrefixed(_pappl_snmp_t *packet, const int *prefix) _PAPPL_PRIVATE;
extern char		*_papplSNMPOIDToString(const int *src, char *dst, size_t dstsize) _PAPPL_PRIVATE;
extern int		_papplSNMPOpen(int family) _PAPPL_PRIVATE;
extern _pappl_snmp_t	*_papplSNMPRead(int fd, _pappl_snmp_t *packet, double timeout) _PAPPL_PRIVATE;
extern int		_papplSNMPWalk(int fd, http_addr_t *address, int version, const char *community, const int *prefix, double timeout, _pappl_snmp_cb_t cb, void *data) _PAPPL_PRIVATE;
extern int		_papplSNMPWrite(int fd, http_addr_t *address, int version, const char *community, _pappl_asn1_t request_type, const unsigned request_id, const int *oid) _PAPPL_PRIVATE;

#endif // !_PAPPL_SNMP_PRIVATE_H_
