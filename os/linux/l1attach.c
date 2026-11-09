#include "tnn.h"
#include "tcpkiss.h"

static ULONG   token_sent = 0L;

/* Pruefen, ob KissType in der TBL gesetzt ist. */
static BOOLEAN SearchKissType(UWORD Interface)
{
  register int i;

  /* Alle Ports durchgehen. */
  for (i = 0; i < L1PNUM; ++i)
    /* KissType gleich. */
    if (l1port[i].kisstype == Interface)
      /* Ja. */
      return(TRUE);

  /* Nein, */
  return(FALSE);
}

#define ATT_PORT     1
#define ATT_KISSTYPE 2
#define ATT_SPEED    3
#define ATT_DEVICE   4
#define ATT_CHANNEL  5

ATTPRT attprt[] = {
  {"PORT",      ATT_PORT      },
  {"KTYP",      ATT_KISSTYPE  },
  {"SPEED",     ATT_SPEED     },
  {"DEVICE",    ATT_DEVICE    },
  {"CHANNEL",   ATT_CHANNEL   },
  {NULL,        0             }
};

ATTTYP attyp[] = {
  {"KISS",      KISS_NORMAL   },
  {"SMACK",     KISS_SMACK    },
  {"RMNC",      KISS_RMNC     },
  {"TOKENRING", KISS_TOK      },
  {"VANESSA",   KISS_VAN      },
  {"SCC",       KISS_SCC      },
  {"TF",        KISS_TF       },
  {"IPX",       KISS_IPX      },
  {"AX25IP",    KISS_AXIP     },
  {"LOOP",      KISS_LOOP     },
  {"KAX25",     KISS_KAX25    },
  {"KAX25KJD",  KISS_KAX25KJD },
  {"6PACK",     KISS_6PACK    },
#ifdef L1TELNET
  {"TELNET",    KISS_TELNET   },
#endif /* L1TELNET */
#ifdef L1HTTPD
  {"HTTPD",     KISS_HTTPD    },
#endif /* L1HTTPD */
#ifdef L1IPCONV
  {"IPCONV",    KISS_IPCONV   },
#endif /* L1IPCONV */
#ifdef L1IRC
  {"IRC",       KISS_IRC      },
#endif /* L1IRC */
  {"TCPKISS",   KISS_TCP      },
  {NULL,        0             }
};

#define ATT_NO_OPTION  0
#define ATT_POR_BUSY   1
#define ATT_INV_PORT   2
#define ATT_INV_TYP    3
#define ATT_KIS_BUSY   4
#define ATT_SPE_BUSY   5
#define ATT_INV_OPTION 6
#define ATT_INV_UDP    7
#define ATT_INV_TCP    8
#define ATT_INV_CHANNEL 9
#define ATT_DEVICE_BUSY 10
#define ATT_NO_DEVICE 11

static const char *errormsg[] =
{
  "No Option."
 ,"Port is busy."
 ,"Invalid Port."
 ,"Invalid KissType."
 ,"Kisstype is busy."
 ,"Invalid Speed."
 ,"Invalid Option."
 ,"Invalid UDP-Port."
 ,"Invalid TCP-Port."
 ,"Invalid or duplicate KISS channel (0..15, KTYP=KISS only)."
 ,"Device is busy or serial settings do not match."
 ,"No free interface."
};

/* KissType setzen. */
int SetKissType(WORD Interface, int port)
{
  switch (Interface)
  {
    case KISS_NORMAL:
    case KISS_SMACK:
    case KISS_RMNC:
    case KISS_TOK:
      return(Interface);

    break;
#ifdef VANESSA
    case KISS_VAN:
      /* Testen ob es ueberhaupt eine Vanessa gibt. */
      if (!van_test(port))
        return(KISS_NIX);

    break;
#endif
    case KISS_SCC:
    case KISS_TF:
    case KISS_KAX25:
    case KISS_KAX25KJD:
    case KISS_6PACK:
      return(Interface);

    break;

    /* Pruefe auf doppel-Eintraege! */
    case KISS_IPX:
    case KISS_AXIP:
    case KISS_LOOP:
#ifdef L1TELNET
    case KISS_TELNET:
#endif /* L1TELNET */
#ifdef L1HTTPD
    case KISS_HTTPD:
#endif /* L1HTTPD */
#ifdef L1IPCONV
    case KISS_IPCONV:
#endif /* L1IPCONV */
#ifdef L1IRC
    case KISS_IRC:
#endif /* L1IRC */
      /* KissType schon gesetzt? */
      if (!SearchKissType(Interface))
        /* Nein, dann jetzt setzen. */
        return(Interface);
        /* KissType existiert schon. */
      else
        /* nicht setzen. */
        return(KISS_NIX);

    break;

    /* Unbekanntes KissType. */
    default:
      /* nicht setzen. */
      return(KISS_NIX);

    break;
  }

  return(KISS_NIX);
}

