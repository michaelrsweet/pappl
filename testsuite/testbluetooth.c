//
// Bluetooth unit tests for the Printer Application Framework
//
// Copyright (c) 2026 John Beard
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include <config.h>
#include <pappl/bt-private.h>
#include "test.h"

#if defined(HAVE_LIBBLUETOOTH) && defined(HAVE_DBUS)


//
// _papplBluetoothNormalizeAddress test cases...
//

struct bt_norm_address_test_case_s
{
  const char *input;
  const char *expected_output;
};


static const struct bt_norm_address_test_case_s bt_norm_address_test_cases[] =
{
  {NULL,                      NULL},
  {"",                        NULL},
  {"00:11:22:aa:bb:cc",       "00:11:22:AA:BB:CC"},
  {"00.11.22.aa.bb.cc",       "00:11:22:AA:BB:CC"},
  {"001122AABBCC",            "00:11:22:AA:BB:CC"},
  {"00-11-22-AA-bb-cc",       "00:11:22:AA:BB:CC"},
  {"00:11:22:AA:BB:CC",       "00:11:22:AA:BB:CC"},
  {"00:11:22:AA:BB",          NULL},
  {"00:11:22:AA:BB:CC:DD",    NULL},
  {"not-an-address",          NULL},
};


static void test_papplBluetoothNormalizeAddress(void)
{
  size_t i;

  for (i = 0; i < sizeof(bt_norm_address_test_cases) / sizeof(bt_norm_address_test_cases[0]); i++)
  {
    const struct bt_norm_address_test_case_s *test = &bt_norm_address_test_cases[i];
    char buf[19];

    testBegin("_papplBluetoothNormalizeAddress(\"%s\")", test->input ? test->input : "NULL");

    bool result = _papplBluetoothNormalizeAddress(test->input, buf, sizeof(buf));

    if (test->expected_output)
      testEnd(result && !strcmp(buf, test->expected_output));
    else
      testEnd(!result);
  }
}


//
// _papplBluetoothAddrToBare test cases...
//

struct bt_addr_to_bare_test_case_s
{
  const char *input;
  const char *expected_output;
  size_t      bufsize;        // 0 = use default (13)
};


static const struct bt_addr_to_bare_test_case_s bt_addr_to_bare_test_cases[] =
{
  {"00:11:22:AA:BB:CC",       "001122AABBCC", 0},
  {"FF:FF:FF:FF:FF:FF",       "FFFFFFFFFFFF", 0},
  {"00:00:00:00:00:00",       "000000000000", 0},
  {NULL,                      NULL,           0},
  {"00:11:22:AA:BB:CC",       NULL,          12},
};


static void test_papplBluetoothAddrToBare(void)
{
  size_t i;

  for (i = 0; i < sizeof(bt_addr_to_bare_test_cases) / sizeof(bt_addr_to_bare_test_cases[0]); i++)
  {
    const struct bt_addr_to_bare_test_case_s *test = &bt_addr_to_bare_test_cases[i];
    char buf[_PAPPL_BT_ADDR_BARE_SIZE];
    bool result;

    size_t bufsize = test->bufsize;
    if (bufsize == 0)
      bufsize = sizeof(buf);

    testBegin("_papplBluetoothAddrToBare(\"%s\", %u)",
              test->input ? test->input : "NULL", (unsigned)bufsize);

    result = _papplBluetoothAddrToBare(test->input, buf, bufsize);

    if (test->expected_output)
      testEnd(result && !strcmp(buf, test->expected_output));
    else
      testEnd(!result);
  }
}


//
// _papplBluetoothSppParseURI tests...
//

struct bt_parse_uri_test_case_s
{
  const char *uri;
  const char *expect_addr;
  int expect_channel;
};


