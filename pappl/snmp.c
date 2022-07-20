//
// SNMP functions for the Printer Application Framework.
//
// Copyright © 2020-2022 by Michael R Sweet.
// Copyright © 2007-2019 by Apple Inc.
// Copyright © 2006-2007 by Easy Software Products, all rights reserved.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers.
//

#include "snmp-private.h"


//
// Macros...
//

#define _(x) x
#define snmp_set_error(p,m) p->error = m


//
// Local functions...
//

static int		asn1_decode_snmp(unsigned char *buffer, size_t len, _pappl_snmp_t *packet);
static int		asn1_encode_snmp(unsigned char *buffer, size_t len, _pappl_snmp_t *packet);
static int		asn1_get_integer(unsigned char **buffer, unsigned char *bufend, unsigned length);
static int		asn1_get_oid(unsigned char **buffer, unsigned char *bufend, unsigned length, int *oid, int oidsize);
static int		asn1_get_packed(unsigned char **buffer, unsigned char *bufend);
static char		*asn1_get_string(unsigned char **buffer, unsigned char *bufend, unsigned length, char *string, size_t strsize);
static unsigned		asn1_get_length(unsigned char **buffer, unsigned char *bufend);
static int		asn1_get_type(unsigned char **buffer, unsigned char *bufend);
static void		asn1_set_integer(unsigned char **buffer, int integer);
static void		asn1_set_length(unsigned char **buffer, unsigned length);
static void		asn1_set_oid(unsigned char **buffer, const int *oid);
static void		asn1_set_packed(unsigned char **buffer, int integer);
static unsigned		asn1_size_integer(int integer);
static unsigned		asn1_size_length(unsigned length);
static unsigned		asn1_size_oid(const int *oid);
static unsigned		asn1_size_packed(int integer);


//
// '_papplSNMPClose()' - Close a SNMP socket.
//

void
_papplSNMPClose(int fd)			// I - SNMP socket file descriptor
{
  httpAddrClose(NULL, fd);
}


//
// '_papplSNMPCopyOID()' - Copy an OID.
//
// The array pointed to by "src" is terminated by the value -1.
//

int *					// O - New OID
_papplSNMPCopyOID(int       *dst,	// I - Destination OID
		  const int *src,	// I - Source OID
		  int       dstsize)	// I - Number of integers in dst
{
  int	i;				// Looping var


  for (i = 0, dstsize --; i < dstsize && src[i] >= 0; i ++)
    dst[i] = src[i];

  dst[i] = -1;

  return (dst);
}


//
// '_papplSNMPIsOID()' - Test whether a SNMP response contains the specified OID.
//
// The array pointed to by "oid" is terminated by the value -1.
//

int					// O - 1 if equal, 0 if not equal
_papplSNMPIsOID(_pappl_snmp_t *packet,	// I - Response packet
               const int   *oid)	// I - OID
{
  int	i;				// Looping var


  // Range check input...
  if (!packet || !oid)
    return (0);

  // Compare OIDs...
  for (i = 0; i < _PAPPL_SNMP_MAX_OID && oid[i] >= 0 && packet->object_name[i] >= 0; i ++)
  {
    if (oid[i] != packet->object_name[i])
      return (0);
  }

  return (i < _PAPPL_SNMP_MAX_OID && oid[i] == packet->object_name[i]);
}


//
// '_papplSNMPIsOIDPrefixed()' - Test whether a SNMP response uses the specified
//                               OID prefix.
//
// The array pointed to by "prefix" is terminated by the value -1.
//

int					// O - 1 if prefixed, 0 if not prefixed
_papplSNMPIsOIDPrefixed(
    _pappl_snmp_t *packet,		// I - Response packet
    const int   *prefix)		// I - OID prefix
{
  int	i;				// Looping var


  // Range check input...
  if (!packet || !prefix)
    return (0);

  // Compare OIDs...
  for (i = 0; i < _PAPPL_SNMP_MAX_OID && prefix[i] >= 0 && packet->object_name[i] >= 0; i ++)
  {
    if (prefix[i] != packet->object_name[i])
      return (0);
  }

  return (i < _PAPPL_SNMP_MAX_OID);
}


//
// '_papplSNMPOIDToString()' - Convert an OID to a string.
//