/* Baudrate setzen. */
int SetSpeed(char *speed, UWORD KissType)
{
  int Speed = EOF;

  /* Baudrate holen. */
  if ((sscanf(speed, "%d", &Speed) != 1))
    /* Fehler, abbruch. */
    return(EOF);

  /* Geschwindigkeit fuer Tokenring-Port */
  if (KissType == KISS_TOK)
    /* Tokenring setzen. */
    tkcom = 1;

  switch(Speed)
  {
    case 0:
      return(FALSE);

    case 9600:
      if (KissType == KISS_TOK)
        tkbaud = 96;

      return(B9600);

    case 19200:
      if (KissType == KISS_TOK)
        tkbaud = 192;

      return(B19200);

    case 38400:
      if (KissType == KISS_TOK)
        tkbaud = 384;

      return(B38400);

    case 57600:
      if (KissType == KISS_TOK)
        tkbaud = 576;

#ifdef B57600
      return(B57600);
#else
      return(B38400);
#endif

    case 115200:
      if (KissType == KISS_TOK)
        tkbaud = 1152;

#ifdef B115200
      return(B115200);
#else
      return(B38400);
#endif

    case 230400:
      if (KissType == KISS_TOK)
        tkbaud = 2304;

#ifdef B230400
      return(B230400);
#else
      return(B38400);
#endif

    case 460800:
    if (KissType == KISS_TOK)
      tkbaud = 4608;

#ifdef B460800
      return(B460800);
#else
      return(B38400);
#endif

    /* unbekannte Geschwindigkeiten */
    default:
      return(EOF);
  }

  return(EOF);
}

/* Baudrateflags setzen. */
int SetSpeedflag(char *speed)
{
  int Speed = EOF;

  /* Baudrate holen. */
  if ((sscanf(speed, "%d", &Speed) != 1))
    /* Fehler, abbruch. */
    return(EOF);

  switch(Speed)
  {

  case 57600:
    return(ASYNC_SPD_HI);

  case 115200:
    return(ASYNC_SPD_VHI);

  case 230400:
    return(ASYNC_SPD_SHI);

  case 460800:
    return(ASYNC_SPD_WARP);

    /* unbekannte Geschwindigkeiten */
    default:
      return(FALSE);
  }

  return(FALSE);
}

/* Buffer "clipoi" einlesen. */
char *ReadBuf(char *cBuf, BOOLEAN flag)
{
  register int i;
  skipsp(&clicnt, &clipoi);

  for (i = 0; i < MAXPATH; ++i)
  {
    if ((!clicnt) || (*clipoi == ' '))
      break;

    if (*clipoi == '=')
    {
      clicnt--;
      clipoi++;
      break;
    }
    clicnt--;
    /* TRUE, In Grossbuchstaben umwandeln. */
    if (flag)
      cBuf[i] = toupper(*clipoi++);
      /* FALSE. */
    else
      /* keine Aenderungen durchfuehren. */
      cBuf[i] = (*clipoi++);
  }

  cBuf[i] = 0;

  return(cBuf);
}

