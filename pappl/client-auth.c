//
// Authentication support for the Printer Application Framework
//
// Copyright © 2017-2024 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "client-private.h"
#include "system-private.h"
#if !_WIN32
#  include <pwd.h>
#endif // !_WIN32
#ifdef HAVE_LIBPAM
#  ifdef HAVE_PAM_PAM_APPL_H
#    include <pam/pam_appl.h>
#  else
#    include <security/pam_appl.h>
#  endif // HAVE_PAM_PAM_APPL_H
#endif // HAVE_LIBPAM


//
// Types...
//

typedef struct _pappl_authdata_s	// PAM authentication data
{
  const char	*username,			// Username string
		*password;			// Password string
} _pappl_authdata_t;


//
// Local functions...
//

static int	pappl_authenticate_user(pappl_client_t *client, const char *username, const char *password);
#ifdef HAVE_LIBPAM
static int	pappl_pam_func(int num_msg, const struct pam_message **msg, struct pam_response **resp, _pappl_authdata_t *data);
#endif // HAVE_LIBPAM


//
// 'papplClientIsAuthorized()' - Determine whether a client is authorized for
//                               administrative requests.
//
// This function determines whether a client is authorized to submit an
// administrative request.
//
// The return value is `HTTP_STATUS_CONTINUE` if access is authorized,
// `HTTP_STATUS_FORBIDDEN` if access is not allowed, `HTTP_STATUS_UNAUTHORIZED`
// if authorization is required, or `HTTP_STATUS_UPGRADE_REQUIRED` if the
// connection needs to be encrypted.  All of these values can be passed to the
// @link papplClientRespond@ function.
//

http_status_t				// O - HTTP status
papplClientIsAuthorized(
    pappl_client_t *client)		// I - Client
{
  // Range check input...
  if (!client)
    return (HTTP_STATUS_BAD_REQUEST);

  // Authorize for admin access...
  return (_papplClientIsAuthorizedForGroup(client, false, client->system->admin_group, client->system->admin_gid));
}


//
// '_papplClientIsAuthorizedForGroup()' - Determine whether a client is authorized for the named group.
//

http_status_t				// O - HTTP status
_papplClientIsAuthorizedForGroup(
    pappl_client_t *client,		// I - Client
    bool           allow_remote,	// I - Allow remote access?
    const char     *group,		// I - Group name, if any
    gid_t          groupid)		// I - Group ID, if any
{
  const char	*authorization;		// Authorization: header value


  // Local access is always allowed...
#if !_WIN32
  if (httpAddrGetFamily(httpGetAddress(client->http)) == AF_LOCAL)
    return (HTTP_STATUS_CONTINUE);
#endif // !_WIN32

  if ((allow_remote || httpAddrIsLocalhost(httpGetAddress(client->http))) && !client->system->auth_service && !client->system->auth_cb)
    return (HTTP_STATUS_CONTINUE);

  // Remote access is only allowed if an authentication service is configured...
  if (!client->system->auth_service && !client->system->auth_cb)
    return (HTTP_STATUS_FORBIDDEN);

  // Remote admin access requires encryption...
  if (!httpIsEncrypted(client->http) && !httpAddrIsLocalhost(httpGetAddress(client->http)))
    return (HTTP_STATUS_UPGRADE_REQUIRED);

  if (client->system->auth_cb)
  {
    // Use the authentication callback...
    return ((client->system->auth_cb)(client, group, groupid, client->system->auth_cbdata));
  }

  // Get the authorization header...
  if ((authorization = httpGetField(client->http, HTTP_FIELD_AUTHORIZATION)) != NULL && *authorization)
  {
    if (!strncmp(authorization, "Basic ", 6))
    {
      // Basic authentication...
      char	username[512],		// Username value
		*password;		// Password value
      cups_len_t userlen = sizeof(username);
					// Length of username:password
#if !_WIN32
      struct passwd *user,		// User information
		udata;			// User data
      char	ubuffer[16384];		// User strings
      int	num_groups;		// Number of authenticated groups, if any
#  ifdef __APPLE__
      int	groups[32];		// Authenticated groups, if any
#  else
      gid_t	groups[32];		// Authenticated groups, if any
#  endif // __APPLE__
#endif // !_WIN32

      for (authorization += 6; *authorization && isspace(*authorization & 255); authorization ++)
        ;				// Skip whitespace

      httpDecode64(username, &userlen, authorization, NULL);
      if ((password = strchr(username, ':')) != NULL)
      {
	*password++ = '\0';

        // Authenticate the username and password...
	if (pappl_authenticate_user(client, username, password))
	{
	  // Return now if there is no group for authorization...
	  if (!group)
	    return (HTTP_STATUS_CONTINUE);

#if _WIN32
	  // No groups in stock Windows support...
          return (HTTP_STATUS_CONTINUE);

#else
	  // Get the user information (groups, etc.)
	  if (!getpwnam_r(username, &udata, ubuffer, sizeof(ubuffer), &user) && user)
	  {
	    papplLogClient(client, PAPPL_LOGLEVEL_INFO, "Authenticated as \"%s\" using Basic.", username);
	    papplCopyString(client->username, username, sizeof(client->username));

	    num_groups = (int)(sizeof(groups) / sizeof(groups[0]));

#  ifdef __APPLE__
	    if (getgrouplist(username, (int)user->pw_gid, groups, &num_groups))
#  else
	    if (getgrouplist(username, user->pw_gid, groups, &num_groups))
#  endif // __APPLE__
	    {
	      papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Unable to lookup groups for user '%s': %s", username, strerror(errno));
	      num_groups = 0;
	    }

            // Check group membership...
            if (groupid != (gid_t)-1)
            {
              if (user->pw_gid != groupid)
              {
                int i;			// Looping var

                for (i = 0; i < num_groups; i ++)
		{
		  if ((gid_t)groups[i] == groupid)
		    break;
		}

                if (i >= num_groups)
                {
                  // Not in the admin group, access is forbidden...
                  return (HTTP_STATUS_FORBIDDEN);
		}
              }
            }

            // If we get this far, authentication and authorization are good...
            return (HTTP_STATUS_CONTINUE);
	  }
	  else
	  {
	    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Unable to lookup user '%s'.", username);
	    return (HTTP_STATUS_SERVER_ERROR);
	  }
#endif // _WIN32
	}
	else
	{
	  papplLogClient(client, PAPPL_LOGLEVEL_INFO, "Basic authentication of '%s' failed.", username);
	}
      }
      else
      {
	papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Bad Basic Authorization header value seen.");
	return (HTTP_STATUS_BAD_REQUEST);
      }
    }
    else
    {
      papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Unsupported Authorization header value seen.");
      return (HTTP_STATUS_BAD_REQUEST);
    }
  }

  // If we get there then we don't have any authorization value we can use...
  return (HTTP_STATUS_UNAUTHORIZED);
}