char *					// O - New string or @code NULL@ on error
_papplSNMPOIDToString(const int *src,	// I - OID
                     char      *dst,	// I - String buffer
                     size_t    dstsize)	// I - Size of string buffer
{
  char	*dstptr,			// Pointer into string buffer
	*dstend;			// End of string buffer


  // Range check input...
  if (!src || !dst || dstsize < 4)
    return (NULL);

  // Loop through the OID array and build a string...
  for (dstptr = dst, dstend = dstptr + dstsize - 1; *src >= 0 && dstptr < dstend; src ++, dstptr += strlen(dstptr))
    snprintf(dstptr, (size_t)(dstend - dstptr + 1), ".%d", *src);

  if (*src >= 0)
    return (NULL);
  else
    return (dst);
}


//
// '_papplSNMPOpen()' - Open a SNMP socket.
//

int					// O - SNMP socket file descriptor
_papplSNMPOpen(int family)		// I - Address family - @code AF_INET@ or @code AF_INET6@
{
  int		fd;			// SNMP socket file descriptor
  int		val;			// Socket option value


  // Create the SNMP socket...
  if ((fd = (int)socket(family, SOCK_DGRAM, 0)) < 0)
    return (-1);

  // Set the "broadcast" flag...
  val = 1;

  if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, (const char *)&val, sizeof(val)))
  {
    close(fd);

    return (-1);
  }

  return (fd);
}


//
// '_papplSNMPRead()' - Read and parse a SNMP response.
//
// If "timeout" is negative, @code _papplSNMPRead@ will wait for a response
// indefinitely.
//

_pappl_snmp_t *				// O - SNMP packet or @code NULL@ if none
_papplSNMPRead(int           fd,	// I - SNMP socket file descriptor
	       _pappl_snmp_t *packet,	// I - SNMP packet buffer
	       double        timeout)	// I - Timeout in seconds
{
  unsigned char	buffer[_PAPPL_SNMP_MAX_PACKET];
					// Data packet
  ssize_t	bytes;			// Number of bytes received
  socklen_t	addrlen;		// Source address length
  http_addr_t	address;		// Source address


  // Range check input...
  if (fd < 0 || !packet)
    return (NULL);

  // Optionally wait for a response...
  if (timeout >= 0.0)
  {
    int			ready;		// Data ready on socket?
    struct pollfd	pfd;		// Polled file descriptor

    pfd.fd     = fd;
    pfd.events = POLLIN;

    while ((ready = poll(&pfd, 1, (int)(timeout * 1000.0))) < 0 &&
           (errno == EINTR || errno == EAGAIN))
      ;					// Wait for poll to complete...

    // If we don't have any data ready, return right away...
    if (ready <= 0)
      return (NULL);
  }

  // Read the response data...
  addrlen = sizeof(address);

  if ((bytes = recvfrom(fd, buffer, sizeof(buffer), 0, (void *)&address, &addrlen)) < 0)
    return (NULL);

  // Look for the response status code in the SNMP message header...
  asn1_decode_snmp(buffer, (size_t)bytes, packet);

  memcpy(&(packet->address), &address, sizeof(packet->address));

  // Return decoded data packet...
  return (packet);
}


//
// '_papplSNMPWalk()' - Enumerate a group of OIDs.
//
// This function queries all of the OIDs with the specified OID prefix,
// calling the "cb" function for every response that is received.
//
// The array pointed to by "prefix" is terminated by the value -1.
//
// If "timeout" is negative, @code _papplSNMPWalk@ will wait for a response
// indefinitely.
//

