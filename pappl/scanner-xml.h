//
// XML utilities for the Printer Application Framework
//
// Copyright © 2019-2020 by Michael R Sweet.
// Copyright © 2010-2019 by Apple Inc.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

  //
  // Include necessary headers...
  //

#include "pappl-private.h"
#include <fnmatch.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <atomic>
#include <cassert>
#include <cmath>
#include <regex>
#include <stdexcept>
#include <limits>
#include <sane/sane.h>
#include <sane/saneopts.h>

//
// Forward Declarations
//

// Safe ctype macros
#define safe_isspace(c) isspace((unsigned char)c)
#define safe_isxdigit(c) isxdigit((unsigned char)c)
#define safe_iscntrl(c) iscntrl((unsigned char)c)
#define safe_isprint(c) isprint((unsigned char)c)
#define safe_toupper(c) toupper((unsigned char)c)
#define safe_tolower(c) tolower((unsigned char)c)

#define mem_len(v) (mem_len_bytes(v) / sizeof(*v))
#define mem_cap(v) (mem_cap_bytes(v) / sizeof(*v))

// Allocate `len' elements of type T
#define mem_new(T, len) ((T *)__mem_alloc(len, 0, sizeof(T), true))

  // Resize memory. The returned memory block has length of `len' and capacity at least of `len' + `extra'

#define mem_resize(p, len, extra) \
  ((__typeof__(p))__mem_resize(p, len, extra, sizeof(*p), true))

// Construct error from a string
static inline error
ERROR(const char *s)
{
  return (error)s;
}

// Format error string
error
eloop_eprintf(const char *fmt, ...)
{
  va_list ap;
  char buf[sizeof(eloop_estring)];

  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  strcpy(eloop_estring, buf);
  va_end(ap);

  return ERROR(eloop_estring);
}

// Create new string as a copy of existent string
static inline char *
str_dup(const char *s1)
{
  size_t len = strlen(s1);
  char *s = mem_resize((char *)NULL, len, 1);
  memcpy(s, s1, len + 1);
  return s;
}

// log_ctx represents logging context
typedef struct log_ctx log_ctx;

// Type http_client represents HTTP client instance
typedef struct http_client http_client;

// Type http_uri represents HTTP URI
typedef struct http_uri http_uri;

// HTTP data
typedef struct
{
  const char *content_type; /* Normalized: low-case with stripped directives */
  const void *bytes;        /* Data bytes */
  size_t size;              /* Data size */
} http_data;

// Free memory, previously obtained from mem_new()/mem_expand()
void
mem_free(void *p)
{
  if (p != NULL)
  {
    free(((mem_head *)p) - 1);
  }
}

// Reset array of SANE_Word
static inline void
sane_word_array_reset(SANE_Word **a)
{
  (*a)[0] = 0;
}

// Append word to array. Returns new array (old becomes invalid)
static inline SANE_Word *
sane_word_array_append(SANE_Word *a, SANE_Word w)
{
  size_t len = sane_word_array_len(a) + 1;
  a = mem_resize(a, len + 1, 0);
  a[0] = len;
  a[len] = w;
  return a;
}

// Get length of the SANE_Word array
static inline size_t
sane_word_array_len(const SANE_Word *a)
{
  return (size_t)a[0];
}

// Sort array of SANE_Word in increasing order
void
sane_word_array_sort(SANE_Word *a)
{
  SANE_Word len = a[0];

  if (len)
  {
    qsort(a + 1, len, sizeof(SANE_Word), sane_word_array_sort_cmp);
  }
}

typedef struct
{
  SANE_Word min;   /* minimum (element) value */
  SANE_Word max;   /* maximum (element) value */
  SANE_Word quant; /* quantization value (0 if none) */
} SANE_Range;

// ID_JUSTIFICATION represents hardware-defined ADF justification
typedef enum
{
  ID_JUSTIFICATION_UNKNOWN = -1,
  ID_JUSTIFICATION_LEFT,
  ID_JUSTIFICATION_CENTER,
  ID_JUSTIFICATION_RIGHT,
  ID_JUSTIFICATION_TOP,
  ID_JUSTIFICATION_BOTTOM,

  NUM_ID_JUSTIFICATION
} ID_JUSTIFICATION;

