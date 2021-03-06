#include <Python.h>

#ifndef WIN32

#  include <sys/types.h>
#  include <sys/socket.h>
#  include <net/if.h>
#  include <netdb.h>

#  if HAVE_SOCKET_IOCTLS
#    include <sys/ioctl.h>
#    include <netinet/in.h>
#    include <arpa/inet.h>
#  endif /* HAVE_SOCKET_IOCTLS */

#if HAVE_NET_IF_DL_H
#  include <net/if_dl.h>
#endif

/* For the benefit of stupid platforms (Linux), include all the sockaddr
   definitions we can lay our hands on. */
#if !HAVE_SOCKADDR_SA_LEN
#  if HAVE_NETASH_ASH_H
#    include <netash/ash.h>
#  endif
#  if HAVE_NETATALK_AT_H
#    include <netatalk/at.h>
#  endif
#  if HAVE_NETAX25_AX25_H
#    include <netax25/ax25.h>
#  endif
#  if HAVE_NETECONET_EC_H
#    include <neteconet/ec.h>
#  endif
#  if HAVE_NETIPX_IPX_H
#    include <netipx/ipx.h>
#  endif
#  if HAVE_NETPACKET_PACKET_H
#    include <netpacket/packet.h>
#  endif
#  if HAVE_NETROSE_ROSE_H
#    include <netrose/rose.h>
#  endif
#  if HAVE_LINUX_IRDA_H
#    include <linux/irda.h>
#  endif
#  if HAVE_LINUX_ATM_H
#    include <linux/atm.h>
#  endif
#  if HAVE_LINUX_LLC_H
#    include <linux/llc.h>
#  endif
#  if HAVE_LINUX_TIPC_H
#    include <linux/tipc.h>
#  endif
#  if HAVE_LINUX_DN_H
#    include <linux/dn.h>
#  endif

/* Map address families to sizes of sockaddr structs */
static int af_to_len(int af) 
{
  switch (af) {
  case AF_INET: return sizeof (struct sockaddr_in);
#if defined(AF_INET6) && HAVE_SOCKADDR_IN6
  case AF_INET6: return sizeof (struct sockaddr_in6);
#endif
#if defined(AF_AX25) && HAVE_SOCKADDR_AX25
#  if defined(AF_NETROM)
  case AF_NETROM: /* I'm assuming this is carried over x25 */
#  endif
  case AF_AX25: return sizeof (struct sockaddr_ax25);
#endif
#if defined(AF_IPX) && HAVE_SOCKADDR_IPX
  case AF_IPX: return sizeof (struct sockaddr_ipx);
#endif
#if defined(AF_APPLETALK) && HAVE_SOCKADDR_AT
  case AF_APPLETALK: return sizeof (struct sockaddr_at);
#endif
#if defined(AF_ATMPVC) && HAVE_SOCKADDR_ATMPVC
  case AF_ATMPVC: return sizeof (struct sockaddr_atmpvc);
#endif
#if defined(AF_ATMSVC) && HAVE_SOCKADDR_ATMSVC
  case AF_ATMSVC: return sizeof (struct sockaddr_atmsvc);
#endif
#if defined(AF_X25) && HAVE_SOCKADDR_X25
  case AF_X25: return sizeof (struct sockaddr_x25);
#endif
#if defined(AF_ROSE) && HAVE_SOCKADDR_ROSE
  case AF_ROSE: return sizeof (struct sockaddr_rose);
#endif
#if defined(AF_DECnet) && HAVE_SOCKADDR_DN
  case AF_DECnet: return sizeof (struct sockaddr_dn);
#endif
#if defined(AF_PACKET) && HAVE_SOCKADDR_LL
  case AF_PACKET: return sizeof (struct sockaddr_ll);
#endif
#if defined(AF_ASH) && HAVE_SOCKADDR_ASH
  case AF_ASH: return sizeof (struct sockaddr_ash);
#endif
#if defined(AF_ECONET) && HAVE_SOCKADDR_EC
  case AF_ECONET: return sizeof (struct sockaddr_ec);
#endif
#if defined(AF_IRDA) && HAVE_SOCKADDR_IRDA
  case AF_IRDA: return sizeof (struct sockaddr_irda);
#endif
  }
  return sizeof (struct sockaddr);
}

#define SA_LEN(sa)      af_to_len(sa->sa_family)
#else
#define SA_LEN(sa)      sa->sa_len
#endif /* !HAVE_SOCKADDR_SA_LEN */