int					// O - Number of OIDs found or -1 on error
_papplSNMPWalk(
    int              fd,		// I - SNMP socket
    http_addr_t      *address,		// I - Address to query
    int              version,		// I - SNMP version
    const char       *community,	// I - Community name
    const int        *prefix,		// I - OID prefix
    double           timeout,		// I - Timeout for each response in seconds
    _pappl_snmp_cb_t cb,		// I - Function to call for each response
    void             *data)		// I - User data pointer that is passed to the callback function
{
  int		count = 0;		// Number of OIDs found
  unsigned	request_id = 0;		// Current request ID
  _pappl_snmp_t	packet;			// Current response packet
  int		lastoid[_PAPPL_SNMP_MAX_OID];
					// Last OID we got
#ifdef DEBUG
  char		temp[1024];		// OID returned
#endif // DEBUG


  // Range check input...
  _PAPPL_DEBUG("_papplSNMPWalk(fd=%d, address=%p, version=%d, community=\"%s\", prefix=%s, timeout=%g, cb=%p, data=%p)\n", fd, address, version, community ? community : "(null)", _papplSNMPOIDToString(prefix, temp, sizeof(temp)), timeout, cb, data);

  if (fd < 0 || !address || version != _PAPPL_SNMP_VERSION_1 || !community || !prefix || !cb)
    return (-1);

  // Copy the OID prefix and then loop until we have no more OIDs...
  _papplSNMPCopyOID(packet.object_name, prefix, _PAPPL_SNMP_MAX_OID);
  lastoid[0] = -1;

  for (;;)
  {
    request_id ++;

    if (!_papplSNMPWrite(fd, address, version, community, _PAPPL_ASN1_GET_NEXT_REQUEST, request_id, packet.object_name))
    {
      _PAPPL_DEBUG("_papplSNMPWalk: Unable to send Get-Next-Request.\n");
      return (-1);
    }

    if (!_papplSNMPRead(fd, &packet, timeout))
    {
      _PAPPL_DEBUG("_papplSNMPWalk: Unable to read response.\n");
      return (-1);
    }

    _PAPPL_DEBUG("_papplSNMPWalk: OID %s.\n", _papplSNMPOIDToString(packet.object_name, temp, sizeof(temp)));

    if (!_papplSNMPIsOIDPrefixed(&packet, prefix) || _papplSNMPIsOID(&packet, lastoid))
    {
      _PAPPL_DEBUG("_papplSNMPWalk: Different prefix or same OID as last, returning %d.\n", count);
      return (count);
    }

    if (packet.error || packet.error_status)
    {
      _PAPPL_DEBUG("_papplSNMPWalk: error=\"%s\", error_status=%d, returning %d.\n", packet.error, packet.error_status, count > 0 ? count : -1);
      return (count > 0 ? count : -1);
    }

    _papplSNMPCopyOID(lastoid, packet.object_name, _PAPPL_SNMP_MAX_OID);

    count ++;

    (*cb)(&packet, data);
  }
}


//
// '_papplSNMPWrite()' - Send an SNMP query packet.
//
// The array pointed to by "oid" is terminated by the value -1.
//

int					// O - 1 on success, 0 on error
_papplSNMPWrite(
    int            fd,			// I - SNMP socket
    http_addr_t    *address,		// I - Address to send to
    int            version,		// I - SNMP version
    const char     *community,		// I - Community name
    _pappl_asn1_t  request_type,	// I - Request type
    const unsigned request_id,		// I - Request ID
    const int      *oid)		// I - OID
{
  int		i;			// Looping var
  _pappl_snmp_t	packet;			// SNMP message packet
  unsigned char	buffer[_PAPPL_SNMP_MAX_PACKET];
					// SNMP message buffer
  ssize_t	bytes;			// Size of message
  http_addr_t	temp;			// Copy of address


  // Range check input...
  if (fd < 0 || !address || version != _PAPPL_SNMP_VERSION_1 || !community || (request_type != _PAPPL_ASN1_GET_REQUEST && request_type != _PAPPL_ASN1_GET_NEXT_REQUEST) || request_id < 1 || !oid)
    return (0);

  // Create the SNMP message...
  memset(&packet, 0, sizeof(packet));

  packet.version      = version;
  packet.request_type = request_type;
  packet.request_id   = request_id;
  packet.object_type  = _PAPPL_ASN1_NULL_VALUE;

  papplCopyString(packet.community, community, sizeof(packet.community));

  for (i = 0; i < (_PAPPL_SNMP_MAX_OID - 1) && oid[i] >= 0; i ++)
    packet.object_name[i] = oid[i];
  packet.object_name[i] = -1;

  if (oid[i] >= 0)
  {
    errno = E2BIG;
    return (0);
  }

  bytes = asn1_encode_snmp(buffer, sizeof(buffer), &packet);

  if (bytes < 0)
  {
    errno = E2BIG;
    return (0);
  }

  // Send the message...
  temp               = *address;
  temp.ipv4.sin_port = htons(_PAPPL_SNMP_PORT);

#if _WIN32
  return (sendto(fd, buffer, (int)bytes, 0, (void *)&temp, (socklen_t)httpAddrGetLength(&temp)) == bytes);
#else
  return (sendto(fd, buffer, (size_t)bytes, 0, (void *)&temp, (socklen_t)httpAddrGetLength(&temp)) == bytes);
#endif // _WIN32
}


//
// 'asn1_decode_snmp()' - Decode a SNMP packet.
//