// ID_SOURCE represents scanning source
typedef enum
{
  ID_SOURCE_UNKNOWN = -1,
  ID_SOURCE_PLATEN,
  ID_SOURCE_ADF_SIMPLEX,
  ID_SOURCE_ADF_DUPLEX,

  NUM_ID_SOURCE
} ID_SOURCE;

// ID_COLORMODE represents color mode
typedef enum
{
  ID_COLORMODE_UNKNOWN = -1,
  ID_COLORMODE_COLOR,
  ID_COLORMODE_GRAYSCALE,
  ID_COLORMODE_BW1,

  NUM_ID_COLORMODE
} ID_COLORMODE;

// ID_FORMAT represents image format
typedef enum
{
  ID_FORMAT_UNKNOWN = -1,
  ID_FORMAT_JPEG,
  ID_FORMAT_TIFF,
  ID_FORMAT_PNG,
  ID_FORMAT_PDF,
  ID_FORMAT_BMP,

  NUM_ID_FORMAT
} ID_FORMAT;

// Resize the string
static inline char *
str_resize(char *s, size_t len)
{
  s = mem_resize(s, len, 1);
  s[len] = '\0';
  return s;
}

// Append memory to string: s1 += s2[:l2]
static inline char *
str_append_mem(char *s1, const char *s2, size_t l2)
{
  size_t l1 = str_len(s1);

  s1 = mem_resize(s1, l1 + l2, 1);
  memcpy(s1 + l1, s2, l2);
  s1[l1 + l2] = '\0';

  return s1;
}

// Append string to string: s1 += s2
static inline char *
str_append(char *s1, const char *s2)
{
  return str_append_mem(s1, s2, strlen(s2));
}

// Remove leading and trailing white space.
char *
str_trim(char *s)
{
  size_t len = strlen(s), skip;

  while (len > 0 && safe_isspace(s[len - 1]))
  {
    len--;
  }

  for (skip = 0; skip < len && safe_isspace(s[skip]); skip++)
  {
    ;
  }

  len -= skip;
  if (len != 0 && skip != 0)
  {
    memmove(s, s + skip, len);
  }
  s[len] = '\0';

  return s;
}

// Find max of two words
static inline SANE_Word
math_max(SANE_Word a, SANE_Word b)
{
  return a > b ? a : b;
}

// Find min of two words
static inline SANE_Word
math_min(SANE_Word a, SANE_Word b)
{
  return a < b ? a : b;
}

// Check two ranges for equivalency
static inline bool
math_range_eq(const SANE_Range *r1, const SANE_Range *r2)
{
  return r1->min == r2->min && r1->max == r2->max && r1->quant == r2->quant;
}

// Check two ranges for overlapping
static inline bool
math_range_ovp(const SANE_Range *r1, const SANE_Range *r2)
{
  return r1->max >= r2->min && r2->max >= r1->min;
}

// Choose nearest integer in range
SANE_Word
math_range_fit(const SANE_Range *r, SANE_Word i)
{
  if (i < r->min)
  {
    return r->min;
  }

  if (i > r->max)
  {
    return r->max;
  }

  if (r->quant == 0)
  {
    return i;
  }

  i -= r->min;
  i = ((i + r->quant / 2) / r->quant) * r->quant;
  i += r->min;

  return math_min(i, r->max);
}

// Convert pixels to millimeters, using given resolution
static inline SANE_Fixed
math_px2mm_res(SANE_Word px, SANE_Word res)
{
  return SANE_FIX((double)px * 25.4 / res);
}