#  if HAVE_GETIFADDRS
#    include <ifaddrs.h>
#  endif /* HAVE_GETIFADDRS */

#  if !HAVE_GETIFADDRS && (!HAVE_SOCKET_IOCTLS || !HAVE_SIOCGIFCONF)
/* If the platform doesn't define, what we need, barf.  If you're seeing this,
   it means you need to write suitable code to retrieve interface information
   on your system. */
#    error You need to add code for your platform.
#  endif

#else /* defined(WIN32) */

#  include <winsock2.h>
#  include <iphlpapi.h>

#endif /* defined(WIN32) */

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

/* On systems without AF_LINK (Windows, for instance), define it anyway, but
   give it a crazy value.  On Linux, which has AF_PACKET but not AF_LINK,
   define AF_LINK as the latter instead. */
#ifndef AF_LINK
#  ifdef AF_PACKET
#    define AF_LINK  AF_PACKET
#  else
#    define AF_LINK  -1000
#  endif
#  define HAVE_AF_LINK 0
#else
#  define HAVE_AF_LINK 1
#endif

#if !defined(WIN32)
#if  !HAVE_GETNAMEINFO
#undef getnameinfo
#undef NI_NUMERICHOST

#define getnameinfo our_getnameinfo
#define NI_NUMERICHOST 1

/* A very simple getnameinfo() for platforms without */
static int
getnameinfo (const struct sockaddr *addr, int addr_len,
             char *buffer, int buflen,
             char *buf2, int buf2len,
             int flags)
{
  switch (addr->sa_family) {
  case AF_INET:
    {
      const struct sockaddr_in *sin = (struct sockaddr_in *)addr;
      const unsigned char *bytes = (unsigned char *)&sin->sin_addr.s_addr;
      char tmpbuf[20];

      sprintf (tmpbuf, "%d.%d.%d.%d",
               bytes[0], bytes[1], bytes[2], bytes[3]);

      strncpy (buffer, tmpbuf, buflen);
    }
    break;
#ifdef AF_INET6
  case AF_INET6:
    {
      const struct sockaddr_in6 *sin = (const struct sockaddr_in6 *)addr;
      const unsigned char *bytes = sin->sin6_addr.s6_addr;
      int n;
      char tmpbuf[80], *ptr = tmpbuf;
      int done_double_colon = FALSE;
      int colon_mode = FALSE;

      for (n = 0; n < 8; ++n) {
        unsigned char b1 = bytes[2 * n];
        unsigned char b2 = bytes[2 * n + 1];

        if (b1) {
          if (colon_mode) {
            colon_mode = FALSE;
            *ptr++ = ':';
          }
          sprintf (ptr, "%x%02x", b1, b2);
          ptr += strlen (ptr);
          *ptr++ = ':';
        } else if (b2) {
          if (colon_mode) {
            colon_mode = FALSE;
            *ptr++ = ':';
          }
          sprintf (ptr, "%x", b2);
          ptr += strlen (ptr);
          *ptr++ = ':';
        } else {
          if (!colon_mode) {
            if (done_double_colon) {
              *ptr++ = '0';
              *ptr++ = ':';
            } else {
              if (n == 0)
                *ptr++ = ':';
              colon_mode = TRUE;
              done_double_colon = TRUE;
            }
          }
        }
      }
      if (colon_mode) {
        colon_mode = FALSE;
        *ptr++ = ':';
        *ptr++ = '\0';
      } else {
        *--ptr = '\0';
      }

      strncpy (buffer, tmpbuf, buflen);
    }
    break;
#endif /* AF_INET6 */
  default:
    return -1;
  }

  return 0;
}
#endif