//
// 'pappl_authenticate_user()' - Validate a username + password combination.
//

static int				// O - 1 if correct, 0 otherwise
pappl_authenticate_user(
    pappl_client_t *client,		// I - Client
    const char     *username,		// I - Username string
    const char     *password)		// I - Password string
{
  int			status = 0;	// Return status
#ifdef HAVE_LIBPAM
  _pappl_authdata_t	data;		// Authorization data
  pam_handle_t		*pamh;		// PAM authentication handle
  int			pamerr;		// PAM error code
  struct pam_conv	pamdata;	// PAM conversation data


  data.username = username;
  data.password = password;

  pamdata.conv        = (int (*)(int, const struct pam_message **, struct pam_response **, void *))pappl_pam_func;
  pamdata.appdata_ptr = &data;
  pamh                = NULL;

  if ((pamerr = pam_start(client->system->auth_service, data.username, &pamdata, &pamh)) != PAM_SUCCESS)
  {
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "pam_start() returned %d (%s)", pamerr, pam_strerror(pamh, pamerr));
  }
#  ifdef PAM_RHOST
  else if ((pamerr = pam_set_item(pamh, PAM_RHOST, client->hostname)) != PAM_SUCCESS)
  {
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "pam_set_item(PAM_RHOST) returned %d (%s)", pamerr, pam_strerror(pamh, pamerr));
  }
#  endif // PAM_RHOST
#  ifdef PAM_TTY
  else if ((pamerr = pam_set_item(pamh, PAM_TTY, "lprint")) != PAM_SUCCESS)
  {
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "pam_set_item(PAM_TTY) returned %d (%s)", pamerr, pam_strerror(pamh, pamerr));
  }
#  endif // PAM_TTY
  else if ((pamerr = pam_authenticate(pamh, PAM_SILENT)) != PAM_SUCCESS)
  {
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "pam_authenticate() returned %d (%s)", pamerr, pam_strerror(pamh, pamerr));
  }
  else if ((pamerr = pam_setcred(pamh, PAM_ESTABLISH_CRED | PAM_SILENT)) != PAM_SUCCESS)
  {
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "pam_setcred() returned %d (%s)", pamerr, pam_strerror(pamh, pamerr));
  }
  else if ((pamerr = pam_acct_mgmt(pamh, PAM_SILENT)) != PAM_SUCCESS)
  {
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "pam_acct_mgmt() returned %d (%s)", pamerr, pam_strerror(pamh, pamerr));
  }

  if (pamh)
    pam_end(pamh, PAM_SUCCESS);

  if (pamerr == PAM_SUCCESS)
  {
    papplLogClient(client, PAPPL_LOGLEVEL_INFO, "PAM authentication of '%s' succeeded.", username);
    status = 1;
  }
#endif // HAVE_LIBPAM

  return (status);
}


#ifdef HAVE_LIBPAM
//
// 'pappl_pam_func()' - PAM conversation function.
//

static int				// O - Success or failure
pappl_pam_func(
    int                      num_msg,	// I - Number of messages
    const struct pam_message **msg,	// I - Messages
    struct pam_response      **resp,	// O - Responses
    _pappl_authdata_t        *data)	// I - Authentication data
{
  int			i;		// Looping var
  struct pam_response	*replies;	// Replies


  // Allocate memory for the responses...
  if ((replies = calloc((size_t)num_msg, sizeof(struct pam_response))) == NULL)
    return (PAM_CONV_ERR);

  // Answer all of the messages...
  for (i = 0; i < num_msg; i ++)
  {
    switch (msg[i]->msg_style)
    {
      case PAM_PROMPT_ECHO_ON :
          replies[i].resp_retcode = PAM_SUCCESS;
          replies[i].resp         = strdup(data->username);
          break;

      case PAM_PROMPT_ECHO_OFF :
          replies[i].resp_retcode = PAM_SUCCESS;
          replies[i].resp         = strdup(data->password);
          break;

      default :
          replies[i].resp_retcode = PAM_SUCCESS;
          replies[i].resp         = NULL;
          break;
    }
  }

  // Return the responses back to PAM...
  *resp = replies;

  return (PAM_SUCCESS);
}
#endif // HAVE_LIBPAM
