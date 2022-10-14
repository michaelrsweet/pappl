---
title: PAPPL IPP Extensions
author: Michael R Sweet
copyright: Copyright © 2022 by Michael R Sweet
---


# 1. Introduction

This document describes the IPP extensions unique to the [Printer Application Framework (PAPPL)][PAPPL].  Conformance words including MAY, MUST, MUST NOT, RECOMMENDED, REQUIRED, SHOULD, SHOULD NOT


# 2. Terminology

The key words "MUST", "MUST NOT", "REQUIRED", "SHALL", "SHALL NOT", "SHOULD", "SHOULD NOT", "RECOMMENDED", "MAY", and "OPTIONAL" in this document are to be interpreted as described in [RFC2119][RFC2119].

Other printing terminology is imported from [RFC 8011: Internet Printing Protocol/1.1: Model and Semantics][RFC8011] and [PWG 5100.22-2019: IPP System Service v1.0 (SYSTEM)][PWG5100.22].


# 3. Model

The [Printer Application Framework (PAPPL)][PAPPL] provides a generic framework for integrating legacy printer drivers with a common [IPP Everywhere][IPP-EVERYWHERE] front-end. The framework implements a subset of the [IPP System Service][PWG5100.22] to manage multiple IPP Printers.  Each Printer has an associated Output Device whose URI ("smi55357-device-uri") and driver ("smi55357-driver-name") specify where and how to communicate with the Output Device.

In order to support remote management of Printers, two new operations are required to list the available Output Devices and drivers.


# 4. IPP Operations

## 4.1 PAPPL-Find-Devices

The REQUIRED PAPPL-Find-Devices System operation queries the System for a list of known or discovered Output Devices that can be used with the Create-Printer operation. The System MAY cache the list of Output Devices but the list MUST have been updated or confirmed within the 300 seconds prior to receiving a Client request.

Access Rights: The authenticated user (see [Section 9.3 of RFC 8011][RFC8011]) performing this operation MUST be an Operator or Administrator of the System (see [Sections 1 and 9.5][RFC8011] of RFC 8011).  Otherwise, the IPP System MUST reject the operation and return 'client-error-forbidden', 'client-error-not-authenticated', or 'client-error-not-authorized' as appropriate.

### 4.1.1 PAPPL-Find-Devices Request

Group 1: Operation Attributes

- Natural Language and Character Set: The "attributes-charset" and "attributes-natural-language" attributes as described in [Section 4.1.4.1 of RFC 8011][RFC8011].

- Target: The "system-uri" (uri) operation attribute, which is the target for this operation.

- Requesting User Name: The "requesting-user-name" (name(MAX)) attribute SHOULD be supplied by the Client as described in [Section 9.3 of RFC 8011][RFC8011].

- "smi55357-device-type" (1setOf type2 keyword): The Client OPTIONALLY supplies and a System MUST support this attribute which lists one or more device types to query; the default is 'all'.

### 4.1.2 PAPPL-Find-Devices Response

Group 1: Operation Attributes

- Natural Language and Character Set: The "attributes-charset" and "attributes-natural-language" attributes as described in [Section 4.1.4.1 of RFC 8011][RFC8011].

Group 2: System Attributes

- "smi55357-device-col" (1setOf collection): A list of Output Devices supported by the System.


## 4.2 PAPPL-Find-Drivers

The REQUIRED PAPPL-Find-Drivers System operation queries the System for a list of known or matching Output Device drivers that can be used with the Create-Printer operation.

Access Rights: The authenticated user (see [Section 9.3 of RFC 8011][RFC8011]) performing this operation MUST be an Operator or Administrator of the System (see [Sections 1 and 9.5][RFC8011] of RFC 8011).  Otherwise, the IPP System MUST reject the operation and return 'client-error-forbidden', 'client-error-not-authenticated', or 'client-error-not-authorized' as appropriate.

### 4.2.1 PAPPL-Find-Drivers Request

Group 1: Operation Attributes

- Natural Language and Character Set: The "attributes-charset" and "attributes-natural-language" attributes as described in [Section 4.1.4.1 of RFC 8011][RFC8011].