static int				// O - 0 on success, -1 on error
asn1_decode_snmp(
    unsigned char *buffer,		// I - Buffer
    size_t        len,			// I - Size of buffer
    _pappl_snmp_t *packet)		// I - SNMP packet
{
  unsigned char	*bufptr,		// Pointer into the data
		*bufend;		// End of data
  unsigned	length;			// Length of value


  // Initialize the decoding...
  memset(packet, 0, sizeof(_pappl_snmp_t));
  packet->object_name[0] = -1;

  bufptr = buffer;
  bufend = buffer + len;

  if (asn1_get_type(&bufptr, bufend) != _PAPPL_ASN1_SEQUENCE)
  {
    snmp_set_error(packet, _("Packet does not start with SEQUENCE"));
  }
  else if (asn1_get_length(&bufptr, bufend) == 0)
  {
    snmp_set_error(packet, _("SEQUENCE uses indefinite length"));
  }
  else if (asn1_get_type(&bufptr, bufend) != _PAPPL_ASN1_INTEGER)
  {
    snmp_set_error(packet, _("No version number"));
  }
  else if ((length = asn1_get_length(&bufptr, bufend)) == 0)
  {
    snmp_set_error(packet, _("Version uses indefinite length"));
  }
  else if ((packet->version = asn1_get_integer(&bufptr, bufend, length)) != _PAPPL_SNMP_VERSION_1)
  {
    snmp_set_error(packet, _("Bad SNMP version number"));
  }
  else if (asn1_get_type(&bufptr, bufend) != _PAPPL_ASN1_OCTET_STRING)
  {
    snmp_set_error(packet, _("No community name"));
  }
  else if ((length = asn1_get_length(&bufptr, bufend)) == 0)
  {
    snmp_set_error(packet, _("Community name uses indefinite length"));
  }
  else
  {
    asn1_get_string(&bufptr, bufend, length, packet->community, sizeof(packet->community));

    if ((packet->request_type = (_pappl_asn1_t)asn1_get_type(&bufptr, bufend)) != _PAPPL_ASN1_GET_RESPONSE)
    {
      snmp_set_error(packet, _("Packet does not contain a Get-Response-PDU"));
    }
    else if (asn1_get_length(&bufptr, bufend) == 0)
    {
      snmp_set_error(packet, _("Get-Response-PDU uses indefinite length"));
    }
    else if (asn1_get_type(&bufptr, bufend) != _PAPPL_ASN1_INTEGER)
    {
      snmp_set_error(packet, _("No request-id"));
    }
    else if ((length = asn1_get_length(&bufptr, bufend)) == 0)
    {
      snmp_set_error(packet, _("request-id uses indefinite length"));
    }
    else
    {
      packet->request_id = (unsigned)asn1_get_integer(&bufptr, bufend, length);

      if (asn1_get_type(&bufptr, bufend) != _PAPPL_ASN1_INTEGER)
      {
	snmp_set_error(packet, _("No error-status"));
      }
      else if ((length = asn1_get_length(&bufptr, bufend)) == 0)
      {
	snmp_set_error(packet, _("error-status uses indefinite length"));
      }
      else
      {
	packet->error_status = asn1_get_integer(&bufptr, bufend, length);

	if (asn1_get_type(&bufptr, bufend) != _PAPPL_ASN1_INTEGER)
	{
	  snmp_set_error(packet, _("No error-index"));
	}
	else if ((length = asn1_get_length(&bufptr, bufend)) == 0)
	{
	  snmp_set_error(packet, _("error-index uses indefinite length"));
	}
	else
	{
	  packet->error_index = asn1_get_integer(&bufptr, bufend, length);

          if (asn1_get_type(&bufptr, bufend) != _PAPPL_ASN1_SEQUENCE)
	  {
	    snmp_set_error(packet, _("No variable-bindings SEQUENCE"));
	  }
	  else if (asn1_get_length(&bufptr, bufend) == 0)
	  {
	    snmp_set_error(packet, _("variable-bindings uses indefinite length"));
	  }
	  else if (asn1_get_type(&bufptr, bufend) != _PAPPL_ASN1_SEQUENCE)
	  {
	    snmp_set_error(packet, _("No VarBind SEQUENCE"));
	  }
	  else if (asn1_get_length(&bufptr, bufend) == 0)
	  {
	    snmp_set_error(packet, _("VarBind uses indefinite length"));
	  }
	  else if (asn1_get_type(&bufptr, bufend) != _PAPPL_ASN1_OID)
	  {
	    snmp_set_error(packet, _("No name OID"));
	  }
	  else if ((length = asn1_get_length(&bufptr, bufend)) == 0)
	  {
	    snmp_set_error(packet, _("Name OID uses indefinite length"));
          }
          else
	  {
	    asn1_get_oid(&bufptr, bufend, length, packet->object_name, _PAPPL_SNMP_MAX_OID);

            packet->object_type = (_pappl_asn1_t)asn1_get_type(&bufptr, bufend);

	    if ((length = asn1_get_length(&bufptr, bufend)) == 0 && packet->object_type != _PAPPL_ASN1_NULL_VALUE && packet->object_type != _PAPPL_ASN1_OCTET_STRING)
	    {
	      snmp_set_error(packet, _("Value uses indefinite length"));
	    }
	    else
	    {
	      switch (packet->object_type)
	      {
	        case _PAPPL_ASN1_BOOLEAN :
		    packet->object_value.boolean = asn1_get_integer(&bufptr, bufend, length);
	            break;

	        case _PAPPL_ASN1_INTEGER :
		    packet->object_value.integer = asn1_get_integer(&bufptr, bufend, length);
	            break;

		case _PAPPL_ASN1_NULL_VALUE :
		    break;

	        case _PAPPL_ASN1_OCTET_STRING :
	        case _PAPPL_ASN1_BIT_STRING :
	        case _PAPPL_ASN1_HEX_STRING :
		    packet->object_value.string.num_bytes = length;
		    asn1_get_string(&bufptr, bufend, length, (char *)packet->object_value.string.bytes, sizeof(packet->object_value.string.bytes));
	            break;

	        case _PAPPL_ASN1_OID :
		    asn1_get_oid(&bufptr, bufend, length, packet->object_value.oid, _PAPPL_SNMP_MAX_OID);
	            break;

	        case _PAPPL_ASN1_COUNTER :
		    packet->object_value.counter = asn1_get_integer(&bufptr, bufend, length);
	            break;

	        case _PAPPL_ASN1_GAUGE :
		    packet->object_value.gauge = (unsigned)asn1_get_integer(&bufptr, bufend, length);
	            break;

	        case _PAPPL_ASN1_TIMETICKS :
		    packet->object_value.timeticks = (unsigned)asn1_get_integer(&bufptr, bufend, length);
	            break;

                default :
		    snmp_set_error(packet, _("Unsupported value type"));
		    break;
	      }
	    }
          }
	}
      }
    }
  }

  return (packet->error ? -1 : 0);
}