static int
string_from_sockaddr (struct sockaddr *addr,
                      char *buffer,
                      int buflen)
{
  if (!addr || addr->sa_family == AF_UNSPEC)
    return -1;

  if (getnameinfo (addr, SA_LEN(addr),
                   buffer, buflen,
                   NULL, 0,
                   NI_NUMERICHOST) != 0) {
    int n, len;
    char *ptr;
    const char *data;
      
    len = SA_LEN(addr);

#if HAVE_AF_LINK
    /* BSD-like systems have AF_LINK */
    if (addr->sa_family == AF_LINK) {
      struct sockaddr_dl *dladdr = (struct sockaddr_dl *)addr;
      len = dladdr->sdl_alen;
      data = LLADDR(dladdr);
    } else {
#endif
#if defined(AF_PACKET)
      /* Linux has AF_PACKET instead */
      if (addr->sa_family == AF_PACKET) {
        struct sockaddr_ll *lladdr = (struct sockaddr_ll *)addr;
        len = lladdr->sll_halen;
        data = (const char *)lladdr->sll_addr;
      } else {
#endif
        /* We don't know anything about this sockaddr, so just display
           the entire data area in binary. */
        len -= (sizeof (struct sockaddr) - sizeof (addr->sa_data));
        data = addr->sa_data;
#if defined(AF_PACKET)
      }
#endif
#if HAVE_AF_LINK
    }
#endif

    if (buflen < 3 * len)
      return -1;

    ptr = buffer;
    buffer[0] = '\0';

    for (n = 0; n < len; ++n) {
      sprintf (ptr, "%02x:", data[n] & 0xff);
      ptr += 3;
    }
    *--ptr = '\0';
  }

  return 0;
}
#endif /* !defined(WIN32) */

static int
add_to_family (PyObject *result, int family, PyObject *dict)
{
  PyObject *py_family = PyInt_FromLong (family);
  PyObject *list = PyDict_GetItem (result, py_family);

  if (!py_family) {
    Py_DECREF (dict);
    Py_XDECREF (list);
    return FALSE;
  }

  if (!list) {
    list = PyList_New (1);
    if (!list) {
      Py_DECREF (dict);
      Py_DECREF (py_family);
      return FALSE;
    }

    PyList_SET_ITEM (list, 0, dict);
    PyDict_SetItem (result, py_family, list);
    Py_DECREF (list);
  } else {
    PyList_Append (list, dict);
    Py_DECREF (dict);
  }

  return TRUE;
}