/* Eine Modifizierte Funktion Init_kisslink(); */
BOOLEAN InitKissLink(UWORD port)
{
  DEVICE *l1att = &l1port[l1ptab[port]];
  int     Error = 0;

  if (l1att->kisstype == KISS_NORMAL)
  {
    struct serial_struct serial;
    l1att->kisslink = open(l1att->device, O_RDWR | O_NOCTTY);
    if (l1att->kisslink < 0)
    {
      printf("Error: can't open device %s (%s)\n", l1att->device, strerror(errno));
      return(TRUE);
    }
    if (tcgetattr(l1att->kisslink, &l1att->org_termios) < 0)
      goto kiss_error;
    l1att->wrk_termios = l1att->org_termios;
    l1att->wrk_termios.c_cc[VTIME] = 0;
    l1att->wrk_termios.c_cc[VMIN] = 0;
    l1att->wrk_termios.c_iflag = IGNBRK;
    l1att->wrk_termios.c_oflag = 0;
    l1att->wrk_termios.c_lflag = 0;
    l1att->wrk_termios.c_cflag &= ~(CSIZE | CSTOPB | PARENB | HUPCL | CRTSCTS);
    l1att->wrk_termios.c_cflag |= CS8 | CREAD | CLOCAL;
    if (l1att->speed != B0)
    {
      if (cfsetispeed(&l1att->wrk_termios, l1att->speed) < 0
          || cfsetospeed(&l1att->wrk_termios, l1att->speed) < 0)
        goto kiss_error;
      if (l1att->speed == B38400 && l1att->speedflag != 0)
      {
        if (ioctl(l1att->kisslink, TIOCGSERIAL, &serial) < 0)
          goto kiss_error;
        serial.flags = (serial.flags & ~ASYNC_SPD_MASK) | l1att->speedflag;
        if (ioctl(l1att->kisslink, TIOCSSERIAL, &serial) < 0)
          goto kiss_error;
      }
    }
    if (tcsetattr(l1att->kisslink, TCSADRAIN, &l1att->wrk_termios) < 0)
      goto kiss_error;
    if (l1att->speed == B0)
    {
      int flags = fcntl(l1att->kisslink, F_GETFL);
      if (flags < 0 || fcntl(l1att->kisslink, F_SETFL, flags | O_NONBLOCK) < 0)
      {
        int saved_errno = errno;
        if (tcsetattr(l1att->kisslink, TCSADRAIN, &l1att->org_termios) < 0)
          printf("Error: can't restore device %s (%s)\n", l1att->device, strerror(errno));
        errno = saved_errno;
        goto kiss_error;
      }
    }
    l1att->rx_state = ST_BEGIN;
    l1att->rx_port = 0;
    l1att->tx_port = 0;
    return(FALSE);

kiss_error:
    printf("Error: can't configure device %s (%s)\n", l1att->device, strerror(errno));
    close(l1att->kisslink);
    l1att->kisslink = -1;
    return(TRUE);
  }

  switch(l1att->kisstype)
  {
    case KISS_TOK:
      /* Nur die 1. Schnittstelle Initialisieren. */
      if (tokenring_ports > 1)
        return(FALSE);

    /* Alle Device haben hier nix zu suchen. */
#ifdef VANESSA
    case KISS_VAN:
#endif
    case KISS_SCC:
    case KISS_IPX:
    case KISS_AXIP:
    case KISS_LOOP:
    case KISS_KAX25:
    case KISS_KAX25KJD:
#ifdef L1TELNET
    case KISS_TELNET:
#endif /* L1TELNET */
#ifdef L1HTTPD
    case KISS_HTTPD:
#endif /* L1HTTPD */
#ifdef L1IPCONV
    case KISS_IPCONV:
#endif /* L1IPCONV */
#ifdef L1IRC
    case KISS_IRC:
#endif /* L1IRC */
      /* und weg hier. */
      return(FALSE);

#ifdef SIXPACK
    case KISS_6PACK:
      /* 6PACK-Ring ist schon Initialisiert. */
      if (iDescriptor)
        /* Schliesse schnittstelle. */
        close(iDescriptor);

      /* Neue Konfiguration Initialisieren. */
      Sixpack_l1init();

     return(FALSE);
#endif /* SIXPACK */
  }

/************************************************************************/
/*                                                                      */
/* Port oeffnen.                                                        */
/*                                                                      */
/************************************************************************/
  l1att->kisslink = open(l1att->device, l1att->speed);

  if (l1att->kisslink == KISS_NIX)
  {
    Error = 1;
    printf("Error: can't open device %s\n", l1att->device);
    printf("        (%s)\n", strerror(errno));
  }

/************************************************************************/
/*                                                                      */
/* Serielle Schnittstelle auf neue Parameter einstellen, KISS-Empfang   */
/* initialisieren                                                       */
/*                                                                      */
/************************************************************************/
  if (!Error)
  {
    l1att->rx_state = ST_BEGIN;
    l1att->rx_port = 0;
    l1att->tx_port = 0;

    if (l1att->kisstype == KISS_TF)
      tf_set_kiss(l1att);
  }

/************************************************************************/
/*                                                                      */
/* Wenn kein Fehler aufgetreten ist, dann fuer gewisse Interfaces die   */
/* ersten Aktionen ausfuehren.                                          */
/*                                                                      */
/* Tokenring : Token senden                                             */
/* 6PACK     : TNC-Zaehlung anstossen                                   */
/*                                                                      */
/************************************************************************/

  if (!Error)
  {
        /* Tokenring */
    if (l1att->kisstype == KISS_TOK)
    {
      send_kisscmd(0xff, CMD_TOKEN, 0);         /* Token senden          */
      token_sent = tic10;                       /* Zeitpunkt der Sendung */
      tokenflag = FALSE;                        /* Token unterwegs       */
    }
    return(FALSE);
  }

  if (Error)
  {
    /* Handle schliessen. */
    close(l1att->kisslink);
    return(TRUE);
  }

  /* Initialisierung war erfolgreich. */
  return(FALSE);
}