//
// 'asn1_encode_snmp()' - Encode a SNMP packet.
//

static int				// O - Length on success, -1 on error
asn1_encode_snmp(
    unsigned char *buffer,		// I - Buffer
    size_t        bufsize,		// I - Size of buffer
    _pappl_snmp_t *packet)		// I - SNMP packet
{
  unsigned char	*bufptr;		// Pointer into buffer
  unsigned	total,			// Total length
		msglen,			// Length of entire message
		commlen,		// Length of community string
		reqlen,			// Length of request
		listlen,		// Length of variable list
		varlen,			// Length of variable
		namelen,		// Length of object name OID
		valuelen;		// Length of object value


  // Get the lengths of the community string, OID, and message...
  namelen = asn1_size_oid(packet->object_name);

  switch (packet->object_type)
  {
    case _PAPPL_ASN1_NULL_VALUE :
        valuelen = 0;
	break;

    case _PAPPL_ASN1_BOOLEAN :
        valuelen = asn1_size_integer(packet->object_value.boolean);
	break;

    case _PAPPL_ASN1_INTEGER :
        valuelen = asn1_size_integer(packet->object_value.integer);
	break;

    case _PAPPL_ASN1_OCTET_STRING :
        valuelen = packet->object_value.string.num_bytes;
	break;

    case _PAPPL_ASN1_OID :
        valuelen = asn1_size_oid(packet->object_value.oid);
	break;

    default :
        packet->error = "Unknown object type";
        return (-1);
  }

  varlen  = 1 + asn1_size_length(namelen) + namelen +
            1 + asn1_size_length(valuelen) + valuelen;
  listlen = 1 + asn1_size_length(varlen) + varlen;
  reqlen  = 2 + asn1_size_integer((int)packet->request_id) +
            2 + asn1_size_integer(packet->error_status) +
            2 + asn1_size_integer(packet->error_index) +
            1 + asn1_size_length(listlen) + listlen;
  commlen = (unsigned)strlen(packet->community);
  msglen  = 2 + asn1_size_integer(packet->version) +
            1 + asn1_size_length(commlen) + commlen +
	    1 + asn1_size_length(reqlen) + reqlen;
  total   = 1 + asn1_size_length(msglen) + msglen;

  if (total > bufsize)
  {
    packet->error = "Message too large for buffer";
    return (-1);
  }

  // Then format the message...
  bufptr = buffer;

  *bufptr++ = _PAPPL_ASN1_SEQUENCE;	// SNMPv1 message header
  asn1_set_length(&bufptr, msglen);

  asn1_set_integer(&bufptr, packet->version);
					// version

  *bufptr++ = _PAPPL_ASN1_OCTET_STRING;	// community
  asn1_set_length(&bufptr, commlen);
  memcpy(bufptr, packet->community, commlen);
  bufptr += commlen;

  *bufptr++ = (unsigned char)packet->request_type;	// Get-Request-PDU/Get-Next-Request-PDU
  asn1_set_length(&bufptr, reqlen);

  asn1_set_integer(&bufptr, (int)packet->request_id);

  asn1_set_integer(&bufptr, packet->error_status);

  asn1_set_integer(&bufptr, packet->error_index);

  *bufptr++ = _PAPPL_ASN1_SEQUENCE;	// variable-bindings
  asn1_set_length(&bufptr, listlen);

  *bufptr++ = _PAPPL_ASN1_SEQUENCE;	// variable
  asn1_set_length(&bufptr, varlen);

  asn1_set_oid(&bufptr, packet->object_name);
					// ObjectName

  switch (packet->object_type)
  {
    case _PAPPL_ASN1_NULL_VALUE :
	*bufptr++ = _PAPPL_ASN1_NULL_VALUE;
					// ObjectValue
	*bufptr++ = 0;			// Length
        break;

    case _PAPPL_ASN1_BOOLEAN :
        asn1_set_integer(&bufptr, packet->object_value.boolean);
	break;

    case _PAPPL_ASN1_INTEGER :
        asn1_set_integer(&bufptr, packet->object_value.integer);
	break;

    case _PAPPL_ASN1_OCTET_STRING :
        *bufptr++ = _PAPPL_ASN1_OCTET_STRING;
	asn1_set_length(&bufptr, valuelen);
	memcpy(bufptr, packet->object_value.string.bytes, valuelen);
	bufptr += valuelen;
	break;

    case _PAPPL_ASN1_OID :
        asn1_set_oid(&bufptr, packet->object_value.oid);
	break;

    default :
        break;
  }

  return ((int)(bufptr - buffer));
}