static const struct bt_parse_uri_test_case_s bt_parse_uri_test_cases[] =
{
  {"bt-spp://00:11:22:AA:BB:CC",          "00:11:22:AA:BB:CC", 1},
  {"bt-spp://00:11:22:AA:BB:CC/5",        "00:11:22:AA:BB:CC", 5},
  {"bt-spp://001122aabbcc/3",             "00:11:22:AA:BB:CC", 3},
  {"http://00:11:22:AA:BB:CC",            NULL, 0},
  {"bt-spp://",                           NULL, 0},
  {"bt-spp://not-an-address",             NULL, 0},
  {"bt-spp://00:11:22:AA:BB:CC/31",       NULL, 0},
  {"bt-spp://00:11:22:AA:BB:CC/0",        NULL, 0},
  {"bt-spp://00:11:22:AA:BB:CC/abc",      NULL, 0},
  {"bt-spp://00:11:22:AA:BB:CC/1/extra",  NULL, 0},
};


static void test_papplBluetoothSppParseURI(void)
{
  size_t i;

  for (i = 0; i < sizeof(bt_parse_uri_test_cases) / sizeof(bt_parse_uri_test_cases[0]); i++)
  {
    const struct bt_parse_uri_test_case_s *test = &bt_parse_uri_test_cases[i];
    char buf[19];
    int channel;

    testBegin("_papplBluetoothSppParseURI(\"%s\")", test->uri);

    bool result = _papplBluetoothSppParseURI(test->uri, buf, sizeof(buf), &channel);

    if (test->expect_addr)
      testEnd(result && !strcmp(buf, test->expect_addr) && channel == test->expect_channel);
    else
      testEnd(!result);
  }
}


//
// _papplBluetoothMakeDeviceId tests...
//

struct bt_make_device_id_test_case_s
{
    const char *name;
    const char *addr;
    const char *expect;
};


static const struct bt_make_device_id_test_case_s bt_make_device_id_test_cases[] =
{
    {"Printer1", "00:11:22:AA:BB:CC",   "NAME:Printer1;BTADDR:00:11:22:AA:BB:CC;"},
    {"",         "00:11:22:AA:BB:CC",   "NAME:00:11:22:AA:BB:CC;BTADDR:00:11:22:AA:BB:CC;"},
    {NULL,       "00:11:22:AA:BB:CC",   "NAME:00:11:22:AA:BB:CC;BTADDR:00:11:22:AA:BB:CC;"},
    {"Some Dev", "00:11:22:AA:BB:CC",   "NAME:Some Dev;BTADDR:00:11:22:AA:BB:CC;"},
    {"dev",      NULL,                  NULL},
    {"dev",      "",                    NULL},
};


static void test_papplBluetoothMakeDeviceId(void)
{
  size_t i;

  for (i = 0; i < sizeof(bt_make_device_id_test_cases) / sizeof(bt_make_device_id_test_cases[0]); i++)
  {
    const struct bt_make_device_id_test_case_s *test = &bt_make_device_id_test_cases[i];
    char buf[256];

    testBegin("_papplBluetoothMakeDeviceId(\"%s\", \"%s\")",
              test->name ? test->name : "NULL",
              test->addr ? test->addr : "NULL");

    bool result = _papplBluetoothMakeDeviceId(test->name, test->addr,
                                              buf, sizeof(buf));

    if (test->expect)
      testEnd(result && !strcmp(buf, test->expect));
    else
      testEnd(!result);
  }
}


//
// 'main()' - Test the Bluetooth address/URI functions.
//

int					// O - Exit status
main(void)
{
  test_papplBluetoothMakeDeviceId();
  test_papplBluetoothNormalizeAddress();
  test_papplBluetoothAddrToBare();
  test_papplBluetoothSppParseURI();

  return (testsPassed ? 0 : 1);
}


#else // !(HAVE_LIBBLUETOOTH && HAVE_DBUS)


int					// O - Exit status
main(void)
{
  // Bluetooth support not compiled in: nothing to test.
  return (0);
}


#endif // HAVE_LIBBLUETOOTH && HAVE_DBUS