// Merge two ranges, if possible
bool
math_range_merge(SANE_Range *out, const SANE_Range *r1, const SANE_Range *r2)
{
  /* Check for trivial cases */
  if (math_range_eq(r1, r2))
  {
    *out = *r1;
    return true;
  }

  if (!math_range_ovp(r1, r2))
  {
    return false;
  }

  /* Ranges have equal quantization? If yes, just adjust min and max */
  if (r1->quant == r2->quant)
  {
    out->min = math_max(r1->min, r2->min);
    out->max = math_min(r1->max, r2->max);
    out->quant = r1->quant;
    return true;
  }

  /* At least one of ranges don't have quantization? */
  if (!r1->quant || !r2->quant)
  {
    * /
        if (r1->quant == 0)
    {
      const SANE_Range *tmp = r1;
      r1 = r2;
      r2 = tmp;
    }

    /* And fit r2 within r1 */
    out->min = math_range_fit(r1, r2->min);
    out->max = math_range_fit(r1, r2->max);
    out->quant = r1->quant;
    return true;
  }

  /* Now the most difficult case */
  SANE_Word quant = math_lcm(r1->quant, r2->quant);
  SANE_Word min, max, bounds_min, bounds_max;

  bounds_min = math_max(r1->min, r2->min);
  bounds_max = math_min(r1->max, r2->max);

  for (min = math_min(r1->min, r2->min); min < bounds_min; min += quant)
    ;

  if (min > bounds_max)
  {
    return false;
  }

  for (max = min; max + quant <= bounds_max; max += quant)
    ;

  out->min = min;
  out->max = max;
  out->quant = quant;

  return true;
}

// Source Capabilities
typedef struct
{
  unsigned int flags;               /* Source flags */
  unsigned int colormodes;          /* Set of 1 << ID_COLORMODE */
  unsigned int formats;             /* Set of 1 << ID_FORMAT */
  SANE_Word min_wid_px, max_wid_px; /* Min/max width, in pixels */
  SANE_Word min_hei_px, max_hei_px; /* Min/max height, in pixels */
  SANE_Word *resolutions;           /* Discrete resolutions, in DPI */
  SANE_Range res_range;             /* Resolutions range, in DPI */
  SANE_Range win_x_range_mm;        /* Window x range, in mm */
  SANE_Range win_y_range_mm;        /* Window y range, in mm */
} devcaps_source;

// Device Capabilities

// Source flags
enum
{
  // Supported Intents
  DEVCAPS_SOURCE_INTENT_DOCUMENT = (1 << 3),
  DEVCAPS_SOURCE_INTENT_TXT_AND_GRAPH = (1 << 4),
  DEVCAPS_SOURCE_INTENT_PHOTO = (1 << 5),
  DEVCAPS_SOURCE_INTENT_PREVIEW = (1 << 6),

  DEVCAPS_SOURCE_INTENT_ALL =
      DEVCAPS_SOURCE_INTENT_DOCUMENT |
      DEVCAPS_SOURCE_INTENT_TXT_AND_GRAPH |
      DEVCAPS_SOURCE_INTENT_PHOTO |
      DEVCAPS_SOURCE_INTENT_PREVIEW,

  // How resolutions are defined
  DEVCAPS_SOURCE_RES_DISCRETE = (1 << 7), /* Discrete resolutions */
  DEVCAPS_SOURCE_RES_RANGE = (1 << 8),    /* Range of resolutions */

  DEVCAPS_SOURCE_RES_ALL =
      DEVCAPS_SOURCE_RES_DISCRETE |
      DEVCAPS_SOURCE_RES_RANGE,

  // Miscellaneous flags
  DEVCAPS_SOURCE_HAS_SIZE = (1 << 12), /* max_width, max_height and
                                          derivatives are valid */

  // Protocol dialects
  DEVCAPS_SOURCE_PWG_DOCFMT = (1 << 13),      /* pwg:DocumentFormat */
  DEVCAPS_SOURCE_SCAN_DOCFMT_EXT = (1 << 14), /* scan:DocumentFormatExt */
};

// Supported image formats
#define DEVCAPS_FORMATS_SUPPORTED \
((1 << ID_FORMAT_JPEG) |        \
  (1 << ID_FORMAT_PNG) |         \
  (1 << ID_FORMAT_BMP))

// Supported color modes
#define DEVCAPS_COLORMODES_SUPPORTED \
((1 << ID_COLORMODE_COLOR) |       \
  (1 << ID_COLORMODE_GRAYSCALE))