static PyObject *
ifaddrs (PyObject *self, PyObject *args)
{
  const char *ifname;
  PyObject *result;
  int found = FALSE;
#if defined(WIN32)
  PIP_ADAPTER_INFO pAdapterInfo = NULL;
  PIP_ADAPTER_INFO pInfo = NULL;
  ULONG ulBufferLength = 0;
  DWORD dwRet;
  PIP_ADDR_STRING str;
#endif

  if (!PyArg_ParseTuple (args, "s", &ifname))
    return NULL;

  result = PyDict_New ();

  if (!result)
    return NULL;

#if defined(WIN32)
  /* First, retrieve the adapter information.  We do this in a loop, in
     case someone adds or removes adapters in the meantime. */
  do {
    dwRet = GetAdaptersInfo(pAdapterInfo, &ulBufferLength);

    if (dwRet == ERROR_BUFFER_OVERFLOW) {
      if (pAdapterInfo)
        free (pAdapterInfo);
      pAdapterInfo = (PIP_ADAPTER_INFO)malloc (ulBufferLength);

      if (!pAdapterInfo) {
        Py_DECREF (result);
        PyErr_SetString (PyExc_MemoryError, "Not enough memory");
        return NULL;
      }
    }
  } while (dwRet == ERROR_BUFFER_OVERFLOW);

  /* If we failed, then fail in Python too */
  if (dwRet != ERROR_SUCCESS && dwRet != ERROR_NO_DATA) {
    Py_DECREF (result);
    if (pAdapterInfo)
      free (pAdapterInfo);

    PyErr_SetString (PyExc_OSError,
                     "Unable to obtain adapter information.");
    return NULL;
  }

  for (pInfo = pAdapterInfo; pInfo; pInfo = pInfo->Next) {
    char buffer[256];

    if (strcmp (pInfo->AdapterName, ifname) != 0)
      continue;

    found = TRUE;

    /* Do the physical address */
    if (256 >= 3 * pInfo->AddressLength) {
      PyObject *hwaddr, *dict;
      char *ptr = buffer;
      unsigned n;
      
      *ptr = '\0';
      for (n = 0; n < pInfo->AddressLength; ++n) {
        sprintf (ptr, "%02x:", pInfo->Address[n] & 0xff);
        ptr += 3;
      }
      *--ptr = '\0';

      hwaddr = PyString_FromString (buffer);
      dict = PyDict_New ();

      if (!dict) {
        Py_XDECREF (hwaddr);
        Py_DECREF (result);
        free (pAdapterInfo);
        return NULL;
      }

      PyDict_SetItemString (dict, "addr", hwaddr);
      Py_DECREF (hwaddr);

      if (!add_to_family (result, AF_LINK, dict)) {
        Py_DECREF (result);
        free (pAdapterInfo);
        return NULL;
      }
    }

    for (str = &pInfo->IpAddressList; str; str = str->Next) {
      PyObject *addr = PyString_FromString (str->IpAddress.String);
      PyObject *mask = PyString_FromString (str->IpMask.String);
      PyObject *bcast = NULL;
      PyObject *dict;

      /* If this isn't the loopback interface, work out the broadcast
         address, for better compatibility with other platforms. */
      if (pInfo->Type != MIB_IF_TYPE_LOOPBACK) {
        unsigned long inaddr = inet_addr (str->IpAddress.String);
        unsigned long inmask = inet_addr (str->IpMask.String);
        struct in_addr in;
        char *brstr;

        in.S_un.S_addr = (inaddr | ~inmask) & 0xfffffffful;

        brstr = inet_ntoa (in);

        if (brstr)
          bcast = PyString_FromString (brstr);
      }

      dict = PyDict_New ();

      if (!dict) {
        Py_XDECREF (addr);
        Py_XDECREF (mask);
        Py_XDECREF (bcast);
        Py_DECREF (result);
        free (pAdapterInfo);
        return NULL;
      }

      if (addr)
        PyDict_SetItemString (dict, "addr", addr);
      if (mask)
        PyDict_SetItemString (dict, "netmask", mask);
      if (bcast)
        PyDict_SetItemString (dict, "broadcast", bcast);

      Py_XDECREF (addr);
      Py_XDECREF (mask);
      Py_XDECREF (bcast);

      if (!add_to_family (result, AF_INET, dict)) {
        Py_DECREF (result);
        free (pAdapterInfo);
        return NULL;
      }
    }
  }

  free (pAdapterInfo);
#elif HAVE_GETIFADDRS
  struct ifaddrs *addrs = NULL;
  struct ifaddrs *addr = NULL;

  if (getifaddrs (&addrs) < 0) {
    Py_DECREF (result);
    PyErr_SetFromErrno (PyExc_OSError);
    return NULL;
  }

  for (addr = addrs; addr; addr = addr->ifa_next) {
    char buffer[256];
    PyObject *pyaddr = NULL, *netmask = NULL, *braddr = NULL;

    if (strcmp (addr->ifa_name, ifname) != 0)
      continue;
 
    /* Sometimes there are records without addresses (e.g. in the case of a
       dial-up connection via ppp, which on Linux can have a link address
       record with no actual address).  We skip these as they aren't useful.
       Thanks to Christian Kauhaus for reporting this issue. */
    if (!addr->ifa_addr)
      continue;  

    found = TRUE;

    if (string_from_sockaddr (addr->ifa_addr, buffer, sizeof (buffer)) == 0)
      pyaddr = PyString_FromString (buffer);

    if (string_from_sockaddr (addr->ifa_netmask, buffer, sizeof (buffer)) == 0)
      netmask = PyString_FromString (buffer);

    if (string_from_sockaddr (addr->ifa_broadaddr, buffer, sizeof (buffer)) == 0)
      braddr = PyString_FromString (buffer);

    PyObject *dict = PyDict_New();

    if (!dict) {
      Py_XDECREF (pyaddr);
      Py_XDECREF (netmask);
      Py_XDECREF (braddr);
      Py_DECREF (result);
      freeifaddrs (addrs);
      return NULL;
    }

    if (pyaddr)
      PyDict_SetItemString (dict, "addr", pyaddr);
    if (netmask)
      PyDict_SetItemString (dict, "netmask", netmask);

    if (braddr) {
      if (addr->ifa_flags & (IFF_POINTOPOINT | IFF_LOOPBACK))
        PyDict_SetItemString (dict, "peer", braddr);
      else
        PyDict_SetItemString (dict, "broadcast", braddr);
    }

    Py_XDECREF (pyaddr);
    Py_XDECREF (netmask);
    Py_XDECREF (braddr);

    if (!add_to_family (result, addr->ifa_addr->sa_family, dict)) {
      Py_DECREF (result);
      freeifaddrs (addrs);
      return NULL;
    }
  }

  freeifaddrs (addrs);
#elif HAVE_SOCKET_IOCTLS
  
  int sock = socket(AF_INET, SOCK_DGRAM, 0);

  if (sock < 0) {
    Py_DECREF (result);
    PyErr_SetFromErrno (PyExc_OSError);
    return NULL;
  }

  struct ifreq ifr;
  PyObject *addr = NULL, *netmask = NULL, *braddr = NULL, *dstaddr = NULL;
  int is_p2p = FALSE;
  char buffer[256];

  strncpy (ifr.ifr_name, ifname, IFNAMSIZ);

#if HAVE_SIOCGIFHWADDR
  if (ioctl (sock, SIOCGIFHWADDR, &ifr) == 0) {
    found = TRUE;

    if (string_from_sockaddr (ifr->ifr_addr, buffer, sizeof (buffer)) == 0) {
      PyObject *hwaddr = PyString_FromString (buffer);
      PyObject *dict = PyDict_New ();
      PyObject *list = PyList_New (1);
      PyObject *family = PyInt_FromLong (AF_LINK);

      if (!hwaddr || !dict || !list || !family) {
        Py_XDECREF (hwaddr);
        Py_XDECREF (dict);
        Py_XDECREF (list)
        Py_XDECREF (family);
        Py_XDECREF (result);
        close (sock);
        return NULL;
      }

      PyDict_SetItemString (dict, "addr", hwaddr);
      Py_DECREF (hwaddr);

      PyList_SET_ITEM (list, 0, dict);

      PyDict_SetItem (result, family, list);
      Py_DECREF (family);
      Py_DECREF (list);
    }
  }
#endif

#if HAVE_SIOCGIFADDR
  if (ioctl (sock, SIOCGIFADDR, &ifr) == 0) {
    found = TRUE;

    if (string_from_sockaddr (&ifr.ifr_addr, buffer, sizeof (buffer)) == 0)
      addr = PyString_FromString (buffer);
  }
#endif

#if HAVE_SIOCGIFNETMASK
  if (ioctl (sock, SIOCGIFNETMASK, &ifr) == 0) {
    found = TRUE;

    if (string_from_sockaddr (&ifr.ifr_addr, buffer, sizeof (buffer)) == 0)
      netmask = PyString_FromString (buffer);
  }
#endif

#if HAVE_SIOCGIFFLAGS
  if (ioctl (sock, SIOCGIFFLAGS, &ifr) == 0) {
    found = TRUE;

    if (ifr.ifr_flags & IFF_POINTOPOINT)
      is_p2p = TRUE;
  }
#endif

#if HAVE_SIOCGIFBRDADDR
  if (!is_p2p && ioctl (sock, SIOCGIFBRDADDR, &ifr) == 0) {
    found = TRUE;

    if (string_from_sockaddr (&ifr.ifr_addr, buffer, sizeof (buffer)) == 0)
      braddr = PyString_FromString (buffer);
  }
#endif

#if HAVE_SIOCGIFDSTADDR
  if (is_p2p && ioctl (sock, SIOCGIFBRDADDR, &ifr) == 0) {
    found = TRUE;

    if (string_from_sockaddr (&ifr.ifr_addr, buffer, sizeof (buffer)) == 0)
      dstaddr = PyString_FromString (buffer);
  }
#endif

  PyObject *dict = PyDict_New();

  if (!dict) {
    Py_XDECREF (addr);
    Py_XDECREF (netmask);
    Py_XDECREF (braddr);
    Py_XDECREF (dstaddr);
    Py_DECREF (result);
    close (sock);
    return NULL;
  }

  if (addr)
    PyDict_SetItemString (dict, "addr", addr);
  if (netmask)
    PyDict_SetItemString (dict, "netmask", netmask);
  if (braddr)
    PyDict_SetItemString (dict, "broadcast", braddr);
  if (dstaddr)
    PyDict_SetItemString (dict, "peer", dstaddr);

  Py_XDECREF (addr);
  Py_XDECREF (netmask);
  Py_XDECREF (braddr);
  Py_XDECREF (dstaddr);

  if (!PyDict_Size (dict))
    Py_DECREF (dict);
  else {
    PyObject *list = PyList_New(1);
  
    if (!list) {
      Py_DECREF (dict);
      Py_DECREF (result);
      close (sock);
      return NULL;
    }

    PyList_SET_ITEM (list, 0, dict);

    PyObject *family = PyInt_FromLong (AF_INET);
    if (!family) {
      Py_DECREF (result);
      Py_DECREF (list);
      close (sock);
      return NULL;
    }

    PyDict_SetItem (result, family, list);
    Py_DECREF (family);
    Py_DECREF (list);
  }

  close (sock);

#endif /* HAVE_SOCKET_IOCTLS */

  if (found)
    return result;
  else {
    Py_DECREF (result);
    PyErr_SetString (PyExc_ValueError, 
                     "You must specify a valid interface name.");
    return NULL;
  }
}