/* Port setzen. */
void ccpattach(void)
{
  MBHEAD  *mbp;
  ATTPRT  *attcmd;
  ATTTYP  *attype;
  char     Buf[MAXPATH + 1];
  char    *cBuf = Buf;
  int      port = EOF;
  int      error = ATT_NO_OPTION;
  WORD     found;
  WORD     att_kisstype = EOF;
  WORD     att_speed    = EOF;
  WORD     att_speedflag= EOF;
  int      att_channel = -1;
  int      shared_device = -1;
  int      device, p;
  char     att_device[MAXPATH + 1];

  if (tcpkiss_attach())
    return;

  /* Sysop will aendern und noch was in der Zeile da ? */
  if (issyso() && skipsp(&clicnt, &clipoi))
  {
    memset(att_device, 0, sizeof(att_device));
    do
    {
      /* Gibt es einen Fehler? */
      if (error != ATT_NO_OPTION)
        /* Abbruch. */
        break;

      /* Option einlesen. */
      cBuf = ReadBuf(cBuf, TRUE);

      /* Option in TBL suchen. */
      for (attcmd = attprt, found = 0; !found && attcmd->attstr != NULL; ++attcmd)
      {
        if (!strncmp(attcmd->attstr, cBuf, strlen(cBuf)))
          /* Option gefunden. */
          found = attcmd->attnum;
      }

      switch (found)
      {
        /* PORT. */
        case ATT_PORT:
          {
            char *end;
            long number;
            cBuf = ReadBuf(cBuf, FALSE);
            number = strtol(cBuf, &end, 10);
            if (end == cBuf || *end != '\0' || number < 0 || number >= L2PNUM)
              error = ATT_INV_PORT;
            else
            {
              port = (int)number;
              if (l1ptab[port] != -1)
                error = ATT_POR_BUSY;
            }
          }
        break;

        /* KISSTYPE setzen. */
        case ATT_KISSTYPE:
          /* KissType einlesen. */
          cBuf = ReadBuf(cBuf, TRUE);

          /* KissType in TBL suchen. */
          for (attype = attyp, found = EOF; found == EOF && attype->attstr != NULL; ++attype)
          {
            if (!strncmp(attype->attstr, cBuf, strlen(cBuf)))
            /* KissType gefunden. */
            found = attype->attnum;
          }

          /* Unbekannte KissType. */
          if (found == EOF )
          {
            /* Markiere Fehlermeldung. */
            error = ATT_INV_TYP;
            break;
          }

          /* Kisstype setzen. */
          if ((att_kisstype = SetKissType(found, port)) == EOF)
            /* EOF, Markiere Fehlermeldung. */
            error = ATT_KIS_BUSY;

        break;

        /* Baudrate setzen. */
        case ATT_SPEED:
          /* Baudrate einlesen. */
          cBuf = ReadBuf(cBuf, TRUE);

          /* Baudrate setzen. */
          if ((att_speed = SetSpeed(cBuf, att_kisstype)) == EOF)
            /* EOF, Markiere Fehlermeldung. */
            error = ATT_SPE_BUSY;

          /* Baudrateflag setzen. */
          att_speedflag = SetSpeedflag(cBuf);

        break;

        case ATT_CHANNEL:
          {
            char *end;
            long channel;
            cBuf = ReadBuf(cBuf, FALSE);
            channel = strtol(cBuf, &end, 10);
            if (end == cBuf || *end != '\0' || channel < 0 || channel > 15)
              error = ATT_INV_CHANNEL;
            else
              att_channel = (int)channel;
          }
          break;

        /* Device setzen. */
        case ATT_DEVICE:
          /* Device einlesen. */
          cBuf = ReadBuf(cBuf, FALSE);

          switch(att_kisstype)
          {
            case KISS_NORMAL:
            case KISS_SMACK:
            case KISS_RMNC:
            case KISS_TOK:
#ifdef VANESSA
            case KISS_VAN:
#endif
            case KISS_SCC:
            case KISS_TF:
            case KISS_KAX25:
            case KISS_KAX25KJD:
            case KISS_6PACK:
              strncpy(att_device, cBuf, MAXPATH);
            break;

            /* UDP-Port pruefen. */
            case KISS_AXIP:
              if (atoi(cBuf) < 65536)
              {
                strncpy(att_device, cBuf, MAXPATH);
                /* Neuen UDP-Port markieren. */
                my_udp = htons((unsigned short)atoi(cBuf));
              }
                /* UDP-Port liegt ausserhalb vom Grenzbereich. */
              else
                /* Markiere Fehlermeldung. */
                error = ATT_INV_UDP;

            break;

#ifdef L1TELNET
            /* TCP-Port pruefen. */
            case KISS_TELNET:
              if (atoi(cBuf) < 65536)
                strncpy(att_device, cBuf, MAXPATH);
                /* UDP-Port liegt ausserhalb vom Grenzbereich. */
              else
                /* Markiere Fehlermeldung. */
                error = ATT_INV_TCP;

            break;
#endif /* L1TELNET */
#ifdef L1HTTPD
            /* TCP-Port pruefen. */
            case KISS_HTTPD:
              if (atoi(cBuf) < 65536)
                strncpy(att_device, cBuf, MAXPATH);
                /* UDP-Port liegt ausserhalb vom Grenzbereich. */
              else
                /* Markiere Fehlermeldung. */
                error = ATT_INV_TCP;

            break;
#endif /* L1HTTPD */
#ifdef L1IPCONV
            /* TCP-Port pruefen. */
            case KISS_IPCONV:
              if (atoi(cBuf) < 65536)
                strncpy(att_device, cBuf, MAXPATH);
                /* UDP-Port liegt ausserhalb vom Grenzbereich. */
              else
                /* Markiere Fehlermeldung. */
                error = ATT_INV_TCP;

            break;
#endif /* L1IPCONV */
#ifdef L1IRC
            /* TCP-Port pruefen. */
            case KISS_IRC:
              if (atoi(cBuf) < 65536)
                strncpy(att_device, cBuf, MAXPATH);
                /* UDP-Port liegt ausserhalb vom Grenzbereich. */
              else
                /* Markiere Fehlermeldung. */
                error = ATT_INV_TCP;

            break;
#endif /* L1IRC */
         }

        case KISS_LOOP :
          break;

        default:
          /* Unbekannte Option. */
          error = ATT_INV_OPTION;
        break;
      }
    }
    /* Gibt es noch Zeichen. */
    while (clicnt > 0);
  }
    /* Kein Sysop. */
  else
    {
      if (!issyso())
        invmsg();
      else
        {
          DEVICE *l1att;

          mbp = putals("ATTACH-TABLES:\rPort--KISSTYPE---DEVICE\r");

          /* alle Ports durchgehen */
          for (port = 0; port < L2PNUM; ++port)
          {
            /* uninitialisierter Port */
            if (l1ptab[port] == -1)
            {
              putprintf(mbp, "%2d    Port deaktiv.\r", port);
              continue;
            }

            l1att = &l1port[l1ptab[port]];

            for (attype = attyp; attype->attstr != NULL; ++attype) {
              if (l1att->kisstype == attype->attnum)
                break;
            }

              putprintf(mbp, "%2d    %-10s "
                         , port
                         , attype->attstr);

              switch(l1att->kisstype)
              {
                case KISS_TCP:
                  tcpkiss_dump(mbp, port);
                break;
                case KISS_AXIP:
                  putprintf(mbp, "%u" ,ntohs(my_udp));
                break;

#ifdef L1TELNET
                case KISS_TELNET:
                  putprintf(mbp, "%d" , atoi(l1att->device));
                break;
#endif /* L1TELNET */
#ifdef L1HTTPD
                case KISS_HTTPD:
                  putprintf(mbp, "%d" , atoi(l1att->device));
                break;
#endif /* L1HTTPD */
#ifdef L1IPCONV
                case KISS_IPCONV:
                  putprintf(mbp, "%d" , atoi(l1att->device));
                break;
#endif /* L1IPCONV */
#ifdef L1IRC
                case KISS_IRC:
                  putprintf(mbp, "%d" , atoi(l1att->device));
                break;
#endif /* L1IRC */

                default:
                  putprintf(mbp, "%-5s" ,l1att->device);
                break;
              }
              if (l1att->kisstype == KISS_NORMAL)
                putprintf(mbp, " CHANNEL=%d", kisschannel[port]);
              putstr("\r", mbp);


        }
        /* und ab geht die Post. */
        prompt(mbp);
        seteom(mbp);
    }
    return;
  }

  if (error == ATT_NO_OPTION && att_channel >= 0 && att_kisstype != KISS_NORMAL)
    error = ATT_INV_CHANNEL;
  if (att_channel < 0)
    att_channel = 0;

  if (error == ATT_NO_OPTION && att_device[0]
      && (att_kisstype == KISS_NORMAL || att_kisstype == KISS_SMACK
          || att_kisstype == KISS_RMNC || att_kisstype == KISS_TOK
          || att_kisstype == KISS_TF || att_kisstype == KISS_6PACK))
    for (device = 0; device <= max_device; ++device)
      if (l1port[device].port_active
          && strcmp(l1port[device].device, att_device) == 0
          && (att_kisstype == KISS_NORMAL || l1port[device].kisstype == KISS_NORMAL))
      {
        if (att_kisstype != KISS_NORMAL || l1port[device].kisstype != KISS_NORMAL
            || l1port[device].speed != att_speed
            || l1port[device].speedflag != att_speedflag)
          error = ATT_DEVICE_BUSY;
        else
        {
          shared_device = device;
          for (p = 0; p < L2PNUM; ++p)
            if (l1ptab[p] == device && kisschannel[p] == att_channel)
              error = ATT_INV_CHANNEL;
        }
        break;
      }

  if (error != ATT_NO_OPTION || port == EOF || att_kisstype == EOF
      || (!att_device[0] && att_kisstype != KISS_AXIP && att_kisstype != KISS_LOOP)
      || (att_kisstype == KISS_NORMAL && att_speed == EOF))
  {
    mbp = getmbp();
    putprintf(mbp, "%s\r", errormsg[error]);
    putstr("Syntax: ATT PORT=X KTYP=KISSTYPE SPEED=19200 DEVICE=SCHNITTSTELLE [CHANNEL=0..15]\r", mbp);
    prompt(mbp);
    seteom(mbp);
    return;
  }

  if (shared_device >= 0)
  {
    l1ptab[port] = shared_device;
    kisschannel[port] = att_channel;
    mbp = getmbp();
    putprintf(mbp, "Port %d Attached.\r", port);
    prompt(mbp);
    seteom(mbp);
    return;
  }

  device = max_device + 1;
  if (att_kisstype == KISS_NORMAL)
    for (device = 0; device <= max_device; ++device)
      if (!l1port[device].port_active)
        break;
  if (device >= L1PNUM)
  {
    mbp = getmbp();
    putprintf(mbp, "%s\r", errormsg[ATT_NO_DEVICE]);
    prompt(mbp);
    seteom(mbp);
    return;
  }

  /* Kisstype Tokenring. */
  if (  (att_kisstype == KISS_TOK)
      /* Ja, dann zaehlen wir. */
      &&(++tokenring_ports))
  {
    /* Sonderbehandlung fuer Tokenring-Ports */
    if (att_kisstype == KISS_TOK)
    {
      l1ptab[port] = 0;
      l2ptab[max_device] = -1;
    }
  }
  /* Kein Tokenring. */
#ifdef SIXPACK
  /* Sonderbehandlung fuer 6PACK-Ports */
  if (  (att_kisstype == KISS_6PACK)
      &&(++sixpack_ports))
  {
    /* Sonderbehandlung fuer Tokenring-Ports */
    if (att_kisstype == KISS_6PACK)
    {
      l1ptab[port] = max_device;
      l2ptab[max_device] = -1;
    }
  }
#endif

    {
      /* Port auf aktiv stellen. */
      if (device > max_device)
        max_device = device;
      l1port[device].port_active = TRUE;
      /* Kisstype setzen. */
      l1port[device].kisstype          = att_kisstype;
      /* Baudrateflag setzen. */
      l1port[device].speedflag         = att_speedflag;
      /* Baudrate setzen. */
      l1port[device].speed             = att_speed;
      /* Device setzen. */
      strcpy(l1port[device].device, att_device);
      if (att_kisstype == KISS_NORMAL)
      {
        l1port[device].tnn_lockfile[0] = 0;
        l1port[device].lock = -1;
      }

      /* Port markieren. */
      l1ptab[port] = device;
      kisschannel[port] = att_channel;
      l2ptab[device] = port;

      /* Zaehler L1Ports. */
      ++used_l1ports;
      /* Kiss aktivieren. */
      kiss_active = TRUE;

      /* Serielle Schnittstellen initialisieren */
      if (InitKissLink((UWORD)port))
      {
        /* Es gab ein Fehler bei der Initialisierung. */
        /* Alle Werte zuruecksetzen. */
        l1port[device].port_active = FALSE;
        l1port[device].kisstype    = KISS_NIX;
        l1port[device].speedflag   = 0;
        l1port[device].speed       = 0;
        l1port[device].device[0]   = 0;

        l1ptab[port] = EOF;
        l2ptab[device] = EOF;
        kisschannel[port] = 0;
        while (max_device >= 0 && !l1port[max_device].port_active)
          --max_device;
        --used_l1ports;

        mbp = getmbp();

        putstr("Error with initializing the interface\r", mbp);
        prompt(mbp);
        seteom(mbp);
        return;
      }
    }

  mbp = getmbp();

  putprintf(mbp, "Port %d Attached.\r", port);
  prompt(mbp);
  seteom(mbp);
}