typedef struct
{
  // Fundamental values
  const char *protocol; /* Protocol name */
  SANE_Word units;      /* Size units, pixels per inch */

  // Image compression
  bool compression_ok;          /* Compression params are supported */
  SANE_Range compression_range; /* Compression range */
  SANE_Word compression_norm;   /* Normal compression */

  // Sources
  devcaps_source *src[NUM_ID_SOURCE]; /* Missed sources are NULL */

  // ADF Justification
  ID_JUSTIFICATION justification_x; /* Width justification*/
  ID_JUSTIFICATION justification_y; /* Height justification*/

} devcaps;

// Free devcaps_source
void
devcaps_source_free(devcaps_source *src)
{
  if (src != NULL)
  {
    sane_word_array_free(src->resolutions);
    mem_free(src);
  }
}

// Reset Device Capabilities into initial state
void
devcaps_reset(devcaps *caps)
{
  devcaps_cleanup(caps);
  memset(caps, 0, sizeof(*caps));
  devcaps_init(caps);
}

// PROTO_OP represents operation
typedef enum
{
  PROTO_OP_NONE,     /* No operation */
  PROTO_OP_PRECHECK, /* Pre-scan check */
  PROTO_OP_SCAN,     /* New scan */
  PROTO_OP_LOAD,     /* Load image */
  PROTO_OP_CHECK,    /* Check device status */
  PROTO_OP_CLEANUP,  /* Cleanup after scan */
  PROTO_OP_FINISH    /* Finish scanning */
} PROTO_OP;

// proto_scan_params represents scan parameters
typedef struct
{
  int x_off, y_off;       /* Scan area X/Y offset */
  int wid, hei;           /* Scan area width and height */
  int x_res, y_res;       /* X/Y resolution */
  ID_SOURCE src;          /* Desired source */
  ID_COLORMODE colormode; /* Desired color mode */
  ID_FORMAT format;       /* Image format */
} proto_scan_params;

// XML utilities

//
// xml_ns defines XML namespace.
//

typedef struct
{
  const char *prefix; /* Short prefix */
  const char *uri;    /* The namespace uri (glob pattern for reader) */
} xml_ns;

//
// ScanSettingXML - helper class that extracts values from the settings
//

typedef struct ScanSettingsXml
{
  ScanSettingsXml(const std::string &s)
      : xml(s)
  {
  }

  std::string getString(const std::string &name) const
  { // scan settings xml is simple enough to avoid using a parser
    std::regex r("<([a-zA-Z]+:" + name + ")>([^<]*)</\\1>");
    std::smatch m;
    if (std::regex_search(xml, m, r))
    {
      assert(m.size() == 3);
      return m[2].str();
    }
    return "";
  }

  double getNumber(const std::string &name) const
  {
    return sanecpp::strtod_c(getString(name));
  }
  std::string xml;
} ScanSettingsXml;

struct ScanJob::Private
{
  void init(const ScanSettingsXml &, bool autoselectFormat, const OptionsFile::Options &);
};

// Type error represents an error. Its value either NULL, which indicates "no error" condition, or some opaque non-null pointer, which can be converted to string
typedef struct error_s *error;

// Type http_query represents HTTP query (both request and response)
typedef struct http_query http_query;

//
// proto_ctx represents request context
//
typedef struct
{
  /* Common context */
  log_ctx *log;                 /* Logging context */
  struct proto_handler *proto;  /* Link to proto_handler */
  const devcaps *devcaps;       /* Device capabilities */
  PROTO_OP op;                  /* Current operation */
  http_client *http;            /* HTTP client for sending requests */
  http_uri *base_uri;           /* HTTP base URI for protocol */
  http_uri *base_uri_nozone;    /* base_uri without IPv6 zone */
  proto_scan_params params;     /* Scan parameters */
  const char *location;         /* Image location */
  unsigned int images_received; /* Total count of received images */

  /* Extra context for xxx_decode callbacks */
  const http_query *query; /* Passed to xxx_decode callbacks */

  /* Extra context for status_decode callback */
  PROTO_OP failed_op;     /* Failed operation */
  int failed_http_status; /* Its HTTP status */
  int failed_attempt;     /* Retry count, 0-based */
} proto_ctx;

//
// proto_result represents decoded query results
//
typedef struct
{
  PROTO_OP next;      /* Next operation */
  int delay;          /* In milliseconds */
  SANE_Status status; /* Job status */
  error err;          /* Error string, may be NULL */
  union
  {
    const char *location; /* Image location, protocol-specific */
    http_data *image;     /* Image buffer */
  } data;
} proto_result;