//
// 'asn1_get_integer()' - Get an integer value.
//

static int				// O  - Integer value
asn1_get_integer(
    unsigned char **buffer,		// IO - Pointer in buffer
    unsigned char *bufend,		// I  - End of buffer
    unsigned      length)		// I  - Length of value
{
  int	value;				// Integer value


  if (*buffer >= bufend)
    return (0);

  if (length > sizeof(int))
  {
    if (length > (unsigned)(bufend - *buffer))
      *buffer = bufend;
    else
      (*buffer) += length;

    return (0);
  }

  for (value = (**buffer & 0x80) ? ~0 : 0; length > 0 && *buffer < bufend; length --, (*buffer) ++)
    value = ((value & 0xffffff) << 8) | **buffer;

  return (value);
}


//
// 'asn1_get_length()' - Get a value length.
//

static unsigned				// O  - Length
asn1_get_length(unsigned char **buffer,	// IO - Pointer in buffer
		unsigned char *bufend)	// I  - End of buffer
{
  unsigned	length;			// Length


  if (*buffer >= bufend)
    return (0);

  length = **buffer;
  (*buffer) ++;

  if (length & 128)
  {
    int	count;				// Number of bytes for length

    if ((count = length & 127) > sizeof(unsigned))
    {
      if (count > (bufend - *buffer))
	*buffer = bufend;
      else
	(*buffer) += count;

      return (0);
    }

    for (length = 0; count > 0 && *buffer < bufend; count --, (*buffer) ++)
      length = (length << 8) | **buffer;
  }

  return (length);
}


//
// 'asn1_get_oid()' - Get an OID value.
//