/* Port entladen. */
void ccpdetach(void)
{
  MBHEAD *mbp;
  DEVICE *l1att;
  int     port;

  /* Sysop will aendern und noch was in der Zeile da ? */
  if (issyso() && skipsp(&clicnt, &clipoi))
  {
    /* Port einlesen/pruefen. */
    if ((port = (nxtnum(&clicnt, &clipoi) & 0x7F)) < L2PNUM)
    {
      /* uninitialisierter Port */
      if (l1ptab[port] == EOF)
      {
        mbp = getmbp();
        putprintf(mbp, "Port %d is no activ.\r", port);
        prompt(mbp);
        seteom(mbp);
        return;
      }
      /* Port eingeschaltet. */
      if (portenabled(port))
        /* Port runterfahren. */
        l1detach(port);

      l1att = &l1port[l1ptab[port]];
      if (tcpkiss_detach(port))
        return;
      if (l1att->kisstype == KISS_NORMAL)
      {
        int device = l1ptab[port], other;
        l1ptab[port] = EOF;
        kisschannel[port] = 0;
        if (l1att->rx_port == port)
          l1att->rx_state = ST_BEGIN;
        for (other = 0; other < L2PNUM; ++other)
          if (l1ptab[other] == device)
            break;
        if (other < L2PNUM)
          l2ptab[device] = other;
        else
        {
          if (tcsetattr(l1att->kisslink, TCSADRAIN, &l1att->org_termios) < 0)
            printf("Error: can't restore device %s (%s)\n", l1att->device, strerror(errno));
          close(l1att->kisslink);
          l1att->kisslink = -1;
          if (l1att->tnn_lockfile[0])
          {
            unlink(l1att->tnn_lockfile);
            l1att->tnn_lockfile[0] = 0;
          }
          l1att->port_active = FALSE;
          l1att->kisstype = KISS_NIX;
          l1att->device[0] = 0;
          l2ptab[device] = EOF;
          --used_l1ports;
          while (max_device >= 0 && !l1port[max_device].port_active)
            --max_device;
        }
        mbp = getmbp();
        putprintf(mbp, "Port %d Detach.\r", port);
        prompt(mbp);
        seteom(mbp);
        return;
      }
      /* Schliesse Schnittstelle. */
      switch(l1att->kisstype)
      {
        case KISS_TOK:
          /* Schliesse Schnittstelle. */
          close(l1att->kisslink);

          /* Port's zuruecksetzen. */
          tokenring_ports = 0;
          break;


        case KISS_NORMAL:
        case KISS_SMACK:
        case KISS_RMNC:


        case KISS_TF:
          close(l1att->kisslink);
          break;

#ifdef SIXPACK
        case KISS_6PACK:
          /* Schnittstelle schliessen. */
          Sixpack_l1exit();

          /* Port's zuruecksetzen. */
          sixpack_ports = 0;
#endif /* SIXPACK */

        case KISS_AXIP:
        case KISS_KAX25:
        case KISS_KAX25KJD:
#ifdef L1TELNET
        case KISS_TELNET:
#endif /* L1TELNET */
#ifdef L1HTTPD
        case KISS_HTTPD:
#endif /* L1HTTPD */
#ifdef L1IPCONV
        case KISS_IPCONV:
#endif /* L1IPCONV */
#ifdef L1IRC
        case KISS_IRC:
#endif /* L1IRC */
          close(l1att->kisslink);
          break;

        default:
          break;

      }

      /* Defaultwerte setzen. */
      l1att->port_active = FALSE;
      l1att->kisstype    = KISS_NIX;
      l1att->speed       = 0;
      l1att->device[0]   = 0;

      l1ptab[port] = EOF;
      l2ptab[port] = EOF;

      mbp = getmbp();
      putprintf(mbp, "Port %d Detach.\r", port);
      prompt(mbp);
      seteom(mbp);
      return;
    }
      /* Ungueltiger Port. */
    else
      {
        mbp = getmbp();
        putstr("Invalid Port.\r", mbp);
        prompt(mbp);
        seteom(mbp);
        return;
      }
  }
  else
    { /* Sysop ? */
      if (!issyso())
        invmsg();
      /* Portangabe fehlt.*/
      else
        {
          mbp = getmbp();

          putstr("Port failed.\r", mbp);
          prompt(mbp);
          seteom(mbp);
        }
    }
}
/* Speed einlesen fuer tnb. */
int ReadSpeed(unsigned int Speed)
{
  switch(Speed)
  {
    case 0:
      return(FALSE);

    case B9600:
      return(9600);

    case B19200:
      return(19200);

    case B38400:
      return(38400);

    case B57600:
#ifdef B57600
      return(57600);
#else
      return(38400);
#endif

#ifdef B115200
    case B115200:
      return(115200);
#else
      return(38400);
#endif
    case 230400:
#ifdef B230400
      return(230400);
#else
      return(38400);
#endif

    case 460800:
#ifdef B460800
      return(460800);
#else
      return(38400);
#endif

    /* unbekannte Geschwindigkeiten */
    default:
      return(FALSE);
  }
  return(FALSE);
}