static PyObject *
interfaces (PyObject *self)
{
  PyObject *result;

#if defined(WIN32)
  PIP_ADAPTER_INFO pAdapterInfo = NULL;
  PIP_ADAPTER_INFO pInfo = NULL;
  ULONG ulBufferLength = 0;
  DWORD dwRet;

  /* First, retrieve the adapter information */
  do {
    dwRet = GetAdaptersInfo(pAdapterInfo, &ulBufferLength);

    if (dwRet == ERROR_BUFFER_OVERFLOW) {
      if (pAdapterInfo)
        free (pAdapterInfo);
      pAdapterInfo = (PIP_ADAPTER_INFO)malloc (ulBufferLength);

      if (!pAdapterInfo) {
        PyErr_SetString (PyExc_MemoryError, "Not enough memory");
        return NULL;
      }
    }
  } while (dwRet == ERROR_BUFFER_OVERFLOW);

  /* If we failed, then fail in Python too */
  if (dwRet != ERROR_SUCCESS && dwRet != ERROR_NO_DATA) {
    if (pAdapterInfo)
      free (pAdapterInfo);

    PyErr_SetString (PyExc_OSError,
                     "Unable to obtain adapter information.");
    return NULL;
  }

  result = PyList_New(0);

  if (dwRet == ERROR_NO_DATA) {
    free (pAdapterInfo);
    return result;
  }

  for (pInfo = pAdapterInfo; pInfo; pInfo = pInfo->Next) {
    PyObject *ifname = PyString_FromString (pInfo->AdapterName);

    PyList_Append (result, ifname);
    Py_DECREF (ifname);
  }

  free (pAdapterInfo);
#elif HAVE_GETIFADDRS
  const char *prev_name = NULL;
  struct ifaddrs *addrs = NULL;
  struct ifaddrs *addr = NULL;

  result = PyList_New (0);

  if (getifaddrs (&addrs) < 0) {
    Py_DECREF (result);
    PyErr_SetFromErrno (PyExc_OSError);
    return NULL;
  }

  for (addr = addrs; addr; addr = addr->ifa_next) {
    if (!prev_name || strncmp (addr->ifa_name, prev_name, IFNAMSIZ) != 0) {
      PyObject *ifname = PyString_FromString (addr->ifa_name);
    
      if (!PySequence_Contains (result, ifname))
        PyList_Append (result, ifname);
      Py_DECREF (ifname);
      prev_name = addr->ifa_name;
    }
  }

  freeifaddrs (addrs);
#elif HAVE_SIOCGIFCONF
  const char *prev_name = NULL;
  int fd = socket (AF_INET, SOCK_DGRAM, 0);
  struct ifconf ifc;
  int len = -1, n;

  if (fd < 0) {
    PyErr_SetFromErrno (PyExc_OSError);
    return NULL;
  }

  // Try to find out how much space we need
#if HAVE_SIOCGSIZIFCONF
  if (ioctl (fd, SIOCGSIZIFCONF, &len) < 0)
    len = -1;
#endif

  // As a last resort, guess
  if (len < 0)
    len = 64;

  ifc.ifc_len = len * sizeof (struct ifreq);
  ifc.ifc_buf = malloc (ifc.ifc_len);

  if (!ifc.ifc_buf) {
    PyErr_SetString (PyExc_MemoryError, "Not enough memory");
    close (fd);
    return NULL;
  }

  if (ioctl (fd, SIOCGIFCONF, &ifc) < 0) {
    free (ifc.ifc_req);
    PyErr_SetFromErrno (PyExc_OSError);
    close (fd);
    return NULL;
  }

  result = PyList_New (0);
  struct ifreq *pfreq = ifc.ifc_req;
  for (n = 0; n < ifc.ifc_len;
       pfreq = (struct ifreq *)((caddr_t)pfreq + len), n += len) {
    len = sizeof (pfreq->ifr_name) + pfreq->ifr_addr.sa_len;

    if (!prev_name || strncmp (prev_name, pfreq->ifr_name, IFNAMSIZ) != 0) {
      PyObject *name = PyString_FromString (pfreq->ifr_name);

      if (!PySequence_Contains (result, name))
        PyList_Append (result, name);
      Py_XDECREF (name);

      prev_name = pfreq->ifr_name;
    }
  }

  free (ifc.ifc_buf);
  close (fd);
#endif /* HAVE_SIOCGIFCONF */

  return result;
}