static int				// O  - Number of OIDs
asn1_get_oid(
    unsigned char **buffer,		// IO - Pointer in buffer
    unsigned char *bufend,		// I  - End of buffer
    unsigned      length,		// I  - Length of value
    int           *oid,			// I  - OID buffer
    int           oidsize)		// I  - Size of OID buffer
{
  unsigned char	*valend;		// End of value
  int		*oidptr,		// Current OID
		*oidend;		// End of OID buffer
  int		number;			// OID number


  if (*buffer >= bufend)
  {
    return (0);
  }
  else if (length > (unsigned)(bufend - *buffer))
  {
    *buffer = bufend;
    return (0);
  }

  valend = *buffer + length;
  oidptr = oid;
  oidend = oid + oidsize - 1;

  if (valend > bufend)
    valend = bufend;

  number = asn1_get_packed(buffer, valend);

  if (number < 80)
  {
    *oidptr++ = number / 40;
    number    = number % 40;
    *oidptr++ = number;
  }
  else
  {
    *oidptr++ = 2;
    number    -= 80;
    *oidptr++ = number;
  }

  while (*buffer < valend)
  {
    number = asn1_get_packed(buffer, valend);

    if (oidptr < oidend)
      *oidptr++ = number;
  }

  *oidptr = -1;

  return ((int)(oidptr - oid));
}


//
// 'asn1_get_packed()' - Get a packed integer value.
//

static int				// O  - Value
asn1_get_packed(
    unsigned char **buffer,		// IO - Pointer in buffer
    unsigned char *bufend)		// I  - End of buffer
{
  int	value;				// Value


  if (*buffer >= bufend)
    return (0);

  value = 0;

  while (*buffer < bufend && (**buffer & 128))
  {
    value = (value << 7) | (**buffer & 127);
    (*buffer) ++;
  }

  if (*buffer < bufend)
  {
    value = (value << 7) | **buffer;
    (*buffer) ++;
  }

  return (value);
}


//
// 'asn1_get_string()' - Get a string value.
//

static char *				// O  - String
asn1_get_string(
    unsigned char **buffer,		// IO - Pointer in buffer
    unsigned char *bufend,		// I  - End of buffer
    unsigned      length,		// I  - Value length
    char          *string,		// I  - String buffer
    size_t        strsize)		// I  - String buffer size
{
  if (*buffer >= bufend)
    return (NULL);

  if (length > (unsigned)(bufend - *buffer))
    length = (unsigned)(bufend - *buffer);

  if (length < strsize)
  {
    // String is smaller than the buffer...
    if (length > 0)
      memcpy(string, *buffer, length);

    string[length] = '\0';
  }
  else
  {
    // String is larger than the buffer...
    memcpy(string, *buffer, strsize - 1);
    string[strsize - 1] = '\0';
  }

  if (length > 0)
    (*buffer) += length;

  return (string);
}


//
// 'asn1_get_type()' - Get a value type.
//

static int				// O  - Type
asn1_get_type(unsigned char **buffer,	// IO - Pointer in buffer
	      unsigned char *bufend)	// I  - End of buffer
{
  int	type;				// Type


  if (*buffer >= bufend)
    return (0);

  type = **buffer;
  (*buffer) ++;

  if ((type & 31) == 31)
    type = asn1_get_packed(buffer, bufend);

  return (type);
}


//
// 'asn1_set_integer()' - Set an integer value.
//

static void
asn1_set_integer(unsigned char **buffer,// IO - Pointer in buffer
                 int           integer)	// I  - Integer value
{
  **buffer = _PAPPL_ASN1_INTEGER;
  (*buffer) ++;

  if (integer > 0x7fffff || integer < -0x800000)
  {
    **buffer = 4;
    (*buffer) ++;
    **buffer = (unsigned char)(integer >> 24);
    (*buffer) ++;
    **buffer = (unsigned char)(integer >> 16);
    (*buffer) ++;
    **buffer = (unsigned char)(integer >> 8);
    (*buffer) ++;
    **buffer = (unsigned char)integer;
    (*buffer) ++;
  }
  else if (integer > 0x7fff || integer < -0x8000)
  {
    **buffer = 3;
    (*buffer) ++;
    **buffer = (unsigned char)(integer >> 16);
    (*buffer) ++;
    **buffer = (unsigned char)(integer >> 8);
    (*buffer) ++;
    **buffer = (unsigned char)integer;
    (*buffer) ++;
  }
  else if (integer > 0x7f || integer < -0x80)
  {
    **buffer = 2;
    (*buffer) ++;
    **buffer = (unsigned char)(integer >> 8);
    (*buffer) ++;
    **buffer = (unsigned char)integer;
    (*buffer) ++;
  }
  else
  {
    **buffer = 1;
    (*buffer) ++;
    **buffer = (unsigned char)integer;
    (*buffer) ++;
  }
}