/* Attach-Eintraege in tnb sichern. */
void dump_attach(MBHEAD *mbp)
{
  DEVICE  *l1pp;
  int port;

  putstr(";\r; Attach (l1-Level)\r;\r", mbp);

  /* alle Ports durchgehen */
  for (port = 0; port < L2PNUM; ++port)
  {
    /* uninitialisierter Port */
    if (l1ptab[port] == -1)
      continue;

    l1pp = &l1port[l1ptab[port]];

    putprintf(mbp, "ATTACH PORT=%d ", port);

    switch(l1pp->kisstype)
    {
      case KISS_TCP:
        tcpkiss_dump(mbp, port);
      break;
      case KISS_NORMAL:
        putprintf(mbp, "KTYP=KISS SPEED=%d DEVICE=%s CHANNEL=%d"
                     , ReadSpeed(l1pp->speed)
                     , l1pp->device
                     , kisschannel[port]);
      break;

      case KISS_SMACK:
        putprintf(mbp, "KTYP=SMACK SPEED=%d DEVICE=%s"
                     , ReadSpeed(l1pp->speed)
                     , l1pp->device);
      break;

      case KISS_RMNC:
        putprintf(mbp, "KTYP=RMNC SPEED=%d DEVICE=%s"
                     , ReadSpeed(l1pp->speed)
                     , l1pp->device);
      break;

      case KISS_TOK:
        putprintf(mbp, "KTYP=TOKENRING SPEED=%d DEVICE=%s"
                     , ReadSpeed(l1pp->speed)
                     , l1pp->device);
      break;

      case KISS_SCC:
        putprintf(mbp, "KTYP=SCC DEVIVE=%s"
                     , l1pp->device);
      break;

      case KISS_TF:
        putprintf(mbp, "KTYP=TF SPEED=%d DEVICE=%s"
                     , ReadSpeed(l1pp->speed)
                     , l1pp->device);
      break;

      case KISS_IPX:
        putprintf(mbp, "KTYP=IPX");
      break;

      case KISS_AXIP:
        putstr("KTYP=AX25IP ", mbp);
        if (l1pp->device[0] != '\0')
          putprintf(mbp, "DEVICE=%d"
                       , ntohs(my_udp));
        else
          putstr("DEVICE=", mbp);
      break;

      case KISS_LOOP:
        putprintf(mbp, "KTYP=LOOP");
      break;

      case KISS_KAX25:
        putprintf(mbp, "KTYP=KAX25 DEVICE=%s"
                     , l1pp->device);
      break;

      case KISS_KAX25KJD:
        putprintf(mbp, "KTYP=KAX25DJK DEVICE=%s"
                     , l1pp->device);
      break;

      case KISS_6PACK:
        putprintf(mbp, "KTYP=6PACK SPEED=%d DEVICE=%s"
                     , ReadSpeed(l1pp->speed)
                     , l1pp->device);
      break;

#ifdef L1TELNET
      case KISS_TELNET:
        putprintf(mbp, "KTYP=TELNET DEVICE=%d"
                     , atoi(l1pp->device));
      break;
#endif /* L1TELNET */
#ifdef L1HTTPD
      case KISS_HTTPD:
        putprintf(mbp, "KTYP=HTTPD DEVICE=%d"
                     , atoi(l1pp->device));
      break;
#endif /* L1HTTPD */
#ifdef L1IPCONV
      case KISS_IPCONV:
        putprintf(mbp, "KTYP=IPCONV DEVICE=%d"
                     , atoi(l1pp->device));
      break;
#endif /* L1IPCONV */
#ifdef L1IRC
      case KISS_IRC:
        putprintf(mbp, "KTYP=IRC DEVICE=%d"
                     , atoi(l1pp->device));
      break;
#endif /* L1IRC */
    }
  putstr("\n", mbp);
  }
}

/* End of os/win32/l1attach.c */
