Security Policy
===============

This file describes how security issues are reported and handled, and what the
expectations are for security issues reported to this project.


Supported Versions
------------------

All production releases of this software are subject to this security policy.
A production release is tagged and given a semantic version number of the
form:

    MAJOR.MINOR.PATCH

where "MAJOR" is an integer starting at 1 and "MINOR" and "PATCH" are integers
starting at 0.  A feature release has a "PATCH" value of 0, for example:

    1.0.0
    1.1.0
    2.0.0

Beta releases and release candidates are *not* production releases and use
semantic version numbers of the form:

    MAJOR.MINORbNUMBER
    MAJOR.MINORrcNUMBER

where "MAJOR" and "MINOR" identify the new feature release version number and
"NUMBER" identifies a beta or release candidate number starting at 1, for
example:

    1.0b1
    1.0b2
    1.0rc1


What is a Security Bug?
-----------------------

The following kinds of issues are generally treated as security
vulnerabilities:

- Daemon/service crashes/hangs caused by a network request,
- Remote code execution through an API or other interface used by PAPPL,
- Privilege escalation that allows unauthorized actions or information
  disclosure, and
- Common weaknesses (buffer overflow, divide-by-zero, input validation,
  use-after-free, etc.) that lead to a demonstrated (not theoretical) exploit.

The following kinds of issues are generally treated as regular bugs:

- Vulnerabilities caused by mis-configuration,
- Issues caused by incorrect API usage, and
- Issues that only exist in non-production software.

Regular bugs should be reported to the project using the GitHub (public) issue
tracker page at <https://github.com/michaelrsweet/pappl/issues>.


Reporting a Security Bug
------------------------

Vulnerabilities should be reported to the project security advisory page at
<https://github.com/michaelrsweet/pappl/security/advisories>.

Provide details, impact, reproducer, affected versions, workarounds, and a
patch for the vulnerability, if applicable.

You can expect a response within 5 business days.


Responsible Disclosure
----------------------

With *responsible disclosure*, a security issue (and its fix) is disclosed
only after a mutually-agreed period of time (the "embargo date").  The issue
and fix are shared amongst and reviewed by the key stakeholders and the
CERT/CC.  Fixes are released to the public on the agreed-upon date.

Responsible disclosure is used for security issues in production releases
whose CVSS score is 7 or more.  Fixes for lesser security issues are typically
pushed to the public GitHub repository after review and included in the next
scheduled production release.