- Target: The "system-uri" (uri) operation attribute, which is the target for this operation.

- Requesting User Name: The "requesting-user-name" (name(MAX)) attribute SHOULD be supplied by the Client as described in [Section 9.3 of RFC 8011][RFC8011].

- "smi55357-device-id" (text(MAX)): The Client OPTIONALLY supplies and a System MUST support this attribute which specifies an IEEE-1284 device ID for filtering the list of drivers.

### 4.2.2 PAPPL-Find-Drivers Response

Group 1: Operation Attributes

- Natural Language and Character Set: The "attributes-charset" and "attributes-natural-language" attributes as described in [Section 4.1.4.1 of RFC 8011][RFC8011].

Group 2: System Attributes

- "smi55357-driver-col" (1setOf collection): A list of drivers supported by the System.


# 5. IPP Attributes

## 5.1 Operation Attributes

### 5.1.1 smi55357-device-id (text(MAX))

This operation attribute specifies the IEEE-1284 device ID string for an Output Device.


### 5.1.2 smi55357-device-type (1setOf type2 keyword)

This operation attribute lists the types of Output Devices the Client is interested in. The following keyword values are defined:

- 'all': All devices
- 'dns-sd': Devices discovered via DNS-SD and mDNS
- 'local': Local (directly connected) devices
- 'network': Network devices
- 'other-local': Devices connected via another local interface
- 'other-network': Devices discovered via another network protocol
- 'snmp': Devices discovered via SNMP
- 'usb': Devices connected via USB


## 5.2 Printer Status Attributes

### 5.2.1 smi55357-device-uri (uri)

This REQUIRED attribute specifies the URI for the Output Device.


### 5.2.2 smi55357-driver-name (name(MAX))

This REQUIRED attribute specifies the driver for the Output Device.


## 5.3 System Status Attributes

### 5.3.1 smi55357-device-col (1setOf collection)

This REQUIRED attribute lists available Output Devices. The REQUIRED "smi55357-device-id", "smi55357-device-name", and "smi55357-device-uri" member attributes provide information about an individual Output Device.

#### 5.3.1.1 smi55357-device-id (text(MAX))

This member attribute provides the IEEE-1284 Device ID for the Output Device.

#### 5.3.1.2 smi55357-device-name (name(MAX))

This member attribute provides descriptive information about the Output Device.

#### 5.3.1.3 smi55357-device-uri (uri)

This member attribute provides the URI value to use with the Create-Printer operation.


### 5.3.2 smi55357-driver-col (1setOf collection)

This REQUIRED attribute lists available or matching drivers for Output Devices. The REQUIRED "smi55357-device-id", "smi55357-driver-info", and "smi55357-driver-name" member attributes provide information about a driver.

#### 5.3.2.1 smi55357-device-id (text(MAX))

This member attribute provides the IEEE-1284 Device ID for Output Devices supported by the driver.

#### 5.3.2.2 smi55357-driver-info (text(MAX))

This member attribute provides descriptive information about the driver.

#### 5.3.2.3 smi55357-driver-name (name(MAX))

This member attribute provides the unique driver name.


# 6. References

[IPP-EVERYWHERE: IPP Everywhere™ technology landing page][IPP-EVERYWHERE]

[IPP-EVERYWHERE]: https://www.pwg.org/ipp/everywhere.html


[PAPPL: Printer Application Framework (PAPPL)][PAPPL]

[PAPPL]: https://www.msweet.org/pappl


[PWG5100.22: IPP System Service v1.0 (SYSTEM)][PWG5100.22], November 2019

[PWG5100.22]: https://ftp.pwg.org/pub/pwg/candidates/cs-ippsystem10-20191122-5100.22.pdf


[RFC2199: Key words for use in RFCs to Indicate Requirement Levels][RFC2119], BCP14, March 1997

[RFC2119]: https://www.rfc-editor.org/rfc/rfc2119


[RFC8011: Internet Printing Protocol/1.1: Model and Semantics][RFC8011], STD92, January 2017

[RFC8011]: https://www.rfc-editor.org/rfc/rfc8011