//
// 'asn1_set_length()' - Set a value length.
//

static void
asn1_set_length(unsigned char **buffer,	// IO - Pointer in buffer
		unsigned      length)	// I  - Length value
{
  if (length > 255)
  {
    **buffer = 0x82;			// 2-byte length
    (*buffer) ++;
    **buffer = (unsigned char)(length >> 8);
    (*buffer) ++;
    **buffer = (unsigned char)length;
    (*buffer) ++;
  }
  else if (length > 127)
  {
    **buffer = 0x81;			// 1-byte length
    (*buffer) ++;
    **buffer = (unsigned char)length;
    (*buffer) ++;
  }
  else
  {
    **buffer = (unsigned char)length;	// Length
    (*buffer) ++;
  }
}


//
// 'asn1_set_oid()' - Set an OID value.
//

static void
asn1_set_oid(unsigned char **buffer,	// IO - Pointer in buffer
             const int     *oid)	// I  - OID value
{
  **buffer = _PAPPL_ASN1_OID;
  (*buffer) ++;

  asn1_set_length(buffer, asn1_size_oid(oid));

  if (oid[1] < 0)
  {
    asn1_set_packed(buffer, oid[0] * 40);
    return;
  }

  asn1_set_packed(buffer, oid[0] * 40 + oid[1]);

  for (oid += 2; *oid >= 0; oid ++)
    asn1_set_packed(buffer, *oid);
}


//
// 'asn1_set_packed()' - Set a packed integer value.
//

static void
asn1_set_packed(unsigned char **buffer,	// IO - Pointer in buffer
		int           integer)	// I  - Integer value
{
  if (integer > 0xfffffff)
  {
    **buffer = ((integer >> 28) & 0x7f) | 0x80;
    (*buffer) ++;
  }

  if (integer > 0x1fffff)
  {
    **buffer = ((integer >> 21) & 0x7f) | 0x80;
    (*buffer) ++;
  }

  if (integer > 0x3fff)
  {
    **buffer = ((integer >> 14) & 0x7f) | 0x80;
    (*buffer) ++;
  }

  if (integer > 0x7f)
  {
    **buffer = ((integer >> 7) & 0x7f) | 0x80;
    (*buffer) ++;
  }

  **buffer = integer & 0x7f;
  (*buffer) ++;
}


//
// 'asn1_size_integer()' - Figure out the number of bytes needed for an
//                         integer value.
//

static unsigned				// O - Size in bytes
asn1_size_integer(int integer)		// I - Integer value
{
  if (integer > 0x7fffff || integer < -0x800000)
    return (4);
  else if (integer > 0x7fff || integer < -0x8000)
    return (3);
  else if (integer > 0x7f || integer < -0x80)
    return (2);
  else
    return (1);
}


//
// 'asn1_size_length()' - Figure out the number of bytes needed for a
//                        length value.
//

static unsigned				// O - Size in bytes
asn1_size_length(unsigned length)	// I - Length value
{
  if (length > 0xff)
    return (3);
  else if (length > 0x7f)
    return (2);
  else
    return (1);
}


//
// 'asn1_size_oid()' - Figure out the number of bytes needed for an
//                     OID value.
//

static unsigned				// O - Size in bytes
asn1_size_oid(const int *oid)		// I - OID value
{
  unsigned	length;			// Length of value


  if (oid[1] < 0)
    return (asn1_size_packed(oid[0] * 40));

  for (length = asn1_size_packed(oid[0] * 40 + oid[1]), oid += 2; *oid >= 0; oid ++)
    length += asn1_size_packed(*oid);

  return (length);
}


//
// 'asn1_size_packed()' - Figure out the number of bytes needed for a
//                        packed integer value.
//

static unsigned				// O - Size in bytes
asn1_size_packed(int integer)		// I - Integer value
{
  if (integer > 0xfffffff)
    return (5);
  else if (integer > 0x1fffff)
    return (4);
  else if (integer > 0x3fff)
    return (3);
  else if (integer > 0x7f)
    return (2);
  else
    return (1);
}