//
// proto_handler represents scan protocol implementation
//
typedef struct proto_handler proto_handler;

struct proto_handler
{
  const char *name; // Protocol name

  // Free protocol handler
  void (*free)(proto_handler *proto);

  // Query and decode device capabilities
  http_query *(*devcaps_query)(const proto_ctx *ctx);
  error (*devcaps_decode)(const proto_ctx *ctx, devcaps *caps);

  // Create pre-scan check query and decode result
  http_query *(*precheck_query)(const proto_ctx *ctx);
  proto_result (*precheck_decode)(const proto_ctx *ctx);

  // Initiate scanning and decode result.
  http_query *(*scan_query)(const proto_ctx *ctx);
  proto_result (*scan_decode)(const proto_ctx *ctx);

  // Initiate image downloading and decode result.
  http_query *(*load_query)(const proto_ctx *ctx);
  proto_result (*load_decode)(const proto_ctx *ctx);

  // Request device status and decode result
  http_query *(*status_query)(const proto_ctx *ctx);
  proto_result (*status_decode)(const proto_ctx *ctx);

  // Cleanup after scan
  http_query *(*cleanup_query)(const proto_ctx *ctx);

  // Cancel scan in progress
  http_query *(*cancel_query)(const proto_ctx *ctx);
};

//
// xml_rd - structure for reading value from XML
//

typedef struct xml_rd
{
  xmlDoc *doc;               // XML document
  xmlNode *node;             // Current node
  xmlNode *parent;           // Parent node
  const char *name;          // Name of current node
  char *path;                // Path to current node, /-separated
  size_t *pathlen;           // Stack of path lengths
  const xmlChar *text;       // Textual value of current node
  unsigned int depth;        // Depth of current node, 0 for root
  const xml_ns *subst_rules; // Substitution rules
  xml_ns *subst_cache;       // In the cache, glob-style patterns are replaced by exact-matching strings
} xml_rd;

static const char *
xml_rd_ns_subst_lookup(xml_rd *xml, const char *prefix, const char *href);

//
// xml_rd_skip_dummy - Skip dummy nodes. This is internal function, don't call directly
//

static void
xml_rd_skip_dummy(xml_rd *xml)
{
  xmlNode *node = xml->node;

  while (node != NULL && node->type != XML_ELEMENT_NODE)
  {
    node = node->next;
  }

  xml->node = node;
}

//
// xml_rd_node_invalidate - Invalidate cached value
//

static void
xml_rd_node_invalidate_value(xml_rd *xml)
{
  xmlFree((xmlChar *)xml->text);
  xml->text = NULL;
}

//
// xml_rd_node_switched - It invalidates cached value and updates node name
//

static void
xml_rd_node_switched(xml_rd *xml)
{
  size_t pathlen;

  /* Invalidate cached value */
  xml_rd_node_invalidate_value(xml);

  /* Update node name */
  pathlen = xml->depth ? xml->pathlen[xml->depth - 1] : 0;
  xml->path = str_resize(xml->path, pathlen);

  if (xml->node == NULL)
  {
    xml->name = NULL;
  }
  else
  {
    const char *prefix = NULL;

    if (xml->node->ns != NULL && xml->node->ns->prefix != NULL)
    {
      prefix = (const char *)xml->node->ns->prefix;
      prefix = xml_rd_ns_subst_lookup(xml, prefix,
                                      (const char *)xml->node->ns->href);
    }

    if (prefix != NULL)
    {
      xml->path = str_append(xml->path, prefix);
      xml->path = str_append_c(xml->path, ':');
    }

    xml->path = str_append(xml->path, (const char *)xml->node->name);

    xml->name = xml->path + pathlen;
  }
}

//
// xml_rd_error_callback - XML parser error callback
//

static void
xml_rd_error_callback(void *userdata, xmlErrorPtr error)
{
  (void)userdata;
  (void)error;
}

//
// xml_rd_parse - Parse XML document
//