static PyMethodDef methods[] = {
  { "ifaddresses", (PyCFunction)ifaddrs, METH_VARARGS,
    "Obtain information about the specified network interface.\n"
"\n"
"Returns a dict whose keys are equal to the address family constants,\n"
"e.g. netifaces.AF_INET, and whose values are a list of addresses in\n"
"that family that are attached to the network interface." },
  { "interfaces", (PyCFunction)interfaces, METH_NOARGS,
    "Obtain a list of the interfaces available on this machine." },
  { NULL, NULL, 0, NULL }
};

PyMODINIT_FUNC
initnetifaces (void)
{
  PyObject *m;

#ifdef WIN32
  WSADATA wsad;
  int iResult;
  
  iResult = WSAStartup(MAKEWORD (2, 2), &wsad);
#endif

  m = Py_InitModule ("netifaces", methods);

  /* Address families (auto-detect using #ifdef) */
#ifdef AF_UNSPEC  
  PyModule_AddIntConstant (m, "AF_UNSPEC", AF_UNSPEC);
#endif
#ifdef AF_UNIX
  PyModule_AddIntConstant (m, "AF_UNIX", AF_UNIX);
#endif
#ifdef AF_FILE
  PyModule_AddIntConstant (m, "AF_FILE", AF_FILE);
#endif
#ifdef AF_INET
  PyModule_AddIntConstant (m, "AF_INET", AF_INET);
#endif
#ifdef AF_AX25
  PyModule_AddIntConstant (m, "AF_AX25", AF_AX25);
#endif
#ifdef AF_IMPLINK  
  PyModule_AddIntConstant (m, "AF_IMPLINK", AF_IMPLINK);
#endif
#ifdef AF_PUP  
  PyModule_AddIntConstant (m, "AF_PUP", AF_PUP);
#endif
#ifdef AF_CHAOS
  PyModule_AddIntConstant (m, "AF_CHAOS", AF_CHAOS);
#endif
#ifdef AF_NS
  PyModule_AddIntConstant (m, "AF_NS", AF_NS);
#endif
#ifdef AF_ISO
  PyModule_AddIntConstant (m, "AF_ISO", AF_ISO);
#endif
#ifdef AF_ECMA
  PyModule_AddIntConstant (m, "AF_ECMA", AF_ECMA);
#endif
#ifdef AF_DATAKIT
  PyModule_AddIntConstant (m, "AF_DATAKIT", AF_DATAKIT);
#endif
#ifdef AF_CCITT
  PyModule_AddIntConstant (m, "AF_CCITT", AF_CCITT);
#endif
#ifdef AF_SNA
  PyModule_AddIntConstant (m, "AF_SNA", AF_SNA);
#endif
#ifdef AF_DECnet
  PyModule_AddIntConstant (m, "AF_DECnet", AF_DECnet);
#endif
#ifdef AF_DLI
  PyModule_AddIntConstant (m, "AF_DLI", AF_DLI);
#endif
#ifdef AF_LAT
  PyModule_AddIntConstant (m, "AF_LAT", AF_LAT);
#endif
#ifdef AF_HYLINK
  PyModule_AddIntConstant (m, "AF_HYLINK", AF_HYLINK);
#endif
#ifdef AF_APPLETALK
  PyModule_AddIntConstant (m, "AF_APPLETALK", AF_APPLETALK);
#endif
#ifdef AF_ROUTE
  PyModule_AddIntConstant (m, "AF_ROUTE", AF_ROUTE);
#endif
#ifdef AF_LINK
  PyModule_AddIntConstant (m, "AF_LINK", AF_LINK);
#endif
#ifdef AF_PACKET
  PyModule_AddIntConstant (m, "AF_PACKET", AF_PACKET);
#endif
#ifdef AF_COIP
  PyModule_AddIntConstant (m, "AF_COIP", AF_COIP);
#endif
#ifdef AF_CNT
  PyModule_AddIntConstant (m, "AF_CNT", AF_CNT);
#endif
#ifdef AF_IPX
  PyModule_AddIntConstant (m, "AF_IPX", AF_IPX);
#endif
#ifdef AF_SIP
  PyModule_AddIntConstant (m, "AF_SIP", AF_SIP);
#endif
#ifdef AF_NDRV
  PyModule_AddIntConstant (m, "AF_NDRV", AF_NDRV);
#endif
#ifdef AF_ISDN
  PyModule_AddIntConstant (m, "AF_ISDN", AF_ISDN);
#endif
#ifdef AF_INET6
  PyModule_AddIntConstant (m, "AF_INET6", AF_INET6);
#endif
#ifdef AF_NATM
  PyModule_AddIntConstant (m, "AF_NATM", AF_NATM);
#endif
#ifdef AF_SYSTEM
  PyModule_AddIntConstant (m, "AF_SYSTEM", AF_SYSTEM);
#endif
#ifdef AF_NETBIOS
  PyModule_AddIntConstant (m, "AF_NETBIOS", AF_NETBIOS);
#endif
#ifdef AF_NETBEUI
  PyModule_AddIntConstant (m, "AF_NETBEUI", AF_NETBEUI);
#endif
#ifdef AF_PPP
  PyModule_AddIntConstant (m, "AF_PPP", AF_PPP);
#endif
#ifdef AF_ATM
  PyModule_AddIntConstant (m, "AF_ATM", AF_ATM);
#endif
#ifdef AF_ATMPVC
  PyModule_AddIntConstant (m, "AF_ATMPVC", AF_ATMPVC);
#endif
#ifdef AF_ATMSVC
  PyModule_AddIntConstant (m, "AF_ATMSVC", AF_ATMSVC);
#endif
#ifdef AF_NETGRAPH
  PyModule_AddIntConstant (m, "AF_NETGRAPH", AF_NETGRAPH);
#endif
#ifdef AF_VOICEVIEW
  PyModule_AddIntConstant (m, "AF_VOICEVIEW", AF_VOICEVIEW);
#endif
#ifdef AF_FIREFOX
  PyModule_AddIntConstant (m, "AF_FIREFOX", AF_FIREFOX);
#endif
#ifdef AF_UNKNOWN1
  PyModule_AddIntConstant (m, "AF_UNKNOWN1", AF_UNKNOWN1);
#endif
#ifdef AF_BAN
  PyModule_AddIntConstant (m, "AF_BAN", AF_BAN);
#endif
#ifdef AF_CLUSTER
  PyModule_AddIntConstant (m, "AF_CLUSTER", AF_CLUSTER);
#endif
#ifdef AF_12844
  PyModule_AddIntConstant (m, "AF_12844", AF_12844);
#endif
#ifdef AF_IRDA
  PyModule_AddIntConstant (m, "AF_IRDA", AF_IRDA);
#endif
#ifdef AF_NETDES
  PyModule_AddIntConstant (m, "AF_NETDES", AF_NETDES);
#endif
#ifdef AF_NETROM
  PyModule_AddIntConstant (m, "AF_NETROM", AF_NETROM);
#endif
#ifdef AF_BRIDGE
  PyModule_AddIntConstant (m, "AF_BRIDGE", AF_BRIDGE);
#endif
#ifdef AF_X25
  PyModule_AddIntConstant (m, "AF_X25", AF_X25);
#endif
#ifdef AF_ROSE
  PyModule_AddIntConstant (m, "AF_ROSE", AF_ROSE);
#endif
#ifdef AF_SECURITY
  PyModule_AddIntConstant (m, "AF_SECURITY", AF_SECURITY);
#endif
#ifdef AF_KEY
  PyModule_AddIntConstant (m, "AF_KEY", AF_KEY);
#endif
#ifdef AF_NETLINK
  PyModule_AddIntConstant (m, "AF_NETLINK", AF_NETLINK);
#endif
#ifdef AF_ASH
  PyModule_AddIntConstant (m, "AF_ASH", AF_ASH);
#endif
#ifdef AF_ECONET
  PyModule_AddIntConstant (m, "AF_ECONET", AF_ECONET);
#endif
#ifdef AF_SNA
  PyModule_AddIntConstant (m, "AF_SNA", AF_SNA);
#endif
#ifdef AF_PPPOX
  PyModule_AddIntConstant (m, "AF_PPPOX", AF_PPPOX);
#endif
#ifdef AF_WANPIPE
  PyModule_AddIntConstant (m, "AF_WANPIPE", AF_WANPIPE);
#endif
#ifdef AF_BLUETOOTH
  PyModule_AddIntConstant (m, "AF_BLUETOOTH", AF_BLUETOOTH);
#endif
}