static error
xml_rd_parse(xmlDoc **doc, const char *xml_text, size_t xml_len)
{
  xmlParserCtxtPtr ctxt;
  error err = NULL;

  /* Setup XML parser */
  ctxt = xmlNewParserCtxt();
  if (ctxt == NULL)
  {
    err = ERROR("not enough memory");
    goto DONE;
  }

  ctxt->sax->serror = xml_rd_error_callback;

  /* Parse the document */
  if (xmlCtxtResetPush(ctxt, xml_text, xml_len, NULL, NULL))
  {
    err = ERROR("not enough memory");
    goto DONE;
  }

  xmlParseDocument(ctxt);

  if (ctxt->wellFormed)
  {
    *doc = ctxt->myDoc;
  }
  else
  {
    if (ctxt->lastError.message != NULL)
    {
      err = eloop_eprintf("XML: %s", ctxt->lastError.message);
    }
    else
    {
      err = ERROR("XML: parse error");
    }

    *doc = NULL;
  }

  /* Cleanup and exit */
DONE:
  if (err != NULL && ctxt != NULL && ctxt->myDoc != NULL)
  {
    xmlFreeDoc(ctxt->myDoc);
  }

  if (ctxt != NULL)
  {
    xmlFreeParserCtxt(ctxt);
  }

  return err;
}

//
// xml_rd_begin - Parse XML text and initialize reader to iterate starting from the root node
//

error
xml_rd_begin(xml_rd **xml, const char *xml_text, size_t xml_len,
              const xml_ns *ns)
{
  xmlDoc *doc;
  error err = xml_rd_parse(&doc, xml_text, xml_len);

  *xml = NULL;
  if (err != NULL)
  {
    return err;
  }

  *xml = mem_new(xml_rd, 1);
  (*xml)->doc = doc;
  (*xml)->node = xmlDocGetRootElement((*xml)->doc);
  (*xml)->path = str_new();
  (*xml)->pathlen = mem_new(size_t, 0);
  (*xml)->subst_rules = ns;

  xml_rd_skip_dummy(*xml);
  xml_rd_node_switched(*xml);

  return NULL;
}

//
// xml_rd_finish - Finish reading, free allocated resources
//

void
xml_rd_finish(xml_rd **xml)
{
  if (*xml)
  {
    if ((*xml)->doc)
    {
      xmlFreeDoc((*xml)->doc);
    }
    xml_rd_node_invalidate_value(*xml);

    if ((*xml)->subst_cache != NULL)
    {
      size_t i, len = mem_len((*xml)->subst_cache);
      for (i = 0; i < len; i++)
      {
        mem_free((char *)(*xml)->subst_cache[i].uri);
      }
      mem_free((*xml)->subst_cache);
    }

    mem_free((*xml)->pathlen);
    mem_free((*xml)->path);
    mem_free(*xml);
    *xml = NULL;
  }
}

//
// xml_rd_ns_subst_lookup - Perform namespace prefix substitution. Is substitution
//

static const char *
xml_rd_ns_subst_lookup(xml_rd *xml, const char *prefix, const char *href)
{
  size_t i, len = mem_len(xml->subst_cache);

  /* Substitution enabled? */
  if (xml->subst_rules == NULL)
  {
    return prefix;
  }

  /* Lookup cache first */
  for (i = 0; i < len; i++)
  {
    if (!strcmp(href, xml->subst_cache[i].uri))
    {
      return xml->subst_cache[i].prefix;
    }
  }

  /* Now try glob-style rules */
  for (i = 0; xml->subst_rules[i].prefix != NULL; i++)
  {
    if (!fnmatch(xml->subst_rules[i].uri, href, 0))
    {
      prefix = xml->subst_rules[i].prefix;

      /* Update cache. Grow it if required */
      xml->subst_cache = mem_resize(xml->subst_cache, len + 1, 0);
      xml->subst_cache[len].prefix = prefix;
      xml->subst_cache[len].uri = str_dup(href);

      /* Break out of loop */
      break;
    }
  }

  return prefix;
}

//
// xml_rd_depth - Get current node depth in the tree. Root depth is 0
//

unsigned int
xml_rd_depth(xml_rd *xml)
{
  return xml->depth;
}

//
// xml_rd_end - Check for end-of-document condition
//

bool
xml_rd_end(xml_rd *xml)
{
  return xml->node == NULL;
}

//
// xml_rd_next - Shift to the next node
//

void
xml_rd_next(xml_rd *xml)
{
  if (xml->node)
  {
    xml->node = xml->node->next;
    xml_rd_skip_dummy(xml);
    xml_rd_node_switched(xml);
  }
}

//
// xml_rd_deep_next - Shift to the next node, visiting the nested nodes on the way
//

void
xml_rd_deep_next(xml_rd *xml, unsigned int depth)
{
  xml_rd_enter(xml);

  while (xml_rd_end(xml) && xml_rd_depth(xml) > depth + 1)
  {
    xml_rd_leave(xml);
    xml_rd_next(xml);
  }
}

//
// xml_rd_enter - Enter the current node - iterate its children
//

void
xml_rd_enter(xml_rd *xml)
{
  if (xml->node)
  {
    /* Save current path length into pathlen stack */
    xml->path = str_append_c(xml->path, '/');

    xml->pathlen = mem_resize(xml->pathlen, xml->depth + 1, 0);
    xml->pathlen[xml->depth] = mem_len(xml->path);

    /* Enter the node */
    xml->parent = xml->node;
    xml->node = xml->node->children;
    xml_rd_skip_dummy(xml);

    /* Increment depth and recompute node name */
    xml->depth++;
    xml_rd_skip_dummy(xml);
    xml_rd_node_switched(xml);
  }
}

//
// xml_rd_leave - Leave the current node - return to its parent
//

void
xml_rd_leave(xml_rd *xml)
{
  if (xml->depth > 0)
  {
    xml->depth--;
    xml->node = xml->parent;
    if (xml->node)
    {
      xml->parent = xml->node->parent;
    }

    xml_rd_node_switched(xml);
  }
}

//
// xml_rd_node_name - Get name of the current node.
//

const char *
xml_rd_node_name(xml_rd *xml)
{
  return xml->name;
}

//
// xml_rd_node_path - Get full path to the current node, '/'-separated
//

const char *
xml_rd_node_path(xml_rd *xml)
{
  return xml->node ? xml->path : NULL;
}

//
// xml_rd_node_name_match - Match name of the current node against the pattern
//

bool
xml_rd_node_name_match(xml_rd *xml, const char *pattern)
{
  return xml->name != NULL && !strcmp(xml->name, pattern);
}

//
// xml_rd_node_value - Get value of the current node as text
//

const char *
xml_rd_node_value(xml_rd *xml)
{
  if (xml->text == NULL && xml->node != NULL)
  {
    xml->text = xmlNodeGetContent(xml->node);
    str_trim((char *)xml->text);
  }

  return (const char *)xml->text;
}

//
// xml_rd_node_value_unit - Get value of the current node as unsigned integer
//

error
xml_rd_node_value_uint(xml_rd *xml, SANE_Word *val)
{
  const char *s = xml_rd_node_value(xml);
  char *end;
  unsigned long v;

  log_assert(NULL, s != NULL);

  v = strtoul(s, &end, 10);
  if (end == s || *end || v != (unsigned long)(SANE_Word)v)
  {
    return eloop_eprintf("%s: invalid numerical value",
                          xml_rd_node_name(xml));
  }

  *val = (SANE_Word)v;
  return NULL;
}

//
// devcaps_source - Source Capabilities (each device may contain multiple sources)
//

typedef struct
{
  unsigned int flags;               /* Source flags */
  unsigned int colormodes;          /* Set of 1 << ID_COLORMODE */
  unsigned int formats;             /* Set of 1 << ID_FORMAT */
  SANE_Word min_wid_px, max_wid_px; /* Min/max width, in pixels */
  SANE_Word min_hei_px, max_hei_px; /* Min/max height, in pixels */
  SANE_Word *resolutions;           /* Discrete resolutions, in DPI */
  SANE_Range res_range;             /* Resolutions range, in DPI */
  SANE_Range win_x_range_mm;        /* Window x range, in mm */
  SANE_Range win_y_range_mm;        /* Window y range, in mm */
} devcaps_source;

#ifdef __cplusplus
}
#endif // __cplusplus
